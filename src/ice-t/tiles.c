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

#include <stdlib.h>

void icetResetTiles(void)
{
    GLint iarray[4];

    icetStateSetInteger(ICET_NUM_TILES, 0);
    icetStateSetIntegerv(ICET_TILE_VIEWPORTS, 0, NULL);
    icetStateSetInteger(ICET_TILE_DISPLAYED, -1);
    icetStateSetIntegerv(ICET_DISPLAY_NODES, 0, NULL);

    iarray[0] = 0;  iarray[1] = 0;  iarray[2] = 0;  iarray[3] = 0;
    icetStateSetIntegerv(ICET_GLOBAL_VIEWPORT, 4, iarray);

    icetStateSetInteger(ICET_TILE_MAX_WIDTH, 0);
    icetStateSetInteger(ICET_TILE_MAX_HEIGHT, 0);
    icetStateSetInteger(ICET_TILE_MAX_PIXELS, 0);
}

#ifndef MIN
#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#endif
int  icetAddTile(GLint x, GLint y, GLsizei width, GLsizei height,
		 int display_rank)
{
    GLint num_tiles;
    GLint *viewports;
    GLint gvp[4];
    GLint max_width, max_height;
    GLint *display_nodes;
    GLint rank;
    GLint num_processors;
    char msg[256];
    int i;

  /* Get current number of tiles and viewports. */
    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);
    viewports = malloc((num_tiles+1)*4*sizeof(GLint));
    icetGetIntegerv(ICET_TILE_VIEWPORTS, viewports);

  /* Get display node information. */
    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSES, &num_processors);
    display_nodes = malloc((num_tiles+1)*4*sizeof(GLint));
    icetGetIntegerv(ICET_DISPLAY_NODES, display_nodes);

  /* Check and update display ranks. */
    if (display_rank >= num_processors) {
	sprintf(msg, "icetDisplayNodes: Invalid rank for tile %d.",
		(int)num_tiles);
	icetRaiseError(msg, ICET_INVALID_VALUE);
	free(viewports);
	free(display_nodes);
	return -1;
    }
    for (i = 0; i < num_tiles; i++) {
	if (display_nodes[i] == display_rank) {
	    sprintf(msg, "icetDisplayNodes: Rank %d used for tiles %d and %d.",
		    display_rank, i, (int)num_tiles);
	    icetRaiseError(msg, ICET_INVALID_VALUE);
	    free(viewports);
	    free(display_nodes);
	    return -1;
	}
    }
    display_nodes[num_tiles] = display_rank;
    icetUnsafeStateSet(ICET_DISPLAY_NODES, num_tiles+1,
		       ICET_INT, display_nodes);
    if (display_rank == rank) {
	icetStateSetInteger(ICET_TILE_DISPLAYED, num_tiles);
    }

  /* Figure out current global viewport. */
    gvp[0] = x;  gvp[1] = y;  gvp[2] = x + width;  gvp[3] = y + height;
    for (i = 0; i < num_tiles; i++) {
	gvp[0] = MIN(gvp[0], viewports[i*4+0]);
	gvp[1] = MIN(gvp[1], viewports[i*4+1]);
	gvp[2] = MAX(gvp[2], viewports[i*4+0] + viewports[i*4+2]);
	gvp[3] = MAX(gvp[3], viewports[i*4+1] + viewports[i*4+3]);
    }
    gvp[2] -= gvp[0];
    gvp[3] -= gvp[1];

  /* Add new viewport to current viewports. */
    viewports[4*num_tiles+0] = x;
    viewports[4*num_tiles+1] = y;
    viewports[4*num_tiles+2] = width;
    viewports[4*num_tiles+3] = height;

  /* Set new state. */
    icetStateSetInteger(ICET_NUM_TILES, num_tiles+1);
    icetUnsafeStateSet(ICET_TILE_VIEWPORTS, (num_tiles+1)*4,
		       ICET_INT, viewports);
    icetStateSetIntegerv(ICET_GLOBAL_VIEWPORT, 4, gvp);

    icetGetIntegerv(ICET_TILE_MAX_WIDTH, &max_width);
    max_width = MAX(max_width, width);
    icetStateSetInteger(ICET_TILE_MAX_WIDTH, max_width);
    icetGetIntegerv(ICET_TILE_MAX_HEIGHT, &max_height);
    max_height = MAX(max_height, height);
    icetStateSetInteger(ICET_TILE_MAX_HEIGHT, max_height);
  /* When storing max pixels, leave some padding so that pixels may be
     dropped if the image needs to be divided amongst processors. */
    icetStateSetInteger(ICET_TILE_MAX_PIXELS,
			max_width*max_height + num_processors);

  /* Return index to tile. */
    return num_tiles;
}

