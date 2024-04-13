cmake_minimum_required(VERSION 3.5)
project (quadtree CXX)

# Project properties
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sources
set(quadtree-src quadtree-test.cpp)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    list(APPEND quadtree-src quadtree-debug.cpp)
endif()

# Targets
add_executable(quadtree-test ${quadtree-src})

# Options
if (CMAKE_BUILD_TYPE MATCHES Debug)
    list(APPEND quadtree-src quadtree-debug.cpp)
    target_compile_definitions(quadtree-test PRIVATE -D_DEBUG)
    target_compile_options(quadtree-test PRIVATE -O0)
else()
    set(COMPILE_OPT -flto -Wall -Wextra -Wpedantic -ffast-math -Ofast -march=native)
    set(LINK_OPT -flto -march=native)

    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        list(APPEND COMPILE_OPT -stdlib=libc++)
        list(APPEND LINK_OPT -stdlib=libc++ -fuse-ld=lld)
    else()
        # Can be enabled if Modern Linker (mold) is installed
        # list(APPEND LINK_OPT -fuse-ld=mold)
    endif()

    target_compile_options(quadtree-test PRIVATE ${COMPILE_OPT})
    target_link_options(quadtree-test PRIVATE ${LINK_OPT})
endif()
