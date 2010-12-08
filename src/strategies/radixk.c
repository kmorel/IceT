/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

/* The Radix-k algorithm was designed by Tom Peterka at Argonne National
   Laboratory.

   Copyright (c) University of Chicago
   Permission is hereby granted to use, reproduce, prepare derivative works, and
   to redistribute to others.

   The Radix-k algorithm was ported to IceT by Wesley Kendall from University
   of Tennessee at Knoxville.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <assert.h>
#include <string.h>

#include <IceT.h>

#include <IceTDevCommunication.h>
#include <IceTDevDiagnostics.h>
#include <IceTDevImage.h>

#define SWAP_IMAGE_DATA 21
#define SWAP_DEPTH_DATA 22

#define MAGIC_K 8 /* K value that is attempted to be used each round */
#define MAX_K 256 /* Maximum k value */
#define MAX_R 128 /* Maximum number of rounds */

static int* radixkGetK(int world_size, int* r)
{
    /* Divide the world size into groups that are closest to the magic k
       value. */
    int* k = NULL;
    int num_groups = 0;
    int next_divide = world_size;
    while (next_divide > 1) {
        /* If the magic k value is perfectly divisible by the next_divide
           size, we are good to go */
        int next_k = -1;
        if (next_divide % MAGIC_K == 0) {
            next_k = MAGIC_K;
        } else if (MAGIC_K > next_divide) {
            next_k = next_divide;
        } else {
            /* Iteratively progress upwards to find the next best match for 
               the magic k value. */
            int i;
            for (i = 1; i <= next_divide - MAGIC_K; i++) {
                if ((MAGIC_K + i) % next_divide == 0) {
                    next_k = MAGIC_K + i;
                    break;
                }
            }
            assert(next_k != -1);
        }

        /* Set the k value in the array. */
        k = realloc(k, sizeof(int) * (num_groups + 1));
        k[num_groups] = next_k;
        next_divide /= MAGIC_K;
        num_groups++;
    }

    *r = num_groups;
    return k;
}

#if 0
radixkMirrorPermuation and RadixDist do not seem to be used.

/* computes the generalized mirror permutation of rank in k-space */
static int radixkMirrorPermutation(int r, const int *k, int rank)
{
    int *digits = malloc(r * sizeof(int));
    int suffix_base;
    int quot;
    int new_rank;
    int i;

    /* decompose into digits */
    quot = rank;
    for (i = 0; i < r; ++i) {
        digits[r-i-1] = quot % k[r-i-1]; /* remainder is the current digit */
        quot = quot / k[r-i-1];
    }
    assert(quot == 0);

    /* mirror and calculate the new rank at the same time */
    suffix_base = 1;
    new_rank = 0;
    for (i = 0; i < r; ++i) {
        new_rank += suffix_base * digits[i];
        suffix_base *= k[i];
    }

    free(digits);
    return new_rank;
}

/* RadixDist

   computes radix distribution of results

   inputs:
     size: image size (pixels)
     nprocs: number of processes taking part in radix algorithm
     r: number of rounds
     k: k values

   outputs:
     ofsts: offsets of resulting image portions for each process
     sizes: sizes of resulting image portions for each process

   ofsts and sizes need to be allocated large enough hold nprocs elements
*/
static void RadixDist(int size,
                      int nprocs,
                      int r,
                      const int *k,
                      int *ofsts,
                      int *sizes)
{
    int prod_k;
    int *classes = malloc(nprocs * sizeof(int));
    int rank;
    int i;

    assert(r > 0);
    prod_k = k[0];
    for (i = 1; i < r; ++i) {
        prod_k *= k[i];
    }
    assert(prod_k == nprocs); /* sanity */

    /* First calculate the sizes and offsets as though the algorithm left the
       blocks in process order */
    for (rank = 0; rank < nprocs; ++rank) {
        sizes[rank] = size;
        classes[rank] = rank;
    }

    for (rank = 0; rank < nprocs; ++rank) {
        /* distribute any remainder 1 element at a time from 0..(rem-1) */
        int remain = (rank < (size % nprocs)) ? 1 : 0;
        sizes[rank] = (size / nprocs) + remain;
    }

    /* now permute the calculated values into their actual locations */
    {
        int ofst_total = 0;
        for (rank = 0; rank < nprocs; ++rank) {
            int mirror_rank = radixkMirrorPermutation(r, k, rank);
            ofsts[mirror_rank] = ofst_total;
            ofst_total += sizes[mirror_rank];
        }
    }

    free(classes);

}
#endif

