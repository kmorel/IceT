/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#ifndef _ICET_IMAGE_H_
#define _ICET_IMAGE_H_

#include <IceT.h>
#include "state.h"

typedef IceTUInt *IceTSparseImage;

#define FULL_IMAGE_BASE_MAGIC_NUM       0x004D5000
#define FULL_IMAGE_C_MAGIC_NUM          (FULL_IMAGE_BASE_MAGIC_NUM | ICET_COLOR_BUFFER_BIT)
#define FULL_IMAGE_D_MAGIC_NUM          (FULL_IMAGE_BASE_MAGIC_NUM | ICET_DEPTH_BUFFER_BIT)
#define FULL_IMAGE_CD_MAGIC_NUM         (FULL_IMAGE_BASE_MAGIC_NUM | ICET_COLOR_BUFFER_BIT | ICET_DEPTH_BUFFER_BIT)

#define SPARSE_IMAGE_BASE_MAGIC_NUM     0x004D6000
#define SPARSE_IMAGE_C_MAGIC_NUM        (SPARSE_IMAGE_BASE_MAGIC_NUM | ICET_COLOR_BUFFER_BIT)
#define SPARSE_IMAGE_D_MAGIC_NUM        (SPARSE_IMAGE_BASE_MAGIC_NUM | ICET_DEPTH_BUFFER_BIT)
#define SPARSE_IMAGE_CD_MAGIC_NUM       (SPARSE_IMAGE_BASE_MAGIC_NUM | ICET_COLOR_BUFFER_BIT | ICET_DEPTH_BUFFER_BIT)

/* Returns the size of buffers (in bytes) needed to hold data for images
   for the given number of pixels. */
#define icetFullImageSize(pixels)                                       \
    icetFullImageTypeSize((pixels),                                     \
                            *(icetUnsafeStateGetInteger(ICET_INPUT_BUFFERS))\
                          | FULL_IMAGE_BASE_MAGIC_NUM)
#define icetSparseImageSize(pixels)                                     \
    icetSparseImageTypeSize((pixels),                                   \
                            *(icetUnsafeStateGetInteger(ICET_INPUT_BUFFERS))\
                          | SPARSE_IMAGE_BASE_MAGIC_NUM)

ICET_EXPORT IceTUInt icetFullImageTypeSize(IceTUInt pixels, IceTUInt type);
ICET_EXPORT IceTUInt icetSparseImageTypeSize(IceTUInt pixels, IceTUInt type);

/* Returns the color or depth buffer in a full image. */
ICET_EXPORT IceTUByte *icetGetImageColorBuffer(IceTImage image);
ICET_EXPORT IceTUInt  *icetGetImageDepthBuffer(IceTImage image);

/* Returns the count of pixels in a full or sparse image. */
/* IceTUInt icetGetImagePixelCount(IceTUInt *image); */
#define icetGetImagePixelCount(image)   ((image)[1])

/* Sets up the magic number based on ICET_INPUT_BUFFERS and pixel_count. */
ICET_EXPORT void   icetInitializeImage(IceTImage image, IceTUInt pixel_count);
ICET_EXPORT void   icetInitializeImageType(IceTImage image, IceTUInt pixel_count,
                                           IceTUInt type);

/* Clears the buffers specified in ICET_OUTPUT_BUFFERS. */
ICET_EXPORT void   icetClearImage(IceTImage image);

ICET_EXPORT void   icetGetTileImage(IceTInt tile, IceTImage buffer);

ICET_EXPORT IceTUInt icetGetCompressedTileImage(IceTInt tile,
                                                IceTSparseImage buffer);

ICET_EXPORT IceTUInt icetCompressImage(const IceTImage imageBuffer,
                                       IceTSparseImage compressedBuffer);

ICET_EXPORT IceTUInt icetCompressSubImage(const IceTImage imageBuffer,
                                          IceTUInt offset, IceTUInt pixels,
                                          IceTSparseImage compressedBuffer);

ICET_EXPORT IceTUInt icetDecompressImage(const IceTSparseImage compressedBuffer,
                                         IceTImage imageBuffer);

ICET_EXPORT void   icetComposite(IceTImage destBuffer,
                                 const IceTImage srcBuffer,
                                 int srcOnTop);

ICET_EXPORT void   icetCompressedComposite(IceTImage destBuffer,
                                           const IceTSparseImage srcBuffer,
                                           int srcOnTop);

ICET_EXPORT void   icetCompressedSubComposite(IceTImage destBuffer,
                                              IceTUInt offset, IceTUInt pixels,
                                              const IceTSparseImage srcBuffer,
                                              int srcOnTop);

#define ICET_OVER(src, dest)                                            \
{                                                                       \
    IceTUInt dfactor = 255 - (src)[3];                                  \
    (dest)[0] = (IceTUByte)(((dest)[0]*dfactor)/255 + (src)[0]);        \
    (dest)[1] = (IceTUByte)(((dest)[1]*dfactor)/255 + (src)[1]);        \
    (dest)[2] = (IceTUByte)(((dest)[2]*dfactor)/255 + (src)[2]);        \
    (dest)[3] = (IceTUByte)(((dest)[3]*dfactor)/255 + (src)[3]);        \
}

#define ICET_UNDER(src, dest)                                           \
{                                                                       \
    IceTUInt sfactor = 255 - (dest)[3];                                 \
    (dest)[0] = (IceTUByte)((dest)[0] + ((src)[0]*sfactor)/255);        \
    (dest)[1] = (IceTUByte)((dest)[1] + ((src)[1]*sfactor)/255);        \
    (dest)[2] = (IceTUByte)((dest)[2] + ((src)[2]*sfactor)/255);        \
    (dest)[3] = (IceTUByte)((dest)[3] + ((src)[3]*sfactor)/255);        \
}

#endif /* _ICET_IMAGE_H_ */
