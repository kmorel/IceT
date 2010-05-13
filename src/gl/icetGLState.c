/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 */

#include <IceTGL.h>

#include <diagnostics.h>

void icetGLInitialize(void)
{
    if (icetStateGetType(ICET_GL_INITIALIZED) != ICET_NULL) {
        IceTBoolean initialized;
        icetGetBooleanv(ICET_GL_INITIALIZED, &initialized);
        if (initialized) {
            icetRaiseWarning("icetGLInitialize called multiple times.",
                             ICET_INVALID_OPERATION);
        }
    }

    icetStateSetBoolean(ICET_GL_INITIALIZED, ICET_TRUE);

    icetStateSetInteger(ICET_GL_READ_BUFFER, GL_BACK);
    icetStateSetInteger(ICET_GL_COLOR_FORMAT, GL_RGBA);
}

void icetGLSetReadBuffer(GLenum mode)
{
    if (   (mode == GL_FRONT_LEFT) || (mode == GL_FRONT_RIGHT)
        || (mode == GL_BACK_LEFT)  || (mode == GL_BACK_RIGHT)
        || (mode == GL_FRONT) || (mode == GL_BACK)
        || (mode == GL_LEFT) || (mode == GL_RIGHT)
        || ((mode >= GL_AUX0) && (mode < GL_AUX0 + GL_AUX_BUFFERS)) )
    {
        icetStateSetInteger(ICET_GL_READ_BUFFER, GL_BACK);
    } else {
        icetRaiseError("Invalid OpenGL read buffer.", ICET_INVALID_ENUM);
    }
}
