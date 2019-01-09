#!/bin/bash

# CaPTk Packager
# Cmake command to run from /trunk/bin
# We need this directory structure for appimages to be generated
CAPTK_CMD () {
# cmake -DBUILD_DOCUMENTATION=OFF ..
export CC=/usr/local/opt/llvm/bin/clang
export CXX=/usr/local/opt/llvm/bin/clang++
export LDFLAGS="-L/usr/local/opt/llvm/lib"
export CPPFLAGS="-L/usr/local/opt/llvm/include"

export PATH="$PATH:/usr/local/opt/llvm/bin:/usr/local/Cellar/qt/5.11.1/bin:/usr/local/Cellar/qt/5.11.1/lib/cmake/Qt5"

echo "going to QT5"
cd /usr/local/Cellar/qt/5.11.1/lib/cmake/Qt5
echo "going back"
cd /Users/travis/build/PhucNgo1711/CaPTk/bin

# export CMAKE_PREFIX_PATH=/Library/TeX/texbin

cmake ../
make -j 12

export CMAKE_PREFIX_PATH=/Users/travis/build/PhucNgo1711/CaPTk/bin/ITK
cmake ../
make -j 12

# export CMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH:/Users/travis/build/PhucNgo1711/dependency_manager/bin/ITK-build
}

###########################
echo "[!] Warn: This script is intended for CI use. Only use it if you know what you are doing."

echo "[:] Starting CaPTk packaging process..."

echo "[?] Checking if you are in trunk..."
# Test to see if the user is in trunk
# First see if CMakeLists.txt exists
if [[ -e CMakeLists.txt ]] ; then
# See if it contains PROJECT( CaPTk )
if [[ -z `cat CMakeLists.txt | grep "PROJECT( CaPTk )"` ]] ; then
echo "[!] Error: You do not appear to be within trunk of CaPTk (Project is not CaPTk)"
exit -1
fi
else
echo "[!] Error: You do not appear to be in trunk (CMakeLists.txt not found)"
exit -1
fi

# echo "[:] Set up env..."
# ENV_CMD

# Create binary directory
echo "[:] Creating binary directory..."
mkdir bin
cd bin

# Cmake
echo "[:] Running cmake command and build CaPTk..."
CAPTK_CMD

# # Make install/strip
# echo "[:] Building CaPTk..."
# make

echo "[:] Done. Built test target"
