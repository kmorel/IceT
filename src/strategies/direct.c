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
#include <context.h>
#include <state.h>
#include <diagnostics.h>
#include <string.h>
#include "common.h"

static IceTImage directCompose(void);

IceTStrategy ICET_STRATEGY_DIRECT = { "Direct", ICET_TRUE, directCompose };

static IceTImage directCompose(void)
{
    IceTSparseImage inImage;
    IceTSparseImage outImage;
    IceTImage image;
    GLint *contrib_counts;
    GLint *display_nodes;
    GLint max_pixels;
    GLint num_tiles;
    GLint num_contributors;
    GLint display_tile;
    GLint tile;
    GLint *tile_image_dest;
    icetRaiseDebug("In Direct Compose");

    icetGetIntegerv(ICET_TILE_MAX_PIXELS, &max_pixels);
    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);

    icetResizeBuffer(  2*icetSparseImageSize(max_pixels)
		     + icetFullImageSize(max_pixels)
		     + num_tiles*sizeof(GLint));
    inImage     = icetReserveBufferMem(icetSparseImageSize(max_pixels));
    outImage    = icetReserveBufferMem(icetSparseImageSize(max_pixels));
    image	= icetReserveBufferMem(icetFullImageSize(max_pixels));
    tile_image_dest = icetReserveBufferMem(num_tiles*sizeof(GLint));

    icetGetIntegerv(ICET_TILE_DISPLAYED, &display_tile);
    if (display_tile >= 0) {
	contrib_counts = icetUnsafeStateGet(ICET_TILE_CONTRIB_COUNTS);
	num_contributors = contrib_counts[display_tile];
    } else {
	num_contributors = 0;
    }

    display_nodes = icetUnsafeStateGet(ICET_DISPLAY_NODES);
    for (tile = 0; tile < num_tiles; tile++) {
	tile_image_dest[tile] = display_nodes[tile];
    }

    icetRaiseDebug("Rendering and transferring images.");
    icetRenderTransferFullImages(image, inImage, outImage,
				 num_contributors, tile_image_dest);

    if ((display_tile >= 0) && (num_contributors < 1)) {
      /* Must be displaying a blank tile. */
	icetRaiseDebug("Returning blank tile.");
	icetInitializeImage(image, max_pixels);
	icetClearImage(image);
    }

    return image;
}
