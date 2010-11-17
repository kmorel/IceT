/* -*- c -*- *****************************************************************
** Copyright (C) 2010 Sandia Corporation
** Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
** the U.S. Government retains certain rights in this software.
**
** This source code is released under the New BSD License.
**
** This test provides a simple example of using IceT to perform parallel
** rendering.
*****************************************************************************/

#include <IceTGL.h>
#include "test-util.h"
#include "test_codes.h"

#ifdef __APPLE__
#  include <OpenGL/gl.h>
#  include <GLUT/glut.h>
#else
#  include <GL/gl.h>
#  include <GL/glut.h>
#endif

#include <stdlib.h>
#include <stdio.h>

static void draw(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Draws an axis aligned cube from (-.5, -.5, -.5) to (.5, .5, .5). */
    glutSolidCube(1.0);
}

/* Given the rank of this process in all of them, divides the unit box
 * centered on the origin evenly (w.r.t. area) amongst all processes.  The
 * region for this process, characterized by the min and max corners, is
 * returned in the bounds_min and bounds_max parameters. */
static void find_region(int rank,
                        int num_proc,
                        float *bounds_min,
                        float *bounds_max)
{
    int axis = 0;
    int start_rank = 0;         /* The first rank. */
    int end_rank = num_proc;    /* One after the last rank. */

    bounds_min[0] = bounds_min[1] = bounds_min[2] = -0.5;
    bounds_max[0] = bounds_max[1] = bounds_max[2] = 0.5;

    /* Recursively split each axis, dividing the number of processes in my group
       in half each time. */
    while (1 < (end_rank - start_rank)) {
        float length = bounds_max[axis] - bounds_min[axis];
        int middle_rank = (start_rank + end_rank)/2;
        float region_cut;

        /* Skew the place where we cut the region based on the relative size
         * of the group size on each side, which may be different if the
         * group cannot be divided evenly. */
        region_cut = (  bounds_min[axis]
                      + length*(middle_rank-start_rank)/(end_rank-start_rank) );

        if (rank < middle_rank) {
            /* My rank is in the lower region. */
            bounds_max[axis] = region_cut;
            end_rank = middle_rank;
        } else {
            /* My rank is in the upper region. */
            bounds_min[axis] = region_cut;
            start_rank = middle_rank;
        }

        axis = (axis + 1)%3;
    }
}

static int SimpleTimingRun()
{
    IceTInt rank;
    IceTInt num_proc;

    float aspect = (float)SCREEN_WIDTH/SCREEN_HEIGHT;
    float angle;
    float bounds_min[3];
    float bounds_max[3];

    /* Normally, the first thing that you do is set up your communication and
     * then create at least one IceT context.  This has already been done in the
     * calling function (i.e. icetTests_mpi.c).  See the init_mpi_comm in
     * mpi_comm.h for an example.
     */

    /* If we had set up the communication layer ourselves, we could have gotten
     * these parameters directly from it.  Since we did not, this provides an
     * alternate way. */
    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);

    /* We should be able to set any color we want, but we should do it BEFORE
     * icetDrawFrame() is called, not in the callback drawing function.  There
     * may also be limitations on the background color when performing color
     * blending. */
    glClearColor(0.2f, 0.5f, 0.1f, 1.0f);

    /* Give IceT a function that will issue the OpenGL drawing commands. */
    icetGLDrawCallback(draw);

    /* Give IceT the bounds of the polygons that will be drawn.  Note that IceT
     * will take care of any transformation that happens before
     * icetGLDrawFrame. */
    icetBoundingBoxf(-0.5f, 0.5f, -0.5, 0.5, -0.5, 0.5);

    /* Determine the region we want the local geometry to be in.  This will be
     * used for the modelview transformation later. */
    find_region(rank, num_proc, bounds_min, bounds_max);

    /* Set up the tiled display.  Normally, the display will be fixed for a
     * given installation, but since this is a demo, we give two specific
     * examples. */
    if (num_proc < 4) {
        /* Here is an example of a "1 tile" case.  This is functionally
         * identical to a traditional sort last algorithm. */
        icetResetTiles();
        icetAddTile(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    } else {
        /* Here is an example of a 4x4 tile layout.  The tiles are displayed
         * with the following ranks:
         *
         *               +---+---+
         *               | 0 | 1 |
         *               +---+---+
         *               | 2 | 3 |
         *               +---+---+
         *
         * Each tile is simply defined by grabing a viewport in an infinite
         * global display screen.  The global viewport projection is
         * automatically set to the smallest region containing all tiles.
         *
         * This example also shows tiles abutted against each other.  Mullions
         * and overlaps can be implemented by simply shifting tiles on top of or
         * away from each other.
         */
        icetResetTiles();
        icetAddTile(0,           SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
        icetAddTile(SCREEN_WIDTH,SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, 1);
        icetAddTile(0,           0,             SCREEN_WIDTH, SCREEN_HEIGHT, 2);
        icetAddTile(SCREEN_WIDTH,0,             SCREEN_WIDTH, SCREEN_HEIGHT, 3);
    }

    /* Tell IceT what strategy to use.  The REDUCE strategy is an all-around
     * good performer. */
    icetStrategy(ICET_STRATEGY_REDUCE);

    /* Set up the projection matrix as you normally would. */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.5*aspect, 0.5*aspect, -0.5, 0.5, 1.0, 3.0);

    /* Other normal OpenGL setup. */
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    if (rank%8 != 0) {
        GLfloat color[4];
        color[0] = (float)(rank%2);
        color[1] = (float)((rank/2)%2);
        color[2] = (float)((rank/4)%2);
        color[3] = 1.0;
        glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
    }

  /* Here is an example of an animation loop. */
    for (angle = 0; angle < 360; angle += 10) {
      /* We can set up a modelview matrix here and IceT will factor this in
       * determining the screen projection of the geometry. */
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        /* Move geometry back so that it can be seen by the camera. */
        glTranslatef(0.0, 0.0, -2.0);

        /* Rotate around the predetermined angle. */
        glRotatef(angle, 0.0, 1.0, 0.0);

        /* Translate the unit box centered on the origin to the region specified
         * by bounds_min and bounds_max. */
        glTranslatef(bounds_min[0], bounds_min[1], bounds_min[2]);
        glScalef(bounds_max[0] - bounds_min[0],
                 bounds_max[1] - bounds_min[1],
                 bounds_max[2] - bounds_min[2]);
        glTranslatef(0.5, 0.5, 0.5);

      /* Instead of calling draw() directly, call it indirectly through
       * icetDrawFrame().  IceT will automatically handle image
       * compositing. */
        icetGLDrawFrame();

      /* For obvious reasons, IceT should be run in double-buffered frame
       * mode.  After calling icetDrawFrame, the application should do a
       * synchronize (a barrier is often about as good as you can do) and
       * then a swap buffers. */
        swap_buffers();
    }

    return TEST_PASSED;
}

int SimpleTiming(int argc, char * argv[])
{
    /* To remove warning */
    (void)argc;
    (void)argv;

    return run_test(SimpleTimingRun);
}
