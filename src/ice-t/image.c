/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#include <image.h>
#include <projections.h>
#include <state.h>
#include <diagnostics.h>

/* TODO: Get rid of this and decouple the core IceT from OpenGL. */
#include <IceTGL.h>

#include <stdlib.h>
#include <string.h>

#define ICET_IMAGE_MAGIC_NUM            (IceTEnum)0x004D5000
#define ICET_SPARSE_IMAGE_MAGIC_NUM     (IceTEnum)0x004D6000

#define ICET_IMAGE_MAGIC_NUM_INDEX      0
#define ICET_IMAGE_COLOR_FORMAT_INDEX   1
#define ICET_IMAGE_DEPTH_FORMAT_INDEX   2
#define ICET_IMAGE_SIZE_INDEX           3
#define ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX 4
#define ICET_IMAGE_DATA_START_INDEX     5

#define ICET_IMAGE_HEADER(image)        ((IceTUInt *)image.opaque_internals)
#define ICET_IMAGE_DATA(image) \
    ((IceTVoid *)&(ICET_IMAGE_HEADER(image)[ICET_IMAGE_DATA_START_INDEX]))

#define INACTIVE_RUN_LENGTH(rl) (((IceTUShort *)(rl))[0])
#define ACTIVE_RUN_LENGTH(rl)   (((IceTUShort *)(rl))[1])
#define RUN_LENGTH_SIZE         (2*sizeof(IceTUShort))

#ifndef MIN
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y)       ((x) < (y) ? (y) : (x))
#endif

#ifdef _MSC_VER
#pragma warning(disable:4055)
#endif

/* Returns the size, in bytes, of a color/depth value for a single pixel. */
static IceTSizeType colorPixelSize(IceTEnum color_format);
static IceTSizeType depthPixelSize(IceTEnum depth_format);

/* Returns the pointer to the color/depth data regardless of type. */
static IceTVoid *icetImageGetColor(IceTImage image);
static IceTVoid *icetImageGetDepth(IceTImage image);

/* Renders the geometry for a tile.  The geometry may not be projected
 * exactly into the tile.  screen_viewport gives the offset and dimensions
 * of the image in the OpenGL framebuffer.  tile_viewport gives the offset
 * and dimensions of the place where the pixels actually fall in a viewport
 * for the tile.  Everything outside of this tile viewport should be
 * cleared to the background color. */
static void renderTile(int tile, IceTInt *screen_viewport,
                       IceTInt *tile_viewport);
/* Reads an image from the frame buffer.  x and y are the offset, and width
 * and height are the dimensions of the part of the framebuffer to read.
 * If renderTile has just been called, it is appropriate to use the values
 * from screen_viewport for these parameters. */
static void readImage(IceTInt x, IceTInt y,
                      IceTSizeType width, IceTSizeType height,
                      IceTImage image);
/* Like readImage, except that it stores the result in an offset in buffer
 * and clears out everything else in buffer to the background color.  If
 * renderTile has just been called, and you want the appropriate full tile
 * image in buffer, then pass the values of screen_viewport into the fb_x,
 * fb_y, sub_width, and sub_height parameters.  Pass the first two values
 * of target_viewport into ib_x and ib_y.  The full dimensions of the tile
 * should be passed into full_width and full_height. */
static void readSubImage(IceTInt fb_x, IceTInt fb_y,
                         IceTSizeType sub_width, IceTSizeType sub_height,
                         IceTImage buffer,
                         IceTInt ib_x, IceTInt ib_y,
                         IceTSizeType full_width, IceTSizeType full_height);
/* Gets a static image buffer that is shared amongst all contexts (and
 * therefore not thread safe.  The buffer is resized as necessary. */
static IceTImage getInternalSharedImage(IceTEnum color_format,
                                        IceTEnum depth_format,
                                        IceTSizeType pixels);
/* Releases use of the buffers retreived with getInternalSharedImage.  Currently
 * does nothing as thread safty is not ensured. */
static void releaseInternalSharedImage(void);

static IceTSizeType colorPixelSize(IceTEnum color_format)
{
    switch (color_format) {
      case ICET_IMAGE_COLOR_RGBA_UBYTE: return 4;
      case ICET_IMAGE_COLOR_RGBA_FLOAT: return 4*sizeof(IceTFloat);
      case ICET_IMAGE_COLOR_NONE:       return 0;
      default:
          icetRaiseError("Invalid color format.", ICET_INVALID_ENUM);
          return 0;
    }
}

static IceTSizeType depthPixelSize(IceTEnum depth_format)
{
    switch (depth_format) {
      case ICET_IMAGE_DEPTH_FLOAT: return 4*sizeof(IceTFloat);
      case ICET_IMAGE_DEPTH_NONE:  return 0;
      default:
          icetRaiseError("Invalid depth format.", ICET_INVALID_ENUM);
          return 0;
    }
}

IceTSizeType icetImageBufferSize(IceTEnum color_format,
                                 IceTEnum depth_format,
                                 IceTSizeType num_pixels)
{
    IceTSizeType color_pixel_size = colorPixelSize(color_format);
    IceTSizeType depth_pixel_size = depthPixelSize(depth_format);

    return (  ICET_IMAGE_DATA_START_INDEX*sizeof(IceTUInt)
            + num_pixels*(color_pixel_size + depth_pixel_size) );
}
IceTSizeType icetImageMaxBufferSize(IceTSizeType num_pixels)
{
  /* Maximum color and depth formats are ICET_IMAGE_COLOR_RGBA_FLOAT and
     ICET_IMAGE_DEPTH_FLOAT, respectively. */
    return icetImageBufferSize(ICET_IMAGE_COLOR_RGBA_FLOAT,
                               ICET_IMAGE_DEPTH_FLOAT,
                               num_pixels);
}

