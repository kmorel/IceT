/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#ifndef _ICET_PORTING_H_
#define _ICET_PORTING_H_

#include <IceT.h>

/* Returns the size of the type given by the identifier (ICET_INT, ICET_FLOAT,
   etc.)  in bytes. */
ICET_EXPORT IceTSizeType icetTypeWidth(IceTEnum type);

#endif /*_ICET_PORTING_H_*/
