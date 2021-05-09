#!/bin/bash -e
# build-linux.sh

CMAKE_FLAGS='-DLINUX_LOCAL_DEV=true'

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/"

# Move into the build directory, run CMake, and compile the project
mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ../
#Copy wx folder to Source/Core/
cp /home/runner/work/Ishiiruka/Ishiiruka/Externals/wxWidgets3/include/wx /home/runner/work/Ishiiruka/Ishiiruka/build/Source/Core/ -r
cp /home/runner/work/Ishiiruka/Ishiiruka/Externals/wxWidgets3/wx/* /home/runner/work/Ishiiruka/Ishiiruka/build/Source/Core/wx/
make -j$(nproc)
make install DESTDIR=./AppDir;
popd

# Copy the Sys folder in
cp -r -n ${DATA_SYS_PATH} ${BINARY_PATH}

touch ./build/Binaries/portable.txt
