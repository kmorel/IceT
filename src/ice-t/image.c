/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

/* Id */

#include <image.h>
#include <projections.h>
#include <state.h>
#include <diagnostics.h>

#include <stdlib.h>
#include <string.h>

#define GET_MAGIC_NUM(buf)      (((GLuint *)(buf))[0])
#define GET_PIXEL_COUNT(buf)    (((GLuint *)(buf))[1])
#define GET_DATA_START(buf)     (((GLuint *)(buf)) + 2)

#define GET_IMAGE_COLOR(buf)    (GET_DATA_START(buf))
#define GET_DEPTH_COLOR(buf)    (GET_DATA_START(buf) + GET_PIXEL_NUM(buf))

#define INACTIVE_RUN_LENGTH(rl) (((GLushort *)&(rl))[0])
#define ACTIVE_RUN_LENGTH(rl)   (((GLushort *)&(rl))[1])

#ifndef MIN
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y)       ((x) < (y) ? (y) : (x))
#endif

#ifdef _MSC_VER
#pragma warning(disable:4055)
#endif

/* Renders the geometry for a tile.  The geometry may not be projected
 * exactly into the tile.  screen_viewport gives the offset and dimensions
 * of the image in the OpenGL framebuffer.  tile_viewport gives the offset
 * and dimensions of the place where the pixels actually fall in a viewport
 * for the tile.  Everything outside of this tile viewport should be
 * cleared to the background color. */
static void renderTile(int tile, GLint *screen_viewport, GLint *tile_viewport);
/* Reads an image from the frame buffer.  x and y are the offset, and width
 * and height are the dimensions of the part of the framebuffer to read.
 * If renderTile has just been called, it is appropriate to use the values
 * from screen_viewport for these parameters. */
static void readImage(GLint x, GLint y, GLsizei width, GLsizei height,
                      IceTImage buffer);
/* Like readImage, except that it stores the result in an offset in buffer
 * and clears out everything else in buffer to the background color.  If
 * renderTile has just been called, and you want the appropriate full tile
 * image in buffer, then pass the values of screen_viewport into the fb_x,
 * fb_y, sub_width, and sub_height parameters.  Pass the first two values
 * of target_viewport into ib_x and ib_y.  The full dimensions of the tile
 * should be passed into full_width and full_height. */
static void readSubImage(GLint fb_x, GLint fb_y,
                         GLsizei sub_width, GLsizei sub_height,
                         IceTImage buffer,
                         GLint ib_x, GLint ib_y,
                         GLsizei full_width, GLsizei full_height);
/* Attempts to retrieve the correct value of the far depth.  First gets the
 * ICET_FAR_DEPTH parameter.  If depthBuffer is non NULL, it double checks
 * to make sure the first entry is not less than ICET_FAR_DEPTH.  If so,
 * the parameter is corrected. */
static GLuint getFarDepth(const GLuint *depthBuffer);
/* Gets a static image buffer that is shared amongst all contexts (and
 * therefore not thread safe.  The buffer is resized as necessary. */
static void getBuffers(GLuint type, GLuint pixels, IceTImage *bufferp);
/* Releases use of the buffers retreived with getBuffers.  Currently does
 * nothing as thread safty is not ensured. */
static void releaseBuffers(void);

GLuint icetFullImageTypeSize(GLuint pixels, GLuint type)
{
    switch (type) {
      case FULL_IMAGE_CD_MAGIC_NUM:
          return (2*sizeof(GLuint)*(pixels+1));
      case FULL_IMAGE_C_MAGIC_NUM:
      case FULL_IMAGE_D_MAGIC_NUM:
          return (sizeof(GLuint)*(pixels+2));
      default:
          icetRaiseError("Bad full image type.", ICET_INVALID_VALUE);
          return 0;
    }
}
GLuint icetSparseImageTypeSize(GLuint pixels, GLuint type)
{
    switch (type) {
      case SPARSE_IMAGE_CD_MAGIC_NUM:
          return (sizeof(GLuint)*(2*(pixels+1) + pixels/0x10000 + 1));
      case SPARSE_IMAGE_C_MAGIC_NUM:
      case SPARSE_IMAGE_D_MAGIC_NUM:
          return (sizeof(GLuint)*(pixels + 2 + pixels/0x10000 + 1));
      default:
          icetRaiseError("Bad sparse image type.", ICET_INVALID_VALUE);
          return 0;
    }
}

GLubyte *icetGetImageColorBuffer(IceTImage image)
{
    switch (GET_MAGIC_NUM(image)) {
      case FULL_IMAGE_CD_MAGIC_NUM:
      case FULL_IMAGE_C_MAGIC_NUM:
          return (GLubyte *)(image + 2);
      case FULL_IMAGE_D_MAGIC_NUM:
          return NULL;
      default:
          icetRaiseError("Tried to get colors from invalid image buffer.",
                         ICET_INVALID_VALUE);
          return NULL;
    }
}
GLuint *icetGetImageDepthBuffer(IceTImage image)
{
    switch (GET_MAGIC_NUM(image)) {
      case FULL_IMAGE_CD_MAGIC_NUM:
          return (image + 2 + image[1]);
      case FULL_IMAGE_C_MAGIC_NUM:
          return NULL;
      case FULL_IMAGE_D_MAGIC_NUM:
          return image + 2;
      default:
          icetRaiseError("Tried to get colors from invalid image buffer.",
                         ICET_INVALID_VALUE);
          return NULL;
    }
}

