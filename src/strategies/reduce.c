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

static IceTImage reduceCompose(void);
static int delegate(int **tile_image_destp,
		    int **compose_groupp, int *group_sizep,
		    int *group_image_destp,
		    int *num_receivingp,
		    int buffer_size);


IceTStrategy ICET_STRATEGY_REDUCE = { "Reduce", ICET_TRUE, reduceCompose };

static IceTImage reduceCompose(void)
{
    IceTSparseImage inImage;
    IceTSparseImage outImage;
    IceTImage image;
    GLint max_pixels;
    GLint num_processes;
    GLint tile_displayed;
    int buffer_size;

    int *tile_image_dest;
    int *compose_group, group_size, group_image_dest;
    int num_receiving;
    int compose_tile;

    icetRaiseDebug("In reduceCompose");

    icetGetIntegerv(ICET_NUM_PROCESSES, &num_processes);
    icetGetIntegerv(ICET_TILE_MAX_PIXELS, &max_pixels);

    buffer_size = (  2*icetSparseImageSize(max_pixels)
		   + icetFullImageSize(max_pixels));
    compose_tile = delegate(&tile_image_dest,
			    &compose_group, &group_size, &group_image_dest,
			    &num_receiving,
			    buffer_size);

    inImage  = icetReserveBufferMem(icetSparseImageSize(max_pixels));
    outImage = icetReserveBufferMem(icetSparseImageSize(max_pixels));
    image    = icetReserveBufferMem(icetFullImageSize(max_pixels));

    icetRenderTransferFullImages(image, inImage, outImage,
				 num_receiving, tile_image_dest);

    if (group_size >= 8) {
	icetRaiseDebug("Doing bswap compose");
	icetBswapCompose(compose_group, group_size, group_image_dest,
			 image, inImage, outImage);
    } else if (group_size > 0) {
	icetRaiseDebug("Doing tree compose");
	icetTreeCompose(compose_group, group_size, group_image_dest,
			image, inImage);
    } else {
	icetRaiseDebug("Clearing pixels");
	icetInitializeImage(image, max_pixels);
	icetClearImage(image);
    }

    icetGetIntegerv(ICET_TILE_DISPLAYED, &tile_displayed);
    if ((tile_displayed >= 0) && (tile_displayed != compose_tile)) {
      /* Clear tile if nothing drawn in it. */
	icetRaiseDebug("Clearing pixels");
	icetInitializeImage(image, max_pixels);
	icetClearImage(image);
    }

    return image;
}

