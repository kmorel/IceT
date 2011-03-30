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
#include <string.h>

#include <IceT.h>

#include <IceTDevCommunication.h>
#include <IceTDevDiagnostics.h>
#include <IceTDevImage.h>

#define RADIXK_SWAP_IMAGE_TAG_START     2200
#define RADIXK_TELESCOPE_IMAGE_TAG      2300

#define RADIXK_RECEIVE_BUFFER                   ICET_SI_STRATEGY_BUFFER_0
#define RADIXK_SEND_BUFFER                      ICET_SI_STRATEGY_BUFFER_1
#define RADIXK_SPARE_BUFFER                     ICET_SI_STRATEGY_BUFFER_2
#define RADIXK_INTERLACED_IMAGE_BUFFER          ICET_SI_STRATEGY_BUFFER_3
#define RADIXK_PARTITION_INDICES_BUFFER         ICET_SI_STRATEGY_BUFFER_4
#define RADIXK_PARTITION_INFO_BUFFER            ICET_SI_STRATEGY_BUFFER_5
#define RADIXK_RECEIVE_REQUEST_BUFFER           ICET_SI_STRATEGY_BUFFER_6
#define RADIXK_SEND_REQUEST_BUFFER              ICET_SI_STRATEGY_BUFFER_7
#define RADIXK_FACTORS_ARRAY_BUFFER             ICET_SI_STRATEGY_BUFFER_8
#define RADIXK_SPLIT_OFFSET_ARRAY_BUFFER        ICET_SI_STRATEGY_BUFFER_9
#define RADIXK_SPLIT_IMAGE_ARRAY_BUFFER         ICET_SI_STRATEGY_BUFFER_10
#define RADIXK_RANK_LIST_BUFFER                 ICET_SI_STRATEGY_BUFFER_11

typedef struct radixkPartnerInfoStruct {
    IceTInt rank; /* Rank of partner. */
    IceTSizeType offset; /* Offset of partner's partition in image. */
    IceTVoid *receiveBuffer; /* A buffer for receiving data from partner. */
    IceTSparseImage sendImage; /* A buffer to hold data being sent to partner */
    IceTSparseImage receiveImage; /* Hold for received non-composited image. */
    IceTInt compositeLevel; /* Level in compositing tree for round. */
} radixkPartnerInfo;

/* BEGIN_PIVOT_FOR(loop_var, low, pivot, high)...END_PIVOT_FOR() provides a
   special looping mechanism that iterates over the numbers pivot, pivot-1,
   pivot+1, pivot-2, pivot-3,... until all numbers between low (inclusive) and
   high (exclusive) are visited.  Any numbers outside [low,high) are skipped. */
#define BEGIN_PIVOT_FOR(loop_var, low, pivot, high) \
    { \
        IceTInt loop_var##_true_iter; \
        IceTInt loop_var##_max = 2*(  ((pivot) < ((high)+(low))/2) \
                                    ? ((high)-(pivot)) : ((pivot)-(low)+1) ); \
        for (loop_var##_true_iter = 1; \
             loop_var##_true_iter < loop_var##_max; \
             loop_var##_true_iter ++) { \
            if ((loop_var##_true_iter % 2) == 0) { \
                loop_var = (pivot) - loop_var##_true_iter/2; \
                if (loop_var < (low)) continue; \
            } else { \
                loop_var = (pivot) + loop_var##_true_iter/2; \
                if ((high) <= loop_var) continue; \
            }

#define END_PIVOT_FOR() \
        } \
    }

/* Finds the largest power of 2 equal to or smaller than x. */
static IceTInt radixkFindPower2(IceTInt x)
{
    IceTInt pow2;
    for (pow2 = 1; pow2 <= x; pow2 = pow2 << 1);
    pow2 = pow2 >> 1;
    return pow2;
}

static void radixkSwapImages(IceTSparseImage *image1, IceTSparseImage *image2)
{
    IceTSparseImage old_image1 = *image1;
    *image1 = *image2;
    *image2 = old_image1;
}

