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
static int rtfi_first;
static void *rtfi_generateDataFunc(int id, int dest, int *size) {
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
static void *rtfi_handleDataFunc(void *inImage, int src) {
    if (inImage == NULL) {
      /* Superfluous call from send to self. */
    } else {
	if (rtfi_first) {
	    icetDecompressImage(inImage, rtfi_imageBuffer);
	} else {
	    icetCompressedComposite(rtfi_imageBuffer, inImage);
	}
    }
    rtfi_first = 0;
    return rtfi_inImage;
}
static int *imageDestinations = NULL;
static int allocatedTileSize = 0;
void icetRenderTransferFullImages(IceTImage imageBuffer,
				  IceTSparseImage inImage,
				  IceTSparseImage outImage,
				  int num_receiving, int *tile_image_dest)
{
    GLint num_sending;
    GLint *tile_list;
    GLint max_pixels;
    GLint num_tiles;

    int i;

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
	imageDestinations = malloc(num_tiles * sizeof(int));
	allocatedTileSize = num_tiles;
    }

  /* Make each element imageDestinations point to the processor to send the
     corresponding image in tile_list. */
    for (i = 0; i < num_sending; i++) {
	imageDestinations[i] = tile_image_dest[tile_list[i]];
    }

    icetSendRecvLargeMessages(num_sending, imageDestinations,
			      rtfi_generateDataFunc, rtfi_handleDataFunc,
			      inImage, icetSparseImageSize(max_pixels));
}

