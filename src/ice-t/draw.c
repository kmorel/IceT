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
#include <state.h>
#include <context.h>
#include <diagnostics.h>
#include <image.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MI(r,c)	((c)*4+(r))

static void inflateBuffer(GLubyte *buffer, GLsizei width, GLsizei height);

static void multMatrix(GLdouble *C, const GLdouble *A, const GLdouble *B);

static GLubyte *display_buffer = NULL;
static GLsizei display_buffer_size = 0;

void icetDrawFunc(IceTCallback func)
{
    icetStateSetPointer(ICET_DRAW_FUNCTION, (GLvoid *)func);
}

void icetStrategy(IceTStrategy strategy)
{
    icetStateSetPointer(ICET_STRATEGY_NAME, strategy.name);
    icetStateSetBoolean(ICET_STRATEGY_SUPPORTS_ORDERING,
			strategy.supports_ordering);
    icetStateSetPointer(ICET_STRATEGY_COMPOSE, (GLvoid *)strategy.compose);
}

const GLubyte *icetGetStrategyName(void)
{
    GLvoid *name;
    if (icetStateType(ICET_STRATEGY_NAME) == ICET_NULL) {
	return NULL;
    } else {
	icetGetPointerv(ICET_STRATEGY_NAME, &name);
    }
    return name;
}

void icetCompositeOrder(const GLint *process_ranks)
{
    GLint num_proc;
    GLint i;
    GLint *process_orders;
    GLboolean new_process_orders;

    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);
    if (   (icetStateGetType(ICET_PROCESS_ORDERS) == ICET_INT)
	&& (icetStateGetSize(ICET_PROCESS_ORDERS) >= num_proc) ) {
	process_orders = icetUnsafeStateGet(ICET_PROCESS_ORDERS);
	new_process_orders = 0;
    } else {
	process_orders = malloc(ICET_PROCESS_ORDERS * sizeof(GLint));
	new_process_orders = 1;
    }
    for (i = 0; i < num_proc; i++) {
	process_orders[i] = -1;
    }
    for (i = 0; i < num_proc; i++) {
	process_orders[process_ranks[i]] = i;
    }
    for (i = 0; i < num_proc; i++) {
	if (process_orders[i] == -1) {
	    icetRaiseError("Invalid composit order.", ICET_INVALID_VALUE);
	    return;
	}
    }
    icetStateSetIntegerv(ICET_COMPOSITE_ORDER, num_proc, process_ranks);
    if (new_process_orders) {
	icetUnsafeStateSet(ICET_PROCESS_ORDERS, num_proc,
			   GL_INT, process_orders);
    }
}

void icetDataReplicationGroup(GLint size, const GLint *processes)
{
    GLint rank;
    GLboolean found_myself = ICET_FALSE;
    GLint i;

    icetGetIntegerv(ICET_RANK, &rank);
    for (i = 0; i < size; i++) {
	if (processes[i] == rank) {
	    found_myself = ICET_TRUE;
	    break;
	}
    }

    if (!found_myself) {
	icetRaiseError("Local process not part of data replication group.",
		       ICET_INVALID_VALUE);
	return;
    }

    icetStateSetIntegerv(ICET_DATA_REPLICATION_GROUP_SIZE, 1, &size);
    icetStateSetIntegerv(ICET_DATA_REPLICATION_GROUP, size, processes);
}

void icetDataReplicationGroupColor(GLint color)
{
    GLint *allcolors;
    GLint *mygroup;
    GLint num_proc;
    GLint i;
    GLint size;

    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);
    icetResizeBuffer(2*sizeof(GLint)*num_proc);
    allcolors = icetReserveBufferMem(sizeof(GLint)*num_proc);
    mygroup = icetReserveBufferMem(sizeof(GLint)*num_proc);

    ICET_COMM_ALLGATHER(&color, 1, ICET_INT, allcolors);

    size = 0;
    for (i = 0; i < num_proc; i++) {
	if (allcolors[i] == color) {
	    mygroup[size] = i;
	    size++;
	}
    }

    icetDataReplicationGroup(size, mygroup);
}

