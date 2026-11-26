#pragma once

// The library under test, not a prototype. Resource allocations participate in
// the harness's existing allocation/peak-byte accounting.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "bench-memory.h"
#include "bench-persistent.h"

namespace bench
{
class IndexResource final: public QuadTree::Resource
{
public:
    void* allocate(std::size_t bytes, std::size_t alignment) noexcept override
    {
        return memory::allocateAligned(bytes, alignment);
    }
    void deallocate(void* ptr) noexcept override { memory::freeAligned(ptr); }
};

template<typename T, bool Ordered = false>
class LibraryIndex
{
    using I = QuadTree::Index<std::uint32_t, T>;
    using Handle = typename I::HandleType;

public:
    static constexpr std::string_view name = Ordered ? "index-ordered" : "index";
    static constexpr bool supportsMoved = true;

    void build(std::span<const Box<T>> boxes, Boundary boundary)
    {
        if (!mTree || mBoundary != boundary)
        {
            QuadTree::Config<T> config;
            config.extent = QuadTree::Extent::Snapshot;
            config.snapshotGrowth = QuadTree::SnapshotGrowth::Adaptive;
            config.boundary = boundary == Boundary::Touch ? QuadTree::Boundary::TouchInclusive : QuadTree::Boundary::Default;
            auto created = I::create(config, &mResource);
            require(created.status);
            mTree.emplace(std::move(created).value());
            mBoundary = boundary;
        }
        if (mPrepared < boxes.size())
        {
            require(mWorkspace.reserve({Ordered ? boxes.size() : 0, 0, boxes.size()}));
            mPrepared = boxes.size();
        }
        mInput.resize(boxes.size());
        for (std::size_t i = 0; i < boxes.size(); ++i)
        {
            const auto id = static_cast<std::uint32_t>(i);
            mInput[i] = {boxes[i], id, {}, Ordered ? std::optional<std::uint64_t>(mix64(id ^ 0x72616e6bu)) : std::nullopt};
        }
        require(mTree->build(mInput, {.workspace = &mWorkspace}));
    }

    void reset(const EngineSetup<T>& setup)
    {
        QuadTree::Config<T> config;
        config.extent = QuadTree::Extent::Fixed;
        config.root = setup.world;
        config.boundary = setup.boundary == Boundary::Touch ? QuadTree::Boundary::TouchInclusive : QuadTree::Boundary::Default;
        auto created = I::create(config, &mResource);
        require(created.status);
        mTree.emplace(std::move(created).value());
        mBoundary = setup.boundary;
        // A measured recipe, not a topology-independent guarantee. Growth is
        // still enabled and any extra allocations are visible in the results.
        require(mTree->reserve({.entries = setup.initialCount, .rankedEntries = Ordered ? setup.initialCount : 0,
            .nodes = std::max<std::size_t>(1, setup.initialCount), .descriptors = setup.initialCount}));
        require(mWorkspace.reserve({setup.initialCount, setup.initialCount, 0}));
        mHandles.resize(setup.idCapacity);
        mMoved.reserve(setup.initialCount);
    }

    void insert(std::uint32_t id, const Box<T>& box)
    {
        const auto result = mTree->insert(box, std::uint32_t(id), {},
            Ordered ? std::optional<std::uint64_t>(mix64(id ^ 0x72616e6bu)) : std::nullopt);
        require(result.status);
        mHandles[id] = result.handle;
    }
    void remove(std::uint32_t id) { require(mTree->remove(mHandles[id])); }
    void move(std::uint32_t id, const Box<T>& box) { require(mTree->update(mHandles[id], box)); }
    void finishUpdates() noexcept {} // Mutation eagerly refits only the affected paths.

    template<typename Sink>
    Counters pairs(Sink& sink, std::uint64_t limit) const
    {
        const auto result = mTree->forEachPair([&](const auto& a, const auto& b) noexcept
            { sink(a.payload, b.payload); return QuadTree::Continue; },
            {.ordered = Ordered, .workspace = &mWorkspace, .budget = {.candidateTests = limit}});
        return counters(result);
    }

    template<typename Sink>
    Counters pairsInvolving(std::span<const std::uint32_t> changed, Sink& sink, std::uint64_t limit)
    {
        mMoved.resize(changed.size());
        for (std::size_t i = 0; i < changed.size(); ++i)
            mMoved[i] = mHandles[changed[i]];
        const auto result = mTree->forEachPairInvolving(mMoved, [&](const auto& a, const auto& b) noexcept
            { sink(a.payload, b.payload); return QuadTree::Continue; },
            {.ordered = Ordered, .workspace = &mWorkspace, .budget = {.candidateTests = limit}});
        return counters(result);
    }

private:
    IndexResource mResource;
    std::optional<I> mTree;
    mutable QuadTree::Workspace<T> mWorkspace{&mResource};
    std::vector<QuadTree::BuildEntry<std::uint32_t, T>> mInput;
    std::vector<Handle> mHandles, mMoved;
    Boundary mBoundary = Boundary::Default;
    std::size_t mPrepared = 0;

    static void require(QuadTree::Status status)
    {
        if (status != QuadTree::Status::Ok)
        {
            std::fprintf(stderr, "library Index failed: status %u\n", static_cast<unsigned>(status));
            std::abort();
        }
    }

    Counters counters(const QuadTree::QueryResult& result) const
    {
        if (result.status != QuadTree::Status::BudgetExhausted)
            require(result.status);
        Counters output;
        output.candidates = result.counters.candidateTests;
        output.nodeVisits = result.counters.nodeVisits;
        output.references = mTree->size();
        output.maxReferences = mTree->empty() ? 0u : 1u;
        output.capped = result.status == QuadTree::Status::BudgetExhausted;
        return output;
    }
};
} // namespace bench
