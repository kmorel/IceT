/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include <IceTDevMatrix.h>

#include <math.h>

#define MAT(matrix, row, column) ICET_MATRIX(matrix, row, column)

void icetMatrixMultiply(IceTDouble *C, const IceTDouble *A, const IceTDouble *B)
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

/*
 * The inverse matrix is calculated by performing the Gaussian matrix reduction
 * with a partial pivoting with back substitution embedded.
 *
 * Much of this algorithm was inspired by the matrix inverse function in the
 * Mesa 3D library originally written by Jacques Leroy (jle@stare.be).  However,
 * for maintainability, many of the loops are re-rolled and many of the
 * identifiers are treated differently.  Also, the back substitution takes
 * place while the elimination takes place to shorten the code.
 */
IceTBoolean icetMatrixInverse(const IceTDouble *matrix_in,
                              IceTDouble *matrix_out)
{
    /* A 4x8 matrix we will do operations on.  The matrix starts as matrix_in
     * adjoined with the identity matrix.  We then do Gaussian elimination and
     * back substitution on the matrix that will result with the identity matrix
     * adjoined with the inverse of the original matrix. */
    IceTDouble matrix[4][8];
    int eliminationIdx;
    int rowIdx;

    /* Initialize the matrix. */
    for (rowIdx = 0; rowIdx < 4; rowIdx++) {
        int columnIdx;
        /* Left half is equal to matrix_in. */
        for (columnIdx = 0; columnIdx < 4; columnIdx++) {
            matrix[rowIdx][columnIdx] = MAT(matrix_in, rowIdx, columnIdx);
        }
        /* Right half is the identity matrix. */
        for (columnIdx = 4; columnIdx < 8; columnIdx++) {
            matrix[rowIdx][columnIdx] = 0.0;
        }
        matrix[rowIdx][rowIdx+4] = 1.0;
    }

    /* Per the Gaussian elimination algorithm, starting at the top row make
     * the leftmost value 1 and zero out the column on all lower rows.  Repeat
     * to rows on down.  If a column will all remaining zeros is encountered,
     * no inverse exists so return failure. */
    for (eliminationIdx = 0; eliminationIdx < 4; eliminationIdx++)
    {
        int pivotIdx;
        int columnIdx;
        IceTDouble scale_value;

        /* Choose the pivot row as the one with the highest absolute value in
         * the elimination column. */
        pivotIdx = eliminationIdx;
        for (rowIdx = eliminationIdx+1; rowIdx < 4; rowIdx++) {
            if (  fabs(matrix[pivotIdx][eliminationIdx])
                < fabs(matrix[rowIdx][eliminationIdx]) ) {
                pivotIdx = rowIdx;
            }
        }

        /* If the entry in pivotIdx is 0, then all the values in the column must
         * be zero and no inverse exists. */
        if (matrix[pivotIdx][eliminationIdx] == 0.0) return ICET_FALSE;

        /* Swap the elimination row with the pivot row to make the pivot row the
         * elimination row. */
        if (pivotIdx != eliminationIdx) {
            for (columnIdx = 0; columnIdx < 8; columnIdx++) {
                IceTDouble old_elim_value = matrix[eliminationIdx][columnIdx];
                matrix[eliminationIdx][columnIdx] = matrix[pivotIdx][columnIdx];
                matrix[pivotIdx][columnIdx] = old_elim_value;
            }
        }

        /* Size the elimination row to scale the eliminationIdx column to 1. */
        scale_value = 1.0/matrix[eliminationIdx][eliminationIdx];
        for (columnIdx = eliminationIdx; columnIdx < 8; columnIdx++) {
            matrix[eliminationIdx][columnIdx] *= scale_value;
        }

        /* Add a multiple of the elimination row to all other rows to zero out
         * that column of values.  Note that this is also doing back-propagation
         * as it is zeroing out values above. */
        for (rowIdx = 0; rowIdx < 4; rowIdx++) {
            if (rowIdx == eliminationIdx) continue;
            scale_value = -matrix[rowIdx][eliminationIdx];
            for (columnIdx = eliminationIdx; columnIdx < 8; columnIdx++) {
                matrix[rowIdx][columnIdx]
                    += scale_value*matrix[eliminationIdx][columnIdx];
            }
        }
    }

    /* At this point, the left 4x4 submatrix of matrix should be the identity
     * matrix and the right 4x4 submatrix is the inverse of matrix_in.  Copy the
     * result to matrix_out. */
    for (rowIdx = 0; rowIdx < 4; rowIdx++) {
        int columnIdx;
        for (columnIdx = 0; columnIdx < 4; columnIdx++) {
            MAT(matrix_out, rowIdx, columnIdx) = matrix[rowIdx][columnIdx+4];
        }
    }

    return ICET_TRUE;
}

void icetMatrixTranspose(const IceTDouble *matrix_in, IceTDouble *matrix_out)
{
    int rowIdx;
    for (rowIdx = 0; rowIdx < 4; rowIdx++) {
        int columnIdx;
        for (columnIdx = 0; columnIdx < 4; columnIdx++) {
            MAT(matrix_out, rowIdx, columnIdx)
                = MAT(matrix_in, columnIdx, rowIdx);
        }
    }
}

IceTBoolean icetMatrixInverseTranspose(const IceTDouble *matrix_in,
                                       IceTDouble *matrix_out)
{
    IceTDouble inverse[16];
    IceTBoolean success;

    success = icetMatrixInverse(matrix_in, inverse);
    if (!success) return ICET_FALSE;

    icetMatrixTranspose((const IceTDouble *)inverse, matrix_out);
    return ICET_TRUE;
}
