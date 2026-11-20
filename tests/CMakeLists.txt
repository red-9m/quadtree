set(CMAKE_CXX_EXTENSIONS OFF)

# Warning and exception settings are private to the test targets.
add_library(quadtree-test-options INTERFACE)
target_link_libraries(quadtree-test-options INTERFACE QuadTree::QuadTree)
quadtree_strict_warnings(quadtree-test-options INTERFACE)

add_executable(quadtree-dynamic-static ../examples/dynamic-static.cpp)
target_link_libraries(quadtree-dynamic-static PRIVATE quadtree-test-options)
add_test(NAME index-example-dynamic-static COMMAND quadtree-dynamic-static)

# One header per TU: the runtime and legacy APIs must never share a TU.
add_library(quadtree-public-headers OBJECT)
set_target_properties(quadtree-public-headers PROPERTIES UNITY_BUILD OFF)
target_link_libraries(quadtree-public-headers PRIVATE quadtree-test-options)
target_compile_definitions(quadtree-public-headers PRIVATE $<$<CONFIG:Debug>:_DEBUG>)
foreach (header quadtree-index.h quadtree.h quadtree-debug.h)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/include-${header}.cpp")
    file(GENERATE OUTPUT "${source}" CONTENT "#include <${header}>\n")
    target_sources(quadtree-public-headers PRIVATE "${source}")
endforeach()

add_library(quadtree-no-exceptions INTERFACE)
if (MSVC)
    target_compile_options(quadtree-no-exceptions INTERFACE /EHs-c-)
    target_compile_definitions(quadtree-no-exceptions INTERFACE _HAS_EXCEPTIONS=0)
else()
    target_compile_options(quadtree-no-exceptions INTERFACE -fno-exceptions)
endif()
add_library(quadtree-no-exceptions-check OBJECT no-exceptions-check.cpp)
target_link_libraries(quadtree-no-exceptions-check PRIVATE quadtree-test-options quadtree-no-exceptions)

find_package(Threads REQUIRED)
foreach (variant index-test index-test-noexcept)
    add_executable(${variant} index-test.cpp index-test-tu2.cpp)
    target_link_libraries(${variant} PRIVATE quadtree-test-options Threads::Threads)
    add_test(NAME ${variant} COMMAND ${variant})
endforeach()
target_link_libraries(index-test-noexcept PRIVATE quadtree-no-exceptions)
set_source_files_properties(index-test-tu2.cpp entry-store-test-tu2.cpp
    PROPERTIES COMPILE_DEFINITIONS QUADTREE_TEST_DEBUG_HEADER)

add_executable(index-death index-death.cpp)
target_link_libraries(index-death PRIVATE quadtree-test-options quadtree-no-exceptions)
target_compile_definitions(index-death PRIVATE NDEBUG)
foreach (case stale-moved foreign-moved default-partners cleared-update)
    add_test(NAME index-death-${case}
        COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:index-death> -DCASE=${case}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/expect-abort.cmake)
endforeach()

# Main TU defines NDEBUG, the second TU _DEBUG: mixed-configuration ODR check.
foreach (variant entry-store-test entry-store-test-noexcept)
    add_executable(${variant} entry-store-test.cpp entry-store-test-tu2.cpp)
    target_link_libraries(${variant} PRIVATE quadtree-test-options)
    add_test(NAME ${variant} COMMAND ${variant})
endforeach()
target_link_libraries(entry-store-test-noexcept PRIVATE quadtree-no-exceptions)

add_executable(entry-store-death entry-store-death.cpp)
target_link_libraries(entry-store-death PRIVATE quadtree-test-options quadtree-no-exceptions)
target_compile_definitions(entry-store-death PRIVATE NDEBUG)

add_test(NAME entry-store-death-valid COMMAND entry-store-death valid)
foreach (case stale-remove foreign-payload default-rect cleared-update moved-setfilter)
    add_test(NAME entry-store-death-${case}
        COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:entry-store-death> -DCASE=${case}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/expect-abort.cmake)
endforeach()
