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

#define USE_STDARG
#include <GL/ice-t_mpi.h>

#include <diagnostics.h>

#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
#define BREAK_ON_MPI_ERROR
#endif

static IceTCommunicator Duplicate(IceTCommunicator self);
static void Destroy(IceTCommunicator self);
static void Send(IceTCommunicator self,
                 const void *buf, int count, GLenum datatype, int dest,
                 int tag);
static void Recv(IceTCommunicator self,
                 void *buf, int count, GLenum datatype, int src, int tag);
static void Sendrecv(IceTCommunicator self,
                     const void *sendbuf, int sendcount, GLenum sendtype,
                     int dest, int sendtag,
                     void *recvbuf, int recvcount, GLenum recvtype,
                     int src, int recvtag);
static void Allgather(IceTCommunicator self,
                      const void *sendbuf, int sendcount, int type,
                      void *recvbuf);
static IceTCommRequest Isend(IceTCommunicator self,
                             const void *buf, int count, GLenum datatype,
                             int dest, int tag);
static IceTCommRequest Irecv(IceTCommunicator self,
                             void *buf, int count, GLenum datatype,
                             int src, int tag);
static void Waitone(IceTCommunicator self, IceTCommRequest *request);
static int  Waitany(IceTCommunicator self,
                    int count, IceTCommRequest *array_of_requests);
static int Comm_size(IceTCommunicator self);
static int Comm_rank(IceTCommunicator self);

struct IceTMPICommRequestStruct {
    MPI_Request request;
};

#define REQUEST_POOL        1

#if REQUEST_POOL
/* Note that the use of this array is not thread safe. */
static struct IceTMPICommRequestStruct *request_pool = NULL;
static int request_pool_count = 0;
#endif

static IceTCommRequest create_request(void)
{
#if REQUEST_POOL
    IceTCommRequest i;
    for (i = 0; i < request_pool_count; i++) {
        if (request_pool[i].request == MPI_REQUEST_NULL) break;
    }
    if (i == request_pool_count) {
        request_pool_count += 4;
        request_pool =
            realloc(request_pool,
                    request_pool_count*sizeof(struct IceTMPICommRequestStruct));
        request_pool[i+1].request = MPI_REQUEST_NULL;
        request_pool[i+2].request = MPI_REQUEST_NULL;
        request_pool[i+3].request = MPI_REQUEST_NULL;
    }
    return i;
#else
    return (IceTCommRequest)malloc(sizeof(struct IceTMPICommRequestStruct));
#endif
}
static void destroy_request(IceTCommRequest req)
{
#if REQUEST_POOL
    request_pool[req].request = MPI_REQUEST_NULL;
#else
    free((void *)req);
#endif
}

#if REQUEST_POOL
#define ICETREQ2MPIREQP(req)        (&request_pool[req].request)
#else
#define ICETREQ2MPIREQP(req)        (&((struct IceTMPICommRequestStruct *)req)->request)
#endif
#define ICETREQ2MPIREQ(req)                                                \
    ((req) == ICET_COMM_REQUEST_NULL ? MPI_REQUEST_NULL : *ICETREQ2MPIREQP(req))

#ifdef BREAK_ON_MPI_ERROR
static void ErrorHandler(MPI_Comm *icetNotUsed(comm), int *errorno, ...)
{
    char error_msg[MPI_MAX_ERROR_STRING+16];
    int mpi_error_len;

    strcpy(error_msg, "MPI ERROR:\n");
    MPI_Error_string(*errorno, error_msg + strlen(error_msg), &mpi_error_len);

    icetRaiseError(error_msg, ICET_INVALID_OPERATION);
    icetDebugBreak();
}
#endif

IceTCommunicator icetCreateMPICommunicator(MPI_Comm mpi_comm)
{
    IceTCommunicator comm = malloc(sizeof(struct IceTCommunicatorStruct));
#ifdef BREAK_ON_MPI_ERROR
    MPI_Errhandler eh;
#endif

    comm->Duplicate = Duplicate;
    comm->Destroy = Destroy;
    comm->Send = Send;
    comm->Recv = Recv;
    comm->Sendrecv = Sendrecv;
    comm->Allgather = Allgather;
    comm->Isend = Isend;
    comm->Irecv = Irecv;
    comm->Wait = Waitone;
    comm->Waitany = Waitany;
    comm->Comm_size = Comm_size;
    comm->Comm_rank = Comm_rank;
    MPI_Comm_dup(mpi_comm, (MPI_Comm *)&comm->data);

#ifdef BREAK_ON_MPI_ERROR
    MPI_Errhandler_create(ErrorHandler, &eh);
    MPI_Errhandler_set((MPI_Comm)comm->data, eh);
    MPI_Errhandler_free(&eh);
#endif

    return comm;
}

void icetDestroyMPICommunicator(IceTCommunicator comm)
{
    comm->Destroy(comm);
}


#define MPI_COMM        ((MPI_Comm)self->data)

static IceTCommunicator Duplicate(IceTCommunicator self)
{
    return icetCreateMPICommunicator(MPI_COMM);
}

