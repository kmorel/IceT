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
#include "common.h"

static IceTImage serialCompose(void);

IceTStrategy ICET_STRATEGY_SERIAL = { "Serial", ICET_TRUE, serialCompose };

static IceTImage serialCompose(void)
{
    IceTInt num_tiles;
    IceTInt max_pixels;
    IceTInt rank;
    IceTInt num_proc;
    IceTInt *display_nodes;
    IceTBoolean ordered_composite;
    IceTImage myImage;
    IceTVoid *imageBuffer;
    IceTVoid *inSparseImageBuffer, *outSparseImageBuffer;
    IceTSizeType rawImageSize, sparseImageSize;
    IceTInt *compose_group;
    IceTEnum color_format, depth_format;
    int i;

    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);
    icetGetIntegerv(ICET_TILE_MAX_PIXELS, &max_pixels);
    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);
    display_nodes = icetUnsafeStateGetInteger(ICET_DISPLAY_NODES);
    ordered_composite = icetIsEnabled(ICET_ORDERED_COMPOSITE);
    icetGetEnumv(ICET_COLOR_FORMAT, &color_format);
    icetGetEnumv(ICET_DEPTH_FORMAT, &depth_format);

    rawImageSize    = icetImageBufferSize(color_format, depth_format,
                                          max_pixels);
    sparseImageSize = icetSparseImageBufferSize(color_format, depth_format,
                                                max_pixels);

    icetResizeBuffer(  rawImageSize*2
		     + sparseImageSize*2
		     + sizeof(int)*num_proc);
    imageBuffer          = icetReserveBufferMem(rawImageSize);
    inSparseImageBuffer  = icetReserveBufferMem(sparseImageSize);
    outSparseImageBuffer = icetReserveBufferMem(sparseImageSize);
    compose_group = icetReserveBufferMem(sizeof(IceTInt)*num_proc);

    myImage = icetImageNull();

    if (ordered_composite) {
	icetGetIntegerv(ICET_COMPOSITE_ORDER, compose_group);
    } else {
	for (i = 0; i < num_proc; i++) {
	    compose_group[i] = i;
	}
    }

  /* Render and compose every tile. */
    for (i = 0; i < num_tiles; i++) {
	IceTVoid *ibuf;
        IceTImage image;
	int d_node = display_nodes[i];
	int image_dest;

      /* Make the image go to the display node. */
	if (ordered_composite) {
	    for (image_dest = 0; compose_group[image_dest] != d_node;
		 image_dest++);
	} else {
	  /* Technically, the above computation will work, but this is
	     faster. */
	    image_dest = d_node;
	}

      /* If this processor is display node, make sure image goes to
         myColorBuffer. */
	if (d_node == rank) {
	    ibuf = icetReserveBufferMem(rawImageSize);
	} else {
	    ibuf = imageBuffer;
	}

	image = icetGetTileImage(i, ibuf);
	icetBswapCompose(compose_group, num_proc, image_dest,
			 image, inSparseImageBuffer, outSparseImageBuffer);

        if (d_node == rank) {
            myImage = image;
        }
    }

    return myImage;
}
