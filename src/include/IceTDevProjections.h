/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#ifndef __IceTDevProjections_h
#define __IceTDevProjections_h

#include <IceT.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

ICET_EXPORT void icetProjectTile(IceTInt tile, IceTDouble *mat_out);

ICET_EXPORT void icetGetViewportProject(IceTInt x, IceTInt y,
					IceTSizeType width, IceTSizeType height,
					IceTDouble *mat_out);

#ifdef __cplusplus
}
#endif

#endif /*__IceTDevProjections_h*/
