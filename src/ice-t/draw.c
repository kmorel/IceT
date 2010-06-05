/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#include <IceT.h>

/* TODO: Move all OpenGL dependent operations into its own OpenGL layer. */
#include <IceTGL.h>

#include <state.h>
#include <context.h>
#include <diagnostics.h>
#include <image.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MI(r,c) ((c)*4+(r))

#ifdef _MSC_VER
#pragma warning(disable:4054)
#pragma warning(disable:4055)
#endif

static void inflateBuffer(IceTUByte *buffer,
                          IceTSizeType width, IceTSizeType height);

static void multMatrix(IceTDouble *C, const IceTDouble *A, const IceTDouble *B);

static IceTUByte *display_buffer = NULL;
static IceTSizeType display_buffer_size = 0;

void icetDrawFunc(IceTCallback func)
{
    icetStateSetPointer(ICET_DRAW_FUNCTION, (IceTVoid *)func);
}

void icetStrategy(IceTStrategy strategy)
{
    icetStateSetPointer(ICET_STRATEGY_NAME, strategy.name);
    icetStateSetBoolean(ICET_STRATEGY_SUPPORTS_ORDERING,
                        strategy.supports_ordering);
    icetStateSetPointer(ICET_STRATEGY_COMPOSE, (IceTVoid *)strategy.compose);
}

const IceTUByte *icetGetStrategyName(void)
{
    IceTVoid *name;
    if (icetStateType(ICET_STRATEGY_NAME) == ICET_NULL) {
        return NULL;
    } else {
        icetGetPointerv(ICET_STRATEGY_NAME, &name);
    }
    return name;
}

void icetCompositeOrder(const IceTInt *process_ranks)
{
    IceTInt num_proc;
    IceTInt i;
    IceTInt *process_orders;
    IceTBoolean new_process_orders;

    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);
    if (   (icetStateGetType(ICET_PROCESS_ORDERS) == ICET_INT)
        && (icetStateGetSize(ICET_PROCESS_ORDERS) >= num_proc) ) {
        process_orders = icetUnsafeStateGetInteger(ICET_PROCESS_ORDERS);
        new_process_orders = 0;
    } else {
        process_orders = malloc(ICET_PROCESS_ORDERS * sizeof(IceTInt));
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
                           ICET_INT, process_orders);
    }
}

