# Id
#
## Copyright 2003 Sandia Coporation
## Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
## license for use of this work by or on behalf of the U.S. Government.
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that this Notice and any statement
## of authorship are reproduced on all copies.
#

CMAKE_MINIMUM_REQUIRED(VERSION 1.6)
PROJECT(ICET C)

# Set the current ICE-T version.
SET(ICET_MAJOR_VERSION 0)
SET(ICET_MINOR_VERSION 5)
SET(ICET_PATCH_VERSION 1)
SET(ICET_VERSION "${ICET_MAJOR_VERSION}.${ICET_MINOR_VERSION}.${ICET_PATCH_VERSION}")

# Set output paths.
SET(LIBRARY_OUTPUT_PATH ${ICET_BINARY_DIR}/bin CACHE PATH
	"Output directory for building all libraries.")
SET(EXECUTABLE_OUTPUT_PATH ${ICET_BINARY_DIR}/bin CACHE PATH
	"Output directory for building all executable.")
MARK_AS_ADVANCED(LIBRARY_OUTPUT_PATH EXECUTABLE_OUTPUT_PATH)
SET(ICET_LIBRARY_DIR ${LIBRARY_OUTPUT_PATH}/${CMAKE_CFG_INTDIR})
SET(ICET_EXECUTABLE_DIR ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_CFG_INTDIR})
SET(C_TEST_PATH ${EXECUTABLE_OUTPUT_PATH})

# Turn shared libraries on or off.
OPTION(BUILD_SHARED_LIBS "Build ICE-T with shared libraries." OFF)
SET(ICET_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})

# Configure testing support.
INCLUDE(${CMAKE_ROOT}/Modules/Dart.cmake)
IF (BUILD_TESTING)
  ENABLE_TESTING()
ENDIF (BUILD_TESTING)

IF (CMAKE_BUILD_TYPE MATCHES "Debug")
  ADD_DEFINITIONS(-DDEBUG)
ENDIF (CMAKE_BUILD_TYPE MATCHES "Debug")


# Configure OpenGL support.
INCLUDE(${CMAKE_ROOT}/Modules/FindOpenGL.cmake)
IF (OPENGL_FOUND)
  INCLUDE_DIRECTORIES(${OPENGL_INCLUDE_DIR})
  LINK_LIBRARIES(${OPENGL_LIBRARIES})
ELSE (OPENGL_FOUND)
  MESSAGE(SEND_ERROR "Could not find OpenGL, which is required to compile ICE-T.")
ENDIF (OPENGL_FOUND)

# Configure MPI support.
INCLUDE (${CMAKE_ROOT}/Modules/FindMPI.cmake)
MARK_AS_ADVANCED(CLEAR MPI_INCLUDE_PATH)
MARK_AS_ADVANCED(CLEAR MPI_LIBRARY)
MARK_AS_ADVANCED(CLEAR MPI_EXTRA_LIBRARY)
IF (MPI_INCLUDE_PATH)
  INCLUDE_DIRECTORIES(${MPI_INCLUDE_PATH})
ELSE (MPI_INCLUDE_PATH)
  MESSAGE(SEND_ERROR "Could not find MPI includes, which is required to compile ICE-T.")
ENDIF (MPI_INCLUDE_PATH)
IF (MPI_LIBRARY)
  LINK_LIBRARIES(${MPI_LIBRARY})
ELSE (MPI_LIBRARY)
  MESSAGE(SEND_ERROR "Could not find MPI library, which is required to compile ICE-T.")
ENDIF (MPI_LIBRARY)
IF (MPI_EXTRA_LIBRARY)
  LINK_LIBRARIES(${MPI_EXTRA_LIBRARY})
ENDIF (MPI_EXTRA_LIBRARY)


# Configure MPI testing support.
IF (BUILD_TESTING)
  FIND_PROGRAM(ICET_MPIRUN_EXE NAMES mpirun lamexec)
  SET(ICET_MPI_PREFLAGS "-np;2" CACHE STRING "Flags used by MPI start program.")
  SET(ICET_MPI_POSTFLAGS "" CACHE STRING "Flags used by MPI start program.")
  SET(ICET_MPI_MAX_NUMPROCS "2" CACHE STRING "Maximum number of processors available to run parallel applications.")
  MARK_AS_ADVANCED(ICET_MPIRUN_EXE ICET_MPI_PREFLAGS ICET_MPI_POSTFLAGS ICET_MPI_MAX_NUMPROCS)
  SEPARATE_ARGUMENTS(ICET_MPI_PREFLAGS)
  SEPARATE_ARGUMENTS(ICET_MPI_POSTFLAGS)
