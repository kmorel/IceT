/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include <IceTDevImage.h>

#include <IceT.h>

#include <IceTDevProjections.h>
#include <IceTDevState.h>
#include <IceTDevDiagnostics.h>

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

#define ICET_IMAGE_HEADER(image)        ((IceTInt *)image.opaque_internals)
#define ICET_IMAGE_DATA(image) \
    ((IceTVoid *)&(ICET_IMAGE_HEADER(image)[ICET_IMAGE_DATA_START_INDEX]))

#define INACTIVE_RUN_LENGTH(rl) (((IceTUShort *)(rl))[0])
#define ACTIVE_RUN_LENGTH(rl)   (((IceTUShort *)(rl))[1])
#define RUN_LENGTH_SIZE         (2*sizeof(IceTUShort))

#ifdef DEBUG
static void ICET_TEST_IMAGE_HEADER(IceTImage image)
{
    if (!icetImageIsNull(image)) {
        if (    ICET_IMAGE_HEADER(image)[ICET_IMAGE_MAGIC_NUM_INDEX]
             != ICET_IMAGE_MAGIC_NUM ) {
            icetRaiseError("Detected invalid image header.",
                           ICET_SANITY_CHECK_FAIL);
        }
    }
}
static void ICET_TEST_SPARSE_IMAGE_HEADER(IceTSparseImage image)
{
    if (    ICET_IMAGE_HEADER(image)[ICET_IMAGE_MAGIC_NUM_INDEX]
         != ICET_SPARSE_IMAGE_MAGIC_NUM ) {
        icetRaiseError("Detected invalid image header.",
                       ICET_SANITY_CHECK_FAIL);
    }
}
#else /*DEBUG*/
#define ICET_TEST_IMAGE_HEADER(image)
#define ICET_TEST_SPARSE_IMAGE_HEADER(image)
#endif /*DEBUG*/

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

/* Renders the geometry for a tile and returns an image of the rendered data.
   If IceT determines that it is most efficient to render the data directly to
   the tile projection, then screen_viewport and tile_viewport will be set to
   the same thing, which is a viewport of the valid pixels in the returned
   image.  Any pixels outside of this viewport are undefined and should be
   cleared to the background before used.  If tile_buffer is not a null image,
   that image will be used to render and be returned.  If IceT determines that
   it is most efficient to render a projection that does not exactly fit a tile,
   tile_buffer will be ignored an image with an internal buffer will be
   returned.  screen_viewport will give the offset and dimensions of the valid
   pixels in the returned buffer.  tile_viewport gives the offset and dimensions
   where these pixels reside in the tile.  (The dimensions for both will be the
   same.)  As before, pixels outside of these viewports are undefined. */
static IceTImage renderTile(int tile,
                            IceTInt *screen_viewport,
                            IceTInt *tile_viewport,
                            IceTImage tile_buffer);
/* Gets an image buffer attached to this context. */
static IceTImage getRenderBuffer(void);

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

IceTImage icetGetStateBufferImage(IceTEnum pname,
                                  IceTSizeType width,
                                  IceTSizeType height)
{
    IceTVoid *buffer;
    IceTSizeType buffer_size;

    buffer_size = icetImageBufferSize(width, height);
    buffer = icetGetStateBuffer(pname, buffer_size);

    return icetImageAssignBuffer(buffer, width, height);
}

IceTImage icetImageAssignBuffer(IceTVoid *buffer,
                                IceTSizeType width,
                                IceTSizeType height)
{
    IceTImage image;
    IceTEnum color_format, depth_format;
    IceTInt *header;

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
    header[ICET_IMAGE_WIDTH_INDEX]              = (IceTInt)width;
    header[ICET_IMAGE_HEIGHT_INDEX]             = (IceTInt)height;
    header[ICET_IMAGE_MAX_NUM_PIXELS_INDEX]     = (IceTInt)(width*height);
    header[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX]
        = (IceTInt)icetImageBufferSizeType(color_format,
                                           depth_format,
                                           width,
                                           height);

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
    if (image.opaque_internals == NULL) {
        return ICET_TRUE;
    } else {
        return ICET_FALSE;
    }
}

IceTSparseImage icetGetStateBufferSparseImage(IceTEnum pname,
                                              IceTSizeType width,
                                              IceTSizeType height)
{
    IceTVoid *buffer;
    IceTSizeType buffer_size;

    buffer_size = icetSparseImageBufferSize(width, height);
    buffer = icetGetStateBuffer(pname, buffer_size);

    return icetSparseImageAssignBuffer(buffer, width, height);
}

IceTSparseImage icetSparseImageAssignBuffer(IceTVoid *buffer,
                                            IceTSizeType width,
                                            IceTSizeType height)
{
    IceTSparseImage image;
    IceTEnum color_format, depth_format;
    IceTInt *header;

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
    header[ICET_IMAGE_WIDTH_INDEX]              = (IceTInt)width;
    header[ICET_IMAGE_HEIGHT_INDEX]             = (IceTInt)height;
    header[ICET_IMAGE_MAX_NUM_PIXELS_INDEX]     = (IceTInt)(width*height);
    header[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX] = 0;

  /* Make sure the runlengths are valid. */
    icetClearSparseImage(image);

    return image;
}

