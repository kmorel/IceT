#-----------------------------------------------------------------------------
#
# UseICET.cmake - File to INCLUDE in a CMakeLists.txt file to use IceT.
#
# After including this file, you need only to add icet, icet_strategies, and,
# perhaps, icet_mpi libraries with the LINK_LIBRARIES command.
#
## Copyright 2003 Sandia Coporation
## Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
## license for use of this work by or on behalf of the U.S. Government.
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that this Notice and any statement
## of authorship are reproduced on all copies.
#
# Id
#

#Load compiler settings used for IceT.
#On second thought, why enforce the duplication of compiler flags?
#INCLUDE(${CMAKE_ROOT}/Modules/CMakeImportBuildSettings.cmake)
#CMAKE_IMPORT_BUILD_SETTINGS(${ICET_BUILD_SETTINGS_FILE})

# Add compiler flags needed to use IceT.
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ICET_REQUIRED_C_FLAGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ICET_REQUIRED_CXX_FLAGS}")

# Add include directories needed to use IceT.
INCLUDE_DIRECTORIES(${ICET_INCLUDE_DIRS})

# Add link directories needed to use IceT.
LINK_DIRECTORIES(${ICET_LIBRARY_DIRS})
