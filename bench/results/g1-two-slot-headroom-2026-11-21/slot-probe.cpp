#include "quadtree-index.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>

using namespace QuadTree;
using I = Index<std::uint32_t, std::int32_t>;
using H = I::HandleType;
using B = Box<std::int32_t>;
using E = BuildEntry<std::uint32_t, std::int32_t>;

static void check(bool ok)
{
    if (!ok)
        std::abort();
}

int main(int argc, char** argv)
{
    check(argc == 2);
    const bool invalid = std::string_view(argv[1]) == "invalid-slot-probe";
    Config<std::int32_t> config;
    config.extent = Extent::Snapshot;
    config.snapshotGrowth = SnapshotGrowth::Adaptive;
    auto made = I::create(config);
    check(made.ok());
    auto index = std::move(made).value();
    Workspace<std::int32_t> scratch;
    check(scratch.reserve({0, 0, 2}) == Status::Ok);
    std::array<E, 2> source{{{B{0,0,2,2},1}, {B{1,1,3,3},2}}};
    std::array<H, 2> handles{};
    auto build = [&] { check(index.build(source, handles, {.workspace = &scratch}) == Status::Ok); };
    build(); // Force the cold general path: no entry storage has been reserved.
    for (unsigned i = 0; i < 8; ++i)
    {
        const auto old = handles;
        build();
        check(!index.valid(old[0]) && !index.valid(old[1]));
        check(index.valid(handles[0]) && index.valid(handles[1]));
        unsigned seen = 0;
        const auto result = index.forEachPair([&](const auto& a, const auto& b) noexcept
        {
            check(a.handle == handles[0] && b.handle == handles[1] && a.payload == 1 && b.payload == 2);
            ++seen;
            return Continue;
        });
        check(result.ok() && seen == 1 && result.counters.reports == 1);
    }
    check(index.remove(handles[0]) == Status::Ok);
    const auto inserted = index.insert(B{0,0,3,3}, 3u);
    check(inserted.ok() && index.valid(inserted.handle));
    build();
    if (invalid)
    {
        check(!index.valid(handles[0]));
        std::cout << "Diagnostic is invalid after mutation: successful build returns an invalid new handle.\n";
    }
    else
    {
        check(!index.valid(inserted.handle) && index.valid(handles[0]) && index.valid(handles[1]));
        check(index.payload(handles[0]) == 1 && index.payload(handles[1]) == 2);
        std::cout << "Control invalidates old handles and reconstructs valid mappings after mutation.\n";
    }
    std::cout << "Repeated cold-then-warm two-entry builds preserve exact pair handles.\n";
}