static int delegate(int **tile_image_destp,
		    int **compose_groupp, int *group_sizep,
		    int *group_image_destp,
		    int *num_receivingp,
		    int buffer_size)
{
    GLboolean *all_contained_tiles_masks;
    GLint *contrib_counts;
    GLint total_image_count;

    GLint num_tiles;
    GLint num_processes;
    GLint rank;
    GLint *tile_display_nodes;
    GLint *composite_order;

    int *num_proc_for_tile;
    int *node_assignment;
    int *tile_proc_groups;
    int *group_sizes;
    int *tile_image_dest;
    int group_image_dest = 0;
    int *contributors;

    int pcount;

    int tile, node;
    int snode, rnode, dest;
    int piece;
    int first_loop;
    int num_receiving;

    all_contained_tiles_masks
	= icetUnsafeStateGet(ICET_ALL_CONTAINED_TILES_MASKS);
    contrib_counts = icetUnsafeStateGet(ICET_TILE_CONTRIB_COUNTS);
    icetGetIntegerv(ICET_TOTAL_IMAGE_COUNT, &total_image_count);
    tile_display_nodes = icetUnsafeStateGet(ICET_DISPLAY_NODES);
    composite_order = icetUnsafeStateGet(ICET_COMPOSITE_ORDER);

    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);
    icetGetIntegerv(ICET_NUM_PROCESSES, &num_processes);
    icetGetIntegerv(ICET_RANK, &rank);

    if (total_image_count < 1) {
	icetRaiseDebug("No nodes are drawing.");
	*group_sizep = 0;
	*num_receivingp = 0;
	icetResizeBuffer(buffer_size);
	return -1;
    }

    icetResizeBuffer(  num_tiles*sizeof(int)
		     + num_processes*sizeof(int)
		     + num_tiles*num_processes*sizeof(int)
		     + num_tiles*sizeof(int)
		     + num_tiles*sizeof(int)
		     + num_processes*sizeof(int)
		     + buffer_size);
    num_proc_for_tile = icetReserveBufferMem(num_tiles * sizeof(int));
    node_assignment   = icetReserveBufferMem(num_processes * sizeof(int));
    tile_proc_groups  = icetReserveBufferMem(  num_tiles * num_processes
					     * sizeof(int));
    group_sizes       = icetReserveBufferMem(num_tiles * sizeof(int));
    tile_image_dest   = icetReserveBufferMem(num_tiles * sizeof(int));
    contributors      = icetReserveBufferMem(num_processes * sizeof(int));

  /* Decide the minimum amount of processes that should be added to each
     tile. */
    pcount = 0;
    for (tile = 0; tile < num_tiles; tile++) {
	int allocate = (contrib_counts[tile]*num_processes)/total_image_count;
      /* Make sure at least one process is assigned to tiles that have at
	 least one image. */
	if ((allocate < 1) && (contrib_counts[tile] > 0)) allocate = 1;
      /* Don't allocate more processes to a tile than images for that
         tile. */
	if (allocate > contrib_counts[tile]) allocate = contrib_counts[tile];

	num_proc_for_tile[tile] = allocate;

      /* Keep track of how many processes have been allocated. */
	pcount += allocate;
    }

  /* Handle when we have not allocated all the processes. */
    while (num_processes > pcount) {
      /* Find the tile with the largest image to process ratio that
	 can still have a process added to it. */
	int max = 0;
	for (tile = 1; tile < num_tiles; tile++) {
	    if (   (num_proc_for_tile[tile] < contrib_counts[tile])
		&& (   (num_proc_for_tile[max] == contrib_counts[max])
		    || (  (float)contrib_counts[max]/num_proc_for_tile[max]
			< (float)contrib_counts[tile]/num_proc_for_tile[tile])))
	    {
		max = tile;
	    }
	}
	if (num_proc_for_tile[max] < contrib_counts[max]) {
	    num_proc_for_tile[max]++;
	    pcount++;
	} else {
	  /* Cannot assign any more processes. */
	    break;
	}
    }

  /* Handle when we have allocated too many processes. */
    while (num_processes < pcount) {
      /* Find tile with the smallest image to process ratio that can still
	 have a process removed. */
	int min = 0;
	for (tile = 1; tile < num_tiles; tile++) {
	    if (   (num_proc_for_tile[tile] > 1)
		&& (   (num_proc_for_tile[min] < 2)
		    || (  (float)contrib_counts[min]/num_proc_for_tile[min]
			> (float)contrib_counts[tile]/num_proc_for_tile[tile])))
	    {
		min = tile;
	    }
	}
	num_proc_for_tile[min]--;
	pcount--;
    }

  /* Clear out arrays. */
    memset(group_sizes, 0, num_tiles*sizeof(int));
    for (node = 0; node < num_processes; node++) {
	node_assignment[node] = -1;
    }

