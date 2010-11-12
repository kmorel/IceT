/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include <IceTMPI.h>

#include <IceTDevDiagnostics.h>

#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
#define BREAK_ON_MPI_ERROR
#endif

#define ICET_MPI_REQUEST_MAGIC_NUMBER ((IceTEnum)0xD7168B00)

static IceTCommunicator Duplicate(IceTCommunicator self);
static void Destroy(IceTCommunicator self);
static void Send(IceTCommunicator self,
                 const void *buf, int count, IceTEnum datatype, int dest,
                 int tag);
static void Recv(IceTCommunicator self,
                 void *buf, int count, IceTEnum datatype, int src, int tag);
static void Sendrecv(IceTCommunicator self,
                     const void *sendbuf, int sendcount, IceTEnum sendtype,
                     int dest, int sendtag,
                     void *recvbuf, int recvcount, IceTEnum recvtype,
                     int src, int recvtag);
static void Allgather(IceTCommunicator self,
                      const void *sendbuf, int sendcount, int type,
                      void *recvbuf);
static IceTCommRequest Isend(IceTCommunicator self,
                             const void *buf, int count, IceTEnum datatype,
                             int dest, int tag);
static IceTCommRequest Irecv(IceTCommunicator self,
                             void *buf, int count, IceTEnum datatype,
                             int src, int tag);
static void Waitone(IceTCommunicator self, IceTCommRequest *request);
static int  Waitany(IceTCommunicator self,
                    int count, IceTCommRequest *array_of_requests);
static int Comm_size(IceTCommunicator self);
static int Comm_rank(IceTCommunicator self);

typedef struct IceTMPICommRequestInternalsStruct {
    MPI_Request request;
} *IceTMPICommRequestInternals;

static MPI_Request getMPIRequest(IceTCommRequest icet_request)
{
    if (icet_request == ICET_COMM_REQUEST_NULL) {
        return MPI_REQUEST_NULL;
    }

    if (icet_request->magic_number != ICET_MPI_REQUEST_MAGIC_NUMBER) {
        icetRaiseError("Request object is not from the MPI communicator.",
                       ICET_INVALID_VALUE);
        return MPI_REQUEST_NULL;
    }

    return (((IceTMPICommRequestInternals)icet_request->internals)->request);
}

static void setMPIRequest(IceTCommRequest icet_request, MPI_Request mpi_request)
{
    if (icet_request == ICET_COMM_REQUEST_NULL) {
        icetRaiseError("Cannot set MPI request in null request.",
                       ICET_SANITY_CHECK_FAIL);
        return;
    }

    if (icet_request->magic_number != ICET_MPI_REQUEST_MAGIC_NUMBER) {
        icetRaiseError("Request object is not from the MPI communicator.",
                       ICET_SANITY_CHECK_FAIL);
        return;
    }

    (((IceTMPICommRequestInternals)icet_request->internals)->request)
        = mpi_request;
}

static IceTCommRequest create_request(void)
{
    IceTCommRequest request;

    request = (IceTCommRequest)malloc(sizeof(struct IceTCommRequestStruct));
    request->magic_number = ICET_MPI_REQUEST_MAGIC_NUMBER;
    request->internals=malloc(sizeof(struct IceTMPICommRequestInternalsStruct));

    setMPIRequest(request, MPI_REQUEST_NULL);

    return request;
}

static void destroy_request(IceTCommRequest request)
{
    MPI_Request mpi_request = getMPIRequest(request);
    if (mpi_request != MPI_REQUEST_NULL) {
        icetRaiseError("Destroying MPI request that is not NULL."
                       " Probably leaking MPI requests.",
                       ICET_SANITY_CHECK_FAIL);
    }

    free(request->internals);
    free(request);
}

#ifdef BREAK_ON_MPI_ERROR
static void ErrorHandler(MPI_Comm *comm, int *errorno, ...)
{
    char error_msg[MPI_MAX_ERROR_STRING+16];
    int mpi_error_len;
    (void)comm;

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
    comm->data = malloc(sizeof(MPI_Comm));
    MPI_Comm_dup(mpi_comm, (MPI_Comm *)comm->data);

#ifdef BREAK_ON_MPI_ERROR
    MPI_Errhandler_create(ErrorHandler, &eh);
    MPI_Errhandler_set(*((MPI_Comm *)comm->data), eh);
    MPI_Errhandler_free(&eh);
#endif

    return comm;
}

void icetDestroyMPICommunicator(IceTCommunicator comm)
{
    comm->Destroy(comm);
}


#define MPI_COMM        (*((MPI_Comm *)self->data))

static IceTCommunicator Duplicate(IceTCommunicator self)
{
    return icetCreateMPICommunicator(MPI_COMM);
}