ENDIF (BUILD_TESTING)

# Add options to select install directory.
# IF (WIN32)
#   SET(ICET_DEFAULT_INSTALL_PREFIX "C:/Program Files/ICE-T")
# ELSE (WIN32)
#   SET(ICET_DEFAULT_INSTALL_PREFIX "/usr/local")
# ENDIF (WIN32)
# SET(ICET_INSTALL_PREFIX ${ICET_DEFAULT_INSTALL_PREFIX} CACHE PATH "Directory under which all ICE-T files are installed.")

# Configure files with settings for use by the build.
CONFIGURE_FILE(${ICET_SOURCE_DIR}/src/include/GL/ice-t_config.h.in
	       ${ICET_BINARY_DIR}/src/include/GL/ice-t_config.h)

# Point to ICE-T include files.
INCLUDE_DIRECTORIES(${ICET_SOURCE_DIR}/src/include)
INCLUDE_DIRECTORIES(${ICET_BINARY_DIR}/src/include)

SUBDIRS(src)
IF(NOT WIN32)
  SUBDIRS(doc)
ENDIF(NOT WIN32)

IF (BUILD_TESTING)
  SUBDIRS(tests)
ENDIF (BUILD_TESTING)

# Save the CMake settings so another project can import them.  Loaded by:
#  INCLUDE(${CMAKE_ROOT}/Modules/CMakeImportBuildSettings.cmake)
#  CMAKE_IMPORT_BUILD_SETTINGS(<build_file>)
INCLUDE(${CMAKE_ROOT}/Modules/CMakeExportBuildSettings.cmake)
CMAKE_EXPORT_BUILD_SETTINGS(${ICET_BINARY_DIR}/ICETBuildSettings.cmake)
INSTALL_FILES(/lib .cmake ${ICET_BINARY_DIR}/ICETBuildSettings)

# Save the library dependency graph for external projects.
EXPORT_LIBRARY_DEPENDENCIES(${ICET_BINARY_DIR}/ICETLibraryDepends.cmake)
INSTALL_FILES(/lib .cmake ${ICET_BINARY_DIR}/ICETLibraryDepends)

# Save ICE-T specific configuration options.
#First, configuration for build directory.
SET(ICET_INCLUDE_DIRS_CONFIG "${ICET_SOURCE_DIR}/src/include;${ICET_BINARY_DIR}/src/include")
SET(ICET_LIBRARY_DIRS_CONFIG ${ICET_LIBRARY_DIR})
SET(ICET_USE_FILE_CONFIG ${ICET_SOURCE_DIR}/UseICET.cmake)
SET(ICET_BUILD_SETTINGS_FILE_CONFIG ${ICET_BINARY_DIR}/ICETBuildSettings.cmake)
SET(ICET_LIBRARY_DEPENDS_FILE ${ICET_BINARY_DIR}/ICETLibraryDepends.cmake)
CONFIGURE_FILE(${ICET_SOURCE_DIR}/ICETConfig.cmake.in
	       ${ICET_BINARY_DIR}/ICETConfig.cmake @ONLY IMMEDIATE)
#Second, configuration for install directory.
SET(ICET_INCLUDE_DIRS_CONFIG ${CMAKE_INSTALL_PREFIX}/include)
SET(ICET_LIBRARY_DIRS_CONFIG ${CMAKE_INSTALL_PREFIX}/lib)
SET(ICET_USE_FILE_CONFIG ${CMAKE_INSTALL_PREFIX}/lib/UseICET.cmake)
SET(ICET_BUILD_SETTINGS_FILE_CONFIG
    ${CMAKE_INSTALL_PREFIX}/lib/ICETBuildSettings.cmake)
SET(ICET_LIBRARY_DEPENDS_FILE
    ${CMAKE_INSTALL_PREFIX}/lib/ICETLibraryDepends.cmake)
CONFIGURE_FILE(${ICET_SOURCE_DIR}/ICETConfig.cmake.in
	       ${ICET_LIBRARY_DIR}/ICETConfig.cmake @ONLY IMMEDIATE)
INSTALL_FILES(/lib .cmake ${ICET_SOURCE_DIR}/UseICET
			  ${ICET_LIBRARY_DIR}/ICETConfig)

# Allow local additions to this file without CVS conflicts.
INCLUDE(${ICET_BINARY_DIR}/LocalUserOptions.cmake OPTIONAL)
INCLUDE(${ICET_SOURCE_DIR}/LocalUserOptions.cmake OPTIONAL)
