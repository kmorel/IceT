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

IceTStrategy ICET_STRATEGY_SERIAL = { "Serial", serialCompose };

static IceTImage serialCompose(void)
{
    GLint num_tiles;
    GLint max_pixels;
    GLint rank;
    GLint num_proc;
    GLint *display_nodes;
    IceTImage myImage;
    IceTImage imageBuffer;
    IceTSparseImage inImage, outImage;
    int *compose_group;
    int i;

    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);
    icetGetIntegerv(ICET_TILE_MAX_PIXELS, &max_pixels);
    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSORS, &num_proc);
    display_nodes = icetUnsafeStateGet(ICET_DISPLAY_NODES);

    icetResizeBuffer(  icetFullImageSize(max_pixels)*2
		     + icetSparseImageSize(max_pixels)*2
		     + sizeof(int)*num_proc);
    myImage       = NULL;
    imageBuffer   = icetReserveBufferMem(icetFullImageSize(max_pixels));
    inImage       = icetReserveBufferMem(icetSparseImageSize(max_pixels));
    outImage      = icetReserveBufferMem(icetSparseImageSize(max_pixels));
    compose_group = icetReserveBufferMem(sizeof(int)*num_proc);

    for (i = 0; i < num_proc; i++) {
	compose_group[i] = i;
    }

  /* Render and compose every tile. */
    for (i = 0; i < num_tiles; i++) {
	IceTImage ibuf;
	int d_node = display_nodes[i];

      /* Put the display node at the beginning of the group so it gets the
	 image. */
	compose_group[0] = d_node;
	compose_group[d_node] = 0;

      /* If this processor is display node, make sure image goes to
         myColorBuffer. */
	if (d_node == rank) {
	    myImage = icetReserveBufferMem(icetFullImageSize(max_pixels));
	    ibuf = myImage;
	} else {
	    ibuf = imageBuffer;
	}

	icetGetTileImage(i, ibuf);
	icetBswapCompose(compose_group, num_proc, ibuf, inImage, outImage);

      /* Restore compose_group for next tile. */
	compose_group[d_node] = d_node;
    }

    return myImage;
}
