/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include <IceT.h>

#include <IceTDevCommunication.h>
#include <IceTDevDiagnostics.h>
#include <IceTDevImage.h>

#define BSWAP_IN_SPARSE_IMAGE_BUFFER    ICET_SI_STRATEGY_BUFFER_0
#define BSWAP_OUT_SPARSE_IMAGE_BUFFER   ICET_SI_STRATEGY_BUFFER_1

#define SWAP_IMAGE_DATA 21
#define SWAP_DEPTH_DATA 22

#define MIN(x,y) ((x) <= (y) ? (x) : (y))

#define BIT_REVERSE(result, x, max_val_plus_one)                              \
{                                                                             \
    int placeholder;                                                          \
    int input = (x);                                                          \
    (result) = 0;                                                             \
    for (placeholder=0x0001; placeholder<max_val_plus_one; placeholder<<=1) { \
        (result) <<= 1;                                                       \
        (result) += input & 0x0001;                                           \
        input >>= 1;                                                          \
    }                                                                         \
}

static void bswapCollectFinalImages(IceTInt *compose_group, IceTInt group_size,
                                    IceTInt group_rank, IceTImage image,
                                    IceTSizeType pixel_count)
{
    IceTEnum color_format, depth_format;
    IceTCommRequest *requests;
    int i;

  /* Adjust image for output as some buffers, such as depth, might be
     dropped. */
    icetImageAdjustForOutput(image);

  /* All processors have the same number for pixels and their offset
   * is group_rank*offset. */
    color_format = icetImageGetColorFormat(image);
    depth_format = icetImageGetDepthFormat(image);
    requests = malloc((group_size)*sizeof(IceTCommRequest));

    if (color_format != ICET_IMAGE_COLOR_NONE) {
      /* Use IceTByte for byte-based pointer arithmetic. */
        IceTByte *colorBuffer;
        IceTSizeType pixel_size;
        colorBuffer = icetImageGetColorVoid(image, &pixel_size);
        icetRaiseDebug("Collecting image data.");
        for (i = 0; i < group_size; i++) {
            IceTInt src;
          /* Actual piece is located at the bit reversal of i. */
            BIT_REVERSE(src, i, group_size);
            if (src != group_rank) {
                requests[i] =
                    icetCommIrecv(colorBuffer + pixel_size*pixel_count*i,
                                  pixel_size*pixel_count, ICET_BYTE,
                                  compose_group[src], SWAP_IMAGE_DATA);
            } else {
                requests[i] = ICET_COMM_REQUEST_NULL;
            }
        }
        for (i = 0; i < group_size; i++) {
            icetCommWait(requests + i);
        }
    }

    if (depth_format != ICET_IMAGE_DEPTH_NONE) {
      /* Use IceTByte for byte-based pointer arithmetic. */
        IceTByte *depthBuffer;
        IceTSizeType pixel_size;
        depthBuffer = icetImageGetDepthVoid(image, &pixel_size);
        icetRaiseDebug("Collecting depth data.");
        for (i = 0; i < group_size; i++) {
            IceTInt src;
          /* Actual peice is located at the bit reversal of i. */
            BIT_REVERSE(src, i, group_size);
            if (src != group_rank) {
                requests[i] =
                    icetCommIrecv(depthBuffer + pixel_size*pixel_count*i,
                                  pixel_size*pixel_count, ICET_BYTE,
                                  compose_group[src], SWAP_DEPTH_DATA);
            } else {
                requests[i] = ICET_COMM_REQUEST_NULL;
            }
        }
        for (i = 0; i < group_size; i++) {
            icetCommWait(requests + i);
        }
    }
    free(requests);
}

static void bswapSendFinalImage(IceTInt *compose_group, IceTInt image_dest,
                                IceTImage image,
                                IceTSizeType pixel_count, IceTSizeType offset)
{
    IceTEnum color_format, depth_format;

  /* Adjust image for output as some buffers, such as depth, might be
     dropped. */
    icetImageAdjustForOutput(image);

    color_format = icetImageGetColorFormat(image);
    depth_format = icetImageGetDepthFormat(image);

  /* Correct for last piece that may overrun image size. */
    pixel_count = MIN(pixel_count, icetImageGetNumPixels(image) - offset);

    if (color_format != ICET_IMAGE_COLOR_NONE) {
      /* Use IceTByte for byte-based pointer arithmetic. */
        IceTByte *colorBuffer;
        IceTSizeType pixel_size;
        colorBuffer = icetImageGetColorVoid(image, &pixel_size);
        icetRaiseDebug("Sending image data.");
        icetCommSend(colorBuffer + pixel_size*offset,
                     pixel_size*pixel_count, ICET_BYTE,
                     compose_group[image_dest], SWAP_IMAGE_DATA);
    }

    if (depth_format != ICET_IMAGE_DEPTH_NONE) {
      /* Use IceTByte for byte-based pointer arithmetic. */
        IceTByte *depthBuffer;
        IceTSizeType pixel_size;
        depthBuffer = icetImageGetDepthVoid(image, &pixel_size);
        icetRaiseDebug("Sending depth data.");
        icetCommSend(depthBuffer + pixel_size*offset,
                     pixel_size*pixel_count, ICET_BYTE,
                     compose_group[image_dest], SWAP_DEPTH_DATA);
    }
}

