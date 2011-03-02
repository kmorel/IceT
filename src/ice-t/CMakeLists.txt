## Copyright 2003 Sandia Coporation
## Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
## the U.S. Government retains certain rights in this software.
##
## This source code is released under the New BSD License.
#

SET(ICET_SRCS
        porting.c
        context.c
        state.c
        diagnostics.c
        communication.c
        tiles.c
	timing.c
        matrix.c
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
        ../strategies/radixk.c
        ../strategies/tree.c
        ../strategies/automatic.c
)

ICET_ADD_LIBRARY(IceTCore ${ICET_SRCS})

IF (UNIX)
  # Depend on the math library under Unix.
  TARGET_LINK_LIBRARIES(IceTCore m)
ENDIF (UNIX)

IF(NOT ICET_INSTALL_NO_DEVELOPMENT)
  INSTALL(FILES ${ICET_SOURCE_DIR}/src/include/IceT.h
    ${ICET_BINARY_DIR}/src/include/IceTConfig.h
    DESTINATION ${ICET_INSTALL_INCLUDE_DIR})
ENDIF(NOT ICET_INSTALL_NO_DEVELOPMENT)
