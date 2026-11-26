// Rank-set benchmark (bench/MANIFEST.md): duplicate-rank bookkeeping of the
// entry store under sequential, random and deliberately colliding ranks, with
// the splay-tree fallback (default policy) and without it (hash only), plus
// clear() cost with a large rank reservation.
// Usage: quadtree-rank-bench [--quick] [--csv=FILE]
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "quadtree-index.h"

namespace
{
using QuadTree::Box;
using QuadTree::Status;
using QuadTree::detail::EntryStore;

struct Tag {};

struct HashOnly
{
    static constexpr bool checkHandles = false;
    static constexpr std::uint32_t rankProbeLimit = 0xffffffffu; // No fallback
};

constexpr std::uint64_t inverseOdd(std::uint64_t c)
{
    std::uint64_t x = c;
    for (int i = 0; i < 6; ++i)
        x *= 2 - c * x;
    return x;
}

// Inverse of QuadTree::detail::mix64.
constexpr std::uint64_t unmix64(std::uint64_t x)
{
    x ^= x >> 31 ^ x >> 62;
    x *= inverseOdd(0x94d049bb133111ebu);
    x ^= x >> 27 ^ x >> 54;
    x *= inverseOdd(0xbf58476d1ce4e5b9u);
    x ^= x >> 30 ^ x >> 60;
    return x;
}

static_assert(QuadTree::detail::mix64(unmix64(42)) == 42);

std::vector<std::uint64_t> makeRanks(std::string_view pattern, std::size_t n)
{
    std::vector<std::uint64_t> ranks(n);
    std::uint64_t state = 0x5eed;
    for (std::size_t k = 0; k < n; ++k)
    {
        if (pattern == "sequential")
            ranks[k] = k;
        else if (pattern == "random")
            ranks[k] = QuadTree::detail::mix64(state += 0x9e3779b97f4a7c15u);
        else // colliding: hashes share the low 32 bits, one home bucket
            ranks[k] = unmix64(std::uint64_t(k + 1) << 32);
    }
    return ranks;
}

using Clock = std::chrono::steady_clock;

double nsSince(Clock::time_point start)
{
    return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
}

struct Row
{
    std::string policy;
    std::string pattern;
    std::size_t n = 0;
    double insertNs = 0;    // Per insert (duplicate check included)
    double duplicateNs = 0; // Per rejected duplicate insert
    double removeNs = 0;    // Per remove
    double probesPerInsert = 0;
    std::uint64_t conversions = 0;
    bool tree = false;
    bool ok = true;
};

template<typename Policy>
Row measure(std::string_view policy, std::string_view pattern, std::size_t n, int rounds)
{
    using Store = EntryStore<int, int, Policy, Tag>;
    const std::vector<std::uint64_t> ranks = makeRanks(pattern, n);
    std::vector<double> insert;
    std::vector<double> duplicate;
    std::vector<double> remove;
    Row row{std::string(policy), std::string(pattern), n};
    for (int round = 0; round < rounds; ++round)
    {
        Store store;
        std::vector<typename Store::HandleType> handles(n);
        auto start = Clock::now();
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto result = store.insert(Box<int>{0, 0, 1, 1}, 0, {}, ranks[k]);
            row.ok = row.ok && result.ok();
            handles[k] = result.handle;
        }
        insert.push_back(nsSince(start) / static_cast<double>(n));
        const auto stats = store.rankStats();
        row.probesPerInsert = static_cast<double>(stats.probes) / static_cast<double>(n);
        row.conversions = stats.conversions;
        row.tree = stats.tree;

        start = Clock::now();
        for (std::size_t k = 0; k < n; ++k)
            row.ok = row.ok && store.insert(Box<int>{0, 0, 1, 1}, 0, {}, ranks[k]).status == Status::DuplicateRank;
        duplicate.push_back(nsSince(start) / static_cast<double>(n));

        start = Clock::now();
        for (std::size_t k = 0; k < n; ++k)
            row.ok = row.ok && store.remove(handles[k]) == Status::Ok;
        remove.push_back(nsSince(start) / static_cast<double>(n));
    }
    const auto median = [](std::vector<double>& values)
    {
        std::sort(values.begin(), values.end());
        return values[values.size() / 2];
    };
    row.insertNs = median(insert);
    row.duplicateNs = median(duplicate);
    row.removeNs = median(remove);
    return row;
}

