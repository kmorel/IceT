/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#ifndef __IceTDevMatrix_h
#define __IceTDevMatrix_h

#include <IceT.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

/* Macro to reference an entry in a matrix. */
#define ICET_MATRIX(matrix, row, column) (matrix)[(column)*4+(row)]

/* Multiplies A and B.  Puts the result in C. */
ICET_EXPORT void icetMatrixMultiply(IceTDouble *C,
                                    const IceTDouble *A,
                                    const IceTDouble *B);

/* Multiplies A (a 4x4 matrix) and v (a 4 vector) and returns the result (a 4
 * vector) in out. */
ICET_EXPORT void icetMatrixVectorMultiply(IceTDouble *out,
                                          const IceTDouble *A,
                                          const IceTDouble *v);

/* Returns the dot product of two vector 3's. */
#define icetDot3(v1, v2) ((v1)[0]*(v2)[0] + (v1)[1]*(v2)[1] + (v1)[2]*(v2)[2])

/* Returns the dot product of two vector 4's. */
#define icetDot4(v1, v2) (icetDot3(v1, v2) + (v1)[3]*(v2)[3])

/* Returns an orthographic projection that is equivalent to glOrtho. */
ICET_EXPORT void icetMatrixOrtho(IceTDouble left, IceTDouble right,
                                 IceTDouble bottom, IceTDouble top,
                                 IceTDouble znear, IceTDouble zfar,
                                 IceTDouble *mat_out);

/* Computes the inverse of the given matrix.  Returns ICET_TRUE if successful,
 * ICET_FALSE if the matrix has no inverse. */
ICET_EXPORT IceTBoolean icetMatrixInverse(const IceTDouble *matrix_in,
                                          IceTDouble *matrix_out);

/* Computes the transpose of the given matrix. */
ICET_EXPORT void icetMatrixTranspose(const IceTDouble *matrix_in,
                                     IceTDouble *matrix_out);

/* Computes the inverse transpose of the given matrix. */
ICET_EXPORT IceTBoolean icetMatrixInverseTranspose(const IceTDouble *matrix_in,
                                                   IceTDouble *matrix_out);

#ifdef __cplusplus
}
#endif

#endif /*__IceTDevMatrix_h*/