void icetInitializeImage(IceTImage image, GLuint pixel_count)
{
    GLenum type;

    icetGetIntegerv(ICET_INPUT_BUFFERS, (GLint *)&type);
    type |= FULL_IMAGE_BASE_MAGIC_NUM;

    icetInitializeImageType(image, pixel_count, type);
}

void icetInitializeImageType(IceTImage image, GLuint pixel_count, GLuint type) {
    GET_MAGIC_NUM(image) = type;
    GET_PIXEL_COUNT(image) = pixel_count;
}

void icetClearImage(IceTImage image)
{
    GLenum output_buffers;
    GLuint pixels = icetGetImagePixelCount(image);

    icetGetIntegerv(ICET_OUTPUT_BUFFERS, (GLint *)&output_buffers);
    if ((output_buffers & ICET_COLOR_BUFFER_BIT) != 0) {
        GLuint background_color;
        GLuint *color = (GLuint *)icetGetImageColorBuffer(image);
        GLuint i;
        icetGetIntegerv(ICET_BACKGROUND_COLOR_WORD, (GLint *)&background_color);
        for (i = 0; i < pixels; i++) {
            color[i] = background_color;
        }
    }
    if ((output_buffers & ICET_DEPTH_BUFFER_BIT) != 0) {
        GLuint far_depth = getFarDepth(NULL);
        GLuint *depth = icetGetImageDepthBuffer(image);
        GLuint i;
        for (i = 0; i < pixels; i++) {
            depth[i] = far_depth;
        }
    }
}

void icetInputOutputBuffers(GLenum inputs, GLenum outputs)
{
    if ((inputs & outputs) != outputs) {
        icetRaiseError("Tried to use an output that is not also an input.",
                       ICET_INVALID_VALUE);
        return;
    }

    if ((outputs & (ICET_COLOR_BUFFER_BIT | ICET_DEPTH_BUFFER_BIT)) == 0) {
        icetRaiseError("No output selected?  Why use ICE-T at all?  Ignoring.",
                       ICET_INVALID_VALUE);
        return;
    }

    icetStateSetInteger(ICET_INPUT_BUFFERS, inputs);
    icetStateSetInteger(ICET_OUTPUT_BUFFERS, outputs);
}

void icetGetTileImage(GLint tile, IceTImage buffer)
{
    GLint screen_viewport[4], target_viewport[4];
    GLint *viewports;
    GLsizei width, height;

    viewports = icetUnsafeStateGet(ICET_TILE_VIEWPORTS);
    width = viewports[4*tile+2];
    height = viewports[4*tile+3];

    icetInitializeImage(buffer, width*height);

    renderTile(tile, screen_viewport, target_viewport);

    readSubImage(screen_viewport[0], screen_viewport[1],
                 screen_viewport[2], screen_viewport[3],
                 buffer, target_viewport[0], target_viewport[1],
                 width, height);
}