static IceTInt* radixkGetK(IceTInt world_size, IceTInt* num_rounds_p)
{
    /* Divide the world size into groups that are closest to the magic k
       value. */
    IceTInt magic_k;
    IceTInt* k_array;
    IceTInt max_num_k;
    IceTInt num_groups = 0;
    IceTInt next_divide = world_size;

    /* Special case of when world_size == 1. */
    if (world_size < 2) {
        k_array = icetGetStateBuffer(RADIXK_FACTORS_ARRAY_BUFFER,
                                     sizeof(IceTInt) * 1);
        k_array[0] = 1;
        *num_rounds_p = 1;
        return k_array;
    }

    icetGetIntegerv(ICET_MAGIC_K, &magic_k);

    /* The maximum number of factors possible is the floor of log base 2. */
    max_num_k = (IceTInt)(floor(log10(world_size)/log10(2)));
    k_array = icetGetStateBuffer(RADIXK_FACTORS_ARRAY_BUFFER,
                                 sizeof(IceTInt) * max_num_k);

    while (next_divide > 1) {
        IceTInt next_k = -1;

        /* If the magic k value is perfectly divisible by the next_divide
           size, we are good to go */
        if ((next_divide % magic_k) == 0) {
            next_k = magic_k;
        }

        /* If that does not work, look for a factor near the magic_k. */
        if (next_k == -1) {
            IceTInt try_k;
            BEGIN_PIVOT_FOR(try_k, 2, magic_k, 2*magic_k) {
                if ((next_divide % try_k) == 0) {
                    next_k = try_k;
                    break;
                }
            } END_PIVOT_FOR()
        }

        /* If you STILL don't have a good factor, progress upwards to find the
           best match. */
        if (next_k == -1) {
            IceTInt try_k;
            IceTInt max_k;

            /* The largest possible smallest factor (other than next_divide
               itself) is the square root of next_divide.  We don't have to
               check the values between the square root and next_divide. */
            max_k = (IceTInt)floor(sqrt(next_divide));

            /* It would be better to just visit prime numbers, but other than
               having a huge table, how would you do that?  Hopefully this is an
               uncommon use case. */
            for (try_k = 2*magic_k; try_k < max_k; try_k++) {
                if ((next_divide % try_k) == 0) {
                    next_k = try_k;
                    break;
                }
            }
        }

        /* If we STILL don't have a factor, then next_division must be a large
           prime.  Basically give up by using next_divide as the next k. */
        if (next_k == -1) {
            next_k = next_divide;
        }

        /* Set the k value in the array. */
        k_array[num_groups] = next_k;
        next_divide /= next_k;
        num_groups++;

        if (num_groups > max_num_k) {
            icetRaiseError("Somehow we got more factors than possible.",
                           ICET_SANITY_CHECK_FAIL);
        }
    }

    /* Sanity check to make sure that the k's actually multiply to the number
     * of processes. */
    {
        IceTInt product = k_array[0];
        IceTInt i;
        for (i = 1; i < num_groups; ++i) {
            product *= k_array[i];
        }
        if (product != world_size) {
            icetRaiseError("Product of k's not equal to number of processes.",
                           ICET_SANITY_CHECK_FAIL);
        }
    }

    *num_rounds_p = num_groups;
    return k_array;
}

/* radixkGetPartitionIndices

   my position in each round forms an num_rounds-dimensional vector
   [round 0 pos, round 1 pos, ... round num_rounds-1 pos]
   where pos is my position in the group of partners within that round

   inputs:
     num_rounds: number of rounds
     k_array: vector of k values
     group_rank: my rank in composite order (compose_group in icetRadixkCompose)

   outputs:
     partition_indices: index of my partition for each round.
*/
static void radixkGetPartitionIndices(IceTInt num_rounds,
                                      const IceTInt *k_array,
                                      IceTInt group_rank,
                                      IceTInt *partition_indices)
{

    IceTInt step; /* step size in rank for a lattice direction */
    IceTInt current_round;

    step = 1;
    for (current_round = 0; current_round < num_rounds; current_round++) {
        partition_indices[current_round]
            = (group_rank / step) % k_array[current_round];
        step *= k_array[current_round];
    }

}

/* radixkGetFinalPartitionIndex

   After radix-k completes on a group of size p, the image is partitioned
   into p pieces.  This function finds the index for the final partition
   (with respect to all partitions, not just one within a round) for a
   given rank.

   inputs:
     num_rounds: number of rounds
     k_array: vector of k values
     group_rank: my rank in composite order (compose_group in icetRadixkCompose)

   returns:
     index of final global partition.
*/
static IceTInt radixkGetFinalPartitionIndex(IceTInt num_rounds,
                                            const IceTInt *k_array,
                                            IceTInt group_rank)
{
    IceTInt step; /* step size in rank for a lattice direction */
    IceTInt current_round;
    IceTInt partition_index;

    step = 1;
    partition_index = 0;
    for (current_round = 0; current_round < num_rounds; current_round++) {
        partition_index *= k_array[current_round];
        partition_index += (group_rank / step) % k_array[current_round];
        step *= k_array[current_round];
    }

    return partition_index;
}

/* radixkGetGroupRankForFinalPartitionIndex

   After radix-k completes on a group of size p, the image is partitioned into p
   pieces.  This function finds the group rank for the given index of the final
   partition.  This function is the inverse if radixkGetFinalPartitionIndex.

   inputs:
     num_rounds: number of rounds
     k_array: vector of k values
     partition_index: index of final global partition

   returns:
     group rank holding partition_index
*/
static IceTInt radixkGetGroupRankForFinalPartitionIndex(IceTInt num_rounds,
                                                        const IceTInt *k_array,
                                                        IceTInt partition_index)
{
    IceTInt step; /* step size in rank for a lattice direction */
    IceTInt current_round;
    IceTInt partition_up_to_round;
    IceTInt group_rank;

    /* Need to work backwords in rounds.  Find the final step. */
    step = 1;
    for (current_round = 0; current_round < num_rounds; current_round++) {
        step *= k_array[current_round];
    }

    partition_up_to_round = partition_index;
    group_rank = 0;
    for (current_round = num_rounds-1; current_round >= 0; current_round--) {
        step /= k_array[current_round];
        group_rank += step * (partition_up_to_round % k_array[current_round]);
        partition_up_to_round /= k_array[current_round];
    }

    return group_rank;
}

