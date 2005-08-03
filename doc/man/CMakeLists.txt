# Id
#
## Copyright 2003 Sandia Coporation
## Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
## license for use of this work by or on behalf of the U.S. Government.
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that this Notice and any statement
## of authorship are reproduced on all copies.
#

SET(MAN3_FILES
  man3/icetAddTile.3 
  man3/icetBoundingBox.3 
  man3/icetBoundingVertices.3 
  man3/icetCompositeOrder.3
  man3/icetCopyState.3 
  man3/icetCreateContext.3 
  man3/icetCreateMPICommunicator.3
  man3/icetDataReplicationGroup.3
  man3/icetDestroyContext.3 
  man3/icetDestroyMPICommunicator.3 
  man3/icetDiagnostics.3 
  man3/icetDrawFrame.3 
  man3/icetDrawFunc.3 
  man3/icetEnable.3 
  man3/icetGet.3 
  man3/icetGetColorBuffer.3 
  man3/icetGetContext.3 
  man3/icetGetError.3 
  man3/icetGetStrategyName.3 
  man3/icetInputOutputBuffers.3 
  man3/icetIsEnabled.3 
  man3/icetResetTiles.3 
  man3/icetSetContext.3 
  man3/icetStrategy.3 
  man3/icetTemplate.3 
  man3/icetWallTime.3 
  )

SET(MAN7_FILES
  man7/ICE-T.7
  )

MAKE_DIRECTORY(${CMAKE_CURRENT_BINARY_DIR}/man3)
MAKE_DIRECTORY(${CMAKE_CURRENT_BINARY_DIR}/man7)

MACRO(ICET_LINK_MAN3_FILE srcfile destfile)
  ADD_CUSTOM_TARGET(man3/${destfile}.3 ALL
    ln -sf ${ICET_SOURCE_DIR}/doc/man/man3/${srcfile}.3 man3/${destfile}.3
    DEPENDS man3)
  INSTALL_FILES(/man/man3 FILES ${ICET_BINARY_DIR}/doc/man/man3/${destfile}.3)
ENDMACRO(ICET_LINK_MAN3_FILE)

MACRO(ICET_LINK_MAN7_FILE srcfile destfile)
  ADD_CUSTOM_TARGET(man7/${destfile}.7 ALL
    ln -sf ${ICET_SOURCE_DIR}/doc/man/man7/${srcfile}.7 man7/${destfile}.7
    DEPENDS man7)
  INSTALL_FILES(/man/man7 FILES ${ICET_BINARY_DIR}/doc/man/man7/${destfile}.7)
ENDMACRO(ICET_LINK_MAN7_FILE)

ICET_LINK_MAN3_FILE(icetBoundingBox icetBoundingBoxd)
ICET_LINK_MAN3_FILE(icetBoundingBox icetBoundingBoxf)
ICET_LINK_MAN3_FILE(icetEnable icetDisable)
ICET_LINK_MAN3_FILE(icetGet icetGetBooleanv)
ICET_LINK_MAN3_FILE(icetGet icetGetDoublev)
ICET_LINK_MAN3_FILE(icetGet icetGetFloatv)
ICET_LINK_MAN3_FILE(icetGet icetGetIntegerv)
ICET_LINK_MAN3_FILE(icetGet icetGetPointerv)
ICET_LINK_MAN3_FILE(icetGetColorBuffer icetGetDepthBuffer)

ICET_LINK_MAN7_FILE(ICE-T ice-t)
ICET_LINK_MAN7_FILE(ICE-T icet)

INSTALL_FILES(/man/man3 FILES ${MAN3_FILES})
INSTALL_FILES(/man/man7 FILES ${MAN7_FILES})
