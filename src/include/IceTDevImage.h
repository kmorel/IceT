/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#ifndef _ICET_IMAGE_H_
#define _ICET_IMAGE_H_

#include <IceT.h>
#include <IceTDevState.h>

ICET_EXPORT IceTImage       icetGetStateBufferImage(IceTEnum pname,
                                                    IceTSizeType width,
                                                    IceTSizeType height);
ICET_EXPORT IceTSizeType icetImageBufferSize(IceTSizeType width,
                                             IceTSizeType height);
ICET_EXPORT IceTSizeType icetImageBufferSizeType(IceTEnum color_format,
                                                 IceTEnum depth_format,
                                                 IceTSizeType width,
                                                 IceTSizeType height);
ICET_EXPORT IceTImage icetImageAssignBuffer(IceTVoid *buffer,
                                            IceTSizeType width,
                                            IceTSizeType height);
ICET_EXPORT void icetImageAdjustForOutput(IceTImage image);
ICET_EXPORT void icetImageAdjustForInput(IceTImage image);
ICET_EXPORT void icetImageSetDimensions(IceTImage image,
                                        IceTSizeType width,
                                        IceTSizeType height);
ICET_EXPORT IceTVoid *icetImageGetColorVoid(IceTImage image,
                                            IceTSizeType *pixel_size);
ICET_EXPORT const IceTVoid *icetImageGetColorConstVoid(
                                                      const IceTImage image,
                                                      IceTSizeType *pixel_size);
ICET_EXPORT IceTVoid *icetImageGetDepthVoid(IceTImage image,
                                            IceTSizeType *pixel_size);
ICET_EXPORT const IceTVoid *icetImageGetDepthConstVoid(
                                                      const IceTImage image,
                                                      IceTSizeType *pixel_size);
ICET_EXPORT IceTBoolean icetImageEqual(const IceTImage image1,
                                       const IceTImage image2);
ICET_EXPORT void icetImageCopyPixels(const IceTImage in_image,
                                     IceTSizeType in_offset,
                                     IceTImage out_image,
                                     IceTSizeType out_offset,
                                     IceTSizeType num_pixels);
ICET_EXPORT void icetImageCopyRegion(const IceTImage in_image,
                                     const IceTInt *in_viewport,
                                     IceTImage out_image,
                                     const IceTInt *out_viewport);
ICET_EXPORT void icetImageClearAroundRegion(IceTImage image,
                                            const IceTInt *region);
ICET_EXPORT void icetImagePackageForSend(IceTImage image,
                                         IceTVoid **buffer,
                                         IceTSizeType *size);
ICET_EXPORT IceTImage icetImageUnpackageFromReceive(IceTVoid *buffer);

typedef struct { IceTVoid *opaque_internals; } IceTSparseImage;

ICET_EXPORT IceTSizeType icetSparseImageBufferSize(IceTSizeType width,
                                                   IceTSizeType height);
ICET_EXPORT IceTSizeType icetSparseImageBufferSizeType(IceTEnum color_format,
                                                       IceTEnum depth_format,
                                                       IceTSizeType width,
                                                       IceTSizeType height);
ICET_EXPORT IceTSparseImage icetGetStateBufferSparseImage(IceTEnum pname,
                                                          IceTSizeType width,
                                                          IceTSizeType height);
ICET_EXPORT IceTSparseImage icetSparseImageAssignBuffer(IceTVoid *buffer,
                                                        IceTSizeType width,
                                                        IceTSizeType height);
ICET_EXPORT IceTEnum icetSparseImageGetColorFormat(const IceTSparseImage image);
ICET_EXPORT IceTEnum icetSparseImageGetDepthFormat(const IceTSparseImage image);
ICET_EXPORT IceTSizeType icetSparseImageGetWidth(const IceTSparseImage image);
ICET_EXPORT IceTSizeType icetSparseImageGetHeight(const IceTSparseImage image);
ICET_EXPORT IceTSizeType icetSparseImageGetNumPixels(
                                                   const IceTSparseImage image);
ICET_EXPORT void icetSparseImageSetDimensions(IceTSparseImage image,
                                              IceTSizeType width,
                                              IceTSizeType height);
ICET_EXPORT IceTSizeType icetSparseImageGetCompressedBufferSize(
                                                   const IceTSparseImage image);
ICET_EXPORT void icetSparseImagePackageForSend(IceTSparseImage image,
                                               IceTVoid **buffer,
                                               IceTSizeType *size);
ICET_EXPORT IceTSparseImage icetSparseImageUnpackageFromReceive(
                                                              IceTVoid *buffer);

ICET_EXPORT void icetClearImage(IceTImage image);
ICET_EXPORT void icetClearSparseImage(IceTSparseImage image);

ICET_EXPORT void icetGetTileImage(IceTInt tile, IceTImage image);

ICET_EXPORT void icetGetCompressedTileImage(IceTInt tile,
                                            IceTSparseImage compressed_image);

ICET_EXPORT void icetCompressImage(const IceTImage image,
                                   IceTSparseImage compressed_image);

ICET_EXPORT void icetCompressSubImage(const IceTImage image,
                                      IceTSizeType offset,
                                      IceTSizeType pixels,
                                      IceTSparseImage compressed_image);

ICET_EXPORT void icetDecompressImage(const IceTSparseImage compressed_image,
                                     IceTImage image);

ICET_EXPORT void icetComposite(IceTImage destBuffer,
                               const IceTImage srcBuffer,
                               int srcOnTop);

ICET_EXPORT void icetCompressedComposite(IceTImage destBuffer,
                                         const IceTSparseImage srcBuffer,
                                         int srcOnTop);

ICET_EXPORT void icetCompressedSubComposite(IceTImage destBuffer,
                                            IceTSizeType offset,
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
