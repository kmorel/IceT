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

#include <IceTDevCommunication.h>
#include <IceTDevMatrix.h>
#include "test-util.h"
#include "test_codes.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>

#ifndef M_E
#define M_E         2.71828182845904523536028747135266250   /* e */
#endif

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
static int g_write_image;
static IceTEnum g_strategy;
static IceTEnum g_single_image_strategy;

static float g_color[4];

static void parse_arguments(int argc, char *argv[])
{
    int arg;

    g_num_tiles_x = 1;
    g_num_tiles_y = 1;
    g_num_frames = 100;
    g_seed = (int)time(NULL);
    g_transparent = 0;
    g_write_image = 0;
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
        } else if (strcmp(argv[arg], "-write-image") == 0) {
            g_write_image = 1;
        } else if (strcmp(argv[arg], "-reduce") == 0) {
            g_strategy = ICET_STRATEGY_REDUCE;
        } else if (strcmp(argv[arg], "-vtree") == 0) {
            g_strategy = ICET_STRATEGY_VTREE;
        } else if (strcmp(argv[arg], "-sequential") == 0) {
            g_strategy = ICET_STRATEGY_SEQUENTIAL;
        } else if (strcmp(argv[arg], "-bswap") == 0) {
            g_single_image_strategy = ICET_SINGLE_IMAGE_STRATEGY_BSWAP;
        } else if (strcmp(argv[arg], "-radixk") == 0) {
            g_single_image_strategy = ICET_SINGLE_IMAGE_STRATEGY_RADIXK;
        } else if (strcmp(argv[arg], "-tree") == 0) {
            g_single_image_strategy = ICET_SINGLE_IMAGE_STRATEGY_TREE;
        } else {
            printf("Unknown option `%s'.\n", argv[arg]);
            exit(1);
        }
    }
}

#define NUM_HEX_PLANES 6
struct hexahedron {
    IceTDouble planes[NUM_HEX_PLANES][4];
};

static void intersect_ray_plane(const IceTDouble *ray_origin,
                                const IceTDouble *ray_direction,
                                const IceTDouble *plane,
                                IceTDouble *distance,
                                IceTBoolean *front_facing,
                                IceTBoolean *parallel)
{
    IceTDouble distance_numerator = icetDot3(plane, ray_origin) + plane[3];
    IceTDouble distance_denominator = icetDot3(plane, ray_direction);

    if (distance_denominator == 0.0) {
        *parallel = ICET_TRUE;
        *front_facing = (distance_numerator > 0);
    } else {
        *parallel = ICET_FALSE;
        *distance = -distance_numerator/distance_denominator;
        *front_facing = (distance_denominator < 0);
    }
}

/* This algorithm (and associated intersect_ray_plane) come from Graphics Gems
 * II, Fast Ray-Convex Polyhedron Intersection by Eric Haines. */
static void intersect_ray_hexahedron(const IceTDouble *ray_origin,
                                     const IceTDouble *ray_direction,
                                     const struct hexahedron hexahedron,
                                     IceTDouble *near_distance,
                                     IceTDouble *far_distance,
                                     IceTInt *near_plane_index,
                                     IceTBoolean *intersection_happened)
{
    int planeIdx;

    *near_distance = 0.0;
    *far_distance = 2.0;
    *near_plane_index = -1;

    for (planeIdx = 0; planeIdx < NUM_HEX_PLANES; planeIdx++) {
        IceTDouble distance;
        IceTBoolean front_facing;
        IceTBoolean parallel;

        intersect_ray_plane(ray_origin,
                            ray_direction,
                            hexahedron.planes[planeIdx],
                            &distance,
                            &front_facing,
                            &parallel);

        if (!parallel) {
            if (front_facing) {
                if (*near_distance < distance) {
                    *near_distance = distance;
                    *near_plane_index = planeIdx;
                }
            } else {
                if (distance < *far_distance) {
                    *far_distance = distance;
                }
            }
        } else { /*parallel*/
            if (front_facing) {
                /* Ray missed parallel plane.  No intersection. */
                *intersection_happened = ICET_FALSE;
                return;
            }
        }
    }

    *intersection_happened = (*near_distance < *far_distance);
}

/* Plane equations for unit box on origin. */
struct hexahedron unit_box = {
    {
        { -1.0, 0.0, 0.0, -0.5 },
        { 1.0, 0.0, 0.0, -0.5 },
        { 0.0, -1.0, 0.0, -0.5 },
        { 0.0, 1.0, 0.0, -0.5 },
        { 0.0, 0.0, -1.0, -0.5 },
        { 0.0, 0.0, 1.0, -0.5 }
    }
};

