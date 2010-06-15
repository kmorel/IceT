/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#ifndef _ICET_CONTEXT_H_
#define _ICET_CONTEXT_H_

#include <IceT.h>
#include <state.h>
#include <image.h>

struct IceTContext {
    IceTState state;
    IceTCommunicator communicator;
    IceTStrategy strategy;
    IceTVoid *buffer;
    IceTSizeType buffer_size;
    IceTSizeType buffer_offset;
    IceTUInt display_inflate_texture;
};

ICET_EXPORT extern struct IceTContext *icet_current_context;

#define icetGetState()          (icet_current_context->state)
#define icetGetCommunicator()   (icet_current_context->communicator)

ICET_EXPORT void            icetResizeBuffer(IceTSizeType size);
ICET_EXPORT IceTVoid *      icetReserveBufferMem(IceTSizeType size);
ICET_EXPORT IceTImage       icetReserveBufferImage(IceTSizeType num_pixels);
ICET_EXPORT IceTSparseImage icetReserveBufferSparseImage(
                                                       IceTSizeType num_pixels);

#define ICET_COMM_DUPLICTE()                                            \
    (icetGetCommunicator()->Duplicate(icetGetCommunicator()))
#define ICET_COMM_DESTROY()                                             \
    (icetGetCommunicator()->Destroy(icetGetCommunicator()))
#define ICET_COMM_SEND(buf, count, datatype, dest, tag)                 \
    (icetGetCommunicator()->Send(icetGetCommunicator(),                 \
                                 buf, count, datatype, dest, tag))
#define ICET_COMM_RECV(buf, count, datatype, src, tag)                  \
    (icetGetCommunicator()->Recv(icetGetCommunicator(),                 \
                                 buf, count, datatype, src, tag))
#define ICET_COMM_SENDRECV(sendbuf, sendcount, sendtype, dest, sendtag, \
                           recvbuf, recvcount, recvtype, src, recvtag)  \
    (icetGetCommunicator()->Sendrecv(icetGetCommunicator(),             \
                                     sendbuf, sendcount, sendtype,      \
                                     dest, sendtag,                     \
                                     recvbuf, recvcount, recvtype,      \
                                     src, recvtag))
#define ICET_COMM_ALLGATHER(sendbuf, sendcount, type, recvbuf)          \
    (icetGetCommunicator()->Allgather(icetGetCommunicator(),            \
                                      sendbuf, sendcount, type, recvbuf))
#define ICET_COMM_ISEND(buf, count, datatype, dest, tag)                \
    (icetGetCommunicator()->Isend(icetGetCommunicator(),                \
                                  buf, count, datatype, dest, tag))
#define ICET_COMM_IRECV(buf, count, datatype, src, tag)                 \
    (icetGetCommunicator()->Irecv(icetGetCommunicator(),                \
                                  buf, count, datatype, src, tag))
#define ICET_COMM_WAIT(request)                                         \
    (icetGetCommunicator()->Wait(icetGetCommunicator(), request))
#define ICET_COMM_WAITANY(count, array_of_requests)                     \
    (icetGetCommunicator()->Waitany(icetGetCommunicator(),              \
                                    count, array_of_requests))
#define ICET_COMM_SIZE()                                                \
    (icetGetCommunicator()->Comm_size(icetGetCommunicator()))
#define ICET_COMM_RANK()                                                \
    (icetGetCommunicator()->Comm_rank(icetGetCommunicator()))

#endif /* _ICET_CONTEXT_H_ */
