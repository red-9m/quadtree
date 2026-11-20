cmake_minimum_required(VERSION 3.21)
project(quadtree VERSION 2.0.0 LANGUAGES CXX)

add_library(quadtree INTERFACE)
add_library(QuadTree::QuadTree ALIAS quadtree)
set_target_properties(quadtree PROPERTIES EXPORT_NAME QuadTree)
target_include_directories(quadtree INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_compile_features(quadtree INTERFACE cxx_std_20)

option(QUADTREE_INSTALL "Install the QuadTree runtime headers and CMake package" ${PROJECT_IS_TOP_LEVEL})
if (QUADTREE_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    set(quadtree_include_dir "${CMAKE_INSTALL_INCLUDEDIR}/QuadTree")
    set(quadtree_cmake_dir "${CMAKE_INSTALL_LIBDIR}/cmake/QuadTree")
    target_include_directories(quadtree INTERFACE "$<INSTALL_INTERFACE:${quadtree_include_dir}>")

    configure_package_config_file(cmake/QuadTreeConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/QuadTreeConfig.cmake"
        INSTALL_DESTINATION "${quadtree_cmake_dir}")
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/QuadTreeConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)

    install(TARGETS quadtree EXPORT QuadTreeTargets)
    install(EXPORT QuadTreeTargets NAMESPACE QuadTree:: DESTINATION "${quadtree_cmake_dir}")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/QuadTreeConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/QuadTreeConfigVersion.cmake" DESTINATION "${quadtree_cmake_dir}")
    install(FILES quadtree-index.h DESTINATION "${quadtree_include_dir}")
    install(FILES detail/quadtree-index-impl.h DESTINATION "${quadtree_include_dir}/detail")
    install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/QuadTree")
endif()

# Apply only to repository checks and tools, never to the exported target.
function(quadtree_strict_warnings target scope)
    if (MSVC)
        target_compile_options(${target} ${scope} /W4 /permissive- /WX)
    else()
        target_compile_options(${target} ${scope} -Wall -Wextra -Wpedantic
            -Wconversion -Wsign-conversion -Wshadow -pedantic-errors -Werror)
    endif()
endfunction()

option(QUADTREE_BUILD_DEMO "Build the legacy QuadTree demo" ${PROJECT_IS_TOP_LEVEL})
if (QUADTREE_BUILD_DEMO)
    add_executable(quadtree-test quadtree-test.cpp)
    target_compile_features(quadtree-test PRIVATE cxx_std_20)
    set_target_properties(quadtree-test PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_definitions(quadtree-test PRIVATE $<$<CONFIG:Debug>:_DEBUG>)
    quadtree_strict_warnings(quadtree-test PRIVATE)
endif()

# Tests for the next major version (quadtree-index.h)
option(QUADTREE_BUILD_TESTS "Build QuadTree tests" ${PROJECT_IS_TOP_LEVEL})
if (QUADTREE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmark harness (bench/MANIFEST.md)
option(QUADTREE_BUILD_BENCHMARKS "Build QuadTree benchmarks" ${PROJECT_IS_TOP_LEVEL})
if (QUADTREE_BUILD_BENCHMARKS)
    enable_testing()
    add_subdirectory(bench)
endif()
