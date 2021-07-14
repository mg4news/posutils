# ==========================================================================================================
# This is free and unencumbered software released into the public domain.
# 
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
# 
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
# 
# For more information, please refer to <http://unlicense.org>
# ==========================================================================================================
cmake_minimum_required(VERSION 3.14)
project("posutils" VERSION 1.0.1 LANGUAGES CXX)

# ----------------------------------------------------------------------------------------------------------
# Compiler and link settings
# ----------------------------------------------------------------------------------------------------------
add_compile_options(
  #-Werror=shadow  Renable once RCPis fixed..
  -Werror=undef
  -Werror=uninitialized
  -Werror=cast-align
  -Werror=format=2
  -Werror=init-self
  -Werror=pointer-arith
  -Werror=all
  -Werror=unreachable-code 
  -Werror=parentheses
  -Werror=switch
  -Werror=unused-function
  -Werror=duplicated-branches
  -Werror=implicit-fallthrough
  -Werror=misleading-indentation
  -Werror=unused-function
  -Werror=unused
  -Werror=extra
  -Werror=strict-aliasing
  -Werror=pedantic
  -Werror=cast-qual
  -Werror=init-self
  -Werror=logical-op
  -Werror=missing-include-dirs
  -Werror=redundant-decls
  -Werror=strict-overflow=5
  -Werror=switch-default
  # A zero-length format string shouldn't be considered an issue.
  -Wno-format-zero-length
  -Wno-variadic-macros

  -Wctor-dtor-privacy
  -Wnoexcept
  -Wsign-promo
  -Wstrict-null-sentinel

  $<$<CONFIG:Debug>:>
  $<$<CONFIG:Release>:>
)

add_link_options(
  -Wl,-z,nodelete
)  

set(CXX_STANDARD 11)
set(CXX_STANDARD_REQUIRED YES)
set(CXX_EXTENSIONS NO)

# ----------------------------------------------------------------------------------------------------------
# External dependencies. Most pulled using pkg-config
# ----------------------------------------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(glib2 REQUIRED glib-2.0)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ----------------------------------------------------------------------------------------------------------
# Build the library
# Anyone that links the library automatocally gets the posutils includes
# ----------------------------------------------------------------------------------------------------------
include_directories(include)

set(POSUTILS_SRC
  src/pumutex.cpp
  src/puthread.cpp
  src/putimer.cpp
)

add_library(posutils STATIC ${POSUTILS_SRC})
target_include_directories(posutils 
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} 
  PRIVATE ${glib2_INCLUDE_DIRS})
target_link_libraries(posutils PRIVATE Threads::Threads ${glib2_LIBRARY_DIRS})