GLubyte *icetGetColorBuffer(void)
{
    GLint color_buffer_valid;

    icetGetIntegerv(ICET_COLOR_BUFFER_VALID, &color_buffer_valid);
    if (color_buffer_valid) {
	GLubyte *color_buffer;
	icetGetPointerv(ICET_COLOR_BUFFER, (GLvoid **)&color_buffer);
	return color_buffer;
    } else {
	icetRaiseError("Color buffer not available.", ICET_INVALID_OPERATION);
	return NULL;
    }
}
GLuint *icetGetDepthBuffer(void)
{
    GLint depth_buffer_valid;

    icetGetIntegerv(ICET_DEPTH_BUFFER_VALID, &depth_buffer_valid);
    if (depth_buffer_valid) {
	GLuint *depth_buffer;
	icetGetPointerv(ICET_DEPTH_BUFFER, (GLvoid **)&depth_buffer);
	return depth_buffer;
    } else {
	icetRaiseError("Depth buffer not available.", ICET_INVALID_OPERATION);
	return NULL;
    }
}

static void determine_contained_tiles(const GLint contained_viewport[4],
				      GLdouble znear, GLdouble zfar,
				      const GLint *tile_viewports,
				      GLint num_tiles,
				      GLint *contained_list,
				      GLboolean *contained_mask,
				      GLint *num_contained)
{
    int i;
    *num_contained = 0;
    memset(contained_mask, 0, sizeof(GLboolean)*num_tiles);
    for (i = 0; i < num_tiles; i++) {
	if (   (znear  <= 1.0)
	    && (zfar   >= -1.0)
	    && (  contained_viewport[0]
		< tile_viewports[i*4+0] + tile_viewports[i*4+2])
	    && (  contained_viewport[0] + contained_viewport[2]
		> tile_viewports[i*4+0])
	    && (  contained_viewport[1]
		< tile_viewports[i*4+1] + tile_viewports[i*4+3])
	    && (  contained_viewport[1] + contained_viewport[3]
		> tile_viewports[i*4+1]) ) {
	    contained_list[*num_contained] = i;
	    contained_mask[i] = 1;
	    (*num_contained)++;
	}
    }
}

static GLfloat black[] = {0.0, 0.0, 0.0, 0.0};

