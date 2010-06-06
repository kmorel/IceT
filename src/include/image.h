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

typedef struct { IceTVoid *opaque_internals; } IceTSparseImage;

ICET_EXPORT IceTSizeType icetSparseImageBufferSize(IceTEnum color_format,
                                                   IceTEnum depth_format,
                                                   IceTSizeType num_pixels);
ICET_EXPORT IceTSparseImage icetSparseImageInitialize(IceTVoid *buffer,
                                                      IceTEnum color_format,
                                                      IceTEnum depth_format,
                                                      IceTSizeType num_pixels);
ICET_EXPORT IceTEnum icetSparseImageGetColorFormat(const IceTSparseImage image);
ICET_EXPORT IceTEnum icetSparseImageGetDepthFormat(const IceTSparseImage image);
ICET_EXPORT IceTSizeType icetSparseImageGetSize(const IceTSparseImage image);
ICET_EXPORT IceTSizeType icetSparseImageCompressedBufferSize(
                                                   const IceTSparseImage image);
ICET_EXPORT void icetSparseImagePackageForSend(IceTSparseImage image,
                                               IceTVoid **buffer,
                                               IceTSizeType *size);
ICET_EXPORT IceTSparseImage icetSparseImageUnpackageFromReceive(
                                                              IceTVoid *buffer);

ICET_EXPORT void icetClearImage(IceTImage image);

ICET_EXPORT IceTImage icetGetTileImage(IceTInt tile, IceTVoid *buffer);

ICET_EXPORT IceTSparseImage icetGetCompressedTileImage(IceTInt tile,
                                                       IceTVoid *buffer);

ICET_EXPORT IceTSparseImage icetCompressImage(const IceTImage image,
                                              IceTVoid *compressedBuffer);

ICET_EXPORT IceTSparseImage icetCompressSubImage(const IceTImage image,
                                                 IceTSizeType offset,
                                                 IceTSizeType pixels,
                                                 IceTVoid *compressedBuffer);

ICET_EXPORT IceTImage icetDecompressImage(const IceTSparseImage compressedImage,
                                          IceTVoid *imageBuffer);

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

#define ICET_OVER_UBYTE(src, dest)                                      \
{                                                                       \
    IceTUInt dfactor = 255 - (src)[3];                                  \
    (dest)[0] = (IceTUByte)(((dest)[0]*dfactor)/255 + (src)[0]);        \
    (dest)[1] = (IceTUByte)(((dest)[1]*dfactor)/255 + (src)[1]);        \
    (dest)[2] = (IceTUByte)(((dest)[2]*dfactor)/255 + (src)[2]);        \
    (dest)[3] = (IceTUByte)(((dest)[3]*dfactor)/255 + (src)[3]);        \
}

#define ICET_UNDER_UBYTE(src, dest)                                     \
{                                                                       \
    IceTUInt sfactor = 255 - (dest)[3];                                 \
    (dest)[0] = (IceTUByte)((dest)[0] + ((src)[0]*sfactor)/255);        \
    (dest)[1] = (IceTUByte)((dest)[1] + ((src)[1]*sfactor)/255);        \
    (dest)[2] = (IceTUByte)((dest)[2] + ((src)[2]*sfactor)/255);        \
    (dest)[3] = (IceTUByte)((dest)[3] + ((src)[3]*sfactor)/255);        \
}

#define ICET_OVER_FLOAT(src, dest)                                      \
{                                                                       \
    IceTFloat dfactor = 1.0f - (src)[3];                                \
    (dest)[0] = (dest)[0]*dfactor + (src)[0];                           \
    (dest)[1] = (dest)[1]*dfactor + (src)[1];                           \
    (dest)[2] = (dest)[2]*dfactor + (src)[2];                           \
    (dest)[3] = (dest)[3]*dfactor + (src)[3];                           \
}

#define ICET_UNDER_FLOAT(src, dest)                                     \
{                                                                       \
    IceTFloat sfactor = 1.0f - (dest)[3];                               \
    (dest)[0] = (dest)[0] + (src)[0]*sfactor;                           \
    (dest)[1] = (dest)[1] + (src)[1]*sfactor;                           \
    (dest)[2] = (dest)[2] + (src)[2]*sfactor;                           \
    (dest)[3] = (dest)[3] + (src)[3]*sfactor;                           \
}

#endif /* _ICET_IMAGE_H_ */
