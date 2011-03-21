/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include <IceT.h>

#include <IceTDevCommunication.h>
#include <IceTDevDiagnostics.h>
#include <IceTDevImage.h>
#include <IceTDevMatrix.h>
#include <IceTDevState.h>
#include <IceTDevStrategySelect.h>
#include <IceTDevTiming.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _MSC_VER
#pragma warning(disable:4054)
#pragma warning(disable:4055)
#endif

void icetDrawCallback(IceTDrawCallbackType func)
{
    icetStateSetPointer(ICET_DRAW_FUNCTION, (IceTVoid *)func);
}

void icetStrategy(IceTEnum strategy)
{
    if (icetStrategyValid(strategy)) {
        icetStateSetInteger(ICET_STRATEGY, strategy);
        icetStateSetBoolean(ICET_STRATEGY_SUPPORTS_ORDERING,
                            icetStrategySupportsOrdering(strategy));
    } else {
        icetRaiseError("Invalid strategy.", ICET_INVALID_ENUM);
    }
}

const char *icetGetStrategyName(void)
{
    IceTEnum strategy;

    icetGetEnumv(ICET_STRATEGY, &strategy);
    if (strategy != ICET_STRATEGY_UNDEFINED) {
        return icetStrategyNameFromEnum(strategy);
    } else {
        icetRaiseError("No strategy set. Use icetStrategy to set the strategy.",
                       ICET_INVALID_ENUM);
        return NULL;
    }
}

void icetSingleImageStrategy(IceTEnum strategy)
{
    if (icetSingleImageStrategyValid(strategy)) {
        icetStateSetInteger(ICET_SINGLE_IMAGE_STRATEGY, strategy);
    } else {
        icetRaiseError("Invalid single image strategy.", ICET_INVALID_ENUM);
    }
}

const char *icetGetSingleImageStrategyName(void)
{
    IceTEnum strategy;

    icetGetEnumv(ICET_SINGLE_IMAGE_STRATEGY, &strategy);
    return icetSingleImageStrategyNameFromEnum(strategy);
}

void icetCompositeMode(IceTEnum mode)
{
    if (    (mode != ICET_COMPOSITE_MODE_Z_BUFFER)
         && (mode != ICET_COMPOSITE_MODE_BLEND) ) {
        icetRaiseError("Invalid composite mode.", ICET_INVALID_ENUM);
        return;
    }

    icetStateSetInteger(ICET_COMPOSITE_MODE, mode);
}

void icetCompositeOrder(const IceTInt *process_ranks)
{
    IceTInt num_proc;
    IceTInt i;
    IceTInt *process_orders;

    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);
    process_orders = icetStateAllocateInteger(ICET_PROCESS_ORDERS, num_proc);
    for (i = 0; i < num_proc; i++) {
        process_orders[i] = -1;
    }
    for (i = 0; i < num_proc; i++) {
        process_orders[process_ranks[i]] = i;
    }
    for (i = 0; i < num_proc; i++) {
        if (process_orders[i] == -1) {
            icetRaiseError("Invalid composite order.", ICET_INVALID_VALUE);
            return;
        }
    }
    icetStateSetIntegerv(ICET_COMPOSITE_ORDER, num_proc, process_ranks);
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
    allcolors = malloc(sizeof(IceTInt)*num_proc);
    mygroup = malloc(sizeof(IceTInt)*num_proc);

    icetCommAllgather(&color, 1, ICET_INT, allcolors);

    size = 0;
    for (i = 0; i < num_proc; i++) {
        if (allcolors[i] == color) {
            mygroup[size] = i;
            size++;
        }
    }

    icetDataReplicationGroup(size, mygroup);

    free(allcolors);
    free(mygroup);
}

static IceTFloat black[] = {0.0f, 0.0f, 0.0f, 0.0f};

