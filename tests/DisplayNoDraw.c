/* -*- c -*- *****************************************************************
** Copyright (C) 2003 Sandia Corporation
** Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
** license for use of this work by or on behalf of the U.S. Government.
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that this Notice and any statement
** of authorship are reproduced on all copies.
**
** This test has the display node not draw anything while everyone else
** does.  This should test some boundry conditions where the display node
** will receive images even when it doesn't generate any and for other
** nodes to send images without receiving any.
**
** In addition, this test will also place a blank tile in one of the other
** processors.  This should flag some problems with assumed far depths.
*****************************************************************************/

#include <IceTGL.h>
#include "test_codes.h"
#include "test-util.h"

#include <stdlib.h>
#include <stdio.h>

static int iteration;
static int global_rank;
static int result;

static void draw(void)
{
    printf("In draw\n");
    if (global_rank == 0) {
        printf("ERROR: Draw called on rank 0!\n");
        result = TEST_FAILED;
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (global_rank != iteration) {
        glBegin(GL_QUADS);
          glVertex3f(-1.0, -1.0, 0.0);
          glVertex3f(1.0, -1.0, 0.0);
          glVertex3f(1.0, 1.0, 0.0);
          glVertex3f(-1.0, 1.0, 0.0);
        glEnd();
    }
    printf("Leaving draw\n");
}

static int DisplayNoDrawRun()
{
    result = TEST_PASSED;
    int i;
    IceTInt rank, num_proc;

    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);

    printf("Starting DisplayNoDraw.\n");

    global_rank = rank;

    printf("Setting tile.");
    icetResetTiles();
    icetAddTile(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);

    icetDrawFunc(draw);

    if (rank == 0) {
        icetBoundingBoxf(100.0, 101.0, 100.0, 101.0, 100.0, 101.0);
    } else {
        icetBoundingBoxf(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-0.5, 0.5, -0.5, 0.5, -0.5, 0.5);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glColor4f(1.0, 1.0, 1.0, 1.0);

    for (i = 0; i < STRATEGY_LIST_SIZE; i++) {
        IceTUByte *color_buffer;

        icetStrategy(strategy_list[i]);
        printf("\n\nUsing %s strategy.\n", icetGetStrategyName());

        for (iteration = 0; iteration < num_proc; iteration++) {
            printf("Blank image is rank %d\n", iteration);

            icetDrawFrame();
            swap_buffers();

            if (   (rank == 0)
                && (num_proc > 1)
               /* This last case covers when there is only 2 processes,
                * the root, as always, is not drawing anything and the
                * other process is drawing the clear screen. */
                && ((num_proc > 2) || (iteration != 1)) ) {
                int p;
                int bad_count = 0;
                printf("Checking pixels.\n");
                color_buffer = icetGetColorBuffer();
                for (p = 0; p < SCREEN_WIDTH*SCREEN_HEIGHT*4; p++) {
                    if (color_buffer[p] != 255) {
                        char filename[256];
                        printf("BAD PIXEL %d.%d\n", p/4, p%4);
                        printf("    Expected 255, got %d\n", color_buffer[p]);
                        bad_count++;
                        if (bad_count >= 10) {
                            result = TEST_FAILED;
                            sprintf(filename, "DisplayNoDraw_%s_%d.ppm",
                                    icetGetStrategyName(), iteration);
                            write_ppm(filename, color_buffer,
                                      SCREEN_WIDTH, SCREEN_HEIGHT);
                            break;
                        }
                    }
                }
            }
        }
    }

    return result;
}

int DisplayNoDraw(int argc, char *argv[])
{
    /* To remove warning */
    (void)argc;
    (void)argv;

    return run_test(DisplayNoDrawRun);
}
