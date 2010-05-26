/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

/* This is not a traditional header file, but rather a "macro" file that defines
 * a template for a compression function.  (If this were C++, we would actually
 * use templates.)  In general, there are many flavors of the compression
 * functionality which differ only slightly.  Rather than maintain lots of
 * different code bases or try to debug big macros, we just include this file
 * with various parameters.
 *
 * In general, this file should only be included by compress_func_body.h
 *
 * The following macros must be defined:
 *      CT_COMPRESSED_BUFFER - the buffer that will hold the compressed image.
 *      CT_COLOR_FORMAT - color format IceTEnum for input and output
 *      CT_DEPTH_FORMAT - depth format IceTEnum for input and output
 *      CT_PIXEL_COUNT - the number of pixels in the original image (or a
 *              variable holding it.
 *      CT_ACTIVE() - provides a true value if the current pixel is active.
 *      CT_WRITE_PIXEL(pointer) - writes the current pixel to the pointer and
 *              increments the pointer.
 *      CT_INCREMENT_PIXEL() - Increments to the next pixel.
 *
 * The following macros are optional:
 *      CT_PADDING - If defined, enables inactive pixels to be placed
 *              around the file.  If defined, then CT_SPACE_BOTTOM,
 *              CT_SPACE_TOP, CT_SPACE_LEFT, CT_SPACE_RIGHT, CT_FULL_WIDTH,
 *              and CT_FULL_HEIGHT must all also be defined.
 *
 * All of the above macros are undefined at the end of this file.
 */

#ifndef CT_COMPRESSED_BUFFER
#error Need CT_COMPRESSED_BUFFER macro.  Is this included in image.c?
#endif
#ifndef CT_COLOR_FORMAT
#error Need CT_COLOR_FORMAT macro.  Is this included in image.c?
#endif
#ifndef CT_DEPTH_FORMAT
#error Need CT_DEPTH_FORMAT macro.  Is this included in image.c ?
#endif
#ifndef CT_PIXEL_COUNT
#error Need CT_PIXEL_COUNT macro.  Is this included in image.c?
#endif
#ifndef ICET_IMAGE_DATA
#error Need ICET_IMAGE_DATA macro.  Is this included in image.c?
#endif
#ifndef INACTIVE_RUN_LENGTH
#error Need INACTIVE_RUN_LENGTH macro.  Is this included in image.c?
#endif
#ifndef ACTIVE_RUN_LENGTH
#error Need ACTIVE_RUN_LENGTH macro.  Is this included in image.c?
#endif
#ifndef RUN_LENGTH_SIZE
#error Need RUN_LENGTH_SIZE macro.  Is this included in image.c?
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4127)
#endif

