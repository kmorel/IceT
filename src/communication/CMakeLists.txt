# Id
#
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

ADD_LIBRARY(icet_mpi ${ICET_MPI_SRCS})

TARGET_LINK_LIBRARIES(icet_mpi icet)

INSTALL_TARGETS(/lib icet_mpi)
INSTALL_FILES(/include/GL .h ${ICET_SOURCE_DIR}/src/include/GL/ice-t_mpi)
