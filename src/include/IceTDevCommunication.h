/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#ifndef _ICET_COMMUNICATION_H_
#define _ICET_COMMUNICATION_H_

#include <IceT.h>

/* All of these methods call the associated method in the communicator for
   the current context. */
ICET_EXPORT IceTCommunicator icetCommDuplicate();
ICET_EXPORT void icetCommSend(const void *buf,
                              IceTSizeType count,
                              IceTEnum datatype,
                              int dest,
                              int tag);
ICET_EXPORT void icetCommRecv(void *buf,
                              IceTSizeType count,
                              IceTEnum datatype,
                              int src,
                              int tag);
ICET_EXPORT void icetCommSendrecv(const void *sendbuf,
                                  IceTSizeType sendcount,
                                  IceTEnum sendtype,
                                  int dest,
                                  int sendtag,
                                  void *recvbuf,
                                  IceTSizeType recvcount,
                                  IceTEnum recvtype,
                                  int src,
                                  int recvtag);
ICET_EXPORT void icetCommAllgather(const void *sendbuf,
                                   IceTSizeType sendcount,
                                   int type,
                                   void *recvbuf);
ICET_EXPORT IceTCommRequest icetCommIsend(const void *buf,
                                          IceTSizeType count,
                                          IceTEnum datatype,
                                          int dest,
                                          int tag);
ICET_EXPORT IceTCommRequest icetCommIrecv(void *buf,
                                          IceTSizeType count,
                                          IceTEnum datatype,
                                          int src,
                                          int tag);
ICET_EXPORT void icetCommWait(IceTCommRequest *request);
ICET_EXPORT int icetCommWaitany(int count, IceTCommRequest *array_of_requests);
ICET_EXPORT int icetCommSize();
ICET_EXPORT int icetCommRank();

#endif /*_ICET_COMMUNICATION_H_*/
