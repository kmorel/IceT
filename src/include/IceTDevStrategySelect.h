/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 */

#ifndef __IceTDevStrategySelect_h
#define __IceTDevStrategySelect_h

#include <IceT.h>

#define ICET_STRATEGY_UNDEFINED (IceTEnum)-1

ICET_STRATEGY_EXPORT IceTBoolean icetStrategyValid(IceTEnum strategy);

ICET_STRATEGY_EXPORT const char *icetStrategyNameFromEnum(IceTEnum strategy);

ICET_STRATEGY_EXPORT IceTBoolean icetStrategySupportsOrdering(
                                                             IceTEnum strategy);

ICET_STRATEGY_EXPORT IceTImage icetInvokeStrategy(IceTEnum strategy);

ICET_STRATEGY_EXPORT IceTBoolean icetSingleImageStrategyValid(
                                                             IceTEnum strategy);

ICET_STRATEGY_EXPORT const char *icetSingleImageStrategyNameFromEnum(
                                                             IceTEnum strategy);

ICET_STRATEGY_EXPORT void icetInvokeSingleImageStrategy(IceTEnum strategy,
                                                        IceTInt *compose_group,
                                                        IceTInt group_size,
                                                        IceTInt image_dest,
                                                        IceTImage image);

#endif /*__IceTDevStrategySelect_h*/
