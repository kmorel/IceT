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

#include <GL/ice-t.h>
#include <image.h>
#include <state.h>
#include <context.h>
#include <diagnostics.h>
#include "common.h"

#include <stdlib.h>
#include <string.h>

#define IMAGE_DATA        50
#define COLOR_DATA        51
#define DEPTH_DATA        52

static IceTImage splitStrategy(void);

IceTStrategy ICET_STRATEGY_SPLIT
    = { "Image Split and Delegate", ICET_FALSE, splitStrategy };

static IceTImage splitStrategy(void)
{
    int *tile_groups;
    int my_tile;
    int group_size;
    int fragment_size;

    GLint rank;
    GLint num_proc;
    GLint num_tiles;
    GLint max_pixels;
    GLint *tile_contribs;
    GLint total_image_count;
    GLint *display_nodes;
    GLint tile_displayed;
    GLenum output_buffers;

    GLint num_contained_tiles;
    GLint *contained_tiles_list;
    GLboolean *all_contained_tiles_masks;

    int tile, image, node;
    int num_allocated;

    IceTSparseImage *incoming;
    IceTSparseImage outgoing;
    IceTImage imageFragment;
    IceTImage fullImage;

    int num_requests;
    IceTCommRequest *requests;

    int first_incoming = 1;

    icetRaiseDebug("In splitStrategy");

    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);
    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);
    icetGetIntegerv(ICET_TILE_MAX_PIXELS, &max_pixels);
    tile_contribs = icetUnsafeStateGet(ICET_TILE_CONTRIB_COUNTS);
    icetGetIntegerv(ICET_TOTAL_IMAGE_COUNT, &total_image_count);
    display_nodes = icetUnsafeStateGet(ICET_DISPLAY_NODES);
    icetGetIntegerv(ICET_TILE_DISPLAYED, &tile_displayed);
    icetGetIntegerv(ICET_NUM_CONTAINED_TILES, &num_contained_tiles);
    contained_tiles_list = icetUnsafeStateGet(ICET_CONTAINED_TILES_LIST);
    all_contained_tiles_masks
        = icetUnsafeStateGet(ICET_ALL_CONTAINED_TILES_MASKS);

  /* Special case: no images rendered whatsoever. */
    if (total_image_count < 1) {
        icetRaiseDebug("Not rendering any images.  Quit early.");
        if (tile_displayed >= 0) {
            icetResizeBuffer(icetFullImageSize(max_pixels));
            fullImage =        icetReserveBufferMem(icetFullImageSize(max_pixels));
            icetInitializeImage(fullImage, max_pixels);
            icetClearImage(fullImage);
        } else {
            fullImage = NULL;
        }
        return fullImage;
    }

    tile_groups = malloc(sizeof(int)*(num_tiles+1));

    num_allocated = 0;
    tile_groups[0] = 0;
  /* Set entry of tile_groups[i+1] to the number of processes to help
     compose the image in tile i. */
    for (tile = 0; tile < num_tiles; tile++) {
        int allocate = (tile_contribs[tile]*num_proc)/total_image_count;
        if ((allocate < 1) && (tile_contribs[tile] > 0)) {
            allocate = 1;
        }
        tile_groups[tile+1] = allocate;
        num_allocated += allocate;
    }

  /* Make the number of processes allocated equal exactly the number of
     processes available. */
    while (num_allocated < num_proc) {
      /* Add processes to the tile with the lowest process:image ratio. */
        int min_id = -1;
        float min_ratio = (float)num_proc;
        for (tile = 0; tile < num_tiles; tile++) {
            float ratio;
          /* Don't even consider tiles with no contributors. */
            if (tile_contribs[tile] == 0) continue;
            ratio = (float)tile_groups[tile+1]/tile_contribs[tile];
            if (ratio < min_ratio) {
                min_ratio = ratio;
                min_id = tile;
            }
        }
#ifdef DEBUG
        if (min_id < 0) {
            icetRaiseError("Could not find candidate to add tile.",
                           ICET_SANITY_CHECK_FAIL);
        }
#endif
        tile_groups[min_id+1]++;
        num_allocated++;
    }
    while (num_allocated > num_proc) {
      /* Remove processes from the tile with the highest process:image
         ratio. */
        int max_id = -1;
        float max_ratio = 0;
        for (tile = 0; tile < num_tiles; tile++) {
            float ratio;
          /* Don't even consider tiles with a minimum allocation. */
            if (tile_groups[tile+1] <= 1) continue;
            ratio = (float)tile_groups[tile+1]/tile_contribs[tile];
            if (ratio > max_ratio) {
                max_ratio = ratio;
                max_id = tile;
            }
        }
#ifdef DEBUG
        if (max_id < 0) {
            icetRaiseError("Could not find candidate to remove tile.",
                           ICET_SANITY_CHECK_FAIL);
        }
#endif
        tile_groups[max_id+1]--;
        num_allocated--;
    }

  /* Processes are assigned sequentially from 0 to N to each tile as
     needed.  Change each tile_groups[i] entry to be the lowest rank of the
     processes assigned to tile i.  Thus the processes assigned to tile i
     are tile_groups[i] through tile_groups[i+1]-1. */
    for (tile = 1; tile < num_tiles; tile++) {
        tile_groups[tile] += tile_groups[tile-1];
    }
    tile_groups[num_tiles] = num_proc;

  /* Figure out which tile I am assigned to. */
    for (my_tile = 0; rank >= tile_groups[my_tile+1]; my_tile++);

    group_size = tile_groups[my_tile+1] - tile_groups[my_tile];
    fragment_size = max_pixels/group_size;

    num_requests = tile_contribs[my_tile];
    if (num_requests < 2) num_requests = 2;

    icetResizeBuffer(  sizeof(IceTSparseImage)*tile_contribs[my_tile]
                     + icetFullImageSize(fragment_size)
                     + icetSparseImageSize(max_pixels)
                     + icetFullImageSize(max_pixels)
                     + icetSparseImageSize(fragment_size)*tile_contribs[my_tile]
                     + sizeof(IceTCommRequest)*num_requests);
    incoming
        = icetReserveBufferMem(sizeof(IceTSparseImage)*tile_contribs[my_tile]);
    outgoing = icetReserveBufferMem(icetSparseImageSize(max_pixels));
    imageFragment = icetReserveBufferMem(icetFullImageSize(fragment_size));
    fullImage = icetReserveBufferMem(icetFullImageSize(max_pixels));
    requests = icetReserveBufferMem(sizeof(IceTCommRequest)*num_requests);

  /* Set up asynchronous receives for all incoming image fragments. */