void icetImageAdjustForOutput(IceTImage image)
{
    IceTEnum color_format;

    if (icetImageIsNull(image)) return;

    ICET_TEST_IMAGE_HEADER(image);

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

    if (icetImageIsNull(image)) return;

    ICET_TEST_IMAGE_HEADER(image);

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
    ICET_TEST_IMAGE_HEADER(image);
    if (!image.opaque_internals) return ICET_IMAGE_COLOR_NONE;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_COLOR_FORMAT_INDEX];
}
IceTEnum icetImageGetDepthFormat(const IceTImage image)
{
    ICET_TEST_IMAGE_HEADER(image);
    if (!image.opaque_internals) return ICET_IMAGE_DEPTH_NONE;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_DEPTH_FORMAT_INDEX];
}
IceTSizeType icetImageGetWidth(const IceTImage image)
{
    ICET_TEST_IMAGE_HEADER(image);
    if (!image.opaque_internals) return 0;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_WIDTH_INDEX];
}
IceTSizeType icetImageGetHeight(const IceTImage image)
{
    ICET_TEST_IMAGE_HEADER(image);
    if (!image.opaque_internals) return 0;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_HEIGHT_INDEX];
}
IceTSizeType icetImageGetNumPixels(const IceTImage image)
{
    ICET_TEST_IMAGE_HEADER(image);
    if (!image.opaque_internals) return 0;
    return (  ICET_IMAGE_HEADER(image)[ICET_IMAGE_WIDTH_INDEX]
            * ICET_IMAGE_HEADER(image)[ICET_IMAGE_HEIGHT_INDEX] );
}

IceTEnum icetSparseImageGetColorFormat(const IceTSparseImage image)
{
    ICET_TEST_SPARSE_IMAGE_HEADER(image);
    if (!image.opaque_internals) return ICET_IMAGE_COLOR_NONE;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_COLOR_FORMAT_INDEX];
}
IceTEnum icetSparseImageGetDepthFormat(const IceTSparseImage image)
{
    ICET_TEST_SPARSE_IMAGE_HEADER(image);
    if (!image.opaque_internals) return ICET_IMAGE_DEPTH_NONE;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_DEPTH_FORMAT_INDEX];
}
IceTSizeType icetSparseImageGetWidth(const IceTSparseImage image)
{
    ICET_TEST_SPARSE_IMAGE_HEADER(image);
    if (!image.opaque_internals) return 0;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_WIDTH_INDEX];
}
IceTSizeType icetSparseImageGetHeight(const IceTSparseImage image)
{
    ICET_TEST_SPARSE_IMAGE_HEADER(image);
    if (!image.opaque_internals) return 0;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_HEIGHT_INDEX];
}
IceTSizeType icetSparseImageGetNumPixels(const IceTSparseImage image)
{
    ICET_TEST_SPARSE_IMAGE_HEADER(image);
    if (!image.opaque_internals) return 0;
    return (  ICET_IMAGE_HEADER(image)[ICET_IMAGE_WIDTH_INDEX]
            * ICET_IMAGE_HEADER(image)[ICET_IMAGE_HEIGHT_INDEX] );
}
IceTSizeType icetSparseImageGetCompressedBufferSize(
                                             const IceTSparseImage image)
{
    ICET_TEST_SPARSE_IMAGE_HEADER(image);
    if (!image.opaque_internals) return 0;
    return ICET_IMAGE_HEADER(image)[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX];
}

void icetImageSetDimensions(IceTImage image,
                            IceTSizeType width,
                            IceTSizeType height)
{
    ICET_TEST_IMAGE_HEADER(image);

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

    ICET_IMAGE_HEADER(image)[ICET_IMAGE_WIDTH_INDEX] = (IceTInt)width;
    ICET_IMAGE_HEADER(image)[ICET_IMAGE_HEIGHT_INDEX] = (IceTInt)height;
    ICET_IMAGE_HEADER(image)[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX]
        = (IceTInt)icetImageBufferSizeType(icetImageGetColorFormat(image),
                                           icetImageGetDepthFormat(image),
                                           width,
                                           height);
}

void icetSparseImageSetDimensions(IceTSparseImage image,
                                  IceTSizeType width,
                                  IceTSizeType height)
{
    ICET_TEST_SPARSE_IMAGE_HEADER(image);

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

    ICET_IMAGE_HEADER(image)[ICET_IMAGE_WIDTH_INDEX] = (IceTInt)width;
    ICET_IMAGE_HEADER(image)[ICET_IMAGE_HEIGHT_INDEX] = (IceTInt)height;

  /* Make sure the runlengths are valid. */
    icetClearSparseImage(image);
}

const IceTVoid *icetImageGetColorConstVoid(const IceTImage image,
                                           IceTSizeType *pixel_size)
{
    if (pixel_size) {
        IceTEnum color_format = icetImageGetColorFormat(image);
        *pixel_size = colorPixelSize(color_format);
    }

    return ICET_IMAGE_DATA(image);
}
IceTVoid *icetImageGetColorVoid(IceTImage image, IceTSizeType *pixel_size)
{
    const IceTVoid *const_buffer = icetImageGetColorConstVoid(image, pixel_size);

    /* This const cast is OK because we actually got the pointer from a
       non-const image. */
    return (IceTVoid *)const_buffer;
}