IceTSizeType icetSparseImageBufferSize(IceTEnum color_format,
                                       IceTEnum depth_format,
                                       IceTSizeType num_pixels)
{
  /* Sparse images are designed to never take more than the same size of a
     full image plus the space of 2 run lengths.  This occurs when there are
     no inactive pixels (hence all data is stored plus 2 run lengths to tell
     us that).  Even in the pathalogical case where every run length is 1,
     we are still never any more than that because the 2 active/inactive run
     lengths are packed into 2-bit shorts, which total takes no more space
     than a color or depth value for a single pixel. */
    return (  2*sizeof(IceTUShort)
            + icetImageBufferSize(color_format, depth_format, num_pixels) );
}
IceTSizeType icetSparseImageMaxBufferSize(IceTSizeType num_pixels)
{
    return (2*sizeof(IceTUShort) + icetImageMaxBufferSize(num_pixels));
}

IceTImage icetImageInitialize(IceTVoid *buffer,
                              IceTEnum color_format, IceTEnum depth_format,
                              IceTSizeType num_pixels)
{
    IceTImage image;
    image.opaque_internals = buffer;

    if (buffer == NULL) {
        if (   (color_format == ICET_IMAGE_COLOR_NONE)
            && (depth_format == ICET_IMAGE_DEPTH_NONE)
            && (num_pixels == 0) ) {
          /* Special case: allow NULL pointer to empty images. */
            return image;
        } else {
            icetRaiseError("Tried to create non-empty image with NULL buffer.",
                           ICET_INVALID_VALUE);
            return image;
        }
    }

    IceTUInt *header = ICET_IMAGE_HEADER(image);

    if (   (color_format != ICET_IMAGE_COLOR_RGBA_UBYTE)
        && (color_format != ICET_IMAGE_COLOR_RGBA_FLOAT)
        && (color_format != ICET_IMAGE_COLOR_NONE) ) {
        icetRaiseError("Invalid color format.", ICET_INVALID_ENUM);
        color_format = ICET_IMAGE_COLOR_NONE;
    }
    if (   (depth_format != ICET_IMAGE_DEPTH_FLOAT)
        && (depth_format != ICET_IMAGE_DEPTH_NONE) ) {
        icetRaiseError("Invalid depth format.", ICET_INVALID_ENUM);
        depth_format = ICET_IMAGE_DEPTH_NONE;
    }

    header[ICET_IMAGE_MAGIC_NUM_INDEX]          = ICET_IMAGE_MAGIC_NUM;
    header[ICET_IMAGE_COLOR_FORMAT_INDEX]       = color_format;
    header[ICET_IMAGE_DEPTH_FORMAT_INDEX]       = depth_format;
    header[ICET_IMAGE_SIZE_INDEX]               = num_pixels;
    header[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX]
        = icetImageBufferSize(color_format, depth_format, num_pixels);

    return image;
}

IceTSparseImage icetSparseImageInitialize(IceTVoid *buffer,
                                          IceTEnum color_format,
                                          IceTEnum depth_format,
                                          IceTSizeType num_pixels)
{
    IceTSparseImage image;
    image.opaque_internals = buffer;

    if (buffer == NULL) {
        if (   (color_format == ICET_IMAGE_COLOR_NONE)
            && (depth_format == ICET_IMAGE_DEPTH_NONE)
            && (num_pixels == 0) ) {
          /* Special case: allow NULL pointer to empty images. */
            return image;
        } else {
            icetRaiseError("Tried to create non-empty image with NULL buffer.",
                           ICET_INVALID_VALUE);
            return image;
        }
    }

    IceTUInt *header = ICET_IMAGE_HEADER(image);

    if (   (color_format != ICET_IMAGE_COLOR_RGBA_UBYTE)
        && (color_format != ICET_IMAGE_COLOR_RGBA_FLOAT)
        && (color_format != ICET_IMAGE_COLOR_NONE) ) {
        icetRaiseError("Invalid color format.", ICET_INVALID_ENUM);
        color_format = ICET_IMAGE_COLOR_NONE;
    }
    if (   (depth_format != ICET_IMAGE_DEPTH_FLOAT)
        && (depth_format != ICET_IMAGE_DEPTH_NONE) ) {
        icetRaiseError("Invalid depth format.", ICET_INVALID_ENUM);
        depth_format = ICET_IMAGE_DEPTH_NONE;
    }

    header[ICET_IMAGE_MAGIC_NUM_INDEX]          = ICET_IMAGE_MAGIC_NUM;
    header[ICET_IMAGE_COLOR_FORMAT_INDEX]       = color_format;
    header[ICET_IMAGE_DEPTH_FORMAT_INDEX]       = depth_format;
    header[ICET_IMAGE_SIZE_INDEX]               = num_pixels;
    header[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX] = 0;

    return image;
}

IceTEnum icetImageGetColorFormat(const IceTImage image)
{
    if (!image.opaque_internals) return ICET_IMAGE_COLOR_NONE;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_COLOR_FORMAT_INDEX];
}
IceTEnum icetImageGetDepthFormat(const IceTImage image)
{
    if (!image.opaque_internals) return ICET_IMAGE_DEPTH_NONE;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_DEPTH_FORMAT_INDEX];
}
IceTSizeType icetImageGetSize(const IceTImage image)
{
    if (!image.opaque_internals) return 0;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_SIZE_INDEX];
}