GLuint icetGetCompressedTileImage(GLint tile, IceTSparseImage buffer)
{
    GLint screen_viewport[4], target_viewport[4];
    IceTImage imageBuffer;
    GLubyte *colorBuffer;
    GLuint  *depthBuffer;
    GLint *viewports;
    GLsizei width, height;
    int space_left, space_right, space_bottom, space_top;
    GLuint compressedSize;

    viewports = icetUnsafeStateGet(ICET_TILE_VIEWPORTS);
    width = viewports[4*tile+2];
    height = viewports[4*tile+3];

    renderTile(tile, screen_viewport, target_viewport);
    getBuffers((  *((GLuint *)icetUnsafeStateGet(ICET_INPUT_BUFFERS))
                | FULL_IMAGE_BASE_MAGIC_NUM),
               screen_viewport[2]*screen_viewport[3],
               &imageBuffer);
    readImage(screen_viewport[0], screen_viewport[1],
              screen_viewport[2], screen_viewport[3],
              imageBuffer);

    colorBuffer = icetGetImageColorBuffer(imageBuffer);
    depthBuffer = icetGetImageDepthBuffer(imageBuffer);

    space_left = target_viewport[0];
    space_right = width - target_viewport[2] - space_left;
    space_bottom = target_viewport[1];
    space_top = height - target_viewport[3] - space_bottom;

    if (depthBuffer) {
        GLuint far_depth = getFarDepth(depthBuffer);
        GLuint *depth = depthBuffer;
        if (colorBuffer) {
            GLuint *color = (GLuint*)colorBuffer;
#define MAGIC_NUMBER            SPARSE_IMAGE_CD_MAGIC_NUM
#define COMPRESSED_BUFFER       buffer
#define PIXEL_COUNT             width*height
#define ACTIVE()                (*depth != far_depth)
#define WRITE_PIXEL(dest)       *(dest++) = *color;  *(dest++) = *depth;
#define INCREMENT_PIXEL()       color++;  depth++;
#define COMPRESSED_SIZE         compressedSize
#define PADDING
#define SPACE_BOTTOM    space_bottom
#define SPACE_TOP       space_top
#define SPACE_LEFT      space_left
#define SPACE_RIGHT     space_right
#define FULL_WIDTH      width
#define FULL_HEIGHT     height
#include "compress_func_body.h"
        } else {
#define MAGIC_NUMBER            SPARSE_IMAGE_D_MAGIC_NUM
#define COMPRESSED_BUFFER       buffer
#define PIXEL_COUNT             width*height
#define ACTIVE()                (*depth != far_depth)
#define WRITE_PIXEL(dest)       *(dest++) = *depth;
#define INCREMENT_PIXEL()       depth++;
#define COMPRESSED_SIZE         compressedSize
#define PADDING
#define SPACE_BOTTOM    space_bottom
#define SPACE_TOP       space_top
#define SPACE_LEFT      space_left
#define SPACE_RIGHT     space_right
#define FULL_WIDTH      width
#define FULL_HEIGHT     height
#include "compress_func_body.h"
        }
    } else {
        GLuint *color = (GLuint*)colorBuffer;
#define MAGIC_NUMBER            SPARSE_IMAGE_C_MAGIC_NUM
#define COMPRESSED_BUFFER       buffer
#define PIXEL_COUNT             width*height
#define ACTIVE()                (((GLubyte*)color)[3] != 0x00)
#define WRITE_PIXEL(dest)       *(dest++) = *color;
#define INCREMENT_PIXEL()       color++;
#define COMPRESSED_SIZE         compressedSize
#define PADDING
#define SPACE_BOTTOM    space_bottom
#define SPACE_TOP       space_top
#define SPACE_LEFT      space_left
#define SPACE_RIGHT     space_right
#define FULL_WIDTH      width
#define FULL_HEIGHT     height
#include "compress_func_body.h"
    }

    releaseBuffers();

    return compressedSize;
}

GLuint icetCompressImage(const IceTImage imageBuffer,
                         IceTSparseImage compressedBuffer)
{
    return icetCompressSubImage(imageBuffer, 0, GET_PIXEL_COUNT(imageBuffer),
                                compressedBuffer);
}

GLuint icetCompressSubImage(const IceTImage imageBuffer,
                            GLuint offset, GLuint pixels,
                            IceTSparseImage compressedBuffer)
{
    const GLuint *color
        = (GLuint *)icetGetImageColorBuffer((IceTImage)imageBuffer);
    const GLuint *depth = icetGetImageDepthBuffer((IceTImage)imageBuffer);
    GLuint compressedSize;

    if (depth) {
        GLuint far_depth = getFarDepth(depth);
        depth += offset;
        if (color) {
            color += offset;
#define MAGIC_NUMBER            SPARSE_IMAGE_CD_MAGIC_NUM
#define COMPRESSED_BUFFER       compressedBuffer
#define PIXEL_COUNT             pixels
#define ACTIVE()                (*depth != far_depth)
#define WRITE_PIXEL(dest)       *(dest++) = *color;  *(dest++) = *depth;
#define INCREMENT_PIXEL()       color++;  depth++;
#define COMPRESSED_SIZE         compressedSize
#include "compress_func_body.h"
        } else {
#define MAGIC_NUMBER            SPARSE_IMAGE_D_MAGIC_NUM
#define COMPRESSED_BUFFER       compressedBuffer
#define PIXEL_COUNT             pixels
#define ACTIVE()                (*depth != far_depth)
#define WRITE_PIXEL(dest)       *(dest++) = *depth;
#define INCREMENT_PIXEL()       depth++;
#define COMPRESSED_SIZE         compressedSize
#include "compress_func_body.h"
        }
    } else {
        color += offset;
#define MAGIC_NUMBER            SPARSE_IMAGE_C_MAGIC_NUM
#define COMPRESSED_BUFFER       compressedBuffer
#define PIXEL_COUNT             pixels
#define ACTIVE()                (((GLubyte*)color)[3] != 0x00)
#define WRITE_PIXEL(dest)       *(dest++) = *color;
#define INCREMENT_PIXEL()       color++;
#define COMPRESSED_SIZE         compressedSize
#include "compress_func_body.h"
    }

    return compressedSize;
}

