#!/bin/bash

# configure for debug and for static linked libraries
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -GNinja ..

echo "CMake is configured for release build" 
echo "to use: <cd build> then use ninja"
  
