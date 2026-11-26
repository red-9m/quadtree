// Targeted persistent regressions. Compile unchanged against old/new headers.
// Timers surround batches; setup, reservation and output are outside each batch.
#include "quadtree-index.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{
using I = QuadTree::Index<int, int>;
using QuadTree::Box;
using QuadTree::Continue;
using QuadTree::Status;
using Clock = std::chrono::steady_clock;

class CountingResource final: public QuadTree::Resource
{
public:
    std::size_t allocations = 0;
    void* allocate(std::size_t bytes, std::size_t alignment) noexcept override
    {
        ++allocations;
        return QuadTree::defaultResource()->allocate(bytes, alignment);
    }
    void deallocate(void* ptr) noexcept override { QuadTree::defaultResource()->deallocate(ptr); }
};

void require(bool ok)
{
    if (!ok)
    {
        std::fputs("index regression benchmark failed\n", stderr);
        std::exit(1);
    }
}

I create(CountingResource& resource, const Box<int>& root)
{
    auto made = I::create({.extent = QuadTree::Extent::Fixed, .root = root, .smallSetThreshold = 0}, &resource);
    require(made.ok());
    return std::move(made).value();
}

template<typename Operation>
double measure(std::size_t operations, Operation&& operation)
{
    const auto start = Clock::now();
    operation();
    const auto end = Clock::now();
    return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) /
        static_cast<double>(operations);
}

double median(std::array<double, 3> values)
{
    std::sort(values.begin(), values.end());
    return values[1];
}

void print(const char* label, const char* scene, std::size_t n, const char* operation,
    const std::array<double, 3>& samples, std::size_t allocations, const QuadTree::QueryResult& result = {})
{
    std::printf("%s,%s,%zu,%s,%.1f,%zu,%llu,%llu,%llu\n", label, scene, n, operation, median(samples), allocations,
        static_cast<unsigned long long>(result.counters.candidateTests),
        static_cast<unsigned long long>(result.counters.nodeVisits),
        static_cast<unsigned long long>(result.counters.reports));
    require(allocations == 0);
}

void coincident(const char* label, int count)
{
    CountingResource resource;
    auto tree = create(resource, {0, 0, 16384, 16384});
    const auto n = static_cast<std::size_t>(count);
    require(tree.reserve({.entries = n, .nodes = 1, .descriptors = n}) == Status::Ok);
    const Box<int> original{16, 16, 24, 24}, moved{17, 16, 24, 24};
    std::vector<I::HandleType> handles(n);
    std::array<double, 3> inserts{}, updates{};
    std::size_t insert_allocations = 0, update_allocations = 0;
    for (std::size_t round = 0; round < inserts.size(); ++round)
    {
        tree.clear();
        auto allocations = resource.allocations;
        inserts[round] = measure(n, [&]
        {
            for (int i = 0; i < count; ++i)
            {
                const auto added = tree.insert(original, int(i));
                require(added.ok());
                handles[static_cast<std::size_t>(i)] = added.handle;
            }
        });
        insert_allocations += resource.allocations - allocations;
        require(tree.size() == n && tree.stats().nodes == 1);
        allocations = resource.allocations;
        updates[round] = measure(2 * n, [&]
        {
            for (std::size_t i = 0; i < 2 * n; ++i)
                require(tree.update(handles[0], i % 2 == 0 ? moved : original) == Status::Ok);
        });
        update_allocations += resource.allocations - allocations;
        require(tree.rect(handles[0]) == original);
    }
    print(label, "coincident", n, "insert", inserts, insert_allocations);
    print(label, "coincident", n, "update", updates, update_allocations);
}

void coarse(const char* label, int large)
{
    CountingResource resource;
    auto tree = create(resource, {0, 0, 1024, 1024});
    std::vector<QuadTree::BuildEntry<int, int>> input;
    for (int i = 0; i < large; ++i)
        input.push_back({{0, 0, 1024, 1024}, i});
    for (int i = 0; i < 64; ++i)
    {
        const int x = 32 + (i % 8) * 112, y = 32 + (i / 8) * 112;
        input.push_back({{x, y, x + 8, y + 8}, large + i});
    }
    QuadTree::Workspace<int> workspace(&resource);
    require(workspace.reserve(input.size()) == Status::Ok);
    std::vector<I::HandleType> handles(input.size());
    require(tree.build(input, handles, {.workspace = &workspace}) == Status::Ok);
    const auto handle = handles[static_cast<std::size_t>(large)];
    std::array<double, 3> samples{};
    const auto allocations = resource.allocations;
    for (auto& sample : samples)
        sample = measure(8192, [&]
        {
            for (int i = 0; i < 8192; ++i)
                require(tree.update(handle, Box<int>{32 + i % 2, 32, 40, 40}) == Status::Ok);
        });
    require(tree.rect(handle).minX == 33);
    print(label, "coarse-ancestors", static_cast<std::size_t>(large), "update", samples,
        resource.allocations - allocations);
}

void orderedMoved(const char* label, int count)
{
    CountingResource resource;
    auto made = I::create({.smallSetThreshold = 0}, &resource);
    require(made.ok());
    auto tree = std::move(made).value();
    std::vector<QuadTree::BuildEntry<int, int>> input;
    for (int i = 0; i < count; ++i)
    {
        const int x = (i % 320) * 16, y = (i / 320) * 16;
        input.push_back({{x, y, x + 20, y + 20}, i, {}, std::uint64_t(count - i - 1)});
    }
    QuadTree::Workspace<int> workspace(&resource);
    require(workspace.reserve(input.size()) == Status::Ok);
    std::vector<I::HandleType> handles(input.size());
    require(tree.build(input, handles, {.workspace = &workspace}) == Status::Ok);
    const std::array moved{handles[static_cast<std::size_t>(count / 2)]};
    std::uint64_t expected = 0;
    for (bool ordered : {false, true})
    {
        const std::size_t repetitions = ordered ? 16 : 1024;
        std::array<double, 3> samples{};
        QuadTree::QueryResult result;
        std::uint64_t checksum = 0;
        const auto allocations = resource.allocations;
        for (auto& sample : samples)
            sample = measure(repetitions, [&]
            {
                for (std::size_t i = 0; i < repetitions; ++i)
                {
                    result = tree.forEachPairInvolving(moved, [&](const auto& a, const auto& b) noexcept
                    {
                        checksum += static_cast<std::uint64_t>(a.payload + b.payload);
                        return Continue;
                    }, {.ordered = ordered, .workspace = &workspace});
                    require(result.complete());
                }
            });
        require(checksum != 0);
        if (!ordered)
            expected = result.counters.reports;
        require(expected == 8 && result.counters.reports == expected);
        print(label, "single-moved", static_cast<std::size_t>(count), ordered ? "ordered" : "unordered", samples,
            resource.allocations - allocations, result);
    }
}
} // namespace

int main(int argc, char** argv)
{
    const char* label = argc > 1 ? argv[1] : "current";
    std::puts("label,scene,n,operation,median_ns_per_op,allocations,candidate_tests,node_visits,reports");
    for (int n : {1000, 4000, 16000}) coincident(label, n);
    for (int n : {0, 2000, 8000}) coarse(label, n);
    for (int n : {1000, 10000, 100000}) orderedMoved(label, n);
}