GLuint icetDecompressImage(const IceTSparseImage compressedBuffer,
                           IceTImage imageBuffer)
{
    GLuint *color;
    GLuint *depth;
    GLuint background_color;
    GLuint far_depth;

    switch (GET_MAGIC_NUM(compressedBuffer)) {
      case SPARSE_IMAGE_CD_MAGIC_NUM:
          icetInitializeImageType(imageBuffer,GET_PIXEL_COUNT(compressedBuffer),
                                  FULL_IMAGE_CD_MAGIC_NUM);
          color = (GLuint *)icetGetImageColorBuffer(imageBuffer);
          depth = icetGetImageDepthBuffer(imageBuffer);
          icetGetIntegerv(ICET_BACKGROUND_COLOR_WORD,
                          (GLint *)&background_color);
          far_depth = getFarDepth(NULL);
#define COMPRESSED_BUFFER       compressedBuffer
#define READ_PIXEL(src)         *color = *(src++);  *depth = *(src++);
#define INCREMENT_PIXEL()       color++;  depth++;
#define INCREMENT_INACTIVE_PIXELS(count)                \
    for (_i = 0; _i < count; _i++) {                    \
        *(color++) = background_color;                  \
        *(depth++) = far_depth;                         \
    }
#define TIME_DECOMPRESSION
#include "decompress_func_body.h"
          break;
      case SPARSE_IMAGE_C_MAGIC_NUM:
          icetInitializeImageType(imageBuffer,GET_PIXEL_COUNT(compressedBuffer),
                                  FULL_IMAGE_C_MAGIC_NUM);
          color = (GLuint *)icetGetImageColorBuffer(imageBuffer);
          icetGetIntegerv(ICET_BACKGROUND_COLOR_WORD,
                          (GLint *)&background_color);
#define COMPRESSED_BUFFER       compressedBuffer
#define READ_PIXEL(src)         *color = *(src++);
#define INCREMENT_PIXEL()       color++;
#define INCREMENT_INACTIVE_PIXELS(count)                \
    for (_i = 0; _i < count; _i++) {                    \
        *(color++) = background_color;                  \
    }
#define TIME_DECOMPRESSION
#include "decompress_func_body.h"
          break;
      case SPARSE_IMAGE_D_MAGIC_NUM:
          icetInitializeImageType(imageBuffer,GET_PIXEL_COUNT(compressedBuffer),
                                  FULL_IMAGE_D_MAGIC_NUM);
          depth = icetGetImageDepthBuffer(imageBuffer);
          far_depth = getFarDepth(NULL);
#define COMPRESSED_BUFFER       compressedBuffer
#define READ_PIXEL(src)         *depth = *(src++);
#define INCREMENT_PIXEL()       depth++;
#define INCREMENT_INACTIVE_PIXELS(count)                \
    for (_i = 0; _i < count; _i++) {                    \
        *(depth++) = far_depth;                         \
    }
#define TIME_DECOMPRESSION
#include "decompress_func_body.h"
          break;
      default:
          icetRaiseError("Tried to decompress something not compressed.",
                         ICET_INVALID_VALUE);
          return 0;
    }

    return GET_PIXEL_COUNT(imageBuffer);
}


void icetComposite(IceTImage destBuffer, const IceTImage srcBuffer,
                   int srcOnTop)
{
    GLuint *destColorBuffer;
    GLuint *destDepthBuffer;
    const GLuint *srcColorBuffer;
    const GLuint *srcDepthBuffer;
    GLuint pixels;
    GLuint i;
    GLdouble timer;
    GLdouble *compare_time;

    pixels = GET_PIXEL_COUNT(destBuffer);
    if (pixels != GET_PIXEL_COUNT(srcBuffer)) {
        icetRaiseError("Source and destination sizes don't match.",
                       ICET_SANITY_CHECK_FAIL);
        return;
    }

    if (GET_MAGIC_NUM(destBuffer) != GET_MAGIC_NUM(srcBuffer)) {
        icetRaiseError("Source and destination types don't match.",
                       ICET_SANITY_CHECK_FAIL);
        return;
    }

    destColorBuffer = (GLuint *)icetGetImageColorBuffer(destBuffer);
    destDepthBuffer = icetGetImageDepthBuffer(destBuffer);
    srcColorBuffer =(GLuint *)icetGetImageColorBuffer((IceTImage)srcBuffer);
    srcDepthBuffer = icetGetImageDepthBuffer((IceTImage)srcBuffer);

    compare_time = icetUnsafeStateGet(ICET_COMPARE_TIME);
    timer = icetWallTime();

    if (srcDepthBuffer) {
        if (srcColorBuffer) {
            for (i = 0; i < pixels; i++) {
                if (srcDepthBuffer[i] < destDepthBuffer[i]) {
                    destDepthBuffer[i] = srcDepthBuffer[i];
                    destColorBuffer[i] = srcColorBuffer[i];
                }
            }
        } else {
            for (i = 0; i < pixels; i++) {
                if (srcDepthBuffer[i] < destDepthBuffer[i]) {
                    destDepthBuffer[i] = srcDepthBuffer[i];
                }
            }
        }
    } else {
        if (srcOnTop) {
            for (i = 0; i < pixels; i++) {
              /* The blending should probably be more flexible. */
                ICET_OVER((GLubyte *)(&srcColorBuffer[i]),
                          (GLubyte *)(&destColorBuffer[i]));
            }
        } else {
            for (i = 0; i < pixels; i++) {
              /* The blending should probably be more flexible. */
                ICET_UNDER((GLubyte *)(&srcColorBuffer[i]),
                           (GLubyte *)(&destColorBuffer[i]));
            }
        }
    }

    *compare_time += icetWallTime() - timer;
}