/* Does binary swap, but does not combine the images in the end.  Instead,
 * the image is broken into pow2size pieces and stored in the first set of
 * processes.  pow2size is assumed to be the largest power of 2 <=
 * group_size.  Each process has the image offset in buffer to its
 * appropriate location.  Each process contains the ith piece, where i is
 * group_rank with the bits reversed (which is necessary to get the
 * ordering correct).  If both color and depth buffers are inputs, both are
 * located in the uncollected images regardless of what buffers are
 * selected for outputs. */
static void bswapComposeNoCombine(IceTInt *compose_group, IceTInt group_size,
                                  IceTInt pow2size, IceTInt group_rank,
                                  IceTImage image,
                                  IceTSizeType pixel_count,
                                  IceTVoid *inSparseImageBuffer,
                                  IceTSparseImage outSparseImage)
{
    IceTInt extra_proc;   /* group_size - pow2size */
    IceTInt extra_pow2size;       /* extra_proc rounded down to nearest power of 2. */

    extra_proc = group_size - pow2size;
    for (extra_pow2size = 1; extra_pow2size <= extra_proc; extra_pow2size *= 2);
    extra_pow2size /= 2;

    if (group_rank >= pow2size) {
        IceTInt upper_group_rank = group_rank - pow2size;
      /* I am part of the extra stuff.  Recurse to run bswap on my part. */
        bswapComposeNoCombine(compose_group + pow2size, extra_proc,
                              extra_pow2size, upper_group_rank,
                              image, pixel_count,
                              inSparseImageBuffer, outSparseImage);
      /* Now I may have some image data to send to lower group. */
        if (upper_group_rank < extra_pow2size) {
            IceTInt num_pieces = pow2size/extra_pow2size;
            IceTUInt offset;
            int i;

            BIT_REVERSE(offset, upper_group_rank, extra_pow2size);
            icetRaiseDebug1("My offset: %d", (int)offset);
            offset *= pixel_count/extra_pow2size;

          /* Trying to figure out what processes to send to is tricky.  We
           * can do this by getting the piece number (bit reversal of
           * upper_group_rank), multiply this by num_pieces, add the number
           * of each local piece to get the piece number for the lower
           * half, and finally reverse the bits again.  Equivocally, we can
           * just reverse the bits of the local piece num, multiply by
           * num_peices and add that to upper_group_rank to get the final
           * location. */
            pixel_count = pixel_count/pow2size;
            for (i = 0; i < num_pieces; i++) {
                IceTVoid *package_buffer;
                IceTSizeType package_size;
                IceTInt dest_rank;
                IceTSizeType pixel_start;
                IceTSizeType pixels_sending;

                BIT_REVERSE(dest_rank, i, num_pieces);
                dest_rank = dest_rank*extra_pow2size + upper_group_rank;
                icetRaiseDebug2("Sending piece %d to %d", i, (int)dest_rank);

              /* Make sure we don't send pixels past the end of the buffer. */
                pixel_start = offset + i*pixel_count;
                pixels_sending
                    = MIN(pixel_count,icetImageGetNumPixels(image)-pixel_start);

              /* Is compression the right thing?  It's currently easier. */
                icetCompressSubImage(image, pixel_start, pixels_sending,
                                     outSparseImage);
                icetSparseImagePackageForSend(outSparseImage,
                                              &package_buffer, &package_size);
              /* Send to processor in lower "half" that has same part of
               * image. */
                icetCommSend(package_buffer, package_size, ICET_BYTE,
                             compose_group[dest_rank],
                             SWAP_IMAGE_DATA);
            }
        }
        return;
    } else {
      /* I am part of the lower group.  Do the actual binary swap. */
        IceTEnum color_format, depth_format;
        int bitmask;
        int offset;

        color_format = icetImageGetColorFormat(image);
        depth_format = icetImageGetDepthFormat(image);

      /* To do the ordering correct, at iteration i we must swap with a
       * process 2^i units away.  The easiest way to find the process to
       * pair with is to simply xor the group_rank with a value with the
       * ith bit set. */

        for (bitmask = 0x0001, offset = 0; bitmask < pow2size; bitmask <<= 1) {
            IceTInt pair;
            IceTInt inOnTop;
            IceTVoid *package_buffer;
            IceTSizeType package_size;
            IceTSizeType incoming_size;
            IceTSparseImage inSparseImage;
            IceTSizeType truncated_pixel_count;

            pair = group_rank ^ bitmask;

            pixel_count /= 2;

          /* Pieces grabbed at the bottom of the image may be truncated if the
             pixel count does not divide evently.  Check for that. */
            truncated_pixel_count
                = MIN(pixel_count,
                      icetImageGetNumPixels(image)-offset-pixel_count);

            if (group_rank < pair) {
                icetCompressSubImage(image, offset + pixel_count,
                                     truncated_pixel_count,
                                     outSparseImage);
                inOnTop = 0;
            } else {
                icetCompressSubImage(image, offset, pixel_count,
                                     outSparseImage);
                inOnTop = 1;
                offset += pixel_count;
            }

            icetSparseImagePackageForSend(outSparseImage,
                                          &package_buffer, &package_size);
            incoming_size = icetSparseImageBufferSizeType(color_format,
                                                          depth_format,
                                                          pixel_count, 1);
            icetCommSendrecv(package_buffer, package_size,
                             ICET_BYTE, compose_group[pair], SWAP_IMAGE_DATA,
                             inSparseImageBuffer, incoming_size,
                             ICET_BYTE, compose_group[pair], SWAP_IMAGE_DATA);

            inSparseImage
                = icetSparseImageUnpackageFromReceive(inSparseImageBuffer);
            icetCompressedSubComposite(image, offset, inSparseImage, inOnTop);
        }

      /* Now absorb any image that was part of extra stuff. */
      /* To get the processor where the extra stuff is located, I could
       * reverse the bits of the local process, divide by the appropriate
       * amount, and reverse the bits again.  However, the equivalent to
       * this is just clearing out the upper bits. */
        if (extra_pow2size > 0) {
            IceTSizeType incoming_size;
            IceTInt src;
            IceTSparseImage inSparseImage;
            icetRaiseDebug1("Absorbing image from %d", (int)src);
            incoming_size = icetSparseImageBufferSizeType(color_format,
                                                          depth_format,
                                                          pixel_count, 1);
            src = pow2size + (group_rank & (extra_pow2size-1));
            icetCommRecv(inSparseImageBuffer, incoming_size,
                         ICET_BYTE, compose_group[src], SWAP_IMAGE_DATA);
            inSparseImage
                = icetSparseImageUnpackageFromReceive(inSparseImageBuffer);
            icetCompressedSubComposite(image, offset, inSparseImage, 0);
        }
    }
}