const IceTUByte *icetImageGetColorcub(const IceTImage image)
{
    IceTEnum color_format = icetImageGetColorFormat(image);

    if (color_format != ICET_IMAGE_COLOR_RGBA_UBYTE) {
        icetRaiseError("Color format is not of type ubyte.",
                       ICET_INVALID_OPERATION);
        return NULL;
    }

    return icetImageGetColorConstVoid(image, NULL);
}
IceTUByte *icetImageGetColorub(IceTImage image)
{
    const IceTUByte *const_buffer = icetImageGetColorcub(image);

    /* This const cast is OK because we actually got the pointer from a
       non-const image. */
    return (IceTUByte *)const_buffer;
}
const IceTUInt *icetImageGetColorcui(const IceTImage image)
{
    return (const IceTUInt *)icetImageGetColorcub(image);
}
IceTUInt *icetImageGetColorui(IceTImage image)
{
    return (IceTUInt *)icetImageGetColorub(image);
}
const IceTFloat *icetImageGetColorcf(const IceTImage image)
{
    IceTEnum color_format = icetImageGetColorFormat(image);

    if (color_format != ICET_IMAGE_COLOR_RGBA_FLOAT) {
        icetRaiseError("Color format is not of type float.",
                       ICET_INVALID_OPERATION);
        return NULL;
    }

    return icetImageGetColorConstVoid(image, NULL);
}
IceTFloat *icetImageGetColorf(IceTImage image)
{
    const IceTFloat *const_buffer = icetImageGetColorcf(image);

    
    /* This const cast is OK because we actually got the pointer from a
       non-const image. */
    return (IceTFloat *)const_buffer;
}

const IceTVoid *icetImageGetDepthConstVoid(const IceTImage image,
                                           IceTSizeType *pixel_size)
{
    IceTEnum color_format = icetImageGetColorFormat(image);
    IceTSizeType color_format_bytes;
    const IceTByte *image_data_pointer;

    if (pixel_size) {
        IceTEnum depth_format = icetImageGetDepthFormat(image);
        *pixel_size = depthPixelSize(depth_format);
    }

    color_format_bytes = (  icetImageGetNumPixels(image)
                          * colorPixelSize(color_format) );

    /* Cast to IceTByte to ensure pointer arithmetic is correct. */
    image_data_pointer = (const IceTByte*)ICET_IMAGE_DATA(image);

    return image_data_pointer + color_format_bytes;
}
IceTVoid *icetImageGetDepthVoid(IceTImage image, IceTSizeType *pixel_size)
{
    const IceTVoid *const_buffer =icetImageGetDepthConstVoid(image, pixel_size);

    /* This const cast is OK because we actually got the pointer from a
       non-const image. */
    return (IceTVoid *)const_buffer;  
}
const IceTFloat *icetImageGetDepthcf(const IceTImage image)
{
    IceTEnum depth_format = icetImageGetDepthFormat(image);

    if (depth_format != ICET_IMAGE_DEPTH_FLOAT) {
        icetRaiseError("Depth format is not of type float.",
                       ICET_INVALID_OPERATION);
        return NULL;
    }

    return icetImageGetDepthConstVoid(image, NULL);
}
IceTFloat *icetImageGetDepthf(IceTImage image)
{
    const IceTFloat *const_buffer = icetImageGetDepthcf(image);

    
    /* This const cast is OK because we actually got the pointer from a
       non-const image. */
    return (IceTFloat *)const_buffer;
}

