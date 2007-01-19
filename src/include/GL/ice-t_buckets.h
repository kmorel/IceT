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

#ifndef _ICET_BUCKETS_H_
#define _ICET_BUCKETS_H_

#include "GL/ice-t_config.h"

#ifdef __APPLE__
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

typedef struct icet_bucket_st {
    GLdouble *bounds;
    int num_bounds;
} *IceTBucket;

ICET_EXPORT IceTBucket icetCreateBucket(void);
ICET_EXPORT void       icetDestroyBucket(IceTBucket bucket);

ICET_EXPORT void icetBucketVertices(IceTBucket bucket,
                                    GLint size, GLenum type, GLsizei stride,
                                    GLsizei count, const GLvoid *pointer);
ICET_EXPORT void icetBucketBoxd(IceTBucket bucket,
                                GLdouble x_min, GLdouble x_max,
                                GLdouble y_min, GLdouble y_max,
                                GLdouble z_min, GLdouble z_max);
ICET_EXPORT void icetBucketBoxf(IceTBucket bucket,
                                GLfloat x_min, GLfloat x_max,
                                GLfloat y_min, GLfloat y_max,
                                GLfloat z_min, GLfloat z_max);

ICET_EXPORT GLboolean icetBucketInView(IceTBucket bucket, GLdouble *transform);

ICET_EXPORT void icetSetBoundsFromBuckets(IceTBucket *buckets, int num_buckets);

ICET_EXPORT void icetBucketsDraw(const IceTBucket *buckets, int num_buckets,
                                 void (*draw_func)(int));

#ifdef __cplusplus
}
#endif

#endif /*_ICET_BUCKETS_H_*/
