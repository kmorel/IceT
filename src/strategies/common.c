/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

/* Id */

#include "common.h"

#include <GL/ice-t.h>
#include <state.h>
#include <context.h>
#include <diagnostics.h>

#include <stdlib.h>

#define FULL_IMAGE_DATA 20
#define SWAP_IMAGE_DATA 21
#define SWAP_DEPTH_DATA 22
#define TREE_IMAGE_DATA 23

#define LARGE_MESSAGE 23

static IceTImage rtfi_imageBuffer;
static IceTSparseImage rtfi_inImage;
static IceTSparseImage rtfi_outImage;
static GLint rtfi_first;
static void *rtfi_generateDataFunc(GLint id, GLint dest, GLint *size) {
    GLint rank;
    GLint *tile_list = icetUnsafeStateGet(ICET_CONTAINED_TILES_LIST);

    icetGetIntegerv(ICET_RANK, &rank);
    if (dest == rank) {
      /* Special case: sending to myself.
         Just get directly to color and depth buffers. */
        icetGetTileImage(tile_list[id], rtfi_imageBuffer);
        *size = 0;
        return NULL;
    }
    *size = icetGetCompressedTileImage(tile_list[id], rtfi_outImage);
    return rtfi_outImage;
}
static void *rtfi_handleDataFunc(void *inImage, GLint src) {
    if (inImage == NULL) {
      /* Superfluous call from send to self. */
    } else {
        if (rtfi_first) {
            icetDecompressImage(inImage, rtfi_imageBuffer);
        } else {
            GLint rank;
            GLint *process_orders;
            icetGetIntegerv(ICET_RANK, &rank);
            process_orders = icetUnsafeStateGet(ICET_PROCESS_ORDERS);
            icetCompressedComposite(rtfi_imageBuffer, inImage,
                                    process_orders[src] < process_orders[rank]);
        }
    }
    rtfi_first = 0;
    return rtfi_inImage;
}
static GLint *imageDestinations = NULL;
static GLint allocatedTileSize = 0;
void icetRenderTransferFullImages(IceTImage imageBuffer,
                                  IceTSparseImage inImage,
                                  IceTSparseImage outImage,
                                  GLint num_receiving , 
                                  GLint *tile_image_dest)
{
    GLint num_sending;
    GLint *tile_list;
    GLint max_pixels;
    GLint num_tiles;

    GLint i;

    /* To remove warning */
    (void)num_receiving;

    rtfi_imageBuffer = imageBuffer;
    rtfi_inImage = inImage;
    rtfi_outImage = outImage;
    rtfi_first = 1;

    icetGetIntegerv(ICET_NUM_CONTAINED_TILES, &num_sending);
    tile_list = icetUnsafeStateGet(ICET_CONTAINED_TILES_LIST);
    icetGetIntegerv(ICET_TILE_MAX_PIXELS, &max_pixels);
    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);

    if (allocatedTileSize < num_tiles) {
        free(imageDestinations);
        imageDestinations = malloc(num_tiles * sizeof(GLint));
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
                              inImage, icetSparseImageSize(max_pixels));
}

