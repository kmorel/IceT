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

#ifndef _ICET_IMAGE_H_
#define _ICET_IMAGE_H_

#include <GL/ice-t.h>
#include "state.h"

typedef GLuint *IceTSparseImage;

#define FULL_IMAGE_BASE_MAGIC_NUM	0x004D5000
#define FULL_IMAGE_C_MAGIC_NUM		(FULL_IMAGE_BASE_MAGIC_NUM | ICET_COLOR_BUFFER_BIT)
#define FULL_IMAGE_D_MAGIC_NUM		(FULL_IMAGE_BASE_MAGIC_NUM | ICET_DEPTH_BUFFER_BIT)
#define FULL_IMAGE_CD_MAGIC_NUM		(FULL_IMAGE_BASE_MAGIC_NUM | ICET_COLOR_BUFFER_BIT | ICET_DEPTH_BUFFER_BIT)

#define SPARSE_IMAGE_BASE_MAGIC_NUM	0x004D6000
#define SPARSE_IMAGE_C_MAGIC_NUM	(SPARSE_IMAGE_BASE_MAGIC_NUM | ICET_COLOR_BUFFER_BIT)
#define SPARSE_IMAGE_D_MAGIC_NUM	(SPARSE_IMAGE_BASE_MAGIC_NUM | ICET_DEPTH_BUFFER_BIT)
#define SPARSE_IMAGE_CD_MAGIC_NUM	(SPARSE_IMAGE_BASE_MAGIC_NUM | ICET_COLOR_BUFFER_BIT | ICET_DEPTH_BUFFER_BIT)

/* Returns the size of buffers (in bytes) needed to hold data for images
   for the given number of pixels. */
#define icetFullImageSize(pixels)					\
    icetFullImageTypeSize((pixels),					\
			    *((GLuint *)icetUnsafeStateGet(ICET_INPUT_BUFFERS))\
			  | FULL_IMAGE_BASE_MAGIC_NUM)
#define icetSparseImageSize(pixels)					\
    icetSparseImageTypeSize((pixels),					\
			    *((GLuint *)icetUnsafeStateGet(ICET_INPUT_BUFFERS))\
			  | SPARSE_IMAGE_BASE_MAGIC_NUM)

ICET_EXPORT GLuint icetFullImageTypeSize(GLuint pixels, GLuint type);
ICET_EXPORT GLuint icetSparseImageTypeSize(GLuint pixels, GLuint type);

/* Returns the color or depth buffer in a full image. */
ICET_EXPORT GLubyte *icetGetImageColorBuffer(IceTImage image);
ICET_EXPORT GLuint  *icetGetImageDepthBuffer(IceTImage image);

/* Returns the count of pixels in a full or sparse image. */
/* GLuint icetGetImagePixelCount(GLuint *image); */
#define icetGetImagePixelCount(image)	((image)[1])

/* Sets up the magic number based on ICET_INPUT_BUFFERS and pixel_count. */
ICET_EXPORT void   icetInitializeImage(IceTImage image, GLuint pixel_count);
ICET_EXPORT void   icetInitializeImageType(IceTImage image, GLuint pixel_count,
					   GLuint type);

/* Clears the buffers specified in ICET_OUTPUT_BUFFERS. */
ICET_EXPORT void   icetClearImage(IceTImage image);

ICET_EXPORT void   icetGetTileImage(GLint tile, IceTImage buffer);

ICET_EXPORT GLuint icetGetCompressedTileImage(GLint tile,
					      IceTSparseImage buffer);

ICET_EXPORT GLuint icetCompressImage(const IceTImage imageBuffer,
				     IceTSparseImage compressedBuffer);

ICET_EXPORT GLuint icetCompressSubImage(const IceTImage imageBuffer,
					GLuint offset, GLuint pixels,
					IceTSparseImage compressedBuffer);

ICET_EXPORT GLuint icetDecompressImage(const IceTSparseImage compressedBuffer,
				       IceTImage imageBuffer);

ICET_EXPORT void   icetComposite(IceTImage destBuffer,
				 const IceTImage srcBuffer,
				 int srcOnTop);

ICET_EXPORT void   icetCompressedComposite(IceTImage destBuffer,
					   const IceTSparseImage srcBuffer,
					   int srcOnTop);

ICET_EXPORT void   icetCompressedSubComposite(IceTImage destBuffer,
					      GLuint offset, GLuint pixels,
					      const IceTSparseImage srcBuffer,
					      int srcOnTop);

#endif /* _ICET_IMAGE_H_ */