/* radixkGetPartners
   gets the ranks of my trading partners

   inputs:
    k_array: vector of k values
    current_round: current round number (0 to num_rounds - 1)
    partition_index: image partition to collect (0 to k[current_round] - 1)
    remaining_partitions: Number of pieces the image will be split into by
        the end of the algorithm.
    compose_group: array of world ranks representing the group of processes
        participating in compositing (passed into icetRadixkCompose)
    group_rank: Index in compose_group that represents me
    start_offset: Start of partition that is being divided in current_round
    start_size: Size of partition that is being divided in current_round

   output:
    partners: Array of radixkPartnerInfo describing all the processes
        participating in this round.
*/
static radixkPartnerInfo *radixkGetPartners(const IceTInt *k_array,
                                            IceTInt current_round,
                                            IceTInt partition_index,
                                            IceTInt remaining_partitions,
                                            const IceTInt *compose_group,
                                            IceTInt group_rank,
                                            IceTSizeType start_size)
{
    IceTInt current_k = k_array[current_round];
    radixkPartnerInfo *partners
        = icetGetStateBuffer(RADIXK_PARTITION_INFO_BUFFER,
                             sizeof(radixkPartnerInfo) * current_k);
    IceTInt step; /* ranks jump by this much in the current round */
    IceTVoid *recv_buf_pool;
    IceTVoid *send_buf_pool;
    IceTSizeType partition_num_pixels;
    IceTSizeType sparse_image_size;
    IceTInt first_partner_group_rank;
    IceTInt i;

    step = 1;
    for (i = 0; i < current_round; i++) {
        step *= k_array[i];
    }

    /* Allocate arrays that can be used as send/receive buffers. */
    partition_num_pixels
      = icetSparseImageSplitPartitionNumPixels(start_size,
                                               current_k,
                                               remaining_partitions);
    sparse_image_size = icetSparseImageBufferSize(partition_num_pixels, 1);
    recv_buf_pool = icetGetStateBuffer(RADIXK_RECEIVE_BUFFER,
                                       sparse_image_size * current_k);
    send_buf_pool = icetGetStateBuffer(RADIXK_SEND_BUFFER,
                                       sparse_image_size * current_k);

#ifdef DEBUG
    /* Defensive */
    memset(partners, 0xDC, sizeof(radixkPartnerInfo) * current_k);
    memset(recv_buf_pool, 0xDC, sparse_image_size * current_k);
    memset(send_buf_pool, 0xDC, sparse_image_size * current_k);
#endif

    first_partner_group_rank = group_rank - partition_index * step;
    for (i = 0; i < current_k; i++) {
        radixkPartnerInfo *p = &partners[i];
        IceTInt partner_group_rank = first_partner_group_rank + i*step;
        IceTVoid *send_buffer;

        p->rank = compose_group[partner_group_rank];

        /* To be filled later. */
        p->offset = -1;

        p->receiveBuffer = ((IceTByte*)recv_buf_pool + i*sparse_image_size);

        send_buffer = ((IceTByte*)send_buf_pool + i*sparse_image_size);
        p->sendImage = icetSparseImageAssignBuffer(send_buffer,
                                                   partition_num_pixels, 1);

        p->receiveImage = icetSparseImageNull();

        p->compositeLevel = -1;
    }

    return partners;
}

/* As applicable, posts an asynchronous receive for each process from which
   we are receiving an image piece. */
static IceTCommRequest *radixkPostReceives(radixkPartnerInfo *partners,
                                           IceTInt current_k,
                                           IceTInt current_round,
                                           IceTInt current_partition_index,
                                           IceTInt remaining_partitions,
                                           IceTSizeType start_size)
{
    IceTCommRequest *receive_requests;
    radixkPartnerInfo *me;
    IceTSizeType partition_num_pixels;
    IceTSizeType sparse_image_size;
    IceTInt tag;
    IceTInt i;

    me = &partners[current_partition_index];

    receive_requests = icetGetStateBuffer(RADIXK_RECEIVE_REQUEST_BUFFER,
                                          current_k * sizeof(IceTCommRequest));

    partition_num_pixels
      = icetSparseImageSplitPartitionNumPixels(start_size,
                                               current_k,
                                               remaining_partitions);
    sparse_image_size = icetSparseImageBufferSize(partition_num_pixels, 1);

    tag = RADIXK_SWAP_IMAGE_TAG_START + current_round;

    for (i = 0; i < current_k; i++) {
        radixkPartnerInfo *p = &partners[i];
        if (i != current_partition_index) {
            receive_requests[i] = icetCommIrecv(p->receiveBuffer,
                                                sparse_image_size,
                                                ICET_BYTE,
                                                p->rank,
                                                tag);
            p->compositeLevel = -1;
        } else {
            /* No need to send to myself. */
            receive_requests[i] = ICET_COMM_REQUEST_NULL;
            p->receiveImage = p->sendImage;
            p->compositeLevel = 0;
        }
    }

    return receive_requests;
}

/* As applicable, posts an asynchronous send for each process to which we are
   sending an image piece. */
static IceTCommRequest *radixkPostSends(radixkPartnerInfo *partners,
                                        IceTInt current_k,
                                        IceTInt current_round,
                                        IceTInt current_partition_index,
                                        IceTInt remaining_partitions,
                                        IceTSizeType start_offset,
                                        const IceTSparseImage image)
{
    radixkPartnerInfo *me;
    IceTCommRequest *send_requests;
    IceTInt *piece_offsets;
    IceTSparseImage *image_pieces;
    IceTInt tag;
    IceTInt i;

    me = &partners[current_partition_index];

    send_requests = icetGetStateBuffer(RADIXK_SEND_REQUEST_BUFFER,
                                       current_k * sizeof(IceTCommRequest));

    piece_offsets = icetGetStateBuffer(RADIXK_SPLIT_OFFSET_ARRAY_BUFFER,
                                       current_k * sizeof(IceTInt));
    image_pieces = icetGetStateBuffer(RADIXK_SPLIT_IMAGE_ARRAY_BUFFER,
                                      current_k * sizeof(IceTSparseImage));
    for (i = 0; i < current_k; i++) {
        image_pieces[i] = partners[i].sendImage;
    }
    icetSparseImageSplit(image,
                         start_offset,
                         current_k,
                         remaining_partitions,
                         image_pieces,
                         piece_offsets);


    tag = RADIXK_SWAP_IMAGE_TAG_START + current_round;

    /* The pivot for loop arranges the sends to happen in an order such that
       those to be composited first in their destinations will be sent first.
       This serves little purpose other than to try to stagger the order of
       sending images so that no everyone sends to the same process first. */
    BEGIN_PIVOT_FOR(i, 0, current_partition_index, current_k) {
        radixkPartnerInfo *p = &partners[i];
        p->offset = piece_offsets[i];
        if (i != current_partition_index) {
            IceTVoid *package_buffer;
            IceTSizeType package_size;

            icetSparseImagePackageForSend(image_pieces[i],
                                          &package_buffer, &package_size);

            send_requests[i] = icetCommIsend(package_buffer,
                                             package_size,
                                             ICET_BYTE,
                                             p->rank,
                                             tag);
        } else {
            /* No need to send to myself. */
            send_requests[i] = ICET_COMM_REQUEST_NULL;
        }
    } END_PIVOT_FOR();

    return send_requests;
}

