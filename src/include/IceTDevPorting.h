/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#ifndef _ICET_PORTING_H_
#define _ICET_PORTING_H_

#include <IceT.h>

/* Returns the size of the type given by the identifier (ICET_INT, ICET_FLOAT,
   etc.)  in bytes. */
ICET_EXPORT IceTInt icetTypeWidth(IceTEnum type);

#endif /*_ICET_PORTING_H_*/
