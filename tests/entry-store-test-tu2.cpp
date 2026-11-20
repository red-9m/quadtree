// Second translation unit: shares the module identity counter with the main
// test and is compiled with _DEBUG while the main test defines NDEBUG.
#include "entry-store-test.h"

std::vector<std::uint64_t> tu2Owners(int count)
{
    std::vector<std::uint64_t> owners;
    for (int i = 0; i < count; ++i)
    {
        StoreA store;
        const auto inserted = store.insert(QuadTree::Box<int>{0, 0, 1, 1}, i);
        owners.push_back(QuadTree::detail::HandleAccess::owner(inserted.handle));
    }
    return owners;
}

HandleA tu2Insert(StoreA& store, int payload)
{
    return store.insert(QuadTree::Box<int>{0, 0, 1, 1}, payload).handle;
}

bool tu2Valid(const StoreA& store, HandleA handle)
{
    return store.valid(handle);
}
