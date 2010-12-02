/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include <IceTDevMatrix.h>

#define MAT(matrix, row, column) ICET_MATRIX(matrix, row, column)


void icetMatrixMult(IceTDouble *C, const IceTDouble *A, const IceTDouble *B)
{
    int row, column, k;

    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            MAT(C, row, column) = 0.0;
            for (k = 0; k < 4; k++) {
                MAT(C, row, column) += MAT(A, row, k) * MAT(B, k, column);
            }
        }
    }
}

ICET_EXPORT void icetMatrixOrtho(IceTDouble left, IceTDouble right,
                                 IceTDouble bottom, IceTDouble top,
                                 IceTDouble znear, IceTDouble zfar,
                                 IceTDouble *mat_out)
{
    mat_out[ 0] = 2.0/(right-left);
    mat_out[ 1] = 0.0;
    mat_out[ 2] = 0.0;
    mat_out[ 3] = 0.0;

    mat_out[ 4] = 0.0;
    mat_out[ 5] = 2.0/(top-bottom);
    mat_out[ 6] = 0.0;
    mat_out[ 7] = 0.0;

    mat_out[ 8] = 0.0;
    mat_out[ 9] = 0.0;
    mat_out[10] = -2.0/(zfar - znear);
    mat_out[11] = 0.0;

    mat_out[12] = -(right+left)/(right-left);
    mat_out[13] = -(top+bottom)/(top-bottom);
    mat_out[14] = -(zfar+znear)/(zfar-znear);
    mat_out[15] = 1.0;
}