void icetDrawFrame(void)
{
    GLint rank, num_proc;
    GLboolean isDrawing;
    GLint frame_count;
    GLdouble projection_matrix[16];
    GLint global_viewport[4];
    GLint contained_viewport[4];
    GLdouble znear, zfar;
    IceTStrategy strategy;
    GLvoid *value;
    GLint num_tiles;
    GLint num_bounding_verts;
    GLint *tile_viewports;
    GLint *contained_list;
    GLint num_contained;
    GLboolean *contained_mask;
    GLboolean *all_contained_masks;
    GLint *data_replication_group;
    GLint data_replication_group_size;
    GLint *contrib_counts;
    GLint total_image_count;
    IceTImage image;
    GLint display_tile;
    GLint *display_nodes;
    GLint color_format;
    GLdouble render_time;
    GLdouble buf_read_time;
    GLdouble buf_write_time;
    GLdouble compose_time;
    GLdouble total_time;
    GLfloat background_color[4];
    GLuint background_color_word;
    GLboolean color_blending;
    int i, j;

    icetRaiseDebug("In icetDrawFrame");

    icetGetBooleanv(ICET_IS_DRAWING_FRAME, &isDrawing);
    if (isDrawing) {
	icetRaiseError("Recursive frame draw detected.",ICET_INVALID_OPERATION);
	return;
    }

    icetGetIntegerv(ICET_COLOR_FORMAT, &color_format);

    icetStateResetTiming();
    total_time = icetWallTime();

    color_blending = (   *((GLuint *)icetUnsafeStateGet(ICET_INPUT_BUFFERS))
		      == ICET_COLOR_BUFFER_BIT);

  /* Make sure background color is up to date. */
    glGetFloatv(GL_COLOR_CLEAR_VALUE, background_color);
    switch (color_format) {
      case GL_RGBA:
	  ((GLubyte *)&background_color_word)[0]
	      = (GLubyte)(255*background_color[0]);
	  ((GLubyte *)&background_color_word)[1]
	      = (GLubyte)(255*background_color[1]);
	  ((GLubyte *)&background_color_word)[2]
	      = (GLubyte)(255*background_color[2]);
	  ((GLubyte *)&background_color_word)[3]
	      = (GLubyte)(255*background_color[3]);
	  break;
#if defined(GL_BGRA)
      case GL_BGRA:
#elif defined(GL_BGRA_EXT)
      case GL_BGRA_EXT:
#endif
	  ((GLubyte *)&background_color_word)[0]
	      = (GLubyte)(255*background_color[2]);
	  ((GLubyte *)&background_color_word)[1]
	      = (GLubyte)(255*background_color[1]);
	  ((GLubyte *)&background_color_word)[2]
	      = (GLubyte)(255*background_color[0]);
	  ((GLubyte *)&background_color_word)[3]
	      = (GLubyte)(255*background_color[3]);
	  break;
    }
    if (color_blending && (   icetIsEnabled(ICET_CORRECT_COLORED_BACKGROUND)
			   || icetIsEnabled(ICET_DISPLAY_COLORED_BACKGROUND))) {
      /* We need to correct the background color by zeroing it out at
       * blending it back at the end. */
	glClearColor(0.0, 0.0, 0.0, 0.0);
	icetStateSetFloatv(ICET_BACKGROUND_COLOR, 4, black);
	icetStateSetInteger(ICET_BACKGROUND_COLOR_WORD, 0);
    } else {
	icetStateSetFloatv(ICET_BACKGROUND_COLOR, 4, background_color);
	icetStateSetInteger(ICET_BACKGROUND_COLOR_WORD, background_color_word);
    }

    icetGetIntegerv(ICET_FRAME_COUNT, &frame_count);
    frame_count++;
    icetStateSetIntegerv(ICET_FRAME_COUNT, 1, &frame_count);
    if (frame_count == 1) {
      /* On the first frame, try to fix the far depth. */
	GLint readBuffer;
	GLuint depth;
	GLuint far_depth;
	icetGetIntegerv(ICET_READ_BUFFER, &readBuffer);

	icetRaiseDebug("Trying to get far depth.");

	glClear(GL_DEPTH_BUFFER_BIT);
	glReadBuffer(readBuffer);
	glFlush();
	glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, &depth);

	icetGetIntegerv(ICET_ABSOLUTE_FAR_DEPTH, (GLint *)&far_depth);
	if (depth > far_depth) {
	    icetStateSetIntegerv(ICET_ABSOLUTE_FAR_DEPTH, 1, (GLint *)&depth);
	}
    }

    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);
    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);
    icetResizeBuffer(  sizeof(GLint)*num_tiles
		     + sizeof(GLboolean)*num_tiles*(num_proc+1)
		     + sizeof(int)    /* So stuff can land on byte boundries.*/
		     + sizeof(GLint)*num_tiles*num_proc
		     + sizeof(GLint)*num_proc);
    contained_list = icetReserveBufferMem(sizeof(GLint) * num_tiles);
    contained_mask = icetReserveBufferMem(sizeof(GLboolean)*num_tiles);

    icetGetIntegerv(ICET_GLOBAL_VIEWPORT, global_viewport);
    tile_viewports = icetUnsafeStateGet(ICET_TILE_VIEWPORTS);

    icetGetIntegerv(ICET_TILE_DISPLAYED, &display_tile);
    display_nodes = icetUnsafeStateGet(ICET_DISPLAY_NODES);

  /* Get the current projection matrix. */
    icetRaiseDebug("Getting projection matrix.");
    glGetDoublev(GL_PROJECTION_MATRIX, projection_matrix);
    icetStateSetDoublev(ICET_PROJECTION_MATRIX, 16, projection_matrix);

    icetGetIntegerv(ICET_NUM_BOUNDING_VERTS, &num_bounding_verts);
    if (num_bounding_verts < 1) {
      /* User never set bounding vertices.  Assume image covers all
         tiles. */
	for (i = 0; i < num_tiles; i++) {
	    contained_list[i] = i;
	    contained_mask[i] = 1;
	}
	contained_viewport[0] = global_viewport[0];
	contained_viewport[1] = global_viewport[1];
	contained_viewport[2] = global_viewport[2];
	contained_viewport[3] = global_viewport[3];
	znear = -1.0;
	zfar = 1.0;
	num_contained = num_tiles;
    } else {
      /* Figure out which tiles the geometry may lie in. */
	GLdouble *bound_vert;
	GLdouble modelview_matrix[16];
	GLdouble viewport_matrix[16];
	GLdouble tmp_matrix[16];
	GLdouble total_transform[16];
	GLint left, right, bottom, top;
	GLdouble x, y;
	GLdouble z, invw;

      /* Strange projection matrix that transforms the x and y of normalized
	 screen coordinates into viewport coordinates that may be cast to
	 integers. */
	viewport_matrix[ 0] = global_viewport[2];
	viewport_matrix[ 1] = 0.0;
	viewport_matrix[ 2] = 0.0;
	viewport_matrix[ 3] = 0.0;

	viewport_matrix[ 4] = 0.0;
	viewport_matrix[ 5] = global_viewport[3];
	viewport_matrix[ 6] = 0.0;
	viewport_matrix[ 7] = 0.0;

	viewport_matrix[ 8] = 0.0;
	viewport_matrix[ 9] = 0.0;
	viewport_matrix[10] = 2.0;
	viewport_matrix[11] = 0.0;

	viewport_matrix[12] = global_viewport[2] + global_viewport[0]*2.0;
	viewport_matrix[13] = global_viewport[3] + global_viewport[1]*2.0;
	viewport_matrix[14] = 0.0;
	viewport_matrix[15] = 2.0;

	glGetDoublev(GL_MODELVIEW_MATRIX, modelview_matrix);

	multMatrix(tmp_matrix, projection_matrix, modelview_matrix);
	multMatrix(total_transform, viewport_matrix, tmp_matrix);

      /* Set absolute mins and maxes. */
	left   = global_viewport[0] + global_viewport[2];
	right  = global_viewport[0];
	bottom = global_viewport[1] + global_viewport[3];
	top    = global_viewport[1];
	znear  = 1.0;
	zfar   = -1.0;

      /* Transform each vertex to find where it lies in the global
         viewport and normalized z. */
	bound_vert = icetUnsafeStateGet(ICET_GEOMETRY_BOUNDS);
	for (i = 0; i < num_bounding_verts; i++) {
	    invw = 1.0/(  total_transform[MI(3,0)]*bound_vert[0]
			+ total_transform[MI(3,1)]*bound_vert[1]
			+ total_transform[MI(3,2)]*bound_vert[2]
			+ total_transform[MI(3,3)]);
	    x = (  (  total_transform[MI(0,0)]*bound_vert[0]
		    + total_transform[MI(0,1)]*bound_vert[1]
		    + total_transform[MI(0,2)]*bound_vert[2]
		    + total_transform[MI(0,3)])
		 * invw);
	    y = (  (  total_transform[MI(1,0)]*bound_vert[0]
		    + total_transform[MI(1,1)]*bound_vert[1]
		    + total_transform[MI(1,2)]*bound_vert[2]
		    + total_transform[MI(1,3)])
		 * invw);
	    z = (  (  total_transform[MI(2,0)]*bound_vert[0]
		    + total_transform[MI(2,1)]*bound_vert[1]
		    + total_transform[MI(2,2)]*bound_vert[2]
		    + total_transform[MI(2,3)])
		 * invw);

	    if (left   > floor(x)) left   = (GLint)floor(x);
	    if (right  < ceil(x) ) right  = (GLint)ceil(x);
	    if (bottom > floor(y)) bottom = (GLint)floor(y);
	    if (top    < ceil(y) ) top    = (GLint)ceil(y);
	    if (znear  > z) znear  = z;
	    if (zfar   < z) zfar   = z;

	    bound_vert += 3;
	}

      /* Clip bounds to global viewport. */
	if (left   < global_viewport[0]) left = global_viewport[0];
	if (right  > global_viewport[0] + global_viewport[2])
	    right  = global_viewport[0] + global_viewport[2];
	if (bottom < global_viewport[1]) bottom = global_viewport[1];
	if (top    > global_viewport[1] + global_viewport[3])
	    top    = global_viewport[1] + global_viewport[3];
	if (znear  < -1.0) znear = -1.0;
	if (zfar   >  1.0) zfar = 1.0;

      /* Use this information to build a containing viewport. */
	contained_viewport[0] = left;
	contained_viewport[1] = bottom;
	contained_viewport[2] = right - left;
	contained_viewport[3] = top - bottom;

      /* Now use this information to figure out which tiles need to be
	 drawn. */
	determine_contained_tiles(contained_viewport, znear, zfar,
				  tile_viewports, num_tiles,
				  contained_list, contained_mask,
				  &num_contained);
    }

    icetRaiseDebug4("contained_viewport = %d %d %d %d",
		    contained_viewport[0], contained_viewport[1],
		    contained_viewport[2], contained_viewport[3]);

  /* If we are doing data replication, reduced the amount of screen space
     we are responsible for. */
    icetGetIntegerv(ICET_DATA_REPLICATION_GROUP_SIZE,
		    &data_replication_group_size);
    if (data_replication_group_size > 1) {
	data_replication_group = icetReserveBufferMem(sizeof(GLint)*num_proc);
	icetGetIntegerv(ICET_DATA_REPLICATION_GROUP, data_replication_group);
	if (data_replication_group_size >= num_contained) {
	  /* We have at least as many processes in the group as tiles we
	     are projecting to.  First check to see if anybody in the group
	     is displaying one of the tiles. */
	    int tile_rendering = -1;
	    int num_rendering_tile;
	    int tile_allocation_num;
	    int tile_id;
	    for (tile_id = 0; tile_id < num_contained; tile_id++) {
		int tile = contained_list[tile_id];
		int group_id;
		for (group_id = 0; group_id < data_replication_group_size;
		     group_id++) {
		    if (display_nodes[tile]==data_replication_group[group_id]) {
			if (data_replication_group[group_id] == rank) {
			  /* I'm displaying this tile, let's render it. */
			    tile_rendering = tile;
			    num_rendering_tile = 1;
			    tile_allocation_num = 0;
			}
		      /* Remove both the tile and display node to prevent
			 pairing either with something else. */
			num_contained--;
			contained_list[tile_id] = contained_list[num_contained];
			data_replication_group_size--;
			data_replication_group[group_id] =
			    data_replication_group[data_replication_group_size];
		      /* And decrement the tile counter so that we actually
			 check the tile we just moved. */
			tile_id--;
			break;
		    }
		}
	    }

	  /* Assign the rest of the processes to tiles. */
	    if (num_contained > 0) {
		int proc_to_tiles = 0;
		int group_id;
		for (tile_id = 0, group_id = 0;
		     group_id < data_replication_group_size;
		     group_id++, tile_id++) {
		    if (tile_id >= num_contained) {
			tile_id = 0;
			proc_to_tiles++;
		    }
		    if (data_replication_group[group_id] == rank) {
		      /* Assign this process to the tile. */
			tile_rendering = contained_list[tile_id];
			tile_allocation_num = proc_to_tiles;
			num_rendering_tile = proc_to_tiles+1;
		    } else if (tile_rendering == contained_list[tile_id]) {
		      /* This process already assigned to tile.  Mark that
			 another process is also assigned to it. */
			num_rendering_tile++;
		    }
		}
	    }

	  /* Record a new viewport covering only my portion of the tile. */
	    if (tile_rendering >= 0) {
		GLint *tv = tile_viewports + 4*tile_rendering;
		int new_length = tv[2]/num_rendering_tile;
		num_contained = 1;
		contained_list[0] = tile_rendering;
		contained_viewport[1] = tv[1];
		contained_viewport[3] = tv[3];
		contained_viewport[0] = tv[0] + tile_allocation_num*new_length;
		if (tile_allocation_num == num_rendering_tile-1) {
		  /* Make sure last piece does not drop pixels due to rounding
		     errors. */
		    contained_viewport[2]
			= tv[2] - tile_allocation_num*new_length;
		} else {
		    contained_viewport[2] = new_length;
		}
	    } else {
		num_contained = 0;
		contained_viewport[0] = global_viewport[0]-1;
		contained_viewport[1] = global_viewport[1]-1;
		contained_viewport[2] = 0;
		contained_viewport[3] = 0;
	    }

	  /* Fix contained_mask. */
	    for (i = 0; i < num_tiles; i++) {
		contained_mask[i] = (i == tile_rendering);
	    }
	} else {
	  /* More tiles than processes.  Split up the contained_viewport as
	     best as possible. */
	    int factor = 2;
	    while (factor <= data_replication_group_size) {
		int split_axis = contained_viewport[2] < contained_viewport[3];
		int new_length;
		while (data_replication_group_size%factor != 0) factor++;
	      /* Split the viewport along the axis factor times.  Also
		 split the group into factor pieces. */
		new_length = contained_viewport[2+split_axis]/factor;
		for (i = 0; data_replication_group[i] != rank; i++);
		data_replication_group_size /= factor;	/* New subgroup. */
		i /= data_replication_group_size;	/* i = piece I'm in. */
		data_replication_group += i*data_replication_group_size;
		if (i == factor-1) {
		  /* Make sure last piece does not drop pixels due to
		     rounding errors. */
		    contained_viewport[2+split_axis] -= i*new_length;
		} else {
		    contained_viewport[2+split_axis] = new_length;
		}
		contained_viewport[split_axis] += i*new_length;
	    }
	    determine_contained_tiles(contained_viewport, znear, zfar,
				      tile_viewports, num_tiles,
				      contained_list, contained_mask,
				      &num_contained);
	}
    }

    icetRaiseDebug4("new contained_viewport = %d %d %d %d",
		    contained_viewport[0], contained_viewport[1],
		    contained_viewport[2], contained_viewport[3]);
    icetStateSetIntegerv(ICET_CONTAINED_VIEWPORT, 4, contained_viewport);
    icetStateSetDoublev(ICET_NEAR_DEPTH, 1, &znear);
    icetStateSetDoublev(ICET_FAR_DEPTH, 1, &zfar);
    icetStateSetInteger(ICET_NUM_CONTAINED_TILES, num_contained);
    icetStateSetIntegerv(ICET_CONTAINED_TILES_LIST, num_contained,
			 contained_list);
    icetStateSetBooleanv(ICET_CONTAINED_TILES_MASK, num_tiles, contained_mask);

  /* Get information on what tiles other processes are supposed to render. */
    icetRaiseDebug("Gathering rendering information.");
    all_contained_masks
	= icetReserveBufferMem(sizeof(GLint)*num_tiles*num_proc);
    contrib_counts = contained_list;

    ICET_COMM_ALLGATHER(contained_mask, num_tiles, ICET_BYTE,
			all_contained_masks);
    icetStateSetBooleanv(ICET_ALL_CONTAINED_TILES_MASKS,
			 num_tiles*num_proc, all_contained_masks);
    total_image_count = 0;
    for (i = 0; i < num_tiles; i++) {
	contrib_counts[i] = 0;
	for (j = 0; j < num_proc; j++) {
	    if (all_contained_masks[j*num_tiles + i]) {
		contrib_counts[i]++;
	    }
	}
	total_image_count += contrib_counts[i];
    }
    icetStateSetIntegerv(ICET_TILE_CONTRIB_COUNTS, num_tiles, contrib_counts);
    icetStateSetIntegerv(ICET_TOTAL_IMAGE_COUNT, 1, &total_image_count);

    icetGetPointerv(ICET_DRAW_FUNCTION, &value);
    if (value == NULL) {
	icetRaiseError("Drawing function not set.", ICET_INVALID_OPERATION);
	return;
    }
    icetRaiseDebug("Calling strategy.compose");
    icetGetPointerv(ICET_STRATEGY_COMPOSE, &value);
    if (value == NULL) {
	icetRaiseError("Strategy not set.", ICET_INVALID_OPERATION);
	return;
    }
    strategy.compose = (IceTImage (*)(void))value;
    icetStateSetBoolean(ICET_IS_DRAWING_FRAME, 1);
    image = (*strategy.compose)();

  /* Restore projection matrix. */
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(projection_matrix);
    glMatrixMode(GL_MODELVIEW);
    icetStateSetBoolean(ICET_IS_DRAWING_FRAME, 0);

  /* Correct background color where applicable. */
    glClearColor(background_color[0], background_color[1],
		 background_color[2], background_color[3]);
    if (   color_blending && (display_tile >= 0) && (background_color_word != 0)
	&& icetIsEnabled(ICET_CORRECT_COLORED_BACKGROUND) ) {
	GLubyte *color = icetGetImageColorBuffer(image);
	GLubyte *bc = (GLubyte *)(&background_color_word);
	GLuint pixels = icetGetImagePixelCount(image);
	GLuint i;
	GLdouble blend_time;
	icetGetDoublev(ICET_BLEND_TIME, &blend_time);
	blend_time = icetWallTime() - blend_time;
	for (i = 0; i < pixels; i++, color += 4) {
	    ICET_UNDER(bc, color);
	}
	blend_time = icetWallTime() - blend_time;
	icetStateSetDouble(ICET_BLEND_TIME, blend_time);
    }

    buf_write_time = icetWallTime();
    if (display_tile >= 0) {
	GLubyte *colorBuffer;
	GLenum output_buffers;

	icetGetIntegerv(ICET_OUTPUT_BUFFERS, (GLint *)&output_buffers);
	if ((output_buffers & ICET_COLOR_BUFFER_BIT) != 0) {
	    icetStateSetBoolean(ICET_COLOR_BUFFER_VALID, 1);
	    colorBuffer = icetGetImageColorBuffer(image);
	    icetStateSetPointer(ICET_COLOR_BUFFER, colorBuffer);
				
	}
	if ((output_buffers & ICET_DEPTH_BUFFER_BIT) != 0) {
	    icetStateSetBoolean(ICET_DEPTH_BUFFER_VALID, 1);
	    icetStateSetPointer(ICET_DEPTH_BUFFER,
				icetGetImageDepthBuffer(image));
	}    

	if (   ((output_buffers & ICET_COLOR_BUFFER_BIT) != 0)
	    && icetIsEnabled(ICET_DISPLAY) ) {
	    icetRaiseDebug("Displaying image.");

	    colorBuffer = icetGetImageColorBuffer(image);

	    glPushAttrib(GL_TEXTURE_BIT | GL_COLOR_BUFFER_BIT);
	    glDisable(GL_TEXTURE_1D);
	    glDisable(GL_TEXTURE_2D);
#ifdef GL_TEXTURE_3D
	    glDisable(GL_TEXTURE_3D);
#endif
	    if (   color_blending
		&& icetIsEnabled(ICET_DISPLAY_COLORED_BACKGROUND)
		&& !icetIsEnabled(ICET_CORRECT_COLORED_BACKGROUND) ) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glClear(GL_COLOR_BUFFER_BIT);
	    } else {
		glDisable(GL_BLEND);
	    }
	    glClear(GL_DEPTH_BUFFER_BIT);
	    if (icetIsEnabled(ICET_DISPLAY_INFLATE)) {
		inflateBuffer(colorBuffer,
			      tile_viewports[display_tile*4+2],
			      tile_viewports[display_tile*4+3]);
	    } else {
		glDrawPixels(tile_viewports[display_tile*4+2],
			     tile_viewports[display_tile*4+3],
			     color_format, GL_UNSIGNED_BYTE, colorBuffer);
	    }
	    glPopAttrib();
	}
    }

    icetRaiseDebug("Calculating times.");
    buf_write_time = icetWallTime() - buf_write_time;
    icetStateSetDouble(ICET_BUFFER_WRITE_TIME, buf_write_time);

    icetGetDoublev(ICET_RENDER_TIME, &render_time);
    icetGetDoublev(ICET_BUFFER_READ_TIME, &buf_read_time);

    total_time = icetWallTime() - total_time;
    icetStateSetDouble(ICET_TOTAL_DRAW_TIME, total_time);

    compose_time = total_time - render_time - buf_read_time - buf_write_time;
    icetStateSetDouble(ICET_COMPOSITE_TIME, compose_time);
}

