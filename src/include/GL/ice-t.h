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

#ifndef _ICET_H_
#define _ICET_H_

#include <GL/ice-t_config.h>

#include <stdlib.h>

#include <GL/gl.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

typedef int IceTContext;

struct IceTCommunicatorStruct;

typedef int IceTCommRequest;
#define ICET_COMM_REQUEST_NULL ((IceTCommRequest)-1)

struct IceTCommunicatorStruct {
    struct IceTCommunicatorStruct *
	 (*Duplicate)(struct IceTCommunicatorStruct *self);
    void (*Destroy)(struct IceTCommunicatorStruct *self);
    void (*Send)(struct IceTCommunicatorStruct *self,
		 const void *buf, int count, GLenum datatype, int dest,
		 int tag);
    void (*Recv)(struct IceTCommunicatorStruct *self,
		 void *buf, int count, GLenum datatype, int src, int tag);

    void (*Sendrecv)(struct IceTCommunicatorStruct *self,
		     const void *sendbuf, int sendcount, GLenum sendtype,
		     int dest, int sendtag,
		     void *recvbuf, int recvcount, GLenum recvtype,
		     int src, int recvtag);
    void (*Allgather)(struct IceTCommunicatorStruct *self,
		      const void *sendbuf, int sendcount, int type,
		      void *recvbuf);

    IceTCommRequest (*Isend)(struct IceTCommunicatorStruct *self,
			     const void *buf, int count, GLenum datatype,
			     int dest, int tag);
    IceTCommRequest (*Irecv)(struct IceTCommunicatorStruct *self,
			     void *buf, int count, GLenum datatype,
			     int src, int tag);

    void (*Wait)(struct IceTCommunicatorStruct *self, IceTCommRequest *request);
    int  (*Waitany)(struct IceTCommunicatorStruct *self,
		    int count, IceTCommRequest *array_of_requests);

    int	 (*Comm_size)(struct IceTCommunicatorStruct *self);
    int	 (*Comm_rank)(struct IceTCommunicatorStruct *self);
    void *data;
};

typedef struct IceTCommunicatorStruct *IceTCommunicator;

ICET_EXPORT double	icetWallTime(void);

ICET_EXPORT IceTContext	icetCreateContext(IceTCommunicator comm);
ICET_EXPORT void	icetDestroyContext(IceTContext context);
ICET_EXPORT IceTContext	icetGetContext(void);
ICET_EXPORT void	icetSetContext(IceTContext context);
ICET_EXPORT void	icetCopyState(IceTContext dest, const IceTContext src);

typedef void (*IceTCallback)(void);

ICET_EXPORT void icetDrawFunc(IceTCallback func);

#define ICET_BOOLEAN	0x8000
#define ICET_BYTE	0x8001
#define ICET_SHORT	0x8002
#define ICET_INT	0x8003
#define ICET_FLOAT	0x8004
#define ICET_DOUBLE	0x8005
#define ICET_POINTER	0x8008
#define ICET_NULL	0x0000

#define ICET_FALSE	0
#define ICET_TRUE	1

ICET_EXPORT void icetBoundingVertices(GLint size, GLenum type, GLsizei stride,
				      GLsizei count, const GLvoid *pointer);
ICET_EXPORT void icetBoundingBoxd(GLdouble x_min, GLdouble x_max,
				  GLdouble y_min, GLdouble y_max,
				  GLdouble z_min, GLdouble z_max);
ICET_EXPORT void icetBoundingBoxf(GLfloat x_min, GLfloat x_max,
				  GLfloat y_min, GLfloat y_max,
				  GLfloat z_min, GLfloat z_max);

ICET_EXPORT void icetResetTiles(void);
ICET_EXPORT int  icetAddTile(GLint x, GLint y, GLsizei width, GLsizei height,
			     int display_rank);

ICET_EXPORT void icetDrawFrame(void);

ICET_EXPORT GLubyte *icetGetColorBuffer(void);
ICET_EXPORT GLuint  *icetGetDepthBuffer(void);

typedef GLuint *IceTImage;
typedef struct _IceTStrategy {
    char *name;
    GLboolean supports_ordering;
    IceTImage (*compose)(void);
} IceTStrategy;

ICET_STRATEGY_EXPORT extern IceTStrategy ICET_STRATEGY_DIRECT;
ICET_STRATEGY_EXPORT extern IceTStrategy ICET_STRATEGY_SERIAL;
ICET_STRATEGY_EXPORT extern IceTStrategy ICET_STRATEGY_SPLIT;
ICET_STRATEGY_EXPORT extern IceTStrategy ICET_STRATEGY_REDUCE;
ICET_STRATEGY_EXPORT extern IceTStrategy ICET_STRATEGY_VTREE;