static void drawUseBackgroundColor(const IceTFloat *background_color,
                                   IceTUInt *background_color_word_p,
                                   IceTBoolean *need_color_correction_p)
{
  IceTBoolean use_color_blending
    = (IceTBoolean)(   *(icetUnsafeStateGetInteger(ICET_COMPOSITE_MODE))
                    == ICET_COMPOSITE_MODE_BLEND);

  /* Make sure background color is up to date. */
    ((IceTUByte *)background_color_word_p)[0]
      = (IceTUByte)(255*background_color[0]);
    ((IceTUByte *)background_color_word_p)[1]
      = (IceTUByte)(255*background_color[1]);
    ((IceTUByte *)background_color_word_p)[2]
      = (IceTUByte)(255*background_color[2]);
    ((IceTUByte *)background_color_word_p)[3]
      = (IceTUByte)(255*background_color[3]);
    if (use_color_blending) {
        IceTInt display_tile;
      /* We need to correct the background color by zeroing it out at
       * blending it back at the end. */
        icetStateSetFloatv(ICET_BACKGROUND_COLOR, 4, black);
        icetStateSetInteger(ICET_BACKGROUND_COLOR_WORD, 0);

        icetGetIntegerv(ICET_TILE_DISPLAYED, &display_tile);
        if (   (display_tile >= 0)
            && (*background_color_word_p != 0)
            && icetIsEnabled(ICET_CORRECT_COLORED_BACKGROUND) ) {
            *need_color_correction_p = ICET_TRUE;
        } else {
            *need_color_correction_p = ICET_FALSE;
        }
    } else {
        icetStateSetFloatv(ICET_BACKGROUND_COLOR, 4, background_color);
        icetStateSetInteger(ICET_BACKGROUND_COLOR_WORD,
                            *background_color_word_p);
        *need_color_correction_p = ICET_FALSE;
    }
}

static void drawFindContainedViewport(IceTInt contained_viewport[4],
                                      IceTDouble *znear, IceTDouble *zfar)
{
    IceTDouble total_transform[16];
    IceTDouble left, right, bottom, top;
    IceTDouble *transformed_verts;
    IceTInt global_viewport[4];
    IceTInt num_bounding_verts;
    int i;

    icetGetIntegerv(ICET_GLOBAL_VIEWPORT, global_viewport);

    {
        IceTDouble projection_matrix[16];
        IceTDouble modelview_matrix[16];
        IceTDouble viewport_matrix[16];
        IceTDouble tmp_matrix[16];

        icetGetDoublev(ICET_PROJECTION_MATRIX, projection_matrix);
        icetGetDoublev(ICET_MODELVIEW_MATRIX, modelview_matrix);

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

        icetMatrixMultiply(tmp_matrix,
                           (const IceTDouble *)projection_matrix,
                           (const IceTDouble *)modelview_matrix);
        icetMatrixMultiply(total_transform,
                           (const IceTDouble *)viewport_matrix,
                           (const IceTDouble *)tmp_matrix);
    }

    icetGetIntegerv(ICET_NUM_BOUNDING_VERTS, &num_bounding_verts);
    transformed_verts = icetGetStateBuffer(
                                       ICET_TRANSFORMED_BOUNDS,
                                       sizeof(IceTDouble)*num_bounding_verts*4);

    /* Transform each vertex to find where it lies in the global viewport and
       normalized z.  Leave the results in homogeneous coordinates for now. */
    {
        const IceTDouble *bound_vert
            = icetUnsafeStateGetDouble(ICET_GEOMETRY_BOUNDS);
        for (i = 0; i < num_bounding_verts; i++) {
            IceTDouble bound_vert_4vec[4];
            bound_vert_4vec[0] = bound_vert[3*i+0];
            bound_vert_4vec[1] = bound_vert[3*i+1];
            bound_vert_4vec[2] = bound_vert[3*i+2];
            bound_vert_4vec[3] = 1.0;
            icetMatrixVectorMultiply(transformed_verts + 4*i,
                                     (const IceTDouble *)total_transform,
                                     (const IceTDouble *)bound_vert_4vec);
        }
    }

    /* Set absolute mins and maxes. */
    left   = global_viewport[0] + global_viewport[2];
    right  = global_viewport[0];
    bottom = global_viewport[1] + global_viewport[3];
    top    = global_viewport[1];
    *znear = 1.0;
    *zfar  = -1.0;

    /* Now iterate over all the transformed verts and adjust the absolute mins
       and maxs to include them all. */
    for (i = 0; i < num_bounding_verts; i++)
    {
        IceTDouble *vert = transformed_verts + 4*i;

        /* Check to see if the vertex is in front of the near cut plane.  This
           is true when z/w >= -1 or z + w >= 0.  The second form is better just
           in case w is 0. */
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
             misleading projections.  Instead, find all the other vertices on
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
    contained_viewport[0] = (IceTInt)left;
    contained_viewport[1] = (IceTInt)bottom;
    contained_viewport[2] = (IceTInt)(right - left);
    contained_viewport[3] = (IceTInt)(top - bottom);
}

static void drawDetermineContainedTiles(const IceTInt contained_viewport[4],
                                        IceTDouble znear, IceTDouble zfar,
                                        IceTInt *contained_list,
                                        IceTBoolean *contained_mask,
                                        IceTInt *num_contained_p)
{
    const IceTInt *tile_viewports;
    IceTInt num_tiles;
    int i;

    tile_viewports = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS);
    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);

    *num_contained_p = 0;
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
            contained_list[*num_contained_p] = i;
            contained_mask[i] = 1;
            (*num_contained_p)++;
        }
    }
}

