## Copyright 2003 Sandia Coporation
## Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
## license for use of this work by or on behalf of the U.S. Government.
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that this Notice and any statement
## of authorship are reproduced on all copies.
#

SET(ICET_STRATEGIES_SRCS
        common.c
        direct.c
        sequential.c
        split.c
        reduce.c
        vtree.c
	bswap.c
	tree.c
	automatic.c
)

ADD_LIBRARY(IceTStrategies ${ICET_STRATEGIES_SRCS})

TARGET_LINK_LIBRARIES(IceTStrategies IceTCore)

IF(NOT ICET_INSTALL_NO_LIBRARIES)
  INSTALL(TARGETS IceTStrategies
    RUNTIME DESTINATION ${ICET_INSTALL_BIN_DIR} COMPONENT RuntimeLibraries
    LIBRARY DESTINATION ${ICET_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries
    ARCHIVE DESTINATION ${ICET_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries)
ENDIF(NOT ICET_INSTALL_NO_LIBRARIES)