/*      for (image = 0; image < tile_contribs[my_tile]; image++) { */
/*          incoming[image] */
/*              = icetReserveBufferMem(icetSparseImageSize(fragment_size)); */
/*          MPI_Irecv(incoming[image], icetSparseImageSize(fragment_size), */
/*                    MPI_BYTE, MPI_ANY_SOURCE, IMAGE_DATA, */
/*                    icetGetCommunicator(), requests + image); */
/*      } */
    for (image = 0, node = 0; image < tile_contribs[my_tile]; node++) {
        if (all_contained_tiles_masks[node*num_tiles + my_tile]) {
            icetRaiseDebug1("Setting up receive from node %d", node);
            incoming[image]
                = icetReserveBufferMem(icetSparseImageSize(fragment_size));
            requests[image] =
                ICET_COMM_IRECV(incoming[image],
                                icetSparseImageSize(fragment_size),
                                ICET_BYTE, node, IMAGE_DATA);
            image++;
        }
    }

  /* Render and send all tile images I am rendering. */
    for (image = 0; image < num_contained_tiles; image++) {
        int sending_frag_size;
        int compressedSize;
        GLuint offset;

        tile = contained_tiles_list[image];
        icetGetTileImage(tile, fullImage);
        icetRaiseDebug1("Got image for tile %d", tile);
        offset = 0;
        sending_frag_size = max_pixels/(tile_groups[tile+1]-tile_groups[tile]);
        for (node = tile_groups[tile]; node < tile_groups[tile+1]; node++) {
            icetRaiseDebug2("Sending tile %d to node %d", tile, node);
            compressedSize = icetCompressSubImage(fullImage, offset,
                                                  sending_frag_size, outgoing);
            icetAddSentBytes(compressedSize);
            ICET_COMM_SEND(outgoing, compressedSize,
                           ICET_BYTE, node, IMAGE_DATA);
            offset += sending_frag_size;
        }
    }

  /* Wait for images to come in and Z compare them. */
    for (image = 0; image < tile_contribs[my_tile]; image++) {
        int idx;
        idx = ICET_COMM_WAITANY(tile_contribs[my_tile], requests);
        if (first_incoming) {
            icetRaiseDebug1("Got first image (%d).", idx);
            icetDecompressImage(incoming[idx], imageFragment);
            first_incoming = 0;
        } else {
            icetRaiseDebug1("Got subsequent image (%d).", idx);
            icetCompressedComposite(imageFragment,
                                    incoming[idx], 1);
        }
    }

  /* Send composited fragment to display process. */
    icetGetIntegerv(ICET_OUTPUT_BUFFERS, (GLint *)&output_buffers);
    if ((output_buffers & ICET_COLOR_BUFFER_BIT) != 0) {
        icetAddSentBytes(4*fragment_size);
        requests[0] = ICET_COMM_ISEND(icetGetImageColorBuffer(imageFragment),
                                      4*fragment_size,
                                      ICET_BYTE, display_nodes[my_tile],
                                      COLOR_DATA);
    }
    if ((output_buffers & ICET_DEPTH_BUFFER_BIT) != 0) {
        icetAddSentBytes(4*fragment_size);
        requests[1] = ICET_COMM_ISEND(icetGetImageDepthBuffer(imageFragment),
                                      fragment_size, ICET_INT,
                                      display_nodes[my_tile], DEPTH_DATA);
    }

  /* If I am displaying a tile, receive image data. */
    if (tile_displayed >= 0) {
        icetInitializeImage(fullImage, max_pixels);
      /* Check to make sure tile is not blank. */
        if (tile_groups[tile_displayed+1] > tile_groups[tile_displayed]) {
            int my_frag_size = max_pixels/(  tile_groups[tile_displayed+1]
                                           - tile_groups[tile_displayed]);
            if ((output_buffers & ICET_COLOR_BUFFER_BIT) != 0) {
                GLubyte *cb = icetGetImageColorBuffer(fullImage);
                for (node = tile_groups[tile_displayed];
                     node < tile_groups[tile_displayed+1]; node++) {
                    icetRaiseDebug1("Getting final color fragment from %d",
                                    node);
                    ICET_COMM_RECV(cb, 4*my_frag_size,
                                   ICET_BYTE, node, COLOR_DATA);
                    cb += 4*my_frag_size;
                }
            }
            if ((output_buffers & ICET_DEPTH_BUFFER_BIT) != 0) {
                GLuint *db = icetGetImageDepthBuffer(fullImage);
                for (node = tile_groups[tile_displayed];
                     node < tile_groups[tile_displayed+1]; node++) {
                    icetRaiseDebug1("Getting final depth fragment from %d",
                                    node);
                    ICET_COMM_RECV(db, my_frag_size,
                                   ICET_INT, node, DEPTH_DATA);
                    db += my_frag_size;
                }
            }
        } else {
            icetClearImage(fullImage);
        }
    }

    if ((output_buffers & ICET_COLOR_BUFFER_BIT) != 0) {
        ICET_COMM_WAIT(requests);
    }
    if ((output_buffers & ICET_DEPTH_BUFFER_BIT) != 0) {
        ICET_COMM_WAIT(requests + 1);
    }

    free(tile_groups);

    return fullImage;
}
