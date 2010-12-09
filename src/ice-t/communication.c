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
#include <IceTDevPorting.h>

#define icetAddSentBytes(num_sending)                                   \
    (icetUnsafeStateGetInteger(ICET_BYTES_SENT))[0] += (num_sending)

#define icetAddSent(count, datatype)                                    \
    (icetAddSentBytes(count*icetTypeWidth(datatype)))

IceTCommunicator icetCommDuplicate()
{
    IceTCommunicator comm = icetGetCommunicator();
    return comm->Duplicate(comm);
}

void icetCommSend(const void *buf,
                  int count,
                  IceTEnum datatype,
                  int dest,
                  int tag)
{
    IceTCommunicator comm = icetGetCommunicator();
    icetAddSent(count, datatype);
    comm->Send(comm, buf, count, datatype, dest, tag);
}

void icetCommRecv(void *buf,
                  int count,
                  IceTEnum datatype,
                  int src,
                  int tag)
{
    IceTCommunicator comm = icetGetCommunicator();
    comm->Recv(comm, buf, count, datatype, src, tag);
}

void icetCommSendrecv(const void *sendbuf,
                      int sendcount,
                      IceTEnum sendtype,
                      int dest,
                      int sendtag,
                      void *recvbuf,
                      int recvcount,
                      IceTEnum recvtype,
                      int src,
                      int recvtag)
{
    IceTCommunicator comm = icetGetCommunicator();
    icetAddSent(sendcount, sendtype);
    comm->Sendrecv(comm, sendbuf, sendcount, sendtype, dest, sendtag,
                   recvbuf, recvcount, recvtype, src, recvtag);
}

void icetCommGather(const void *sendbuf,
                    int sendcount,
                    int type,
                    void *recvbuf,
                    int root)
{
    IceTCommunicator comm = icetGetCommunicator();
    icetAddSent(sendcount, type);
    comm->Gather(comm, sendbuf, sendcount, type, recvbuf, root);
}

void icetCommAllgather(const void *sendbuf,
                       int sendcount,
                       int type,
                       void *recvbuf)
{
    IceTCommunicator comm = icetGetCommunicator();
    icetAddSent(sendcount, type);
    comm->Allgather(comm, sendbuf, sendcount, type, recvbuf);
}

IceTCommRequest icetCommIsend(const void *buf,
                              int count,
                              IceTEnum datatype,
                              int dest,
                              int tag)
{
    IceTCommunicator comm = icetGetCommunicator();
    icetAddSent(count, datatype);
    return comm->Isend(comm, buf, count, datatype, dest, tag);
}

IceTCommRequest icetCommIrecv(void *buf,
                              int count,
                              IceTEnum datatype,
                              int src,
                              int tag)
{
    IceTCommunicator comm = icetGetCommunicator();
    return comm->Irecv(comm, buf, count, datatype, src, tag);
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

void icetCommWaitall(int count, IceTCommRequest *array_of_requests)
{
    int i;
    for (i = 0; i < count; i++) {
        icetCommWait(&array_of_requests[i]);
    }
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
