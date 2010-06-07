/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#ifndef _ICET_H_
#define _ICET_H_

#include <IceTConfig.h>

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

typedef IceTUnsignedInt32       IceTEnum;
typedef IceTUnsignedInt32       IceTBitField;
typedef IceTFloat64             IceTDouble;
typedef IceTFloat32             IceTFloat;
typedef IceTInt32               IceTInt;
typedef IceTUnsignedInt32       IceTUInt;
typedef IceTInt16               IceTShort;
typedef IceTUnsignedInt16       IceTUShort;
typedef IceTInt8                IceTByte;
typedef IceTUnsignedInt8        IceTUByte;
typedef IceTUnsignedInt8        IceTBoolean;
typedef void                    IceTVoid;
typedef IceTPointerArithmetic   IceTSizeType;

typedef IceTUnsignedInt32 IceTContext;

struct IceTCommunicatorStruct;

typedef IceTUnsignedInt32 IceTCommRequest;
#define ICET_COMM_REQUEST_NULL ((IceTCommRequest)-1)

struct IceTCommunicatorStruct {
    struct IceTCommunicatorStruct *
         (*Duplicate)(struct IceTCommunicatorStruct *self);
    void (*Destroy)(struct IceTCommunicatorStruct *self);
    void (*Send)(struct IceTCommunicatorStruct *self,
                 const void *buf, int count, IceTEnum datatype, int dest,
                 int tag);
    void (*Recv)(struct IceTCommunicatorStruct *self,
                 void *buf, int count, IceTEnum datatype, int src, int tag);

    void (*Sendrecv)(struct IceTCommunicatorStruct *self,
                     const void *sendbuf, int sendcount, IceTEnum sendtype,
                     int dest, int sendtag,
                     void *recvbuf, int recvcount, IceTEnum recvtype,
                     int src, int recvtag);
    void (*Allgather)(struct IceTCommunicatorStruct *self,
                      const void *sendbuf, int sendcount, int type,
                      void *recvbuf);

    IceTCommRequest (*Isend)(struct IceTCommunicatorStruct *self,
                             const void *buf, int count, IceTEnum datatype,
                             int dest, int tag);
    IceTCommRequest (*Irecv)(struct IceTCommunicatorStruct *self,
                             void *buf, int count, IceTEnum datatype,
                             int src, int tag);

    void (*Wait)(struct IceTCommunicatorStruct *self, IceTCommRequest *request);
    int  (*Waitany)(struct IceTCommunicatorStruct *self,
                    int count, IceTCommRequest *array_of_requests);

    int  (*Comm_size)(struct IceTCommunicatorStruct *self);
    int  (*Comm_rank)(struct IceTCommunicatorStruct *self);
    void *data;
};

typedef struct IceTCommunicatorStruct *IceTCommunicator;

ICET_EXPORT IceTDouble  icetWallTime(void);

ICET_EXPORT IceTContext icetCreateContext(IceTCommunicator comm);
ICET_EXPORT void        icetDestroyContext(IceTContext context);
ICET_EXPORT IceTContext icetGetContext(void);
ICET_EXPORT void        icetSetContext(IceTContext context);
ICET_EXPORT void        icetCopyState(IceTContext dest, const IceTContext src);

typedef void (*IceTCallback)(void);

ICET_EXPORT void icetDrawFunc(IceTCallback func);

#define ICET_BOOLEAN    (IceTEnum)0x8000
#define ICET_BYTE       (IceTEnum)0x8001
#define ICET_SHORT      (IceTEnum)0x8002
#define ICET_INT        (IceTEnum)0x8003
#define ICET_FLOAT      (IceTEnum)0x8004
#define ICET_DOUBLE     (IceTEnum)0x8005
#define ICET_POINTER    (IceTEnum)0x8008
#define ICET_NULL       (IceTEnum)0x0000

#define ICET_FALSE      0
#define ICET_TRUE       1

ICET_EXPORT void icetBoundingVertices(IceTInt size, IceTEnum type,
                                      IceTSizeType stride, IceTSizeType count,
                                      const IceTVoid *pointer);
ICET_EXPORT void icetBoundingBoxd(IceTDouble x_min, IceTDouble x_max,
                                  IceTDouble y_min, IceTDouble y_max,
                                  IceTDouble z_min, IceTDouble z_max);
ICET_EXPORT void icetBoundingBoxf(IceTFloat x_min, IceTFloat x_max,
                                  IceTFloat y_min, IceTFloat y_max,
                                  IceTFloat z_min, IceTFloat z_max);

ICET_EXPORT void icetResetTiles(void);
ICET_EXPORT int  icetAddTile(IceTInt x, IceTInt y,
                             IceTSizeType width, IceTSizeType height,
                             int display_rank);

