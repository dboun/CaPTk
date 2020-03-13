#MESSAGE( "External project - OpenCV" )

SET( LIBZIP_DEPENDENCIES )

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_STANDARD_REQUIRED YES) 

SET( EXTRA_NON_WINDOWS_OPTIONS "")
IF(NOT WIN32)
SET( EXTRA_NON_WINDOWS_OPTIONS -DCMAKE_BUILD_TYPE=Release)
ENDIF()

MESSAGE( STATUS "Adding LIBZIP-1.5.2a ...")

ExternalProject_Add( 
  LIBZIP
  DEPENDS ZLIB
  GIT_REPOSITORY https://github.com/nih-at/libzip.git #  url from where to download
  GIT_TAG rel-1-6-1
  SOURCE_DIR LibZip-source
  BINARY_DIR LibZip-build
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  # INSTALL_COMMAND ""
  CMAKE_GENERATOR ${gen}
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX:STRING=${PROJECT_BINARY_DIR}/ep
    -DENABLE_BZIP2:BOOL=OFF
    -DZLIB_INCLUDE_DIR:STRING=${PROJECT_BINARY_DIR}/ep/include
    -DZLIB_LIBRARY_DEBUG:STRING=${ZLIB_LIB_DEBUG}
    -DZLIB_LIBRARY_RELEASE:STRING=${ZLIB_LIB_RELEASE}
    -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS} 
    -DCMAKE_BUILD_TYPE:STRING=Release
)


SET( LIBZIP_DIR ${CMAKE_BINARY_DIR}/LIBZIP-build )
#LIST(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/OpenCV-build")
SET( ENV{CMAKE_PREFIX_PATH} "${CMAKE_PREFIX_PATH};${CMAKE_BINARY_DIR}/LIBZIP-build" )