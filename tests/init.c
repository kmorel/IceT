/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include "test-util.h"
#include "test_codes.h"

#include <IceTGL.h>

#define __USE_POSIX

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef __APPLE__
#include <GL/glut.h>
#include <GL/gl.h>
#else
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#define dup(fildes)             _dup(fildes)
#define dup2(fildes, fildes2)   _dup2(fildes, fildes2)
#endif

IceTEnum strategy_list[5];
int STRATEGY_LIST_SIZE = 5;
/* int STRATEGY_LIST_SIZE = 1; */

IceTEnum single_image_strategy_list[3];
int SINGLE_IMAGE_STRATEGY_LIST_SIZE = 3;
/* int SINGLE_IMAGE_STRATEGY_LIST_SIZE = 1; */

IceTSizeType SCREEN_WIDTH;
IceTSizeType SCREEN_HEIGHT;

static int windowId;

static int (*test_function)(void);

static void checkOglError(void)
{
    GLenum error = glGetError();
#define TRY_ERROR(ename)                                                \
    if (error == ename) {                                               \
        printf("## Current OpenGL error = " #ename "\n");               \
        return;                                                         \
    }
    
    TRY_ERROR(GL_NO_ERROR);
    TRY_ERROR(GL_INVALID_ENUM);
    TRY_ERROR(GL_INVALID_VALUE);
    TRY_ERROR(GL_INVALID_OPERATION);
    TRY_ERROR(GL_STACK_OVERFLOW);
    TRY_ERROR(GL_STACK_UNDERFLOW);
    TRY_ERROR(GL_OUT_OF_MEMORY);
#ifdef GL_TABLE_TOO_LARGE
    TRY_ERROR(GL_TABLE_TOO_LARGE);
#endif
    printf("## UNKNOWN OPENGL ERROR CODE!!!!!!\n");
#undef TRY_ERROR
}

static void checkIceTError(void)
{
    IceTEnum error = icetGetError();
#define TRY_ERROR(ename)                                                \
    if (error == ename) {                                               \
        printf("## Current IceT error = " #ename "\n");                \
        return;                                                         \
    }
    TRY_ERROR(ICET_NO_ERROR);
    TRY_ERROR(ICET_SANITY_CHECK_FAIL);
    TRY_ERROR(ICET_INVALID_ENUM);
    TRY_ERROR(ICET_BAD_CAST);
    TRY_ERROR(ICET_OUT_OF_MEMORY);
    TRY_ERROR(ICET_INVALID_OPERATION);
    TRY_ERROR(ICET_INVALID_VALUE);
    printf("## UNKNOWN ICET ERROR CODE!!!!!\n");
#undef TRY_ERROR
}

/* Just in case I need to actually print stuff out to the screen in the
   future. */
static FILE *realstdout;
#if 0
static void realprintf(const char *fmt, ...)
{
    va_list ap;

    if (realstdout != NULL) {
        va_start(ap, fmt);
        vfprintf(realstdout, fmt, ap);
        va_end(ap);
        fflush(realstdout);
    }
}
#endif

static IceTContext context;

static void usage(char **argv)
{
    printf("\nUSAGE: %s [options] [-R] testname [testargs]\n", argv[0]);
    printf("\nWhere options are:\n");
    printf("  -width <n>  Width of window (default n=1024)\n");
    printf("  -height <n> Height of window (default n=768).\n");
    printf("  -display <display>\n");
    printf("              X server each node contacts.  Default display=localhost:0\n");
    printf("  -nologdebug Do not add debugging statements.  Provides less information, but\n");
    printf("              makes identifying errors and warnings easier.\n");
    printf("  -redirect   Redirect standard output to log.????, where ???? is the rank\n");
    printf("  --          Parse no more arguments.\n");
    printf("  -h          This help message.\n");
}

void initialize_test(int *argcp, char ***argvp, IceTCommunicator comm)
{
    int arg;
    int argc = *argcp;
    char **argv = *argvp;
    int width = 1024;
    int height = 768;
    IceTBitField diag_level = ICET_DIAG_FULL;
    int redirect = 0;
    int rank, num_proc;

    rank = (*comm->Comm_rank)(comm);
    num_proc = (*comm->Comm_size)(comm);

  /* This is convenient code to attach a debugger to a particular process at the
     start of a test. */
    /* if (rank == 0) { */
    /*     int i = 0; */
    /*     printf("Waiting in process %d\n", getpid()); */
    /*     while (i == 0) sleep(1); */
    /* } */

  /* Let Glut have first pass at the arguments to grab any that it can use. */
    glutInit(argcp, *argvp);

  /* Parse my arguments. */
    for (arg = 1; arg < argc; arg++) {
        if (strcmp(argv[arg], "-width") == 0) {
            width = atoi(argv[++arg]);
        } else if (strcmp(argv[arg], "-height") == 0) {
            height = atoi(argv[++arg]);
        } else if (strcmp(argv[arg], "-nologdebug") == 0) {
            diag_level &= ICET_DIAG_WARNINGS | ICET_DIAG_ALL_NODES;
        } else if (strcmp(argv[arg], "-redirect") == 0) {
            redirect = 1;
        } else if (strcmp(argv[arg], "-h") == 0) {
            usage(argv);
            exit(0);
        } else if (   (strcmp(argv[arg], "-R") == 0)
                   || (strncmp(argv[arg], "-", 1) != 0) ) {
            break;
        } else if (strcmp(argv[arg], "--") == 0) {
            arg++;
            break;
        } else {
            printf("Unknown options `%s'.  Try -h for help.\n", argv[arg]);
            exit(1);
        }
    }

  /* Fix arguments for next bout of parsing. */
    *argcp = 1;
    for ( ; arg < argc; arg++, (*argcp)++) {
        argv[*argcp] = argv[arg];
    }
    argc = *argcp;

  /* Make sure selected options are consistent. */
    if ((num_proc > 1) && (argc < 2)) {
        printf("You must select a test on the command line when using more than one process.\n");
        printf("Try -h for help.\n");
        exit(1);
    }
    if (redirect && (argc < 2)) {
        printf("You must select a test on the command line when redirecting the output.\n");
        printf("Try -h for help.\n");
        exit(1);
    }

  /* Create a renderable window. */
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_ALPHA);
    glutInitWindowPosition(0, 0);
    glutInitWindowSize(width, height);

    {
        char title[256];
        sprintf(title, "IceT Test %d of %d", rank, num_proc);
        windowId = glutCreateWindow(title);
    }

    SCREEN_WIDTH = width;
    SCREEN_HEIGHT = height;

  /* Create an IceT context. */
    context = icetCreateContext(comm);
    icetDiagnostics(diag_level);
    icetGLInitialize();

  /* Redirect standard output on demand. */
    if (redirect) {
        char filename[64];
        int outfd;
        if (rank == 0) {
            realstdout = fdopen(dup(1), "wt");
        } else {
            realstdout = NULL;
        }
        sprintf(filename, "log.%04d", rank);
        outfd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (outfd < 0) {
            printf("Could not open %s for writing.\n", filename);
            exit(1);
        }
        dup2(outfd, 1);
    } else {
        realstdout = stdout;
    }

    strategy_list[0] = ICET_STRATEGY_DIRECT;
    strategy_list[1] = ICET_STRATEGY_SEQUENTIAL;
    strategy_list[2] = ICET_STRATEGY_SPLIT;
    strategy_list[3] = ICET_STRATEGY_REDUCE;
    strategy_list[4] = ICET_STRATEGY_VTREE;

    single_image_strategy_list[0] = ICET_SINGLE_IMAGE_STRATEGY_AUTOMATIC;
    single_image_strategy_list[1] = ICET_SINGLE_IMAGE_STRATEGY_BSWAP;
    single_image_strategy_list[2] = ICET_SINGLE_IMAGE_STRATEGY_TREE;
}

IceTBoolean strategy_uses_single_image_strategy(IceTEnum strategy)
{
    switch (strategy) {
      case ICET_STRATEGY_DIRECT:        return ICET_FALSE;
      case ICET_STRATEGY_SEQUENTIAL:    return ICET_TRUE;
      case ICET_STRATEGY_SPLIT:         return ICET_FALSE;
      case ICET_STRATEGY_REDUCE:        return ICET_TRUE;
      case ICET_STRATEGY_VTREE:         return ICET_FALSE;
      default:
          printf("ERROR: unknown strategy type.");
          return ICET_TRUE;
    }
}

static void no_op()
{
}

static void glut_draw()
{
    int result;

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, (GLsizei)SCREEN_WIDTH, (GLsizei)SCREEN_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT);
    swap_buffers();

    result = test_function();

    finalize_test(result);

    exit(result);
}

