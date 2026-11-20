#pragma once

// Match the STL/CRT ABI across both TUs: MSVC's _DEBUG affects more than
// iterator checking. Load all standard headers used by Index before toggling
// macros, and restore the macros before any other headers can see them.
#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

#pragma push_macro("_DEBUG")
#pragma push_macro("NDEBUG")
#undef _DEBUG
#undef NDEBUG
#ifdef QUADTREE_TEST_DEBUG_HEADER
#define _DEBUG 1
#else
#define NDEBUG 1
#endif
#include "quadtree-index.h"
#pragma pop_macro("NDEBUG")
#pragma pop_macro("_DEBUG")