{
    IceTVoid *_dest;
    IceTSizeType _pixels = CT_PIXEL_COUNT;
    IceTSizeType _p;
    IceTSizeType _count;
#ifdef DEBUG
    IceTSizeType _totalcount = 0;
#endif
    IceTDouble _timer;
    IceTDouble *_compress_time;
    IceTSizeType _compressed_size;

    _compress_time = icetUnsafeStateGetDouble(ICET_COMPRESS_TIME);
    _timer = icetWallTime();

    icetSparseImageInitialize(CT_COMPRESSED_BUFFER, CT_COLOR_FORMAT,
                              CT_DEPTH_FORMAT, _pixels);
    _dest = ICET_IMAGE_DATA(CT_COMPRESSED_BUFFER);

#ifndef CT_PADDING
    _count = 0;
#else /* CT_PADDING */
    _count = CT_SPACE_BOTTOM*CT_FULL_WIDTH;

    if ((CT_SPACE_LEFT != 0) || (CT_SPACE_RIGHT != 0)) {
        int _line, _lastline;
        for (_line = CT_SPACE_BOTTOM, _lastline = CT_FULL_HEIGHT-CT_SPACE_TOP;
             _line < _lastline; _line++) {
            int _x = CT_SPACE_LEFT;
            int _lastx = CT_FULL_WIDTH-CT_SPACE_RIGHT;
            _count += CT_SPACE_LEFT;
            while (ICET_TRUE) {
                IceTUInt *_runlengths;
                while ((_x < _lastx) && (!CT_ACTIVE())) {
                    _x++;
                    _count++;
                    CT_INCREMENT_PIXEL();
                }
                if (_x >= _lastx) break;
                _runlengths = _dest++;
                while (_count > 0xFFFF) {
                    INACTIVE_RUN_LENGTH(_runlengths) = 0xFFFF;
                    ACTIVE_RUN_LENGTH(_runlengths) = 0;
#ifdef DEBUG
                    _totalcount += 0xFFFF;
#endif
                    _count -= 0xFFFF;
                    _runlengths = _dest++;
                }
                INACTIVE_RUN_LENGTH(_runlengths) = (IceTUShort)_count;
#ifdef DEBUG
                _totalcount += _count;
#endif
                _count = 0;
                while ((_x < _lastx) && CT_ACTIVE() && (_count < 0xFFFF)) {
                    CT_WRITE_PIXEL(_dest);
                    CT_INCREMENT_PIXEL();
                    _count++;
                    _x++;
                }
                ACTIVE_RUN_LENGTH(_runlengths) = (IceTUShort)_count;
#ifdef DEBUG
                _totalcount += _count;
#endif
                _count = 0;
                if (_x >= _lastx) break;
            }
            _count += CT_SPACE_RIGHT;
        }
    } else { /* CT_SPACE_LEFT == CT_SPACE_RIGHT == 0 */
        _pixels = (CT_FULL_HEIGHT-CT_SPACE_BOTTOM-CT_SPACE_TOP)*CT_FULL_WIDTH;
#endif /* CT_PADDING */

        _p = 0;
        while (_p < _pixels) {
            IceTUInt *_runlengths = _dest++;
          /* Count background pixels. */
            while ((_p < _pixels) && (!CT_ACTIVE())) {
                _p++;
                _count++;
                CT_INCREMENT_PIXEL();
            }
            while (_count > 0xFFFF) {
                INACTIVE_RUN_LENGTH(_runlengths) = 0xFFFF;
                ACTIVE_RUN_LENGTH(_runlengths) = 0;
#ifdef DEBUG
                _totalcount += 0xFFFF;
#endif
                _count -= 0xFFFF;
                _runlengths = _dest++;
            }
            INACTIVE_RUN_LENGTH(_runlengths) = (IceTUShort)_count;
#ifdef DEBUG
            _totalcount += _count;
#endif

          /* Count and store active pixels. */
            _count = 0;
            while ((_p < _pixels) && CT_ACTIVE() && (_count < 0xFFFF)) {
                CT_WRITE_PIXEL(_dest);
                CT_INCREMENT_PIXEL();
                _count++;
                _p++;
            }
            ACTIVE_RUN_LENGTH(_runlengths) = (IceTUShort)_count;
#ifdef DEBUG
            _totalcount += _count;
#endif

            _count = 0;
        }
#ifdef CT_PADDING
    }

    _count += CT_SPACE_TOP*CT_FULL_WIDTH;
    if (_count > 0) {
        while (_count > 0xFFFF) {
            INACTIVE_RUN_LENGTH(_dest) = 0xFFFF;
            ACTIVE_RUN_LENGTH(_dest) = 0;
            _dest++;
#ifdef DEBUG
            _totalcount += 0xFFFF;
#endif /*DEBUG*/
            _count -= 0xFFFF;
        }
        INACTIVE_RUN_LENGTH(_dest) = (IceTUShort)_count;
        ACTIVE_RUN_LENGTH(_dest) = 0;
        _dest++;
#ifdef DEBUG
        _totalcount += _count;
#endif /*DEBUG*/
    }
#endif /*CT_PADDING*/

#ifdef DEBUG
    if (_totalcount != (IceTUInt)CT_PIXEL_COUNT) {
        char msg[256];
        sprintf(msg, "Total run lengths don't equal pixel count: %d != %d",
                (int)_totalcount, (int)(CT_PIXEL_COUNT));
        icetRaiseError(msg, ICET_SANITY_CHECK_FAIL);
    }
#endif

    *_compress_time += icetWallTime() - _timer;

    _compressed_size
        = (IceTSizeType)(  (IceTPointerArithmetic)_dest
                         - (IceTPointerArithmetic)CT_COMPRESSED_BUFFER);
    ICET_IMAGE_HEADER(CT_COMPRESSED_BUFFER)[ICET_IMAGE_ACTUAL_BUFFER_SIZE_INDEX]
        = _compressed_size;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#undef CT_COMPRESSED_BUFFER
#undef CT_COLOR_FORMAT
#undef CT_DEPTH_FORMAT
#undef CT_PIXEL_COUNT
#undef CT_ACTIVE
#undef CT_WRITE_PIXEL
#undef CT_INCREMENT_PIXEL
#undef COMPRESSED_SIZE

#ifdef CT_PADDING
#undef CT_PADDING
#undef CT_SPACE_BOTTOM
#undef CT_SPACE_TOP
#undef CT_SPACE_LEFT
#undef CT_SPACE_RIGHT
#undef CT_FULL_WIDTH
#undef CT_FULL_HEIGHT
#endif
