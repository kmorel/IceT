/* -*- c -*- *****************************************************************
** Id
**
** Copyright (C) 2005 Sandia Corporation
** Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
** license for use of this work by or on behalf of the U.S. Government.
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that this Notice and any statement
** of authorship are reproduced on all copies.
**
** This tests a corner case in which the bounds of an object extens from
** the viewing frustum to behind the viewpoint in perspective mode.
*****************************************************************************/

#include <GL/ice-t.h>
#include "test-util.h"
#include "test_codes.h"
#include "glwin.h"

#include <stdlib.h>
#include <stdio.h>

static void draw(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBegin(GL_QUADS);
      glVertex3f(-1.0, -1.0, 0.0);
      glVertex3f(1.0, -1.0, 0.0);
      glVertex3f(1.0, 1.0, 0.0);
      glVertex3f(-1.0, 1.0, 0.0);
    glEnd();
}

static void PrintMatrix(float *mat)
{
    int r, c;

    for (c = 0; c < 4; c++) {
        for (r = 0; r < 4; r++) {
            printf("%f ", mat[4*r + c]);
        }
        printf("\n");
    }
}

int BoundsBehindViewer(int argc, char * argv[])
{
    float mat[16];

    /* To remove warning */
    (void)argc;
    (void)argv;

    GLint rank;
    icetGetIntegerv(ICET_RANK, &rank);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    icetDrawFunc(draw);
    icetStrategy(ICET_STRATEGY_REDUCE);

    icetBoundingBoxf(-1.0, 1.0, -1.0, 1.0, -0.0, 0.0);

  /* We're just going to use one tile. */
    icetResetTiles();
    icetAddTile(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);

  /* Set up the transformation such that the quad in draw should cover the
     entire screen, but part of it extends behind the viewpoint.  Furthermore, a
     naive division by w will show all points to the right of the screen (which,
     of course, is wrong). */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -1.5);
    glRotatef(10.0, 0.0, 1.0, 0.0);
    glScalef(10.0, 10.0, 10.0);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -1.0, 1.0, 1.0, 2.0);

    printf("Modelview matrix:\n");
    glGetFloatv(GL_MODELVIEW_MATRIX, mat);
    PrintMatrix(mat);
    printf("Projection matrix:\n");
    glGetFloatv(GL_PROJECTION_MATRIX, mat);
    PrintMatrix(mat);

  /* Other normal OpenGL setup. */
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glColor3f(1.0, 1.0, 1.0);

  /* All the processes have the same data.  Go ahead and tell IceT. */
    icetDataReplicationGroupColor(0);

    icetDrawFrame();

  /* Test the resulting image to make sure the polygon was drawn over it. */
    if (rank == 0) {
        GLuint *cb = (GLuint *)icetGetColorBuffer();
        if (cb[0] != 0xFFFFFFFF) {
            printf("First pixel in color buffer wrong: 0x%x\n", cb[0]);
            finalize_test(TEST_FAILED);
            return TEST_FAILED;
        }
    }

    finalize_test(TEST_PASSED);
    return TEST_PASSED;
}
