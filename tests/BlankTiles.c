/* -*- c -*- *****************************************************************
** Copyright (C) 2003 Sandia Corporation
** Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
** license for use of this work by or on behalf of the U.S. Government.
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that this Notice and any statement
** of authorship are reproduced on all copies.
**
** Tests to make sure blank tiles are correctly handled.
*****************************************************************************/

#include <GL/ice-t.h>
#include "test-util.h"
#include "test_codes.h"

#include <stdlib.h>
#include <stdio.h>

static void draw(void)
{
    printf("In draw\n");
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glBegin(GL_QUADS);
      glVertex3f(-0.5, -0.5, 0.0);
      glVertex3f(0.5, -0.5, 0.0);
      glVertex3f(0.5, 0.5, 0.0);
      glVertex3f(-0.5, 0.5, 0.0);
    glEnd();
    printf("Leaving draw\n");
}

static int BlankTilesRun()
{
    int i, j, x, y;
    GLubyte *cb;
    int result = TEST_PASSED;
    GLint rank, num_proc;

    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);

    glClearColor(0.0, 0.0, 0.0, 0.0);

    icetDrawFunc(draw);
    icetBoundingBoxf(-0.5, 0.5, -0.5, 0.5, -0.5, 0.5);

    for (i = 0; i < STRATEGY_LIST_SIZE; i++) {
        int tile_dim;

        icetStrategy(strategy_list[i]);
        printf("\n\nUsing %s strategy.\n", icetGetStrategyName());

        for (tile_dim = 1; tile_dim*tile_dim <= num_proc; tile_dim++) {
            printf("\nRunning on a %d x %d display.\n", tile_dim, tile_dim);
            icetResetTiles();
            for (y = 0; y < tile_dim; y++) {
                for (x = 0; x < tile_dim; x++) {
                    icetAddTile(x*SCREEN_WIDTH, y*SCREEN_HEIGHT,
                                SCREEN_WIDTH, SCREEN_HEIGHT, y*tile_dim + x);
                }
            }

            printf("Rendering frame.\n");
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(-1, tile_dim*2-1, -1, tile_dim*2-1, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            icetDrawFrame();
            swap_buffers();

            if (rank == 0) {
                printf("Rank == 0, tile should have stuff in it.\n");
            } else if (rank < tile_dim*tile_dim) {
                printf("Checking returned image.\n");
                cb = icetGetColorBuffer();
                for (j = 0; j < SCREEN_WIDTH*SCREEN_HEIGHT*4; j++) {
                    if (cb[j] != 0) {
                        printf("Found bad pixel!!!!!!!!\n");
                        result = TEST_FAILED;
                        break;
                    }
                }
            } else {
                printf("Not a display node.  Not testing image.\n");
            }
        }
    }

    return result;
}

int BlankTiles(int argc, char *argv[])
{
  /* To remove warning */
  (void)argc;
  (void)argv;

  return run_test(BlankTilesRun);
}