static void inflateBuffer(GLubyte *buffer, GLsizei width, GLsizei height)
{
    GLint physical_viewport[4];
    GLint display_width, display_height;
    GLint color_format;

    glGetIntegerv(GL_VIEWPORT, physical_viewport);
    display_width = physical_viewport[2];
    display_height = physical_viewport[3];

    icetGetIntegerv(ICET_COLOR_FORMAT, &color_format);

    if ((display_width > width) || (display_height > height)) {
	GLsizei x, y;
	GLsizei x_div, y_div;
	GLubyte *last_scanline;

      /* Make sure buffer is big enough. */
	if (display_buffer_size < display_width*display_height) {
	    free(display_buffer);
	    display_buffer_size = display_width*display_height;
	    display_buffer = malloc(4*sizeof(GLubyte)*display_buffer_size);
	}

      /* This is how we scale the image with integer arithmetic.
       * If a/b = r = a div b + (a mod b)/b then:
       * 	c/r = c/(a div b + (a mod b)/b) = c*b/(b*(a div b) + a mod b)
       * In our case a/b is display_width/width and display_height/height.
       * x_div and y_div are the denominators in the equation above.
       */
	x_div = width*(display_width/width) + display_width%width;
	y_div = height*(display_height/height) + display_height%height;
	last_scanline = NULL;
	for (y = 0; y < display_height; y++) {
	    GLubyte *src_scanline;
	    GLubyte *dest_scanline;

	    src_scanline = buffer + 4*width*((y*height)/y_div);
	    dest_scanline = display_buffer + 4*display_width*y;

	    if (src_scanline == last_scanline) {
	      /* Repeating last scanline.  Just copy memory. */
		memcpy(dest_scanline,
		       (const GLubyte *)(dest_scanline - 4*display_width),
		       4*display_width);
		continue;
	    }

	    for (x = 0; x < display_width; x++) {
		((GLuint *)dest_scanline)[x] =
		    ((GLuint *)src_scanline)[(x*width)/x_div];
	    }

	    last_scanline = src_scanline;
	}
	glDrawPixels(display_width, display_height, color_format,
		     GL_UNSIGNED_BYTE, display_buffer);
    } else {
      /* No need to inflate image. */
	glDrawPixels(width, height, color_format, GL_UNSIGNED_BYTE, buffer);
    }
}

static void multMatrix(GLdouble *C, const GLdouble *A, const GLdouble *B)
{
    int i, j, k;

    for (i = 0; i < 4; i++) {
	for (j = 0; j < 4; j++) {
	    C[MI(i,j)] = 0.0;
	    for (k = 0; k < 4; k++) {
		C[MI(i,j)] += A[MI(i,k)] * B[MI(k,j)];
	    }
	}
    }
}
