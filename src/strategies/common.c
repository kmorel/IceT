/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include "common.h"

#include <IceT.h>
#include <IceTDevCommunication.h>
#include <IceTDevDiagnostics.h>
#include <IceTDevState.h>
#include <IceTDevStrategySelect.h>

#include <stdlib.h>

#define FULL_IMAGE_DATA 20

#define LARGE_MESSAGE 23

static IceTImage rtfi_image;
static IceTSparseImage rtfi_outSparseImage;
static IceTBoolean rtfi_first;
static IceTVoid *rtfi_generateDataFunc(IceTInt id, IceTInt dest,
                                       IceTSizeType *size) {
    IceTInt rank;
    IceTInt *tile_list = icetUnsafeStateGetInteger(ICET_CONTAINED_TILES_LIST);
    IceTVoid *outBuffer;

    icetGetIntegerv(ICET_RANK, &rank);
    if (dest == rank) {
      /* Special case: sending to myself.
         Just get directly to color and depth buffers. */
        icetGetTileImage(tile_list[id], rtfi_image);
        *size = 0;
        return NULL;
    }
    icetGetCompressedTileImage(tile_list[id], rtfi_outSparseImage);
    icetSparseImagePackageForSend(rtfi_outSparseImage, &outBuffer, size);
    return outBuffer;
}
static void rtfi_handleDataFunc(void *inSparseImageBuffer, IceTInt src) {
    if (inSparseImageBuffer == NULL) {
      /* Superfluous call from send to self. */
        if (!rtfi_first) {
            icetRaiseError("Unexpected callback order"
                           " in icetRenderTransferFullImages.",
                           ICET_SANITY_CHECK_FAIL);
        }
    } else {
        IceTSparseImage inSparseImage
            = icetSparseImageUnpackageFromReceive(inSparseImageBuffer);
        if (rtfi_first) {
            icetDecompressImage(inSparseImage, rtfi_image);
        } else {
            IceTInt rank;
            IceTInt *process_orders;
            icetGetIntegerv(ICET_RANK, &rank);
            process_orders = icetUnsafeStateGetInteger(ICET_PROCESS_ORDERS);
            icetCompressedComposite(rtfi_image, inSparseImage,
                                    process_orders[src] < process_orders[rank]);
        }
    }
    rtfi_first = ICET_FALSE;
}
static IceTInt *imageDestinations = NULL;
static IceTInt allocatedTileSize = 0;
void icetRenderTransferFullImages(IceTImage image,
                                  IceTVoid *inSparseImageBuffer,
                                  IceTSparseImage outSparseImage,
                                  IceTInt *tile_image_dest)
{
    IceTInt num_sending;
    IceTInt *tile_list;
    IceTInt num_tiles;
    IceTInt width, height;

    IceTInt i;

    rtfi_image = image;
    rtfi_outSparseImage = outSparseImage;
    rtfi_first = ICET_TRUE;

    icetGetIntegerv(ICET_NUM_CONTAINED_TILES, &num_sending);
    tile_list = icetUnsafeStateGetInteger(ICET_CONTAINED_TILES_LIST);
    icetGetIntegerv(ICET_TILE_MAX_WIDTH, &width);
    icetGetIntegerv(ICET_TILE_MAX_HEIGHT, &height);
    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);

    if (allocatedTileSize < num_tiles) {
        free(imageDestinations);
        imageDestinations = malloc(num_tiles * sizeof(IceTInt));
        allocatedTileSize = num_tiles;
    }

  /* Make each element imageDestinations point to the processor to send the
     corresponding image in tile_list. */
    for (i = 0; i < num_sending; i++) {
        imageDestinations[i] = tile_image_dest[tile_list[i]];
    }

    icetSendRecvLargeMessages(num_sending, imageDestinations,
                              icetIsEnabled(ICET_ORDERED_COMPOSITE),
                              rtfi_generateDataFunc, rtfi_handleDataFunc,
                              inSparseImageBuffer,
                              icetSparseImageBufferSize(width, height));
}

