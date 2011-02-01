/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include <IceT.h>

#include <IceTDevImage.h>
#include <IceTDevState.h>
#include <IceTDevDiagnostics.h>
#include "common.h"

#define SEQUENTIAL_IMAGE_BUFFER                 ICET_STRATEGY_BUFFER_0
#define SEQUENTIAL_FINAL_IMAGE_BUFFER           ICET_STRATEGY_BUFFER_1
#define SEQUENTIAL_INTERMEDIATE_IMAGE_BUFFER    ICET_STRATEGY_BUFFER_2
#define SEQUENTIAL_COMPOSE_GROUP_BUFFER         ICET_STRATEGY_BUFFER_3

IceTImage icetSequentialCompose(void)
{
    IceTInt num_tiles;
    IceTInt rank;
    IceTInt num_proc;
    const IceTInt *display_nodes;
    const IceTInt *tile_viewports;
    IceTBoolean ordered_composite;
    IceTImage my_image;
    IceTInt *compose_group;
    int i;

    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);
    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);
    display_nodes = icetUnsafeStateGetInteger(ICET_DISPLAY_NODES);
    tile_viewports = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS);
    ordered_composite = icetIsEnabled(ICET_ORDERED_COMPOSITE);

    compose_group = icetGetStateBuffer(SEQUENTIAL_COMPOSE_GROUP_BUFFER,
                                       sizeof(IceTInt)*num_proc);

    my_image = icetImageNull();

    if (ordered_composite) {
	icetGetIntegerv(ICET_COMPOSITE_ORDER, compose_group);
    } else {
	for (i = 0; i < num_proc; i++) {
	    compose_group[i] = i;
	}
    }

  /* Render and compose every tile. */
    for (i = 0; i < num_tiles; i++) {
        IceTImage tile_image;
	int d_node = display_nodes[i];
	int image_dest;
        IceTSparseImage rendered_image;
        IceTSparseImage composited_image;
        IceTSizeType piece_offset;
        IceTSizeType tile_width;
        IceTSizeType tile_height;

        tile_width = tile_viewports[4*i + 2];
        tile_height = tile_viewports[4*i + 3];

      /* Make the image go to the display node. */
	if (ordered_composite) {
	    for (image_dest = 0; compose_group[image_dest] != d_node;
		 image_dest++);
	} else {
	  /* Technically, the above computation will work, but this is
	     faster. */
	    image_dest = d_node;
	}


        rendered_image = icetGetStateBufferSparseImage(SEQUENTIAL_IMAGE_BUFFER,
                                                       tile_width, tile_height);

        icetGetCompressedTileImage(i, rendered_image);
	icetSingleImageCompose(compose_group,
                               num_proc,
                               image_dest,
                               rendered_image,
                               &composited_image,
                               &piece_offset);

      /* If this processor is display node, make sure image goes to
         myColorBuffer. */
	if (d_node == rank) {
            tile_image = icetGetStateBufferImage(SEQUENTIAL_FINAL_IMAGE_BUFFER,
                                                 tile_width, tile_height);
	} else {
            tile_image = icetGetStateBufferImage(
                                           SEQUENTIAL_INTERMEDIATE_IMAGE_BUFFER,
                                           tile_width, tile_height);
	}

        icetSingleImageCollect(composited_image,
                               d_node,
                               piece_offset,
                               tile_image);

        if (d_node == rank) {
            my_image = tile_image;
        }
    }

    return my_image;
}
