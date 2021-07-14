#!/bin/bash

# configure for debug and for static linked libraries
meson build
meson configure -Ddefault_library=static -Dbuildtype=debug build

echo "Meson is configured for debug build" 
echo "to use: <cd build> then use ninja"
  