static void Destroy(IceTCommunicator self)
{
    MPI_Comm_free((MPI_Comm *)&self->data);
    free(self);
}

#define CONVERT_DATATYPE(icet_type, mpi_type)                                \
    switch (icet_type) {                                                \
      case ICET_BYTE:        mpi_type = MPI_BYTE;        break;                        \
      case ICET_SHORT:        mpi_type = MPI_SHORT;        break;                        \
      case ICET_INT:        mpi_type = MPI_INT;        break;                        \
      case ICET_FLOAT:        mpi_type = MPI_FLOAT;        break;                        \
      case ICET_DOUBLE:        mpi_type = MPI_DOUBLE;        break;                        \
      default:                                                                \
          icetRaiseError("MPI Communicator received bad data type.",        \
                         ICET_INVALID_ENUM);                                \
          mpi_type = MPI_BYTE;                                                \
          break;                                                        \
    }

static void Send(IceTCommunicator self,
                 const void *buf, int count, GLenum datatype, int dest, int tag)
{
    MPI_Datatype mpidatatype;
    CONVERT_DATATYPE(datatype, mpidatatype);
    MPI_Send((void *)buf, count, mpidatatype, dest, tag, MPI_COMM);
}

static void Recv(IceTCommunicator self,
                 void *buf, int count, GLenum datatype, int src, int tag)
{
    MPI_Status status;
    MPI_Datatype mpidatatype;
    CONVERT_DATATYPE(datatype, mpidatatype);
    MPI_Recv(buf, count, mpidatatype, src, tag, MPI_COMM, &status);
}

static void Sendrecv(IceTCommunicator self,
                     const void *sendbuf, int sendcount, GLenum sendtype,
                     int dest, int sendtag,
                     void *recvbuf, int recvcount, GLenum recvtype,
                     int src, int recvtag)
{
    MPI_Status status;
    MPI_Datatype mpisendtype;
    MPI_Datatype mpirecvtype;
    CONVERT_DATATYPE(sendtype, mpisendtype);
    CONVERT_DATATYPE(recvtype, mpirecvtype);

    MPI_Sendrecv((void *)sendbuf, sendcount, mpisendtype, dest, sendtag,
                 recvbuf, recvcount, mpirecvtype, src, recvtag, MPI_COMM, &status);
}

static void Allgather(IceTCommunicator self,
                      const void *sendbuf, int sendcount, int type,
                      void *recvbuf)
{
    MPI_Datatype mpitype;
    CONVERT_DATATYPE(type, mpitype);

    MPI_Allgather((void *)sendbuf, sendcount, mpitype,
                  recvbuf, sendcount, mpitype,
                  MPI_COMM);
}

static IceTCommRequest Isend(IceTCommunicator self,
                             const void *buf, int count, GLenum datatype,
                             int dest, int tag)
{
    IceTCommRequest icet_request;
    MPI_Datatype mpidatatype;

    icet_request = create_request();
    CONVERT_DATATYPE(datatype, mpidatatype);
    MPI_Isend((void *)buf, count, mpidatatype, dest, tag, MPI_COMM,
              ICETREQ2MPIREQP(icet_request));

    return icet_request;
}

static IceTCommRequest Irecv(IceTCommunicator self,
                             void *buf, int count, GLenum datatype,
                             int src, int tag)
{
    IceTCommRequest icet_request;
    MPI_Datatype mpidatatype;

    icet_request = create_request();
    CONVERT_DATATYPE(datatype, mpidatatype);
    MPI_Irecv(buf, count, mpidatatype, src, tag, MPI_COMM,
              ICETREQ2MPIREQP(icet_request));

    return icet_request;
}

static void Waitone(IceTCommunicator self , IceTCommRequest *request)
{
    MPI_Status status;

    /* To remove warning */
    (void)self;

    if (*request == ICET_COMM_REQUEST_NULL) return;

    MPI_Wait(ICETREQ2MPIREQP(*request), &status);
    destroy_request(*request);
    *request = ICET_COMM_REQUEST_NULL;
}

static int  Waitany(IceTCommunicator  self,
                    int count, IceTCommRequest *array_of_requests)
{
    MPI_Status status;
    MPI_Request *requests;
    int idx;

    /* To remove warning */
    (void)self;
    requests = malloc(sizeof(MPI_Request)*count);
    for (idx = 0; idx < count; idx++) {
        requests[idx] = ICETREQ2MPIREQ(array_of_requests[idx]);
    }

    MPI_Waitany(count, requests, &idx, &status);
    destroy_request(array_of_requests[idx]);
    array_of_requests[idx] = ICET_COMM_REQUEST_NULL;
    free(requests);

    return idx;
}

static int Comm_size(IceTCommunicator self)
{
    int size;
    MPI_Comm_size(MPI_COMM, &size);
    return size;
}

static int Comm_rank(IceTCommunicator self)
{
    int rank;
    MPI_Comm_rank(MPI_COMM, &rank);
    return rank;
}