ICET_EXPORT void icetPhysicalRenderSize(IceTInt width, IceTInt height);

typedef struct { IceTVoid *opaque_internals; } IceTImage;

#define ICET_IMAGE_COLOR_RGBA_UBYTE     (IceTEnum)0xC001
#define ICET_IMAGE_COLOR_RGBA_FLOAT     (IceTEnum)0xC002
#define ICET_IMAGE_COLOR_NONE           (IceTEnum)0x0000

#define ICET_IMAGE_DEPTH_FLOAT          (IceTEnum)0xD001
#define ICET_IMAGE_DEPTH_NONE           (IceTEnum)0x0000

ICET_EXPORT IceTSizeType icetImageBufferSize(IceTEnum color_format,
                                             IceTEnum depth_format,
                                             IceTSizeType num_pixels);
ICET_EXPORT IceTSizeType icetImageMaxBufferSize(IceTSizeType num_pixels);
ICET_EXPORT IceTImage icetImageInitialize(IceTVoid *buffer,
                                          IceTEnum color_format,
                                          IceTEnum depth_format,
                                          IceTSizeType num_pixels);
ICET_EXPORT IceTEnum icetImageGetColorFormat(const IceTImage image);
ICET_EXPORT IceTEnum icetImageGetDepthFormat(const IceTImage image);
ICET_EXPORT IceTSizeType icetImageGetSize(const IceTImage image);
ICET_EXPORT IceTUByte *icetImageGetColorUByte(IceTImage image);
ICET_EXPORT IceTUInt *icetImageGetColorUInt(IceTImage image);
ICET_EXPORT IceTFloat *icetImageGetColorFloat(IceTImage image);
ICET_EXPORT IceTFloat *icetImageGetDepthFloat(IceTImage image);
ICET_EXPORT void icetImageCopyColorUByte(const IceTImage image,
                                         IceTUByte *color_buffer,
                                         IceTEnum color_format);
ICET_EXPORT void icetImageCopyColorFloat(const IceTImage image,
                                         IceTFloat *color_buffer,
                                         IceTEnum color_format);
ICET_EXPORT void icetImageCopyDepthFloat(const IceTImage image,
                                         IceTFloat *depth_buffer,
                                         IceTEnum depth_format);
ICET_EXPORT void icetImageCopyPixels(const IceTImage in_image,
                                     IceTSizeType in_offset,
                                     IceTImage out_image,
                                     IceTSizeType out_offset,
                                     IceTSizeType num_pixels);
ICET_EXPORT void icetImagePackageForSend(IceTImage image,
                                         IceTVoid **buffer,
                                         IceTSizeType *size);
ICET_EXPORT IceTImage icetImageUnpackageFromReceive(IceTVoid *buffer);

typedef struct _IceTStrategy {
    const char *name;
    IceTBoolean supports_ordering;
    IceTImage (*compose)(void);
} IceTStrategy;

ICET_STRATEGY_EXPORT extern IceTStrategy ICET_STRATEGY_DIRECT;
ICET_STRATEGY_EXPORT extern IceTStrategy ICET_STRATEGY_SERIAL;
ICET_STRATEGY_EXPORT extern IceTStrategy ICET_STRATEGY_SPLIT;
ICET_STRATEGY_EXPORT extern IceTStrategy ICET_STRATEGY_REDUCE;
ICET_STRATEGY_EXPORT extern IceTStrategy ICET_STRATEGY_VTREE;

ICET_EXPORT void icetStrategy(IceTStrategy strategy);

ICET_EXPORT const char *icetGetStrategyName(void);

#define ICET_COMPOSITE_MODE_Z_BUFFER    (IceTEnum)0x0301
#define ICET_COMPOSITE_MODE_BLEND       (IceTEnum)0x0302
ICET_EXPORT void icetCompositeMode(IceTEnum mode);

ICET_EXPORT void icetCompositeOrder(const IceTInt *process_ranks);

ICET_EXPORT void icetDataReplicationGroup(IceTInt size,
                                          const IceTInt *processes);
ICET_EXPORT void icetDataReplicationGroupColor(IceTInt color);

ICET_EXPORT IceTImage icetDrawFrame(void);

#define ICET_DIAG_OFF           (IceTEnum)0x0000
#define ICET_DIAG_ERRORS        (IceTEnum)0x0001
#define ICET_DIAG_WARNINGS      (IceTEnum)0x0003
#define ICET_DIAG_DEBUG         (IceTEnum)0x0007
#define ICET_DIAG_ROOT_NODE     (IceTEnum)0x0000
#define ICET_DIAG_ALL_NODES     (IceTEnum)0x0100
#define ICET_DIAG_FULL          (IceTEnum)0xFFFF

