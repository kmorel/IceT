/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
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