void icetImageCopyColorub(const IceTImage image,
                          IceTUByte *color_buffer,
                          IceTEnum out_color_format)
{
    IceTEnum in_color_format = icetImageGetColorFormat(image);

    if (out_color_format != ICET_IMAGE_COLOR_RGBA_UBYTE) {
        icetRaiseError("Color format is not of type ubyte.",
                       ICET_INVALID_ENUM);
        return;
    }
    if (in_color_format == ICET_IMAGE_COLOR_NONE) {
        icetRaiseError("Input image has no color data.",
                       ICET_INVALID_OPERATION);
        return;
    }

    if (in_color_format == out_color_format) {
        const IceTUByte *in_buffer = icetImageGetColorcub(image);
        IceTSizeType color_format_bytes = (  icetImageGetNumPixels(image)
                                           * colorPixelSize(in_color_format) );
        memcpy(color_buffer, in_buffer, color_format_bytes);
    } else if (   (in_color_format == ICET_IMAGE_COLOR_RGBA_FLOAT)
               && (out_color_format == ICET_IMAGE_COLOR_RGBA_UBYTE) ) {
        const IceTFloat *in_buffer = icetImageGetColorcf(image);
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

void icetImageCopyColorf(const IceTImage image,
                         IceTFloat *color_buffer,
                         IceTEnum out_color_format)
{
    IceTEnum in_color_format = icetImageGetColorFormat(image);

    if (out_color_format != ICET_IMAGE_COLOR_RGBA_FLOAT) {
        icetRaiseError("Color format is not of type float.",
                       ICET_INVALID_ENUM);
        return;
    }
    if (in_color_format == ICET_IMAGE_COLOR_NONE) {
        icetRaiseError("Input image has no color data.",
                       ICET_INVALID_OPERATION);
        return;
    }

    if (in_color_format == out_color_format) {
        const IceTFloat *in_buffer = icetImageGetColorcf(image);
        IceTSizeType color_format_bytes = (  icetImageGetNumPixels(image)
                                           * colorPixelSize(in_color_format) );
        memcpy(color_buffer, in_buffer, color_format_bytes);
    } else if (   (in_color_format == ICET_IMAGE_COLOR_RGBA_UBYTE)
               && (out_color_format == ICET_IMAGE_COLOR_RGBA_FLOAT) ) {
        const IceTUByte *in_buffer = icetImageGetColorcub(image);
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

void icetImageCopyDepthf(const IceTImage image,
                         IceTFloat *depth_buffer,
                         IceTEnum out_depth_format)
{
    IceTEnum in_depth_format = icetImageGetDepthFormat(image);

    if (out_depth_format != ICET_IMAGE_DEPTH_FLOAT) {
        icetRaiseError("Depth format is not of type float.",
                       ICET_INVALID_ENUM);
        return;
    }
    if (in_depth_format == ICET_IMAGE_DEPTH_NONE) {
        icetRaiseError("Input image has no depth data.",
                       ICET_INVALID_OPERATION);
        return;
    }

  /* Currently only possibility is
     in_color_format == out_color_format == ICET_IMAGE_DEPTH_FLOAT. */
    {
        const IceTFloat *in_buffer = icetImageGetDepthcf(image);
        IceTSizeType depth_format_bytes = (  icetImageGetNumPixels(image)
                                           * depthPixelSize(in_depth_format) );
        memcpy(depth_buffer, in_buffer, depth_format_bytes);
    }
}

IceTBoolean icetImageEqual(const IceTImage image1, const IceTImage image2)
{
    return image1.opaque_internals == image2.opaque_internals;
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
        const IceTByte *in_colors;  /* Use IceTByte for pointer arithmetic */
        IceTByte *out_colors;
        IceTSizeType pixel_size;
        in_colors = icetImageGetColorVoid(in_image, &pixel_size);
        out_colors = icetImageGetColorVoid(out_image, NULL);
        memcpy(out_colors + pixel_size*out_offset,
               in_colors + pixel_size*in_offset,
               pixel_size*num_pixels);
    }

    if (depth_format != ICET_IMAGE_DEPTH_NONE) {
        const IceTByte *in_depths;  /* Use IceTByte for pointer arithmetic */
        IceTByte *out_depths;
        IceTSizeType pixel_size;
        in_depths = icetImageGetDepthVoid(in_image, &pixel_size);
        out_depths = icetImageGetDepthVoid(out_image, NULL);
        memcpy(out_depths + pixel_size*out_offset,
               in_depths + pixel_size*in_offset,
               pixel_size*num_pixels);
    }
}

void icetImageCopyRegion(const IceTImage in_image,
                         const IceTInt *in_viewport,
                         IceTImage out_image,
                         const IceTInt *out_viewport)
{
    IceTEnum color_format = icetImageGetColorFormat(in_image);
    IceTEnum depth_format = icetImageGetDepthFormat(in_image);

    if (    (color_format != icetImageGetColorFormat(out_image))
         || (depth_format != icetImageGetDepthFormat(out_image)) ) {
        icetRaiseError("icetImageCopyRegion only supports copying images"
                       " of the same format.", ICET_INVALID_VALUE);
        return;
    }

    if (    (in_viewport[2] != out_viewport[2])
         || (in_viewport[3] != out_viewport[3]) ) {
        icetRaiseError("Sizes of input and output regions must be the same.",
                       ICET_INVALID_VALUE);
        return;
    }

    if (color_format != ICET_IMAGE_COLOR_NONE) {
        IceTSizeType pixel_size;
        /* Use IceTByte for byte-based pointer arithmetic. */
        const IceTByte *src = icetImageGetColorVoid(in_image, &pixel_size);
        IceTByte *dest = icetImageGetColorVoid(out_image, &pixel_size);
        IceTSizeType y;

      /* Advance pointers up to vertical offset. */
        src  += in_viewport[1]*icetImageGetWidth(in_image)*pixel_size;
        dest += out_viewport[1]*icetImageGetWidth(out_image)*pixel_size;

      /* Advance pointers forward to horizontal offset. */
        src  += in_viewport[0]*pixel_size;
        dest += out_viewport[0]*pixel_size;

        for (y = 0; y < in_viewport[3]; y++) {
            memcpy(dest, src, in_viewport[2]*pixel_size);
            src  += icetImageGetWidth(in_image)*pixel_size;
            dest += icetImageGetWidth(out_image)*pixel_size;
        }
    }

    if (depth_format != ICET_IMAGE_DEPTH_NONE) {
        IceTSizeType pixel_size;
        /* Use IceTByte for byte-based pointer arithmetic. */
        const IceTByte *src = icetImageGetDepthVoid(in_image, &pixel_size);
        IceTByte *dest = icetImageGetDepthVoid(out_image, &pixel_size);
        IceTSizeType y;

      /* Advance pointers up to vertical offset. */
        src  += in_viewport[1]*icetImageGetWidth(in_image)*pixel_size;
        dest += out_viewport[1]*icetImageGetWidth(out_image)*pixel_size;

      /* Advance pointers forward to horizontal offset. */
        src  += in_viewport[0]*pixel_size;
        dest += out_viewport[0]*pixel_size;

        for (y = 0; y < in_viewport[3]; y++) {
            memcpy(dest, src, in_viewport[2]*pixel_size);
            src  += icetImageGetWidth(in_image)*pixel_size;
            dest += icetImageGetWidth(out_image)*pixel_size;
        }
    }
}

void icetImageClearAroundRegion(IceTImage image, const IceTInt *region)
{
    IceTSizeType width = icetImageGetWidth(image);
    IceTSizeType height = icetImageGetHeight(image);
    IceTEnum color_format = icetImageGetColorFormat(image);
    IceTEnum depth_format = icetImageGetDepthFormat(image);
    IceTSizeType x, y;

    if (color_format == ICET_IMAGE_COLOR_RGBA_UBYTE) {
        IceTUInt *color_buffer = icetImageGetColorui(image);
        IceTUInt background_color;

        icetGetIntegerv(ICET_BACKGROUND_COLOR_WORD,(IceTInt*)&background_color);

      /* Clear out bottom. */
        for (y = 0; y < region[1]; y++) {
            for (x = 0; x < width; x++) {
                color_buffer[y*width + x] = background_color;
            }
        }
      /* Clear out left and right. */
        if ((region[0] > 0) || (region[0]+region[2] < width)) {
            for (y = region[1]; y < region[1]+region[3]; y++) {
                for (x = 0; x < region[0]; x++) {
                    color_buffer[y*width + x] = background_color;
                }
                for (x = region[0]+region[2]; x < width; x++) {
                    color_buffer[y*width + x] = background_color;
                }
            }
        }
      /* Clear out top. */
        for (y = region[1]+region[3]; y < height; y++) {
            for (x = 0; x < width; x++) {
                color_buffer[y*width + x] = background_color;
            }
        }
    } else if (color_format == ICET_IMAGE_COLOR_RGBA_FLOAT) {
        IceTFloat *color_buffer = icetImageGetColorf(image);
        IceTFloat background_color[4];

        icetGetFloatv(ICET_BACKGROUND_COLOR, background_color);

      /* Clear out bottom. */
        for (y = 0; y < region[1]; y++) {
            for (x = 0; x < width; x++) {
                color_buffer[4*(y*width + x) + 0] = background_color[0];
                color_buffer[4*(y*width + x) + 1] = background_color[1];
                color_buffer[4*(y*width + x) + 2] = background_color[2];
                color_buffer[4*(y*width + x) + 3] = background_color[3];
            }
        }
      /* Clear out left and right. */
        if ((region[0] > 0) || (region[0]+region[2] < width)) {
            for (y = region[1]; y < region[1]+region[3]; y++) {
                for (x = 0; x < region[0]; x++) {
                    color_buffer[4*(y*width + x) + 0] = background_color[0];
                    color_buffer[4*(y*width + x) + 1] = background_color[1];
                    color_buffer[4*(y*width + x) + 2] = background_color[2];
                    color_buffer[4*(y*width + x) + 3] = background_color[3];
                }
                for (x = region[0]+region[2]; x < width; x++) {
                    color_buffer[4*(y*width + x) + 0] = background_color[0];
                    color_buffer[4*(y*width + x) + 1] = background_color[1];
                    color_buffer[4*(y*width + x) + 2] = background_color[2];
                    color_buffer[4*(y*width + x) + 3] = background_color[3];
                }
            }
        }
      /* Clear out top. */
        for (y = region[1]+region[3]; y < height; y++) {
            for (x = 0; x < width; x++) {
                color_buffer[4*(y*width + x) + 0] = background_color[0];
                color_buffer[4*(y*width + x) + 1] = background_color[1];
                color_buffer[4*(y*width + x) + 2] = background_color[2];
                color_buffer[4*(y*width + x) + 3] = background_color[3];
            }
        }
    } else if (color_format != ICET_IMAGE_COLOR_NONE) {
        icetRaiseError("Invalid color format.", ICET_SANITY_CHECK_FAIL);
    }

    if (depth_format == ICET_IMAGE_DEPTH_FLOAT) {
        IceTFloat *depth_buffer = icetImageGetDepthf(image);

      /* Clear out bottom. */
        for (y = 0; y < region[1]; y++) {
            for (x = 0; x < width; x++) {
                depth_buffer[y*width + x] = 1.0f;
            }
        }
      /* Clear out left and right. */
        if ((region[0] > 0) || (region[0]+region[2] < width)) {
            for (y = region[1]; y < region[1]+region[3]; y++) {
                for (x = 0; x < region[0]; x++) {
                    depth_buffer[y*width + x] = 1.0f;
                }
                for (x = region[0]+region[2]; x < width; x++) {
                    depth_buffer[y*width + x] = 1.0f;
                }
            }
        }
      /* Clear out top. */
        for (y = region[1]+region[3]; y < height; y++) {
            for (x = 0; x < width; x++) {
                depth_buffer[y*width + x] = 1.0f;
            }
        }
    } else if (depth_format != ICET_IMAGE_DEPTH_NONE) {
        icetRaiseError("Invalid depth format.", ICET_SANITY_CHECK_FAIL);
    }
}

void icetImagePackageForSend(IceTImage image,
                             IceTVoid **buffer, IceTSizeType *size)
{
    ICET_TEST_IMAGE_HEADER(image);

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
        = (IceTInt)icetImageGetNumPixels(image);

  /* The image is valid (as far as we can tell). */
    return image;
}

void icetSparseImagePackageForSend(IceTSparseImage image,
                                   IceTVoid **buffer, IceTSizeType *size)
{
    ICET_TEST_SPARSE_IMAGE_HEADER(image);

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
        = (IceTInt)icetSparseImageGetNumPixels(image);

  /* The image is valid (as far as we can tell). */
    return image;
}

void icetClearImage(IceTImage image)
{
    IceTInt region[4] = {0, 0, 0, 0};
    icetImageClearAroundRegion(image, region);
}

void icetClearSparseImage(IceTSparseImage image)
{
    IceTByte *data;
    IceTSizeType p;

    ICET_TEST_SPARSE_IMAGE_HEADER(image);

    /* Use IceTByte for byte-based pointer arithmetic. */
    data = ICET_IMAGE_DATA(image);
    p = icetSparseImageGetNumPixels(image);

    while (p > 0xFFFF) {
        INACTIVE_RUN_LENGTH(data) = 0xFFFF;
        ACTIVE_RUN_LENGTH(data) = 0;
        data += RUN_LENGTH_SIZE;
        p -= 0xFFFF;
    }

    INACTIVE_RUN_LENGTH(data) = (IceTUShort)p;
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
    IceTImage rendered_image;
    IceTDouble read_time;
    IceTDouble timer;

    viewports = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS);
    width = viewports[4*tile+2];
    height = viewports[4*tile+3];
    icetImageSetDimensions(image, width, height);

    rendered_image = renderTile(tile, screen_viewport, target_viewport, image);

    timer = icetWallTime();

    if (icetImageEqual(rendered_image, image)) {
      /* Check to make sure the screen and target viewports are also equal. */
        if (    (screen_viewport[0] != target_viewport[0])
             || (screen_viewport[1] != target_viewport[1])
             || (screen_viewport[2] != target_viewport[2])
             || (screen_viewport[3] != target_viewport[3]) ) {
            icetRaiseError("Inconsistent values returned from renderTile.",
                           ICET_SANITY_CHECK_FAIL);
        }
    } else {
      /* Copy the appropriate part of the image to the output buffer. */
        icetImageCopyRegion(rendered_image, screen_viewport,
                            image, target_viewport);
    }

    icetImageClearAroundRegion(image, target_viewport);

    icetGetDoublev(ICET_BUFFER_READ_TIME, &read_time);
    read_time += icetWallTime() - timer;
    icetStateSetDouble(ICET_BUFFER_READ_TIME, read_time);
}

void icetGetCompressedTileImage(IceTInt tile, IceTSparseImage compressed_image)
{
    IceTInt screen_viewport[4], target_viewport[4];
    IceTImage raw_image;
    IceTInt *viewports;
    IceTSizeType width, height;
    IceTSizeType space_left, space_right, space_bottom, space_top;

    viewports = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS);
    width = viewports[4*tile+2];
    height = viewports[4*tile+3];
    icetSparseImageSetDimensions(compressed_image, width, height);

    raw_image = renderTile(tile, screen_viewport, target_viewport,
                           icetImageNull());

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
#define REGION
#define REGION_OFFSET_X         screen_viewport[0]
#define REGION_OFFSET_Y         screen_viewport[1]
#define REGION_WIDTH            screen_viewport[2]
#define REGION_HEIGHT           screen_viewport[3]
#include "compress_func_body.h"
}

void icetCompressImage(const IceTImage image,
                       IceTSparseImage compressed_image)
{
    icetCompressSubImage(image, 0, icetImageGetNumPixels(image),
                         compressed_image);

  /* This is a hack to get the width/height of the compressed image to agree
     with the original image. */
    ICET_IMAGE_HEADER(compressed_image)[ICET_IMAGE_WIDTH_INDEX]
        = (IceTInt)icetImageGetWidth(image);
    ICET_IMAGE_HEADER(compressed_image)[ICET_IMAGE_HEIGHT_INDEX]
        = (IceTInt)icetImageGetHeight(image);
}

void icetCompressSubImage(const IceTImage image,
                          IceTSizeType offset, IceTSizeType pixels,
                          IceTSparseImage compressed_image)
{
    ICET_TEST_IMAGE_HEADER(image);
    ICET_TEST_SPARSE_IMAGE_HEADER(compressed_image);

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
    ICET_TEST_IMAGE_HEADER(image);
    ICET_TEST_SPARSE_IMAGE_HEADER(compressed_image);

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
            const IceTFloat *srcDepthBuffer = icetImageGetDepthf(srcBuffer);
            IceTFloat *destDepthBuffer = icetImageGetDepthf(destBuffer);

            if (color_format == ICET_IMAGE_COLOR_RGBA_UBYTE) {
                const IceTUInt *srcColorBuffer=icetImageGetColorui(srcBuffer);
                IceTUInt *destColorBuffer = icetImageGetColorui(destBuffer);
                for (i = 0; i < pixels; i++) {
                    if (srcDepthBuffer[i] < destDepthBuffer[i]) {
                        destDepthBuffer[i] = srcDepthBuffer[i];
                        destColorBuffer[i] = srcColorBuffer[i];
                    }
                }
            } else if (color_format == ICET_IMAGE_COLOR_RGBA_FLOAT) {
                const IceTFloat *srcColorBuffer = icetImageGetColorf(srcBuffer);
                IceTFloat *destColorBuffer = icetImageGetColorf(destBuffer);
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
            const IceTUByte *srcColorBuffer = icetImageGetColorcub(srcBuffer);
            IceTUByte *destColorBuffer = icetImageGetColorub(destBuffer);
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
            const IceTFloat *srcColorBuffer = icetImageGetColorcf(srcBuffer);
            IceTFloat *destColorBuffer = icetImageGetColorf(destBuffer);
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
                                IceTSizeType offset,
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

    *compare_time += icetWallTime() - timer;
}

static IceTImage renderTile(int tile,
                            IceTInt *screen_viewport,
                            IceTInt *target_viewport,
                            IceTImage tile_buffer)
{
    IceTInt *contained_viewport;
    IceTInt *tile_viewport;
    IceTBoolean *contained_mask;
    IceTInt physical_width, physical_height;
    IceTBoolean use_floating_viewport;
    IceTDrawCallbackType drawfunc;
    IceTVoid *value;
    IceTDouble render_time;
    IceTDouble timer;
    IceTInt readback_viewport[4];
    IceTImage render_buffer;
    IceTDouble projection_matrix[16];
    IceTDouble modelview_matrix[16];
    IceTFloat background_color[4];

    icetRaiseDebug1("Rendering tile %d", tile);
    contained_viewport = icetUnsafeStateGetInteger(ICET_CONTAINED_VIEWPORT);
    tile_viewport = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS) + 4*tile;
    contained_mask = icetUnsafeStateGetBoolean(ICET_CONTAINED_TILES_MASK);
    use_floating_viewport = icetIsEnabled(ICET_FLOATING_VIEWPORT);

    icetGetIntegerv(ICET_PHYSICAL_RENDER_WIDTH, &physical_width);
    icetGetIntegerv(ICET_PHYSICAL_RENDER_HEIGHT, &physical_height);

    icetRaiseDebug4("contained viewport: %d %d %d %d",
                    (int)contained_viewport[0], (int)contained_viewport[1],
                    (int)contained_viewport[2], (int)contained_viewport[3]);
    icetRaiseDebug4("tile viewport: %d %d %d %d",
                    (int)tile_viewport[0], (int)tile_viewport[1],
                    (int)tile_viewport[2], (int)tile_viewport[3]);

    render_buffer = tile_buffer;

    if (   !contained_mask[tile]
        || (contained_viewport[0] + contained_viewport[2] < tile_viewport[0])
        || (contained_viewport[1] + contained_viewport[3] < tile_viewport[1])
        || (contained_viewport[0] > tile_viewport[0] + tile_viewport[2])
        || (contained_viewport[1] > tile_viewport[1] + tile_viewport[3]) ) {
      /* Case 0: geometry completely outside tile. */
        icetRaiseDebug("Case 0: geometry completely outside tile.");
        screen_viewport[0] = target_viewport[0] = 0;
        screen_viewport[1] = target_viewport[1] = 0;
        screen_viewport[2] = target_viewport[2] = 0;
        screen_viewport[3] = target_viewport[3] = 0;
      /* Don't bother to render. */
        return tile_buffer;
#if 1
    } else if (   (contained_viewport[0] >= tile_viewport[0])
               && (contained_viewport[1] >= tile_viewport[1])
               && (   contained_viewport[2]+contained_viewport[0]
                   <= tile_viewport[2]+tile_viewport[0])
               && (   contained_viewport[3]+contained_viewport[1]
                   <= tile_viewport[3]+tile_viewport[1]) ) {
      /* Case 1: geometry fits entirely within tile. */
        icetRaiseDebug("Case 1: geometry fits entirely within tile.");

        icetProjectTile(tile, projection_matrix);
        icetStateSetIntegerv(ICET_RENDERED_VIEWPORT, 4, tile_viewport);
        screen_viewport[0] = target_viewport[0]
            = contained_viewport[0] - tile_viewport[0];
        screen_viewport[1] = target_viewport[1]
            = contained_viewport[1] - tile_viewport[1];
        screen_viewport[2] = target_viewport[2] = contained_viewport[2];
        screen_viewport[3] = target_viewport[3] = contained_viewport[3];

        readback_viewport[0] = screen_viewport[0];
        readback_viewport[1] = screen_viewport[1];
        readback_viewport[2] = screen_viewport[2];
        readback_viewport[3] = screen_viewport[3];
#endif
    } else if (   !use_floating_viewport
               || (contained_viewport[2] > physical_width)
               || (contained_viewport[3] > physical_height) ) {
      /* Case 2: Can't use floating viewport due to use selection or image
         does not fit. */
        icetRaiseDebug("Case 2: Can't use floating viewport.");

        icetProjectTile(tile, projection_matrix);
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

        readback_viewport[0] = screen_viewport[0];
        readback_viewport[1] = screen_viewport[1];
        readback_viewport[2] = screen_viewport[2];
        readback_viewport[3] = screen_viewport[3];
    } else {
      /* Case 3: Using floating viewport. */
        IceTDouble viewport_project_matrix[16];
        IceTDouble global_projection_matrix[16];
        IceTInt rendered_viewport[4];
        icetRaiseDebug("Case 3: Using floating viewport.");

      /* This is the viewport in the global tiled display that we will be
         rendering. */
        rendered_viewport[0] = contained_viewport[0];
        rendered_viewport[1] = contained_viewport[1];
        rendered_viewport[2] = physical_width;
        rendered_viewport[3] = physical_height;

      /* This is the area that has valid pixels.  The screen_viewport will be a
         subset of this. */
        readback_viewport[0] = 0;
        readback_viewport[1] = 0;
        readback_viewport[2] = contained_viewport[2];
        readback_viewport[3] = contained_viewport[3];

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

      /* Floating viewport must be stored in our own buffer so subsequent tiles
         can be read from it. */
        render_buffer = getRenderBuffer();

      /* Check to see if we already rendered the floating viewport.  The whole
         point of the floating viewport is to do one actual render and reuse the
         image to grab all the actual tile images. */
        if (  icetStateGetTime(ICET_RENDERED_VIEWPORT)
            > icetStateGetTime(ICET_IS_DRAWING_FRAME) ) {
          /* Already rendered image for this tile. */
            IceTInt *old_rendered_viewport
                = icetUnsafeStateGetInteger(ICET_RENDERED_VIEWPORT);
            IceTBoolean old_rendered_viewport_valid
                = (   (old_rendered_viewport[0] == rendered_viewport[0])
                   || (old_rendered_viewport[1] == rendered_viewport[1])
                   || (old_rendered_viewport[2] == rendered_viewport[2])
                   || (old_rendered_viewport[3] == rendered_viewport[3]) );
            if (!old_rendered_viewport_valid) {
                icetRaiseError("Rendered floating viewport became invalidated.",
                               ICET_SANITY_CHECK_FAIL);
            } else {
                icetRaiseDebug("Already rendered floating viewport.");
                return render_buffer;
            }
        }
        icetStateSetIntegerv(ICET_RENDERED_VIEWPORT, 4, rendered_viewport);

      /* Setup render for this tile. */

        icetGetViewportProject(rendered_viewport[0], rendered_viewport[1],
                               rendered_viewport[2], rendered_viewport[3],
                               viewport_project_matrix);
        icetGetDoublev(ICET_PROJECTION_MATRIX, global_projection_matrix);
        icetMultMatrix(projection_matrix,
                       viewport_project_matrix, global_projection_matrix);
    }

  /* Make sure that the current render_buffer is sized appropriately for the
     physical viewport.  If not, use our own buffer. */
    if (    (icetImageGetWidth(render_buffer) != physical_width)
         || (icetImageGetHeight(render_buffer) != physical_height) ) {
        render_buffer = getRenderBuffer();
    }

  /* Now we can actually start to render an image. */
    icetGetDoublev(ICET_MODELVIEW_MATRIX, modelview_matrix);
    icetGetFloatv(ICET_BACKGROUND_COLOR, background_color);

  /* Draw the geometry. */
    icetGetPointerv(ICET_DRAW_FUNCTION, &value);
    drawfunc = (IceTDrawCallbackType)value;
    icetRaiseDebug("Calling draw function.");
    timer = icetWallTime();
    (*drawfunc)(projection_matrix, modelview_matrix, background_color,
                readback_viewport, render_buffer);
    icetGetDoublev(ICET_RENDER_TIME, &render_time);
    render_time += icetWallTime() - timer;
    icetStateSetDouble(ICET_RENDER_TIME, render_time);

    return render_buffer;
}

/* This function is full of hackery. */
static IceTImage getRenderBuffer(void)
{
    /* Check to see if we are in the same frame as the last time we returned
       this buffer.  In that case, just restore the buffer because it still has
       the image we need. */
    if (  icetStateGetTime(ICET_RENDER_BUFFER_SIZE)
        > icetStateGetTime(ICET_IS_DRAWING_FRAME) ) {
      /* A little bit of hackery: this assumes that a buffer initialized is the
         same one returned from icetImagePackageForSend.  It (currently)
         does. */
        IceTVoid *buffer;
        icetRaiseDebug("Last render should still be good.");
        buffer = icetGetStateBuffer(ICET_RENDER_BUFFER, 0);
        return icetImageUnpackageFromReceive(buffer);       
    } else {
        IceTInt dim[2];

        icetGetIntegerv(ICET_PHYSICAL_RENDER_WIDTH, &dim[0]);
        icetGetIntegerv(ICET_PHYSICAL_RENDER_HEIGHT, &dim[1]);

      /* Creating a new image object.  "Touch" the ICET_RENDER_BUFFER_SIZE state
         variable to signify the time we created the image so the above check
         works on the next call. */
        icetStateSetIntegerv(ICET_RENDER_BUFFER_SIZE, 2, dim);

        return icetGetStateBufferImage(ICET_RENDER_BUFFER, dim[0], dim[1]);
    }
}
