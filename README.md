# POSUTILS
this is a simple utility library that:
- wraps and simplifies posix thread creation
- wraps and simplifies posix mutex creation
- provides a simple and accurate timer mechansim leveraging threads, conditional variables and callbacks

## Why even do this?
Someone recently said to me "why not just use the C++ std::threads stuff, and even if you don't, why not make this look like the C++ std::threads stuff?". So, here is why..
- there are still quite a few people that develop in C. It is simple to include a C API and implementation into a C++ implementation. It is not simple to include a C++ interface and implementation in a pure C project. 
- the std::threads interface is woefully limited in anything less than the C++20 standard, and not everyone is able to use C++20. Yes, you can extract the raw pthread and do things like set thread names. But, even in C++ 20 you cannot modify thread creation to add things like fixed stack sizes and guards. 

If you are a C++ fan boy/girl you could definitely wrap this in a std-like class. It would add no intrinsic value, make the usage more complex, but may well assuage any deep seated fears brought on by a simple functional C API..  

## Requirements:
- the library will build with any compiler capable of C++11 or C99. It is currently configured for gcc using std=C++11, that is easily changed. 
- both Meson and CMake are supported. For CMake the minimum version required is 2.x. There are no complex subrproject requirements
- depends on pthreads and on GLIB 2.x
 
## Current Release/Version
1.0.2

### To use in a Meson project
Create a wrap file in the "subprojects" directory. Use the commit hash corresponding to release 1.0.2:

```
[wrap-git]
directory = posutils
url = https://github.com/mg4news/posutils.git
# uses the commit tag corresponding to version 1.0.2
revision=c804ee063f5c8b215b2037ca80f4a879a2dbd0f5
```

### To use in a CMake project
Add the following to your top level CMakeLists.txt file:

```
include(FetchContent)
FetchContent_Declare(
  posutils
  GIT_REPOSITORY https://github.com/mg4news/posutils.git
  GIT_TAG 1.0.2
)  
```