/* When compositing incoming images, we pair up the images and composite in
   a tree.  This minimizes the amount of times non-overlapping pixels need
   to be copied.  Returns true when all images are composited */
static IceTBoolean radixkTryCompositeIncoming(radixkPartnerInfo *partners,
                                              IceTInt current_k,
                                              IceTInt incoming_index,
                                              IceTSparseImage *spare_image_p,
                                              IceTSparseImage final_image)
{
    IceTSparseImage spare_image = *spare_image_p;
    IceTInt to_composite_index = incoming_index;

    while (ICET_TRUE) {
        IceTInt level = partners[to_composite_index].compositeLevel;
        IceTInt dist_to_sibling = (1 << level);
        IceTInt subtree_size = (dist_to_sibling << 1);
        IceTInt front_index;
        IceTInt back_index;

        if (to_composite_index%subtree_size == 0) {
            front_index = to_composite_index;
            back_index = to_composite_index + dist_to_sibling;

            if (back_index >= current_k) {
                /* This image has no partner at this level.  Just promote
                   the level and continue. */
                if (front_index == 0) {
                    /* Special case.  When index 0 has no partner, we must
                       be at the top of the tree and we are done. */
                    break;
                }
                partners[to_composite_index].compositeLevel++;
                continue;
            }
        } else {
            back_index = to_composite_index;
            front_index = to_composite_index - dist_to_sibling;
        }

        if (   partners[front_index].compositeLevel
            != partners[back_index].compositeLevel ) {
            /* Paired images are not on the same level.  Cannot composite
               until more images come in.  We are done for now. */
            break;
        }

        if ((front_index == 0) && (subtree_size >= current_k)) {
            /* This will be the last image composited.  Composite to final
               location. */
            spare_image = final_image;
        }
        icetCompressedCompressedComposite(partners[front_index].receiveImage,
                                          partners[back_index].receiveImage,
                                          spare_image);
        radixkSwapImages(&partners[front_index].receiveImage, &spare_image);
        partners[front_index].compositeLevel++;
        to_composite_index = front_index;
    }

    *spare_image_p = spare_image;
    return ((1 << partners[0].compositeLevel) >= current_k);
}

static void radixkCompositeIncomingImages(radixkPartnerInfo *partners,
                                          IceTCommRequest *receive_requests,
                                          IceTInt current_k,
                                          IceTInt current_partition_index,
                                          IceTSparseImage image)
{
    const radixkPartnerInfo *me = &partners[current_partition_index];

    IceTSparseImage spare_image;
    IceTInt total_composites;

    IceTSizeType width;
    IceTSizeType height;

    IceTBoolean composites_done;

    /* Regardless of order, there are k-1 composite operations to perform. */
    total_composites = current_k-1;

    /* We will be reusing buffers like crazy, but we'll need at least one more
       for the first composite, assuming we have at least two composites. */
    width = icetSparseImageGetWidth(me->sendImage);
    height = icetSparseImageGetHeight(me->sendImage);
    if (total_composites >= 2) {
        spare_image = icetGetStateBufferSparseImage(RADIXK_SPARE_BUFFER,
                                                    width,
                                                    height);
    } else {
        spare_image = icetSparseImageNull();
    }

    /* Start by trying to composite the implicit receive from myself.  It won't
       actually composite anything, but it may change the composite level.  It
       will also defensively set composites_done correctly. */
    composites_done = radixkTryCompositeIncoming(partners,
                                                 current_k,
                                                 current_partition_index,
                                                 &spare_image,
                                                 image);

    while (!composites_done) {
        IceTInt receive_idx;
        radixkPartnerInfo *receiver;

        /* Wait for an image to come in. */
        receive_idx = icetCommWaitany(current_k, receive_requests);
        receiver = &partners[receive_idx];
        receiver->compositeLevel = 0;
        receiver->receiveImage
            = icetSparseImageUnpackageFromReceive(receiver->receiveBuffer);
        if (   (icetSparseImageGetWidth(receiver->receiveImage) != width)
            || (icetSparseImageGetHeight(receiver->receiveImage) != height) ) {
            icetRaiseError("Radix-k received image with wrong size.",
                           ICET_SANITY_CHECK_FAIL);
        }

        /* Try to composite that image. */
        composites_done = radixkTryCompositeIncoming(partners,
                                                     current_k,
                                                     receive_idx,
                                                     &spare_image,
                                                     image);
    }
}

