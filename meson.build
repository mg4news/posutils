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

project('posutils', 'cpp',
    version : '1.0.1',
    default_options : ['cpp_std=c++11', 'warning_level=1'])

# global defines
add_global_arguments('-DLOG_TO_STDOUT', language : 'cpp')

# build and link options
# -Wall already handled by ''warning_level=1''
err_warn_cxx_args = [
  '-Werror=shadow',
  '-Werror=undef',
  '-Werror=uninitialized',
  '-Werror=cast-align',
  '-Werror=format=2',
  '-Werror=init-self',
  '-Werror=pointer-arith',
  '-Werror=unreachable-code', 
  '-Werror=parentheses', 
  '-Werror=switch', 
  '-Werror=unused-function',
  '-Werror=duplicated-branches',
  '-Werror=implicit-fallthrough',
  '-Werror=misleading-indentation',
  '-Werror=unused-function',
  '-Werror=unused',
  '-Werror=extra',
  '-Werror=strict-aliasing',
  '-Werror=pedantic',
  '-Werror=cast-qual',
  '-Werror=init-self',
  '-Werror=logical-op',
  '-Werror=missing-include-dirs',
  '-Werror=redundant-decls',
  '-Werror=strict-overflow=5',
  '-Werror=switch-default',
  # A zero-length format string shouldn't be considered an issue.
  '-Wno-format-zero-length'
]
warn_cxx_link_args = [
  '-Wl,-z,nodelete'
]

cxx = meson.get_compiler('cpp')
add_project_arguments(cxx.get_supported_arguments(err_warn_cxx_args), language : 'cpp')

# includes
posutils_inc = include_directories('include')

# external dependencies used in multiple places
thread_dep = dependency('threads')
glib_dep   = dependency('glib-2.0')

# Source is listed explicitly. Meson doesn't support wildcarding
posutils_lib_src = ['src/puthread.cpp', 'src/pumutex.cpp', 'src/putimer.cpp']

# building this as a shared library, linked as needed
# declare the dependency
posutils_lib = static_library(
  'posutils', 
  posutils_lib_src, 
  dependencies: [thread_dep, glib_dep],
  include_directories : posutils_inc )
  
# create a dependencies object people that pull in this project
posutils_dep = declare_dependency(link_with : posutils_lib, include_directories : posutils_inc)


