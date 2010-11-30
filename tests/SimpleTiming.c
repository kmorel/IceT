/* -*- c -*- *****************************************************************
** Copyright (C) 2010 Sandia Corporation
** Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
** the U.S. Government retains certain rights in this software.
**
** This source code is released under the New BSD License.
**
** This test provides a simple means of timing the IceT compositing.  It can be
** used for quick measurements and simple scaling studies.
*****************************************************************************/

#include <IceTGL.h>
#include <IceTDevCommunication.h>
#include "test-util.h"
#include "test_codes.h"

#ifdef __APPLE__
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
#  include <GLUT/glut.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#  include <GL/glut.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Structure used to capture the recursive division of space. */
struct region_divide_struct {
    int axis;           /* x = 0, y = 1, z = 2: the index to a vector array. */
    float cut;          /* Coordinate where cut occurs. */
    int my_side;        /* -1 on the negative side, 1 on the positive side. */
    int num_other_side; /* Number of partitions on other side. */
    struct region_divide_struct *next;
};

typedef struct region_divide_struct *region_divide;

/* Program arguments. */
static int g_num_tiles_x;
static int g_num_tiles_y;
static int g_num_frames;
static int g_seed;
static int g_transparent;
static IceTEnum g_strategy;
static IceTEnum g_single_image_strategy;

static void parse_arguments(int argc, char *argv[])
{
    int arg;

    g_num_tiles_x = 1;
    g_num_tiles_y = 1;
    g_num_frames = 100;
    g_seed = time(NULL);
    g_transparent = 0;
    g_strategy = ICET_STRATEGY_REDUCE;
    g_single_image_strategy = ICET_SINGLE_IMAGE_STRATEGY_AUTOMATIC;

    for (arg = 1; arg < argc; arg++) {
        if (strcmp(argv[arg], "-tilesx") == 0) {
            arg++;
            g_num_tiles_x = atoi(argv[arg]);
        } else if (strcmp(argv[arg], "-tilesy") == 0) {
            arg++;
            g_num_tiles_y = atoi(argv[arg]);
        } else if (strcmp(argv[arg], "-frames") == 0) {
            arg++;
            g_num_frames = atoi(argv[arg]);
        } else if (strcmp(argv[arg], "-seed") == 0) {
            arg++;
            g_seed = atoi(argv[arg]);
        } else if (strcmp(argv[arg], "-transparent") == 0) {
            g_transparent = 1;
        } else if (strcmp(argv[arg], "-reduce") == 0) {
            g_strategy = ICET_STRATEGY_REDUCE;
        } else if (strcmp(argv[arg], "-vtree") == 0) {
            g_strategy = ICET_STRATEGY_VTREE;
        } else if (strcmp(argv[arg], "-sequential") == 0) {
            g_strategy = ICET_STRATEGY_SEQUENTIAL;
        } else if (strcmp(argv[arg], "-bswap") == 0) {
            g_single_image_strategy = ICET_SINGLE_IMAGE_STRATEGY_BSWAP;
        } else if (strcmp(argv[arg], "-tree") == 0) {
            g_single_image_strategy = ICET_SINGLE_IMAGE_STRATEGY_TREE;
        } else {
            printf("Unknown option `%s'.\n", argv[arg]);
            exit(1);
        }
    }
}

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
                        float *bounds_max,
                        region_divide *divisions)
{
    int axis = 0;
    int start_rank = 0;         /* The first rank. */
    int end_rank = num_proc;    /* One after the last rank. */
    region_divide current_division = NULL;

    bounds_min[0] = bounds_min[1] = bounds_min[2] = -0.5;
    bounds_max[0] = bounds_max[1] = bounds_max[2] = 0.5;

    *divisions = NULL;

    /* Recursively split each axis, dividing the number of processes in my group
       in half each time. */
    while (1 < (end_rank - start_rank)) {
        float length = bounds_max[axis] - bounds_min[axis];
        int middle_rank = (start_rank + end_rank)/2;
        float region_cut;
        region_divide new_divide = malloc(sizeof(struct region_divide_struct));

        /* Skew the place where we cut the region based on the relative size
         * of the group size on each side, which may be different if the
         * group cannot be divided evenly. */
        region_cut = (  bounds_min[axis]
                      + length*(middle_rank-start_rank)/(end_rank-start_rank) );

        new_divide->axis = axis;
        new_divide->cut = region_cut;
        new_divide->next = NULL;

        if (rank < middle_rank) {
            /* My rank is in the lower region. */
            new_divide->my_side = -1;
            new_divide->num_other_side = end_rank - middle_rank;
            bounds_max[axis] = region_cut;
            end_rank = middle_rank;
        } else {
            /* My rank is in the upper region. */
            new_divide->my_side = 1;
            new_divide->num_other_side = middle_rank - start_rank;
            bounds_min[axis] = region_cut;
            start_rank = middle_rank;
        }

        if (current_division != NULL) {
            current_division->next = new_divide;
        } else {
            *divisions = new_divide;
        }
        current_division = new_divide;

        axis = (axis + 1)%3;
    }
}

