/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#include <IceTDevContext.h>

#include <IceT.h>

#include <IceTDevDiagnostics.h>
#include <IceTDevImage.h>

#include <stdlib.h>
#include <string.h>

#define CONTEXT_MAGIC_NUMBER ((IceTEnum)0x12358D15)

struct IceTContextStruct {
    IceTEnum magic_number;
    IceTState state;
    IceTCommunicator communicator;
    IceTStrategy strategy;
    IceTVoid *buffer;
    IceTSizeType buffer_size;
    IceTSizeType buffer_offset;
};

static IceTContext icet_current_context = NULL;

IceTContext icetCreateContext(IceTCommunicator comm)
{
    IceTContext context = malloc(sizeof(struct IceTContextStruct));

    context->magic_number = CONTEXT_MAGIC_NUMBER;

    context->communicator = comm->Duplicate(comm);

    context->buffer = NULL;
    context->buffer_size = 0;
    context->buffer_offset = 0;

    context->state = icetStateCreate();

    icetSetContext(context);
    icetStateSetDefaults();

    return context;
}

static void callDestructor(IceTEnum dtor_variable)
{
    IceTVoid *void_dtor_pointer;
    void (*dtor_function)(void);

    icetGetPointerv(dtor_variable, &void_dtor_pointer);
    dtor_function = (void (*)(void))void_dtor_pointer;

    if (dtor_function) {
        (*dtor_function)();
    }
}

void icetDestroyContext(IceTContext context)
{
    IceTContext saved_current_context;

    saved_current_context = icetGetContext();
    if (context == saved_current_context) {
        icetRaiseDebug("Destroying current context.");
        saved_current_context = NULL;
    }

  /* Temporarily make the context to be destroyed current. */
    icetSetContext(context);

  /* Call destructors for other dependent units. */
    callDestructor(ICET_RENDER_LAYER_DESTRUCTOR);

  /* From here on out be careful.  We are invalidating the context. */
    context->magic_number = 0;

    icetStateDestroy(context->state);
    context->state = NULL;

    free(context->buffer);
    context->communicator->Destroy(context->communicator);
    context->buffer = NULL;
    context->buffer_size = 0;
    context->buffer_offset = 0;

  /* The context is now completely destroyed and now null.  Restore saved
     context. */
    icetSetContext(saved_current_context);
}

IceTContext icetGetContext(void)
{
    return icet_current_context;
}

void icetSetContext(IceTContext context)
{
    if (context && (context->magic_number != CONTEXT_MAGIC_NUMBER)) {
        icetRaiseError("Invalid context.", ICET_INVALID_VALUE);
        return;
    }
    icet_current_context = context;
}

IceTState icetGetState()
{
    return icet_current_context->state;
}

IceTCommunicator icetGetCommunicator()
{
    return icet_current_context->communicator;
}

IceTVoid *icetReserveBufferMem(IceTSizeType size)
{
    IceTVoid *mem = ((IceTUByte *)icet_current_context->buffer)
        + icet_current_context->buffer_offset;

  /* Integer boundries are good. */
    if (size%sizeof(IceTInt64) != 0) {
        size += sizeof(IceTInt64) - size%sizeof(IceTInt64);
    }

    icet_current_context->buffer_offset += size;

    if (icet_current_context->buffer_offset > icet_current_context->buffer_size)
        icetRaiseError("Reserved more memory then allocated.",
                       ICET_OUT_OF_MEMORY);

    return mem;
}

IceTImage icetReserveBufferImage(IceTSizeType width, IceTSizeType height)
{
    IceTVoid *buffer;
    IceTSizeType buffer_size;

    buffer_size = icetImageBufferSize(width, height);
    buffer = icetReserveBufferMem(buffer_size);

    return icetImageAssignBuffer(buffer, width, height);
}

IceTSparseImage icetReserveBufferSparseImage(IceTSizeType width,
                                             IceTSizeType height)
{
    IceTVoid *buffer;
    IceTSizeType buffer_size;

    buffer_size = icetSparseImageBufferSize(width, height);
    buffer = icetReserveBufferMem(buffer_size);

    return icetSparseImageAssignBuffer(buffer, width, height);
}

void icetCopyState(IceTContext dest, const IceTContext src)
{
    icetStateCopy(dest->state, src->state);
}

void icetResizeBuffer(IceTSizeType size)
{
    icetRaiseDebug1("Resizing buffer to %d bytes.", (IceTInt)size);

  /* Add some padding in case the user's data does not lie on byte boundries. */
    size += 32*sizeof(IceTInt64);
    if (icet_current_context->buffer_size < size) {
        free(icet_current_context->buffer);
        icet_current_context->buffer = malloc(size);
        if (icet_current_context->buffer == NULL) {
            icetRaiseError("Could not allocate more buffer space",
                           ICET_OUT_OF_MEMORY);
          /* Try to back out of change. */
            icet_current_context->buffer
                = malloc(icet_current_context->buffer_size);
            if (icet_current_context->buffer == NULL) {
                icetRaiseError("Could not back out of memory change",
                               ICET_OUT_OF_MEMORY);
                icet_current_context->buffer_size = 0;
            }
        } else {
            icet_current_context->buffer_size = size;
        }
    }

    icet_current_context->buffer_offset = 0;
}
