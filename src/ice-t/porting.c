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

#include <porting.h>
#include <GL/ice-t.h>

#ifndef WIN32
#include <sys/time.h>
#else
#include <windows.h>
#include <winbase.h>
#endif

#ifndef WIN32
double icetWallTime(void)
{
    static struct timeval start = { 0, 0 };
    struct timeval now;
    struct timeval *tp;

  /* Make the first call to icetWallTime happen at second 0.  This should
     allow for more significant bits in the microseconds. */
    if (start.tv_sec == 0) {
        tp = &start;
    } else {
        tp = &now;
    }

    gettimeofday(tp, NULL);

    return (tp->tv_sec - start.tv_sec) + 0.000001*(double)tp->tv_usec;
}
#else /*WIN32*/
double icetWallTime(void)
{
    static DWORD start = 0;

    if (start == 0) {
        start = GetTickCount();
        return 0.0;
    } else {
        DWORD now = GetTickCount();
        return 0.001*(now-start);
    }
}
#endif /*WIN32*/
