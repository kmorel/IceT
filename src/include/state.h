/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

/* Id */

#ifndef _ICET_STATE_H_
#define _ICET_STATE_H_

#include <GL/ice-t.h>

typedef IceTUnsignedInt64 IceTTimeStamp;

struct IceTStateValue {
    GLenum type;
    GLint size;
    void *data;
    IceTTimeStamp mod_time;
};

typedef struct IceTStateValue *IceTState;

IceTState icetStateCreate(void);
void      icetStateDestroy(IceTState state);
void      icetStateCopy(IceTState dest, const IceTState src);
void      icetStateSetDefaults(void);

ICET_EXPORT void icetStateSetDoublev(GLenum pname, GLint size,
                                     const GLdouble *data);
ICET_EXPORT void icetStateSetFloatv(GLenum pname, GLint size,
                                    const GLfloat *data);
ICET_EXPORT void icetStateSetIntegerv(GLenum pname, GLint size,
                                      const GLint *data);
ICET_EXPORT void icetStateSetBooleanv(GLenum pname, GLint size,
                                      const GLboolean *data);
ICET_EXPORT void icetStateSetPointerv(GLenum pname, GLint size,
                                      const GLvoid **data);

ICET_EXPORT void icetStateSetDouble(GLenum pname, GLdouble value);
ICET_EXPORT void icetStateSetFloat(GLenum pname, GLfloat value);
ICET_EXPORT void icetStateSetInteger(GLenum pname, GLint value);
ICET_EXPORT void icetStateSetBoolean(GLenum pname, GLboolean value);
ICET_EXPORT void icetStateSetPointer(GLenum pname, const GLvoid *value);

ICET_EXPORT GLenum icetStateGetType(GLenum pname);
ICET_EXPORT GLint icetStateGetSize(GLenum pname);
ICET_EXPORT IceTTimeStamp icetStateGetTime(GLenum pname);

ICET_EXPORT void icetUnsafeStateSet(GLenum pname, GLint size, GLenum type,
                                    GLvoid *data);
ICET_EXPORT void *icetUnsafeStateGet(GLenum pname);
ICET_EXPORT GLenum icetStateType(GLenum pname);

ICET_EXPORT IceTTimeStamp icetGetTimeStamp(void);

void icetStateResetTiming(void);

void icetStateDump(void);

#endif /* _ICET_STATE_H_ */