ICET_EXPORT void icetStrategy(IceTStrategy strategy);

ICET_EXPORT const GLubyte *icetGetStrategyName(void);

#define ICET_COLOR_BUFFER_BIT	0x0100
#define ICET_DEPTH_BUFFER_BIT	0x0200
ICET_EXPORT void icetInputOutputBuffers(GLenum inputs, GLenum outputs);

ICET_EXPORT void icetCompositeOrder(const GLint *process_ranks);

ICET_EXPORT void icetDataReplicationGroup(GLint size, const GLint *processes);
ICET_EXPORT void icetDataReplicationGroupColor(GLint color);

#define ICET_DIAG_OFF		0x0000
#define ICET_DIAG_ERRORS	0x0001
#define ICET_DIAG_WARNINGS	0x0003
#define ICET_DIAG_DEBUG		0x0007
#define ICET_DIAG_ROOT_NODE	0x0000
#define ICET_DIAG_ALL_NODES	0x0100
#define ICET_DIAG_FULL		0xFFFF

ICET_EXPORT void icetDiagnostics(GLbitfield mask);


#define ICET_STATE_ENGINE_START	0x00000000

#define ICET_DIAGNOSTIC_LEVEL	(ICET_STATE_ENGINE_START | 0x0001)
#define ICET_RANK		(ICET_STATE_ENGINE_START | 0x0002)
#define ICET_NUM_PROCESSES	(ICET_STATE_ENGINE_START | 0x0003)
#define ICET_ABSOLUTE_FAR_DEPTH	(ICET_STATE_ENGINE_START | 0x0004)
#define ICET_BACKGROUND_COLOR	(ICET_STATE_ENGINE_START | 0x0005)
#define ICET_BACKGROUND_COLOR_WORD (ICET_STATE_ENGINE_START | 0x0006)

#define ICET_NUM_TILES		(ICET_STATE_ENGINE_START | 0x0010)
#define ICET_TILE_VIEWPORTS	(ICET_STATE_ENGINE_START | 0x0011)
#define ICET_GLOBAL_VIEWPORT	(ICET_STATE_ENGINE_START | 0x0012)
#define ICET_TILE_MAX_WIDTH	(ICET_STATE_ENGINE_START | 0x0013)
#define ICET_TILE_MAX_HEIGHT	(ICET_STATE_ENGINE_START | 0x0014)
#define ICET_TILE_MAX_PIXELS	(ICET_STATE_ENGINE_START | 0x0015)
#define ICET_DISPLAY_NODES	(ICET_STATE_ENGINE_START | 0x001A)
#define ICET_TILE_DISPLAYED	(ICET_STATE_ENGINE_START | 0x001B)

#define ICET_GEOMETRY_BOUNDS	(ICET_STATE_ENGINE_START | 0x0022)
#define ICET_NUM_BOUNDING_VERTS	(ICET_STATE_ENGINE_START | 0x0023)
#define ICET_STRATEGY_NAME	(ICET_STATE_ENGINE_START | 0x0024)
#define ICET_STRATEGY_COMPOSE	(ICET_STATE_ENGINE_START | 0x0025)
#define ICET_INPUT_BUFFERS	(ICET_STATE_ENGINE_START | 0x0026)
#define ICET_OUTPUT_BUFFERS	(ICET_STATE_ENGINE_START | 0x0027)
#define ICET_COMPOSITE_ORDER	(ICET_STATE_ENGINE_START | 0x0028)
#define ICET_PROCESS_ORDERS	(ICET_STATE_ENGINE_START | 0x0029)
#define ICET_STRATEGY_SUPPORTS_ORDERING (ICET_STATE_ENGINE_START | 0x002A)
#define ICET_DATA_REPLICATION_GROUP (ICET_STATE_ENGINE_START | 0x002B)
#define ICET_DATA_REPLICATION_GROUP_SIZE (ICET_STATE_ENGINE_START | 0x002C)

#define ICET_DRAW_FUNCTION	(ICET_STATE_ENGINE_START | 0x0060)
#define ICET_READ_BUFFER	(ICET_STATE_ENGINE_START | 0x0061)
#define ICET_COLOR_FORMAT	(ICET_STATE_ENGINE_START | 0x0062)
#define ICET_FRAME_COUNT	(ICET_STATE_ENGINE_START | 0x0063)

#define ICET_STATE_FRAME_START	0x00000080

