cmake_minimum_required(VERSION 3.5)
project (quadtree CXX)

# Project properties
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sources
set(quadtree-src quadtree-test.cpp)

# Targets
add_executable(quadtree-test ${quadtree-src})

# Show Compiler
message(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER_ID}")

# Options
set(COMPILE_OPT )
set(LINK_OPT )

if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(quadtree-test PRIVATE -D_DEBUG)
    target_compile_options(quadtree-test PRIVATE -O0)
else()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        list(APPEND COMPILE_OPT -stdlib=libc++ -fstrict-aliasing)
        list(APPEND LINK_OPT -stdlib=libc++ -fuse-ld=lld)
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Modern Linker (mold) can be enabled if it is already installed
        # list(APPEND LINK_OPT -fuse-ld=mold)
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        # https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options?view=msvc-170
        list(APPEND LINK_OPT /WX)
    else()
        list(APPEND COMPILE_OPT -flto=auto -Wall -Wextra -Wpedantic -O2 -march=native)
        list(APPEND LINK_OPT -flto -march=native -Wl,-O2 -Wl,--as-needed -Wl,--strip-all)
    endif()

    target_compile_options(quadtree-test PRIVATE ${COMPILE_OPT})
    target_link_options(quadtree-test PRIVATE ${LINK_OPT})
endif()
