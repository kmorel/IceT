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

#define ICET_IMAGE_MAGIC_NUM_INDEX              0
#define ICET_IMAGE_COLOR_FORMAT_INDEX           1
#define ICET_IMAGE_DEPTH_FORMAT_INDEX           2
#define ICET_IMAGE_WIDTH_INDEX                  3
#define ICET_IMAGE_HEIGHT_INDEX                 4
#define ICET_IMAGE_MAX_NUM_PIXELS_INDEX         5
#define ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX     6
#define ICET_IMAGE_DATA_START_INDEX             7

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
static IceTImage getInternalSharedImage(IceTSizeType width,
                                        IceTSizeType height);
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
      case ICET_IMAGE_DEPTH_FLOAT: return sizeof(IceTFloat);
      case ICET_IMAGE_DEPTH_NONE:  return 0;
      default:
          icetRaiseError("Invalid depth format.", ICET_INVALID_ENUM);
          return 0;
    }
}

IceTSizeType icetImageBufferSize(IceTSizeType width, IceTSizeType height)
{
    IceTEnum color_format, depth_format;

    icetGetEnumv(ICET_COLOR_FORMAT, &color_format);
    icetGetEnumv(ICET_DEPTH_FORMAT, &depth_format);

    return icetImageBufferSizeType(color_format, depth_format,
                                   width, height);
}

IceTSizeType icetImageBufferSizeType(IceTEnum color_format,
                                     IceTEnum depth_format,
                                     IceTSizeType width,
                                     IceTSizeType height)
{
    IceTSizeType color_pixel_size = colorPixelSize(color_format);
    IceTSizeType depth_pixel_size = depthPixelSize(depth_format);

    return (  ICET_IMAGE_DATA_START_INDEX*sizeof(IceTUInt)
            + width*height*(color_pixel_size + depth_pixel_size) );
}

IceTSizeType icetSparseImageBufferSize(IceTSizeType width, IceTSizeType height)
{
    IceTEnum color_format, depth_format;

    icetGetEnumv(ICET_COLOR_FORMAT, &color_format);
    icetGetEnumv(ICET_DEPTH_FORMAT, &depth_format);

    return icetSparseImageBufferSizeType(color_format, depth_format,
                                         width, height);
}

IceTSizeType icetSparseImageBufferSizeType(IceTEnum color_format,
                                           IceTEnum depth_format,
                                           IceTSizeType width,
                                           IceTSizeType height)
{
  /* Sparse images are designed to never take more than the same size of a full
     image plus the space of 2 run lengths per 0xFFFF (65,535) pixels.  This
     occurs when there are no inactive pixels (hence all data is stored plus the
     necessary run lengths, where the largest run length is 0xFFFF so it can fit
     into a 16-bit integer).  Even in the pathalogical case where every run
     length is 1, we are still never any more than that because the 2
     active/inactive run lengths are packed into 2-bit shorts, which total takes
     no more space than a color or depth value for a single pixel. */
    return (  2*sizeof(IceTUShort)*((width*height)/0xFFFF + 1)
            + icetImageBufferSizeType(color_format,depth_format,width,height) );
}

IceTImage icetImageAssignBuffer(IceTVoid *buffer,
                                IceTSizeType width,
                                IceTSizeType height)
{
    IceTImage image;
    IceTEnum color_format, depth_format;
    IceTUInt *header;

    image.opaque_internals = buffer;

    if (buffer == NULL) {
        icetRaiseError("Tried to create image with NULL buffer.",
                       ICET_INVALID_VALUE);
        return icetImageNull();
    }

    icetGetEnumv(ICET_COLOR_FORMAT, &color_format);
    icetGetEnumv(ICET_DEPTH_FORMAT, &depth_format);

    header = ICET_IMAGE_HEADER(image);

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
    header[ICET_IMAGE_WIDTH_INDEX]              = width;
    header[ICET_IMAGE_HEIGHT_INDEX]             = height;
    header[ICET_IMAGE_MAX_NUM_PIXELS_INDEX]     = width*height;
    header[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX]
        = icetImageBufferSizeType(color_format, depth_format, width, height);

    return image;
}