void icetDataReplicationGroup(IceTInt size, const IceTInt *processes)
{
    IceTInt rank;
    IceTBoolean found_myself = ICET_FALSE;
    IceTInt i;

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

void icetDataReplicationGroupColor(IceTInt color)
{
    IceTInt *allcolors;
    IceTInt *mygroup;
    IceTInt num_proc;
    IceTInt i;
    IceTInt size;

    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);
    icetResizeBuffer(2*sizeof(IceTInt)*num_proc);
    allcolors = icetReserveBufferMem(sizeof(IceTInt)*num_proc);
    mygroup = icetReserveBufferMem(sizeof(IceTInt)*num_proc);

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

static void find_contained_viewport(const IceTDouble projection_matrix[16],
                                    const IceTInt global_viewport[4],
                                    IceTInt contained_viewport[4],
                                    IceTDouble *znear, IceTDouble *zfar)
{
    IceTDouble *bound_vert;
    IceTDouble modelview_matrix[16];
    IceTDouble viewport_matrix[16];
    IceTDouble tmp_matrix[16];
    IceTDouble total_transform[16];
    IceTDouble left, right, bottom, top;
    IceTDouble *transformed_verts;
    IceTInt num_bounding_verts;
    int i;

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
    *znear = 1.0;
    *zfar  = -1.0;

  /* Transform each vertex to find where it lies in the global viewport and
     normalized z.  Leave the results in homogeneous coordinates for now. */
    bound_vert = icetUnsafeStateGetDouble(ICET_GEOMETRY_BOUNDS);
    icetGetIntegerv(ICET_NUM_BOUNDING_VERTS, &num_bounding_verts);
    transformed_verts
        = icetReserveBufferMem(sizeof(IceTDouble)*num_bounding_verts*4);
    for (i = 0; i < num_bounding_verts; i++) {
        transformed_verts[4*i + 0]
            = (  total_transform[MI(0,0)]*bound_vert[3*i+0]
               + total_transform[MI(0,1)]*bound_vert[3*i+1]
               + total_transform[MI(0,2)]*bound_vert[3*i+2]
               + total_transform[MI(0,3)] );
        transformed_verts[4*i + 1]
            = (  total_transform[MI(1,0)]*bound_vert[3*i+0]
               + total_transform[MI(1,1)]*bound_vert[3*i+1]
               + total_transform[MI(1,2)]*bound_vert[3*i+2]
               + total_transform[MI(1,3)] );
        transformed_verts[4*i + 2]
            = (  total_transform[MI(2,0)]*bound_vert[3*i+0]
               + total_transform[MI(2,1)]*bound_vert[3*i+1]
               + total_transform[MI(2,2)]*bound_vert[3*i+2]
               + total_transform[MI(2,3)] );
        transformed_verts[4*i + 3]
            = (  total_transform[MI(3,0)]*bound_vert[3*i+0]
               + total_transform[MI(3,1)]*bound_vert[3*i+1]
               + total_transform[MI(3,2)]*bound_vert[3*i+2]
               + total_transform[MI(3,3)] );
    }

  /* Now iterate over all the transformed verts and adjust the absolute mins
     and maxs to include them all. */
    for (i = 0; i < num_bounding_verts; i++)
    {
        IceTDouble *vert = transformed_verts + 4*i;

      /* Check to see if the vertex is in front of the near cut plane.  This
         is true when z/w >= -1 or z + w >= 0.  The second form is better
         just in case w is 0. */
        if (vert[2] + vert[3] >= 0.0) {
          /* Normalize homogeneous coordinates. */
            IceTDouble invw = 1.0/vert[3];
            IceTDouble x = vert[0]*invw;
            IceTDouble y = vert[1]*invw;
            IceTDouble z = vert[2]*invw;

          /* Update contained region. */
            if (left   > x) left   = x;
            if (right  < x) right  = x;
            if (bottom > y) bottom = y;
            if (top    < y) top    = y;
            if (*znear > z) *znear = z;
            if (*zfar  < z) *zfar  = z;
        } else {
          /* The vertex is being clipped by the near plane.  In perspective
             mode, vertices behind the near clipping plane can sometimes give
             missleading projections.  Instead, find all the other vertices on
             the other side of the near plane, compute the intersection of the
             segment between the two points and the near plane (in homogeneous
             coordinates) and use that as the projection. */
            int j;
            for (j = 0; j < num_bounding_verts; j++) {
                IceTDouble *vert2 = transformed_verts + 4*j;
                double t;
                IceTDouble x, y, invw;
                if (vert2[2] + vert2[3] < 0.0) {
                  /* Ignore other points behind near plane. */
                    continue;
                }
              /* Let the two points in question be v_i and v_j.  Define the
                 segment between them with the parametric equation
                 p(t) = (vert - vert2)t + vert2.  First, find t where the z and
                 w coordinates of p(t) sum to zero. */
                t = (vert2[2]+vert2[3])/(vert2[2]-vert[2] + vert2[3]-vert[3]);
              /* Use t to find the intersection point.  While we are at it,
                 normalize the resulting coordinates.  We don't need z because
                 we know it is going to be -1. */
                invw = 1.0/((vert[3] - vert2[3])*t + vert2[3] );
                x = ((vert[0] - vert2[0])*t + vert2[0] ) * invw;
                y = ((vert[1] - vert2[1])*t + vert2[1] ) * invw;

              /* Update contained region. */
                if (left   > x) left   = x;
                if (right  < x) right  = x;
                if (bottom > y) bottom = y;
                if (top    < y) top    = y;
                *znear = -1.0;
            }
        }
    }

    left = floor(left);
    right = ceil(right);
    bottom = floor(bottom);
    top = ceil(top);

  /* Clip bounds to global viewport. */
    if (left   < global_viewport[0]) left = global_viewport[0];
    if (right  > global_viewport[0] + global_viewport[2])
        right  = global_viewport[0] + global_viewport[2];
    if (bottom < global_viewport[1]) bottom = global_viewport[1];
    if (top    > global_viewport[1] + global_viewport[3])
        top    = global_viewport[1] + global_viewport[3];
    if (*znear < -1.0) *znear = -1.0;
    if (*zfar  >  1.0) *zfar = 1.0;

  /* Use this information to build a containing viewport. */
    contained_viewport[0] = (int)left;
    contained_viewport[1] = (int)bottom;
    contained_viewport[2] = (int)(right - left);
    contained_viewport[3] = (int)(top - bottom);
}

static void determine_contained_tiles(const IceTInt contained_viewport[4],
                                      IceTDouble znear, IceTDouble zfar,
                                      const IceTInt *tile_viewports,
                                      IceTInt num_tiles,
                                      IceTInt *contained_list,
                                      IceTBoolean *contained_mask,
                                      IceTInt *num_contained)
{
    int i;
    *num_contained = 0;
    memset(contained_mask, 0, sizeof(IceTBoolean)*num_tiles);
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

static IceTFloat black[] = {0.0, 0.0, 0.0, 0.0};

IceTImage icetDrawFrame(void)
{
    IceTInt rank, num_proc;
    IceTBoolean isDrawing;
    IceTInt frame_count;
    IceTDouble projection_matrix[16];
    IceTInt global_viewport[4];
    IceTInt contained_viewport[4];
    IceTDouble znear, zfar;
    IceTStrategy strategy;
    IceTVoid *value;
    IceTInt num_tiles;
    IceTInt num_bounding_verts;
    IceTInt *tile_viewports;
    IceTInt *contained_list;
    IceTInt num_contained;
    IceTBoolean *contained_mask;
    IceTBoolean *all_contained_masks;
    IceTInt *data_replication_group;
    IceTInt data_replication_group_size;
    IceTInt *contrib_counts;
    IceTInt total_image_count;
    IceTImage image;
    IceTInt display_tile;
    IceTInt *display_nodes;
    IceTInt color_format;
    IceTDouble render_time;
    IceTDouble buf_read_time;
    IceTDouble buf_write_time;
    IceTDouble compose_time;
    IceTDouble total_time;
    IceTFloat background_color[4];
    IceTUInt background_color_word;
    IceTBoolean color_blending;
    GLint physical_viewport[4];
    int i, j;

    icetRaiseDebug("In icetDrawFrame");

    icetGetBooleanv(ICET_IS_DRAWING_FRAME, &isDrawing);
    if (isDrawing) {
        icetRaiseError("Recursive frame draw detected.",ICET_INVALID_OPERATION);
        return icetImageInitialize(NULL, ICET_IMAGE_COLOR_NONE,
                                   ICET_IMAGE_DEPTH_NONE, 0);
    }

    icetGetIntegerv(ICET_GL_COLOR_FORMAT, &color_format);

    icetStateResetTiming();
    total_time = icetWallTime();

    color_blending =
        (IceTBoolean)(   *(icetUnsafeStateGetInteger(ICET_INPUT_BUFFERS))
                      == ICET_COLOR_BUFFER_BIT);

  /* Update physical render size to actual OpenGL viewport. */
    glGetIntegerv(GL_VIEWPORT, physical_viewport);
    icetPhysicalRenderSize(physical_viewport[2], physical_viewport[3]);

  /* Make sure background color is up to date. */
    glGetFloatv(GL_COLOR_CLEAR_VALUE, background_color);
    switch (color_format) {
      case GL_RGBA:
      default:
          ((IceTUByte *)&background_color_word)[0]
              = (IceTUByte)(255*background_color[0]);
          ((IceTUByte *)&background_color_word)[1]
              = (IceTUByte)(255*background_color[1]);
          ((IceTUByte *)&background_color_word)[2]
              = (IceTUByte)(255*background_color[2]);
          ((IceTUByte *)&background_color_word)[3]
              = (IceTUByte)(255*background_color[3]);
          break;
#if defined(GL_BGRA)
      case GL_BGRA:
#elif defined(GL_BGRA_EXT)
      case GL_BGRA_EXT:
#endif
          ((IceTUByte *)&background_color_word)[0]
              = (IceTUByte)(255*background_color[2]);
          ((IceTUByte *)&background_color_word)[1]
              = (IceTUByte)(255*background_color[1]);
          ((IceTUByte *)&background_color_word)[2]
              = (IceTUByte)(255*background_color[0]);
          ((IceTUByte *)&background_color_word)[3]
              = (IceTUByte)(255*background_color[3]);
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

    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);
    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);
    icetGetIntegerv(ICET_NUM_BOUNDING_VERTS, &num_bounding_verts);
    icetResizeBuffer(  sizeof(IceTInt)*num_tiles
                     + sizeof(IceTBoolean)*num_tiles*(num_proc+1)
                     + sizeof(int)    /* So stuff can land on byte boundries.*/
                     + sizeof(IceTInt)*num_tiles*num_proc
                     + sizeof(IceTInt)*num_proc
                     + sizeof(IceTDouble)*num_bounding_verts*4 );
    contained_list = icetReserveBufferMem(sizeof(IceTInt) * num_tiles);
    contained_mask = icetReserveBufferMem(sizeof(IceTBoolean)*num_tiles);

    icetGetIntegerv(ICET_GLOBAL_VIEWPORT, global_viewport);
    tile_viewports = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS);

    icetGetIntegerv(ICET_TILE_DISPLAYED, &display_tile);
    display_nodes = icetUnsafeStateGetInteger(ICET_DISPLAY_NODES);

  /* Get the current projection matrix. */
    icetRaiseDebug("Getting projection matrix.");
    glGetDoublev(GL_PROJECTION_MATRIX, projection_matrix);
    icetStateSetDoublev(ICET_PROJECTION_MATRIX, 16, projection_matrix);

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
      /* Figure out how the geometry projects onto the display. */
        find_contained_viewport(projection_matrix, global_viewport,
                                contained_viewport, &znear, &zfar);

      /* Now use this information to figure out which tiles need to be
         drawn. */
        determine_contained_tiles(contained_viewport, znear, zfar,
                                  tile_viewports, num_tiles,
                                  contained_list, contained_mask,
                                  &num_contained);
    }

    icetRaiseDebug4("contained_viewport = %d %d %d %d",
                    (int)contained_viewport[0], (int)contained_viewport[1],
                    (int)contained_viewport[2], (int)contained_viewport[3]);

  /* If we are doing data replication, reduced the amount of screen space
     we are responsible for. */
    icetGetIntegerv(ICET_DATA_REPLICATION_GROUP_SIZE,
                    &data_replication_group_size);
    if (data_replication_group_size > 1) {
        data_replication_group = icetReserveBufferMem(sizeof(IceTInt)*num_proc);
        icetGetIntegerv(ICET_DATA_REPLICATION_GROUP, data_replication_group);
        if (data_replication_group_size >= num_contained) {
          /* We have at least as many processes in the group as tiles we
             are projecting to.  First check to see if anybody in the group
             is displaying one of the tiles. */
            int tile_rendering = -1;
            int num_rendering_tile = 0;
            int tile_allocation_num = -1;
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
                IceTInt *tv = tile_viewports + 4*tile_rendering;
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
                contained_mask[i] = (IceTBoolean)(i == tile_rendering);
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
                data_replication_group_size /= factor;  /* New subgroup. */
                i /= data_replication_group_size;       /* i = piece I'm in. */
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
                    (int)contained_viewport[0], (int)contained_viewport[1],
                    (int)contained_viewport[2], (int)contained_viewport[3]);
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
        = icetReserveBufferMem(sizeof(IceTInt)*num_tiles*num_proc);
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
        return icetImageInitialize(NULL, ICET_IMAGE_COLOR_NONE,
                                   ICET_IMAGE_DEPTH_NONE, 0);
    }
    icetRaiseDebug("Calling strategy.compose");
    icetGetPointerv(ICET_STRATEGY_COMPOSE, &value);
    if (value == NULL) {
        icetRaiseError("Strategy not set.", ICET_INVALID_OPERATION);
        return icetImageInitialize(NULL, ICET_IMAGE_COLOR_NONE,
                                   ICET_IMAGE_DEPTH_NONE, 0);
    }
    strategy.compose = (IceTImage (*)(void))value;
    icetStateSetBoolean(ICET_IS_DRAWING_FRAME, 1);
    image = (*strategy.compose)();

  /* Correct background color where applicable. */
    glClearColor(background_color[0], background_color[1],
                 background_color[2], background_color[3]);
    if (   color_blending && (display_tile >= 0) && (background_color_word != 0)
        && icetIsEnabled(ICET_CORRECT_COLORED_BACKGROUND) ) {
        IceTSizeType pixels = icetImageGetSize(image);
        IceTEnum color_format = icetImageGetColorFormat(image);
        IceTDouble blend_time;
        icetGetDoublev(ICET_BLEND_TIME, &blend_time);
        blend_time = icetWallTime() - blend_time;
        if (color_format == ICET_IMAGE_COLOR_RGBA_UBYTE) {
            IceTUByte *color = icetImageGetColorUByte(image);
            IceTUByte *bc = (IceTUByte *)(&background_color_word);
            IceTSizeType p;
            for (p = 0; p < pixels; p++, color += 4) {
                ICET_UNDER_UBYTE(bc, color);
            }
        } else if (color_format == ICET_IMAGE_COLOR_RGBA_FLOAT) {
            IceTFloat *color = icetImageGetColorFloat(image);
            IceTSizeType p;
            for (p = 0; p < pixels; p++, color += 4) {
                ICET_UNDER_FLOAT(background_color, color);
            }
        } else {
            icetRaiseError("Encountered invalid color buffer type"
                           " with color blending.", ICET_SANITY_CHECK_FAIL);
        }
        blend_time = icetWallTime() - blend_time;
        icetStateSetDouble(ICET_BLEND_TIME, blend_time);
    }

    buf_write_time = icetWallTime();
    if (display_tile >= 0) {
        IceTEnum output_buffers;

        icetGetIntegerv(ICET_OUTPUT_BUFFERS, (IceTInt *)&output_buffers);

        if (   ((output_buffers & ICET_COLOR_BUFFER_BIT) != 0)
            && icetIsEnabled(ICET_DISPLAY) ) {
            IceTUByte *colorBuffer;
            IceTInt readBuffer;

            icetRaiseDebug("Displaying image.");

            icetGetIntegerv(ICET_GL_READ_BUFFER, &readBuffer);
            glDrawBuffer(readBuffer);

          /* Place raster position in lower left corner. */
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();
            glRasterPos2f(-1, -1);
            glPopMatrix();

          /* This could be made more efficient by natively handling all the
             image formats.  Don't forget to free memory later if necessary. */
            if (icetImageGetColorFormat(image) == ICET_IMAGE_COLOR_RGBA_UBYTE) {
                colorBuffer = icetImageGetColorUByte(image);
            } else {
                colorBuffer = (IceTUByte *)malloc(4*icetImageGetSize(image));
                icetImageCopyColorUByte(image, colorBuffer,
                                        ICET_IMAGE_COLOR_RGBA_UBYTE);
            }

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

          /* Delete the color buffer if we had to create our own. */
            if (icetImageGetColorFormat(image) != ICET_IMAGE_COLOR_RGBA_UBYTE) {
                free(colorBuffer);
            }
        }
    }

  /* Restore projection matrix. */
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(projection_matrix);
    glMatrixMode(GL_MODELVIEW);
    icetStateSetBoolean(ICET_IS_DRAWING_FRAME, 0);

    icetRaiseDebug("Calculating times.");
    buf_write_time = icetWallTime() - buf_write_time;
    icetStateSetDouble(ICET_BUFFER_WRITE_TIME, buf_write_time);

    icetGetDoublev(ICET_RENDER_TIME, &render_time);
    icetGetDoublev(ICET_BUFFER_READ_TIME, &buf_read_time);

    total_time = icetWallTime() - total_time;
    icetStateSetDouble(ICET_TOTAL_DRAW_TIME, total_time);

    compose_time = total_time - render_time - buf_read_time - buf_write_time;
    icetStateSetDouble(ICET_COMPOSITE_TIME, compose_time);

    return image;
}