static void startLargeRecv(void *buf, IceTSizeType size, IceTInt src,
                           IceTCommRequest *req) {
    *req = icetCommIrecv(buf, size, ICET_BYTE, src, LARGE_MESSAGE);
}
static void startLargeSend(IceTInt dest, IceTCommRequest *req,
                           IceTGenerateData callback, IceTInt *sendIds) {
    IceTSizeType data_size;
    IceTVoid *data;
    data = (*callback)(sendIds[dest], dest, &data_size);
    *req = icetCommIsend(data, data_size, ICET_BYTE, dest, LARGE_MESSAGE);
}
static IceTInt *sendIds = NULL;
static char *myDests = NULL;
static char *allDests = NULL;
static IceTInt *sendQueue = NULL;
static IceTInt *recvQueue = NULL;
static IceTInt *recvFrom = NULL;
static IceTInt allocatedCommSize = 0;
void icetSendRecvLargeMessages(IceTInt numMessagesSending,
                               IceTInt *messageDestinations,
                               IceTBoolean messagesInOrder,
                               IceTGenerateData generateDataFunc,
                               IceTHandleData handleDataFunc,
                               IceTVoid *incomingBuffer,
                               IceTSizeType bufferSize)
{
    IceTInt comm_size;
    IceTInt rank;
    IceTInt i;
    IceTInt sender;
    IceTInt numSend, numRecv;
    IceTInt someoneSends;
    IceTInt sendToSelf;
    IceTInt sqi, rqi;     /* Send/Recv queue index. */
    IceTInt *composite_order;
    IceTInt *process_orders;

#define RECV_IDX 0
#define SEND_IDX 1
    IceTCommRequest requests[2];

    icetGetIntegerv(ICET_NUM_PROCESSES, &comm_size);
    icetGetIntegerv(ICET_RANK, &rank);

    composite_order = icetUnsafeStateGetInteger(ICET_COMPOSITE_ORDER);
    process_orders = icetUnsafeStateGetInteger(ICET_PROCESS_ORDERS);

  /* Make sure we have big enough arrays.  We should not have to allocate
     very often. */
    if (comm_size > allocatedCommSize) {
        free(sendIds);
        free(myDests);
        free(allDests);
        free(sendQueue);
        free(recvQueue);
        free(recvFrom);
        sendIds = malloc(comm_size * sizeof(IceTInt));
        myDests = malloc(comm_size);
        allDests = malloc(comm_size * comm_size);
        sendQueue = malloc(comm_size * sizeof(IceTInt));
        recvQueue = malloc(comm_size * sizeof(IceTInt));
        recvFrom = malloc(comm_size * sizeof(IceTInt));
        allocatedCommSize = comm_size;
    }

  /* Convert array of ranks to a mask of ranks. */
    for (i = 0; i < comm_size; i++) {
        myDests[i] = 0;
    }
    for (i = 0; i < numMessagesSending; i++) {
        myDests[messageDestinations[i]] = 1;
        sendIds[messageDestinations[i]] = i;
    }

  /* We'll just handle send to self as a special case. */
    sendToSelf = myDests[rank];
    myDests[rank] = 0;

  /* Gather masks for all processes. */
    icetCommAllgather(myDests, comm_size, ICET_BYTE, allDests);

  /* Determine communications.  We will determine communications in a
     series of steps.  At each step, we will try to find the most send/recv
     pairs.  Each step will be able to complete without deadlock.  Rather
     than saving the sends and recieves per step, we will simply push them
     into queues and run the send and receive queues in parallel later. */
    numSend = 0;  numRecv = 0;
    do {
        someoneSends = 0;
      /* We are going to keep track of what every processor receives to
         ensure that no processor receives more than one message per
         iteration.  Clear out the array first. */
        for (i = 0; i < comm_size; i++) recvFrom[i] = -1;
      /* Try to find a destination for each sender. */
        for (sender = 0; sender < comm_size; sender++) {
            char *localDests = allDests + sender*comm_size;
            IceTInt receiver;
            for (receiver = 0; receiver < comm_size; receiver++) {
                if (localDests[receiver] && (recvFrom[receiver] < 0)) {
                    if (messagesInOrder) {
                      /* Make sure there is not another message that must
                         be sent first. */
                        IceTInt left, right;
                        if (process_orders[sender] < process_orders[receiver]) {
                            left = process_orders[sender];
                            right = process_orders[receiver];
                        } else {
                            left = process_orders[receiver];
                            right = process_orders[sender];
                        }
                        for (left++; left < right; left++) {
                            IceTInt p = composite_order[left];
                            if (allDests[p*comm_size + receiver]) break;
                        }
                        if (left != right) {
                          /* We have to wait for someone else to send
                             before we can send this one. */
                            continue;
                        }
                    }
                    localDests[receiver] = 0;
                  /* This is no longer necessary since we take care of
                     sentToSelf above
                     
                    if (sender == receiver) {
                        if (rank == sender) sendToSelf = 1;
                        continue;
                    }
                  */
                    recvFrom[receiver] = sender;
                    if (sender == rank) {
                        sendQueue[numSend++] = receiver;
                    }
                    someoneSends = 1;
                    break;
                }
            }
        }
      /* Check to see if we are receiving something this iteration. */
        if (recvFrom[rank] >= 0) {
            recvQueue[numRecv++] = recvFrom[rank];
        }
    } while (someoneSends);
#ifdef DEBUG
    for (i = 0; i < comm_size*comm_size; i++) {
        if (allDests[i] != 0) {
            icetRaiseError("Apperent deadlock encountered.",
                           ICET_SANITY_CHECK_FAIL);
        }
    }
#endif

    sqi = 0;  rqi = 0;
    if (rqi < numRecv) {
        icetRaiseDebug1("Receiving from %d", (int)recvQueue[rqi]);
        startLargeRecv(incomingBuffer, bufferSize, recvQueue[rqi],
                       &requests[RECV_IDX]);
    } else {
        requests[RECV_IDX] = ICET_COMM_REQUEST_NULL;
    }
    if (sendToSelf) {
        IceTSizeType data_size;
        IceTVoid *data;
        icetRaiseDebug("Sending to self.");
        data = (*generateDataFunc)(sendIds[rank], rank, &data_size);
        (*handleDataFunc)(data, rank);
    }
    if (sqi < numSend) {
        icetRaiseDebug1("Sending to %d", (int)sendQueue[sqi]);
        startLargeSend(sendQueue[sqi], &requests[SEND_IDX],
                       generateDataFunc, sendIds);
    } else {
        requests[SEND_IDX] = ICET_COMM_REQUEST_NULL;
    }
    while ((rqi < numRecv) || (sqi < numSend)) {
        icetRaiseDebug("Starting wait.");
        i = icetCommWaitany(2, requests);
        icetRaiseDebug1("Wait returned with %d finished.", (int)i);
        switch (i) {
          case RECV_IDX:
              icetRaiseDebug1("Receive from %d finished", (int)recvQueue[rqi]);
              (*handleDataFunc)(incomingBuffer, recvQueue[rqi]);
              rqi++;
              if (rqi < numRecv) {
                  icetRaiseDebug1("Receiving from %d", (int)recvQueue[rqi]);
                  startLargeRecv(incomingBuffer, bufferSize, recvQueue[rqi],
                                 &requests[RECV_IDX]);
              }
              continue;
          case SEND_IDX:
              icetRaiseDebug1("Send to %d finished", (int)sendQueue[sqi]);
              sqi++;
              if (sqi < numSend) {
                  icetRaiseDebug1("Sending to %d", (int)sendQueue[sqi]);
                  startLargeSend(sendQueue[sqi], &requests[SEND_IDX],
                                 generateDataFunc, sendIds);
              }
              continue;
        }
    }
}

void icetSingleImageCompose(IceTInt *compose_group, IceTInt group_size,
                            IceTInt image_dest,
                            IceTImage image)
{
    IceTEnum strategy;

    icetGetEnumv(ICET_SINGLE_IMAGE_STRATEGY, &strategy);
    icetInvokeSingleImageStrategy(strategy,
                                  compose_group, group_size,
                                  image_dest,
                                  image);
}