static void drawAdjustContainedForDataReplication(IceTInt *contained_viewport,
                                                  IceTInt *contained_list,
                                                  IceTBoolean *contained_mask,
                                                  IceTInt *num_contained_p)
{
    IceTInt *data_replication_group;
    IceTInt data_replication_group_size;
    const IceTInt *display_nodes;
    IceTInt rank;
    IceTInt num_proc;

    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);

    display_nodes = icetUnsafeStateGetInteger(ICET_DISPLAY_NODES);

    /* If we are doing data replication, reduced the amount of screen space
       we are responsible for. */
    icetGetIntegerv(ICET_DATA_REPLICATION_GROUP_SIZE,
                    &data_replication_group_size);
    if (data_replication_group_size > 1) {
        /* Need to copy data_replication_group to temporary buffer so that we
           can modify it based on the current view's projection. */
        data_replication_group = icetGetStateBuffer(ICET_DATA_REP_GROUP_BUF,
                                                    sizeof(IceTInt)*num_proc);
        icetGetIntegerv(ICET_DATA_REPLICATION_GROUP, data_replication_group);
        if (data_replication_group_size >= *num_contained_p) {
            /* We have at least as many processes in the group as tiles we
               are projecting to.  First check to see if anybody in the group
               is displaying one of the tiles. */
            int tile_rendering = -1;
            int num_rendering_tile = 0;
            int tile_allocation_num = -1;
            int tile_id;

            for (tile_id = 0; tile_id < *num_contained_p; tile_id++) {
                int tile = contained_list[tile_id];
                int group_display_rank
                    = icetFindRankInGroup(data_replication_group,
                                          data_replication_group_size,
                                          display_nodes[tile]);
                if (group_display_rank >= 0) {
                    if (data_replication_group[group_display_rank] == rank) {
                        /* I'm displaying this tile, let's render it. */
                        tile_rendering = tile;
                        num_rendering_tile = 1;
                        tile_allocation_num = 0;
                    }
                    /* Remove both the tile and display node to prevent
                       pairing either with something else. */
                    (*num_contained_p)--;
                    contained_list[tile_id] = contained_list[*num_contained_p];
                    data_replication_group_size--;
                    data_replication_group[group_display_rank] =
                        data_replication_group[data_replication_group_size];
                    /* And decrement the tile counter so that we actually
                       check the tile we just moved. */
                    tile_id--;
                    break;
                }
            }

            /* Assign the rest of the processes to tiles. */
            if (*num_contained_p > 0) {
                int proc_per_tile = 0;
                int group_id;
                for (tile_id = 0, group_id = 0;
                     group_id < data_replication_group_size;
                     group_id++, tile_id++) {
                    if (tile_id >= *num_contained_p) {
                        tile_id = 0;
                        proc_per_tile++;
                    }
                    if (data_replication_group[group_id] == rank) {
                      /* Assign this process to the tile. */
                        tile_rendering = contained_list[tile_id];
                        tile_allocation_num = proc_per_tile;
                        num_rendering_tile = proc_per_tile+1;
                    } else if (tile_rendering == contained_list[tile_id]) {
                      /* This process already assigned to tile.  Mark that
                         another process is also assigned to it. */
                        num_rendering_tile++;
                    }
                }
            }

            /* Record a new viewport covering only my portion of the tile. */
            if (tile_rendering >= 0) {
                const IceTInt *tile_viewports
                    = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS);
                const IceTInt *tv = tile_viewports + 4*tile_rendering;
                int new_length = tv[2]/num_rendering_tile;
                *num_contained_p = 1;
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
                *num_contained_p = 0;
                contained_viewport[0] = -10000;
                contained_viewport[1] = -10000;
                contained_viewport[2] = 0;
                contained_viewport[3] = 0;
            }

            /* Fix contained_mask. */
            {
                int num_tiles;
                icetGetIntegerv(ICET_NUM_TILES, &num_tiles);
                for (tile_id = 0; tile_id < num_tiles; tile_id++) {
                    contained_mask[tile_id]
                        = (IceTBoolean)(tile_id == tile_rendering);
                }
            }
        } else {
            /* More tiles than processes.  Split up the contained_viewport as
               best as possible. */
            int factor = 2;
            while (factor <= data_replication_group_size) {
                int split_axis = contained_viewport[2] < contained_viewport[3];
                int new_length;
                int group_rank;
                int group_piece;
                while (data_replication_group_size%factor != 0) factor++;
              /* Split the viewport along the axis factor times.  Also
                 split the group into factor pieces. */
                new_length = contained_viewport[2+split_axis]/factor;
                group_rank = icetFindMyRankInGroup(data_replication_group,
                                                   data_replication_group_size);
                data_replication_group_size /= factor;  /* New subgroup. */
                group_piece = group_rank/data_replication_group_size;
                data_replication_group
                    += group_piece*data_replication_group_size;
                if (group_piece == factor-1) {
                  /* Make sure last piece does not drop pixels due to
                     rounding errors. */
                    contained_viewport[2+split_axis] -= group_piece*new_length;
                } else {
                    contained_viewport[2+split_axis] = new_length;
                }
                contained_viewport[split_axis] += group_piece*new_length;
            }
            drawDetermineContainedTiles(contained_viewport,
                                        0.0,    /* Fake znear and zfar. */
                                        0.0,    /* Already checked in range. */
                                        contained_list,
                                        contained_mask,
                                        num_contained_p);
        }
    }
}


