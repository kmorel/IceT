# Id
#
## Copyright 2003 Sandia Coporation
## Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
## license for use of this work by or on behalf of the U.S. Government.
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that this Notice and any statement
## of authorship are reproduced on all copies.
#

ADD_SUBDIRECTORY(ice-t)
ADD_SUBDIRECTORY(communication)
ADD_SUBDIRECTORY(strategies)

IF (NOT ICET_INSTALL_NO_DEVELOPMENT)
  SET(resPath 
      "${CMAKE_CURRENT_SOURCE_DIR}/ice-t"
      "${CMAKE_CURRENT_SOURCE_DIR}/communication"
      "${CMAKE_CURRENT_SOURCE_DIR}/strategies")
  SET(filesToInstall)
  FOREACH(p IN ${resPath})
      SET(tmpFilesToInstall)
      SET(exts "${p}/*.h;${p}/*.hxx;${p}/*.txx")
      FOREACH(ext IN ${exts})
          FILE(GLOB tmpFilesToInstall
          RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
          "${ext}")
          IF(tmpFilesToInstall)
              SET(filesToInstall "${filesToInstall};${tmpFilesToInstall}")
          ENDIF(tmpFilesToInstall)
      ENDFOREACH(ext IN ${exts})
  ENDFOREACH(p IN ${resPath})
  INSTALL(
      FILES ${filesToInstall}
      DESTINATION "${ICET_INSTALL_INCLUDE_DIR}/ice-t"
      COMPONENT Development)
ENDIF (NOT ICET_INSTALL_NO_DEVELOPMENT)
