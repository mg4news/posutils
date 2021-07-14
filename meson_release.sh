#!/bin/bash

# configure for release
meson build
meson configure -Ddefault_library=shared -Dbuildtype=release build

echo "Meson is configured for release build"
echo "to use: <cd build> then use ninja"
  
