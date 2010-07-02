/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#ifndef _ICET_STATE_H_
#define _ICET_STATE_H_

#include <IceT.h>

typedef IceTUnsignedInt64 IceTTimeStamp;

struct IceTStateValue;
typedef struct IceTStateValue *IceTState;

IceTState icetStateCreate(void);
void      icetStateDestroy(IceTState state);
void      icetStateCopy(IceTState dest, const IceTState src);
void      icetStateSetDefaults(void);

ICET_EXPORT void icetStateSetDoublev(IceTEnum pname,
                                     IceTSizeType num_entries,
                                     const IceTDouble *data);
ICET_EXPORT void icetStateSetFloatv(IceTEnum pname,
                                    IceTSizeType num_entries,
                                    const IceTFloat *data);
ICET_EXPORT void icetStateSetIntegerv(IceTEnum pname,
                                      IceTSizeType num_entries,
                                      const IceTInt *data);
ICET_EXPORT void icetStateSetBooleanv(IceTEnum pname,
                                      IceTSizeType num_entries,
                                      const IceTBoolean *data);
ICET_EXPORT void icetStateSetPointerv(IceTEnum pname,
                                      IceTSizeType num_entries,
                                      const IceTVoid **data);

ICET_EXPORT void icetStateSetDouble(IceTEnum pname, IceTDouble value);
ICET_EXPORT void icetStateSetFloat(IceTEnum pname, IceTFloat value);
ICET_EXPORT void icetStateSetInteger(IceTEnum pname, IceTInt value);
ICET_EXPORT void icetStateSetBoolean(IceTEnum pname, IceTBoolean value);
ICET_EXPORT void icetStateSetPointer(IceTEnum pname, const IceTVoid *value);

ICET_EXPORT IceTEnum icetStateGetType(IceTEnum pname);
ICET_EXPORT IceTSizeType icetStateGetNumEntries(IceTEnum pname);
ICET_EXPORT IceTTimeStamp icetStateGetTime(IceTEnum pname);

ICET_EXPORT void icetUnsafeStateSet(IceTEnum pname,
                                    IceTSizeType size,
                                    IceTEnum type,
                                    IceTVoid *data);
ICET_EXPORT IceTDouble  *icetUnsafeStateGetDouble(IceTEnum pname);
ICET_EXPORT IceTFloat   *icetUnsafeStateGetFloat(IceTEnum pname);
ICET_EXPORT IceTInt     *icetUnsafeStateGetInteger(IceTEnum pname);
ICET_EXPORT IceTBoolean *icetUnsafeStateGetBoolean(IceTEnum pname);
ICET_EXPORT IceTVoid   **icetUnsafeStateGetPointer(IceTEnum pname);
ICET_EXPORT IceTEnum icetStateType(IceTEnum pname);

ICET_EXPORT IceTVoid       *icetGetStateBuffer(IceTEnum pname,
                                               IceTSizeType num_bytes);

ICET_EXPORT IceTTimeStamp icetGetTimeStamp(void);

void icetStateResetTiming(void);

void icetStateDump(void);

#endif /* _ICET_STATE_H_ */