ICET_EXPORT void icetDiagnostics(IceTBitField mask);


#define ICET_STATE_ENGINE_START (IceTEnum)0x00000000

#define ICET_DIAGNOSTIC_LEVEL   (ICET_STATE_ENGINE_START | (IceTEnum)0x0001)
#define ICET_RANK               (ICET_STATE_ENGINE_START | (IceTEnum)0x0002)
#define ICET_NUM_PROCESSES      (ICET_STATE_ENGINE_START | (IceTEnum)0x0003)
/* #define ICET_ABSOLUTE_FAR_DEPTH (ICET_STATE_ENGINE_START | (IceTEnum)0x0004) */
#define ICET_BACKGROUND_COLOR   (ICET_STATE_ENGINE_START | (IceTEnum)0x0005)
#define ICET_BACKGROUND_COLOR_WORD (ICET_STATE_ENGINE_START | (IceTEnum)0x0006)
#define ICET_PHYSICAL_RENDER_WIDTH (ICET_STATE_ENGINE_START | (IceTEnum)0x0007)
#define ICET_PHYSICAL_RENDER_HEIGHT (ICET_STATE_ENGINE_START| (IceTEnum)0x0008)

#define ICET_NUM_TILES          (ICET_STATE_ENGINE_START | (IceTEnum)0x0010)
#define ICET_TILE_VIEWPORTS     (ICET_STATE_ENGINE_START | (IceTEnum)0x0011)
#define ICET_GLOBAL_VIEWPORT    (ICET_STATE_ENGINE_START | (IceTEnum)0x0012)
#define ICET_TILE_MAX_WIDTH     (ICET_STATE_ENGINE_START | (IceTEnum)0x0013)
#define ICET_TILE_MAX_HEIGHT    (ICET_STATE_ENGINE_START | (IceTEnum)0x0014)
#define ICET_TILE_MAX_PIXELS    (ICET_STATE_ENGINE_START | (IceTEnum)0x0015)
#define ICET_DISPLAY_NODES      (ICET_STATE_ENGINE_START | (IceTEnum)0x001A)
#define ICET_TILE_DISPLAYED     (ICET_STATE_ENGINE_START | (IceTEnum)0x001B)

#define ICET_GEOMETRY_BOUNDS    (ICET_STATE_ENGINE_START | (IceTEnum)0x0022)
#define ICET_NUM_BOUNDING_VERTS (ICET_STATE_ENGINE_START | (IceTEnum)0x0023)
#define ICET_STRATEGY_NAME      (ICET_STATE_ENGINE_START | (IceTEnum)0x0024)
#define ICET_STRATEGY_COMPOSE   (ICET_STATE_ENGINE_START | (IceTEnum)0x0025)
#define ICET_COMPOSITE_MODE     (ICET_STATE_ENGINE_START | (IceTEnum)0x0026)
#define ICET_COMPOSITE_ORDER    (ICET_STATE_ENGINE_START | (IceTEnum)0x0028)
#define ICET_PROCESS_ORDERS     (ICET_STATE_ENGINE_START | (IceTEnum)0x0029)
#define ICET_STRATEGY_SUPPORTS_ORDERING (ICET_STATE_ENGINE_START | (IceTEnum)0x002A)
#define ICET_DATA_REPLICATION_GROUP (ICET_STATE_ENGINE_START | (IceTEnum)0x002B)
#define ICET_DATA_REPLICATION_GROUP_SIZE (ICET_STATE_ENGINE_START | (IceTEnum)0x002C)

#define ICET_DRAW_FUNCTION      (ICET_STATE_ENGINE_START | (IceTEnum)0x0060)
#define ICET_FRAME_COUNT        (ICET_STATE_ENGINE_START | (IceTEnum)0x0063)

#define ICET_STATE_FRAME_START  (IceTEnum)0x00000080

#define ICET_IS_DRAWING_FRAME   (ICET_STATE_FRAME_START | (IceTEnum)0x0000)
#define ICET_PROJECTION_MATRIX  (ICET_STATE_FRAME_START | (IceTEnum)0x0001)
#define ICET_CONTAINED_VIEWPORT (ICET_STATE_FRAME_START | (IceTEnum)0x0002)
#define ICET_NEAR_DEPTH         (ICET_STATE_FRAME_START | (IceTEnum)0x0003)
#define ICET_FAR_DEPTH          (ICET_STATE_FRAME_START | (IceTEnum)0x0004)
#define ICET_NUM_CONTAINED_TILES (ICET_STATE_FRAME_START| (IceTEnum)0x0005)
#define ICET_CONTAINED_TILES_LIST (ICET_STATE_FRAME_START|(IceTEnum)0x0006)
#define ICET_CONTAINED_TILES_MASK (ICET_STATE_FRAME_START|(IceTEnum)0x0007)
#define ICET_ALL_CONTAINED_TILES_MASKS (ICET_STATE_FRAME_START|(IceTEnum)0x0008)
#define ICET_TILE_CONTRIB_COUNTS (ICET_STATE_FRAME_START| (IceTEnum)0x0009)
#define ICET_TOTAL_IMAGE_COUNT  (ICET_STATE_FRAME_START | (IceTEnum)0x000A)