/* radixkGetRndVec

   Computes my round vector coordinates
   my position in each round forms an r-dimensional vector
   [round 0 pos, round 1 pos, ... round r-1 pos]
   where pos is my position in the group of partners within that round

   inputs:
     r: number of rounds
     k: vector of k values
     rank: my MPI rank

   outputs:
     rv: round vector coordinates
*/
static void radixkGetRndVec(int r, int *k, int rank, int *rv)
{

    int step; /* step size in rank for a lattice direction */
    int i;

    step = 1;
    for (i = 0; i < r; i++) {
        rv[i] = rank / step % k[i];
        step *= k[i];
    }

}

/* GetPartners
   gets the ranks of my trading partners

   inputs:
    k: vector of k values
    cur_r: current round number (0 to r - 1)
    rc: current round vector coordinate (0 to cur_r - 1)
    rank: my MPI rank

   output:
    partners: MPI ranks of the partners in my group, including myself
*/
static void GetPartners(int *k, int cur_r, int rc, int rank, int *partners)
{

    int step; /* ranks jump by this much in the current round */
    int i;

    step = 1;
    for (i = 0; i < cur_r; i++)
        step *= k[i];

    partners[0] = rank - rc * step;
    for (i = 1; i < k[cur_r]; i++)
        partners[i] = partners[i - 1] + step;

}

