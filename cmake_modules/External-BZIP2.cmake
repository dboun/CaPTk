#MESSAGE( "External project - OpenCV" )

SET( BZIP2_DEPENDENCIES )

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_STANDARD_REQUIRED YES) 

SET( EXTRA_NON_WINDOWS_OPTIONS "")
IF(NOT WIN32)
SET( EXTRA_NON_WINDOWS_OPTIONS -DCMAKE_BUILD_TYPE=Release)
ENDIF()

MESSAGE( STATUS "Adding BZIP2-1.0.6 ...")

ExternalProject_Add( 
  BZIP2
  URL https://newcontinuum.dl.sourceforge.net/project/bzip2/bzip2-1.0.6.tar.gz
  SOURCE_DIR BZIP2-source
  BINARY_DIR BZIP2-build
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
#   INSTALL_COMMAND
  CMAKE_GENERATOR ${gen}
  CMAKE_ARGS
  -DCMAKE_INSTALL_PREFIX:STRING=${PROJECT_BINARY_DIR}/ep
  -DCMAKE_BUILD_TYPE:STRING=Release
)

# SET( BZIP2_DIR ${CMAKE_BINARY_DIR}/BZIP2-build )
#LIST(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/OpenCV-build")
# SET( ENV{CMAKE_PREFIX_PATH} "${CMAKE_PREFIX_PATH};${CMAKE_BINARY_DIR}/BZIP2-build" )