IceTEnum icetSparseImageGetColorFormat(const IceTSparseImage image)
{
    if (!image.opaque_internals) return ICET_IMAGE_COLOR_NONE;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_COLOR_FORMAT_INDEX];
}
IceTEnum icetSparseImageGetDepthFormat(const IceTSparseImage image)
{
    if (!image.opaque_internals) return ICET_IMAGE_DEPTH_NONE;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_DEPTH_FORMAT_INDEX];
}
IceTSizeType icetSparseImageGetSize(const IceTSparseImage image)
{
    if (!image.opaque_internals) return 0;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_SIZE_INDEX];
}
IceTSizeType icetSparseImageGetCompressedBufferSize(
                                             const IceTSparseImage image)
{
    if (!image.opaque_internals) return 0;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX];
}

IceTVoid *icetImageGetColor(IceTImage image)
{
    return ICET_IMAGE_DATA(image);
}
IceTUByte *icetImageGetColorUByte(IceTImage image)
{
    IceTEnum color_format = icetImageGetColorFormat(image);

    if (color_format != ICET_IMAGE_COLOR_RGBA_UBYTE) {
        icetRaiseError("Color format is not of type ubyte.",
                       ICET_INVALID_OPERATION);
        return NULL;
    }

    return icetImageGetColor(image);
}
IceTUInt *icetImageGetColorUInt(IceTImage image)
{
    return (IceTUInt *)icetImageGetColorUByte(image);
}
IceTFloat *icetImageGetColorFloat(IceTImage image)
{
    IceTEnum color_format = icetImageGetColorFormat(image);

    if (color_format != ICET_IMAGE_COLOR_RGBA_FLOAT) {
        icetRaiseError("Color format is not of type float.",
                       ICET_INVALID_OPERATION);
        return NULL;
    }

    return icetImageGetColor(image);
}

IceTVoid *icetImageGetDepth(IceTImage image)
{
    IceTEnum color_format = icetImageGetColorFormat(image);
    IceTSizeType color_format_bytes;

    color_format_bytes = (  icetImageGetSize(image)
                          * colorPixelSize(color_format) );

    return ICET_IMAGE_DATA(image) + color_format_bytes;
}
IceTFloat *icetImageGetDepthFloat(IceTImage image)
{
    IceTEnum depth_format = icetImageGetDepthFormat(image);

    if (depth_format != ICET_IMAGE_DEPTH_FLOAT) {
        icetRaiseError("Depth format is not of type float.",
                       ICET_INVALID_OPERATION);
        return NULL;
    }

    return icetImageGetDepth(image);
}

void icetImageCopyColorUByte(const IceTImage image,
                             IceTUByte *color_buffer,
                             IceTEnum out_color_format)
{
    IceTEnum in_color_format = icetImageGetColorFormat(image);

    if (out_color_format != ICET_IMAGE_COLOR_RGBA_UBYTE) {
        icetRaiseError("Color format is not of type ubyte.",
                       ICET_INVALID_OPERATION);
        return;
    }
    if (in_color_format == ICET_IMAGE_COLOR_NONE) {
        icetRaiseError("Input image has no color data.",
                       ICET_INVALID_OPERATION);
        return;
    }

    if (in_color_format == out_color_format) {
        const IceTUByte *in_buffer
            = icetImageGetColorUByte((IceTImage)image);
        IceTSizeType color_format_bytes = (  icetImageGetSize(image)
                                           * colorPixelSize(in_color_format) );
        memcpy(color_buffer, in_buffer, color_format_bytes);
    } else { /* in_color_format == ICET_IMAGE_COLOR_RGBA_FLOAT
                out_color_format == ICET_IMAGE_COLOR_RGBA_UBYTE */
        const IceTFloat *in_buffer
            = icetImageGetColorFloat((IceTImage)image);
        IceTSizeType num_pixels = icetImageGetSize(image);
        IceTSizeType i;
        const IceTFloat *in;
        IceTUByte *out;
        for (i = 0, in = in_buffer, out = color_buffer; i < 4*num_pixels;
             i++, in++, out++) {
            out[0] = (IceTUByte)(255*in[0]);
        }
    }
}

void icetImageCopyColorFloat(const IceTImage image,
                             IceTFloat *color_buffer,
                             IceTEnum out_color_format)
{
    IceTEnum in_color_format = icetImageGetColorFormat(image);

    if (out_color_format != ICET_IMAGE_COLOR_RGBA_FLOAT) {
        icetRaiseError("Color format is not of type float.",
                       ICET_INVALID_OPERATION);
        return;
    }
    if (in_color_format == ICET_IMAGE_COLOR_NONE) {
        icetRaiseError("Input image has no color data.",
                       ICET_INVALID_OPERATION);
        return;
    }

    if (in_color_format == out_color_format) {
        const IceTFloat *in_buffer
            = icetImageGetColorFloat((IceTImage)image);
        IceTSizeType color_format_bytes = (  icetImageGetSize(image)
                                           * colorPixelSize(in_color_format) );
        memcpy(color_buffer, in_buffer, color_format_bytes);
    } else { /* in_color_format == ICET_IMAGE_COLOR_RGBA_UBYTE
                out_color_format == ICET_IMAGE_COLOR_RGBA_FLOAT */
        const IceTUByte *in_buffer
            = icetImageGetColorUByte((IceTImage)image);
        IceTSizeType num_pixels = icetImageGetSize(image);
        IceTSizeType i;
        const IceTUByte *in;
        IceTFloat *out;
        for (i = 0, in = in_buffer, out = color_buffer; i < 4*num_pixels;
             i++, in++, out++) {
            out[0] = (IceTFloat)in[0]/255.0f;
        }
    }
}

