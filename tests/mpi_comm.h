/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#ifndef _MPI_COMM_H_
#define _MPI_COMM_H_

#include "test-util.h"
#include <IceTMPI.h>

void init_mpi_comm(int *argcp, char ***argvp)
{
    IceTCommunicator comm;

    MPI_Init(argcp, argvp);
    comm = icetCreateMPICommunicator(MPI_COMM_WORLD);

    initialize_test(argcp, argvp, comm);

    icetDestroyMPICommunicator(comm);    
}

void finalize_communication(void)
{
    MPI_Finalize();
}

#endif /*_MPI_COMM_H_*/
