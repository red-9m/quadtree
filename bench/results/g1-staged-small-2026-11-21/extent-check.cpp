#include "quadtree-index.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <utility>

using namespace QuadTree;
using I = Index<unsigned, int>;
using B = Box<int>;
using H = I::HandleType;

static void check(bool ok, const char* label)
{
    if (!ok)
    {
        std::cerr << label << '\n';
        std::abort();
    }
}

int main()
{
    const B world{-100, -100, 100, 100};
    for (auto extent: {Extent::Adaptive, Extent::Snapshot, Extent::Fixed})
        for (auto growth: {SnapshotGrowth::Reject, SnapshotGrowth::Adaptive})
            for (unsigned n = 1; n <= 4; ++n)
            {
                Config<int> config;
                config.extent = extent;
                config.snapshotGrowth = growth;
                config.smallSetThreshold = 4;
                if (extent != Extent::Snapshot)
                    config.root = world;
                auto made = I::create(config);
                check(made.ok(), "create");
                auto index = std::move(made).value();
                Workspace<int> workspace;
                check(index.reserve({.entries = 5, .nodes = 8}) == Status::Ok, "index reserve");
                check(workspace.reserve({0, 0, 5}) == Status::Ok, "workspace reserve");
                std::array<BuildEntry<unsigned, int>, 4> entries;
                std::array<H, 4> handles{};
                for (unsigned i = 0; i != n; ++i)
                    entries[i] = {B{0, 0, static_cast<int>(i+2), static_cast<int>(2*i+1)}, i};
                const auto input = std::span(entries).first(n);
                const B expected = extent == Extent::Fixed ? world : B{0, 0, static_cast<int>(n+1), static_cast<int>(2*n-1)};
                const auto build = [&]
                {
                    check(index.build(input, handles, {.workspace = &workspace}) == Status::Ok, "warm build");
                    check(index.extent() == expected && index.size() == n, "build extent and size");
                    for (unsigned i = 0; i < n; ++i)
                        check(index.valid(handles[i]) && index.payload(handles[i]) == i && index.rect(handles[i]) == input[i].rect,
                              "build handle and payload");
                };
                build();
                auto previous = handles;
                build();
                for (unsigned i = 0; i < n; ++i)
                    check(!index.valid(previous[i]), "old handle invalidated");
                auto invalid = entries;
                invalid[n-1].rect.minX = 12;
                previous = handles;
                check(index.build(std::span(invalid).first(n), handles, {.workspace = &workspace}) == Status::InvalidGeometry,
                      "reject inverted geometry");
                Workspace<int> insufficient;
                check(index.build(input, handles, {.workspace = &insufficient}) == Status::InsufficientWorkspace,
                      "reject insufficient workspace");
                check(index.extent() == expected && handles == previous && index.valid(handles[0]), "failed build rolled back");
                I moved(std::move(index));
                check(moved.extent() == expected && moved.valid(handles[0]) && index.empty() && !index.valid(handles[0]), "move construct");
                index = std::move(moved);
                check(index.extent() == expected && index.valid(handles[0]) && moved.empty(), "move assign");
                check(index.update(handles[n-1], B{0, 0, 1, 1}) == Status::Ok && index.extent() == expected, "update retains extent");
                build();
                check(index.insert(B{0, 0, 1, 1}, 42u).ok() && index.extent() == expected, "insert / topology transition");
                build();
                check(index.shrink() == Status::Ok && index.extent() == expected && index.valid(handles[0]), "shrink");
                check(index.reserve({.entries = 4}) == Status::Ok, "reserve after shrink");
                build();
                for (unsigned i = n; i > 0; --i)
                    check(index.remove(handles[i-1]) == Status::Ok && index.extent() == expected, "remove retains extent");
                check(index.empty(), "last remove empties");
                check(index.build(input.first(0), {.workspace = &workspace}) == Status::Ok &&
                    (extent == Extent::Fixed ? index.extent() == expected : !index.extent()), "empty build resets extent");
            }
    std::cout << "24 extent/handle cases: build, replacement failure, move, insert, update, shrink, removal and empty rebuild pass\n";
}