static void icetRadixkBasicCompose(const IceTInt *compose_group,
                                   IceTInt group_size,
                                   IceTInt largest_group_size,
                                   IceTSparseImage working_image,
                                   IceTSizeType *piece_offset)
{
    IceTInt num_rounds;
    IceTInt* k_array;

    IceTSizeType my_offset;
    IceTInt *partition_indices; /* My round vector [round0 pos, round1 pos, ...] */
    IceTInt current_round;
    IceTInt remaining_partitions;

    /* Find your rank in your group. */
    IceTInt group_rank = icetFindMyRankInGroup(compose_group, group_size);
    if (group_rank < 0) {
        icetRaiseError("Local process not in compose_group?",
                       ICET_SANITY_CHECK_FAIL);
        *piece_offset = 0;
        return;
    }

    if (group_size == 1) {
        /* I am the only process in the group.  No compositing to be done.
         * Just return and the image will be complete. */
        *piece_offset = 0;
        return;
    }

    k_array = radixkGetK(group_size, &num_rounds);

    /* num_rounds > 0 is assumed several places throughout this function */
    if (num_rounds <= 0) {
        icetRaiseError("Radix-k has no rounds?", ICET_SANITY_CHECK_FAIL);
    }

    /* initialize size, my round vector, my offset */
    partition_indices = icetGetStateBuffer(RADIXK_PARTITION_INDICES_BUFFER,
                                           sizeof(IceTInt) * num_rounds);
    radixkGetPartitionIndices(num_rounds,
                              k_array,
                              group_rank,
                              partition_indices);

    /* Any peer we communicate with in round i starts that round with a block of
       the same size as ours prior to splitting for sends/recvs.  So we can
       calculate the current round's peer sizes based on our current size and
       the k_array[i] info. */
    my_offset = 0;
    remaining_partitions = largest_group_size;

    for (current_round = 0; current_round < num_rounds; current_round++) {
        IceTSizeType my_size = icetSparseImageGetNumPixels(working_image);
        IceTInt current_k = k_array[current_round];
        IceTInt current_partition_index = partition_indices[current_round];
        radixkPartnerInfo *partners = radixkGetPartners(k_array,
                                                        current_round,
                                                        current_partition_index,
                                                        remaining_partitions,
                                                        compose_group,
                                                        group_rank,
                                                        my_size);
        IceTCommRequest *receive_requests;
        IceTCommRequest *send_requests;

        receive_requests = radixkPostReceives(partners,
                                              current_k,
                                              current_round,
                                              current_partition_index,
                                              remaining_partitions,
                                              my_size);

        send_requests = radixkPostSends(partners,
                                        current_k,
                                        current_round,
                                        current_partition_index,
                                        remaining_partitions,
                                        my_offset,
                                        working_image);

        radixkCompositeIncomingImages(partners,
                                      receive_requests,
                                      current_k,
                                      current_partition_index,
                                      working_image);

        icetCommWaitall(current_k, send_requests);

        my_offset = partners[current_partition_index].offset;
        remaining_partitions /= current_k;
    } /* for all rounds */

    *piece_offset = my_offset;

    return;
}

static IceTInt icetRadixkTelescopeFindUpperGroupSender(
                                                     const IceTInt *my_group,
                                                     IceTInt my_group_size,
                                                     const IceTInt *upper_group,
                                                     IceTInt upper_group_size)
{
    IceTInt direct_upper_group_size = radixkFindPower2(upper_group_size);
    IceTInt group_difference_factor = my_group_size/direct_upper_group_size;
    IceTInt *k_array;
    IceTInt num_rounds;
    IceTInt my_group_rank;
    IceTInt my_partition_index;
    IceTInt upper_partition_index;
    IceTInt sender_group_rank;

    k_array = radixkGetK(my_group_size, &num_rounds);
    my_group_rank = icetFindMyRankInGroup(my_group, my_group_size);
    my_partition_index = radixkGetFinalPartitionIndex(num_rounds,
                                                      k_array,
                                                      my_group_rank);
    upper_partition_index = my_partition_index / group_difference_factor;

    k_array = radixkGetK(direct_upper_group_size, &num_rounds);
    sender_group_rank
        = radixkGetGroupRankForFinalPartitionIndex(num_rounds,
                                                   k_array,
                                                   upper_partition_index);
    return upper_group[sender_group_rank];
}

static void icetRadixkTelescopeFindLowerGroupReceivers(
                                                     const IceTInt *lower_group,
                                                     IceTInt lower_group_size,
                                                     const IceTInt *my_group,
                                                     IceTInt my_group_size,
                                                     IceTInt **receiver_ranks_p,
                                                     IceTInt *num_receivers_p)
{
    IceTInt num_receivers = lower_group_size/my_group_size;
    IceTInt *receiver_ranks = icetGetStateBuffer(RADIXK_RANK_LIST_BUFFER,
                                                 num_receivers*sizeof(IceTInt));
    IceTInt my_group_rank = icetFindMyRankInGroup(my_group, my_group_size);
    IceTInt *k_array;
    IceTInt num_rounds;
    IceTInt my_partition_index;
    IceTInt lower_partition_index;
    IceTInt receiver_idx;

    k_array = radixkGetK(my_group_size, &num_rounds);
    my_partition_index = radixkGetFinalPartitionIndex(num_rounds,
                                                      k_array,
                                                      my_group_rank);

    k_array = radixkGetK(lower_group_size, &num_rounds);
    lower_partition_index = my_partition_index*num_receivers;
    for (receiver_idx = 0; receiver_idx < num_receivers; receiver_idx++) {
        IceTInt receiver_group_rank
            = radixkGetGroupRankForFinalPartitionIndex(num_rounds,
                                                       k_array,
                                                       lower_partition_index);
        receiver_ranks[receiver_idx] = lower_group[receiver_group_rank];
        lower_partition_index++;
    }

    *receiver_ranks_p = receiver_ranks;
    *num_receivers_p = num_receivers;
}

