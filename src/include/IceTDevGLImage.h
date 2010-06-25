/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 */

#ifndef __icetGLImage_h
#define __icetGLImage_h

#include <IceTGL.h>

ICET_GL_EXPORT void icetGLDrawCallbackFunction(
                                            const IceTDouble *projection_matrix,
                                            const IceTDouble *modelview_matrix,
                                            const IceTFloat *background_color,
                                            const IceTInt *readback_viewport,
                                            IceTImage result);

#endif //__icetGLImage_h