IceTImage icetImageNull(void)
{
    IceTImage image;
    image.opaque_internals = NULL;
    return image;
}

IceTBoolean icetImageIsNull(const IceTImage image)
{
    return (image.opaque_internals == NULL);
}

IceTSparseImage icetSparseImageAssignBuffer(IceTVoid *buffer,
                                            IceTSizeType width,
                                            IceTSizeType height)
{
    IceTSparseImage image;
    IceTEnum color_format, depth_format;
    IceTUInt *header;

    image.opaque_internals = buffer;

    if (buffer == NULL) {
        icetRaiseError("Tried to create sparse image with NULL buffer.",
                       ICET_INVALID_VALUE);
        return image;
    }

    icetGetEnumv(ICET_COLOR_FORMAT, &color_format);
    icetGetEnumv(ICET_DEPTH_FORMAT, &depth_format);

    header = ICET_IMAGE_HEADER(image);

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

    header[ICET_IMAGE_MAGIC_NUM_INDEX]          = ICET_SPARSE_IMAGE_MAGIC_NUM;
    header[ICET_IMAGE_COLOR_FORMAT_INDEX]       = color_format;
    header[ICET_IMAGE_DEPTH_FORMAT_INDEX]       = depth_format;
    header[ICET_IMAGE_WIDTH_INDEX]              = width;
    header[ICET_IMAGE_HEIGHT_INDEX]             = height;
    header[ICET_IMAGE_MAX_NUM_PIXELS_INDEX]     = width*height;
    header[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX] = 0;

  /* Make sure the runlengths are valid. */
    icetClearSparseImage(image);

    return image;
}

void icetImageAdjustForOutput(IceTImage image)
{
    IceTEnum color_format;

    if (icetIsEnabled(ICET_COMPOSITE_ONE_BUFFER)) {
        color_format = icetImageGetColorFormat(image);
        if (color_format != ICET_IMAGE_COLOR_NONE) {
          /* Set to no depth information. */
            ICET_IMAGE_HEADER(image)[ICET_IMAGE_DEPTH_FORMAT_INDEX]
                = ICET_IMAGE_DEPTH_NONE;
          /* Reset the image size (changes actual buffer size). */
            icetImageSetDimensions(image,
                                   icetImageGetWidth(image),
                                   icetImageGetHeight(image));
        }
    }
}

void icetImageAdjustForInput(IceTImage image)
{
    IceTEnum color_format, depth_format;

    icetGetEnumv(ICET_COLOR_FORMAT, &color_format);
    icetGetEnumv(ICET_DEPTH_FORMAT, &depth_format);

  /* Reset to the specified image format. */
    ICET_IMAGE_HEADER(image)[ICET_IMAGE_COLOR_FORMAT_INDEX] = color_format;
    ICET_IMAGE_HEADER(image)[ICET_IMAGE_DEPTH_FORMAT_INDEX] = depth_format;

  /* Reset the image size (changes actual buffer size). */
    icetImageSetDimensions(image,
                           icetImageGetWidth(image),
                           icetImageGetHeight(image));
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
IceTSizeType icetImageGetWidth(const IceTImage image)
{
    if (!image.opaque_internals) return 0;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_WIDTH_INDEX];
}
IceTSizeType icetImageGetHeight(const IceTImage image)
{
    if (!image.opaque_internals) return 0;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_HEIGHT_INDEX];
}
IceTSizeType icetImageGetNumPixels(const IceTImage image)
{
    if (!image.opaque_internals) return 0;
    return (  ICET_IMAGE_HEADER(image)[ICET_IMAGE_WIDTH_INDEX]
            * ICET_IMAGE_HEADER(image)[ICET_IMAGE_HEIGHT_INDEX] );
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
IceTSizeType icetSparseImageGetWidth(const IceTSparseImage image)
{
    if (!image.opaque_internals) return 0;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_WIDTH_INDEX];
}
IceTSizeType icetSparseImageGetHeight(const IceTSparseImage image)
{
    if (!image.opaque_internals) return 0;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_HEIGHT_INDEX];
}
IceTSizeType icetSparseImageGetNumPixels(const IceTSparseImage image)
{
    if (!image.opaque_internals) return 0;
    return (  ICET_IMAGE_HEADER(image)[ICET_IMAGE_WIDTH_INDEX]
            * ICET_IMAGE_HEADER(image)[ICET_IMAGE_HEIGHT_INDEX] );
}
IceTSizeType icetSparseImageGetCompressedBufferSize(
                                             const IceTSparseImage image)
{
    if (!image.opaque_internals) return 0;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX];
}