/* Given the current OpenGL transformation matricies (representing camera
 * position), determine which side of each axis-aligned plane faces the
 * camera.  The results are stored in plane_orientations, which is expected
 * to be an array of size 3.  Entry 0 in plane_orientations will be positive
 * if the vector (1, 0, 0) points towards the camera, negative otherwise.
 * Entries 1 and 2 are likewise for the y and z vectors. */
static void get_axis_plane_orientations(int *plane_orientations)
{
    GLdouble modelview[16];
    GLdouble projection[16];
    GLint view[4] = { 0, 0, 1, 1};
    GLdouble dummy_x, dummy_y;
    GLdouble dist_0;
    int i;

    /* Get transformation matrices. */
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);

    /* Get distance from viewpoint to origin. */
    gluProject(0.0, 0.0, 0.0,
               modelview, projection, view,
               &dummy_x, &dummy_y, &dist_0);

    /* For each axis, determine the distance between viewpoint and unit vector
     * and use that and the distance to origin to determine whether the vector
     * points towards or away from the camera. */
    for (i = 0; i < 3; i++) {
        GLdouble unit_vector[3];
        GLdouble dist_unit;

        unit_vector[0] = unit_vector[1] = unit_vector[2] = 0.0;
        unit_vector[i] = 1.0;
        gluProject(unit_vector[0], unit_vector[1], unit_vector[2],
                   modelview, projection, view,
                   &dummy_x, &dummy_y, &dist_unit);

        if (dist_unit < dist_0) {
            plane_orientations[i] = 1;
        } else {
            plane_orientations[i] = -1;
        }
    }
}

/* Use the current OpenGL transformation matricies (representing camera
 * position) and the given region divisions to determine the composite
 * ordering. */
static void find_composite_order(region_divide region_divisions)
{
    int num_proc = icetCommSize();
    IceTInt *process_ranks = malloc(num_proc * sizeof(IceTInt));
    IceTInt my_position;
    int plane_orientations[3];
    region_divide current_divide;

    get_axis_plane_orientations(plane_orientations);

    my_position = 0;
    for (current_divide = region_divisions;
         current_divide != NULL;
         current_divide = current_divide->next) {
        int axis = current_divide->axis;
        int my_side = current_divide->my_side;
        int plane_side = plane_orientations[axis];
        /* If my_side is the side of the plane away from the camera, add
           everything on the other side as before me. */
        if (   ((my_side < 0) && (0 < plane_side))
            || ((0 < my_side) && (plane_side < 0)) ) {
            my_position += current_divide->num_other_side;
        }
    }

    process_ranks = malloc(num_proc * sizeof(IceTInt));
    icetCommAllgather(&my_position, 1, ICET_INT, process_ranks);

    icetEnable(ICET_ORDERED_COMPOSITE);
    icetCompositeOrder(process_ranks);

    free(process_ranks);
}