#define ICET_IS_DRAWING_FRAME	(ICET_STATE_FRAME_START | 0x0000)
#define ICET_PROJECTION_MATRIX	(ICET_STATE_FRAME_START | 0x0001)
#define ICET_CONTAINED_VIEWPORT	(ICET_STATE_FRAME_START | 0x0002)
#define ICET_NEAR_DEPTH		(ICET_STATE_FRAME_START | 0x0003)
#define ICET_FAR_DEPTH		(ICET_STATE_FRAME_START | 0x0004)
#define ICET_NUM_CONTAINED_TILES (ICET_STATE_FRAME_START| 0x0005)
#define ICET_CONTAINED_TILES_LIST (ICET_STATE_FRAME_START|0x0006)
#define ICET_CONTAINED_TILES_MASK (ICET_STATE_FRAME_START|0x0007)
#define ICET_ALL_CONTAINED_TILES_MASKS (ICET_STATE_FRAME_START|0x0008)
#define ICET_TILE_CONTRIB_COUNTS (ICET_STATE_FRAME_START| 0x0009)
#define ICET_TOTAL_IMAGE_COUNT	(ICET_STATE_FRAME_START | 0x000A)

#define ICET_RENDERED_VIEWPORT	(ICET_STATE_FRAME_START | 0x0010)

#define ICET_COLOR_BUFFER	(ICET_STATE_FRAME_START | 0x0018)
#define ICET_DEPTH_BUFFER	(ICET_STATE_FRAME_START | 0x0019)
#define ICET_COLOR_BUFFER_VALID	(ICET_STATE_FRAME_START | 0x001A)
#define ICET_DEPTH_BUFFER_VALID	(ICET_STATE_FRAME_START | 0x001B)

#define ICET_STATE_TIMING_START	0x000000C0

#define ICET_RENDER_TIME	(ICET_STATE_TIMING_START | 0x0001)
#define ICET_BUFFER_READ_TIME	(ICET_STATE_TIMING_START | 0x0002)
#define ICET_BUFFER_WRITE_TIME	(ICET_STATE_TIMING_START | 0x0003)
#define ICET_COMPRESS_TIME	(ICET_STATE_TIMING_START | 0x0004)
#define ICET_COMPARE_TIME	(ICET_STATE_TIMING_START | 0x0005)
#define ICET_COMPOSITE_TIME	(ICET_STATE_TIMING_START | 0x0006)
#define ICET_TOTAL_DRAW_TIME	(ICET_STATE_TIMING_START | 0x0007)
#define ICET_BYTES_SENT		(ICET_STATE_TIMING_START | 0x0010)

#define ICET_STATE_ENABLE_START	0x00000100
#define ICET_STATE_ENABLE_END	0x00000120

#define ICET_FLOATING_VIEWPORT	(ICET_STATE_ENABLE_START | 0x0001)
#define ICET_ORDERED_COMPOSITE	(ICET_STATE_ENABLE_START | 0x0002)

#define ICET_DISPLAY		(ICET_STATE_ENABLE_START | 0x0010)
#define ICET_DISPLAY_COLORED_BACKGROUND	(ICET_STATE_ENABLE_START | 0x0011)
#define ICET_DISPLAY_INFLATE	(ICET_STATE_ENABLE_START | 0x0012)

#define ICET_STATE_SIZE		0x00000200

ICET_EXPORT void icetGetDoublev(GLenum pname, GLdouble *params);
ICET_EXPORT void icetGetFloatv(GLenum pname, GLfloat *params);
ICET_EXPORT void icetGetIntegerv(GLenum pname, GLint *params);
ICET_EXPORT void icetGetBooleanv(GLenum pname, GLboolean *params);
ICET_EXPORT void icetGetPointerv(GLenum pname, GLvoid **params);

ICET_EXPORT void icetEnable(GLenum pname);
ICET_EXPORT void icetDisable(GLenum pname);
ICET_EXPORT GLboolean icetIsEnabled(GLenum pname);

#define ICET_NO_ERROR		0x00000000
#define ICET_SANITY_CHECK_FAIL	0xFFFFFFFF
#define ICET_INVALID_ENUM	0xFFFFFFFE
#define ICET_BAD_CAST		0xFFFFFFFD
#define ICET_OUT_OF_MEMORY	0xFFFFFFFC
#define ICET_INVALID_OPERATION  0xFFFFFFFB
#define ICET_INVALID_VALUE	0xFFFFFFFA

ICET_EXPORT GLenum icetGetError(void);

#ifdef __cplusplus
}
#endif

#endif /* _ICET_H_ */