int run_test(int (*tf)(void))
{
  /* Record the test function so we can run it in the Glut draw callback. */
    test_function = tf;

    glutDisplayFunc(no_op);
    glutIdleFunc(glut_draw);

  /* Glut will reliably create the OpenGL context only after the main loop is
   * started.  This will create the window and then call our glut_draw function
   * to populate it.  It will never return, which is why we call exit in
   * glut_draw. */
    glutMainLoop();

  /* We do not expect to be here.  Raise an alert to signal that the tests are
   * not running as expected. */
    return TEST_NOT_PASSED;
}

void swap_buffers(void)
{
    glutSwapBuffers();
}

extern void finalize_communication(void);
void finalize_test(int result)
{
    IceTInt rank;

    checkOglError();
    checkIceTError();

    icetGetIntegerv(ICET_RANK, &rank);
    if (rank == 0) {
        switch (result) {
          case TEST_PASSED:
              printf("***Test Passed***\n");
              break;
          case TEST_NOT_RUN:
              printf("***TEST NOT RUN***\n");
              break;
          case TEST_NOT_PASSED:
              printf("***TEST NOT PASSED***\n");
              break;
          case TEST_FAILED:
              printf("***TEST FAILED***\n");
              break;
        }
    }

    icetDestroyContext(context);
    finalize_communication();
    glutDestroyWindow(windowId);
}
