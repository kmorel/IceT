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

void icetDrawFrame(void)
{
    int rank, processors;
    GLboolean isDrawing;
    GLint frame_count;
    GLdouble projection_matrix[16];
    GLint global_viewport[4];
    IceTStrategy strategy;
    GLvoid *value;
    int num_tiles;
    int num_bounding_verts;
    GLint *tile_viewports;
    GLint *contained_list;
    GLboolean *contained_mask;
    GLboolean *all_contained_masks;
    GLint *contrib_counts;
    GLint total_image_count;
    IceTImage image;
    GLint display_tile;
    GLint color_format;
    GLdouble render_time;
    GLdouble buf_read_time;
    GLdouble buf_write_time;
    GLdouble compose_time;
    GLdouble total_time;
    GLfloat background_color[4];
    GLuint background_color_word;
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

  /* Make sure background color is up to date. */
    glGetFloatv(GL_COLOR_CLEAR_VALUE, background_color);
    icetStateSetFloatv(ICET_BACKGROUND_COLOR, 4, background_color);
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
    icetStateSetInteger(ICET_BACKGROUND_COLOR_WORD, background_color_word);

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
    icetGetIntegerv(ICET_NUM_PROCESSES, &processors);
    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);
    icetResizeBuffer(  sizeof(GLint)*num_tiles
		     + sizeof(GLboolean)*num_tiles*(processors+1)
		     + sizeof(int)    /* So stuff can land on byte boundries.*/
		     + sizeof(GLint)*num_tiles*processors);
    contained_list = icetReserveBufferMem(sizeof(GLint) * num_tiles);
    contained_mask = icetReserveBufferMem(sizeof(GLboolean)*num_tiles);
    memset(contained_mask, 0, sizeof(GLboolean)*num_tiles);

    icetGetIntegerv(ICET_GLOBAL_VIEWPORT, global_viewport);
    tile_viewports = icetUnsafeStateGet(ICET_TILE_VIEWPORTS);

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
	icetStateSetIntegerv(ICET_CONTAINED_VIEWPORT, 4, global_viewport);
	icetStateSetDouble(ICET_NEAR_DEPTH, -1.0);
	icetStateSetDouble(ICET_FAR_DEPTH, 1.0);
	icetStateSetInteger(ICET_NUM_CONTAINED_TILES, num_tiles);
	icetStateSetIntegerv(ICET_CONTAINED_TILES_LIST, num_tiles,
			     contained_list);
	icetStateSetBooleanv(ICET_CONTAINED_TILES_MASK, num_tiles,
			     contained_mask);
    } else {
      /* Figure out which tiles the geometry may lie in. */
	GLdouble *bound_vert;
	GLdouble modelview_matrix[16];
	GLdouble viewport_matrix[16];
	GLdouble tmp_matrix[16];
	GLdouble total_transform[16];
	GLint left, right, bottom, top;
	GLdouble znear, zfar;
	GLint num_contained = 0;
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
	global_viewport[0] = left;
	global_viewport[1] = bottom;
	global_viewport[2] = right - left;
	global_viewport[3] = top - bottom;
	icetStateSetIntegerv(ICET_CONTAINED_VIEWPORT, 4, global_viewport);
	icetStateSetDouble(ICET_NEAR_DEPTH, znear);
	icetStateSetDouble(ICET_FAR_DEPTH, zfar);

      /* Now use this information to figure out which tiles need to be
	 drawn. */
	for (i = 0; i < num_tiles; i++) {
	    if (   (znear  <= 1.0)
		&& (zfar   >= -1.0)
		&& (left   <  tile_viewports[i*4+0] + tile_viewports[i*4+2])
		&& (right  >  tile_viewports[i*4+0])
		&& (bottom <  tile_viewports[i*4+1] + tile_viewports[i*4+3])
		&& (top    >  tile_viewports[i*4+1]) ) {
		contained_list[num_contained] = i;
		contained_mask[i] = 1;
		num_contained++;
	    }
	}
	icetStateSetInteger(ICET_NUM_CONTAINED_TILES, num_contained);
	icetStateSetIntegerv(ICET_CONTAINED_TILES_LIST, num_contained,
			     contained_list);
	icetStateSetBooleanv(ICET_CONTAINED_TILES_MASK, num_tiles,
			     contained_mask);
    }

  /* Get information on what tiles other processors are supposed to render. */
    icetRaiseDebug("Gathering rendering information.");
    all_contained_masks
	= icetReserveBufferMem(sizeof(GLint)*num_tiles*processors);
    contrib_counts = contained_list;

    ICET_COMM_ALLGATHER(contained_mask, num_tiles, ICET_BYTE,
			all_contained_masks);
    icetStateSetBooleanv(ICET_ALL_CONTAINED_TILES_MASKS,
			 num_tiles*processors, all_contained_masks);
    total_image_count = 0;
    for (i = 0; i < num_tiles; i++) {
	contrib_counts[i] = 0;
	for (j = 0; j < processors; j++) {
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

    icetGetIntegerv(ICET_TILE_DISPLAYED, &display_tile);
    buf_write_time = icetWallTime();
    if (display_tile >= 0) {
	GLubyte *colorBuffer;
	GLenum output_buffers;

	icetGetIntegerv(ICET_OUTPUT_BUFFERS, &output_buffers);
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
	    if (icetIsEnabled(ICET_DISPLAY_COLORED_BACKGROUND)) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
	    } else {
		glDisable(GL_BLEND);
	    }
	    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