void icetImageCopyDepthFloat(const IceTImage image,
                             IceTFloat *depth_buffer,
                             IceTEnum out_depth_format)
{
    IceTEnum in_depth_format = icetImageGetDepthFormat(image);

    if (out_depth_format != ICET_IMAGE_DEPTH_FLOAT) {
        icetRaiseError("Depth format is not of type float.",
                       ICET_INVALID_OPERATION);
        return;
    }
    if (in_depth_format == ICET_IMAGE_DEPTH_NONE) {
        icetRaiseError("Input image has no depth data.",
                       ICET_INVALID_OPERATION);
        return;
    }

  /* Currently only possibility is
     in_color_format == out_color_format == ICET_IMAGE_DEPTH_FLOAT. */
    const IceTFloat *in_buffer
        = icetImageGetDepthFloat((IceTImage)image);
    IceTSizeType depth_format_bytes = (  icetImageGetSize(image)
                                       * depthPixelSize(in_depth_format) );
    memcpy(depth_buffer, in_buffer, depth_format_bytes);
}

void icetImageCopyPixels(const IceTImage in_image, IceTSizeType in_offset,
                         IceTImage out_image, IceTSizeType out_offset,
                         IceTSizeType num_pixels)
{
    IceTEnum color_format, depth_format;

    color_format = icetImageGetColorFormat(in_image);
    depth_format = icetImageGetDepthFormat(in_image);
    if (   (color_format != icetImageGetColorFormat(out_image))
        || (depth_format != icetImageGetDepthFormat(out_image)) ) {
        icetRaiseError("Cannot copy pixels of images with different formats.",
                       ICET_INVALID_VALUE);
        return;
    }

    if (    (in_offset < 0)
         || (in_offset + num_pixels > icetImageGetSize(in_image)) ) {
        icetRaiseError("Pixels to copy are outside of range of source image.",
                       ICET_INVALID_VALUE);
    }
    if (    (out_offset < 0)
         || (out_offset + num_pixels > icetImageGetSize(out_image)) ) {
        icetRaiseError("Pixels to copy are outside of range of source image.",
                       ICET_INVALID_VALUE);
    }

    if (color_format != ICET_IMAGE_COLOR_NONE) {
        const IceTVoid *in_colors = icetImageGetColor(in_image);
        IceTVoid *out_colors = icetImageGetDepth(out_image);
        IceTSizeType pixel_size = colorPixelSize(color_format);
        memcpy(out_colors + pixel_size*out_offset,
               in_colors + pixel_size*in_offset,
               pixel_size*num_pixels);
    }

    if (depth_format != ICET_IMAGE_DEPTH_NONE) {
        const IceTVoid *in_depths = icetImageGetDepth(in_image);
        IceTVoid *out_depths = icetImageGetDepth(out_image);
        IceTSizeType pixel_size = depthPixelSize(depth_format);
        memcpy(out_depths + pixel_size*out_offset,
               in_depths + pixel_size*in_offset,
               pixel_size*num_pixels);
    }
}

void icetImagePackageForSend(IceTImage image,
                             IceTVoid **buffer, IceTSizeType *size)
{
    *buffer = image.opaque_internals;
    *size = ICET_IMAGE_HEADER(image)[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX];
}

IceTImage icetImageUnpackageFromReceive(IceTVoid *buffer)
{
    IceTImage image;
    IceTEnum color_format, depth_format;

    image.opaque_internals = buffer;

  /* Check the image for validity. */
    if (    ICET_IMAGE_HEADER(image)[ICET_IMAGE_MAGIC_NUM_INDEX]
         != ICET_IMAGE_MAGIC_NUM ) {
        icetRaiseError("Invalid image buffer: no magic number.",
                       ICET_INVALID_VALUE);
        image.opaque_internals = NULL;
        return image;
    }

    color_format = icetImageGetColorFormat(image);
    if (    (color_format != ICET_IMAGE_COLOR_RGBA_UBYTE)
         && (color_format != ICET_IMAGE_COLOR_RGBA_FLOAT)
         && (color_format != ICET_IMAGE_COLOR_NONE) ) {
        icetRaiseError("Invalid image buffer: invalid color format.",
                       ICET_INVALID_VALUE);
        image.opaque_internals = NULL;
        return image;
    }

    depth_format = icetImageGetDepthFormat(image);
    if (    (depth_format != ICET_IMAGE_DEPTH_FLOAT)
         && (depth_format != ICET_IMAGE_DEPTH_NONE) ) {
        icetRaiseError("Invalid image buffer: invalid depth format.",
                       ICET_INVALID_VALUE);
        image.opaque_internals = NULL;
        return image;
    }

    if (    icetImageBufferSize(color_format, depth_format,
                                icetImageGetSize(image))
         != ICET_IMAGE_HEADER(image)[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX] ) {
        icetRaiseError("Inconsistent sizes in image data.", ICET_INVALID_VALUE);
        image.opaque_internals = NULL;
        return image;
    }

  /* The image is valid (as far as we can tell). */
    return image;
}

void icetSparseImagePackageForSend(IceTSparseImage image,
                                   IceTVoid **buffer, IceTSizeType *size)
{
    *buffer = image.opaque_internals;
    *size = ICET_IMAGE_HEADER(image)[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX];
}