static void draw(const IceTDouble *projection_matrix,
                 const IceTDouble *modelview_matrix,
                 const IceTFloat *background_color,
                 const IceTInt *readback_viewport,
                 IceTImage result)
{
    IceTDouble transform[16];
    IceTDouble inverse_transpose_transform[16];
    IceTBoolean success;
    int planeIdx;
    struct hexahedron transformed_box;
    IceTInt screen_width;
    IceTInt screen_height;
    IceTFloat *colors_float = NULL;
    IceTUByte *colors_byte = NULL;
    IceTFloat *depths = NULL;
    IceTInt pixel_x;
    IceTInt pixel_y;
    IceTDouble ray_origin[3];
    IceTDouble ray_direction[3];

    icetMatrixMultiply(transform, projection_matrix, modelview_matrix);

    success = icetMatrixInverseTranspose((const IceTDouble *)transform,
                                         inverse_transpose_transform);
    if (!success) {
        printf("ERROR: Inverse failed.\n");
    }

    for (planeIdx = 0; planeIdx < NUM_HEX_PLANES; planeIdx++) {
        const IceTDouble *original_plane = unit_box.planes[planeIdx];
        IceTDouble *transformed_plane = transformed_box.planes[planeIdx];

        icetMatrixVectorMultiply(transformed_plane,
                                 inverse_transpose_transform,
                                 original_plane);
    }

    icetGetIntegerv(ICET_PHYSICAL_RENDER_WIDTH, &screen_width);
    icetGetIntegerv(ICET_PHYSICAL_RENDER_HEIGHT, &screen_height);

    if (g_transparent) {
        colors_float = icetImageGetColorf(result);
    } else {
        colors_byte = icetImageGetColorub(result);
        depths = icetImageGetDepthf(result);
    }

    ray_direction[0] = ray_direction[1] = 0.0;
    ray_direction[2] = 1.0;
    ray_origin[2] = -1.0;
    for (pixel_y = readback_viewport[1];
         pixel_y < readback_viewport[1] + readback_viewport[3];
         pixel_y++) {
        ray_origin[1] = (2.0*pixel_y)/screen_height - 1.0;
        for (pixel_x = readback_viewport[0];
             pixel_x < readback_viewport[0] + readback_viewport[2];
             pixel_x++) {
            IceTDouble near_distance;
            IceTDouble far_distance;
            IceTInt near_plane_index;
            IceTBoolean intersection_happened;
            IceTFloat color[4];
            IceTFloat depth;

            ray_origin[0] = (2.0*pixel_x)/screen_width - 1.0;

            intersect_ray_hexahedron(ray_origin,
                                     ray_direction,
                                     transformed_box,
                                     &near_distance,
                                     &far_distance,
                                     &near_plane_index,
                                     &intersection_happened);

            if (intersection_happened) {
                const IceTDouble *near_plane;
                IceTDouble shading;

                near_plane = transformed_box.planes[near_plane_index];
                shading = -near_plane[2]/sqrt(icetDot3(near_plane, near_plane));

                color[0] = g_color[0] * (IceTFloat)shading;
                color[1] = g_color[1] * (IceTFloat)shading;
                color[2] = g_color[2] * (IceTFloat)shading;
                color[3] = g_color[3];
                depth = (IceTFloat)(0.5*near_distance);
                if (g_transparent) {
                    /* Modify color by an opacity determined by thickness. */
                    IceTDouble thickness = far_distance - near_distance;
                    IceTDouble opacity = 1.0 - pow(M_E, -4.0*thickness);
                    color[0] *= (IceTFloat)opacity;
                    color[1] *= (IceTFloat)opacity;
                    color[2] *= (IceTFloat)opacity;
                    color[3] *= (IceTFloat)opacity;
                }
            } else {
                color[0] = background_color[0];
                color[1] = background_color[1];
                color[2] = background_color[2];
                color[3] = background_color[3];
                depth = 1.0f;
            }

            if (g_transparent) {
                IceTFloat *color_dest
                    = colors_float + 4*(pixel_y*screen_width + pixel_x);
                color_dest[0] = color[0];
                color_dest[1] = color[1];
                color_dest[2] = color[2];
                color_dest[3] = color[3];
            } else {
                IceTUByte *color_dest
                    = colors_byte + 4*(pixel_y*screen_width + pixel_x);
                IceTFloat *depth_dest
                    = depths + pixel_y*screen_width + pixel_x;
                color_dest[0] = (IceTUByte)(color[0]*255);
                color_dest[1] = (IceTUByte)(color[1]*255);
                color_dest[2] = (IceTUByte)(color[2]*255);
                color_dest[3] = (IceTUByte)(color[3]*255);
                depth_dest[0] = depth;
            }
        }
    }
}

