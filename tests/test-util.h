/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#ifndef _TEST_UTIL_H_
#define _TEST_UTIL_H_

#include "test-config.h"

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <IceT.h>

extern IceTEnum strategy_list[];
extern int STRATEGY_LIST_SIZE;

extern IceTEnum single_image_strategy_list[];
extern int SINGLE_IMAGE_STRATEGY_LIST_SIZE;

extern IceTSizeType SCREEN_WIDTH;
extern IceTSizeType SCREEN_HEIGHT;

void initialize_test(int *argcp, char ***argvp, IceTCommunicator comm);

int run_test(int (*test_function)(void));

#ifdef ICET_TESTS_USE_OPENGL
void swap_buffers(void);
#endif

void finalize_test(int result);

void write_ppm(const char *filename,
               const IceTUByte *image,
               int width, int height);

IceTBoolean strategy_uses_single_image_strategy(IceTEnum strategy);

#ifdef __cplusplus
}
#endif

#endif /*_TEST_UTIL_H_*/