IceTSparseImage icetSparseImageUnpackageFromReceive(IceTVoid *buffer)
{
    IceTSparseImage image;
    IceTEnum color_format, depth_format;

    image.opaque_internals = buffer;

  /* Check the image for validity. */
    if (    ICET_IMAGE_HEADER(image)[ICET_IMAGE_MAGIC_NUM_INDEX]
         != ICET_SPARSE_IMAGE_MAGIC_NUM ) {
        icetRaiseError("Invalid image buffer: no magic number.",
                       ICET_INVALID_VALUE);
        image.opaque_internals = NULL;
        return image;
    }

    color_format = icetSparseImageGetColorFormat(image);
    if (    (color_format != ICET_IMAGE_COLOR_RGBA_UBYTE)
         && (color_format != ICET_IMAGE_COLOR_RGBA_FLOAT)
         && (color_format != ICET_IMAGE_COLOR_NONE) ) {
        icetRaiseError("Invalid image buffer: invalid color format.",
                       ICET_INVALID_VALUE);
        image.opaque_internals = NULL;
        return image;
    }

    depth_format = icetSparseImageGetDepthFormat(image);
    if (    (depth_format != ICET_IMAGE_DEPTH_FLOAT)
         && (depth_format != ICET_IMAGE_DEPTH_NONE) ) {
        icetRaiseError("Invalid image buffer: invalid depth format.",
                       ICET_INVALID_VALUE);
        image.opaque_internals = NULL;
        return image;
    }

    if (   icetSparseImageBufferSize(color_format, depth_format,
                                     icetSparseImageGetSize(image))
         > ICET_IMAGE_HEADER(image)[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX] ) {
        icetRaiseError("Inconsistent sizes in image data.", ICET_INVALID_VALUE);
        image.opaque_internals = NULL;
        return image;
    }

  /* The image is valid (as far as we can tell). */
    return image;
}

void icetClearImage(IceTImage image)
{
    IceTUInt *ip;
    IceTFloat *fp;
    IceTSizeType pixels = icetImageGetSize(image);
    IceTSizeType i;
    IceTUInt i_bg_color;
    IceTFloat f_bg_color[4];

    icetGetIntegerv(ICET_BACKGROUND_COLOR_WORD, (IceTInt *)&i_bg_color);
    icetGetFloatv(ICET_BACKGROUND_COLOR, f_bg_color);

    switch (icetImageGetColorFormat(image)) {
      case ICET_IMAGE_COLOR_RGBA_UBYTE:
          for (i = 0, ip = icetImageGetColorUInt(image); i<pixels; i++, ip++) {
              ip[0] = i_bg_color;
          }
          break;
      case ICET_IMAGE_COLOR_RGBA_FLOAT:
          for (i = 0, fp = icetImageGetColorFloat(image); i<pixels;
               i++, fp += 4) {
              fp[0] = f_bg_color[0];
              fp[1] = f_bg_color[1];
              fp[2] = f_bg_color[2];
              fp[3] = f_bg_color[3];
          }
          break;
      case ICET_IMAGE_COLOR_NONE:
        /* Nothing to do. */
          break;
    }

    switch (icetImageGetDepthFormat(image)) {
      case ICET_IMAGE_DEPTH_FLOAT:
          for (i = 0, fp = icetImageGetDepthFloat(image); i<pixels; i++, fp++) {
              fp[0] = 1.0;
          }
          break;
      case ICET_IMAGE_DEPTH_NONE:
        /* Nothing to do. */
          break;
    }
}

IceTImage icetGetTileImage(IceTInt tile, IceTVoid *buffer)
{
    IceTInt screen_viewport[4], target_viewport[4];
    IceTInt *viewports;
    IceTSizeType width, height;
    IceTEnum color_format, depth_format;
    IceTImage image;

    viewports = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS);
    width = viewports[4*tile+2];
    height = viewports[4*tile+3];

    icetGetEnumv(ICET_GL_COLOR_FORMAT, &color_format);
    icetGetEnumv(ICET_GL_DEPTH_FORMAT, &depth_format);
    image = icetImageInitialize(buffer, color_format, depth_format,
                                width*height);

    renderTile(tile, screen_viewport, target_viewport);

    readSubImage(screen_viewport[0], screen_viewport[1],
                 screen_viewport[2], screen_viewport[3],
                 image, target_viewport[0], target_viewport[1],
                 width, height);

    return image;
}

IceTSparseImage icetGetCompressedTileImage(IceTInt tile, IceTVoid *buffer)
{
    IceTInt screen_viewport[4], target_viewport[4];
    IceTImage full_image;
    IceTSparseImage compressed_image;
    IceTInt *viewports;
    IceTSizeType width, height;
    int space_left, space_right, space_bottom, space_top;
    IceTEnum color_format, depth_format;

    viewports = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS);
    width = viewports[4*tile+2];
    height = viewports[4*tile+3];

    renderTile(tile, screen_viewport, target_viewport);

    icetGetEnumv(ICET_GL_COLOR_FORMAT, &color_format);
    icetGetEnumv(ICET_GL_DEPTH_FORMAT, &depth_format);

    full_image = getInternalSharedImage(color_format, depth_format,
                                        screen_viewport[2]*screen_viewport[3]);

    readImage(screen_viewport[0], screen_viewport[1],
              screen_viewport[2], screen_viewport[3],
              full_image);

    compressed_image = icetSparseImageInitialize(buffer, color_format,
                                                 depth_format, width*height);

    space_left = target_viewport[0];
    space_right = width - target_viewport[2] - space_left;
    space_bottom = target_viewport[1];
    space_top = height - target_viewport[3] - space_bottom;

#define INPUT_IMAGE             full_image
#define OUTPUT_SPARSE_IMAGE     compressed_image
#define PADDING
#define SPACE_BOTTOM            space_bottom
#define SPACE_TOP               space_top
#define SPACE_LEFT              space_left
#define SPACE_RIGHT             space_right
#define FULL_WIDTH              width
#define FULL_HEIGHT             height
#include "compress_func_body.h"

    releaseInternalSharedImage();

    return compressed_image;
}

IceTSparseImage icetCompressImage(const IceTImage image,
                                  IceTVoid *compressedBuffer)
{
    return icetCompressSubImage(image, 0, icetImageGetSize(image),
                                compressedBuffer);
}

