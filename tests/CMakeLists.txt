# Id
#
## Copyright 2003 Sandia Coporation
## Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
## license for use of this work by or on behalf of the U.S. Government.
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that this Notice and any statement
## of authorship are reproduced on all copies.
#

SET(MyTests
	SimpleExample.c
	BlankTiles.c
	CompressionSize.c
	DisplayNoDraw.c
	RandomTransform.c
	)

SET(UTIL_SRCS init.c ppm.c)

IF (WIN32)
  SET(UTIL_SRCS ${UTIL_SRCS} glwin_wgl.c)
ELSE (WIN32)
  SET(UTIL_SRCS ${UTIL_SRCS} glwin_glx.c)
ENDIF (WIN32)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

CREATE_TEST_SOURCELIST(Tests icetTests_mpi.c ${MyTests}
		       EXTRA_INCLUDE mpi_comm.h
		       FUNCTION init_mpi_comm)

ADD_EXECUTABLE(icetTests_mpi ${Tests} ${UTIL_SRCS})
TARGET_LINK_LIBRARIES(icetTests_mpi icet icet_strategies icet_mpi)

IF (ICET_MPIRUN_EXE)
  SET(PRE_TEST_FLAGS ${ICET_MPIRUN_EXE} ${ICET_MPI_PREFLAGS})
  SET(POST_TEST_FLAGS ${ICET_MPI_POSTFLAGS})
ENDIF (ICET_MPIRUN_EXE)

SET(ICET_TEST_FLAGS "-redirect" CACHE STRING "Arguments given to the test program.")
MARK_AS_ADVANCED(ICET_TEST_FLAGS)
SEPARATE_ARGUMENTS(ICET_TEST_FLAGS)

FOREACH (test ${MyTests})
  GET_FILENAME_COMPONENT(TName ${test} NAME_WE)
  ADD_TEST(${TName}
	   ${PRE_TEST_FLAGS}
		${C_TEST_PATH}/icetTests_mpi ${ICET_TEST_FLAGS} ${TName}
		${POST_TEST_FLAGS})
ENDFOREACH(test)

IF (WIN32)
  ADD_CUSTOM_TARGET(logclean erase log.???? *.ppm)
ELSE (WIN32)
  ADD_CUSTOM_TARGET(logclean rm -f log.???? *.ppm core.*)
ENDIF (WIN32)