// clear() with 2^20 reserved ranks and one live ranked entry.
double clearCost(int iterations)
{
    EntryStore<int, int, QuadTree::Unchecked, Tag> store;
    if (store.reserve(QuadTree::Reservation{.entries = 1u << 20, .rankedEntries = 1u << 20}) != Status::Ok)
        return -1;
    const auto start = Clock::now();
    for (int i = 0; i < iterations; ++i)
    {
        if (!store.insert(Box<int>{0, 0, 1, 1}, i, {}, 7u).ok())
            return -1;
        store.clear();
    }
    return nsSince(start) / static_cast<double>(iterations);
}
} // namespace

int main(int argc, char** argv)
{
    bool quick = false;
    std::string csv;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if (arg == "--quick")
            quick = true;
        else if (arg.substr(0, 6) == "--csv=")
            csv = arg.substr(6);
        else
        {
            std::fprintf(stderr, "usage: quadtree-rank-bench [--quick] [--csv=FILE]\n");
            return 2;
        }
    }

    const int rounds = quick ? 1 : 5;
    const std::vector<std::size_t> sizes = quick ? std::vector<std::size_t>{1000} : std::vector<std::size_t>{1000, 8000, 64000};
    std::vector<Row> rows;
    for (const std::size_t n : sizes)
        for (const char* pattern : {"sequential", "random", "colliding"})
        {
            // Hash only (no fallback) only for colliding ranks, capped at 8000
            // entries (quadratic). On ordinary ranks both policies run the same
            // code; an isolated A/B shows no difference, while within one binary
            // per-instantiation code layout shifts results by 10-30%.
            measure<QuadTree::Unchecked>("fallback", pattern, n, 1); // Warm-up
            rows.push_back(measure<QuadTree::Unchecked>("fallback", pattern, n, rounds));
            if (std::string_view(pattern) == "colliding" && n <= 8000)
                rows.push_back(measure<HashOnly>("hash-only", pattern, n, rounds));
        }

    bool ok = true;
    std::printf("%-10s %-11s %7s %11s %11s %11s %12s %6s %5s\n", "policy", "ranks", "n", "insert_ns", "dup_ns",
        "remove_ns", "probes/ins", "conv", "tree");
    for (const Row& r : rows)
    {
        ok = ok && r.ok;
        std::printf("%-10s %-11s %7zu %11.1f %11.1f %11.1f %12.1f %6llu %5s%s\n", r.policy.c_str(), r.pattern.c_str(),
            r.n, r.insertNs, r.duplicateNs, r.removeNs, r.probesPerInsert,
            static_cast<unsigned long long>(r.conversions), r.tree ? "yes" : "no", r.ok ? "" : "  FAILED");
    }
    const double clear_ns = clearCost(quick ? 1000 : 100000);
    std::printf("clear() with 2^20 reserved ranks, one live: %.1f ns\n", clear_ns);

    if (!csv.empty())
        if (std::FILE* file = std::fopen(csv.c_str(), "w"))
        {
            std::fprintf(file, "policy,ranks,n,insert_ns,duplicate_ns,remove_ns,probes_per_insert,conversions,tree\n");
            for (const Row& r : rows)
                std::fprintf(file, "%s,%s,%zu,%.1f,%.1f,%.1f,%.1f,%llu,%d\n", r.policy.c_str(), r.pattern.c_str(), r.n,
                    r.insertNs, r.duplicateNs, r.removeNs, r.probesPerInsert,
                    static_cast<unsigned long long>(r.conversions), r.tree ? 1 : 0);
            std::fprintf(file, "# clear_ns_reserved_2^20,%.1f\n", clear_ns);
            std::fclose(file);
        }
    return ok && clear_ns >= 0 ? 0 : 1;
}