IceTSparseImage icetCompressSubImage(const IceTImage image,
                                     IceTSizeType offset, IceTSizeType pixels,
                                     IceTVoid *compressedBuffer)
{
    IceTSparseImage compressedImage
        = icetSparseImageInitialize(compressedBuffer,
                                    icetImageGetColorFormat(image),
                                    icetImageGetDepthFormat(image),
                                    pixels);

#define INPUT_IMAGE             image
#define OUTPUT_SPARSE_IMAGE     compressedImage
#define OFFSET                  offset
#define PIXEL_COUNT             pixels
#include "compress_func_body.h"

    return compressedImage;
}

IceTImage icetDecompressImage(const IceTSparseImage compressedImage,
                              IceTVoid *imageBuffer)
{
    IceTImage image
        = icetImageInitialize(imageBuffer,
                              icetSparseImageGetColorFormat(compressedImage),
                              icetSparseImageGetDepthFormat(compressedImage),
                              icetSparseImageGetSize(compressedImage));

#define INPUT_SPARSE_IMAGE      compressedImage
#define OUTPUT_IMAGE            image
#define TIME_DECOMPRESSION
#include "decompress_func_body.h"

    return image;
}


void icetComposite(IceTImage destBuffer, const IceTImage srcBuffer,
                   int srcOnTop)
{
    IceTSizeType pixels;
    IceTSizeType i;
    IceTEnum composite_mode;
    IceTEnum color_format, depth_format;
    IceTDouble timer;
    IceTDouble *compare_time;

    pixels = icetImageGetSize(destBuffer);
    if (pixels != icetImageGetSize(srcBuffer)) {
        icetRaiseError("Source and destination sizes don't match.",
                       ICET_SANITY_CHECK_FAIL);
        return;
    }

    color_format = icetImageGetColorFormat(destBuffer);
    depth_format = icetImageGetDepthFormat(destBuffer);

    if (   (color_format != icetImageGetColorFormat(srcBuffer))
        || (depth_format != icetImageGetDepthFormat(srcBuffer)) ) {
        icetRaiseError("Source and destination types don't match.",
                       ICET_SANITY_CHECK_FAIL);
        return;
    }

    icetGetEnumv(ICET_COMPOSITE_MODE, &composite_mode);

    compare_time = icetUnsafeStateGetDouble(ICET_COMPARE_TIME);
    timer = icetWallTime();

    if (composite_mode == ICET_COMPOSITE_MODE_Z_BUFFER) {
        if (depth_format == ICET_IMAGE_DEPTH_FLOAT) {
            const IceTFloat *srcDepthBuffer = icetImageGetDepthFloat(srcBuffer);
            IceTFloat *destDepthBuffer = icetImageGetDepthFloat(destBuffer);

            if (color_format == ICET_IMAGE_COLOR_RGBA_UBYTE) {
                const IceTUInt *srcColorBuffer=icetImageGetColorUInt(srcBuffer);
                IceTUInt *destColorBuffer = icetImageGetColorUInt(destBuffer);
                for (i = 0; i < pixels; i++) {
                    if (srcDepthBuffer[i] < destDepthBuffer[i]) {
                        destDepthBuffer[i] = srcDepthBuffer[i];
                        destColorBuffer[i] = srcColorBuffer[i];
                    }
                }
            } else if (color_format == ICET_IMAGE_COLOR_RGBA_FLOAT) {
                const IceTFloat *srcColorBuffer
                    = icetImageGetColorFloat(srcBuffer);
                IceTFloat *destColorBuffer = icetImageGetColorFloat(destBuffer);
                for (i = 0; i < pixels; i++) {
                    if (srcDepthBuffer[i] < destDepthBuffer[i]) {
                        destDepthBuffer[i] = srcDepthBuffer[i];
                        destColorBuffer[4*i+0] = srcColorBuffer[4*i+0];
                        destColorBuffer[4*i+1] = srcColorBuffer[4*i+1];
                        destColorBuffer[4*i+2] = srcColorBuffer[4*i+2];
                        destColorBuffer[4*i+3] = srcColorBuffer[4*i+3];
                    }
                }
            } else if (color_format == ICET_IMAGE_COLOR_NONE) {
                for (i = 0; i < pixels; i++) {
                    if (srcDepthBuffer[i] < destDepthBuffer[i]) {
                        destDepthBuffer[i] = srcDepthBuffer[i];
                    }
                }
            } else {
                icetRaiseError("Encountered invalid color format.",
                               ICET_SANITY_CHECK_FAIL);
            }
        } else if (depth_format == ICET_IMAGE_DEPTH_NONE) {
            icetRaiseError("Cannot use Z buffer compositing operation with no"
                           " Z buffer.", ICET_INVALID_OPERATION);
        } else {
            icetRaiseError("Encountered invalid depth format.",
                           ICET_SANITY_CHECK_FAIL);
        }
    } else if (composite_mode == ICET_COMPOSITE_MODE_BLEND) {
        if (depth_format != ICET_IMAGE_DEPTH_NONE) {
            icetRaiseWarning("Z buffer ignored during blend composite"
                             " operation.  Output z buffer meaningless.",
                             ICET_INVALID_VALUE);
        }
        if (color_format == ICET_IMAGE_COLOR_RGBA_UBYTE) {
            const IceTUByte *srcColorBuffer = icetImageGetColorUByte(srcBuffer);
            IceTUByte *destColorBuffer = icetImageGetColorUByte(destBuffer);
            if (srcOnTop) {
                for (i = 0; i < pixels; i++) {
                    ICET_OVER_UBYTE(srcColorBuffer + i*4,
                                    destColorBuffer + i*4);
                }
            } else {
                for (i = 0; i < pixels; i++) {
                    ICET_UNDER_UBYTE(srcColorBuffer + i*4,
                                     destColorBuffer + i*4);
                }
            }
        } else if (color_format == ICET_IMAGE_COLOR_RGBA_FLOAT) {
            const IceTFloat *srcColorBuffer = icetImageGetColorFloat(srcBuffer);
            IceTFloat *destColorBuffer = icetImageGetColorFloat(destBuffer);
            if (srcOnTop) {
                for (i = 0; i < pixels; i++) {
                    ICET_OVER_FLOAT(srcColorBuffer + i*4,
                                    destColorBuffer + i*4);
                }
            } else {
                for (i = 0; i < pixels; i++) {
                    ICET_UNDER_FLOAT(srcColorBuffer + i*4,
                                     destColorBuffer + i*4);
                }
            }
        } else if (color_format == ICET_IMAGE_COLOR_NONE) {
            icetRaiseWarning("Compositing image with no data.",
                             ICET_INVALID_OPERATION);
        } else {
            icetRaiseError("Encountered invalid color format.",
                           ICET_SANITY_CHECK_FAIL);
        }
    } else {
        icetRaiseError("Encountered invalid composite mode.",
                       ICET_SANITY_CHECK_FAIL);
    }

    *compare_time += icetWallTime() - timer;
}

