/* -*- c -*- *****************************************************************
** Id
**
** Copyright (C) 2003 Sandia Corporation
** Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
** license for use of this work by or on behalf of the U.S. Government.
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that this Notice and any statement
** of authorship are reproduced on all copies.
**
** Test that has each processor draw a randomly placed quadrilateral.
** Makes sure that all compositions are equivalent.
*****************************************************************************/

#include <GL/ice-t.h>
#include "test_codes.h"
#include "test-util.h"
#include "glwin.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>

static int tile_dim;

static void draw(void)
{
    printf("In draw\n");
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBegin(GL_QUADS);
      glVertex3f(-1.0, -1.0, 0.0);
      glVertex3f(1.0, -1.0, 0.0);
      glVertex3f(1.0, 1.0, 0.0);
      glVertex3f(-1.0, 1.0, 0.0);
    glEnd();
    printf("Leaving draw\n");
}

int RandomTransform(int argc, char *argv[])
{
    int i, x, y;
    GLubyte *cb;
    GLubyte *refcbuf = NULL;
    GLuint *db;
    GLuint *refdbuf = NULL;
    int result = TEST_PASSED;
    GLfloat mat[16];
    char filename[FILENAME_MAX];
    int rank, num_proc;

    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSORS, &num_proc);

  /* Set up OpenGL. */
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glDisable(GL_LIGHTING);
    if ((rank&0x07) == 0) {
	glColor3f(0.5, 0.5, 0.5);
    } else {
	glColor3f(1.0f*((rank&0x01) == 0x01),
		  1.0f*((rank&0x02) == 0x02),
		  1.0f*((rank&0x04) == 0x04));
    }

  /* Set up ICE-T. */
    icetDrawFunc(draw);
    icetBoundingBoxf(-1.0, 1.0, -1.0, 1.0, -0.125, 0.125);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);

  /* Get random transformation matrix. */
    srand(time(NULL) + 10*num_proc*rank);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(2.0f*rand()/RAND_MAX - 1.0f,
		 2.0f*rand()/RAND_MAX - 1.0f,
		 2.0f*rand()/RAND_MAX - 1.0f);
    glRotatef(360.0f*rand()/RAND_MAX, 0.0f, 0.0f,
	      (float)rand()/RAND_MAX);
    glScalef((float)(1.0/sqrt(num_proc) - 1.0)*(float)rand()/RAND_MAX + 1.0f,
	     (float)(1.0/sqrt(num_proc) - 1.0)*(float)rand()/RAND_MAX + 1.0f,
	     1.0f);
    glGetFloatv(GL_MODELVIEW_MATRIX, mat);

    printf("Transformation:\n");
    printf("    %f %f %f %f\n", mat[0], mat[4], mat[8], mat[12]);
    printf("    %f %f %f %f\n", mat[1], mat[5], mat[9], mat[13]);
    printf("    %f %f %f %f\n", mat[2], mat[6], mat[10], mat[14]);
    printf("    %f %f %f %f\n", mat[3], mat[7], mat[11], mat[15]);

  /* Let everyone get a base image for comparison. */
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    icetStrategy(ICET_STRATEGY_SERIAL);
    icetResetTiles();
    for (i = 0; i < num_proc; i++) {
	icetAddTile(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, i);
    }
    icetInputOutputBuffers(ICET_COLOR_BUFFER_BIT | ICET_DEPTH_BUFFER_BIT,
			   ICET_COLOR_BUFFER_BIT | ICET_DEPTH_BUFFER_BIT);

    printf("\nGetting base image.\n");
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(mat);
    icetDrawFrame();
    swap_buffers();

    cb = icetGetColorBuffer();
    refcbuf = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    memcpy(refcbuf, cb, SCREEN_WIDTH * SCREEN_HEIGHT * 4);

    db = icetGetDepthBuffer();
    refdbuf = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(GLuint));
    memcpy(refdbuf, db, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(GLuint));

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    for (i = 0; i < STRATEGY_LIST_SIZE; i++) {
	icetStrategy(strategy_list[i]);
	printf("\n\nUsing %s strategy.\n", icetGetStrategyName());

	for (tile_dim = 1; tile_dim*tile_dim <= num_proc; tile_dim++) {
	    int local_width = SCREEN_WIDTH/tile_dim;
	    int local_height = SCREEN_HEIGHT/tile_dim;
	    int viewport_width = SCREEN_WIDTH, viewport_height = SCREEN_HEIGHT;
	    int viewport_offset_x = 0, viewport_offset_y = 0;

	    printf("\nRunning on a %d x %d display.\n", tile_dim, tile_dim);
	    icetResetTiles();
	    for (y = 0; y < tile_dim; y++) {
		for (x = 0; x < tile_dim; x++) {
		    icetAddTile(x*local_width, y*local_height,
				local_width, local_height, y*tile_dim + x);
		}
	    }

	    if (tile_dim > 1) {
		viewport_width
		    = rand()%(SCREEN_WIDTH-local_width) + local_width;
		viewport_height
		    = rand()%(SCREEN_HEIGHT-local_height) + local_height;
	    }
	    if (viewport_width < SCREEN_WIDTH) {
		viewport_offset_x = rand()%(SCREEN_WIDTH-viewport_width);
	    }
	    if (viewport_width < SCREEN_HEIGHT) {
		viewport_offset_y = rand()%(SCREEN_HEIGHT-viewport_height);
	    }
	    
/* 	    glViewport(viewport_offset_x, viewport_offset_y, */
/* 		       viewport_width, viewport_height); */
/* 	    glViewport(0, 0, local_width, local_height); */

	    printf("\nDoing color buffer.\n");
	    icetInputOutputBuffers(ICET_COLOR_BUFFER_BIT|ICET_DEPTH_BUFFER_BIT,
				   ICET_COLOR_BUFFER_BIT);

	    printf("Rendering frame.\n");
	    glMatrixMode(GL_PROJECTION);
	    glLoadIdentity();
	    glOrtho(-1, (float)(2*local_width*tile_dim)/SCREEN_WIDTH-1,
		    -1, (float)(2*local_height*tile_dim)/SCREEN_HEIGHT-1,
		    -1, 1);
	    glMatrixMode(GL_MODELVIEW);
	    glLoadMatrixf(mat);
	    icetDrawFrame();
	    swap_buffers();

	    if (rank < tile_dim*tile_dim) {
		int ref_off_x, ref_off_y;
		int bad_pixel_count;
		printf("Checking returned image.\n");
		cb = icetGetColorBuffer();
		ref_off_x = (rank%tile_dim) * local_width;
		ref_off_y = (rank/tile_dim) * local_height;
		bad_pixel_count = 0;
#define CBR(x, y) (cb[(y)*local_width*4 + (x)*4 + 0])
#define CBG(x, y) (cb[(y)*local_width*4 + (x)*4 + 1])
#define CBB(x, y) (cb[(y)*local_width*4 + (x)*4 + 2])
#define CBA(x, y) (cb[(y)*local_width*4 + (x)*4 + 3])
#define REFCBUFR(x, y) (refcbuf[(y)*SCREEN_WIDTH*4 + (x)*4 + 0])
#define REFCBUFG(x, y) (refcbuf[(y)*SCREEN_WIDTH*4 + (x)*4 + 1])
#define REFCBUFB(x, y) (refcbuf[(y)*SCREEN_WIDTH*4 + (x)*4 + 2])
#define REFCBUFA(x, y) (refcbuf[(y)*SCREEN_WIDTH*4 + (x)*4 + 3])
#define CB_EQUALS_REF(x, y)			\
    (   (CBR((x), (y)) == REFCBUFR((x) + ref_off_x, (y) + ref_off_y) )	\
     && (CBG((x), (y)) == REFCBUFG((x) + ref_off_x, (y) + ref_off_y) )	\
     && (CBB((x), (y)) == REFCBUFB((x) + ref_off_x, (y) + ref_off_y) )	\
     && (   CBA((x), (y)) == REFCBUFA((x) + ref_off_x, (y) + ref_off_y)	\
	 || CBA((x), (y)) == 0 ) )

		for (y = 0; y < local_height; y++) {
		    for (x = 0; x < local_width; x++) {
			if (!CB_EQUALS_REF(x, y)) {
			  /* Uh, oh.  Pixels don't match.  This could be a  */
			  /* genuine error or it could be a floating point  */
			  /* offset when projecting edge boundries to	    */
			  /* pixels.  If the latter is the case, there will */
			  /* be very few errors.  Count the errors, and	    */
			  /* make sure there are not too many.		    */
			    bad_pixel_count++;
			}
		    }
		}

	      /* Check to make sure there were not too many errors. */
		if (   (bad_pixel_count > 0.001*local_width*local_height)
		    && (bad_pixel_count > local_width)
		    && (bad_pixel_count > local_height) )
		{
		  /* Too many errors.  Call it bad. */
		    printf("Too many bad pixels!!!!!!\n");
		  /* Write current images. */
		    sprintf(filename, "ref%03d.ppm", rank);
		    write_ppm(filename, refcbuf, SCREEN_WIDTH, SCREEN_HEIGHT);
		    sprintf(filename, "bad%03d.ppm", rank);
		    write_ppm(filename, cb, local_width, local_height);
		  /* Write difference image. */
		    for (y = 0; y < local_height; y++) {
			for (x = 0; x < local_width; x++) {
			    int off_x = x + ref_off_x;
			    int off_y = y + ref_off_y;
			    if (CBR(x, y) < REFCBUFR(off_x, off_y)){
				CBR(x,y) = REFCBUFR(off_x,off_y) - CBR(x,y);
			    } else {
				CBR(x,y) = CBR(x,y) - REFCBUFR(off_x,off_y);
			    }
			    if (CBG(x, y) < REFCBUFG(off_x, off_y)){
				CBG(x,y) = REFCBUFG(off_x,off_y) - CBG(x,y);
			    } else {
				CBG(x,y) = CBG(x,y) - REFCBUFG(off_x,off_y);
			    }
			    if (CBB(x, y) < REFCBUFB(off_x, off_y)){
				CBB(x,y) = REFCBUFB(off_x,off_y) - CBB(x,y);
			    } else {
				CBB(x,y) = CBB(x,y) - REFCBUFB(off_x,off_y);
			    }
			}
		    }
		    sprintf(filename, "diff%03d.ppm", rank);
		    write_ppm(filename, cb, local_width, local_height);
		    result = TEST_FAILED;
		}
#undef CBR
#undef CBG
#undef CBB
#undef CBA
#undef REFCBUFR
#undef REFCBUFG
#undef REFCBUFB
#undef REFCBUFA
#undef CB_EQUALS_REF
	    }

	    printf("\nDoing depth buffer.\n");
	    icetInputOutputBuffers(ICET_DEPTH_BUFFER_BIT,ICET_DEPTH_BUFFER_BIT);

	    printf("Rendering frame.\n");
	    glMatrixMode(GL_PROJECTION);
	    glLoadIdentity();
	    glOrtho(-1, (float)(2*local_width*tile_dim)/SCREEN_WIDTH-1,
		    -1, (float)(2*local_height*tile_dim)/SCREEN_HEIGHT-1,
		    -1, 1);
	    glMatrixMode(GL_MODELVIEW);
	    glLoadMatrixf(mat);
	    icetDrawFrame();
	    swap_buffers();

	    if (rank < tile_dim*tile_dim) {
		int ref_off_x, ref_off_y;
		int bad_pixel_count;
		printf("Checking returned image.\n");
		db = icetGetDepthBuffer();
		ref_off_x = (rank%tile_dim) * local_width;
		ref_off_y = (rank/tile_dim) * local_height;
		bad_pixel_count = 0;

		for (y = 0; y < local_height; y++) {
		    for (x = 0; x < local_width; x++) {
			if (   db[y*local_width + x]
			    != refdbuf[(y+ref_off_y)*SCREEN_WIDTH
				      +x + ref_off_x] ) {
			  /* Uh, oh.  Pixels don't match.  This could be a  */
			  /* genuine error or it could be a floating point  */
			  /* offset when projecting edge boundries to	    */
			  /* pixels.  If the latter is the case, there will */
			  /* be very few errors.  Count the errors, and	    */
			  /* make sure there are not too many.		    */
			    bad_pixel_count++;
			}
		    }
		}

	      /* Check to make sure there were not too many errors. */
		if (   (bad_pixel_count > 0.001*local_width*local_height)
		    && (bad_pixel_count > local_width)
		    && (bad_pixel_count > local_height) )
		{
		  /* Too many errors.  Call it bad. */
		    printf("Too many bad pixels!!!!!!\n");

		  /* Write encoded image. */
		    for (y = 0; y < local_height; y++) {
			for (x = 0; x < local_width; x++) {
			    GLuint ref = refdbuf[(y+ref_off_y)*SCREEN_WIDTH
						+x + ref_off_x];
			    GLuint rendered = db[y*local_width + x];
			    GLubyte *encoded = (GLubyte *)&db[y*local_width+x];
			    long error = ref - rendered;
			    if (error < 0) error = -error;
			    encoded[0] = (error & 0xFF000000) >> 24;
			    encoded[1] = (error & 0x00FF0000) >> 16;
			    encoded[2] = (error & 0x0000FF00) >> 8;
			}
		    }
		    sprintf(filename, "depth_error%03d.ppm", rank);
		    write_ppm(filename, (GLubyte*)db,
			      local_width, local_height);

		    result = TEST_FAILED;
		}
	    } else {
		printf("Not a display node.  Not testing image.\n");
	    }
	}
    }

    printf("Cleaning up.\n");
    free(refcbuf);
    free(refdbuf);
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    icetInputOutputBuffers(ICET_COLOR_BUFFER_BIT | ICET_DEPTH_BUFFER_BIT,
			   ICET_COLOR_BUFFER_BIT);

    finalize_test();
    return result;
}
