#!/bin/bash -e
# build-mac.sh

CMAKE_FLAGS=''

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/Dolphin.app/Contents/Resources/"

# Move into the build directory, run CMake, and compile the project
mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ..
#Copy wx folder to Source/Core/
cp -R /Users/runner/work/Ishiiruka/Ishiiruka/Externals/wxWidgets3/include/wx /Users/runner/work/Ishiiruka/Ishiiruka/build/Source/Core/
cp /Users/runner/work/Ishiiruka/Ishiiruka/Externals/wxWidgets3/wx/* /Users/runner/work/Ishiiruka/Ishiiruka/build/Source/Core/wx/
make -j7
popd

# Copy the Sys folder in
echo "Copying Sys files into the bundle"
cp -Rfn "${DATA_SYS_PATH}" "${BINARY_PATH}"