void icetCompressedComposite(IceTImage destBuffer,
                             const IceTSparseImage srcBuffer,
                             int srcOnTop)
{
    icetCompressedSubComposite(destBuffer, 0, icetImageGetSize(destBuffer),
                               srcBuffer, srcOnTop);
}
void icetCompressedSubComposite(IceTImage destBuffer,
                                IceTUInt offset, IceTUInt pixels,
                                const IceTSparseImage srcBuffer,
                                int srcOnTop)
{
    IceTDouble timer;
    IceTDouble *compare_time;

    compare_time = icetUnsafeStateGetDouble(ICET_COMPARE_TIME);
    timer = icetWallTime();

    if (srcOnTop) {
#define INPUT_SPARSE_IMAGE      srcBuffer
#define OUTPUT_IMAGE            destBuffer
#define OFFSET                  offset
#define PIXEL_COUNT             pixels
#define COMPOSITE
#define BLEND_RGBA_UBYTE        ICET_OVER_UBYTE
#define BLEND_RGBA_FLOAT        ICET_OVER_FLOAT
#include "decompress_func_body.h"
    } else {
#define INPUT_SPARSE_IMAGE      srcBuffer
#define OUTPUT_IMAGE            destBuffer
#define OFFSET                  offset
#define PIXEL_COUNT             pixels
#define COMPOSITE
#define BLEND_RGBA_UBYTE        ICET_UNDER_UBYTE
#define BLEND_RGBA_FLOAT        ICET_UNDER_FLOAT
#include "decompress_func_body.h"
    }
}

/* Makes sure that all the information for the current tile is rendered and
   currently available in the given OpenGL frame buffers.  Returns a
   viewport in the screen which has the pertinent information and a
   viewport that it should be mapped to in the given tile screen space. */