#define ICET_RENDERED_VIEWPORT  (ICET_STATE_FRAME_START | (IceTEnum)0x0010)

#define ICET_STATE_TIMING_START (IceTEnum)0x000000C0

#define ICET_RENDER_TIME        (ICET_STATE_TIMING_START | (IceTEnum)0x0001)
#define ICET_BUFFER_READ_TIME   (ICET_STATE_TIMING_START | (IceTEnum)0x0002)
#define ICET_BUFFER_WRITE_TIME  (ICET_STATE_TIMING_START | (IceTEnum)0x0003)
#define ICET_COMPRESS_TIME      (ICET_STATE_TIMING_START | (IceTEnum)0x0004)
#define ICET_COMPARE_TIME       (ICET_STATE_TIMING_START | (IceTEnum)0x0005)
#define ICET_BLEND_TIME         ICET_COMPARE_TIME
#define ICET_COMPOSITE_TIME     (ICET_STATE_TIMING_START | (IceTEnum)0x0006)
#define ICET_TOTAL_DRAW_TIME    (ICET_STATE_TIMING_START | (IceTEnum)0x0007)
#define ICET_BYTES_SENT         (ICET_STATE_TIMING_START | (IceTEnum)0x0010)

#define ICET_STATE_ENABLE_START (IceTEnum)0x00000100
#define ICET_STATE_ENABLE_END   (IceTEnum)0x00000120

#define ICET_FLOATING_VIEWPORT  (ICET_STATE_ENABLE_START | (IceTEnum)0x0001)
#define ICET_ORDERED_COMPOSITE  (ICET_STATE_ENABLE_START | (IceTEnum)0x0002)
#define ICET_CORRECT_COLORED_BACKGROUND (ICET_STATE_ENABLE_START | (IceTEnum)0x0003)
#define ICET_COMPOSITE_ONE_BUFFER (ICET_STATE_ENABLE_START | (IceTEnum)0x0004)

/* These should go to GL layer. */
#define ICET_DISPLAY            (ICET_STATE_ENABLE_START | (IceTEnum)0x0010)
#define ICET_DISPLAY_COLORED_BACKGROUND (ICET_STATE_ENABLE_START | (IceTEnum)0x0011)
#define ICET_DISPLAY_INFLATE    (ICET_STATE_ENABLE_START | (IceTEnum)0x0012)
#define ICET_DISPLAY_INFLATE_WITH_HARDWARE (ICET_STATE_ENABLE_START | (IceTEnum)0x0013)

#define ICET_STATE_SIZE         (IceTEnum)0x00000200

ICET_EXPORT void icetGetDoublev(IceTEnum pname, IceTDouble *params);
ICET_EXPORT void icetGetFloatv(IceTEnum pname, IceTFloat *params);
ICET_EXPORT void icetGetIntegerv(IceTEnum pname, IceTInt *params);
ICET_EXPORT void icetGetBooleanv(IceTEnum pname, IceTBoolean *params);
ICET_EXPORT void icetGetEnumv(IceTEnum pname, IceTEnum *params);
ICET_EXPORT void icetGetBitFieldv(IceTEnum pname, IceTEnum *bitfield);
ICET_EXPORT void icetGetPointerv(IceTEnum pname, IceTVoid **params);

ICET_EXPORT void icetEnable(IceTEnum pname);
ICET_EXPORT void icetDisable(IceTEnum pname);
ICET_EXPORT IceTBoolean icetIsEnabled(IceTEnum pname);

#define ICET_NO_ERROR           (IceTEnum)0x00000000
#define ICET_SANITY_CHECK_FAIL  (IceTEnum)0xFFFFFFFF
#define ICET_INVALID_ENUM       (IceTEnum)0xFFFFFFFE
#define ICET_BAD_CAST           (IceTEnum)0xFFFFFFFD
#define ICET_OUT_OF_MEMORY      (IceTEnum)0xFFFFFFFC
#define ICET_INVALID_OPERATION  (IceTEnum)0xFFFFFFFB
#define ICET_INVALID_VALUE      (IceTEnum)0xFFFFFFFA

ICET_EXPORT IceTEnum icetGetError(void);

#ifdef __cplusplus
}
#endif

#endif /* _ICET_H_ */
