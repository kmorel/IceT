/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 */

/* This is not a traditional header file, but rather a "macro" file that defines
 * a function body for a compression function.  (If this were C++, we would
 * actually use templates to automatically generate all these cases.)  In
 * general, there are many flavors of the compression functionality which differ
 * only slightly.  Rather than maintain lots of different code bases or try to
 * debug big macros, we just include this file with various parameters.
 *
 * The following macros must be defined:
 *      INPUT_IMAGE - an IceTImage object containing the data to be compressed.
 *      OUTPUT_SPARSE_IMAGE - the buffer that will hold the compressed image
 *              (i.e. an allocated IceTSparseImage pointer).
 *
 * The following macros are optional:
 *      PADDING - If defined, enables inactive pixels to be placed
 *              around the file.  If defined, then SPACE_BOTTOM, SPACE_TOP,
 *              SPACE_LEFT, SPACE_RIGHT, FULL_WIDTH, and FULL_HEIGHT must
 *              all also be defined.
 *      OFFSET - If defined to a number (or variable holding a number), skips
 *              that many pixels at the beginning of the image.
 *      PIXEL_COUNT - If defined to a number (or a variable holding a number),
 *              uses this as the size of the image rather than the actual size
 *              defined in the image.  This should be defined if OFFSET is
 *              defined.
 *
 * All of the above macros are undefined at the end of this file.
 */

#ifndef INPUT_IMAGE
#error Need INPUT_IMAGE macro.  Is this included in image.c?
#endif
#ifndef OUTPUT_SPARSE_IMAGE
#error Need OUTPUT_SPARSE_IMAGE macro.  Is this included in image.c?
#endif
#ifndef INACTIVE_RUN_LENGTH
#error Need INACTIVE_RUN_LENGTH macro.  Is this included in image.c?
#endif
#ifndef ACTIVE_RUN_LENGTH
#error Need ACTIVE_RUN_LENGTH macro.  Is this included in image.c?
#endif