static void renderTile(int tile, IceTInt *screen_viewport,
                       IceTInt *target_viewport)
{
    IceTInt *contained_viewport;
    IceTInt *tile_viewport;
    IceTBoolean *contained_mask;
    IceTInt max_width, max_height;
    IceTBoolean use_floating_viewport;
    IceTCallback drawfunc;
    IceTVoid *value;
    IceTDouble render_time;
    IceTDouble timer;

    icetRaiseDebug1("Rendering tile %d", tile);
    contained_viewport = icetUnsafeStateGetInteger(ICET_CONTAINED_VIEWPORT);
    tile_viewport = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS) + 4*tile;
    contained_mask = icetUnsafeStateGetBoolean(ICET_CONTAINED_TILES_MASK);
    use_floating_viewport = icetIsEnabled(ICET_FLOATING_VIEWPORT);

    icetGetIntegerv(ICET_PHYSICAL_RENDER_WIDTH, &max_width);
    icetGetIntegerv(ICET_PHYSICAL_RENDER_HEIGHT, &max_height);

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
#if 1
    } else if (   (contained_viewport[0] >= tile_viewport[0])
               && (contained_viewport[1] >= tile_viewport[1])
               && (   contained_viewport[2]+contained_viewport[0]
                   <= tile_viewport[2]+tile_viewport[0])
               && (   contained_viewport[3]+contained_viewport[1]
                   <= tile_viewport[3]+tile_viewport[1]) ) {
      /* Case 1: geometry fits entirely within tile. */
        IceTDouble matrix[16];
        icetRaiseDebug("Case 1: geometry fits entirely within tile.");

        icetProjectTile(tile, matrix);
        glMultMatrixd(matrix);
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
        IceTDouble matrix[16];
        icetRaiseDebug("Case 2: Can't use floating viewport.");

        icetProjectTile(tile, matrix);
        glMultMatrixd(matrix);
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
        IceTDouble matrix[16];
        IceTInt *rendered_viewport;
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
        rendered_viewport = malloc(sizeof(IceTInt)*4);
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
static void readImage(IceTInt x, IceTInt y,
                      IceTSizeType width, IceTSizeType height,
                      IceTImage buffer)
{
    readSubImage(x, y, width, height, buffer, 0, 0, width, height);
}
static void readSubImage(IceTInt fb_x, IceTInt fb_y,
                         IceTSizeType sub_width, IceTSizeType sub_height,
                         IceTImage buffer,
                         IceTInt ib_x, IceTInt ib_y,
                         IceTSizeType full_width, IceTSizeType full_height)
{
    IceTInt readBuffer;
    IceTInt colorFormat;
    IceTDouble *read_time;
    IceTDouble timer;
    IceTUByte *colorBuffer;
    IceTFloat *depthBuffer;
    IceTInt physical_viewport[4];
    IceTInt x_offset, y_offset;
    IceTUInt background_color;
    IceTInt x, y;

    icetRaiseDebug4("Reading viewport %d %d %d %d", (int)fb_x, (int)fb_y,
                    (int)sub_width, (int)sub_height);
    icetRaiseDebug2("Image offset %d %d", (int)ib_x, (int)ib_y);
    icetRaiseDebug2("Full image dimensions %d %d",
                    (int)full_width, (int)full_height);

#ifdef DEBUG
    if (   (ICET_IMAGE_HEADER(buffer)[ICET_IMAGE_MAGIC_NUM_INDEX])
        != ICET_IMAGE_MAGIC_NUM ) {
        icetRaiseError("Buffer magic number not set.", ICET_SANITY_CHECK_FAIL);
        return;
    }
    if (icetImageGetSize(buffer) != (IceTSizeType)(full_width*full_height)) {
        icetRaiseError("Buffer size was not set.", ICET_SANITY_CHECK_FAIL);
        return;
    }
#endif /* DEBUG */

  /* TODO: Handle different color formats.  Do this when moving this to
     the OpenGL layer.  Note, you will get errors when one of the buffers
     does not exist, but it should still work. */
    colorBuffer = icetImageGetColorUByte(buffer);
    depthBuffer = icetImageGetDepthFloat(buffer);

    glPixelStorei(GL_PACK_ROW_LENGTH, full_width);

  /* These pixel store parameters are not working on one of the platforms
   * I am testing on (thank you Mac).  Instead of using these, just offset
   * the buffers we read in from. */
  /* glPixelStorei(GL_PACK_SKIP_PIXELS, ib_x); */
  /* glPixelStorei(GL_PACK_SKIP_ROWS, ib_y); */

    icetGetIntegerv(ICET_GL_READ_BUFFER, &readBuffer);
    glReadBuffer(readBuffer);

    glGetIntegerv(GL_VIEWPORT, physical_viewport);
    x_offset = physical_viewport[0];
    y_offset = physical_viewport[1];

    read_time = icetUnsafeStateGetDouble(ICET_BUFFER_READ_TIME);
    timer = icetWallTime();

    if (colorBuffer != NULL) {
        icetGetIntegerv(ICET_GL_COLOR_FORMAT, &colorFormat);
        glReadPixels(fb_x + x_offset, fb_y + y_offset, sub_width, sub_height,
                     colorFormat, GL_UNSIGNED_BYTE,
                     colorBuffer + 4*(ib_x + full_width*ib_y));

        icetGetIntegerv(ICET_BACKGROUND_COLOR_WORD, (IceTInt *)&background_color);
      /* Clear out bottom. */
        for (y = 0; y < ib_y; y++) {
            for (x = 0; x < full_width; x++) {
                ((IceTUInt *)colorBuffer)[y*full_width + x] = background_color;
            }
        }
      /* Clear out left. */
        if (ib_x > 0) {
            for (y = ib_y; y < sub_height+ib_y; y++) {
                for (x = 0; x < ib_x; x++) {
                    ((IceTUInt *)colorBuffer)[y*full_width + x] =background_color;
                }
            }
        }
      /* Clear out right. */
        if (ib_x + sub_width < full_width) {
            for (y = ib_y; y < sub_height+ib_y; y++) {
                for (x = ib_x+sub_width; x < full_width; x++) {
                    ((IceTUInt *)colorBuffer)[y*full_width + x] =background_color;
                }
            }
        }
      /* Clear out top. */
        for (y = ib_y+sub_height; y < full_height; y++) {
            for (x = 0; x < full_width; x++) {
                ((IceTUInt *)colorBuffer)[y*full_width + x] = background_color;
            }
        }
    }
    if (depthBuffer != NULL) {
        glReadPixels(fb_x + x_offset, fb_y + y_offset, sub_width, sub_height,
                     GL_DEPTH_COMPONENT, GL_FLOAT,
                     depthBuffer + ib_x + full_width*ib_y);

      /* Clear out bottom. */
        for (y = 0; y < ib_y; y++) {
            for (x = 0; x < full_width; x++) {
                depthBuffer[y*full_width + x] = 1.0;
            }
        }
      /* Clear out left. */
        if (ib_x > 0) {
            for (y = ib_y; y < sub_height+ib_y; y++) {
                for (x = 0; x < ib_x; x++) {
                    depthBuffer[y*full_width + x] = 1.0;
                }
            }
        }
      /* Clear out right. */
        if (ib_x + sub_width < full_width) {
            for (y = ib_y; y < sub_height+ib_y; y++) {
                for (x = ib_x+sub_width; x < full_width; x++) {
                    depthBuffer[y*full_width + x] = 1.0;
                }
            }
        }
      /* Clear out top. */
        for (y = ib_y+sub_height; y < full_height; y++) {
            for (x = 0; x < full_width; x++) {
                depthBuffer[y*full_width + x] = 1.0;
            }
        }
    }

    *read_time += icetWallTime() - timer;

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  /* glPixelStorei(GL_PACK_SKIP_PIXELS, 0); */
  /* glPixelStorei(GL_PACK_SKIP_ROWS, 0); */
}

/* Currently not thread safe. */
static IceTImage getInternalSharedImage(IceTEnum color_format,
                                        IceTEnum depth_format,
                                        IceTSizeType pixels)
{
    static IceTVoid *buffer = NULL;
    static IceTSizeType bufferSize = 0;

    IceTSizeType newBufferSize = icetImageBufferSize(color_format, depth_format,
                                                     pixels);

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

    return icetImageInitialize(buffer, color_format, depth_format, pixels);
}

static void releaseInternalSharedImage(void)
{
  /* If we were thread safe, we would unlock the buffers. */
}

