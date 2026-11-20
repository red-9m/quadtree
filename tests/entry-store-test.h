#pragma once

#include <cstdint>
#include <vector>

#include "index-mixed-macros.h"

struct TagA {};
struct TagB {};

using StoreA = QuadTree::detail::EntryStore<int, int, QuadTree::Unchecked, TagA>;
using HandleA = StoreA::HandleType;

// Implemented in entry-store-test-tu2.cpp, compiled with different _DEBUG/NDEBUG settings.
std::vector<std::uint64_t> tu2Owners(int count);
HandleA tu2Insert(StoreA& store, int payload);
bool tu2Valid(const StoreA& store, HandleA handle);
