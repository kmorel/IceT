## Copyright 2003 Sandia Coporation
## Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
## license for use of this work by or on behalf of the U.S. Government.
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that this Notice and any statement
## of authorship are reproduced on all copies.
#

SET(ICET_MPI_SRCS
  mpi.c
  )

IF (ICET_USE_MPI)
  ADD_LIBRARY(IceTMPI ${ICET_MPI_SRCS})

  TARGET_LINK_LIBRARIES(IceTMPI
    IceTCore
    ${ICET_MPI_LIBRARIES}
    )

  IF(NOT ICET_INSTALL_NO_LIBRARIES)
    INSTALL(TARGETS IceTMPI
      RUNTIME DESTINATION ${ICET_INSTALL_BIN_DIR} COMPONENT RuntimeLibraries
      LIBRARY DESTINATION ${ICET_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries
      ARCHIVE DESTINATION ${ICET_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries)
  ENDIF(NOT ICET_INSTALL_NO_LIBRARIES)
  IF(NOT ICET_INSTALL_NO_DEVELOPMENT)
    INSTALL(FILES ${ICET_SOURCE_DIR}/src/include/IceTMPI.h
      DESTINATION ${ICET_INSTALL_INCLUDE_DIR})
    INSTALL(TARGETS IceTMPI
      DESTINATION ${ICET_INSTALL_LIB_DIR} COMPONENT Development)
  ENDIF(NOT ICET_INSTALL_NO_DEVELOPMENT)

ENDIF (ICET_USE_MPI)