static void icetRadixkTelescopeComposeReceive(const IceTInt *my_group,
                                              IceTInt my_group_size,
                                              const IceTInt *upper_group,
                                              IceTInt upper_group_size,
                                              IceTInt largest_group_size,
                                              IceTBoolean local_in_front,
                                              IceTSparseImage input_image,
                                              IceTSparseImage *result_image,
                                              IceTSizeType *piece_offset)
{
    IceTSparseImage working_image = input_image;

    /* Start with the basic compose of my group. */
    icetRadixkBasicCompose(my_group,
                           my_group_size,
                           largest_group_size,
                           working_image,
                           piece_offset);

    /* Collect image from upper group. */
    if (upper_group_size > 0) {
        IceTInt upper_sender;
        IceTVoid *incoming_image_buffer;
        IceTSparseImage incoming_image;
        IceTSparseImage composited_image;
        IceTSizeType sparse_image_size;

        upper_sender
            = icetRadixkTelescopeFindUpperGroupSender(my_group,
                                                      my_group_size,
                                                      upper_group,
                                                      upper_group_size);

        sparse_image_size = icetSparseImageBufferSize(
                                 icetSparseImageGetNumPixels(working_image), 1);
        incoming_image_buffer = icetGetStateBuffer(RADIXK_RECEIVE_BUFFER,
                                                   sparse_image_size);

        /* Reuse the spare buffer for the final image.  At this point we are
           finishing compositing in this process and are either returning the
           image or sending it elsewhere, so using this buffer should be safe.
           I know.  Yuck. */
        composited_image = icetGetStateBufferSparseImage(
                                       RADIXK_SPARE_BUFFER,
                                       icetSparseImageGetWidth(working_image),
                                       icetSparseImageGetHeight(working_image));

        icetCommRecv(incoming_image_buffer,
                     sparse_image_size,
                     ICET_BYTE,
                     upper_sender,
                     RADIXK_TELESCOPE_IMAGE_TAG);
        incoming_image
            = icetSparseImageUnpackageFromReceive(incoming_image_buffer);

        if (local_in_front) {
            icetCompressedCompressedComposite(working_image,
                                              incoming_image,
                                              composited_image);
        } else {
            icetCompressedCompressedComposite(incoming_image,
                                              working_image,
                                              composited_image);
        }

        *result_image = composited_image;
    } else {
        *result_image = working_image;
    }
}

static void icetRadixkTelescopeComposeSend(const IceTInt *lower_group,
                                           IceTInt lower_group_size,
                                           const IceTInt *my_group,
                                           IceTInt my_group_size,
                                           IceTInt largest_group_size,
                                           IceTSparseImage input_image)
{
    const IceTInt *main_group;
    IceTInt main_group_size;
    const IceTInt *sub_group;
    IceTInt sub_group_size;
    IceTBoolean main_in_front;
    IceTInt main_group_rank;

    /* Check for any further telescoping. */
    main_group_size = radixkFindPower2(my_group_size);
    sub_group_size = my_group_size - main_group_size;

    main_group = my_group;
    sub_group = my_group + main_group_size;
    main_in_front = ICET_TRUE;
    main_group_rank = icetFindMyRankInGroup(main_group, main_group_size);

    if (main_group_rank >= 0) {
        /* In the main group. */
        IceTSparseImage working_image;
        IceTSizeType piece_offset;
        IceTInt *receiver_ranks;
        IceTInt num_receivers;
        IceTSizeType partition_num_pixels;
        IceTSizeType sparse_image_size;
        IceTVoid *send_buf_pool;
        IceTInt *piece_offsets;
        IceTSparseImage *image_pieces;
        IceTInt receiver_idx;
        IceTCommRequest *send_requests;

        icetRadixkTelescopeComposeReceive(main_group,
                                          main_group_size,
                                          sub_group,
                                          sub_group_size,
                                          largest_group_size,
                                          main_in_front,
                                          input_image,
                                          &working_image,
                                          &piece_offset);

        icetRadixkTelescopeFindLowerGroupReceivers(lower_group,
                                                   lower_group_size,
                                                   main_group,
                                                   main_group_size,
                                                   &receiver_ranks,
                                                   &num_receivers);

        partition_num_pixels = icetSparseImageSplitPartitionNumPixels(
                                     icetSparseImageGetNumPixels(working_image),
                                     num_receivers,
                                     largest_group_size/main_group_size);
        sparse_image_size = icetSparseImageBufferSize(partition_num_pixels, 1);
        send_buf_pool = icetGetStateBuffer(RADIXK_SEND_BUFFER,
                                           sparse_image_size * num_receivers);

        piece_offsets = icetGetStateBuffer(RADIXK_SPLIT_OFFSET_ARRAY_BUFFER,
                                           num_receivers * sizeof(IceTInt));
        image_pieces = icetGetStateBuffer(
                                       RADIXK_SPLIT_IMAGE_ARRAY_BUFFER,
                                       num_receivers * sizeof(IceTSparseImage));
        for (receiver_idx = 0; receiver_idx < num_receivers; receiver_idx++) {
            IceTVoid *send_buffer
                = ((IceTByte*)send_buf_pool + receiver_idx*sparse_image_size);
            image_pieces[receiver_idx]
                = icetSparseImageAssignBuffer(send_buffer,
                                              partition_num_pixels, 1);
        }

        icetSparseImageSplit(working_image,
                             piece_offset,
                             num_receivers,
                             largest_group_size/main_group_size,
                             image_pieces,
                             piece_offsets);

        send_requests
            = icetGetStateBuffer(RADIXK_SEND_REQUEST_BUFFER,
                                 num_receivers * sizeof(IceTCommRequest));
        for (receiver_idx = 0; receiver_idx < num_receivers; receiver_idx++) {
            IceTVoid *package_buffer;
            IceTSizeType package_size;
            IceTInt receiver_rank;

            icetSparseImagePackageForSend(image_pieces[receiver_idx],
                                          &package_buffer, &package_size);
            receiver_rank = receiver_ranks[receiver_idx];

            send_requests[receiver_idx]
                = icetCommIsend(package_buffer,
                                package_size,
                                ICET_BYTE,
                                receiver_rank,
                                RADIXK_TELESCOPE_IMAGE_TAG);
        }

        icetCommWaitall(num_receivers, send_requests);
    } else {
        /* In the sub group. */
        icetRadixkTelescopeComposeSend(main_group,
                                       main_group_size,
                                       sub_group,
                                       sub_group_size,
                                       largest_group_size,
                                       input_image);
    }
}