static void radixkGatherFinalImage(IceTInt* compose_group, IceTInt group_rank,
                                   IceTInt group_size, IceTInt image_dest,
                                   int offset, int size, IceTImage image)
{
    int i;
    IceTEnum color_format;
    IceTEnum depth_format;
    IceTCommRequest *requests;
    int *all_sizes;
    int* all_offsets;

    icetRaiseDebug("Collecting image data.");
    /* Adjust image for output as some buffers, such as depth, might be
       dropped. */
    icetImageAdjustForOutput(image);

    color_format = icetImageGetColorFormat(image);
    depth_format = icetImageGetDepthFormat(image);
    requests =  malloc((group_size) * sizeof(IceTCommRequest));

    /* TODO: Compute the sizes instead of communicate them. */ 
    /* Find out the sizes of each process. */
    all_sizes = malloc(sizeof(int) * group_size);
    if (group_rank == image_dest) {
        all_sizes[group_rank] = size;
        for (i = 0; i < group_size; i++) {
            if (i != group_rank) {
                requests[i] = icetCommIrecv(&(all_sizes[i]), 1, ICET_INT,
                                            compose_group[i], SWAP_IMAGE_DATA);
            } else {
                requests[i] = ICET_COMM_REQUEST_NULL;
            }
        }
        for (i = 0; i < group_size; i++) {
            icetCommWait(requests + i);
        }
    } else {
        icetCommSend(&size, 1, ICET_INT, compose_group[image_dest],
                     SWAP_IMAGE_DATA);
    }

    /* Compute all the offsets. */
    all_offsets = malloc(sizeof(int) * group_size);
    all_offsets[0] = 0;
    for (i = 1; i < group_size; i++) {
        all_offsets[i] = all_offsets[i - 1] + all_sizes[i - 1];
    }

    /* Exchange color and depth data. */
    if (color_format != ICET_IMAGE_COLOR_NONE) {
        IceTSizeType pixel_size;
        IceTVoid* color_buf = icetImageGetColorVoid(image, &pixel_size);
        if (group_rank == image_dest) {
            for (i = 0; i < group_size; i++) {
                if (i != group_rank) {
                    requests[i] = icetCommIrecv((IceTByte*)color_buf
                                                + pixel_size * all_offsets[i],
                                                pixel_size * all_sizes[i],
                                                ICET_BYTE,
                                                compose_group[i],
                                                SWAP_IMAGE_DATA);
                } else {
                    requests[i] = ICET_COMM_REQUEST_NULL;
                }
            }
            for (i = 0; i < group_size; i++) {
                icetCommWait(requests + i);
            }
        } else {
            icetCommSend((IceTByte*)color_buf + pixel_size * offset,
                         pixel_size * size,
                         ICET_BYTE,
                         compose_group[image_dest],
                         SWAP_IMAGE_DATA);
        }
    }
    if (depth_format != ICET_IMAGE_DEPTH_NONE) {
        IceTSizeType pixel_size;
        IceTVoid* depth_buf = icetImageGetDepthVoid(image, &pixel_size);
        if (group_rank == image_dest) {
            for (i = 0; i < group_size; i++) {
                if (i != group_rank) {
                    requests[i] = icetCommIrecv((IceTByte*)depth_buf
                                                + pixel_size * all_offsets[i],
                                                pixel_size * all_sizes[i],
                                                ICET_BYTE,
                                                compose_group[i],
                                                SWAP_IMAGE_DATA);
                } else {
                    requests[i] = ICET_COMM_REQUEST_NULL;
                }
            }
            for (i = 0; i < group_size; i++) {
                icetCommWait(requests + i);
            }
        } else {
            icetCommSend((IceTByte*)depth_buf + pixel_size * offset,
                         pixel_size * size,
                         ICET_BYTE,
                         compose_group[image_dest],
                         SWAP_IMAGE_DATA);
        }
    }

    free(requests);
    free(all_offsets);
    free(all_sizes);
    /* This will not work for multi-tile compositing most likely because IceT
       does not create separate communicators for each compositing group. */
    /* TODO use Gatherv since processes might not contain equal portions. */
#if 0
    if (color_format != ICET_IMAGE_COLOR_NONE) {
        IceTSizeType pixel_size;
        IceTVoid* color_buf = icetImageGetColorVoid(image, &pixel_size);
        if (group_rank == image_dest) {
            icetCommGather(MPI_IN_PLACE, size * pixel_size,
                           ICET_BYTE, color_buf + pixel_size * offset,
                           compose_group[image_dest]);
        } else {
            icetCommGather(color_buf + pixel_size * offset, size * pixel_size,
                           ICET_BYTE, color_buf + pixel_size * offset,
                           compose_group[image_dest]);
        }
    }
    if (depth_format != ICET_IMAGE_DEPTH_NONE) {
        IceTSizeType pixel_size;
        IceTVoid* depth_buf = icetImageGetDepthVoid(image, &pixel_size);
        if (group_rank == image_dest) {
            icetCommGather(MPI_IN_PLACE, size * pixel_size,
                           ICET_BYTE, depth_buf + pixel_size * offset,
                           compose_group[image_dest]);
        } else {
            icetCommGather(depth_buf + pixel_size * offset, size * pixel_size,
                           ICET_BYTE, depth_buf + pixel_size * offset,
                           compose_group[image_dest]);
        }
    }
#endif
}

