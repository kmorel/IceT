/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include <IceTDevState.h>

#include <IceT.h>
#include <IceTDevCommunication.h>
#include <IceTDevContext.h>
#include <IceTDevDiagnostics.h>
#include <IceTDevPorting.h>
#include <IceTDevStrategySelect.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct IceTStateValue {
    IceTEnum type;
    IceTSizeType num_entries;
    void *data;
    IceTTimeStamp mod_time;
};

static IceTVoid *stateAllocate(IceTEnum pname,
                               IceTSizeType num_entries,
                               IceTEnum type);
static void stateSet(IceTEnum pname, IceTSizeType num_entries, IceTEnum type,
                     const IceTVoid *data);

IceTState icetStateCreate(void)
{
    IceTState state;

    state = (IceTState)malloc(sizeof(struct IceTStateValue) * ICET_STATE_SIZE);
    memset(state, 0, sizeof(struct IceTStateValue) * ICET_STATE_SIZE);

    return state;
}

void icetStateDestroy(IceTState state)
{
    IceTEnum i;

    for (i = 0; i < ICET_STATE_SIZE; i++) {
        if (state[i].type != ICET_NULL) {
            free(state[i].data);
        }
    }
    free(state);
}

void icetStateCopy(IceTState dest, const IceTState src)
{
    IceTEnum i;
    IceTSizeType type_width;
    IceTTimeStamp mod_time;

    mod_time = icetGetTimeStamp();

    for (i = 0; i < ICET_STATE_SIZE; i++) {
        if (   (i == ICET_RANK) || (i == ICET_NUM_PROCESSES)
            || (i == ICET_DATA_REPLICATION_GROUP)
            || (i == ICET_DATA_REPLICATION_GROUP_SIZE)
            || (i == ICET_COMPOSITE_ORDER) || (i == ICET_PROCESS_ORDERS) )
        {
            continue;
        }

        if (dest[i].type != ICET_NULL) {
            free(dest[i].data);
        }

        type_width = icetTypeWidth(src[i].type);

        dest[i].type = src[i].type;
        dest[i].num_entries = src[i].num_entries;
        if (type_width > 0) {
            dest[i].data = malloc(type_width * dest[i].num_entries);
            memcpy(dest[i].data, src[i].data, src[i].num_entries * type_width);
        } else {
            dest[i].data = NULL;
        }
        dest[i].mod_time = mod_time;
    }
}

static IceTFloat black[] = {0.0f, 0.0f, 0.0f, 0.0f};

void icetStateSetDefaults(void)
{
    IceTInt *int_array;
    int i;
    int comm_size, comm_rank;

    icetDiagnostics(ICET_DIAG_ALL_NODES | ICET_DIAG_WARNINGS);

    comm_size = icetCommSize();
    comm_rank = icetCommRank();
    icetStateSetInteger(ICET_RANK, comm_rank);
    icetStateSetInteger(ICET_NUM_PROCESSES, comm_size);
    /* icetStateSetInteger(ICET_ABSOLUTE_FAR_DEPTH, 1); */
  /*icetStateSetInteger(ICET_ABSOLUTE_FAR_DEPTH, 0xFFFFFFFF);*/
    icetStateSetFloatv(ICET_BACKGROUND_COLOR, 4, black);
    icetStateSetInteger(ICET_BACKGROUND_COLOR_WORD, 0);
    icetStateSetInteger(ICET_COLOR_FORMAT, ICET_IMAGE_COLOR_RGBA_UBYTE);
    icetStateSetInteger(ICET_DEPTH_FORMAT, ICET_IMAGE_DEPTH_FLOAT);

    icetResetTiles();
    icetStateSetIntegerv(ICET_DISPLAY_NODES, 0, NULL);

    icetStateSetDoublev(ICET_GEOMETRY_BOUNDS, 0, NULL);
    icetStateSetInteger(ICET_NUM_BOUNDING_VERTS, 0);
    icetStateSetInteger(ICET_STRATEGY, ICET_STRATEGY_UNDEFINED);
    icetSingleImageStrategy(ICET_SINGLE_IMAGE_STRATEGY_AUTOMATIC);
    icetCompositeMode(ICET_COMPOSITE_MODE_Z_BUFFER);
    int_array = malloc(comm_size * sizeof(IceTInt));
    for (i = 0; i < comm_size; i++) {
        int_array[i] = i;
    }
    icetStateSetIntegerv(ICET_COMPOSITE_ORDER, comm_size, int_array);
    icetStateSetIntegerv(ICET_PROCESS_ORDERS, comm_size, int_array);
    free(int_array);

    icetStateSetInteger(ICET_DATA_REPLICATION_GROUP, comm_rank);
    icetStateSetInteger(ICET_DATA_REPLICATION_GROUP_SIZE, 1);
    icetStateSetInteger(ICET_FRAME_COUNT, 0);

    icetStateSetPointer(ICET_DRAW_FUNCTION, NULL);
    icetStateSetPointer(ICET_RENDER_LAYER_DESTRUCTOR, NULL);

    icetEnable(ICET_FLOATING_VIEWPORT);
    icetDisable(ICET_ORDERED_COMPOSITE);
    icetDisable(ICET_CORRECT_COLORED_BACKGROUND);
    icetEnable(ICET_COMPOSITE_ONE_BUFFER);

    icetStateSetBoolean(ICET_IS_DRAWING_FRAME, 0);
    icetStateSetBoolean(ICET_RENDER_BUFFER_SIZE, 0);

    icetStateResetTiming();
}