static void Destroy(IceTCommunicator self)
{
    MPI_Comm_free((MPI_Comm *)self->data);
    free(self->data);
    free(self);
}

#define CONVERT_DATATYPE(icet_type, mpi_type)                                \
    switch (icet_type) {                                                     \
      case ICET_BYTE:   mpi_type = MPI_BYTE;    break;                       \
      case ICET_SHORT:  mpi_type = MPI_SHORT;   break;                       \
      case ICET_INT:    mpi_type = MPI_INT;     break;                       \
      case ICET_FLOAT:  mpi_type = MPI_FLOAT;   break;                       \
      case ICET_DOUBLE: mpi_type = MPI_DOUBLE;  break;                       \
      default:                                                               \
          icetRaiseError("MPI Communicator received bad data type.",         \
                         ICET_INVALID_ENUM);                                 \
          mpi_type = MPI_BYTE;                                               \
          break;                                                             \
    }

static void Send(IceTCommunicator self,
                 const void *buf, int count, IceTEnum datatype, int dest, int tag)
{
    MPI_Datatype mpidatatype;
    CONVERT_DATATYPE(datatype, mpidatatype);
    MPI_Send((void *)buf, count, mpidatatype, dest, tag, MPI_COMM);
}

static void Recv(IceTCommunicator self,
                 void *buf, int count, IceTEnum datatype, int src, int tag)
{
    MPI_Datatype mpidatatype;
    CONVERT_DATATYPE(datatype, mpidatatype);
    MPI_Recv(buf, count, mpidatatype, src, tag, MPI_COMM, MPI_STATUS_IGNORE);
}

static void Sendrecv(IceTCommunicator self,
                     const void *sendbuf, int sendcount, IceTEnum sendtype,
                     int dest, int sendtag,
                     void *recvbuf, int recvcount, IceTEnum recvtype,
                     int src, int recvtag)
{
    MPI_Datatype mpisendtype;
    MPI_Datatype mpirecvtype;
    CONVERT_DATATYPE(sendtype, mpisendtype);
    CONVERT_DATATYPE(recvtype, mpirecvtype);

    MPI_Sendrecv((void *)sendbuf, sendcount, mpisendtype, dest, sendtag,
                 recvbuf, recvcount, mpirecvtype, src, recvtag, MPI_COMM,
                 MPI_STATUS_IGNORE);
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
                             const void *buf, int count, IceTEnum datatype,
                             int dest, int tag)
{
    IceTCommRequest icet_request;
    MPI_Request mpi_request;
    MPI_Datatype mpidatatype;

    CONVERT_DATATYPE(datatype, mpidatatype);
    MPI_Isend((void *)buf, count, mpidatatype, dest, tag, MPI_COMM,
              &mpi_request);

    icet_request = create_request();
    setMPIRequest(icet_request, mpi_request);

    return icet_request;
}

static IceTCommRequest Irecv(IceTCommunicator self,
                             void *buf, int count, IceTEnum datatype,
                             int src, int tag)
{
    IceTCommRequest icet_request;
    MPI_Request mpi_request;
    MPI_Datatype mpidatatype;

    CONVERT_DATATYPE(datatype, mpidatatype);
    MPI_Irecv(buf, count, mpidatatype, src, tag, MPI_COMM,
              &mpi_request);

    icet_request = create_request();
    setMPIRequest(icet_request, mpi_request);

    return icet_request;
}

static void Waitone(IceTCommunicator self , IceTCommRequest *icet_request)
{
    MPI_Request mpi_request;

    /* To remove warning */
    (void)self;

    if (*icet_request == ICET_COMM_REQUEST_NULL) return;

    mpi_request = getMPIRequest(*icet_request);
    MPI_Wait(&mpi_request, MPI_STATUS_IGNORE);
    setMPIRequest(*icet_request, mpi_request);

    destroy_request(*icet_request);
    *icet_request = ICET_COMM_REQUEST_NULL;
}

static int  Waitany(IceTCommunicator  self,
                    int count, IceTCommRequest *array_of_requests)
{
    MPI_Request *mpi_requests;
    int idx;

    /* To remove warning */
    (void)self;

    mpi_requests = malloc(sizeof(MPI_Request)*count);
    for (idx = 0; idx < count; idx++) {
        mpi_requests[idx] = getMPIRequest(array_of_requests[idx]);
    }

    MPI_Waitany(count, mpi_requests, &idx, MPI_STATUS_IGNORE);

    setMPIRequest(array_of_requests[idx], mpi_requests[idx]);
    destroy_request(array_of_requests[idx]);
    array_of_requests[idx] = ICET_COMM_REQUEST_NULL;

    free(mpi_requests);

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
