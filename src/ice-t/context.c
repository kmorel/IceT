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

#include <context.h>
#include <GL/ice-t.h>
#include <diagnostics.h>

#include <stdlib.h>
#include <string.h>

static struct IceTContext *context_list = NULL;

static int num_contexts = 0;

static int current_context_index;

struct IceTContext *icet_current_context = NULL;

IceTContext icetCreateContext(IceTCommunicator comm)
{
    int index;

    for (index = 0; index < num_contexts; index++) {
	if (context_list[index].state == NULL) {
	    break;
	}
    }

    if (index >= num_contexts) {
	num_contexts += 4;
	context_list = realloc(context_list,
			       num_contexts*sizeof(struct IceTContext));
	memset(context_list + index, 0, 4 * sizeof(struct IceTContext));
    }

    context_list[index].communicator = comm->Duplicate(comm);

    context_list[index].buffer = NULL;
    context_list[index].buffer_size = 0;
    context_list[index].buffer_offset = 0;

    context_list[index].state = icetStateCreate();

    icetSetContext(index);
    icetStateSetDefaults();

    return index;
}

void icetDestroyContext(IceTContext context)
{
    struct IceTContext *cp = &(context_list[context]);

    if (context == current_context_index) {
	icetRaiseDebug("Destroying current context.");
    }

    icetStateDestroy(cp->state);
    cp->state = NULL;

    free(cp->buffer);
    cp->communicator->Destroy(cp->communicator);
    cp->buffer = NULL;
    cp->buffer_size = 0;
    cp->buffer_offset = 0;
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

void *icetReserveBufferMem(int size)
{
    void *mem = ((GLubyte *)icet_current_context->buffer)
	+ icet_current_context->buffer_offset;

  /* Integer boundries are good. */
    if (size%sizeof(long) != 0) {
	size += sizeof(long) - size%sizeof(long);
    }

    icet_current_context->buffer_offset += size;

    if (icet_current_context->buffer_offset > icet_current_context->buffer_size)
	icetRaiseError("Reserved more memory then allocated.",
		       ICET_OUT_OF_MEMORY);

    return mem;
}

void icetCopyState(IceTContext dest, const IceTContext src)
{
    icetStateCopy(context_list[dest].state, context_list[src].state);
}

void icetResizeBuffer(int size)
{
  /* Add some padding in case the user's data does not lie on byte boundries. */
    size += 32*sizeof(long);
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

  /* The color and depth buffers rely on this memory pool, so we have
     probably just invalidated them. */
    icetStateSetBoolean(ICET_COLOR_BUFFER_VALID, 0);
    icetStateSetBoolean(ICET_DEPTH_BUFFER_VALID, 0);
}
