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

/* This is not a traditional header file, but rather a "macro" file that
 * defines the body of a decompression function.  In general, there are
 * many flavors of the decompression functionality which differ only
 * slightly.  Rather than maintain lots of different code bases or try to
 * debug big macros, we just include this file with various parameters.
 *
 * The following macros must be defined:
 *	COMPRESSED_BUFFER - the buffer that holds the compressed image.
 *	READ_PIXEL(pointer) - reads the current pixel from the pointer and
 *		increments the pointer.
 *	INCREMENT_PIXEL() - Increments to the next pixel.
 *	INCREMENT_INACTIVE_PIXELS(count) - Increments over count pixels,
 *		setting them all to appropriate inactive values.
 *
 * The following macros are optional:
 *	TIME_DECOMPRESSION - if defined, the time to perform the
 *		decompression is added to the total compress time.
 *
 * All of the above macros are undefined at the end of this file.
 */

#ifndef GET_MAGIC_NUM
#error Need GET_MAGIC_NUM macro.  Is this included in image.c?
#endif
#ifndef GET_PIXEL_COUNT
#error Need GET_PIXEL_COUNT macro.  Is this included in image.c?
#endif
#ifndef GET_DATA_START
#error Need GET_DATA_START macro.  Is this included in image.c?
#endif
#ifndef INACTIVE_RUN_LENGTH
#error Need INACTIVE_RUN_LENGTH macro.  Is this included in image.c?
#endif
#ifndef ACTIVE_RUN_LENGTH
#error Need ACTIVE_RUN_LENGTH macro.  Is this included in image.c?
#endif

{
    const GLuint *_src;
    GLuint _pixels;
    GLuint _p;
    GLuint _i;
#ifdef TIME_DECOMPRESSION
    GLdouble _timer;
    GLdouble *_compress_time;

    _compress_time = icetUnsafeStateGet(ICET_COMPRESS_TIME);
    _timer = icetWallTime();
#endif /* TIME_DECOMPRESSION */

    _pixels = GET_PIXEL_COUNT(COMPRESSED_BUFFER);
    _src = GET_DATA_START(COMPRESSED_BUFFER);

    _p = 0;
    while (_p < _pixels) {
	const GLuint *_runlengths = _src++;
	GLushort _rl;
      /* Set background pixels. */
	_rl = INACTIVE_RUN_LENGTH(*_runlengths);
	_p += _rl;
	if (_p > _pixels) {
	    icetRaiseError("Corrupt compressed image.", ICET_INVALID_VALUE);
	    break;
	}
	INCREMENT_INACTIVE_PIXELS(_rl);

      /* Set active pixels. */
	_rl = ACTIVE_RUN_LENGTH(*_runlengths);
	_p += _rl;
	if (_p > _pixels) {
	    icetRaiseError("Corrupt compressed image.", ICET_INVALID_VALUE);
	    break;
	}
	for (_i = 0; _i < _rl; _i++) {
	    READ_PIXEL(_src);
	    INCREMENT_PIXEL();
	}
    }

#ifdef TIME_DECOMPRESSION
    *_compress_time += icetWallTime() - _timer;
#endif
}

#undef COMPRESSED_BUFFER
#undef READ_PIXEL
#undef INCREMENT_PIXEL
#undef INCREMENT_INACTIVE_PIXELS

#ifdef TIME_DECOMPRESSION
#undef TIME_DECOMPRESSION
#endif