void icetStateSetDoublev(IceTEnum pname,
                         IceTSizeType num_entries,
                         const IceTDouble *data)
{
    stateSet(pname, num_entries, ICET_DOUBLE, data);
}
void icetStateSetFloatv(IceTEnum pname,
                        IceTSizeType num_entries,
                        const IceTFloat *data)
{
    stateSet(pname, num_entries, ICET_FLOAT, data);
}
void icetStateSetIntegerv(IceTEnum pname,
                          IceTSizeType num_entries,
                          const IceTInt *data)
{
    stateSet(pname, num_entries, ICET_INT, data);
}
void icetStateSetBooleanv(IceTEnum pname,
                          IceTSizeType num_entries,
                          const IceTBoolean *data)
{
    stateSet(pname, num_entries, ICET_BOOLEAN, data);
}
void icetStateSetPointerv(IceTEnum pname,
                          IceTSizeType num_entries,
                          const IceTVoid **data)
{
    stateSet(pname, num_entries, ICET_POINTER, data);
}

void icetStateSetDouble(IceTEnum pname, IceTDouble value)
{
    stateSet(pname, 1, ICET_DOUBLE, &value);
}
void icetStateSetFloat(IceTEnum pname, IceTFloat value)
{
    stateSet(pname, 1, ICET_FLOAT, &value);
}
void icetStateSetInteger(IceTEnum pname, IceTInt value)
{
    stateSet(pname, 1, ICET_INT, &value);
}
void icetStateSetBoolean(IceTEnum pname, IceTBoolean value)
{
    stateSet(pname, 1, ICET_BOOLEAN, &value);
}
void icetStateSetPointer(IceTEnum pname, const IceTVoid *value)
{
    stateSet(pname, 1, ICET_POINTER, &value);
}

IceTEnum icetStateGetType(IceTEnum pname)
{
    return icetGetState()[pname].type;
}
IceTSizeType icetStateGetNumEntries(IceTEnum pname)
{
    return icetGetState()[pname].num_entries;
}
IceTTimeStamp icetStateGetTime(IceTEnum pname)
{
    return icetGetState()[pname].mod_time;
}

#define copyArrayGivenCType(type_dest, array_dest, type_src, array_src, size) \
    for (i = 0; i < (size); i++) {                                      \
        ((type_dest *)(array_dest))[i]                                  \
            = (type_dest)(((type_src *)(array_src))[i]);                \
    }
#define copyArray(type_dest, array_dest, type_src, array_src, size)            \
    switch (type_src) {                                                        \
      case ICET_DOUBLE:                                                        \
          copyArrayGivenCType(type_dest,array_dest, IceTDouble,array_src, size); \
          break;                                                               \
      case ICET_FLOAT:                                                         \
          copyArrayGivenCType(type_dest,array_dest, IceTFloat,array_src, size);  \
          break;                                                               \
      case ICET_BOOLEAN:                                                       \
          copyArrayGivenCType(type_dest,array_dest, IceTBoolean,array_src, size);\
          break;                                                               \
      case ICET_INT:                                                           \
          copyArrayGivenCType(type_dest,array_dest, IceTInt,array_src, size);    \
          break;                                                               \
      case ICET_NULL:                                                          \
          {                                                                    \
              char msg[256];                                                   \
              sprintf(msg, "No such parameter, 0x%x.", (int)pname);            \
              icetRaiseError(msg, ICET_INVALID_ENUM);                          \
          }                                                                    \
          break;                                                               \
      default:                                                                 \
          {                                                                    \
              char msg[256];                                                   \
              sprintf(msg, "Could not cast value for 0x%x.", (int)pname);      \
              icetRaiseError(msg, ICET_BAD_CAST);                              \
          }                                                                    \
    }

