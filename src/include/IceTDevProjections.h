/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include <IceT.h>

ICET_EXPORT void icetProjectTile(IceTInt tile, IceTDouble *mat_out);

ICET_EXPORT void icetGetViewportProject(IceTInt x, IceTInt y,
					IceTSizeType width, IceTSizeType height,
					IceTDouble *mat_out);

ICET_EXPORT void icetMultMatrix(IceTDouble *C,
                                const IceTDouble *A, const IceTDouble *B);

/* Returns an orthographic projection that is equivalent to glOrtho. */
ICET_EXPORT void icetOrtho(IceTDouble left, IceTDouble right,
                           IceTDouble bottom, IceTDouble top,
                           IceTDouble znear, IceTDouble zfar,
                           IceTDouble *mat_out);
