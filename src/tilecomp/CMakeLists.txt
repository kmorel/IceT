# Id
#
## Copyright 2003 Sandia Coporation
## Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
## license for use of this work by or on behalf of the U.S. Government.
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that this Notice and any statement
## of authorship are reproduced on all copies.
#

SET(TILECOMP_SRCS
	propread.c
	tilecomp.c)

ADD_EXECUTABLE(tilecomp ${TILECOMP_SRCS})

#We need to find gmrd.
FIND_PATH(GMRD_INCLUDE_PATH gmrd.h
	  /usr/local/include
	  /usr/include
	  ${ICET_SOURCE_DIR}/../gmrd/src)
FIND_LIBRARY(GMRD_LIBRARY
	     NAMES gmrd libgmrd
	     PATHS /usr/local/lib /usr/lib ${ICET_SOURCE_DIR}/../gmrd/src
		   ${ICET_SOURCE_DIR}/../gmrd/win32/Release
		   ${ICET_SOURCE_DIR}/../gmrd/win32/Debug)

IF (GMRD_INCLUDE_PATH)
  INCLUDE_DIRECTORIES(${GMRD_INCLUDE_PATH})
ELSE (GMRD_INCLUDE_PATH)
  MESSAGE(SEND_ERROR "Could not find GMRD header required for tilecomp.")
ENDIF (GMRD_INCLUDE_PATH)

IF (GMRD_LIBRARY)
  TARGET_LINK_LIBRARIES(tilecomp "${GMRD_LIBRARY}")
ELSE (GMRD_LIBRARY)
  MESSAGE(SEND_ERROR "Could not find GMRD library required for tilecomp.")
ENDIF (GMRD_LIBRARY)

#Find glut
INCLUDE(${CMAKE_ROOT}/Modules/FindGLUT.cmake)
MARK_AS_ADVANCED(CLEAR GLUT_INCLUDE_DIR)
MARK_AS_ADVANCED(CLEAR GLUT_glut_LIBRARY)
IF (GLUT_FOUND)
  INCLUDE_DIRECTORIES(${GLUT_INCLUDE_DIR})
  TARGET_LINK_LIBRARIES(tilecomp ${GLUT_LIBRARIES})
ELSE (GLUT_FOUND)
  MESSAGE(SEND_ERROR "Could not find GLUT library required for tilecomp.")
ENDIF (GLUT_FOUND)

TARGET_LINK_LIBRARIES(tilecomp icet icet_strategies icet_mpi)