static void inflateBuffer(IceTUByte *buffer,
                          IceTSizeType width, IceTSizeType height)
{
    IceTInt display_width, display_height;
    IceTInt color_format;

    icetGetIntegerv(ICET_PHYSICAL_RENDER_WIDTH, &display_width);
    icetGetIntegerv(ICET_PHYSICAL_RENDER_HEIGHT, &display_height);

    icetGetIntegerv(ICET_GL_COLOR_FORMAT, &color_format);

    if ((display_width <= width) && (display_height <= height)) {
      /* No need to inflate image. */
        glDrawPixels(width, height, color_format, GL_UNSIGNED_BYTE, buffer);
    } else {
        IceTSizeType x, y;
        IceTSizeType x_div, y_div;
        IceTUByte *last_scanline;
        IceTInt target_width, target_height;
        int use_textures = icetIsEnabled(ICET_DISPLAY_INFLATE_WITH_HARDWARE);

      /* If using hardware, resize to the nearest greater power of two.
         Otherwise, resize to screen size. */
        if (use_textures) {
            for (target_width=1; target_width < width; target_width <<= 1);
            for (target_height=1; target_height < height; target_height <<= 1);
            if (target_width*target_height >= display_width*display_height) {
              /* Sizing up to a power of two takes more data than just
                 sizing up to the screen size. */
                use_textures = 0;
                target_width = display_width;
                target_height = display_height;
            }
        } else {
            target_width = display_width;
            target_height = display_height;
        }

      /* Make sure buffer is big enough. */
        if (display_buffer_size < target_width*target_height) {
            free(display_buffer);
            display_buffer_size = target_width*target_height;
            display_buffer = malloc(4*sizeof(IceTUByte)*display_buffer_size);
        }

      /* This is how we scale the image with integer arithmetic.
       * If a/b = r = a div b + (a mod b)/b then:
       *        c/r = c/(a div b + (a mod b)/b) = c*b/(b*(a div b) + a mod b)
       * In our case a/b is target_width/width and target_height/height.
       * x_div and y_div are the denominators in the equation above.
       */
        x_div = width*(target_width/width) + target_width%width;
        y_div = height*(target_height/height) + target_height%height;
        last_scanline = NULL;
        for (y = 0; y < target_height; y++) {
            IceTUByte *src_scanline;
            IceTUByte *dest_scanline;

            src_scanline = buffer + 4*width*((y*height)/y_div);
            dest_scanline = display_buffer + 4*target_width*y;

            if (src_scanline == last_scanline) {
              /* Repeating last scanline.  Just copy memory. */
                memcpy(dest_scanline,
                       (const IceTUByte *)(dest_scanline - 4*target_width),
                       4*target_width);
                continue;
            }

            for (x = 0; x < target_width; x++) {
                ((IceTUInt *)dest_scanline)[x] =
                    ((IceTUInt *)src_scanline)[(x*width)/x_div];
            }

            last_scanline = src_scanline;
        }

        if (use_textures) {
          /* Setup texture. */
            if (icet_current_context->display_inflate_texture == 0) {
                glGenTextures(1,
                              &(icet_current_context->display_inflate_texture));
            }
            glBindTexture(GL_TEXTURE_2D,
                          icet_current_context->display_inflate_texture);
            glEnable(GL_TEXTURE_2D);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, target_width, target_height,
                         0, color_format, GL_UNSIGNED_BYTE, display_buffer);

          /* Setup geometry. */
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

          /* Draw texture. */
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glBegin(GL_QUADS);
              glTexCoord2f(0, 0);  glVertex2f(-1, -1);
              glTexCoord2f(1, 0);  glVertex2f( 1, -1);
              glTexCoord2f(1, 1);  glVertex2f( 1,  1);
              glTexCoord2f(0, 1);  glVertex2f(-1,  1);
            glEnd();

          /* Clean up. */
            glPopMatrix();
        } else {
            glDrawPixels(target_width, target_height, color_format,
                         GL_UNSIGNED_BYTE, display_buffer);
        }
    }
}

static void multMatrix(IceTDouble *C, const IceTDouble *A, const IceTDouble *B)
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
