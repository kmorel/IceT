/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#include <IceT.h>

#include <image.h>
#include <context.h>
#include <state.h>
#include <diagnostics.h>
#include <string.h>
#include "common.h"

static IceTImage directCompose(void);

IceTStrategy ICET_STRATEGY_DIRECT = { "Direct", ICET_TRUE, directCompose };

static IceTImage directCompose(void)
{
    IceTImage image;
    IceTVoid *inSparseImageBuffer;
    IceTSparseImage outSparseImage;
    IceTSizeType rawImageSize, sparseImageSize;
    IceTInt *contrib_counts;
    IceTInt *display_nodes;
    IceTInt max_pixels;
    IceTInt num_tiles;
    IceTInt num_contributors;
    IceTInt display_tile;
    IceTInt tile;
    IceTInt *tile_image_dest;
    icetRaiseDebug("In Direct Compose");

    icetGetIntegerv(ICET_TILE_MAX_PIXELS, &max_pixels);
    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);

    rawImageSize = icetImageBufferSize(max_pixels);
    sparseImageSize = icetSparseImageBufferSize(max_pixels);

    icetResizeBuffer(  rawImageSize
		     + 2*sparseImageSize
		     + num_tiles*sizeof(IceTInt));
    image               = icetReserveBufferImage(max_pixels);
    inSparseImageBuffer = icetReserveBufferMem(sparseImageSize);
    outSparseImage      = icetReserveBufferSparseImage(max_pixels);
    tile_image_dest     = icetReserveBufferMem(num_tiles*sizeof(IceTInt));

    icetGetIntegerv(ICET_TILE_DISPLAYED, &display_tile);
    if (display_tile >= 0) {
	contrib_counts = icetUnsafeStateGetInteger(ICET_TILE_CONTRIB_COUNTS);
	num_contributors = contrib_counts[display_tile];
    } else {
	num_contributors = 0;
    }

    display_nodes = icetUnsafeStateGetInteger(ICET_DISPLAY_NODES);
    for (tile = 0; tile < num_tiles; tile++) {
	tile_image_dest[tile] = display_nodes[tile];
    }

    icetRaiseDebug("Rendering and transferring images.");
    icetRenderTransferFullImages(image,
                                 inSparseImageBuffer,
                                 outSparseImage,
                                 tile_image_dest);

    if ((display_tile >= 0) && (num_contributors < 1)) {
      /* Must be displaying a blank tile. */
	icetRaiseDebug("Returning blank tile.");
        icetClearImage(image);
    }

    return image;
}