void icetImageSetDimensions(IceTImage image,
                            IceTSizeType width,
                            IceTSizeType height)
{
    if (icetImageIsNull(image)) {
        icetRaiseError("Cannot set number of pixels on null image.",
                       ICET_INVALID_VALUE);
        return;
    }

    if (   width*height
         > ICET_IMAGE_HEADER(image)[ICET_IMAGE_MAX_NUM_PIXELS_INDEX] ){
        icetRaiseError("Cannot set an image size to greater than what the"
                       " image was originally created.", ICET_INVALID_VALUE);
        return;
    }

    ICET_IMAGE_HEADER(image)[ICET_IMAGE_WIDTH_INDEX] = width;
    ICET_IMAGE_HEADER(image)[ICET_IMAGE_HEIGHT_INDEX] = height;
    ICET_IMAGE_HEADER(image)[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX]
        = icetImageBufferSizeType(icetImageGetColorFormat(image),
                                  icetImageGetDepthFormat(image),
                                  width, height);
}

void icetSparseImageSetDimensions(IceTSparseImage image,
                                  IceTSizeType width,
                                  IceTSizeType height)
{
    if (image.opaque_internals == NULL) {
        icetRaiseError("Cannot set number of pixels on null image.",
                       ICET_INVALID_VALUE);
        return;
    }

    if (   width*height
         > ICET_IMAGE_HEADER(image)[ICET_IMAGE_MAX_NUM_PIXELS_INDEX] ){
        icetRaiseError("Cannot set an image size to greater than what the"
                       " image was originally created.", ICET_INVALID_VALUE);
        return;
    }

    ICET_IMAGE_HEADER(image)[ICET_IMAGE_WIDTH_INDEX] = width;
    ICET_IMAGE_HEADER(image)[ICET_IMAGE_HEIGHT_INDEX] = height;

  /* Make sure the runlengths are valid. */
    icetClearSparseImage(image);
}

IceTVoid *icetImageGetColorVoid(IceTImage image, IceTSizeType *pixel_size)
{
    if (pixel_size) {
        IceTEnum color_format = icetImageGetColorFormat(image);
        *pixel_size = colorPixelSize(color_format);
    }

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

    return icetImageGetColorVoid(image, NULL);
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

    return icetImageGetColorVoid(image, NULL);
}

