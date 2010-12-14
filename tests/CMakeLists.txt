## Copyright 2003 Sandia Coporation
## Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
## the U.S. Government retains certain rights in this software.
##
## This source code is released under the New BSD License.
#

SET(MyTests
  BlankTiles.c
  BoundsBehindViewer.c
  CompressionSize.c
  DisplayNoDraw.c
  OddImageSizes.c
  RandomTransform.c
  SimpleExample.c
  )

SET(UTIL_SRCS init.c ppm.c)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

CREATE_TEST_SOURCELIST(Tests icetTests_mpi.c ${MyTests}
  EXTRA_INCLUDE mpi_comm.h
  FUNCTION init_mpi_comm)

ADD_EXECUTABLE(icetTests_mpi ${Tests} ${UTIL_SRCS})
TARGET_LINK_LIBRARIES(icetTests_mpi
  IceTCore
  IceTGL
  IceTMPI
  ${OPENGL_glu_LIBRARY}
  ${GLUT_LIBRARIES}
  )

IF (ICET_MPIRUN_EXE)
  SET(PRE_TEST_FLAGS ${ICET_MPIRUN_EXE} ${ICET_MPI_NUMPROC_FLAG} ${ICET_MPI_MAX_NUMPROCS} ${ICET_MPI_PREFLAGS})
  SET(POST_TEST_FLAGS ${ICET_MPI_POSTFLAGS})
ENDIF (ICET_MPIRUN_EXE)

SET(ICET_TEST_FLAGS "" CACHE STRING "Arguments given to the test program.  These are given between the test executable and the name of the test.  Run the test executable with the -h flag to see what is available.")
MARK_AS_ADVANCED(ICET_TEST_FLAGS)
SEPARATE_ARGUMENTS(ICET_TEST_FLAGS)

FOREACH (test ${MyTests})
  GET_FILENAME_COMPONENT(TName ${test} NAME_WE)
  ADD_TEST(IceT${TName}
    ${PRE_TEST_FLAGS}
    ${C_TEST_PATH}/icetTests_mpi ${ICET_TEST_FLAGS} ${TName}
    ${POST_TEST_FLAGS})
  IF (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 2.1)
    SET_TESTS_PROPERTIES(IceT${TName}
      PROPERTIES FAIL_REGULAR_EXPRESSION
      ":ERROR:;TEST NOT RUN;TEST NOT PASSED;TEST FAILED"
      )
    SET_TESTS_PROPERTIES(IceT${TName}
      PROPERTIES PASS_REGULAR_EXPRESSION "Test Passed"
      )
  ENDIF (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 2.1)
ENDFOREACH(test)

IF (WIN32)
  ADD_CUSTOM_TARGET(logclean erase log.???? *.ppm)
ELSE (WIN32)
  ADD_CUSTOM_TARGET(logclean rm -f log.???? *.ppm core.*)
ENDIF (WIN32)
