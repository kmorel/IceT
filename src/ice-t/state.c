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

#include <state.h>

#include <GL/ice-t.h>
#include <context.h>
#include <diagnostics.h>
#include <porting.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int typeWidth(GLenum type);
static void stateSet(GLenum pname, GLint size, GLenum type, const GLvoid *data);

IceTState icetStateCreate(void)
{
    IceTState state;

    state = (IceTState)malloc(sizeof(struct IceTStateValue) * ICET_STATE_SIZE);
    memset(state, 0, sizeof(struct IceTStateValue) * ICET_STATE_SIZE);

    return state;
}

void icetStateDestroy(IceTState state)
{
    int i;

    for (i = 0; i < ICET_STATE_SIZE; i++) {
	if (state[i].type != ICET_NULL) {
	    free(state[i].data);
	}
    }
    free(state);
}

void icetStateCopy(IceTState dest, const IceTState src)
{
    int i;
    int type_width;
    unsigned long mod_time;

    mod_time = get_usec();

    for (i = 0; i < ICET_STATE_SIZE; i++) {
	if (   (i == ICET_RANK) || (i == ICET_NUM_PROCESSES)
	    || (i == ICET_DATA_REPLICATION_GROUP)
	    || (i == ICET_DATA_REPLICATION_GROUP_SIZE)
	    || (i == ICET_COMPOSITE_ORDER) || (i == ICET_PROCESS_ORDERS)
	    || (i == ICET_COLOR_BUFFER) || (i == ICET_COLOR_BUFFER_VALID)
	    || (i == ICET_DEPTH_BUFFER) || (i == ICET_DEPTH_BUFFER_VALID) )
	{
	    continue;
	}

	if (dest[i].type != ICET_NULL) {
	    free(dest[i].data);
	}

	type_width = typeWidth(src[i].type);

	dest[i].type = src[i].type;
	dest[i].size = src[i].size;
	if (type_width > 0) {
	    dest[i].data = malloc(type_width * dest[i].size);
	    memcpy(dest[i].data, src[i].data, src[i].size * type_width);
	} else {
	    dest[i].data = NULL;
	}
	dest[i].mod_time = mod_time;
    }
}

static GLfloat black[] = {0.0, 0.0, 0.0, 0.0};

void icetStateSetDefaults(void)
{
    GLint *int_array;
    int i;

    icetDiagnostics(ICET_DIAG_ALL_NODES | ICET_DIAG_WARNINGS);

    icetStateSetInteger(ICET_RANK, ICET_COMM_RANK());
    icetStateSetInteger(ICET_NUM_PROCESSES, ICET_COMM_SIZE());
    icetStateSetInteger(ICET_ABSOLUTE_FAR_DEPTH, 1);
  /*icetStateSetInteger(ICET_ABSOLUTE_FAR_DEPTH, 0xFFFFFFFF);*/
    icetStateSetFloatv(ICET_BACKGROUND_COLOR, 4, black);
    icetStateSetInteger(ICET_BACKGROUND_COLOR_WORD, 0);

    icetResetTiles();
    icetStateSetIntegerv(ICET_DISPLAY_NODES, 0, NULL);

    icetStateSetDoublev(ICET_GEOMETRY_BOUNDS, 0, NULL);
    icetStateSetInteger(ICET_NUM_BOUNDING_VERTS, 0);
    icetStateSetPointer(ICET_STRATEGY_COMPOSE, NULL);
    icetInputOutputBuffers(ICET_COLOR_BUFFER_BIT | ICET_DEPTH_BUFFER_BIT,
			   ICET_COLOR_BUFFER_BIT);
    int_array = malloc(ICET_COMM_SIZE() * sizeof(GLint));
    for (i = 0; i < ICET_COMM_SIZE(); i++) {
	int_array[i] = i;
    }
    icetStateSetIntegerv(ICET_COMPOSITE_ORDER, ICET_COMM_SIZE(), int_array);
    icetStateSetIntegerv(ICET_PROCESS_ORDERS, ICET_COMM_SIZE(), int_array);
    free(int_array);

    icetStateSetInteger(ICET_DATA_REPLICATION_GROUP, ICET_COMM_RANK());
    icetStateSetInteger(ICET_DATA_REPLICATION_GROUP_SIZE, 1);

    icetStateSetPointer(ICET_DRAW_FUNCTION, NULL);
    icetStateSetInteger(ICET_READ_BUFFER, GL_BACK);
#ifdef _WIN32
    icetStateSetInteger(ICET_COLOR_FORMAT, GL_BGRA_EXT);
#else
    icetStateSetInteger(ICET_COLOR_FORMAT, GL_RGBA);
#endif
    icetStateSetInteger(ICET_FRAME_COUNT, 0);

    icetEnable(ICET_FLOATING_VIEWPORT);
    icetDisable(ICET_ORDERED_COMPOSITE);
    icetDisable(ICET_CORRECT_COLORED_BACKGROUND);
    icetEnable(ICET_DISPLAY);
    icetDisable(ICET_DISPLAY_COLORED_BACKGROUND);
    icetDisable(ICET_DISPLAY_INFLATE);

    icetStateSetBoolean(ICET_IS_DRAWING_FRAME, 0);

    icetStateSetPointer(ICET_COLOR_BUFFER, NULL);
    icetStateSetPointer(ICET_DEPTH_BUFFER, NULL);
    icetStateSetBoolean(ICET_COLOR_BUFFER_VALID, 0);
    icetStateSetBoolean(ICET_DEPTH_BUFFER_VALID, 0);

    icetStateResetTiming();
}