void icetBswapCompose(IceTInt *compose_group, IceTInt group_size,
                      IceTInt image_dest,
                      IceTImage image)
{
    IceTInt group_rank;
    IceTInt rank;
    IceTInt pow2size;
    IceTUInt pixel_count;
    IceTVoid *inSparseImageBuffer;
    IceTSparseImage outSparseImage;
    IceTSizeType width, height;

    icetRaiseDebug("In bswapCompose");
#if 1 
    width = icetImageGetWidth(image);
    height = icetImageGetHeight(image);

    icetGetIntegerv(ICET_RANK, &rank);
    group_rank = 0;
    while ((group_rank < group_size) && (compose_group[group_rank] != rank)) {
        group_rank++;
    }
    if (group_rank >= group_size) {
        icetRaiseError("Local process not in compose_group?",
                       ICET_SANITY_CHECK_FAIL);
        return;
    }

  /* Make size of group be a power of 2. */
    for (pow2size = 1; pow2size <= group_size; pow2size *= 2);
    pow2size /= 2;

    pixel_count = icetImageGetNumPixels(image);
  /* Make sure we can divide pixels evenly amongst processors. */
  /* WARNING: This adds padding at the end of the image that could cause an
     error if you do not catch it. */
    pixel_count = ((pixel_count+pow2size-1)/pow2size)*pow2size;

    inSparseImageBuffer = icetGetStateBuffer(BSWAP_IN_SPARSE_IMAGE_BUFFER,
                                      icetSparseImageBufferSize(width, height));
    outSparseImage =icetGetStateBufferSparseImage(BSWAP_OUT_SPARSE_IMAGE_BUFFER,
                                                  width, height);

  /* Do actual bswap. */
    bswapComposeNoCombine(compose_group, group_size, pow2size, group_rank,
                          image, pixel_count,
                          inSparseImageBuffer, outSparseImage);
    if (group_rank == image_dest) {
      /* Collect image if I'm the destination. */
        bswapCollectFinalImages(compose_group, pow2size, group_rank,
                                image, pixel_count/pow2size);
    } else if (group_rank < pow2size) {
      /* Send image to destination. */
        IceTInt sub_image_size = pixel_count/pow2size;
        IceTInt piece_num;
        BIT_REVERSE(piece_num, group_rank, pow2size);
        bswapSendFinalImage(compose_group, image_dest, image,
                            sub_image_size, piece_num*sub_image_size);
    }
#endif
}
