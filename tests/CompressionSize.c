/* -*- c -*- *****************************************************************
** Copyright (C) 2003 Sandia Corporation
** Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
** license for use of this work by or on behalf of the U.S. Government.
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that this Notice and any statement
** of authorship are reproduced on all copies.
**
** This test checks to make sure the size of compressed images never
** exceeds the advertised maximum buffer size.
*****************************************************************************/

#include <IceTGL.h>
#include "test_codes.h"
#include "test-util.h"

#include <image.h>
#include <state.h>

#include <stdlib.h>
#include <stdio.h>

static void draw(void)
{
    printf("In draw\n");
  /*I really don't care what Ice-T set the projection to.*/
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glColor4f(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBegin(GL_QUADS);
    glVertex3f(-1.0, -1.0, 0.0);
    glVertex3f(1.0, -1.0, 0.0);
    glVertex3f(1.0, 1.0, 0.0);
    glVertex3f(-1.0, 1.0, 0.0);
    glEnd();
}

static int DoCompressionTest(int num_buffers)
{
    GLint viewport[4];
    int pixels;
    IceTImage imagebuffer;
    IceTUByte *colorbuffer;
    IceTUInt *depthbuffer;
    IceTSparseImage compressedbuffer;
    IceTInt fardepth;
    int i;
    IceTUInt size;
    int result;
    IceTInt tmp;

    result = TEST_PASSED;

    icetGetIntegerv(ICET_INPUT_BUFFERS, &tmp);
    printf("Using input buffers of 0x%x\n", (int)tmp);

    glGetIntegerv(GL_VIEWPORT, viewport);
    pixels = viewport[2]*viewport[3];
    icetGetIntegerv(ICET_ABSOLUTE_FAR_DEPTH, &fardepth);

    printf("Allocating memory for %d pixel image.\n", pixels);
    imagebuffer = malloc(icetFullImageSize(pixels));
    compressedbuffer = malloc(icetSparseImageSize(pixels));
    icetInitializeImage(imagebuffer, pixels);
    colorbuffer = icetGetImageColorBuffer(imagebuffer);
    depthbuffer = icetGetImageDepthBuffer(imagebuffer);

    printf("\nCreating worst possible image.\n");
    for (i = 0; i < pixels; i += 2) {
        if (colorbuffer) {
            colorbuffer[4*i + 0] = 0xFF;
            colorbuffer[4*i + 1] = 0xFF;
            colorbuffer[4*i + 2] = 0xFF;
            colorbuffer[4*i + 3] = 0xFF;

            colorbuffer[4*(i+1) + 0] = 0x00;
            colorbuffer[4*(i+1) + 1] = 0x00;
            colorbuffer[4*(i+1) + 2] = 0x00;
            colorbuffer[4*(i+1) + 3] = 0x00;
        }
        if (depthbuffer) {
            depthbuffer[i] = 0x00000000;
            depthbuffer[i+1] = fardepth;
        }
    }
    printf("Compressing image.\n");
    size = icetCompressImage(imagebuffer, compressedbuffer);
    printf("Expected size: %d.  Actual size: %d\n",
           (int)icetSparseImageSize(pixels), (int)size);
    if (   (size > icetSparseImageSize(pixels))
        || (size < (IceTUInt)(2+2*num_buffers)*pixels) ) {
        printf("Size differs from expected size!\n");
        result = TEST_FAILED;
    }

    printf("\nCreating a different worst possible image.\n");
    for (i = 0; i < pixels; i++) {
        if (colorbuffer) {
            colorbuffer[4*i + 0] = 0xAA;
            colorbuffer[4*i + 1] = i%256;
            colorbuffer[4*i + 2] = 255 - i%256;
            colorbuffer[4*i + 3] = 0xAA;
        }
        if (depthbuffer) {
            depthbuffer[i] = 0x00000000;
        }
    }
    printf("Compressing image.\n");
    size = icetCompressImage(imagebuffer, compressedbuffer);
    printf("Expected size: %d.  Actual size: %d\n",
           (int)icetSparseImageSize(pixels), (int)size);
    if (   (size > icetSparseImageSize(pixels))
        || (size < (IceTUInt)4*num_buffers*pixels) ) {
        printf("Size differs from expected size!\n");
        result = TEST_FAILED;
    }

    printf("\nCompressing zero size image.\n");
    icetInitializeImage(imagebuffer, 0);
    size = icetCompressImage(imagebuffer, compressedbuffer);
    printf("Expected size: %d.  Actual size: %d\n",
           (int)icetSparseImageSize(0), (int)size);
    if (size > icetSparseImageSize(0)) {
        printf("Size differs from expected size!\n");
        result = TEST_FAILED;
    }

  /* This test can be a little volatile.  The icetGetCompressedTileImage
   * expects certain things to be set correctly by the icetDrawFrame
   * function.  Since we want to call icetGetCompressedTileImage directly,
   * we try to set up these parameters by hand.  It is possible for this
   * test to incorrectly fail if the two functions are mutually changed and
   * this scaffolding is not updated correctly. */
    printf("\nSetup for actual render.\n");
    icetResetTiles();
    icetAddTile(viewport[0], viewport[1], viewport[2], viewport[3], 0);
    icetDrawFunc(draw);
  /* Do a perfunctory draw to set other state variables. */
    icetDrawFrame();
    icetStateSetIntegerv(ICET_CONTAINED_VIEWPORT, 4, viewport);
    printf("Now render and get compressed image.\n");
    size = icetGetCompressedTileImage(0, compressedbuffer);
    printf("Expected size: %d.  Actual size: %d\n",
           (int)icetSparseImageSize(pixels), (int)size);
    if (   (size > icetSparseImageSize(pixels))
        || (size < (IceTUInt)4*num_buffers*pixels) ) {
        printf("Size differs from expected size!\n");
        result = TEST_FAILED;
    }

    printf("Cleaning up.\n");
    free(imagebuffer);
    free(compressedbuffer);
    return result;
}

static int CompressionSizeRun()
{
    int result;

    icetStrategy(ICET_STRATEGY_REDUCE);

    printf("Compress depth only.\n");
    icetInputOutputBuffers(ICET_DEPTH_BUFFER_BIT, ICET_DEPTH_BUFFER_BIT);
    result = DoCompressionTest(1);
    printf("\n\nCompress color only.\n");
    icetInputOutputBuffers(ICET_COLOR_BUFFER_BIT, ICET_COLOR_BUFFER_BIT);
    if (result == TEST_PASSED) {
        result = DoCompressionTest(1);
    } else {
        DoCompressionTest(1);
    }
    icetInputOutputBuffers(ICET_COLOR_BUFFER_BIT | ICET_DEPTH_BUFFER_BIT,
                           ICET_COLOR_BUFFER_BIT);
    printf("\n\nCompress color and depth.\n");
    if (result == TEST_PASSED) {
        result = DoCompressionTest(2);
    } else {
        DoCompressionTest(2);
    }

    return result;
}

int CompressionSize(int argc, char *argv[])
{
    /* To remove warning */
    (void)argc;
    (void)argv;

    run_test(CompressionSizeRun);
}
