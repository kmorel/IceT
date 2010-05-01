/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#ifndef _ICET_BUCKETS_H_
#define _ICET_BUCKETS_H_

#include "IceT.h"

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

typedef struct icet_bucket_st {
    IceTDouble *bounds;
    IceTInt num_bounds;
} *IceTBucket;

ICET_EXPORT IceTBucket icetCreateBucket(void);
ICET_EXPORT void       icetDestroyBucket(IceTBucket bucket);

ICET_EXPORT void icetBucketVertices(IceTBucket bucket,
                                    IceTInt size, IceTEnum type,
                                    IceTSizeType stride, IceTSizeType count,
                                    const IceTVoid *pointer);
ICET_EXPORT void icetBucketBoxd(IceTBucket bucket,
                                IceTDouble x_min, IceTDouble x_max,
                                IceTDouble y_min, IceTDouble y_max,
                                IceTDouble z_min, IceTDouble z_max);
ICET_EXPORT void icetBucketBoxf(IceTBucket bucket,
                                IceTFloat x_min, IceTFloat x_max,
                                IceTFloat y_min, IceTFloat y_max,
                                IceTFloat z_min, IceTFloat z_max);

ICET_EXPORT IceTBoolean icetBucketInView(IceTBucket bucket,
                                         IceTDouble *transform);

ICET_EXPORT void icetSetBoundsFromBuckets(IceTBucket *buckets, int num_buckets);

ICET_EXPORT void icetBucketsDraw(const IceTBucket *buckets, int num_buckets,
                                 void (*draw_func)(int));

#ifdef __cplusplus
}
#endif

#endif /*_ICET_BUCKETS_H_*/
