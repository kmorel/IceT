/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#ifndef _TEST_UTIL_H_
#define _TEST_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <IceT.h>

extern IceTStrategy strategy_list[];
extern int STRATEGY_LIST_SIZE;

extern IceTSizeType SCREEN_WIDTH;
extern IceTSizeType SCREEN_HEIGHT;

void initialize_test(int *argcp, char ***argvp, IceTCommunicator comm);

int run_test(int (*test_function)(void));

void swap_buffers(void);

void finalize_test(int result);

void write_ppm(const char *filename,
               const IceTUByte *image,
               int width, int height);

#ifdef __cplusplus
}
#endif

#endif /*_TEST_UTIL_H_*/
