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

#include <GL/ice-t_buckets.h>
#include <GL/ice-t.h>

#include "diagnostics.h"

#include <stdlib.h>

#define MI(r,c)	((c)*4+(r))

static void multMatrix(GLdouble *C, const GLdouble *A, const GLdouble *B);

IceTBucket icetCreateBucket(void)
{
    IceTBucket bucket;

    bucket = malloc(sizeof(struct icet_bucket_st));
    bucket->bounds = NULL;
    bucket->num_bounds = 0;

    return bucket;
}

void icetDestroyBucket(IceTBucket bucket)
{
    free(bucket->bounds);
    free(bucket);
}

void icetBucketVertices(IceTBucket bucket,
			GLint size, GLenum type, GLsizei stride,
			GLsizei count, const GLvoid *pointer)
{
    GLdouble *verts;
    int i, j;

    if (stride < 1) {
	stride = size;
    }

    verts = malloc(count*3*sizeof(GLdouble));
    for (i = 0; i < count; i++) {
	for (j = 0; j < 3; j++) {
	    switch (type) {
#define castcopy(ptype)							\
		  if (j < size) {					\
		      verts[i*3+j] = ((ptype *)pointer)[i*stride+j];	\
		  } else {						\
		      verts[i*3+j] = 1.0;				\
		  }							\
		  if (size >= 4) {					\
		      verts[i*3+j] /= ((ptype *)pointer)[i*stride+4];	\
		  }							\
		  break;
	      case ICET_SHORT:
		  castcopy(GLshort);
	      case ICET_INT:
		  castcopy(GLint);
	      case ICET_FLOAT:
		  castcopy(GLfloat);
	      case ICET_DOUBLE:
		  castcopy(GLdouble);
	      default:
		  icetRaiseError("Bad type to icetBucketVertices.",
				 ICET_INVALID_VALUE);
		  free(verts);
		  return;
	    }
	}
    }

    free(bucket->bounds);
    bucket->bounds = verts;
    bucket->num_bounds = count;
}

void icetBucketBoxd(IceTBucket bucket,
		    GLdouble x_min, GLdouble x_max,
		    GLdouble y_min, GLdouble y_max,
		    GLdouble z_min, GLdouble z_max)
{
    GLdouble *verts;

    verts = malloc(8*3*sizeof(GLdouble));
    verts[0*3 + 0] = x_min;  verts[0*3 + 1] = y_min;  verts[0*3 + 2] = z_min;
    verts[1*3 + 0] = x_min;  verts[1*3 + 1] = y_min;  verts[1*3 + 2] = z_max;
    verts[2*3 + 0] = x_min;  verts[2*3 + 1] = y_max;  verts[2*3 + 2] = z_min;
    verts[3*3 + 0] = x_min;  verts[3*3 + 1] = y_max;  verts[3*3 + 2] = z_max;
    verts[4*3 + 0] = x_max;  verts[4*3 + 1] = y_min;  verts[4*3 + 2] = z_min;
    verts[5*3 + 0] = x_max;  verts[5*3 + 1] = y_min;  verts[5*3 + 2] = z_max;
    verts[6*3 + 0] = x_max;  verts[6*3 + 1] = y_max;  verts[6*3 + 2] = z_min;
    verts[7*3 + 0] = x_max;  verts[7*3 + 1] = y_max;  verts[7*3 + 2] = z_max;

    free(bucket->bounds);
    bucket->bounds = verts;
    bucket->num_bounds = 8;
}

void icetBucketBoxf(IceTBucket bucket, GLfloat x_min, GLfloat x_max,
		    GLfloat y_min, GLfloat y_max,
		    GLfloat z_min, GLfloat z_max)
{
    icetBucketBoxd(bucket, x_min, x_max, y_min, y_max, z_min, z_max);
}

GLboolean icetBucketInView(IceTBucket bucket, GLdouble *transform)
{
    int left, right, bottom, top, znear, zfar;
    int i;
    left = right = bottom = top = znear = zfar = 0;

    for (i = 0; i < bucket->num_bounds; i++) {
	GLdouble *vert = bucket->bounds + 3*i;
	GLdouble x, y, z, w;

	w = transform[MI(3,0)]*vert[0] + transform[MI(3,1)]*vert[1]
	    + transform[MI(3,2)]*vert[2] + transform[MI(3,3)];
	x = transform[MI(0,0)]*vert[0] + transform[MI(0,1)]*vert[1]
	    + transform[MI(0,2)]*vert[2] + transform[MI(0,3)];
	if (x < w)  left   = 1;
	if (x > -w) right  = 1;
	y = transform[MI(1,0)]*vert[0] + transform[MI(1,1)]*vert[1]
	    + transform[MI(1,2)]*vert[2] + transform[MI(1,3)];
	if (y < w)  bottom = 1;
	if (y > -w) top    = 1;
	z = transform[MI(2,0)]*vert[0] + transform[MI(2,1)]*vert[1]
	    + transform[MI(2,2)]*vert[2] + transform[MI(2,3)];
	if (z < w)  znear  = 1;
	if (z > -w) zfar   = 1;

	if (left && right && bottom && top && znear && zfar) return 1;
    }
    return 0;
}

void icetSetBoundsFromBuckets(IceTBucket *buckets, int num_buckets)
{
    GLdouble x_min, x_max, y_min, y_max, z_min, z_max;
    int b, v;

    if (num_buckets < 1) return;

    x_min = y_min = z_min = 1.0e64;
    x_max = y_max = z_max = -1.0e64;

    for (b = 0; b < num_buckets; b++) {
	for (v = 0; v < buckets[b]->num_bounds; v++) {
	    GLdouble *vert = buckets[b]->bounds + v*3;
	    if (x_min > vert[0]) x_min = vert[0];
	    if (x_max < vert[0]) x_max = vert[0];
	    if (y_min > vert[1]) y_min = vert[1];
	    if (y_max < vert[1]) y_max = vert[1];
	    if (z_min > vert[2]) z_min = vert[2];
	    if (z_max < vert[3]) z_max = vert[2];
	}
    }

    icetBoundingBoxd(x_min, x_max, y_min, y_max, z_min, z_max);
}

void icetBucketsDraw(const IceTBucket *buckets, int num_buckets,
		     void (*draw_func)(int))
{
    GLdouble projection[16];
    GLdouble modelview[16];
    GLdouble transform[16];
    int i;

    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    multMatrix(transform, projection, modelview);

    for (i = 0; i < num_buckets; i++) {
	if (icetBucketInView(buckets[i], transform)) {
	    draw_func(i);
	}
    }
}

static void multMatrix(GLdouble *C, const GLdouble *A, const GLdouble *B)
{
    int i, j, k;

    for (i = 0; i < 4; i++) {
	for (j = 0; j < 4; j++) {
	    C[MI(i,j)] = 0.0;
	    for (k = 0; k < 4; k++) {
		C[MI(i,j)] += A[MI(i,k)] * B[MI(k,j)];
	    }
	}
    }
}
