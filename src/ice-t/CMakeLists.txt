## Copyright 2003 Sandia Coporation
## Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
## license for use of this work by or on behalf of the U.S. Government.
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that this Notice and any statement
## of authorship are reproduced on all copies.
#

SET(ICET_SRCS
        porting.c
        context.c
        state.c
        diagnostics.c
	communication.c
        tiles.c
        projections.c
        draw.c
        image.c

        ../strategies/common.c
	../strategies/select.c
        ../strategies/direct.c
        ../strategies/sequential.c
        ../strategies/split.c
        ../strategies/reduce.c
        ../strategies/vtree.c
	../strategies/bswap.c
	../strategies/tree.c
	../strategies/automatic.c
)

ADD_LIBRARY(IceTCore ${ICET_SRCS})

IF (UNIX)
  # Depend on the math library under Unix.
  TARGET_LINK_LIBRARIES(IceTCore m)
ENDIF (UNIX)

IF(NOT ICET_INSTALL_NO_LIBRARIES)
  INSTALL(TARGETS IceTCore
        RUNTIME DESTINATION ${ICET_INSTALL_BIN_DIR} COMPONENT RuntimeLibraries
        LIBRARY DESTINATION ${ICET_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries
	ARCHIVE DESTINATION ${ICET_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries)
ENDIF(NOT ICET_INSTALL_NO_LIBRARIES)
IF(NOT ICET_INSTALL_NO_DEVELOPMENT)
  INSTALL(FILES ${ICET_SOURCE_DIR}/src/include/IceT.h
    ${ICET_BINARY_DIR}/src/include/IceTConfig.h
    DESTINATION ${ICET_INSTALL_INCLUDE_DIR})
ENDIF(NOT ICET_INSTALL_NO_DEVELOPMENT)