static void drawProjectBounds(void)
{
    IceTInt num_bounding_verts;
    IceTInt *contained_list;
    IceTBoolean *contained_mask;
    IceTInt contained_viewport[4];
    IceTDouble znear, zfar;
    IceTInt num_tiles;
    IceTInt num_contained;

    icetGetIntegerv(ICET_NUM_BOUNDING_VERTS, &num_bounding_verts);
    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);

    contained_list = icetGetStateBuffer(ICET_CONTAINED_LIST_BUF,
                                        sizeof(IceTInt) * num_tiles);
    contained_mask = icetGetStateBuffer(ICET_CONTAINED_MASK_BUF,
                                        sizeof(IceTBoolean)*num_tiles);

    if (num_bounding_verts < 1) {
        /* User never set bounding vertices.  Assume image covers all tiles. */
        IceTInt i;
        for (i = 0; i < num_tiles; i++) {
            contained_list[i] = i;
            contained_mask[i] = 1;
        }
        icetGetIntegerv(ICET_GLOBAL_VIEWPORT, contained_viewport);
        znear = -1.0;
        zfar = 1.0;
        num_contained = num_tiles;
    } else {
      /* Figure out how the geometry projects onto the display. */
        drawFindContainedViewport(contained_viewport, &znear, &zfar);

      /* Now use this information to figure out which tiles need to be
         drawn. */
        drawDetermineContainedTiles(contained_viewport,
                                    znear,
                                    zfar,
                                    contained_list,
                                    contained_mask,
                                    &num_contained);
    }

    icetRaiseDebug4("contained_viewport = %d %d %d %d",
                    (int)contained_viewport[0], (int)contained_viewport[1],
                    (int)contained_viewport[2], (int)contained_viewport[3]);

    drawAdjustContainedForDataReplication(contained_viewport,
                                          contained_list,
                                          contained_mask,
                                          &num_contained);

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
}

