/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#ifndef _ICET_MPI_H_
#define _ICET_MPI_H_

#include <IceT.h>
#include <mpi.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

ICET_MPI_EXPORT IceTCommunicator icetCreateMPICommunicator(MPI_Comm mpi_comm);
ICET_MPI_EXPORT void icetDestroyMPICommunicator(IceTCommunicator comm);

#ifdef __cplusplus
}
#endif

#endif /*_ICET_MPI_H_*/
