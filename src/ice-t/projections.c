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

#include <projections.h>
#include <GL/ice-t.h>

#include <state.h>
#include <diagnostics.h>

#include <stdlib.h>

static GLint num_tiles = 0;
static GLdouble *tile_projections = NULL;
static unsigned long viewport_time = (unsigned long)-1;

static GLdouble global_projection[16];
static unsigned long projection_time = (unsigned long)-1;

static void update_tile_projections(void);

/* static void multMatrix(GLdouble *C, const GLdouble *A, const GLdouble *B); */

void icetProjectTile(GLint tile)
{
    GLint *viewports;
    GLint physical_viewport[4];
    GLint tile_width, tile_height;
    GLint renderable_width, renderable_height;

  /* Update tile projections. */
    if (viewport_time != icetGetTime(ICET_TILE_VIEWPORTS)) {
	update_tile_projections();
	viewport_time = icetGetTime(ICET_TILE_VIEWPORTS);
    }

    if ((tile < 0) || (tile >= num_tiles)) {
	icetRaiseError("Bad tile passed to icetProjectTile.",
		       ICET_INVALID_VALUE);
	return;
    }

    viewports = icetUnsafeStateGet(ICET_TILE_VIEWPORTS);
    tile_width = viewports[tile*4+2];
    tile_height = viewports[tile*4+3];
    glGetIntegerv(GL_VIEWPORT, physical_viewport);
    renderable_width = physical_viewport[2];
    renderable_height = physical_viewport[3];

    if ((renderable_width != tile_width) || (renderable_height != tile_height)){
      /* Compensate for fact that tile is smaller than actual window. */
	glOrtho(-1.0, 2.0*renderable_width/tile_width - 1.0,
		-1.0, 2.0*renderable_height/tile_height - 1.0,
		1.0, -1.0);
    }

    glMultMatrixd(tile_projections + 16*tile);

    if (projection_time != icetGetTime(ICET_PROJECTION_MATRIX)) {
	icetGetDoublev(ICET_PROJECTION_MATRIX, global_projection);
	projection_time = icetGetTime(ICET_PROJECTION_MATRIX);
    }

    glMultMatrixd(global_projection);
}

void icetGetViewportProject(GLint x, GLint y, GLsizei width, GLsizei height,
			    GLdouble *mat_out)
{
    GLint global_viewport[4];
/*     GLdouble viewport_transform[16]; */
/*     GLdouble tile_transform[16]; */

    icetGetIntegerv(ICET_GLOBAL_VIEWPORT, global_viewport);

/*     viewport_transform[ 0] = 0.5*global_viewport[2]; */
/*     viewport_transform[ 1] = 0.0; */
/*     viewport_transform[ 2] = 0.0; */
/*     viewport_transform[ 3] = 0.0; */

/*     viewport_transform[ 4] = 0.0; */
/*     viewport_transform[ 5] = 0.5*global_viewport[3]; */
/*     viewport_transform[ 6] = 0.0; */
/*     viewport_transform[ 7] = 0.0; */

/*     viewport_transform[ 8] = 0.0; */
/*     viewport_transform[ 9] = 0.0; */
/*     viewport_transform[10] = 1.0; */
/*     viewport_transform[11] = 0.0; */

/*     viewport_transform[12] = 0.5*global_viewport[2] + global_viewport[0]*; */
/*     viewport_transform[13] = 0.5*global_viewport[3] + global_viewport[1]*; */
/*     viewport_transform[14] = 0.0; */
/*     viewport_transform[15] = 1.0; */

/*     tile_transform[ 0] = 2.0/width; */
/*     tile_transform[ 1] = 0.0; */
/*     tile_transform[ 2] = 0.0; */
/*     tile_transform[ 3] = 0.0; */

/*     tile_transform[ 4] = 0.0; */
/*     tile_transform[ 5] = 2.0/height; */
/*     tile_transform[ 6] = 0.0; */
/*     tile_transform[ 7] = 0.0; */

/*     tile_transform[ 8] = 0.0; */
/*     tile_transform[ 9] = 0.0; */
/*     tile_transform[10] = 1.0; */
/*     tile_transform[11] = 0.0; */

/*     tile_transform[12] = -(2.0*x)/width - 1.0; */
/*     tile_transform[13] = -(2.0*y)/height - 1.0; */
/*     tile_transform[14] = 0.0; */
/*     tile_transform[15] = 1.0; */

/*     multMatrix(mat_out, tile_transform, viewport_transform); */

    mat_out[ 0] = (GLdouble)global_viewport[2]/width;
    mat_out[ 1] = 0.0;
    mat_out[ 2] = 0.0;
    mat_out[ 3] = 0.0;

    mat_out[ 4] = 0.0;
    mat_out[ 5] = (GLdouble)global_viewport[3]/height;
    mat_out[ 6] = 0.0;
    mat_out[ 7] = 0.0;

    mat_out[ 8] = 0.0;
    mat_out[ 9] = 0.0;
    mat_out[10] = 1.0;
    mat_out[11] = 0.0;

    mat_out[12] = (GLdouble)(  global_viewport[2] + 2*global_viewport[0]
			     - 2*x - width)/width;
    mat_out[13] = (GLdouble)(  global_viewport[3] + 2*global_viewport[1]
			     - 2*y - height)/height;
    mat_out[14] = 0.0;
    mat_out[15] = 1.0;
}

static void update_tile_projections(void)
{
    GLint *viewports;
    int i;

    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);
    free(tile_projections);
    tile_projections = malloc(num_tiles*16*sizeof(GLdouble));
    viewports = icetUnsafeStateGet(ICET_TILE_VIEWPORTS);

    for (i = 0; i < num_tiles; i++) {
	icetGetViewportProject(viewports[i*4+0], viewports[i*4+1],
			       viewports[i*4+2], viewports[i*4+3],
			       tile_projections + 16*i);
    }
}

/* #define MI(r,c)	((c)*4+(r)) */
/* static void multMatrix(GLdouble *C, const GLdouble *A, const GLdouble *B) */
/* { */
/*     int i, j, k; */

/*     for (i = 0; i < 4; i++) { */
/* 	for (j = 0; j < 4; j++) { */
/* 	    C[MI(i,j)] = 0.0; */
/* 	    for (k = 0; k < 4; k++) { */
/* 		C[MI(i,j)] += A[MI(i,k)] * B[MI(k,j)]; */
/* 	    } */
/* 	} */
/*     } */
/* } */