void icetGetDoublev(IceTEnum pname, IceTDouble *params)
{
    struct IceTStateValue *value = icetGetState() + pname;
    int i;
    copyArray(IceTDouble, params, value->type, value->data, value->num_entries);
}
void icetGetFloatv(IceTEnum pname, IceTFloat *params)
{
    struct IceTStateValue *value = icetGetState() + pname;
    int i;
    copyArray(IceTFloat, params, value->type, value->data, value->num_entries);
}
void icetGetBooleanv(IceTEnum pname, IceTBoolean *params)
{
    struct IceTStateValue *value = icetGetState() + pname;
    int i;
    copyArray(IceTBoolean, params, value->type, value->data,value->num_entries);
}
void icetGetIntegerv(IceTEnum pname, IceTInt *params)
{
    struct IceTStateValue *value = icetGetState() + pname;
    int i;
    copyArray(IceTInt, params, value->type, value->data, value->num_entries);
}

void icetGetEnumv(IceTEnum pname, IceTEnum *params)
{
    struct IceTStateValue *value = icetGetState() + pname;
    int i;
    if ((value->type == ICET_FLOAT) || (value->type == ICET_DOUBLE)) {
        icetRaiseError("Floating point values cannot be enumerations.",
                       ICET_BAD_CAST);
    }
    copyArray(IceTEnum, params, value->type, value->data, value->num_entries);
}
void icetGetBitFieldv(IceTEnum pname, IceTBitField *params)
{
    struct IceTStateValue *value = icetGetState() + pname;
    int i;
    if ((value->type == ICET_FLOAT) || (value->type == ICET_DOUBLE)) {
        icetRaiseError("Floating point values cannot be enumerations.",
                       ICET_BAD_CAST);
    }
    copyArray(IceTBitField, params, value->type,
              value->data, value->num_entries);
}

void icetGetPointerv(IceTEnum pname, IceTVoid **params)
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
    copyArrayGivenCType(IceTVoid *, params, IceTVoid *,
                        value->data, value->num_entries);
}

void icetEnable(IceTEnum pname)
{
    if ((pname < ICET_STATE_ENABLE_START) || (pname >= ICET_STATE_ENABLE_END)) {
        icetRaiseError("Bad value to icetEnable", ICET_INVALID_VALUE);
        return;
    }
    icetStateSetBoolean(pname, ICET_TRUE);
}

void icetDisable(IceTEnum pname)
{
    if ((pname < ICET_STATE_ENABLE_START) || (pname >= ICET_STATE_ENABLE_END)) {
        icetRaiseError("Bad value to icetDisable", ICET_INVALID_VALUE);
        return;
    }
    icetStateSetBoolean(pname, ICET_FALSE);
}

IceTBoolean icetIsEnabled(IceTEnum pname)
{
    IceTBoolean isEnabled;

    if ((pname < ICET_STATE_ENABLE_START) || (pname >= ICET_STATE_ENABLE_END)) {
        icetRaiseError("Bad value to icetIsEnabled", ICET_INVALID_VALUE);
        return ICET_FALSE;
    }
    icetGetBooleanv(pname, &isEnabled);
    return isEnabled;
}

void icetUnsafeStateSet(IceTEnum pname,
                        IceTSizeType num_entries,
                        IceTEnum type,
                        IceTVoid *data)
{
    IceTState state = icetGetState();

    if (state[pname].type != ICET_NULL) {
        free(state[pname].data);
    }

    state[pname].type = type;
    state[pname].num_entries = num_entries;
    state[pname].mod_time = icetGetTimeStamp();
    state[pname].data = data;
}

static IceTVoid *stateAllocate(IceTEnum pname,
                               IceTSizeType num_entries,
                               IceTEnum type)
{
    IceTState state = icetGetState();

    if (   (num_entries == state[pname].num_entries)
        && (type == state[pname].type) ) {
      /* Return the current buffer. */
        state[pname].mod_time = icetGetTimeStamp();
        return state[pname].data;
    } else {
      /* Create a new buffer. */
        IceTVoid *buffer = malloc(num_entries * icetTypeWidth(type));
        icetUnsafeStateSet(pname, num_entries, type, buffer);
        return buffer;
    }
}