static void startLargeRecv(void *buf, GLint size, GLint src,
                           IceTCommRequest *req) {
    *req = ICET_COMM_IRECV(buf, size, ICET_BYTE, src, LARGE_MESSAGE);
}
static void startLargeSend(GLint dest, IceTCommRequest *req,
                           IceTGenerateData callback, GLint *sendIds) {
    GLint data_size;
    void *data;
    data = (*callback)(sendIds[dest], dest, &data_size);
    icetAddSentBytes(data_size);
    *req = ICET_COMM_ISEND(data, data_size, ICET_BYTE, dest, LARGE_MESSAGE);
}
static GLint *sendIds = NULL;
static char *myDests = NULL;
static char *allDests = NULL;
static GLint *sendQueue = NULL;
static GLint *recvQueue = NULL;
static GLint *recvFrom = NULL;
static GLint allocatedCommSize = 0;
void icetSendRecvLargeMessages(GLint numMessagesSending,
                               GLint *messageDestinations,
                               GLint messagesInOrder,
                               IceTGenerateData generateDataFunc,
                               IceTHandleData handleDataFunc,
                               void *incomingBuffer,
                               GLint bufferSize)
{
    GLint comm_size;
    GLint rank;
    GLint i;
    GLint sender;
    GLint numSend, numRecv;
    GLint someoneSends;
    GLint sendToSelf;
    GLint sqi, rqi;     /* Send/Recv queue index. */
    GLint *composite_order;
    GLint *process_orders;

#define RECV_IDX 0
#define SEND_IDX 1
    IceTCommRequest requests[2];

    icetGetIntegerv(ICET_NUM_PROCESSES, &comm_size);
    icetGetIntegerv(ICET_RANK, &rank);

    composite_order = icetUnsafeStateGet(ICET_COMPOSITE_ORDER);
    process_orders = icetUnsafeStateGet(ICET_PROCESS_ORDERS);

  /* Make sure we have big enough arrays.  We should not have to allocate
     very often. */
    if (comm_size > allocatedCommSize) {
        free(sendIds);
        free(myDests);
        free(allDests);
        free(sendQueue);
        free(recvQueue);
        free(recvFrom);
        sendIds = malloc(comm_size * sizeof(GLint));
        myDests = malloc(comm_size);
        allDests = malloc(comm_size * comm_size);
        sendQueue = malloc(comm_size * sizeof(GLint));
        recvQueue = malloc(comm_size * sizeof(GLint));
        recvFrom = malloc(comm_size * sizeof(GLint));
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
    ICET_COMM_ALLGATHER(myDests, comm_size, ICET_BYTE, allDests);

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
            GLint receiver;
            for (receiver = 0; receiver < comm_size; receiver++) {
                if (localDests[receiver] && (recvFrom[receiver] < 0)) {
                    if (messagesInOrder) {
                      /* Make sure there is not another message that must
                         be sent first. */
                        GLint left, right;
                        if (process_orders[sender] < process_orders[receiver]) {
                            left = process_orders[sender];
                            right = process_orders[receiver];
                        } else {
                            left = process_orders[receiver];
                            right = process_orders[sender];
                        }
                        for (left++; left < right; left++) {
                            GLint p = composite_order[left];
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
        icetRaiseDebug1("Receiving from %d", recvQueue[rqi]);
        startLargeRecv(incomingBuffer, bufferSize, recvQueue[rqi],
                       &requests[RECV_IDX]);
    } else {
        requests[RECV_IDX] = ICET_COMM_REQUEST_NULL;
    }
    if (sendToSelf) {
        GLint data_size;
        void *data;
        icetRaiseDebug("Sending to self.");
        data = (*generateDataFunc)(sendIds[rank], rank, &data_size);
        (*handleDataFunc)(data, rank);
    }
    if (sqi < numSend) {
        icetRaiseDebug1("Sending to %d", sendQueue[sqi]);
        startLargeSend(sendQueue[sqi], &requests[SEND_IDX],
                       generateDataFunc, sendIds);
    } else {
        requests[SEND_IDX] = ICET_COMM_REQUEST_NULL;
    }
    while ((rqi < numRecv) || (sqi < numSend)) {
        icetRaiseDebug("Starting wait.");
        i = ICET_COMM_WAITANY(2, requests);
        icetRaiseDebug1("Wait returned with %d finished.", i);
        switch (i) {
          case RECV_IDX:
              icetRaiseDebug1("Receive from %d finished", recvQueue[rqi]);
              incomingBuffer = (*handleDataFunc)(incomingBuffer,
                                                 recvQueue[rqi]);
              rqi++;
              if (rqi < numRecv) {
                  icetRaiseDebug1("Receiving from %d", recvQueue[rqi]);
                  startLargeRecv(incomingBuffer, bufferSize, recvQueue[rqi],
                                 &requests[RECV_IDX]);
              }
              continue;
          case SEND_IDX:
              icetRaiseDebug1("Send to %d finished", sendQueue[sqi]);
              sqi++;
              if (sqi < numSend) {
                  icetRaiseDebug1("Sending to %d", sendQueue[sqi]);
                  startLargeSend(sendQueue[sqi], &requests[SEND_IDX],
                                 generateDataFunc, sendIds);
              }
              continue;
        }
    }
}

#define BIT_REVERSE(result, x, max_val_plus_one)                              \
{                                                                             \
    int placeholder;                                                          \
    int input = (x);                                                          \
    (result) = 0;                                                             \
    for (placeholder=0x0001; placeholder<max_val_plus_one; placeholder<<=1) { \
        (result) <<= 1;                                                       \
        (result) += input & 0x0001;                                           \
        input >>= 1;                                                          \
    }                                                                         \
}

static void BswapCollectFinalImages(GLint *compose_group, GLint group_size,
                                    GLint group_rank, IceTImage imageBuffer,
                                    GLint pixel_count)
{
    GLenum output_buffers;
    IceTCommRequest *requests;
    int i;

  /* All processors have the same number for pixels and their offset
   * is group_rank*offset. */
    icetGetIntegerv(ICET_OUTPUT_BUFFERS, (GLint *)&output_buffers);
    requests = malloc((group_size)*sizeof(IceTCommRequest));

    if ((output_buffers & ICET_COLOR_BUFFER_BIT) != 0) {
        GLubyte *colorBuffer = icetGetImageColorBuffer(imageBuffer);
        icetRaiseDebug("Collecting image data.");
        for (i = 0; i < group_size; i++) {
            GLint src;
          /* Actual peice is located at the bit reversal of i. */
            BIT_REVERSE(src, i, group_size);
            if (src != group_rank) {
                requests[i] =
                    ICET_COMM_IRECV(colorBuffer + 4*pixel_count*i,
                                    4*pixel_count, ICET_BYTE,
                                    compose_group[src], SWAP_IMAGE_DATA);
            } else {
                requests[i] = ICET_COMM_REQUEST_NULL;
            }
        }
        for (i = 0; i < group_size; i++) {
            ICET_COMM_WAIT(requests + i);
        }
    }
    if ((output_buffers & ICET_DEPTH_BUFFER_BIT) != 0) {
        GLuint *depthBuffer = icetGetImageDepthBuffer(imageBuffer);
        icetRaiseDebug("Collecting depth data.");
        for (i = 0; i < group_size; i++) {
            GLint src;
          /* Actual peice is located at the bit reversal of i. */
            BIT_REVERSE(src, i, group_size);
            if (src != group_rank) {
                requests[i] =
                    ICET_COMM_IRECV(depthBuffer + pixel_count*i,
                                    pixel_count, ICET_INT,
                                    compose_group[src], SWAP_DEPTH_DATA);
            } else {
                requests[i] = ICET_COMM_REQUEST_NULL;
            }
        }
        for (i = 0; i < group_size; i++) {
            ICET_COMM_WAIT(requests + i);
        }
    }
    free(requests);
}

static void BswapSendFinalImage(GLint *compose_group, GLint image_dest,
                                IceTImage imageBuffer,
                                GLint pixel_count, GLint offset)
{
    GLenum output_buffers;

    icetGetIntegerv(ICET_OUTPUT_BUFFERS, (GLint *)&output_buffers);
    if ((output_buffers & ICET_COLOR_BUFFER_BIT) != 0) {
        GLubyte *colorBuffer = icetGetImageColorBuffer(imageBuffer);
            icetRaiseDebug("Sending image data.");
            icetAddSentBytes(4*pixel_count);
            ICET_COMM_SEND(colorBuffer + 4*offset, 4*pixel_count, ICET_BYTE,
                           compose_group[image_dest], SWAP_IMAGE_DATA);
    }
    if ((output_buffers & ICET_DEPTH_BUFFER_BIT) != 0) {
        GLuint *depthBuffer = icetGetImageDepthBuffer(imageBuffer);
        icetRaiseDebug("Sending depth data.");
        icetAddSentBytes(4*pixel_count);
        ICET_COMM_SEND(depthBuffer + offset, pixel_count, ICET_INT,
                       compose_group[image_dest], SWAP_DEPTH_DATA);
    }
}

/* Does binary swap, but does not combine the images in the end.  Instead,
 * the image is broken into pow2size pieces and stored in the first set of
 * processes.  pow2size is assumed to be the largest power of 2 <=
 * group_size.  Each process has the image offset in buffer to its
 * appropriate location.  Each process contains the ith piece, where i is
 * group_rank with the bits reversed (which is necessary to get the
 * ordering correct).  If both color and depth buffers are inputs, both are
 * located in the uncollected images regardless of what buffers are
 * selected for outputs. */
static void BswapComposeNoCombine(GLint *compose_group, GLint group_size,
                                  GLint pow2size, GLint group_rank,
                                  IceTImage imageBuffer, GLint pixels,
                                  IceTSparseImage inImage,
                                  IceTSparseImage outImage)
{
    GLint extra_proc;   /* group_size - pow2size */
    GLint extra_pow2size;       /* extra_proc rounded down to nearest power of 2. */

    extra_proc = group_size - pow2size;
    for (extra_pow2size = 1; extra_pow2size <= extra_proc; extra_pow2size *= 2);
    extra_pow2size /= 2;

    if (group_rank >= pow2size) {
        GLint upper_group_rank = group_rank - pow2size;
      /* I am part of the extra stuff.  Recurse to run bswap on my part. */
        BswapComposeNoCombine(compose_group + pow2size, extra_proc,
                              extra_pow2size, upper_group_rank,
                              imageBuffer, pixels, inImage, outImage);
      /* Now I may have some image data to send to lower group. */
        if (upper_group_rank < extra_pow2size) {
            GLint num_pieces = pow2size/extra_pow2size;
            GLuint offset;
            int i;

            BIT_REVERSE(offset, upper_group_rank, extra_pow2size);
            icetRaiseDebug1("My offset: %d", offset);
            offset *= pixels/extra_pow2size;

          /* Trying to figure out what processes to send to is tricky.  We
           * can do this by getting the peice number (bit reversal of
           * upper_group_rank), multiply this by num_pieces, add the number
           * of each local piece to get the piece number for the lower
           * half, and finally reverse the bits again.  Equivocally, we can
           * just reverse the bits of the local piece num, multiply by
           * num_peices and add that to upper_group_rank to get the final
           * location. */
            pixels = pixels/pow2size;
            for (i = 0; i < num_pieces; i++) {
                GLint compressedSize;
                GLint dest_rank;

                BIT_REVERSE(dest_rank, i, num_pieces);
                dest_rank = dest_rank*extra_pow2size + upper_group_rank;
                icetRaiseDebug2("Sending piece %d to %d", i, dest_rank);

              /* Is compression the right thing?  It's currently easier. */
                compressedSize = icetCompressSubImage(imageBuffer,
                                                      offset + i*pixels,
                                                      pixels, outImage);
                icetAddSentBytes(compressedSize);
              /* Send to processor in lower "half" that has same part of
               * image. */
                ICET_COMM_SEND(outImage, compressedSize, ICET_BYTE,
                               compose_group[dest_rank],
                               SWAP_IMAGE_DATA);
            }
        }
        return;
    } else {
      /* I am part of the lower group.  Do the actual binary swap. */
      /* To do the ordering correct, at iteration i we must swap with a
       * process 2^i units away.  The easiest way to find the process to
       * pair with is to simply xor the group_rank with a value with the
       * ith bit set. */
        int bitmask = 0x0001;
        int offset = 0;

        for (bitmask = 0x0001; bitmask < pow2size; bitmask <<= 1) {
            GLint pair;
            GLint inOnTop;
            GLint compressedSize;

            pair = group_rank ^ bitmask;

            pixels /= 2;

            if (group_rank < pair) {
                compressedSize = icetCompressSubImage(imageBuffer,
                                                      offset + pixels, pixels,
                                                      outImage);
                inOnTop = 0;
            } else {
                compressedSize = icetCompressSubImage(imageBuffer,
                                                      offset, pixels,
                                                      outImage);
                inOnTop = 1;
                offset += pixels;
            }

            icetAddSentBytes(compressedSize);
            ICET_COMM_SENDRECV(outImage, compressedSize,
                               ICET_BYTE, compose_group[pair], SWAP_IMAGE_DATA,
                               inImage, icetSparseImageSize(pixels),
                               ICET_BYTE, compose_group[pair], SWAP_IMAGE_DATA);

            icetCompressedSubComposite(imageBuffer, offset, pixels,
                                       inImage, inOnTop);
        }

      /* Now absorb any image that was part of extra stuff. */
      /* To get the processor where the extra stuff is located, I could
       * reverse the bits of the local process, divide by the appropriate
       * amount, and reverse the bits again.  However, the equivalent to
       * this is just clearing out the upper bits. */
        if (extra_pow2size > 0) {
            GLint src = pow2size + (group_rank & (extra_pow2size-1));
            icetRaiseDebug1("Absorbing image from %d", src);
            ICET_COMM_RECV(inImage, icetSparseImageSize(pixels),
                           ICET_BYTE, compose_group[src], SWAP_IMAGE_DATA);
            icetCompressedSubComposite(imageBuffer, offset, pixels,
                                       inImage, 0);
        }
    }
}

void icetBswapCompose(GLint *compose_group, GLint group_size, GLint image_dest,
                      IceTImage imageBuffer,
                      IceTSparseImage inImage, IceTSparseImage outImage)
{
    GLint group_rank;
    GLint rank;
    GLint pow2size;
    GLuint pixels;

    icetRaiseDebug("In icetBswapCompose");

    icetGetIntegerv(ICET_RANK, &rank);
    for (group_rank = 0; compose_group[group_rank] != rank; group_rank++);

  /* Make size of group be a power of 2. */
    for (pow2size = 1; pow2size <= group_size; pow2size *= 2);
    pow2size /= 2;

    pixels = icetGetImagePixelCount(imageBuffer);
  /* Make sure we can divide pixels evenly amongst processors. */
  /* WARNING: Will leave some pixels un-composed. */
    pixels = (pixels/pow2size)*pow2size;

  /* Do actual bswap. */
    BswapComposeNoCombine(compose_group, group_size, pow2size, group_rank,
                          imageBuffer, pixels, inImage, outImage);

    if (group_rank == image_dest) {
      /* Collect image if I'm the destination. */
        BswapCollectFinalImages(compose_group, pow2size, group_rank,
                                imageBuffer, pixels/pow2size);
    } else if (group_rank < pow2size) {
      /* Send image to destination. */
        GLint sub_image_size = pixels/pow2size;
        GLint piece_num;
        BIT_REVERSE(piece_num, group_rank, pow2size);
        BswapSendFinalImage(compose_group, image_dest, imageBuffer,
                            sub_image_size, piece_num*sub_image_size);
    }
}

static void RecursiveTreeCompose(GLint *compose_group, GLint group_size,
                                 GLint group_rank, GLint image_dest,
                                 IceTImage imageBuffer,
                                 IceTSparseImage compressedImageBuffer)
{
    GLint middle;
    enum { NO_IMAGE, SEND_IMAGE, RECV_IMAGE } current_image;
    GLint pair_proc;

    if (group_size <= 1) return;

  /* Build composite tree by splitting down middle. */
  /* If middle is in a group, then the image is sent there, otherwise the
   * image is sent to the processor of group_rank 0 (for that subgroup). */
    middle = group_size/2;
    if (group_rank < middle) {
        RecursiveTreeCompose(compose_group, middle, group_rank, image_dest,
                             imageBuffer, compressedImageBuffer);
        if (group_rank == image_dest) {
          /* I'm the destination.  GIMME! */
            current_image = RECV_IMAGE;
            pair_proc = middle;
        } else if (   (group_rank == 0)
                   && ((image_dest < 0) || (image_dest >= middle)) ) {
          /* I have an image by default. */
            if ((image_dest >= middle) && (image_dest < group_size)) {
              /* Opposite subtree has destination.  Hand it over. */
                current_image = SEND_IMAGE;
                pair_proc = image_dest;
            } else {
              /* Opposite subtree does not have destination either.  Give
                 to me by default. */
                current_image = RECV_IMAGE;
                pair_proc = middle;
            }
        } else {
          /* I don't even have an image anymore. */
            current_image = NO_IMAGE;
            pair_proc = -1;
        }
    } else {
        RecursiveTreeCompose(compose_group + middle, group_size - middle,
                             group_rank - middle, image_dest - middle,
                             imageBuffer, compressedImageBuffer);
        if (group_rank == image_dest) {
          /* I'm the destination.  GIMME! */
            current_image = RECV_IMAGE;
            pair_proc = 0;
        } else if (   (group_rank == middle)
                   && ((image_dest < middle) || (image_dest >= group_size)) ) {
          /* I have an image by default. */
            current_image = SEND_IMAGE;
            if ((image_dest >= 0) && (image_dest < middle)) {
                pair_proc = image_dest;
            } else {
                pair_proc = 0;
            }
        } else {
          /* I don't even have an image anymore. */
            current_image = NO_IMAGE;
            pair_proc = -1;
        }
    }

    if (current_image == SEND_IMAGE) {
      /* Hasta la vista, baby. */
        GLint compressedSize;
        icetRaiseDebug1("Sending image to %d", compose_group[pair_proc]);
        compressedSize = icetCompressImage(imageBuffer,
                                           compressedImageBuffer);
        icetAddSentBytes(compressedSize);
        ICET_COMM_SEND(compressedImageBuffer, compressedSize, ICET_BYTE,
                       compose_group[pair_proc], TREE_IMAGE_DATA);
    } else if (current_image == RECV_IMAGE) {
      /* Get my image. */
        icetRaiseDebug1("Getting image from %d", compose_group[pair_proc]);
        ICET_COMM_RECV(compressedImageBuffer,
                       icetSparseImageSize(icetGetImagePixelCount(imageBuffer)),
                       ICET_BYTE,
                       compose_group[pair_proc], TREE_IMAGE_DATA);
        icetCompressedComposite(imageBuffer, compressedImageBuffer,
                                pair_proc < group_rank);
    }
}

void icetTreeCompose(GLint *compose_group, GLint group_size, GLint image_dest,
                     IceTImage imageBuffer,
                     IceTSparseImage compressedImageBuffer)
{
    GLint group_rank;
    GLint rank;

    icetGetIntegerv(ICET_RANK, &rank);
    for (group_rank = 0; compose_group[group_rank] != rank; group_rank++);

    RecursiveTreeCompose(compose_group, group_size, group_rank, image_dest,
                         imageBuffer, compressedImageBuffer);
}