void icetCompressedComposite(IceTImage destBuffer,
                             const IceTSparseImage srcBuffer,
                             int srcOnTop)
{
    icetCompressedSubComposite(destBuffer, 0, GET_PIXEL_COUNT(destBuffer),
                               srcBuffer, srcOnTop);
}
void icetCompressedSubComposite(IceTImage destBuffer,
                                GLuint offset, GLuint pixels,
                                const IceTSparseImage srcBuffer,
                                int srcOnTop)
{
    GLuint *destColor;
    GLuint *destDepth;
    GLdouble timer;
    GLdouble *compare_time;

    compare_time = icetUnsafeStateGet(ICET_COMPARE_TIME);
    timer = icetWallTime();

    if (   (GET_MAGIC_NUM(srcBuffer) ^ SPARSE_IMAGE_BASE_MAGIC_NUM)
        != (GET_MAGIC_NUM(destBuffer) ^ FULL_IMAGE_BASE_MAGIC_NUM) ) {
        icetRaiseError("Source and destination buffer types do not match.",
                       ICET_INVALID_VALUE);
        return;
    }
    if (pixels != GET_PIXEL_COUNT(srcBuffer)) {
        icetRaiseError("Sizes of src and dest do not agree.",
                       ICET_SANITY_CHECK_FAIL);
        return;
    }

    destColor = (GLuint *)icetGetImageColorBuffer(destBuffer) + offset;
    destDepth = icetGetImageDepthBuffer(destBuffer) + offset;

    switch (GET_MAGIC_NUM(srcBuffer)) {
      case SPARSE_IMAGE_CD_MAGIC_NUM:
#define COMPRESSED_BUFFER       srcBuffer
#define READ_PIXEL(src)                                 \
    if (src[1] < *destDepth) {                          \
        *destColor = src[0];                            \
        *destDepth = src[1];                            \
    }                                                   \
    src += 2;
#define INCREMENT_PIXEL()       destColor++;  destDepth++;
#define INCREMENT_INACTIVE_PIXELS(count) destColor += count; destDepth += count;
#include "decompress_func_body.h"
          break;
      case SPARSE_IMAGE_D_MAGIC_NUM:
#define COMPRESSED_BUFFER       srcBuffer
#define READ_PIXEL(src)                                 \
    if (src[0] < *destDepth) {                          \
        *destDepth = src[0];                            \
    }                                                   \
    src += 1;
#define INCREMENT_PIXEL()       destDepth++;
#define INCREMENT_INACTIVE_PIXELS(count) destDepth += count;
#include "decompress_func_body.h"
          break;
      case SPARSE_IMAGE_C_MAGIC_NUM:
          if (srcOnTop) {
#define COMPRESSED_BUFFER       srcBuffer
#define READ_PIXEL(srcColor)                                            \
    {                                                                   \
      /* The blending should probably be more flexible. */              \
        ICET_OVER((GLubyte *)(srcColor), (GLubyte *)(destColor));       \
    }                                                                   \
    srcColor += 1;
#define INCREMENT_PIXEL()       destColor++;
#define INCREMENT_INACTIVE_PIXELS(count) destColor += count;
#include "decompress_func_body.h"
          } else {
#define COMPRESSED_BUFFER       srcBuffer
#define READ_PIXEL(srcColor)                                            \
    {                                                                   \
      /* The blending should probably be more flexible. */              \
        ICET_UNDER((GLubyte *)(srcColor), (GLubyte *)(destColor));      \
    }                                                                   \
    srcColor += 1;
#define INCREMENT_PIXEL()       destColor++;
#define INCREMENT_INACTIVE_PIXELS(count) destColor += count;
#include "decompress_func_body.h"
          }
          break;
    }
}

/* Makes sure that all the information for the current tile is rendered and
   currently available in the given OpenGL frame buffers.  Returns a
   viewport in the screen which has the pertinent information and a
   viewport that it should be mapped to in the given tile screen space. */