static void stateSet(IceTEnum pname,
                     IceTSizeType num_entries,
                     IceTEnum type,
                     const IceTVoid *data)
{
    IceTSizeType type_width = icetTypeWidth(type);
    void *datacopy = stateAllocate(pname, num_entries, type);

    memcpy(datacopy, data, num_entries * type_width);
}

static void *icetUnsafeStateGet(IceTEnum pname, IceTEnum type)
{
    if (icetGetState()[pname].type != type) {
	icetRaiseError("Mismatched types in unsafe state get.",
		       ICET_SANITY_CHECK_FAIL);
	return NULL;
    }
    return icetGetState()[pname].data;
}

IceTDouble *icetUnsafeStateGetDouble(IceTEnum pname)
{
    return icetUnsafeStateGet(pname, ICET_DOUBLE);
}
IceTFloat *icetUnsafeStateGetFloat(IceTEnum pname)
{
    return icetUnsafeStateGet(pname, ICET_FLOAT);
}
IceTInt *icetUnsafeStateGetInteger(IceTEnum pname)
{
    return icetUnsafeStateGet(pname, ICET_INT);
}
IceTBoolean *icetUnsafeStateGetBoolean(IceTEnum pname)
{
    return icetUnsafeStateGet(pname, ICET_BOOLEAN);
}
IceTVoid **icetUnsafeStateGetPointer(IceTEnum pname)
{
    return icetUnsafeStateGet(pname, ICET_POINTER);
}

IceTEnum icetStateType(IceTEnum pname)
{
    return icetGetState()[pname].type;
}

IceTDouble *icetStateAllocateDouble(IceTEnum pname, IceTSizeType num_entries)
{
    return stateAllocate(pname, num_entries, ICET_DOUBLE);
}
IceTFloat *icetStateAllocateFloat(IceTEnum pname, IceTSizeType num_entries)
{
    return stateAllocate(pname, num_entries, ICET_FLOAT);
}
IceTInt *icetStateAllocateInteger(IceTEnum pname, IceTSizeType num_entries)
{
    return stateAllocate(pname, num_entries, ICET_INT);
}
IceTBoolean *icetStateAllocateBoolean(IceTEnum pname, IceTSizeType num_entries)
{
    return stateAllocate(pname, num_entries, ICET_BOOLEAN);
}
IceTVoid **icetStateAllocatePointer(IceTEnum pname, IceTSizeType num_entries)
{
    return stateAllocate(pname, num_entries, ICET_POINTER);
}

IceTVoid *icetGetStateBuffer(IceTEnum pname, IceTSizeType num_bytes)
{
    if (   (icetStateGetType(pname) == ICET_VOID)
        && (icetStateGetNumEntries(pname) >= num_bytes) ) {
      /* A big enough buffer is already allocated. */
        return icetUnsafeStateGet(pname, ICET_VOID);
    }

  /* Check to make sure this state variable has not been used for anything
   * besides a buffer. */
    if (   (icetStateGetType(pname) != ICET_VOID)
        && (icetStateGetType(pname) != ICET_NULL) ) {
        icetRaiseWarning("A non-buffer state variable is being reallocated as"
                         " a state variable.  This is probably indicative of"
                         " mixing up state variables.",
                         ICET_SANITY_CHECK_FAIL);
    }

    {
        IceTVoid *buffer = malloc(num_bytes);
        icetUnsafeStateSet(pname, num_bytes, ICET_VOID, buffer);
        return buffer;
    }
}

IceTTimeStamp icetGetTimeStamp(void)
{
    static IceTTimeStamp current_time = 0;

    return current_time++;
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
    IceTEnum i;
    IceTState state;

    state = icetGetState();
    printf("State dump:\n");
    for (i = 0; i < ICET_STATE_SIZE; i++) {
        if (state->type != ICET_NULL) {
            printf("param       = 0x%x\n", i);
            printf("type        = 0x%x\n", (int)state->type);
            printf("num_entries = %d\n", (int)state->num_entries);
            printf("data        = %p\n", state->data);
            printf("mod         = %d\n", (int)state->mod_time);
        }
        state++;
    }
}