{
    IceTEnum _color_format, _depth_format;
    IceTSizeType _pixel_count;
    IceTEnum _composite_mode;

    icetGetEnumv(ICET_COMPOSITE_MODE, &_composite_mode);

    _color_format = icetImageGetColorFormat(INPUT_IMAGE);
    _depth_format = icetImageGetDepthFormat(INPUT_IMAGE);

#ifdef PIXEL_COUNT
    _pixel_count = PIXEL_COUNT;
#else
    _pixel_count = icetImageGetSize(INPUT_IMAGE);
#endif

#ifdef DEBUG
    if (   (icetSparseImageGetColorFormat(OUTPUT_SPARSE_IMAGE) != _color_format)
        || (icetSparseImageGetDepthFormat(OUTPUT_SPARSE_IMAGE) != _depth_format)
           ) {
        icetRaiseError("Format of input and output to compress do not match.",
                       ICET_SANITY_CHECK_FAIL);
    }
#ifdef PADDING
    if (   icetSparseImageGetSize(OUTPUT_SPARSE_IMAGE)
        != (  _pixel_count + (FULL_WIDTH)*(SPACE_TOP+SPACE_BOTTOM)
            + ((FULL_HEIGHT)-(SPACE_TOP+SPACE_BOTTOM))*(SPACE_LEFT+SPACE_RIGHT))
           ) {
        icetRaiseError("Size of input and output to compress do not match.",
                       ICET_SANITY_CHECK_FAIL);
    }
#else /*PADDING*/
    if (icetSparseImageGetSize(OUTPUT_SPARSE_IMAGE) != _pixel_count) {
        icetRaiseError("Size of input and output to compress do not match.",
                       ICET_SANITY_CHECK_FAIL);
    }
#endif /*PADDING*/
#endif /*DEBUG*/

    if (_composite_mode == ICET_COMPOSITE_MODE_Z_BUFFER) {
        if (_depth_format == ICET_IMAGE_DEPTH_FLOAT) {
          /* Use Z buffer for active pixel testing. */
            const IceTFloat *_depth = icetImageGetDepthFloat(INPUT_IMAGE);
#ifdef OFFSET
            _depth += OFFSET;
#endif
            if (_color_format == ICET_IMAGE_COLOR_RGBA_UBYTE) {
                const IceTUInt *_color;
                IceTUInt *_c_out;
                IceTFloat *_d_out;
                _color = icetImageGetColorUInt(INPUT_IMAGE);
#ifdef OFFSET
                _color += OFFSET;
#endif
#define CT_COMPRESSED_IMAGE     OUTPUT_SPARSE_IMAGE
#define CT_COLOR_FORMAT         _color_format
#define CT_DEPTH_FORMAT         _depth_format
#define CT_PIXEL_COUNT          _pixel_count
#define CT_ACTIVE()             (_depth[0] < 1.0)
#define CT_WRITE_PIXEL(dest)    _c_out = (IceTUInt *)dest;      \
                                _c_out[0] = _color[0];          \
                                dest += sizeof(IceTUInt);       \
                                _d_out = (IceTFloat *)dest;     \
                                _d_out[0] = _depth[0];          \
                                dest += sizeof(IceTFloat);
#define CT_INCREMENT_PIXEL()    _color++;  _depth++;
#ifdef PADDING
#define CT_PADDING
#define CT_SPACE_BOTTOM         SPACE_BOTTOM
#define CT_SPACE_TOP            SPACE_TOP
#define CT_SPACE_LEFT           SPACE_LEFT
#define CT_SPACE_RIGHT          SPACE_RIGHT
#define CT_FULL_WIDTH           FULL_WIDTH
#define CT_FULL_HEIGHT          FULL_HEIGHT
#endif
#include "compress_template_body.h"
            } else if (_color_format == ICET_IMAGE_COLOR_RGBA_FLOAT) {
                const IceTFloat *_color;
                IceTFloat *_out;
                _color = icetImageGetColorFloat(INPUT_IMAGE);
#ifdef OFFSET
                _color += 4*(OFFSET);
#endif
#define CT_COMPRESSED_IMAGE     OUTPUT_SPARSE_IMAGE
#define CT_COLOR_FORMAT         _color_format
#define CT_DEPTH_FORMAT         _depth_format
#define CT_PIXEL_COUNT          _pixel_count
#define CT_ACTIVE()             (_depth[0] < 1.0)
#define CT_WRITE_PIXEL(dest)    _out = (IceTFloat *)dest;       \
                                _out[0] = _color[0];            \
                                _out[1] = _color[1];            \
                                _out[2] = _color[2];            \
                                _out[3] = _color[3];            \
                                _out[4] = _depth[0];            \
                                dest += 5*sizeof(IceTFloat);
#define CT_INCREMENT_PIXEL()    _color += 4;  _depth++;
#ifdef PADDING
#define CT_PADDING
#define CT_SPACE_BOTTOM         SPACE_BOTTOM
#define CT_SPACE_TOP            SPACE_TOP
#define CT_SPACE_LEFT           SPACE_LEFT
#define CT_SPACE_RIGHT          SPACE_RIGHT
#define CT_FULL_WIDTH           FULL_WIDTH
#define CT_FULL_HEIGHT          FULL_HEIGHT
#endif
#include "compress_template_body.h"
            } else if (_color_format == ICET_IMAGE_COLOR_NONE) {
                IceTFloat *_out;
#define CT_COMPRESSED_IMAGE     OUTPUT_SPARSE_IMAGE
#define CT_COLOR_FORMAT         _color_format
#define CT_DEPTH_FORMAT         _depth_format
#define CT_PIXEL_COUNT          _pixel_count
#define CT_ACTIVE()             (_depth[0] < 1.0)
#define CT_WRITE_PIXEL(dest)    _out = (IceTFloat *)dest;       \
                                _out[0] = _depth[0];            \
                                dest += 1*sizeof(IceTFloat);
#define CT_INCREMENT_PIXEL()    _depth++;
#ifdef PADDING
#define CT_PADDING
#define CT_SPACE_BOTTOM         SPACE_BOTTOM
#define CT_SPACE_TOP            SPACE_TOP
#define CT_SPACE_LEFT           SPACE_LEFT
#define CT_SPACE_RIGHT          SPACE_RIGHT
#define CT_FULL_WIDTH           FULL_WIDTH
#define CT_FULL_HEIGHT          FULL_HEIGHT
#endif
#include "compress_template_body.h"
            } else {
                icetRaiseError("Encountered invalid color format.",
                               ICET_SANITY_CHECK_FAIL);
            }
        } else if (_depth_format == ICET_IMAGE_DEPTH_NONE) {
            icetRaiseError("Cannot use Z buffer compression with no"
                           " Z buffer.", ICET_INVALID_OPERATION);
        } else {
            icetRaiseError("Encountered invalid depth format.",
                           ICET_SANITY_CHECK_FAIL);
        }
    } else if (_composite_mode == ICET_COMPOSITE_MODE_BLEND) {
      /* Use alpha for active pixel testing. */
        if (_depth_format != ICET_IMAGE_DEPTH_NONE) {
            icetRaiseWarning("Z buffer ignored during blend compress"
                             " operation.  Output z buffer meaningless.",
                             ICET_INVALID_VALUE);
        }
        if (_color_format == ICET_IMAGE_COLOR_RGBA_UBYTE) {
            const IceTUInt *_color;
            IceTUInt *_out;
            _color = icetImageGetColorUInt(INPUT_IMAGE);
#ifdef OFFSET
            _color += OFFSET;
#endif
#define CT_COMPRESSED_IMAGE     OUTPUT_SPARSE_IMAGE
#define CT_COLOR_FORMAT         _color_format
#define CT_DEPTH_FORMAT         _depth_format
#define CT_PIXEL_COUNT          _pixel_count
#define CT_ACTIVE()             (((IceTUByte*)_color)[3] != 0x00)
#define CT_WRITE_PIXEL(dest)    _out = (IceTUInt *)dest;        \
                                _out[0] = _color[0];            \
                                dest += sizeof(IceTUInt);
#define CT_INCREMENT_PIXEL()    _color++;
#ifdef PADDING
#define CT_PADDING
#define CT_SPACE_BOTTOM         SPACE_BOTTOM
#define CT_SPACE_TOP            SPACE_TOP
#define CT_SPACE_LEFT           SPACE_LEFT
#define CT_SPACE_RIGHT          SPACE_RIGHT
#define CT_FULL_WIDTH           FULL_WIDTH
#define CT_FULL_HEIGHT          FULL_HEIGHT
#endif
#include "compress_template_body.h"
        } else if (_color_format == ICET_IMAGE_COLOR_RGBA_FLOAT) {
            const IceTFloat *_color;
            IceTFloat *_out;
            _color = icetImageGetColorFloat(INPUT_IMAGE);
#ifdef OFFSET
            _color += 4*(OFFSET);
#endif
#define CT_COMPRESSED_IMAGE     OUTPUT_SPARSE_IMAGE
#define CT_COLOR_FORMAT         _color_format
#define CT_DEPTH_FORMAT         _depth_format
#define CT_PIXEL_COUNT          _pixel_count
#define CT_ACTIVE()             (_color[3] != 0.0)
#define CT_WRITE_PIXEL(dest)    _out = (IceTFloat *)dest;       \
                                _out[0] = _color[0];            \
                                _out[1] = _color[1];            \
                                _out[2] = _color[2];            \
                                _out[3] = _color[3];            \
                                dest += 4*sizeof(IceTUInt);
#define CT_INCREMENT_PIXEL()    _color += 4;
#ifdef PADDING
#define CT_PADDING
#define CT_SPACE_BOTTOM         SPACE_BOTTOM
#define CT_SPACE_TOP            SPACE_TOP
#define CT_SPACE_LEFT           SPACE_LEFT
#define CT_SPACE_RIGHT          SPACE_RIGHT
#define CT_FULL_WIDTH           FULL_WIDTH
#define CT_FULL_HEIGHT          FULL_HEIGHT
#endif
#include "compress_template_body.h"
        } else if (_color_format == ICET_IMAGE_COLOR_NONE) {
            IceTUInt *_out;
            IceTSizeType _runlength;
            icetRaiseWarning("Compressing image with no data.",
                             ICET_INVALID_OPERATION);
            _out = ICET_IMAGE_DATA(OUTPUT_SPARSE_IMAGE);
            _runlength = _pixel_count;
            while (_runlength > 0xFFFF) {
                INACTIVE_RUN_LENGTH(_out) = 0xFFFF;
                ACTIVE_RUN_LENGTH(_out) = 0;
                _out++;
                _runlength -= 0xFFFF;
            }
            INACTIVE_RUN_LENGTH(_out) = _runlength;
            ACTIVE_RUN_LENGTH(_out) = 0;
            _out++;
            ICET_IMAGE_HEADER(OUTPUT_SPARSE_IMAGE)[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX]
                = (IceTSizeType)
                  (  (IceTPointerArithmetic)_out
                   - (IceTPointerArithmetic)ICET_IMAGE_HEADER(OUTPUT_SPARSE_IMAGE));
        } else {
            icetRaiseError("Encountered invalid color format.",
                           ICET_SANITY_CHECK_FAIL);
        }
    } else {
        icetRaiseError("Encountered invalid composite mode.",
                       ICET_SANITY_CHECK_FAIL);
    }

    icetRaiseDebug1("Compression: %f%%\n",
        100.0f - (  100.0f*icetSparseImageGetCompressedBufferSize(OUTPUT_SPARSE_IMAGE)
                  / icetImageBufferSize(_color_format, _depth_format, _pixel_count) ));
}

#undef INPUT_IMAGE
#undef OUTPUT_SPARSE_IMAGE

#ifdef PADDING
#undef PADDING
#undef SPACE_BOTTOM
#undef SPACE_TOP
#undef SPACE_LEFT
#undef SPACE_RIGHT
#undef FULL_WIDTH
#undef FULL_HEIGHT
#endif

#ifdef OFFSET
#undef OFFSET
#endif

#ifdef PIXEL_COUNT
#undef PIXEL_COUNT
#endif
