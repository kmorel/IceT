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
unsigned long get_usec(void)
{
    struct timeval tp;

    gettimeofday(&tp, NULL);

    return (((unsigned long)tp.tv_sec) * 1000000) + (unsigned long)tp.tv_usec;
}
#else /*WIN32*/
unsigned long get_usec(void)
{
    return ((unsigned long)GetTickCount() * 1000);
}
#endif /*WIN32*/

double icetWallTime(void)
{
    return 0.000001*get_usec();
}