#define ASSIGN_NODE2TILE(node, tile)					\
node_assignment[(node)] = (tile);					\
tile_proc_groups[(tile)*num_processes + group_sizes[(tile)]] = (node);	\
group_sizes[(tile)]++;

  /* Assign each display node to the group processing that tile if there
     are any images in that tile. */
    for (tile = 0; tile < num_tiles; tile++) {
	if (contrib_counts[tile] > 0) {
	    ASSIGN_NODE2TILE(tile_display_nodes[tile], tile);
	}
    }

  /* Assign each node to a tile it is rendering, if possible. */
    for (node = 0; node < num_processes; node++) {
	if (node_assignment[node] < 0) {
	    GLboolean *tile_mask = all_contained_tiles_masks + node*num_tiles;
	    for (tile = 0; tile < num_tiles; tile++) {
		if (   (tile_mask[tile])
		    && (group_sizes[tile] < num_proc_for_tile[tile])) {
		    ASSIGN_NODE2TILE(node, tile);
		    break;
		}
	    }
	}
    }

  /* Assign rest of the nodes. */
    node = 0;
    for (tile = 0; tile < num_tiles; tile++) {
	while (group_sizes[tile] < num_proc_for_tile[tile]) {
	    while (node_assignment[node] >= 0) node++;
	    ASSIGN_NODE2TILE(node, tile);
	}
    }

    num_receiving = 0;

  /* Now figure out who I am sending to and how many I am receiving. */
    for (tile = 0; tile < num_tiles; tile++) {
	int *proc_group = tile_proc_groups + tile*num_processes;

	if (   (node_assignment[rank] != tile)
	    && !all_contained_tiles_masks[rank*num_tiles + tile]) {
	  /* Not involved with this tile.  Skip it. */
	    continue;
	}

	if (!icetIsEnabled(ICET_ORDERED_COMPOSITE)) {
	  /* If we are not doing an ordered composite, then we are free
	   * to assign processes to images in any way we please.  Here we
	   * will do everything we can to minimize communication. */

	  /* First, have processes send images to themselves when possible. */
	    if (   (node_assignment[rank] == tile)
		&& all_contained_tiles_masks[rank*num_tiles + tile]) {
		tile_image_dest[tile] = rank;
		num_receiving++;
	    }

	    snode = -1;
	    rnode = -1;
	    first_loop = 1;
	    while (1) {
	      /* Find next process that still needs a place to send images. */
		do {
		    snode++;
		} while (   (snode < num_processes)
			 && !all_contained_tiles_masks[snode*num_tiles + tile]);
		if (snode >= num_processes) {
		  /* We must be finished. */
		    break;
		}
		if (node_assignment[snode] == tile) {
		  /* This node keeps image in itself. */
		    continue;
		}

	      /* Find the next process that can accept the image. */
		do {
		    rnode++;
		    if (rnode >= group_sizes[tile]) {
			rnode = 0;
			first_loop = 0;
		    }
		    dest = proc_group[rnode];
		} while (   first_loop
			 && all_contained_tiles_masks[dest*num_tiles + tile]
			 && (node_assignment[dest] == tile));

	      /* Check to see if this node is sending the image data. */
		if (snode == rank) {
		    tile_image_dest[tile] = dest;
		}

	      /* Check to see if this node is receiving the image data. */
		if (dest == rank) {
		    num_receiving++;
		}
	    }
	} else {
	  /* We are doing an ordered composite.  It is vital that each process
	   * gets images that are consecutive in the ordering.  Communication
	   * costs come second. */

	    int num_contributors = 0;
	    int i;
	  /* First, we make a list of all processes contributing to this
	   * tile in the order in which the images need to be composed.
	   * We will then split this list into contiguous chunks and assign
	   * each chunk to a process. */
	    for (i = 0; i < num_processes; i++) {
		snode = composite_order[i];
		if (all_contained_tiles_masks[snode*num_tiles + tile]) {
		  /* The process snode contains an image for this tile.
		   * Add it to compositors. */
		    contributors[num_contributors] = snode;
		    num_contributors++;
		}
	    }
#ifdef DEBUG
	    if (num_contributors != contrib_counts[tile]) {
		icetRaiseError("Miscounted number of tile contributions",
			       ICET_SANITY_CHECK_FAIL);
	    }
#endif

	  /* contributors is split up as evenly as possible.  We will
	   * assign nodes in the order they appear in proc_group.  We now
	   * re-order proc_group to minimize the communications. */
	    for (i = 0; i < num_contributors; i++) {
		piece = i*group_sizes[tile]/num_contributors;
		snode = contributors[i];
		if (node_assignment[snode] == tile) {
		  /* snode is part of this group.  Assign it to piece it is
		   * part of. */
		    int j;
		    for (j = group_sizes[tile]-1; j >=0; j--) {
			if (proc_group[j] == snode) {
			  /* Found snode in proc_group.  Place it at
			   * "piece" in proc_group. */
			    proc_group[j] = proc_group[piece];
			    proc_group[piece] = snode;
			    break;
			}
		    }
#ifdef DEBUG
		    if (j < 0) {
			icetRaiseError("node_assignment/proc_group mismatch",
				       ICET_SANITY_CHECK_FAIL);
		    }
#endif
		}
	    }

	  /* We have just shuffled proc_group, so the tile display node is
	   * no longer necessarily at 0.  Find out where it should be. */
	    if (node_assignment[rank] == tile) {
		for (i = 0; i < group_sizes[tile]; i++) {
		    if (proc_group[i] == tile_display_nodes[tile]) {
			group_image_dest = i;
			break;
		    }
		}
#ifdef DEBUG
		if (i == group_sizes[tile]) {
		    icetRaiseError("Display process not participating in tile?",
				   ICET_SANITY_CHECK_FAIL);
		}
#endif
	    }

	  /* Assign nodes in the order they appear in proc_group. */
	    for (i = 0; i < num_contributors; i++) {
		piece = i*group_sizes[tile]/num_contributors;
		snode = contributors[i];
		rnode = proc_group[piece];
		
	      /* Check to see if this node is sending the image data. */
		if (snode == rank) {
		    tile_image_dest[tile] = rnode;
		}

	      /* Check to see if this node is receiving the image data. */
		if (rnode == rank) {
		    num_receiving++;
		}
	    }
	}
    }

    *tile_image_destp = tile_image_dest;
    if (node_assignment[rank] < 0) {
	*compose_groupp = NULL;
	*group_sizep = 0;
	*group_image_destp = 0;
	*num_receivingp = 0;
    } else {
	*compose_groupp = tile_proc_groups+node_assignment[rank]*num_processes;
	*group_sizep = group_sizes[node_assignment[rank]];
	*group_image_destp = group_image_dest;
	*num_receivingp = num_receiving;
    }
    return node_assignment[rank];
}