void icetStateSetDoublev(GLenum pname, GLint size, const GLdouble *data)
{
    stateSet(pname, size, ICET_DOUBLE, data);
}
void icetStateSetFloatv(GLenum pname, GLint size, const GLfloat *data)
{
    stateSet(pname, size, ICET_FLOAT, data);
}
void icetStateSetIntegerv(GLenum pname, GLint size, const GLint *data)
{
    stateSet(pname, size, ICET_INT, data);
}
void icetStateSetBooleanv(GLenum pname, GLint size, const GLboolean *data)
{
    stateSet(pname, size, ICET_BOOLEAN, data);
}
void icetStateSetPointerv(GLenum pname, GLint size, const GLvoid **data)
{
    stateSet(pname, size, ICET_POINTER, data);
}

void icetStateSetDouble(GLenum pname, GLdouble value)
{
    stateSet(pname, 1, ICET_DOUBLE, &value);
}
void icetStateSetFloat(GLenum pname, GLfloat value)
{
    stateSet(pname, 1, ICET_FLOAT, &value);
}
void icetStateSetInteger(GLenum pname, GLint value)
{
    stateSet(pname, 1, ICET_INT, &value);
}
void icetStateSetBoolean(GLenum pname, GLboolean value)
{
    stateSet(pname, 1, ICET_BOOLEAN, &value);
}
void icetStateSetPointer(GLenum pname, const GLvoid *value)
{
    stateSet(pname, 1, ICET_POINTER, &value);
}

GLenum icetStateGetType(GLenum pname)
{
    return icetGetState()[pname].type;
}
GLint icetStateGetSize(GLenum pname)
{
    return icetGetState()[pname].size;
}
unsigned long icetStateGetTime(GLenum pname)
{
    return icetGetState()[pname].mod_time;
}

#define copyArrayGivenCType(type_dest, array_dest, type_src, array_src, size) \
    for (i = 0; i < (size); i++) {					\
	((type_dest *)(array_dest))[i]					\
	    = (type_dest)(((type_src *)(array_src))[i]);		\
    }
#define copyArray(type_dest, array_dest, type_src, array_src, size)	       \
    switch (type_src) {							       \
      case ICET_DOUBLE:							       \
	  copyArrayGivenCType(type_dest,array_dest, GLdouble,array_src, size); \
	  break;							       \
      case ICET_FLOAT:							       \
	  copyArrayGivenCType(type_dest,array_dest, GLfloat,array_src, size);  \
	  break;							       \
      case ICET_BOOLEAN:						       \
	  copyArrayGivenCType(type_dest,array_dest, GLboolean,array_src, size);\
	  break;							       \
      case ICET_INT:							       \
	  copyArrayGivenCType(type_dest,array_dest, GLint,array_src, size);    \
	  break;							       \
      case ICET_NULL:							       \
	  {								       \
	      char msg[256];						       \
	      sprintf(msg, "No such parameter, 0x%x.", (int)pname);	       \
	      icetRaiseError(msg, ICET_INVALID_ENUM);			       \
	  }								       \
	  break;							       \
      default:								       \
	  {								       \
	      char msg[256];						       \
	      sprintf(msg, "Could not cast value for 0x%x.", (int)pname);      \
	      icetRaiseError(msg, ICET_BAD_CAST);			       \
	  }								       \
    }

void icetGetDoublev(GLenum pname, GLdouble *params)
{
    struct IceTStateValue *value = icetGetState() + pname;
    int i;
    copyArray(GLdouble, params, value->type, value->data, value->size);
}
void icetGetFloatv(GLenum pname, GLfloat *params)
{
    struct IceTStateValue *value = icetGetState() + pname;
    int i;
    copyArray(GLfloat, params, value->type, value->data, value->size);
}
void icetGetBooleanv(GLenum pname, GLboolean *params)
{
    struct IceTStateValue *value = icetGetState() + pname;
    int i;
    copyArray(GLboolean, params, value->type, value->data, value->size);
}
void icetGetIntegerv(GLenum pname, GLint *params)
{
    struct IceTStateValue *value = icetGetState() + pname;
    int i;
    copyArray(GLint, params, value->type, value->data, value->size);
}

