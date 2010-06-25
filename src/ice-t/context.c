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

static struct IceTContextData *context_list = NULL;

static IceTInt num_contexts = 0;

static IceTInt current_context_index;

struct IceTContextData *icet_current_context = NULL;

IceTContext icetCreateContext(IceTCommunicator comm)
{
    IceTInt idx;

    for (idx = 0; idx < num_contexts; idx++) {
        if (context_list[idx].state == NULL) {
            break;
        }
    }

    if (idx >= num_contexts) {
        num_contexts += 4;
        context_list = realloc(context_list,
                               num_contexts*sizeof(struct IceTContextData));
        memset(context_list + idx, 0, 4 * sizeof(struct IceTContextData));
    }

    context_list[idx].communicator = comm->Duplicate(comm);

    context_list[idx].buffer = NULL;
    context_list[idx].buffer_size = 0;
    context_list[idx].buffer_offset = 0;

    context_list[idx].state = icetStateCreate();

    icetSetContext(idx);
    icetStateSetDefaults();

    return idx;
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
    struct IceTContextData *cp;
    IceTContext saved_current_context;

    saved_current_context = icetGetContext();
    if (context == saved_current_context) {
        icetRaiseDebug("Destroying current context.");
    }

  /* Temporarily make the context to be destroyed current. */
    icetSetContext(context);
    cp = icet_current_context;

  /* Call destructors for other dependent units. */
    callDestructor(ICET_RENDER_LAYER_DESTRUCTOR);

  /* From here on out be careful.  We are invalidating the context. */
    icetStateDestroy(cp->state);
    cp->state = NULL;

    free(cp->buffer);
    cp->communicator->Destroy(cp->communicator);
    cp->buffer = NULL;
    cp->buffer_size = 0;
    cp->buffer_offset = 0;

  /* The context is now completely destroyed and now null.  Restore saved
     context. */
    icetSetContext(saved_current_context);
}

IceTContext icetGetContext(void)
{
    return current_context_index;
}

void icetSetContext(IceTContext context)
{
    if (   (context < 0)
        || (context >= num_contexts)
        || (context_list[context].state == NULL) ) {
        icetRaiseError("No such context", ICET_INVALID_VALUE);
        return;
    }
    current_context_index = context;
    icet_current_context = &(context_list[context]);
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
    icetStateCopy(context_list[dest].state, context_list[src].state);
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
