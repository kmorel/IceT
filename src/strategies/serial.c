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
#include "common.h"

static IceTImage serialCompose(void);

IceTStrategy ICET_STRATEGY_SERIAL = { "Serial", ICET_TRUE, serialCompose };

static IceTImage serialCompose(void)
{
    GLint num_tiles;
    GLint max_pixels;
    GLint rank;
    GLint num_proc;
    GLint *display_nodes;
    GLboolean ordered_composite;
    IceTImage myImage;
    IceTImage imageBuffer;
    IceTSparseImage inImage, outImage;
    int *compose_group;
    int i;

    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);
    icetGetIntegerv(ICET_TILE_MAX_PIXELS, &max_pixels);
    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);
    display_nodes = icetUnsafeStateGet(ICET_DISPLAY_NODES);
    ordered_composite = icetIsEnabled(ICET_ORDERED_COMPOSITE);

    icetResizeBuffer(  icetFullImageSize(max_pixels)*2
		     + icetSparseImageSize(max_pixels)*2
		     + sizeof(int)*num_proc);
    myImage       = NULL;
    imageBuffer   = icetReserveBufferMem(icetFullImageSize(max_pixels));
    inImage       = icetReserveBufferMem(icetSparseImageSize(max_pixels));
    outImage      = icetReserveBufferMem(icetSparseImageSize(max_pixels));
    compose_group = icetReserveBufferMem(sizeof(int)*num_proc);

    if (ordered_composite) {
	icetGetIntegerv(ICET_COMPOSITE_ORDER, compose_group);
    } else {
	for (i = 0; i < num_proc; i++) {
	    compose_group[i] = i;
	}
    }

  /* Render and compose every tile. */
    for (i = 0; i < num_tiles; i++) {
	IceTImage ibuf;
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
	    myImage = icetReserveBufferMem(icetFullImageSize(max_pixels));
	    ibuf = myImage;
	} else {
	    ibuf = imageBuffer;
	}

	icetGetTileImage(i, ibuf);
	icetBswapCompose(compose_group, num_proc, image_dest,
			 ibuf, inImage, outImage);
    }

    return myImage;
}