static int SimpleTimingRun()
{
    IceTInt rank;
    IceTInt num_proc;

    float aspect
        = (float)(g_num_tiles_x*SCREEN_WIDTH)/(g_num_tiles_y*SCREEN_HEIGHT);
    int frame;
    float bounds_min[3];
    float bounds_max[3];
    region_divide region_divisions;

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

    /* Other IceT state. */
    if (g_transparent) {
        icetCompositeMode(ICET_COMPOSITE_MODE_BLEND);
        icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_FLOAT);
        icetSetDepthFormat(ICET_IMAGE_DEPTH_NONE);
    } else {
        icetCompositeMode(ICET_COMPOSITE_MODE_Z_BUFFER);
        icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_UBYTE);
        icetSetDepthFormat(ICET_IMAGE_DEPTH_FLOAT);
    }

    /* Give IceT the bounds of the polygons that will be drawn.  Note that IceT
     * will take care of any transformation that happens before
     * icetGLDrawFrame. */
    icetBoundingBoxf(-0.5f, 0.5f, -0.5, 0.5, -0.5, 0.5);

    /* Determine the region we want the local geometry to be in.  This will be
     * used for the modelview transformation later. */
    find_region(rank, num_proc, bounds_min, bounds_max, &region_divisions);

    /* Set up the tiled display.  The asignment of displays to processes is
     * arbitrary because,as this is a timing test, I am not too concerned
     * about who shows what. */
    if (g_num_tiles_x*g_num_tiles_y <= num_proc) {
        int x, y, display_rank;
        icetResetTiles();
        display_rank = 0;
        for (y = 0; y < g_num_tiles_y; y++) {
            for (x = 0; x < g_num_tiles_x; x++) {
                icetAddTile(x*SCREEN_WIDTH,
                            y*SCREEN_HEIGHT,
                            SCREEN_WIDTH,
                            SCREEN_HEIGHT,
                            display_rank);
                display_rank++;
            }
        }
    } else {
        printf("Not enough processes to %dx%d tiles.\n",
               g_num_tiles_x, g_num_tiles_y);
        return TEST_FAILED;
    }

    icetStrategy(g_strategy);
    icetSingleImageStrategy(g_single_image_strategy);

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
        if (g_transparent) {
            color[0] *= 0.25;
            color[1] *= 0.25;
            color[2] *= 0.25;
            color[3] *= 0.25;
        }
        glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
    }

    /* Initialize randomness. */
    if (rank == 0) {
        int i;
        printf("Seed = %d\n", g_seed);
        for (i = 1; i < num_proc; i++) {
            icetCommSend(&g_seed, 1, ICET_INT, i, 33);
        }
    } else {
        icetCommRecv(&g_seed, 1, ICET_INT, 0, 33);
    }

    srand(g_seed);

    /* Print logging header. */
    if (rank == 0) {
        printf("HEADER,num processes,tiles x,tiles y,frame,rank,render time,buffer read time,buffer write time,compress time,blend time,draw time,composite time,bytes sent,frame time\n");
    }

    for (frame = 0; frame < g_num_frames; frame++) {
        IceTDouble elapsed_time = icetWallTime();

        /* We can set up a modelview matrix here and IceT will factor this in
         * determining the screen projection of the geometry. */
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        /* Move geometry back so that it can be seen by the camera. */
        glTranslatef(0.0, 0.0, -2.0);

        /* Rotate to some random view. */
        glRotatef((360.0*rand())/RAND_MAX, 1.0, 0.0, 0.0);
        glRotatef((360.0*rand())/RAND_MAX, 0.0, 1.0, 0.0);
        glRotatef((360.0*rand())/RAND_MAX, 0.0, 0.0, 1.0);

        /* Determine view ordering of geometry based on camera position
           (represented by the current projection and modelview matrices). */
        if (g_transparent) {
            find_composite_order(region_divisions);
        }

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

        elapsed_time = icetWallTime() - elapsed_time;

        /* Print timings to logging. */
        {
            IceTDouble render_time;
            IceTDouble buffer_read_time;
            IceTDouble buffer_write_time;
            IceTDouble compress_time;
            IceTDouble blend_time;
            IceTDouble draw_time;
            IceTDouble composite_time;
            IceTInt bytes_sent;

            icetGetDoublev(ICET_RENDER_TIME, &render_time);
            icetGetDoublev(ICET_BUFFER_READ_TIME, &buffer_read_time);
            icetGetDoublev(ICET_BUFFER_WRITE_TIME, &buffer_write_time);
            icetGetDoublev(ICET_COMPRESS_TIME, &compress_time);
            icetGetDoublev(ICET_BLEND_TIME, &blend_time);
            icetGetDoublev(ICET_TOTAL_DRAW_TIME, &draw_time);
            icetGetDoublev(ICET_COMPOSITE_TIME, &composite_time);
            icetGetIntegerv(ICET_BYTES_SENT, &bytes_sent);

            printf("LOG,%d,%d,%d,%d,%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%d,%lf\n",
                   num_proc,
                   g_num_tiles_x,
                   g_num_tiles_y,
                   frame,
                   rank,
                   render_time,
                   buffer_read_time,
                   buffer_write_time,
                   compress_time,
                   blend_time,
                   draw_time,
                   composite_time,
                   bytes_sent,
                   elapsed_time);
        }
    }

    return TEST_PASSED;
}

int SimpleTiming(int argc, char * argv[])
{
    parse_arguments(argc, argv);

    return run_test(SimpleTimingRun);
}