static void drawCollectTileInformation(void)
{
    IceTBoolean *all_contained_masks;
    IceTInt num_proc;
    IceTInt num_tiles;

    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);
    icetGetIntegerv(ICET_NUM_TILES, &num_tiles);

    all_contained_masks
        = icetStateAllocateBoolean(ICET_ALL_CONTAINED_TILES_MASKS,
                                   num_tiles*num_proc);

    icetRaiseDebug("Gathering rendering information.");
    {
        const IceTBoolean *contained_mask;

        contained_mask = icetUnsafeStateGetBoolean(ICET_CONTAINED_TILES_MASK);

        icetCommAllgather(contained_mask, num_tiles, ICET_BYTE,
                          all_contained_masks);
    }

    {
        IceTInt *contrib_counts;
        IceTInt total_image_count;
        IceTInt tile_id;

        contrib_counts
            = icetStateAllocateInteger(ICET_TILE_CONTRIB_COUNTS, num_tiles);
        total_image_count = 0;
        for (tile_id = 0; tile_id < num_tiles; tile_id++) {
            IceTInt proc_id;
            contrib_counts[tile_id] = 0;
            for (proc_id = 0; proc_id < num_proc; proc_id++) {
                if (all_contained_masks[proc_id*num_tiles + tile_id]) {
                    contrib_counts[tile_id]++;
                }
            }
            total_image_count += contrib_counts[tile_id];
        }
        icetStateSetIntegerv(ICET_TOTAL_IMAGE_COUNT, 1, &total_image_count);
    }
}

static IceTImage drawInvokeStrategy(void)
{
    IceTImage image;
    IceTVoid *value;
    IceTEnum strategy;
    IceTInt display_tile;

    icetGetPointerv(ICET_DRAW_FUNCTION, &value);
    if (value == NULL) {
        icetRaiseError("Drawing function not set.  Call icetDrawCallback.",
                       ICET_INVALID_OPERATION);
        return icetImageNull();
    }

    icetRaiseDebug("Calling strategy");
    icetStateSetBoolean(ICET_IS_DRAWING_FRAME, 1);
    icetGetEnumv(ICET_STRATEGY, &strategy);
    image = icetInvokeStrategy(strategy);

    icetStateSetBoolean(ICET_IS_DRAWING_FRAME, 0);

    /* Ensure that the returned image is the expected size. */
    icetGetIntegerv(ICET_TILE_DISPLAYED, &display_tile);
    if (display_tile >= 0) {
        const IceTInt *display_tile_viewport
            = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS) + 4*display_tile;
        if (   (display_tile_viewport[2] != icetImageGetWidth(image))
            || (display_tile_viewport[3] != icetImageGetHeight(image)) ) {
            icetRaiseDebug4("Expected size: %d %d.  Returned size: %d %d",
                            display_tile_viewport[2], display_tile_viewport[3],
                            (int)icetImageGetWidth(image),
                            (int)icetImageGetHeight(image));
            icetRaiseError("Got unexpected image size from strategy.",
                           ICET_SANITY_CHECK_FAIL);
        }
    }

    icetStateCheckMemory();

    return image;
}

static void drawCorrectBackground(IceTImage image,
                                  const IceTFloat *background_color,
                                  IceTUInt background_color_word)
{
    IceTSizeType pixels = icetImageGetWidth(image)*icetImageGetHeight(image);
    IceTEnum color_format = icetImageGetColorFormat(image);

    icetTimingBlendBegin();

    if (color_format == ICET_IMAGE_COLOR_RGBA_UBYTE) {
        IceTUByte *color = icetImageGetColorub(image);
        IceTUByte *bc = (IceTUByte *)(&background_color_word);
        IceTSizeType p;
        for (p = 0; p < pixels; p++, color += 4) {
            ICET_UNDER_UBYTE(bc, color);
        }
    } else if (color_format == ICET_IMAGE_COLOR_RGBA_FLOAT) {
        IceTFloat *color = icetImageGetColorf(image);
        IceTSizeType p;
        for (p = 0; p < pixels; p++, color += 4) {
            ICET_UNDER_FLOAT(background_color, color);
        }
    } else {
        icetRaiseError("Encountered invalid color buffer type"
                       " with color blending.", ICET_SANITY_CHECK_FAIL);
    }

    icetTimingBlendEnd();
}