static void startLargeRecv(void *buf, int size, int src, IceTCommRequest *req) {
    *req = ICET_COMM_IRECV(buf, size, ICET_BYTE, src, LARGE_MESSAGE);
}
static void startLargeSend(int dest, IceTCommRequest *req,
			   IceTGenerateData callback, int *sendIds) {
    int data_size;
    void *data;
    data = (*callback)(sendIds[dest], dest, &data_size);
    icetAddSentBytes(data_size);
    *req = ICET_COMM_ISEND(data, data_size, ICET_BYTE, dest, LARGE_MESSAGE);
}
static int *sendIds = NULL;
static char *myDests = NULL;
static char *allDests = NULL;
static int *sendQueue = NULL;
static int *recvQueue = NULL;
static int *recvFrom = NULL;
static int allocatedCommSize = 0;
void icetSendRecvLargeMessages(int numMessagesSending,
			       int *messageDestinations,
			       IceTGenerateData generateDataFunc,
			       IceTHandleData handleDataFunc,
			       void *incomingBuffer,
			       int bufferSize)
{
    int comm_size;
    int rank;
    int i, j;
    int numSend, numRecv;
    int someoneSends;
    int sendToSelf;
    int sqi, rqi;	/* Send/Recv queue index. */

#define RECV_IDX 0
#define SEND_IDX 1
    IceTCommRequest requests[2];

    icetGetIntegerv(ICET_NUM_PROCESSORS, &comm_size);
    icetGetIntegerv(ICET_RANK, &rank);

  /* Make sure we have big enough arrays.  We should not have to allocate
     very often. */
    if (comm_size > allocatedCommSize) {
	free(sendIds);
	free(myDests);
	free(allDests);
	free(sendQueue);
	free(recvQueue);
	free(recvFrom);
	sendIds = malloc(comm_size * sizeof(int));
	myDests = malloc(comm_size);
	allDests = malloc(comm_size * comm_size);
	sendQueue = malloc(comm_size * sizeof(int));
	recvQueue = malloc(comm_size * sizeof(int));
	recvFrom = malloc(comm_size * sizeof(int));
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

  /* Gather masks for all processes. */
    ICET_COMM_ALLGATHER(myDests, comm_size, ICET_BYTE, allDests);

  /* Determine communications.  We will determine communications in a
     series of steps.  At each step, we will try to find the most send/recv
     pairs.  Each step will be able to complete without deadlock.  Rather
     than saving the sends and recieves per step, we will simply push them
     into queues and run the send and receive queues in parallel later. */
    numSend = 0;  numRecv = 0;
    sendToSelf = 0;
    do {
	someoneSends = 0;
      /* We are going to keep track of what every processor receives to
	 ensure that no processor receives more than one message per
	 iteration.  Clear out the array first. */
	for (i = 0; i < comm_size; i++) recvFrom[i] = -1;
      /* Try to find a destination for each sender. */
	for (i = 0; i < comm_size; i++) {
	    char *localDests = allDests + i*comm_size;
	    for (j = 0; j < comm_size; j++) {
		if (localDests[j] && (recvFrom[j] < 0)) {
		    localDests[j] = 0;
		    if (i == j) {
			if (rank == i) sendToSelf = 1;
			continue;
		    }
		    recvFrom[j] = i;
		    if (i == rank) {
			sendQueue[numSend++] = j;
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

    sqi = 0;  rqi = 0;
    if (rqi < numRecv) {
	icetRaiseDebug1("Receiving from %d", recvQueue[rqi]);
	startLargeRecv(incomingBuffer, bufferSize, recvQueue[rqi],
		       &requests[RECV_IDX]);
    } else {
	requests[RECV_IDX] = ICET_COMM_REQUEST_NULL;
    }
    if (sendToSelf) {
	int data_size;
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

void icetBswapCompose(int *compose_group, int group_size,
		      IceTImage imageBuffer,
		      IceTSparseImage inImage, IceTSparseImage outImage)
{
    int i;
    int group_rank;
    int left, right;
    GLint rank;
    int pow2size;
    int compressedSize;
    GLuint offset;
    GLuint pixels;
    IceTCommRequest *requests;
    GLenum output_buffers;

    icetRaiseDebug("In icetBswapCompose");

    icetGetIntegerv(ICET_RANK, &rank);
    for (group_rank = 0; compose_group[group_rank] != rank; group_rank++);

    pixels = icetGetImagePixelCount(imageBuffer);

  /* Make size of group be a power of 2. */
    for (pow2size = 1; pow2size <= group_size; pow2size *= 2);
    pow2size /= 2;
    if (group_rank >= pow2size) {
      /* Give up image and drop out of computation. */
	compressedSize = icetCompressImage(imageBuffer, outImage);
	icetAddSentBytes(compressedSize);
	ICET_COMM_SEND(outImage, compressedSize, ICET_BYTE,
		       compose_group[group_rank - pow2size], SWAP_IMAGE_DATA);
	return;
    } else if (group_rank < group_size - pow2size) {
      /* Absorb image. */
	ICET_COMM_RECV(inImage, icetSparseImageSize(pixels), ICET_BYTE,
		 compose_group[group_rank + pow2size], SWAP_IMAGE_DATA);
	icetCompressedComposite(imageBuffer, inImage);
    }
    group_size = pow2size;

    offset = 0;
  /* Make sure we can divide pixels evenly amongst processors. */
    pixels = (pixels/group_size + 1)*group_size;

    left = 0;
    right = group_size;
    while ((right-left) > 1) {
	int middle   = (right+left)/2;
	int sub_size = (right-left)/2;
	int pair;

	pixels /= 2;

	if (group_rank < middle) {
	    compressedSize = icetCompressSubImage(imageBuffer,
						  offset + pixels, pixels,
						  outImage);
	    pair = compose_group[group_rank + sub_size];

	    right = middle;
	} else {
	    compressedSize = icetCompressSubImage(imageBuffer,
						  offset, pixels,
						  outImage);
	    pair = compose_group[group_rank - sub_size];

	    offset += pixels;

	    left = middle;
	}

	icetAddSentBytes(compressedSize);
	ICET_COMM_SENDRECV(outImage, compressedSize,
			   ICET_BYTE, pair, SWAP_IMAGE_DATA,
			   inImage, icetSparseImageSize(pixels),
			   ICET_BYTE, pair, SWAP_IMAGE_DATA);

	icetCompressedSubComposite(imageBuffer, offset, pixels, inImage);
    }

  /* Collect pieces */
  /* All processors have the same number for pixels and their offset
   * is group_rank*offset. */
    icetGetIntegerv(ICET_OUTPUT_BUFFERS, &output_buffers);
    if (group_rank == 0) {
	requests = malloc((group_size-1)*sizeof(IceTCommRequest));
    }
    if ((output_buffers & ICET_COLOR_BUFFER_BIT) != 0) {
	GLubyte *colorBuffer = icetGetImageColorBuffer(imageBuffer);
	if (group_rank == 0) {
	    icetRaiseDebug("Collecting image data.");
	    for (i = 1; i < group_size; i++) {
		requests[i-1] =
		    ICET_COMM_IRECV(colorBuffer + 4*pixels*i, 4*pixels,
				    ICET_BYTE, compose_group[i],
				    SWAP_IMAGE_DATA);
	    }
	    for (i = 0; i < group_size-1; i++) {
		ICET_COMM_WAIT(requests + i);
	    }
	} else {
	    icetRaiseDebug("Sending image data.");
	    icetAddSentBytes(4*pixels);
	    ICET_COMM_SEND(colorBuffer + 4*offset, 4*pixels, ICET_BYTE,
			   compose_group[0], SWAP_IMAGE_DATA);
	}
    }
    if ((output_buffers & ICET_DEPTH_BUFFER_BIT) != 0) {
	GLuint *depthBuffer = icetGetImageDepthBuffer(imageBuffer);
	if (group_rank == 0) {
	    icetRaiseDebug("Collecting depth data.");
	    for (i = 1; i < group_size; i++) {
		requests[i-1] =
		    ICET_COMM_IRECV(depthBuffer + pixels*i, pixels, ICET_INT,
				    compose_group[i], SWAP_DEPTH_DATA);
	    }
	    for (i = 0; i < group_size-1; i++) {
		ICET_COMM_WAIT(requests + i);
	    }
	} else {
	    icetRaiseDebug("Sending depth data.");
	    icetAddSentBytes(4*pixels);
	    ICET_COMM_SEND(depthBuffer + offset, pixels, ICET_INT,
			   compose_group[0], SWAP_DEPTH_DATA);
	}
    }
    if (group_rank == 0) {
	free(requests);
    }
}

void icetTreeCompose(int *compose_group, int group_size,
		     IceTImage imageBuffer,
		     IceTSparseImage compressedImageBuffer)
{
    int group_rank;
    GLint rank;
    GLuint incomingBufferSize;

    icetGetIntegerv(ICET_RANK, &rank);
    for (group_rank = 0; compose_group[group_rank] != rank; group_rank++);

    incomingBufferSize
	= icetSparseImageSize(icetGetImagePixelCount(imageBuffer));

    while (group_size > 1) {
	int half_size = (group_size+1)/2;	/* Round up. */
	if (group_rank >= half_size) {
	    int compressedSize;
	    compressedSize = icetCompressImage(imageBuffer,
					       compressedImageBuffer);
	    icetAddSentBytes(compressedSize);
	    ICET_COMM_SEND(compressedImageBuffer, compressedSize, ICET_BYTE,
			   compose_group[group_rank - half_size],
			   TREE_IMAGE_DATA);
	    return;
	} else if (group_rank + half_size < group_size) {
	    ICET_COMM_RECV(compressedImageBuffer, incomingBufferSize, ICET_BYTE,
			   compose_group[group_rank + half_size],
			   TREE_IMAGE_DATA);
	    icetCompressedComposite(imageBuffer, compressedImageBuffer);
	}
	group_size = half_size;
    }
}