static void icetRadixkTelescopeCompose(const IceTInt *compose_group,
                                       IceTInt group_size,
                                       IceTInt image_dest,
                                       IceTSparseImage input_image,
                                       IceTSparseImage *result_image,
                                       IceTSizeType *piece_offset)
{
    const IceTInt *main_group;
    IceTInt main_group_size;
    const IceTInt *sub_group;
    IceTInt sub_group_size;
    IceTBoolean main_in_front;
    IceTBoolean use_interlace;
    IceTInt main_group_rank;

    IceTSparseImage working_image = input_image;

    main_group_size = radixkFindPower2(group_size);
    sub_group_size = group_size - main_group_size;
    /* Simple optimization to put image_dest in main group so that it has at
       least some data. */
    if (image_dest < main_group_size) {
        main_group = compose_group;
        sub_group = compose_group + main_group_size;
        main_in_front = ICET_TRUE;
    } else {
        sub_group = compose_group;
        main_group = compose_group + sub_group_size;
        main_in_front = ICET_FALSE;
    }
    main_group_rank = icetFindMyRankInGroup(main_group, main_group_size);

    /* Since we know the number of final pieces we will create
       (main_group_size), now is a good place to interlace the image (and then
       later adjust the offset. */
    {
        IceTInt magic_k;
        use_interlace = icetIsEnabled(ICET_INTERLACE_IMAGES);

        icetGetIntegerv(ICET_MAGIC_K, &magic_k);
        use_interlace &= main_group_size > magic_k;
    }

    if (use_interlace) {
        IceTSparseImage interlaced_image = icetGetStateBufferSparseImage(
                                       RADIXK_INTERLACED_IMAGE_BUFFER,
                                       icetSparseImageGetWidth(working_image),
                                       icetSparseImageGetHeight(working_image));
        icetSparseImageInterlace(working_image,
                                 main_group_size,
                                 RADIXK_PARTITION_INDICES_BUFFER,
                                 interlaced_image);
        working_image = interlaced_image;
    }

    /* Do the actual compositing. */
    if (main_group_rank >= 0) {
        /* In the main group. */
        icetRadixkTelescopeComposeReceive(main_group,
                                          main_group_size,
                                          sub_group,
                                          sub_group_size,
                                          main_group_size,
                                          main_in_front,
                                          working_image,
                                          result_image,
                                          piece_offset);
    } else {
        /* In the sub group. */
        icetRadixkTelescopeComposeSend(main_group,
                                       main_group_size,
                                       sub_group,
                                       sub_group_size,
                                       main_group_size,
                                       working_image);
        *result_image = icetSparseImageNull();
        *piece_offset = 0;
    }

    /* If we interlaced the image and are actually returning something,
       correct the offset. */
    if (use_interlace && !icetSparseImageIsNull(*result_image)) {
        IceTInt *k_array;
        IceTInt num_rounds;
        IceTInt global_partition;

        if (main_group_rank < 0) {
            icetRaiseError("Process not in main group got image piece.",
                           ICET_SANITY_CHECK_FAIL);
            return;
        }

        k_array = radixkGetK(main_group_size, &num_rounds);

        global_partition = radixkGetFinalPartitionIndex(num_rounds,
                                                        k_array,
                                                        main_group_rank);
        *piece_offset
            = icetGetInterlaceOffset(global_partition,
                                     group_size,
                                     icetSparseImageGetNumPixels(input_image));
    }

    return;
}


void icetRadixkCompose(const IceTInt *compose_group,
                       IceTInt group_size,
                       IceTInt image_dest,
                       IceTSparseImage input_image,
                       IceTSparseImage *result_image,
                       IceTSizeType *piece_offset)
{
    icetRadixkTelescopeCompose(compose_group,
                               group_size,
                               image_dest,
                               input_image,
                               result_image,
                               piece_offset);
}