IceTImage icetDrawFrame(const IceTDouble *projection_matrix,
                        const IceTDouble *modelview_matrix,
                        const IceTFloat *background_color)
{
    IceTInt frame_count;
    IceTImage image;
    IceTDouble render_time;
    IceTDouble buf_read_time;
    IceTDouble compose_time;
    IceTDouble total_time;
    IceTUInt background_color_word;
    IceTBoolean need_color_correction;

    icetRaiseDebug("In icetDrawFrame");

    {
        IceTBoolean isDrawing;
        icetGetBooleanv(ICET_IS_DRAWING_FRAME, &isDrawing);
        if (isDrawing) {
            icetRaiseError("Recursive frame draw detected.",
                           ICET_INVALID_OPERATION);
            return icetImageNull();
        }
    }

    icetStateResetTiming();
    icetTimingDrawFrameBegin();

    icetStateSetDoublev(ICET_PROJECTION_MATRIX, 16, projection_matrix);
    icetStateSetDoublev(ICET_MODELVIEW_MATRIX, 16, modelview_matrix);

    drawUseBackgroundColor(background_color,
                           &background_color_word,
                           &need_color_correction);

    icetGetIntegerv(ICET_FRAME_COUNT, &frame_count);
    frame_count++;
    icetStateSetIntegerv(ICET_FRAME_COUNT, 1, &frame_count);

    drawProjectBounds();

    {
        IceTEnum strategy;
        icetGetEnumv(ICET_STRATEGY, &strategy);

        /* drawCollectTileInformation does an allgather to get information
         * about the tiles in other processes.  These variables are
         * ICET_ALL_CONTAINED_TILES_MASKS, ICET_TILE_CONTRIB_COUNTS, and
         * ICET_TOTAL_IMAGE_COUNT.  However, the sequential strategy ignores
         * this information and just uses all processes for all tiles.  When
         * compositing a single tile, this is a fine strategy and we can save
         * a significant proportion of frame time by skipping this step. */
        if (strategy != ICET_STRATEGY_SEQUENTIAL) {
            drawCollectTileInformation();
        }
    }

    {
        IceTInt tile_displayed;
        icetGetIntegerv(ICET_TILE_DISPLAYED, &tile_displayed);

        if (tile_displayed >= 0) {
            const IceTInt *tile_viewports
                = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS);
            IceTInt num_pixels = (  tile_viewports[4*tile_displayed+2]
                                  * tile_viewports[4*tile_displayed+3] );
            icetStateSetInteger(ICET_VALID_PIXELS_TILE, tile_displayed);
            icetStateSetInteger(ICET_VALID_PIXELS_OFFSET, 0);
            icetStateSetInteger(ICET_VALID_PIXELS_NUM, num_pixels);
        } else {
            icetStateSetInteger(ICET_VALID_PIXELS_TILE, -1);
            icetStateSetInteger(ICET_VALID_PIXELS_OFFSET, 0);
            icetStateSetInteger(ICET_VALID_PIXELS_NUM, 0);
        }
    }

    image = drawInvokeStrategy();

    /* Correct background color where applicable. */
    if (need_color_correction) {
        drawCorrectBackground(image, background_color, background_color_word);
    }

    /* Calculate times. */
    icetGetDoublev(ICET_RENDER_TIME, &render_time);
    icetGetDoublev(ICET_BUFFER_READ_TIME, &buf_read_time);

    icetTimingDrawFrameEnd();

    icetGetDoublev(ICET_TOTAL_DRAW_TIME, &total_time);

    compose_time = total_time - render_time - buf_read_time;
    icetStateSetDouble(ICET_COMPOSITE_TIME, compose_time);

    icetStateSetDouble(ICET_BUFFER_WRITE_TIME, 0.0);

    icetStateCheckMemory();

    return image;
}