IceTVoid *icetImageGetDepthVoid(IceTImage image, IceTSizeType *pixel_size)
{
    IceTEnum color_format = icetImageGetColorFormat(image);
    IceTSizeType color_format_bytes;

    if (pixel_size) {
        IceTEnum depth_format = icetImageGetDepthFormat(image);
        *pixel_size = depthPixelSize(depth_format);
    }

    color_format_bytes = (  icetImageGetNumPixels(image)
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

    return icetImageGetDepthVoid(image, NULL);
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
        IceTSizeType color_format_bytes = (  icetImageGetNumPixels(image)
                                           * colorPixelSize(in_color_format) );
        memcpy(color_buffer, in_buffer, color_format_bytes);
    } else if (   (in_color_format == ICET_IMAGE_COLOR_RGBA_FLOAT)
               && (out_color_format == ICET_IMAGE_COLOR_RGBA_UBYTE) ) {
        const IceTFloat *in_buffer
            = icetImageGetColorFloat((IceTImage)image);
        IceTSizeType num_pixels = icetImageGetNumPixels(image);
        IceTSizeType i;
        const IceTFloat *in;
        IceTUByte *out;
        for (i = 0, in = in_buffer, out = color_buffer; i < 4*num_pixels;
             i++, in++, out++) {
            out[0] = (IceTUByte)(255*in[0]);
        }
    } else {
        icetRaiseError("Encountered unexpected color format combination.",
                       ICET_SANITY_CHECK_FAIL);
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
        IceTSizeType color_format_bytes = (  icetImageGetNumPixels(image)
                                           * colorPixelSize(in_color_format) );
        memcpy(color_buffer, in_buffer, color_format_bytes);
    } else if (   (in_color_format == ICET_IMAGE_COLOR_RGBA_UBYTE)
               && (out_color_format == ICET_IMAGE_COLOR_RGBA_FLOAT) ) {
        const IceTUByte *in_buffer
            = icetImageGetColorUByte((IceTImage)image);
        IceTSizeType num_pixels = icetImageGetNumPixels(image);
        IceTSizeType i;
        const IceTUByte *in;
        IceTFloat *out;
        for (i = 0, in = in_buffer, out = color_buffer; i < 4*num_pixels;
             i++, in++, out++) {
            out[0] = (IceTFloat)in[0]/255.0f;
        }
    } else {
        icetRaiseError("Unexpected format combination.",
                       ICET_SANITY_CHECK_FAIL);
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
    IceTSizeType depth_format_bytes = (  icetImageGetNumPixels(image)
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
         || (in_offset + num_pixels > icetImageGetNumPixels(in_image)) ) {
        icetRaiseError("Pixels to copy are outside of range of source image.",
                       ICET_INVALID_VALUE);
    }
    if (    (out_offset < 0)
         || (out_offset + num_pixels > icetImageGetNumPixels(out_image)) ) {
        icetRaiseError("Pixels to copy are outside of range of source image.",
                       ICET_INVALID_VALUE);
    }

    if (color_format != ICET_IMAGE_COLOR_NONE) {
        const IceTVoid *in_colors;
        IceTVoid *out_colors;
        IceTSizeType pixel_size;
        in_colors = icetImageGetColorVoid(in_image, &pixel_size);
        out_colors = icetImageGetColorVoid(out_image, NULL);
        memcpy(out_colors + pixel_size*out_offset,
               in_colors + pixel_size*in_offset,
               pixel_size*num_pixels);
    }

    if (depth_format != ICET_IMAGE_DEPTH_NONE) {
        const IceTVoid *in_depths;
        IceTVoid *out_depths;
        IceTSizeType pixel_size;
        in_depths = icetImageGetDepthVoid(in_image, &pixel_size);
        out_depths = icetImageGetDepthVoid(out_image, NULL);
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

    if (*size != icetImageBufferSizeType(icetImageGetColorFormat(image),
                                         icetImageGetDepthFormat(image),
                                         icetImageGetWidth(image),
                                         icetImageGetHeight(image))) {
        icetRaiseError("Inconsistent buffer size detected.",
                       ICET_SANITY_CHECK_FAIL);
    }
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

    if (    icetImageBufferSizeType(color_format, depth_format,
                                    icetImageGetWidth(image),
                                    icetImageGetHeight(image))
         != ICET_IMAGE_HEADER(image)[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX] ) {
        icetRaiseError("Inconsistent sizes in image data.", ICET_INVALID_VALUE);
        image.opaque_internals = NULL;
        return image;
    }

  /* The source may have used a bigger buffer than allocated here at the
     receiver.  Record only size that holds current image. */
    ICET_IMAGE_HEADER(image)[ICET_IMAGE_MAX_NUM_PIXELS_INDEX]
        = icetImageGetNumPixels(image);

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

    if (   icetSparseImageBufferSizeType(color_format, depth_format,
                                         icetSparseImageGetWidth(image),
                                         icetSparseImageGetHeight(image))
         < ICET_IMAGE_HEADER(image)[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX] ) {
        icetRaiseError("Inconsistent sizes in image data.", ICET_INVALID_VALUE);
        image.opaque_internals = NULL;
        return image;
    }

  /* The source may have used a bigger buffer than allocated here at the
     receiver.  Record only size that holds current image. */
    ICET_IMAGE_HEADER(image)[ICET_IMAGE_MAX_NUM_PIXELS_INDEX]
        = icetSparseImageGetNumPixels(image);

  /* The image is valid (as far as we can tell). */
    return image;
}

void icetClearImage(IceTImage image)
{
    IceTUInt *ip;
    IceTFloat *fp;
    IceTSizeType pixels = icetImageGetNumPixels(image);
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

void icetClearSparseImage(IceTSparseImage image)
{
    IceTVoid *data = ICET_IMAGE_DATA(image);
    IceTSizeType p = icetSparseImageGetNumPixels(image);

    while (p > 0xFFFF) {
        INACTIVE_RUN_LENGTH(data) = 0xFFFF;
        ACTIVE_RUN_LENGTH(data) = 0;
        data += RUN_LENGTH_SIZE;
        p -= 0xFFFF;
    }

    INACTIVE_RUN_LENGTH(data) = p;
    ACTIVE_RUN_LENGTH(data) = 0;
}

void icetSetColorFormat(IceTEnum color_format)
{
    IceTBoolean isDrawing;

    icetGetBooleanv(ICET_IS_DRAWING_FRAME, &isDrawing);
    if (isDrawing) {
        icetRaiseError("Attempted to change the color format while drawing."
                       " This probably means that you called icetSetColorFormat"
                       " in a drawing callback. You cannot do that. Call this"
                       " function before starting the draw operation.",
                       ICET_INVALID_OPERATION);
        return;
    }

    if (   (color_format == ICET_IMAGE_COLOR_RGBA_UBYTE)
        || (color_format == ICET_IMAGE_COLOR_RGBA_FLOAT)
        || (color_format == ICET_IMAGE_COLOR_NONE) ) {
        icetStateSetInteger(ICET_COLOR_FORMAT, color_format);
    } else {
        icetRaiseError("Invalid IceT color format.", ICET_INVALID_ENUM);
    }
}

void icetSetDepthFormat(IceTEnum depth_format)
{
    IceTBoolean isDrawing;

    icetGetBooleanv(ICET_IS_DRAWING_FRAME, &isDrawing);
    if (isDrawing) {
        icetRaiseError("Attempted to change the depth format while drawing."
                       " This probably means that you called icetSetDepthFormat"
                       " in a drawing callback. You cannot do that. Call this"
                       " function before starting the draw operation.",
                       ICET_INVALID_OPERATION);
        return;
    }

    if (   (depth_format == ICET_IMAGE_DEPTH_FLOAT)
        || (depth_format == ICET_IMAGE_DEPTH_NONE) ) {
        icetStateSetInteger(ICET_DEPTH_FORMAT, depth_format);
    } else {
        icetRaiseError("Invalid IceT depth format.", ICET_INVALID_ENUM);
    }
}

void icetGetTileImage(IceTInt tile, IceTImage image)
{
    IceTInt screen_viewport[4], target_viewport[4];
    IceTInt *viewports;
    IceTSizeType width, height;

    viewports = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS);
    width = viewports[4*tile+2];
    height = viewports[4*tile+3];

    renderTile(tile, screen_viewport, target_viewport);

    readSubImage(screen_viewport[0], screen_viewport[1],
                 screen_viewport[2], screen_viewport[3],
                 image, target_viewport[0], target_viewport[1],
                 width, height);
}

void icetGetCompressedTileImage(IceTInt tile, IceTSparseImage compressed_image)
{
    IceTInt screen_viewport[4], target_viewport[4];
    IceTImage raw_image;
    IceTInt *viewports;
    IceTSizeType width, height;
    int space_left, space_right, space_bottom, space_top;

    viewports = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS);
    width = viewports[4*tile+2];
    height = viewports[4*tile+3];

    renderTile(tile, screen_viewport, target_viewport);

    raw_image = getInternalSharedImage(screen_viewport[2], screen_viewport[3]);

    readImage(screen_viewport[0], screen_viewport[1],
              screen_viewport[2], screen_viewport[3],
              raw_image);

    space_left = target_viewport[0];
    space_right = width - target_viewport[2] - space_left;
    space_bottom = target_viewport[1];
    space_top = height - target_viewport[3] - space_bottom;

    icetSparseImageSetDimensions(compressed_image, width, height);

#define INPUT_IMAGE             raw_image
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
}

void icetCompressImage(const IceTImage image,
                       IceTSparseImage compressed_image)
{
    return icetCompressSubImage(image, 0, icetImageGetNumPixels(image),
                                compressed_image);
}

void icetCompressSubImage(const IceTImage image,
                          IceTSizeType offset, IceTSizeType pixels,
                          IceTSparseImage compressed_image)
{
    icetSparseImageSetDimensions(compressed_image, pixels, 1);

#define INPUT_IMAGE             image
#define OUTPUT_SPARSE_IMAGE     compressed_image
#define OFFSET                  offset
#define PIXEL_COUNT             pixels
#include "compress_func_body.h"
}

void icetDecompressImage(const IceTSparseImage compressed_image,
                         IceTImage image)
{
    icetImageSetDimensions(image,
                           icetSparseImageGetWidth(compressed_image),
                           icetSparseImageGetHeight(compressed_image));

#define INPUT_SPARSE_IMAGE      compressed_image
#define OUTPUT_IMAGE            image
#define TIME_DECOMPRESSION
#include "decompress_func_body.h"
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

    pixels = icetImageGetNumPixels(destBuffer);
    if (pixels != icetImageGetNumPixels(srcBuffer)) {
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
    if (    icetImageGetNumPixels(destBuffer)
         != icetSparseImageGetNumPixels(srcBuffer) ) {
        icetRaiseError("Size of input and output buffers do not agree.",
                       ICET_INVALID_VALUE);
    }
    icetCompressedSubComposite(destBuffer, 0, srcBuffer, srcOnTop);
}
void icetCompressedSubComposite(IceTImage destBuffer,
                                IceTUInt offset,
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
#define PIXEL_COUNT             icetSparseImageGetNumPixels(srcBuffer)
#define COMPOSITE
#define BLEND_RGBA_UBYTE        ICET_OVER_UBYTE
#define BLEND_RGBA_FLOAT        ICET_OVER_FLOAT
#include "decompress_func_body.h"
    } else {
#define INPUT_SPARSE_IMAGE      srcBuffer
#define OUTPUT_IMAGE            destBuffer
#define OFFSET                  offset
#define PIXEL_COUNT             icetSparseImageGetNumPixels(srcBuffer)
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
    IceTFloat background_color[4];

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

    icetGetFloatv(ICET_BACKGROUND_COLOR, background_color);
    glClearColor(background_color[0], background_color[1],
                 background_color[2], background_color[3]);

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
    IceTEnum color_format;
    IceTEnum depth_format;
    IceTDouble *read_time;
    IceTDouble timer;
    IceTInt physical_viewport[4];
    IceTInt x_offset, y_offset;
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
#endif /* DEBUG */

    icetImageSetDimensions(buffer, full_width, full_height);

    color_format = icetImageGetColorFormat(buffer);
    depth_format = icetImageGetDepthFormat(buffer);

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

    if (color_format == ICET_IMAGE_COLOR_RGBA_UBYTE) {
        IceTUInt *colorBuffer = icetImageGetColorUInt(buffer);
        IceTUInt background_color;
        glReadPixels(fb_x + x_offset, fb_y + y_offset, sub_width, sub_height,
                     GL_RGBA, GL_UNSIGNED_BYTE,
                     colorBuffer + (ib_x + full_width*ib_y));

        icetGetIntegerv(ICET_BACKGROUND_COLOR_WORD, (IceTInt *)&background_color);
      /* Clear out bottom. */
        for (y = 0; y < ib_y; y++) {
            for (x = 0; x < full_width; x++) {
                colorBuffer[y*full_width + x] = background_color;
            }
        }
      /* Clear out left. */
        if (ib_x > 0) {
            for (y = ib_y; y < sub_height+ib_y; y++) {
                for (x = 0; x < ib_x; x++) {
                    colorBuffer[y*full_width + x] =background_color;
                }
            }
        }
      /* Clear out right. */
        if (ib_x + sub_width < full_width) {
            for (y = ib_y; y < sub_height+ib_y; y++) {
                for (x = ib_x+sub_width; x < full_width; x++) {
                    colorBuffer[y*full_width + x] =background_color;
                }
            }
        }
      /* Clear out top. */
        for (y = ib_y+sub_height; y < full_height; y++) {
            for (x = 0; x < full_width; x++) {
                colorBuffer[y*full_width + x] = background_color;
            }
        }
    } else if (color_format == ICET_IMAGE_COLOR_RGBA_FLOAT) {
        IceTFloat *colorBuffer = icetImageGetColorFloat(buffer);
        IceTFloat background_color[4];
        glReadPixels(fb_x + x_offset, fb_y + y_offset, sub_width, sub_height,
                     GL_RGBA, GL_FLOAT,
                     colorBuffer + 4*(ib_x + full_width*ib_y));

        icetGetFloatv(ICET_BACKGROUND_COLOR, background_color);
      /* Clear out bottom. */
        for (y = 0; y < ib_y; y++) {
            for (x = 0; x < full_width; x++) {
                colorBuffer[4*(y*full_width + x) + 0] = background_color[0];
                colorBuffer[4*(y*full_width + x) + 1] = background_color[1];
                colorBuffer[4*(y*full_width + x) + 2] = background_color[2];
                colorBuffer[4*(y*full_width + x) + 3] = background_color[3];
            }
        }
      /* Clear out left. */
        if (ib_x > 0) {
            for (y = ib_y; y < sub_height+ib_y; y++) {
                for (x = 0; x < ib_x; x++) {
                    colorBuffer[4*(y*full_width + x) + 0] = background_color[0];
                    colorBuffer[4*(y*full_width + x) + 1] = background_color[1];
                    colorBuffer[4*(y*full_width + x) + 2] = background_color[2];
                    colorBuffer[4*(y*full_width + x) + 3] = background_color[3];
                }
            }
        }
      /* Clear out right. */
        if (ib_x + sub_width < full_width) {
            for (y = ib_y; y < sub_height+ib_y; y++) {
                for (x = ib_x+sub_width; x < full_width; x++) {
                    colorBuffer[4*(y*full_width + x) + 0] = background_color[0];
                    colorBuffer[4*(y*full_width + x) + 1] = background_color[1];
                    colorBuffer[4*(y*full_width + x) + 2] = background_color[2];
                    colorBuffer[4*(y*full_width + x) + 3] = background_color[3];
                }
            }
        }
      /* Clear out top. */
        for (y = ib_y+sub_height; y < full_height; y++) {
            for (x = 0; x < full_width; x++) {
                colorBuffer[4*(y*full_width + x) + 0] = background_color[0];
                colorBuffer[4*(y*full_width + x) + 1] = background_color[1];
                colorBuffer[4*(y*full_width + x) + 2] = background_color[2];
                colorBuffer[4*(y*full_width + x) + 3] = background_color[3];
            }
        }
    } else if (color_format != ICET_IMAGE_COLOR_NONE) {
        icetRaiseError("Invalid color format.", ICET_SANITY_CHECK_FAIL);
    }

    if (depth_format == ICET_IMAGE_DEPTH_FLOAT) {
        IceTFloat *depthBuffer = icetImageGetDepthFloat(buffer);;
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
    } else if (depth_format != ICET_IMAGE_DEPTH_NONE) {
        icetRaiseError("Invalid depth format.", ICET_SANITY_CHECK_FAIL);
    }

    *read_time += icetWallTime() - timer;

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  /* glPixelStorei(GL_PACK_SKIP_PIXELS, 0); */
  /* glPixelStorei(GL_PACK_SKIP_ROWS, 0); */
}

/* Currently not thread safe. */
static IceTImage getInternalSharedImage(IceTSizeType width, IceTSizeType height)
{
    static IceTVoid *buffer = NULL;
    static IceTSizeType bufferSize = 0;

    IceTSizeType newBufferSize = icetImageBufferSize(width, height);

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

    return icetImageAssignBuffer(buffer, width, height);
}

static void releaseInternalSharedImage(void)
{
  /* If we were thread safe, we would unlock the buffers. */
}

