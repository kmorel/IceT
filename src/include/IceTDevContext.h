/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#ifndef _ICET_CONTEXT_H_
#define _ICET_CONTEXT_H_

#include <IceT.h>
#include <IceTDevState.h>

IceTState icetGetState();
IceTCommunicator icetGetCommunicator();

#endif /* _ICET_CONTEXT_H_ */