void icetGetPointerv(GLenum pname, GLvoid **params)
{
    struct IceTStateValue *value = icetGetState() + pname;
    int i;
    if (value->type == ICET_NULL) {
	char msg[256];
	sprintf(msg, "No such parameter, 0x%x.", (int)pname);
	icetRaiseError(msg, ICET_INVALID_ENUM);
    }
    if (value->type != ICET_POINTER) {
	char msg[256];
	sprintf(msg, "Could not cast value for 0x%x.", (int)pname);
	icetRaiseError(msg, ICET_BAD_CAST);
    }
    copyArrayGivenCType(GLvoid *, params, GLvoid *, value->data, value->size);
}

void icetEnable(GLenum pname)
{
    if ((pname < ICET_STATE_ENABLE_START) || (pname >= ICET_STATE_ENABLE_END)) {
	icetRaiseError("Bad value to icetEnable", ICET_INVALID_VALUE);
	return;
    }
    icetStateSetBoolean(pname, ICET_TRUE);
}

void icetDisable(GLenum pname)
{
    if ((pname < ICET_STATE_ENABLE_START) || (pname >= ICET_STATE_ENABLE_END)) {
	icetRaiseError("Bad value to icetDisable", ICET_INVALID_VALUE);
	return;
    }
    icetStateSetBoolean(pname, ICET_FALSE);
}

GLboolean icetIsEnabled(GLenum pname)
{
    GLboolean isEnabled;

    if ((pname < ICET_STATE_ENABLE_START) || (pname >= ICET_STATE_ENABLE_END)) {
	icetRaiseError("Bad value to icetIsEnabled", ICET_INVALID_VALUE);
	return ICET_FALSE;
    }
    icetGetBooleanv(pname, &isEnabled);
    return isEnabled;
}

static int typeWidth(GLenum type)
{
    switch (type) {
      case ICET_DOUBLE:
	  return sizeof(GLdouble);
      case ICET_FLOAT:
	  return sizeof(GLfloat);
      case ICET_BOOLEAN:
	  return sizeof(GLboolean);
      case ICET_SHORT:
	  return sizeof(GLshort);
      case ICET_INT:
	  return sizeof(GLint);
      case ICET_POINTER:
	  return sizeof(GLvoid *);
      case ICET_NULL:
	  return 0;
      default:
	  icetRaiseError("Bad type detected in state.", ICET_SANITY_CHECK_FAIL);
    }

    return 0;
}

void icetUnsafeStateSet(GLenum pname, GLint size, GLenum type, GLvoid *data)
{
    IceTState state = icetGetState();

    if (state[pname].type != ICET_NULL) {
	free(state[pname].data);
    }

    state[pname].type = type;
    state[pname].size = size;
    state[pname].mod_time = get_usec();
    state[pname].data = data;
}

static void stateSet(GLenum pname, GLint size, GLenum type, const GLvoid *data)
{
    IceTState state;
    int type_width;
    void *datacopy;

    state = icetGetState();
    type_width = typeWidth(type);

    if ((size == state[pname].size) && (type == state[pname].type)) {
      /* Save time by just copying data into pre-existing memory. */
	memcpy(state[pname].data, data, size * type_width);
	state[pname].mod_time = get_usec();
    } else {
	datacopy = malloc(size * type_width);
	memcpy(datacopy, data, size * type_width);

	icetUnsafeStateSet(pname, size, type, datacopy);
    }
}

void *icetUnsafeStateGet(GLenum pname)
{
    return icetGetState()[pname].data;
}

GLenum icetStateType(GLenum pname)
{
    return icetGetState()[pname].type;
}

void icetStateResetTiming(void)
{
    icetStateSetDouble(ICET_RENDER_TIME, 0.0);
    icetStateSetDouble(ICET_BUFFER_READ_TIME, 0.0);
    icetStateSetDouble(ICET_BUFFER_WRITE_TIME, 0.0);
    icetStateSetDouble(ICET_COMPRESS_TIME, 0.0);
    icetStateSetDouble(ICET_COMPARE_TIME, 0.0);
    icetStateSetDouble(ICET_COMPOSITE_TIME, 0.0);
    icetStateSetDouble(ICET_TOTAL_DRAW_TIME, 0.0);

    icetStateSetInteger(ICET_BYTES_SENT, 0);
}

void icetStateDump(void)
{
    int i;
    IceTState state;

    state = icetGetState();
    printf("State dump:\n");
    for (i = 0; i < ICET_STATE_SIZE; i++) {
	if (state->type != ICET_NULL) {
	    printf("param = 0x%x\n", i);
	    printf("type  = 0x%x\n", (int)state->type);
	    printf("size  = %d\n", (int)state->size);
	    printf("data  = %p\n", state->data);
	    printf("mod   = %d\n", (int)state->mod_time);
	}
	state++;
    }
}
