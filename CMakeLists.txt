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
project("posutils" VERSION 1.0.6 LANGUAGES CXX)

# ----------------------------------------------------------------------------------------------------------
# External dependencies
# ----------------------------------------------------------------------------------------------------------
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ----------------------------------------------------------------------------------------------------------
# Build the library
# Anyone that links the library automatically gets the posutils includes
# ----------------------------------------------------------------------------------------------------------
set(POSUTILS_SRC
  src/pumutex.cpp
  src/puthread.cpp
  src/putimer.cpp
)

add_library(${PROJECT_NAME} STATIC ${POSUTILS_SRC})

target_include_directories(${PROJECT_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} 
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
) 

target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

set_target_properties(${PROJECT_NAME}  PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
  POSITION_INDEPENDENT_CODE ON
)

target_link_options(${PROJECT_NAME} PUBLIC
  -Wl,-z,nodelete
)  

# ----------------------------------------------------------------------------------------------------------
# Compiler and link settings
# ----------------------------------------------------------------------------------------------------------
target_compile_options(${PROJECT_NAME} PRIVATE
  -Werror=shadow  
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

  # Options added in GCC 6 and 7.1 
  $<$<COMPILE_LANG_AND_ID:CXX,GNU>:
      $<$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,7.1.0>:
          -Wduplicated-branches
          -Wimplicit-fallthrough
          -Wmisleading-indentation
      >
  >

  $<$<CONFIG:Debug>:>
  $<$<CONFIG:Release>:>
)

# Uncomment this to build a simple test program
# Not on by default
#add_subdirectory(tests)