static void renderTile(int tile, GLint *screen_viewport, GLint *target_viewport)
{
    GLint *contained_viewport;
    GLint *tile_viewport;
    GLboolean *contained_mask;
    GLint physical_viewport[4];
    GLint max_width, max_height;
    GLboolean use_floating_viewport;
    IceTCallback drawfunc;
    GLvoid *value;
    GLdouble render_time;
    GLdouble timer;
    GLuint far_depth;
    GLint readBuffer;

    icetRaiseDebug1("Rendering tile %d", tile);
    contained_viewport = icetUnsafeStateGet(ICET_CONTAINED_VIEWPORT);
    tile_viewport = ((GLint *)icetUnsafeStateGet(ICET_TILE_VIEWPORTS)) + 4*tile;
    contained_mask = icetUnsafeStateGet(ICET_CONTAINED_TILES_MASK);
    use_floating_viewport = icetIsEnabled(ICET_FLOATING_VIEWPORT);

    glGetIntegerv(GL_VIEWPORT, physical_viewport);
    max_width = physical_viewport[2];
    max_height = physical_viewport[3];

  /* Get ready for tile projection. */
    icetRaiseDebug("Determine projection");
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    icetRaiseDebug4("contained viewport: %d %d %d %d",
                    (int)contained_viewport[0], (int)contained_viewport[1],
                    (int)contained_viewport[2], (int)contained_viewport[3]);
    icetRaiseDebug4("tile viewport: %d %d %d %d",
                    (int)tile_viewport[0], (int)tile_viewport[1],
                    (int)tile_viewport[2], (int)tile_viewport[3]);

    if (   !contained_mask[tile]
        || (contained_viewport[0] + contained_viewport[2] < tile_viewport[0])
        || (contained_viewport[1] + contained_viewport[3] < tile_viewport[1])
        || (contained_viewport[0] > tile_viewport[0] + tile_viewport[2])
        || (contained_viewport[1] > tile_viewport[1] + tile_viewport[3]) ) {
      /* Case 0: geometry completely outside tile. */
        icetRaiseDebug("Case 0: geometry completely outside tile.");
        screen_viewport[0] = 1; /* Make target and screen viewports slightly */
        target_viewport[0] = 0; /* different to signal not to read the image */
        screen_viewport[1] = 1; /* completely from OpenGL buffers.           */
        target_viewport[1] = 0;
        screen_viewport[2] = target_viewport[2] = 0;
        screen_viewport[3] = target_viewport[3] = 0;
      /* Don't bother to render. */
        return;
#if 0
    } else if (   (contained_viewport[0] >= tile_viewport[0])
               && (contained_viewport[1] >= tile_viewport[1])
               && (   contained_viewport[2]+contained_viewport[0]
                   <= tile_viewport[2]+tile_viewport[0])
               && (   contained_viewport[3]+contained_viewport[1]
                   <= tile_viewport[3]+tile_viewport[1]) ) {
      /* Case 1: geometry fits entirely within tile. */
        icetRaiseDebug("Case 1: geometry fits entirely within tile.");
        icetProjectTile(tile);
        icetStateSetIntegerv(ICET_RENDERED_VIEWPORT, 4, tile_viewport);
        screen_viewport[0] = target_viewport[0]
            = contained_viewport[0] - tile_viewport[0];
        screen_viewport[1] = target_viewport[1]
            = contained_viewport[1] - tile_viewport[1];
        screen_viewport[2] = target_viewport[2] = contained_viewport[2];
        screen_viewport[3] = target_viewport[3] = contained_viewport[3];
#endif
    } else if (   !use_floating_viewport
               || (contained_viewport[2] > max_width)
               || (contained_viewport[3] > max_height) ) {
      /* Case 2: Can't use floating viewport due to use selection or image
         does not fit. */
        icetRaiseDebug("Case 2: Can't use floating viewport.");
        icetProjectTile(tile);
        icetStateSetIntegerv(ICET_RENDERED_VIEWPORT, 4, tile_viewport);
        if (contained_viewport[0] <= tile_viewport[0]) {
            screen_viewport[0] = target_viewport[0] = 0;
            screen_viewport[2] = target_viewport[2]
                = MIN(tile_viewport[2],
                      contained_viewport[0] + contained_viewport[2]
                      - tile_viewport[0]);
        } else {
            screen_viewport[0] = target_viewport[0]
                = contained_viewport[0] - tile_viewport[0];
            screen_viewport[2] = target_viewport[2]
                = MIN(contained_viewport[2],
                      tile_viewport[0] + tile_viewport[2]
                      - contained_viewport[0]);
        }

        if (contained_viewport[1] <= tile_viewport[1]) {
            screen_viewport[1] = target_viewport[1] = 0;
            screen_viewport[3] = target_viewport[3]
                = MIN(tile_viewport[3],
                      contained_viewport[1] + contained_viewport[3]
                      - tile_viewport[1]);
        } else {
            screen_viewport[1] = target_viewport[1]
                = contained_viewport[1] - tile_viewport[1];
            screen_viewport[3] = target_viewport[3]
                = MIN(contained_viewport[3],
                      tile_viewport[1] + tile_viewport[3]
                      - contained_viewport[1]);
        }
    } else {
      /* Case 3: Using floating viewport. */
        GLdouble matrix[16];
        GLint *rendered_viewport;
        icetRaiseDebug("Case 3: Using floating viewport.");

        if (contained_viewport[0] < tile_viewport[0]) {
            screen_viewport[0] = tile_viewport[0] - contained_viewport[0];
            screen_viewport[2] = MIN(contained_viewport[2] - screen_viewport[0],
                                     tile_viewport[2]);
            target_viewport[0] = 0;
            target_viewport[2] = screen_viewport[2];
        } else {
            target_viewport[0] = contained_viewport[0] - tile_viewport[0];
            target_viewport[2] = MIN(tile_viewport[2] - target_viewport[0],
                                     contained_viewport[2]);
            screen_viewport[0] = 0;
            screen_viewport[2] = target_viewport[2];
        }
        if (contained_viewport[1] < tile_viewport[1]) {
            screen_viewport[1] = tile_viewport[1] - contained_viewport[1];
            screen_viewport[3] = MIN(contained_viewport[3] - screen_viewport[1],
                                     tile_viewport[3]);
            target_viewport[1] = 0;
            target_viewport[3] = screen_viewport[3];
        } else {
            target_viewport[1] = contained_viewport[1] - tile_viewport[1];
            target_viewport[3] = MIN(tile_viewport[3] - target_viewport[1],
                                     contained_viewport[3]);
            screen_viewport[1] = 0;
            screen_viewport[3] = target_viewport[3];
        }

        if (  icetStateGetTime(ICET_RENDERED_VIEWPORT)
            > icetStateGetTime(ICET_IS_DRAWING_FRAME) ) {
          /* Already rendered image for this tile. */
            return;
        }

      /* Setup render for this tile. */

      /* This will later be freed with state.c code. */
        rendered_viewport = malloc(sizeof(GLint)*4);
        rendered_viewport[0] = contained_viewport[0];
        rendered_viewport[1] = contained_viewport[1];
        rendered_viewport[2] = max_width;
        rendered_viewport[3] = max_height;
        icetUnsafeStateSet(ICET_RENDERED_VIEWPORT, 4,
                           ICET_INT, rendered_viewport);

        icetGetViewportProject(rendered_viewport[0], rendered_viewport[1],
                               rendered_viewport[2], rendered_viewport[3],
                               matrix);
        glMultMatrixd(matrix);
        icetGetDoublev(ICET_PROJECTION_MATRIX, matrix);
        glMultMatrixd(matrix);
    }

  /* Now we can actually start to render an image. */
    glMatrixMode(GL_MODELVIEW);

  /* This is a good place to check the far depth since we have been asked
     to draw in the current OpenGL context. */
    icetRaiseDebug("Checking depth.");
    icetGetIntegerv(ICET_ABSOLUTE_FAR_DEPTH, (GLint *)&far_depth);
    if (far_depth == 1) {       /* An unlikely initial value. */
        icetGetIntegerv(ICET_READ_BUFFER, &readBuffer);
        glReadBuffer(readBuffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glFlush();
        glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
                     &far_depth);
        icetRaiseDebug1("Setting far depth to 0x%X", (unsigned int)far_depth);
        icetStateSetInteger(ICET_ABSOLUTE_FAR_DEPTH, far_depth);
    }

  /* Draw the geometry. */
    icetRaiseDebug("Getting callback.");
    icetGetPointerv(ICET_DRAW_FUNCTION, &value);
    drawfunc = (IceTCallback)value;
    icetGetDoublev(ICET_RENDER_TIME, &render_time);
    icetRaiseDebug("Calling draw function.");
    timer = icetWallTime();
    (*drawfunc)();
    render_time += icetWallTime() - timer;
    icetStateSetDouble(ICET_RENDER_TIME, render_time);
}

