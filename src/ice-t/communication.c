/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include <IceTDevCommunication.h>

#include <IceT.h>
#include <IceTDevContext.h>
#include <IceTDevDiagnostics.h>
#include <IceTDevPorting.h>

#define icetAddSentBytes(num_sending)                                   \
    (icetUnsafeStateGetInteger(ICET_BYTES_SENT))[0] += (num_sending)

#define icetAddSent(count, datatype)                                    \
    (icetAddSentBytes((IceTInt)count*icetTypeWidth(datatype)))

#define icetCommCheckCount(count)                                       \
    if (count > 1073741824) {                                           \
        icetRaiseWarning("Encountered a ridiculously large message.",   \
                         ICET_INVALID_VALUE);                           \
    }

IceTCommunicator icetCommDuplicate()
{
    IceTCommunicator comm = icetGetCommunicator();
    return comm->Duplicate(comm);
}

void icetCommSend(const void *buf,
                  IceTSizeType count,
                  IceTEnum datatype,
                  int dest,
                  int tag)
{
    IceTCommunicator comm = icetGetCommunicator();
    icetCommCheckCount(count);
    icetAddSent(count, datatype);
    comm->Send(comm, buf, (int)count, datatype, dest, tag);
}

void icetCommRecv(void *buf,
                  IceTSizeType count,
                  IceTEnum datatype,
                  int src,
                  int tag)
{
    IceTCommunicator comm = icetGetCommunicator();
    icetCommCheckCount(count);
    comm->Recv(comm, buf, (int)count, datatype, src, tag);
}

void icetCommSendrecv(const void *sendbuf,
                      IceTSizeType sendcount,
                      IceTEnum sendtype,
                      int dest,
                      int sendtag,
                      void *recvbuf,
                      IceTSizeType recvcount,
                      IceTEnum recvtype,
                      int src,
                      int recvtag)
{
    IceTCommunicator comm = icetGetCommunicator();
    icetCommCheckCount(sendcount);
    icetCommCheckCount(recvcount);
    icetAddSent(sendcount, sendtype);
    comm->Sendrecv(comm, sendbuf, (int)sendcount, sendtype, dest, sendtag,
                   recvbuf, (int)recvcount, recvtype, src, recvtag);
}

void icetCommAllgather(const void *sendbuf,
                       IceTSizeType sendcount,
                       int type,
                       void *recvbuf)
{
    IceTCommunicator comm = icetGetCommunicator();
    icetCommCheckCount(sendcount);
    icetAddSent(sendcount, type);
    comm->Allgather(comm, sendbuf, (int)sendcount, type, recvbuf);
}

IceTCommRequest icetCommIsend(const void *buf,
                              IceTSizeType count,
                              IceTEnum datatype,
                              int dest,
                              int tag)
{
    IceTCommunicator comm = icetGetCommunicator();
    icetCommCheckCount(count);
    icetAddSent(count, datatype);
    return comm->Isend(comm, buf, (int)count, datatype, dest, tag);
}

IceTCommRequest icetCommIrecv(void *buf,
                              IceTSizeType count,
                              IceTEnum datatype,
                              int src,
                              int tag)
{
    IceTCommunicator comm = icetGetCommunicator();
    icetCommCheckCount(count);
    return comm->Irecv(comm, buf, (int)count, datatype, src, tag);
}

void icetCommWait(IceTCommRequest *request)
{
    IceTCommunicator comm = icetGetCommunicator();
    comm->Wait(comm, request);
}

int icetCommWaitany(int count, IceTCommRequest *array_of_requests)
{
    IceTCommunicator comm = icetGetCommunicator();
    return comm->Waitany(comm, count, array_of_requests);
}

int icetCommSize()
{
    IceTCommunicator comm = icetGetCommunicator();
    return comm->Comm_size(comm);
}

int icetCommRank()
{
    IceTCommunicator comm = icetGetCommunicator();
    return comm->Comm_rank(comm);
}
