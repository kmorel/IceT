/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#ifndef __IceTDevCommunication_h
#define __IceTDevCommunication_h

#include <IceT.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

/* All of these methods call the associated method in the communicator for
   the current context. */
ICET_EXPORT IceTCommunicator icetCommDuplicate();
ICET_EXPORT void icetCommSend(const void *buf,
                              int count,
                              IceTEnum datatype,
                              int dest,
                              int tag);
ICET_EXPORT void icetCommRecv(void *buf,
                              int count,
                              IceTEnum datatype,
                              int src,
                              int tag);
ICET_EXPORT void icetCommSendrecv(const void *sendbuf,
                                  int sendcount,
                                  IceTEnum sendtype,
                                  int dest,
                                  int sendtag,
                                  void *recvbuf,
                                  int recvcount,
                                  IceTEnum recvtype,
                                  int src,
                                  int recvtag);
ICET_EXPORT void icetCommAllgather(const void *sendbuf,
                                   int sendcount,
                                   int type,
                                   void *recvbuf);
ICET_EXPORT IceTCommRequest icetCommIsend(const void *buf,
                                          int count,
                                          IceTEnum datatype,
                                          int dest,
                                          int tag);
ICET_EXPORT IceTCommRequest icetCommIrecv(void *buf,
                                          int count,
                                          IceTEnum datatype,
                                          int src,
                                          int tag);
ICET_EXPORT void icetCommWait(IceTCommRequest *request);
ICET_EXPORT int icetCommWaitany(int count, IceTCommRequest *array_of_requests);
ICET_EXPORT void icetCommWaitall(int count, IceTCommRequest *array_of_requests);
ICET_EXPORT int icetCommSize();
ICET_EXPORT int icetCommRank();

#ifdef __cplusplus
}
#endif

#endif /*__icetDevCommunication_h*/