/* Given the rank of this process in all of them, divides the unit box
 * centered on the origin evenly (w.r.t. volume) amongst all processes.  The
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

    bounds_min[0] = bounds_min[1] = bounds_min[2] = -0.5f;
    bounds_max[0] = bounds_max[1] = bounds_max[2] = 0.5f;

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

/* Given the transformation matricies (representing camera position), determine
 * which side of each axis-aligned plane faces the camera.  The results are
 * stored in plane_orientations, which is expected to be an array of size 3.
 * Entry 0 in plane_orientations will be positive if the vector (1, 0, 0) points
 * towards the camera, negative otherwise.  Entries 1 and 2 are likewise for the
 * y and z vectors. */
static void get_axis_plane_orientations(const IceTDouble *projection,
                                        const IceTDouble *modelview,
                                        int *plane_orientations)
{
    IceTDouble full_transform[16];
    IceTDouble inverse_transpose_transform[16];
    IceTBoolean success;
    int planeIdx;

    icetMatrixMultiply(full_transform, projection, modelview);
    success = icetMatrixInverseTranspose((const IceTDouble *)full_transform,
                                         inverse_transpose_transform);

    for (planeIdx = 0; planeIdx < 3; planeIdx++) {
        IceTDouble plane_equation[4];
        IceTDouble transformed_plane[4];

        plane_equation[0] = plane_equation[1]
            = plane_equation[2] = plane_equation[3] = 0.0;
        plane_equation[planeIdx] = 1.0;

        /* To transform a plane, multiply the vector representing the plane
         * equation (ax + by + cz + d = 0) by the inverse transpose of the
         * transform. */
        icetMatrixVectorMultiply(transformed_plane,
                                 (const IceTDouble*)inverse_transpose_transform,
                                 (const IceTDouble*)plane_equation);

        /* If the normal of the plane is facing in the -z direction, then the
         * front of the plane is facing the camera. */
        if (transformed_plane[3] < 0) {
            plane_orientations[planeIdx] = 1;
        } else {
            plane_orientations[planeIdx] = -1;
        }
    }
}

/* Use the current OpenGL transformation matricies (representing camera
 * position) and the given region divisions to determine the composite
 * ordering. */