void icetBoundingBoxd(GLdouble x_min, GLdouble x_max,
		      GLdouble y_min, GLdouble y_max,
		      GLdouble z_min, GLdouble z_max)
{
    GLdouble vertices[8*3];

    vertices[3*0+0] = x_min;  vertices[3*0+1] = y_min;  vertices[3*0+2] = z_min;
    vertices[3*1+0] = x_min;  vertices[3*1+1] = y_min;  vertices[3*1+2] = z_max;
    vertices[3*2+0] = x_min;  vertices[3*2+1] = y_max;  vertices[3*2+2] = z_min;
    vertices[3*3+0] = x_min;  vertices[3*3+1] = y_max;  vertices[3*3+2] = z_max;
    vertices[3*4+0] = x_max;  vertices[3*4+1] = y_min;  vertices[3*4+2] = z_min;
    vertices[3*5+0] = x_max;  vertices[3*5+1] = y_min;  vertices[3*5+2] = z_max;
    vertices[3*6+0] = x_max;  vertices[3*6+1] = y_max;  vertices[3*6+2] = z_min;
    vertices[3*7+0] = x_max;  vertices[3*7+1] = y_max;  vertices[3*7+2] = z_max;

    icetStateSetDoublev(ICET_GEOMETRY_BOUNDS, 8*3, vertices);
    icetStateSetInteger(ICET_NUM_BOUNDING_VERTS, 8);
}

void icetBoundingBoxf(GLfloat x_min, GLfloat x_max,
		      GLfloat y_min, GLfloat y_max,
		      GLfloat z_min, GLfloat z_max)
{
    icetBoundingBoxd(x_min, x_max, y_min, y_max, z_min, z_max);
}

void icetBoundingVertices(GLint size, GLenum type, GLsizei stride,
			  GLsizei count, const GLvoid *pointer)
{
    GLdouble *verts;
    int i, j;

    if (stride < 1) {
	switch (type) {
	  case ICET_SHORT:  stride = size*sizeof(GLshort);  break;
	  case ICET_INT:    stride = size*sizeof(GLint);    break;
	  case ICET_FLOAT:  stride = size*sizeof(GLfloat);  break;
	  case ICET_DOUBLE: stride = size*sizeof(GLdouble); break;
	  default:
	      icetRaiseError("Bad type to icetBoundingVertices.",
			     ICET_INVALID_VALUE);
	      return;
	}
    }

    verts = malloc(count*3*sizeof(GLdouble));
    for (i = 0; i < count; i++) {
	for (j = 0; j < 3; j++) {
	    switch (type) {
#define castcopy(ptype)							\
  if (j < size) {							\
      verts[i*3+j] = ((ptype *)pointer)[i*stride/sizeof(type)+j];	\
  } else {								\
      verts[i*3+j] = 0.0;						\
  }									\
  if (size >= 4) {							\
      verts[i*3+j] /= ((ptype *)pointer)[i*stride/sizeof(type)+4];	\
  }									\
  break;
	      case ICET_SHORT:
		  castcopy(GLshort);
	      case ICET_INT:
		  castcopy(GLint);
	      case ICET_FLOAT:
		  castcopy(GLfloat);
	      case ICET_DOUBLE:
		  castcopy(GLdouble);
	      default:
		  icetRaiseError("Bad type to icetBoundingVertices.",
				 ICET_INVALID_VALUE);
		  free(verts);
		  return;
	    }
	}
    }

    icetUnsafeStateSet(ICET_GEOMETRY_BOUNDS, count*3, ICET_DOUBLE, verts);
    icetStateSetInteger(ICET_NUM_BOUNDING_VERTS, count);
}