/* Performs a glReadPixels on the color and depth buffers (unless they
 * are not needed).  buffer is expected to already have its magic number
 * and pixel count set. */
static void readImage(GLint x, GLint y, GLsizei width, GLsizei height,
                      IceTImage buffer)
{
    readSubImage(x, y, width, height, buffer, 0, 0, width, height);
}
static void readSubImage(GLint fb_x, GLint fb_y,
                         GLsizei sub_width, GLsizei sub_height,
                         IceTImage buffer,
                         GLint ib_x, GLint ib_y,
                         GLsizei full_width, GLsizei full_height)
{
    GLint readBuffer;
    GLint colorFormat;
    GLdouble *read_time;
    GLdouble timer;
    GLubyte *colorBuffer;
    GLuint *depthBuffer;
    GLint physical_viewport[4];
    GLint x_offset, y_offset;
    GLuint background_color;
    GLuint far_depth;
    GLint x, y;

    icetRaiseDebug4("Reading viewport %d %d %d %d", (int)fb_x, (int)fb_y,
                    (int)sub_width, (int)sub_height);
    icetRaiseDebug2("Image offset %d %d", (int)ib_x, (int)ib_y);
    icetRaiseDebug2("Full image dimensions %d %d",
                    (int)full_width, (int)full_height);

#ifdef DEBUG
    {
        GLint alpha_bits;
        GLfloat alpha_bias, alpha_scale;
        glGetIntegerv(GL_ALPHA_BITS, &alpha_bits);
        glGetFloatv(GL_ALPHA_BIAS, &alpha_bias);
        glGetFloatv(GL_ALPHA_SCALE, &alpha_scale);
        icetRaiseDebug1("Alpha bits %d", (int)alpha_bits);
        icetRaiseDebug2("Alpha bias/scale %f/%f",
                        (float)alpha_bias, (float)alpha_scale);
    }
#endif

#ifdef DEBUG
    if (   (GET_MAGIC_NUM(buffer) & FULL_IMAGE_BASE_MAGIC_NUM)
        != FULL_IMAGE_BASE_MAGIC_NUM) {
        icetRaiseError("Buffer magic number not set.", ICET_SANITY_CHECK_FAIL);
        return;
    }
    if (GET_PIXEL_COUNT(buffer) != (GLuint)(full_width*full_height)) {
        icetRaiseError("Buffer size was not set.", ICET_SANITY_CHECK_FAIL);
        return;
    }
#endif /* DEBUG */

    colorBuffer = icetGetImageColorBuffer(buffer);
    depthBuffer = icetGetImageDepthBuffer(buffer);

    glPixelStorei(GL_PACK_ROW_LENGTH, full_width);
    glPixelStorei(GL_PACK_SKIP_PIXELS, ib_x);
    glPixelStorei(GL_PACK_SKIP_ROWS, ib_y);

    icetGetIntegerv(ICET_READ_BUFFER, &readBuffer);
    glReadBuffer(readBuffer);

    glGetIntegerv(GL_VIEWPORT, physical_viewport);
    x_offset = physical_viewport[0];
    y_offset = physical_viewport[1];

    read_time = icetUnsafeStateGet(ICET_BUFFER_READ_TIME);
    timer = icetWallTime();

    if (colorBuffer != NULL) {
        icetGetIntegerv(ICET_COLOR_FORMAT, &colorFormat);
        glReadPixels(fb_x + x_offset, fb_y + y_offset, sub_width, sub_height,
                     colorFormat, GL_UNSIGNED_BYTE, colorBuffer);

        icetGetIntegerv(ICET_BACKGROUND_COLOR_WORD, (GLint *)&background_color);
      /* Clear out bottom. */
        for (y = 0; y < ib_y; y++) {
            for (x = 0; x < full_width; x++) {
                ((GLuint *)colorBuffer)[y*full_width + x] = background_color;
            }
        }
      /* Clear out left. */
        if (ib_x > 0) {
            for (y = ib_y; y < sub_height+ib_y; y++) {
                for (x = 0; x < ib_x; x++) {
                    ((GLuint *)colorBuffer)[y*full_width + x] =background_color;
                }
            }
        }
      /* Clear out right. */
        if (ib_x + sub_width < full_width) {
            for (y = ib_y; y < sub_height+ib_y; y++) {
                for (x = ib_x+sub_width; x < full_width; x++) {
                    ((GLuint *)colorBuffer)[y*full_width + x] =background_color;
                }
            }
        }
      /* Clear out top. */
        for (y = ib_y+sub_height; y < full_height; y++) {
            for (x = 0; x < full_width; x++) {
                ((GLuint *)colorBuffer)[y*full_width + x] = background_color;
            }
        }
    }
    if (depthBuffer != NULL) {
        glReadPixels(fb_x + x_offset, fb_y + y_offset, sub_width, sub_height,
                     GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, depthBuffer);

        far_depth = getFarDepth(NULL);
      /* Clear out bottom. */
        for (y = 0; y < ib_y; y++) {
            for (x = 0; x < full_width; x++) {
                depthBuffer[y*full_width + x] = far_depth;
            }
        }
      /* Clear out left. */
        if (ib_x > 0) {
            for (y = ib_y; y < sub_height+ib_y; y++) {
                for (x = 0; x < ib_x; x++) {
                    depthBuffer[y*full_width + x] = far_depth;
                }
            }
        }
      /* Clear out right. */
        if (ib_x + sub_width < full_width) {
            for (y = ib_y; y < sub_height+ib_y; y++) {
                for (x = ib_x+sub_width; x < full_width; x++) {
                    depthBuffer[y*full_width + x] = far_depth;
                }
            }
        }
      /* Clear out top. */
        for (y = ib_y+sub_height; y < full_height; y++) {
            for (x = 0; x < full_width; x++) {
                depthBuffer[y*full_width + x] = far_depth;
            }
        }
    }

    *read_time += icetWallTime() - timer;

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
}