static void find_composite_order(const IceTDouble *projection,
                                 const IceTDouble *modelview,
                                 region_divide region_divisions)
{
    int num_proc = icetCommSize();
    IceTInt *process_ranks = malloc(num_proc * sizeof(IceTInt));
    IceTInt my_position;
    int plane_orientations[3];
    region_divide current_divide;

    get_axis_plane_orientations(projection, modelview, plane_orientations);

    my_position = 0;
    for (current_divide = region_divisions;
         current_divide != NULL;
         current_divide = current_divide->next) {
        int axis = current_divide->axis;
        int my_side = current_divide->my_side;
        int plane_side = plane_orientations[axis];
        /* If my_side is the side of the plane away from the camera, add
           everything on the other side as before me. */
        if (   ((my_side < 0) && (plane_side < 0))
            || ((0 < my_side) && (0 < plane_side)) ) {
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

    float aspect = (  (float)(g_num_tiles_x*SCREEN_WIDTH)
                    / (float)(g_num_tiles_y*SCREEN_HEIGHT) );
    int frame;
    float bounds_min[3];
    float bounds_max[3];
    region_divide region_divisions;

    IceTDouble projection_matrix[16];
    IceTFloat background_color[4];

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

    background_color[0] = 0.2f;
    background_color[1] = 0.5f;
    background_color[2] = 0.7f;
    background_color[3] = 1.0f;

    /* Give IceT a function that will issue the drawing commands. */
    icetDrawCallback(draw);

    /* Other IceT state. */
    if (g_transparent) {
        icetCompositeMode(ICET_COMPOSITE_MODE_BLEND);
        icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_FLOAT);
        icetSetDepthFormat(ICET_IMAGE_DEPTH_NONE);
        icetEnable(ICET_CORRECT_COLORED_BACKGROUND);
    } else {
        icetCompositeMode(ICET_COMPOSITE_MODE_Z_BUFFER);
        icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_UBYTE);
        icetSetDepthFormat(ICET_IMAGE_DEPTH_FLOAT);
    }

    /* Give IceT the bounds of the polygons that will be drawn.  Note that IceT
     * will take care of any transformation that gets passed to
     * icetDrawFrame. */
    icetBoundingBoxd(-0.5f, 0.5f, -0.5, 0.5, -0.5, 0.5);

    /* Determine the region we want the local geometry to be in.  This will be
     * used for the modelview transformation later. */
    find_region(rank, num_proc, bounds_min, bounds_max, &region_divisions);

    /* Set up the tiled display.  The asignment of displays to processes is
     * arbitrary because, as this is a timing test, I am not too concerned
     * about who shows what. */
    if (g_num_tiles_x*g_num_tiles_y <= num_proc) {
        int x, y, display_rank;
        icetResetTiles();
        display_rank = 0;
        for (y = 0; y < g_num_tiles_y; y++) {
            for (x = 0; x < g_num_tiles_x; x++) {
                icetAddTile(x*(IceTInt)SCREEN_WIDTH,
                            y*(IceTInt)SCREEN_HEIGHT,
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

    /* Set up the projection matrix. */
    icetMatrixFrustum(-0.5*aspect, 0.5*aspect, -0.5, 0.5, 1.0, 3.0,
                      projection_matrix);

    if (rank%8 != 0) {
        g_color[0] = (float)(rank%2);
        g_color[1] = (float)((rank/2)%2);
        g_color[2] = (float)((rank/4)%2);
        g_color[3] = 1.0f;
    } else {
        g_color[0] = g_color[1] = g_color[2] = 0.5f;
        g_color[3] = 1.0f;
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
        IceTDouble elapsed_time;
        IceTDouble modelview_matrix[16];
        IceTImage image;

        /* Get everyone to start at the same time. */
        icetCommBarrier();

        elapsed_time = icetWallTime();

        /* We can set up a modelview matrix here and IceT will factor this in
         * determining the screen projection of the geometry. */
        icetMatrixIdentity(modelview_matrix);

        /* Move geometry back so that it can be seen by the camera. */
        icetMatrixMultiplyTranslate(modelview_matrix, 0.0, 0.0, -2.0);

        /* Rotate to some random view. */
        icetMatrixMultiplyRotate(modelview_matrix,
                                 (360.0*rand())/RAND_MAX, 1.0, 0.0, 0.0);
        icetMatrixMultiplyRotate(modelview_matrix,
                                 (360.0*rand())/RAND_MAX, 0.0, 1.0, 0.0);
        icetMatrixMultiplyRotate(modelview_matrix,
                                 (360.0*rand())/RAND_MAX, 0.0, 0.0, 1.0);

        /* Determine view ordering of geometry based on camera position
           (represented by the current projection and modelview matrices). */
        if (g_transparent) {
            find_composite_order(projection_matrix,
                                 modelview_matrix,
                                 region_divisions);
        }

        /* Translate the unit box centered on the origin to the region specified
         * by bounds_min and bounds_max. */
        icetMatrixMultiplyTranslate(modelview_matrix,
                                    bounds_min[0],
                                    bounds_min[1],
                                    bounds_min[2]);
        icetMatrixMultiplyScale(modelview_matrix,
                                bounds_max[0] - bounds_min[0],
                                bounds_max[1] - bounds_min[1],
                                bounds_max[2] - bounds_min[2]);
        icetMatrixMultiplyTranslate(modelview_matrix, 0.5, 0.5, 0.5);

      /* Instead of calling draw() directly, call it indirectly through
       * icetDrawFrame().  IceT will automatically handle image
       * compositing. */
        image = icetDrawFrame(projection_matrix,
                              modelview_matrix,
                              background_color);

        /* Let everyone catch up before finishing the frame. */
        icetCommBarrier();

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

        /* Write out image to verify rendering occurred correctly. */
        if (   g_write_image
            && (rank < (g_num_tiles_x*g_num_tiles_y))
            && (frame == 0)
               ) {
            IceTUByte *buffer = malloc(SCREEN_WIDTH*SCREEN_HEIGHT*4);
            char filename[256];
            icetImageCopyColorub(image, buffer, ICET_IMAGE_COLOR_RGBA_UBYTE);
            sprintf(filename, "SimpleTiming%02d.ppm", rank);
            write_ppm(filename, buffer, (int)SCREEN_WIDTH, (int)SCREEN_HEIGHT);
            free(buffer);
        }
    }

    return TEST_PASSED;
}

int SimpleTiming(int argc, char * argv[])
{
    parse_arguments(argc, argv);

    return run_test(SimpleTimingRun);
}
