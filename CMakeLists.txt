cmake_minimum_required(VERSION 3.5)
project (quadtree CXX)

# Sources
file(GLOB quadtree-src quadtree-test.cpp)
if (CMAKE_BUILD_TYPE MATCHES Debug)
    file(GLOB quadtree-src ${quadtree-src} quadtree-debug.cpp)
endif()

# Targets
add_executable(quadtree-test ${quadtree-src})
set_target_properties(quadtree-test PROPERTIES CXX_STANDARD 20)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(quadtree-test PRIVATE -D_DEBUG)
    target_compile_options(quadtree-test PRIVATE -O1)
else()
    target_compile_options(quadtree-test PRIVATE -flto -Wall -Wextra -Wpedantic -ffast-math -Ofast)
    target_link_options(quadtree-test PRIVATE -flto -fuse-ld=gold)
endif()