static GLuint getFarDepth(const GLuint *depthBuffer)
{
    GLuint far_depth;

    icetGetIntegerv(ICET_ABSOLUTE_FAR_DEPTH, (GLint *)&far_depth);
    if ((depthBuffer != NULL) && (depthBuffer[0] > far_depth)) {
        icetRaiseDebug("Far depth failed sanity check, resetting.");
        icetRaiseDebug2("Old value: 0x%x,  New value: 0x%x",
                        (unsigned int)far_depth, (unsigned int)depthBuffer[0]);
        far_depth = depthBuffer[0];
        icetStateSetInteger(ICET_ABSOLUTE_FAR_DEPTH, far_depth);
    }

    return far_depth;
}

/* Currently not thread safe. */
static void getBuffers(GLuint type, GLuint pixels, IceTImage *bufferp)
{
    static IceTImage buffer = NULL;
    static GLuint bufferSize = 0;

    GLuint newBufferSize = icetFullImageTypeSize(pixels, type);

    if (newBufferSize > bufferSize) {
        free(buffer);
        buffer = malloc(newBufferSize);
        if (buffer == NULL) {
            icetRaiseError("Could not allocate buffers.", ICET_OUT_OF_MEMORY);
            bufferSize = 0;
        } else {
            bufferSize = newBufferSize;
        }
    }

    icetInitializeImage(buffer, pixels);

    *bufferp = buffer;
}

static void releaseBuffers(void)
{
  /* If we were thread safe, we would unlock the buffers. */
}