ICET_EXPORT IceTBoolean icetRadixkPartitionLookupUnitTest(void)
{
    const IceTInt group_sizes_to_try[] = {
        2,                              /* Base case. */
        ICET_MAGIC_K_DEFAULT,           /* Largest direct send. */
        ICET_MAGIC_K_DEFAULT*2,         /* Changing group sizes. */
        1024,                           /* Large(ish) power of two. */
        576,                            /* Factors into 2 and 3. */
        509                             /* Prime. */
    };
    const IceTInt num_group_sizes_to_try
        = sizeof(group_sizes_to_try)/sizeof(IceTInt);
    IceTInt group_size_index;

    printf("\nTetsting rank/partition mapping.\n");

    for (group_size_index = 0;
         group_size_index < num_group_sizes_to_try;
         group_size_index++) {
        IceTInt group_size = group_sizes_to_try[group_size_index];
        IceTInt *partition_assignments;
        IceTInt *k_array;
        IceTInt num_rounds;
        IceTInt group_rank;
        IceTInt partition_index;

        printf("Trying size %d\n", group_size);

        partition_assignments = malloc(group_size * sizeof(IceTInt));
        for (partition_index = 0;
             partition_index < group_size;
             partition_index++) {
            partition_assignments[partition_index] = -1;
        }

        k_array = radixkGetK(group_size, &num_rounds);

        for (group_rank = 0; group_rank < group_size; group_rank++) {
            IceTInt rank_assignment;

            partition_index = radixkGetFinalPartitionIndex(num_rounds,
                                                           k_array,
                                                           group_rank);
            if ((partition_index < 0) || (group_size <= partition_index)) {
                printf("Invalid partition for rank %d.  Got partition %d.\n",
                       group_rank, partition_index);
                return ICET_FALSE;
            }
            if (partition_assignments[partition_index] != -1) {
                printf("Both ranks %d and %d report assigned partition %d.\n",
                       group_rank,
                       partition_assignments[partition_index],
                       partition_index);
                return ICET_FALSE;
            }
            partition_assignments[partition_index] = group_rank;

            rank_assignment
                = radixkGetGroupRankForFinalPartitionIndex(num_rounds,
                                                           k_array,
                                                           partition_index);
            if (rank_assignment != group_rank) {
                printf("Rank %d reports partition %d, but partition reports rank %d.\n",
                       group_rank, partition_index, rank_assignment);
                return ICET_FALSE;
            }
        }

        free(partition_assignments);
    }

    return ICET_TRUE;
}

#define MAIN_GROUP_RANK(idx)    (10000 + idx)
#define SUB_GROUP_RANK(idx)     (20000 + idx)
ICET_EXPORT IceTBoolean icetRadixTelescopeSendReceiveTest(void)
{
    IceTInt main_group_size;
    IceTInt rank;

    printf("\nTesting send/receive of telescope groups.\n");

    icetGetIntegerv(ICET_RANK, &rank);

    for (main_group_size = 2; main_group_size <= 4096; main_group_size *= 2) {
        IceTInt *main_group = malloc(main_group_size * sizeof(IceTInt));
        IceTInt main_group_idx;
        IceTInt sub_group_size;

        printf("Main group size %d\n", main_group_size);

        /* Fill initial main_group. */
        for (main_group_idx = 0;
             main_group_idx < main_group_size;
             main_group_idx++) {
            main_group[main_group_idx] = MAIN_GROUP_RANK(main_group_idx);
        }

        for (sub_group_size = 1;
             sub_group_size < main_group_size;
             sub_group_size *= 2) {
            IceTInt *sub_group = malloc(sub_group_size * sizeof(IceTInt));
            IceTInt sub_group_idx;

            printf("  Sub group size %d\n", sub_group_size);

            /* Fill initial sub_group. */
            for (sub_group_idx = 0;
                 sub_group_idx < sub_group_size;
                 sub_group_idx++) {
                sub_group[sub_group_idx] = SUB_GROUP_RANK(sub_group_idx);
            }

            /* Check the receives for each entry in sub_group. */
            /* Fill initial sub_group. */
            for (sub_group_idx = 0;
                 sub_group_idx < sub_group_size;
                 sub_group_idx++) {
                IceTInt *receiver_ranks;
                IceTInt num_receivers;
                IceTInt receiver_idx;
                /* The FindLowerGroupReceivers uses local rank to identify in
                   group.  Temporarily replace the rank in the group. */
                sub_group[sub_group_idx] = rank;
                icetRadixkTelescopeFindLowerGroupReceivers(main_group,
                                                           main_group_size,
                                                           sub_group,
                                                           sub_group_size,
                                                           &receiver_ranks,
                                                           &num_receivers);
                sub_group[sub_group_idx] = SUB_GROUP_RANK(sub_group_idx);

                /* For each receiver, check to make sure the correct sender
                   is identified. */
                for (receiver_idx = 0;
                     receiver_idx < num_receivers;
                     receiver_idx++)
                {
                    IceTInt receiver_rank = receiver_ranks[receiver_idx];
                    IceTInt receiver_group_rank;
                    IceTInt send_rank;

                    receiver_group_rank = icetFindRankInGroup(main_group,
                                                              main_group_size,
                                                              receiver_rank);
                    if (receiver_group_rank < 0) {
                        printf("Receiver %d for sub group rank %d is %d"
                               " but is not in main group.\n",
                               receiver_idx,
                               sub_group_idx,
                               receiver_rank);
                        return ICET_FALSE;
                    }

                    /* FindUpperGroupSend uses local rank to identify in group.
                       Temporarily replace the rank in the group. */
                    main_group[receiver_group_rank] = rank;
                    send_rank = icetRadixkTelescopeFindUpperGroupSender(
                                                                main_group,
                                                                main_group_size,
                                                                sub_group,
                                                                sub_group_size);
                    main_group[receiver_group_rank] = receiver_rank;

                    if (send_rank != SUB_GROUP_RANK(sub_group_idx)) {
                        printf("Receiver %d reported sender %d "
                               "but expected %d.\n",
                               receiver_rank,
                               send_rank,
                               SUB_GROUP_RANK(sub_group_idx));
                        return ICET_FALSE;
                    }
                }
            }
        }

        free(main_group);
    }

    return ICET_TRUE;
}