void icetRadixkCompose(IceTInt *compose_group, IceTInt group_size,
                       IceTInt image_dest, IceTImage image) {
    IceTUInt size = icetImageGetNumPixels(image);
    IceTUInt width = icetImageGetWidth(image);
    IceTUInt height = icetImageGetHeight(image);
    int r; /* Number of rounds. */
    int* k = radixkGetK(group_size, &r); /* Communication sizes per round. */

    IceTCommRequest r_reqs[MAX_K]; /* Receive requests */
    IceTCommRequest s_reqs[MAX_K]; /* Send requests */
    IceTVoid *recv_bufs[MAX_K]; /* Receive buffers */
    IceTVoid *send_bufs[MAX_K]; /* Send buffers */
    IceTSparseImage send_img_bufs[MAX_K]; /* Sparse images that use send_bufs */
    int here[MAX_K]; /* Message from this group member has arrived */
    int done[MAX_K]; /* Message from this group member has been composited */
    int dests[MAX_K]; /*Destinations within my group indexed by request number*/
    int dest; /* One send destination within my group */
    int ofsts[MAX_K]; /* Image subregion offset */
    int max_k; /* Max value of k in all the rounds */
    int my_ofst; /* Offset of my subimage */
    int rv[MAX_R]; /* My round vector [round 0 pos, round 1 pos, ...] */
    int arr; /* Index of arrived message */
    int b; /* Distance from destination to start of already composited region */
    int i, j, n;
    int my_size = -1;
    int *sizes = NULL;
    /* This array holds the partners in a compositing group. It is used as an
       index into the actual ordered array of ranks that is passed to this
       function. */
    int partners[MAX_K];

    IceTVoid* intermediate_compose_buf;
    IceTImage intermediate_compose_img;
    int max_sparse_img_size;

    /* Find your rank in your group. */
    IceTInt rank = 0;
    IceTInt world_rank = icetCommRank();
    while ((rank < group_size) && (compose_group[rank] != world_rank)) {
        rank++;
    }
    if (rank >= group_size) {
        icetRaiseError("Local process not in compose_group?",
                       ICET_SANITY_CHECK_FAIL);
        return;
    }

    /* r > 0 is assumed several places throughout this function */
    assert(r > 0);

    n = k[0];
    for (i = 1; i < r; ++i) {
        n *= k[i];
    }
    /* Sanity check that the product of all k's covers all processes */
    assert(n == group_size);
  
    /* A space for uncompressing an intermediate image and then compositing it
       with another compressed image. This is used for group sizes that are 
       greater than two for compositing intermediate results. */
    intermediate_compose_buf = malloc(icetImageBufferSize(width, height));
    intermediate_compose_img
        = icetImageAssignBuffer(intermediate_compose_buf, width, height);

    /* Allocate buffers */
    max_sparse_img_size = icetSparseImageBufferSize(width, height);
    max_k = 0;
    for (i = 0; i < r; i++) {
        max_k = k[i] > max_k ? k[i] : max_k;
    }
    /* TODO: If high k-values only appear later in the algorithm then we don't
       need max_k bufs of size/k[0] length. We can allocate smaller buffers for
       the later rounds. */
    for (i = 0; i < max_k - 1; i++) {
        recv_bufs[i] = malloc(max_sparse_img_size);
        send_bufs[i] = malloc(max_sparse_img_size);
        send_img_bufs[i] = icetSparseImageAssignBuffer(send_bufs[i],
                                                       width, height);
    }

    /* initialize size, my round vector, my offset */
    radixkGetRndVec(r, k, rank, rv);
    my_ofst = 0;

    /* Any peer we communicate with in round i starts that round with a block of
       the same size as ours prior to splitting for sends/recvs.  So we can
       calculate the current round's peer sizes based on our current size and
       the k[i] info. */
    my_size = size;
    sizes = malloc(max_k * sizeof(int));
    for (i = 0; i < max_k; ++i) {
        sizes[i] = -3; /* Defensive */
        ofsts[i] = -5; /* Defensive */
    }

    /* Assumes rv[i] is valid and corresponds to our rank in the round i. */
#define MAP_BUF(k_rank_) ((k_rank_) - ((k_rank_) > rv[i] ? 1 : 0))

    for (i = 0; i < r; i++){
        GetPartners(k, i, rv[i], rank, partners);

        ofsts[0] = my_ofst;
        for (j = 0; j < k[i]; ++j) {
            /* Distribute any remainder 1 element at a time from 0..(rem-1) */
            int remain = (j < (my_size % k[i])) ? 1 : 0;
            sizes[j] = (my_size / k[i]) + remain;

            /* My offset this round is last round's my_ofst plus the sum of the
               sizes of the partners that come before me in the k-ordering */
            if (j > 0) {
                ofsts[j] = ofsts[j - 1] + sizes[j - 1];
            }
        }
        my_size = sizes[rv[i]];
        my_ofst = ofsts[rv[i]];

        /* Post receives starting with nearest neighbors and working outwards */
        n = 0;
        for (j = 1; j < k[i]; j++) {
            dest = rv[i] + j; /* Toward higher group members */
            if (dest < k[i]) {
                r_reqs[n] = icetCommIrecv(recv_bufs[MAP_BUF(dest)],
                                          max_sparse_img_size,
                                          ICET_BYTE,
                                          compose_group[partners[dest]],
                                          0);
                dests[n++] = dest;
            }
            dest = rv[i] - j; /* Toward lower group members */
            if (dest >= 0) {
                r_reqs[n] = icetCommIrecv(recv_bufs[MAP_BUF(dest)],
                                          max_sparse_img_size,
                                          ICET_BYTE,
                                          compose_group[partners[dest]],
                                          0);
                dests[n++] = dest;
            }
        }

        /* Compress subimages and post sends in the same order */
        n = 0;
        for (j = 1; j < k[i]; j++) {
            dest = rv[i] + j; /* Toward higher group members */
            if (dest < k[i]) {
                IceTVoid* package_buffer;
                IceTSizeType package_size;
                icetCompressSubImage(image, ofsts[dest], sizes[dest],
                                     send_img_bufs[MAP_BUF(dest)]);
                icetSparseImagePackageForSend(send_img_bufs[MAP_BUF(dest)], 
                                              &package_buffer, &package_size);
                s_reqs[n++] = icetCommIsend(package_buffer,
                                            package_size,
                                            ICET_BYTE,
                                            compose_group[partners[dest]],
                                            0);
            }
            dest = rv[i] - j; /* Toward lower group members */
            if (dest >= 0) {
                IceTVoid* package_buffer;
                IceTSizeType package_size;
                icetCompressSubImage(image, ofsts[dest], sizes[dest],
                                     send_img_bufs[MAP_BUF(dest)]);
                icetSparseImagePackageForSend(send_img_bufs[MAP_BUF(dest)], 
                                              &package_buffer, &package_size);
                s_reqs[n++] = icetCommIsend(package_buffer,
                                            package_size,
                                            ICET_BYTE,
                                            compose_group[partners[dest]],
                                            0);
            }
        }

        /* clear here and done arrays, mark my own position as here and done */
        for (j = 0; j < k[i]; j++) {
            here[j] = done[j] = 0;
        }
        done[rv[i]] = 1;
        here[rv[i]] = 1;

        /* For all messages plus one to composite leftovers. */
        for(n = 0; n < k[i]; n++) {
            /* Get the next message and mark it as here. */
            if (n < k[i] - 1) {
                arr = icetCommWaitany(k[i] - 1, r_reqs);
                here[dests[arr]] = 1;
            }

            /* scan the group members from me in both directions */
            for (j = 1; j < k[i]; j++) {
                /* --------  up direction ----------- */
                if ((dest = rv[i] + j) < k[i]) {
                    /* distance from dest to start of already composited
                       region */
                    b = 1;
                    while (dest - b >= 0 && done[dest - b] && !here[dest - b]) {
                        b++;
                    }

                    /* composite my image with the next image in order */
                    if (   here[dest] && !done[dest] && here[dest - b] 
                        && done[dest - b]) {
                        IceTSparseImage inSparseImage
                            = icetSparseImageUnpackageFromReceive(
                                                      recv_bufs[MAP_BUF(dest)]);
                        icetCompressedSubComposite(image,
                                                   my_ofst,
                                                   inSparseImage,
                                                   0);
                        done[dest] = 1;
                    }

                    /* distance from dest to start of already composited
                       region */
                    b = 1;
                    while (dest - b >= 0 && done[dest - b] && !here[dest - b]) {
                        b++;
                    }

                    /* composite another adjacent pair */
                    if (dest - b > rv[i] && here[dest] && here[dest - b] &&
                        !done[dest] && !done[dest - b]) {
                        /* Unpackage and uncompress one of the images. This is
                           for making use of IceT's compressed compositing
                           operations. Once a function for compositing two
                           compressed images is available, this can be
                           changed. */
                        IceTSparseImage firstSparseImage;
                        IceTSparseImage secondSparseImage;
                        IceTSizeType package_size;
                        firstSparseImage =
                            icetSparseImageUnpackageFromReceive(
                                                  recv_bufs[MAP_BUF(dest - b)]);
                        icetDecompressImage(firstSparseImage,
                                            intermediate_compose_img);
                        secondSparseImage =
                            icetSparseImageUnpackageFromReceive(
                                                      recv_bufs[MAP_BUF(dest)]);

                        /* Composite. Start at offset 0 since the image we
                           uncompressed is a subimage. */
                        icetCompressedSubComposite(intermediate_compose_img, 0,
                                                   secondSparseImage, 0);

                        /* Compress the composited image back into the original
                           buffer. */
                        icetCompressSubImage(intermediate_compose_img,
                                             0,
                                             my_size,
                                             firstSparseImage);
                        icetSparseImagePackageForSend(
                                                firstSparseImage,
                                                &(recv_bufs[MAP_BUF(dest - b)]), 
                                                &package_size);

                        done[dest] = 1;
                        /* Not available once absorbed by other chunk */
                        here[dest] = 0; 
                    }
                }

                /* --------  down direction ----------- */
                if ((dest = rv[i] - j) >= 0) {
                    /* distance from dest to start of already composited
                       region */
                    b = 1;
                    while (dest + b < k[i] && done[dest + b] && !here[dest + b])
                        b++;

                    /* composite my image with the previous image in order */
                    if (   here[dest] && !done[dest] && here[dest + b] 
                        && done[dest + b]) {
                        IceTSparseImage inSparseImage
                            = icetSparseImageUnpackageFromReceive(
                                                      recv_bufs[MAP_BUF(dest)]);
                        icetCompressedSubComposite(image,
                                                   my_ofst,
                                                   inSparseImage,
                                                   1);
                        done[dest] = 1;
                    }

                    /* distance from dest to start of already composited
                       region */
                    b = 1;
                    while (dest + b < k[i] && done[dest + b] && !here[dest + b])
                    {
                        b++;
                    }

                    /* composite another adjacent pair */
                    if (dest + b < rv[i] && here[dest] && here[dest + b] &&
                        !done[dest] && !done[dest + b]) {
                        /* Unpackage and uncompress one of the images. This is
                           for making use of IceT's compressed compositing
                           operations. Once a function for compositing two
                           compressed images is available, this can be
                           changed. */
                        IceTSparseImage firstSparseImage;
                        IceTSparseImage secondSparseImage;
                        IceTSizeType package_size;
                        firstSparseImage =
                            icetSparseImageUnpackageFromReceive(
                                                  recv_bufs[MAP_BUF(dest + b)]);
                        icetDecompressImage(firstSparseImage,
                                            intermediate_compose_img);
                        secondSparseImage =
                            icetSparseImageUnpackageFromReceive(
                                                      recv_bufs[MAP_BUF(dest)]);
                        /* Start compositing at offset 0 because we just
                           decompressed a subimage. */
                        icetCompressedSubComposite(intermediate_compose_img, 0,
                                                   secondSparseImage, 1);

                        /* Compress the composited image back into the original
                           buffer.  Similarly, the offset is zero is it is
                           intially a subimage. */
                        icetCompressSubImage(intermediate_compose_img,
                                             0,
                                             my_size,
                                             firstSparseImage);
                        icetSparseImagePackageForSend(
                                                firstSparseImage,
                                                &(recv_bufs[MAP_BUF(dest + b)]), 
                                                &package_size);

                        done[dest] = 1;
                        /* not available once absorbed by other chunk */
                        here[dest] = 0; 
                    }
                }
            } /* scan the group members */
        }
        /* wait for the send requests */
        icetCommWaitall(k[i] - 1, s_reqs);
    } /* for all rounds */

#undef MAP_BUF

    free(intermediate_compose_buf);
    free(sizes);
    free(k);
    for (i = 0; i < max_k - 1; i++)
    {
        free(recv_bufs[i]);
        free(send_bufs[i]);
    }

    radixkGatherFinalImage(compose_group, rank, group_size, image_dest,
                           my_ofst, my_size, image);

    return;
}
