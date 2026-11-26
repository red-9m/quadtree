// Checked types terminate on invalid handles before mutation (QT-FR-07,
// QT-NFR-12). Built with NDEBUG and, where supported, without exceptions.
// Usage: entry-store-death <case>; exit code 7 means no termination happened.
#include <cstdlib>
#include <cstring>

#include "quadtree-index.h"

namespace
{
struct Tag {};
using Store = QuadTree::detail::EntryStore<int, int, QuadTree::Checked, Tag>;
constexpr QuadTree::Box<int> kBox{0, 0, 1, 1};
constexpr int kNotTerminated = 7;
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT); // No abort dialog
#endif

    const char* name = argv[1];
    Store store;
    const auto handle = store.insert(kBox, 1).handle;

    if (std::strcmp(name, "valid") == 0)
    {
        // Explicit valid() returns false without terminating.
        const bool ok = store.valid(handle) && !store.valid(Store::HandleType{});
        return ok ? 0 : 1;
    }
    if (std::strcmp(name, "stale-remove") == 0)
    {
        if (store.remove(handle) != QuadTree::Status::Ok)
            return 1;
        (void)store.remove(handle);
    }
    else if (std::strcmp(name, "foreign-payload") == 0)
    {
        Store other;
        (void)other.insert(kBox, 2);
        (void)other.payload(handle);
    }
    else if (std::strcmp(name, "default-rect") == 0)
    {
        (void)store.rect(Store::HandleType{});
    }
    else if (std::strcmp(name, "cleared-update") == 0)
    {
        store.clear();
        (void)store.update(handle, kBox);
    }
    else if (std::strcmp(name, "moved-setfilter") == 0)
    {
        Store target(std::move(store));
        (void)store.setFilter(handle, {});
    }
    else
    {
        return 2;
    }
    return kNotTerminated;
}
