// Broad-phase benchmark (bench/MANIFEST.md).
// Snapshot model: build + all pairs per step.
// Persistent model: insert once, then per step a workload update plus all
// pairs or the pairs involving the changed set.
// Exit code: 0 complete, 1 engine results disagree or are nondeterministic,
// 2 invalid arguments.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#if defined(_MSC_VER)
#include <malloc.h>
#endif
#if defined(__x86_64__) || defined(_M_X64)
#define QT_BENCH_TSC 1
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#include <x86intrin.h>
#endif
#endif

#include "bench-build-info.h"
#include "bench-engines.h"
#include "bench-memory.h"
#include "bench-index.h"
#include "bench-persistent.h"
#include "bench-persistent-loose.h"
#include "bench-prototypes.h"

#if defined(QT_BENCH_HAVE_BOX2D)
#include "bench-box2d.h"
#endif

namespace
{
using namespace bench;

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

struct Options
{
    bool snapshot = true;
    bool persistent = false;
    std::vector<std::string> types{"int32", "float"};
    std::vector<std::string> scenes;    // Empty: all
    std::vector<std::string> engines;   // Empty: all
    std::vector<std::string> workloads; // Empty: all
    std::vector<std::string> modes;     // Empty: all
    std::vector<std::size_t> sizes{100, 1000, 10000, 100000};
    std::vector<std::size_t> smallSizes{0, 1, 2, 4, 8, 16, 32, 64};
    std::vector<std::size_t> persistentSizes{1000, 10000, 100000};
    std::size_t batch = 1024;                 // Scenes per sample for small sizes
    std::size_t steps = 64;                   // Persistent steps per pass
    std::size_t rounds = 5;                   // Interleaved rounds; error estimates need >= 2
    double minTime = 0.5;                     // Seconds per engine, split across rounds
    double maxTime = 4.0;                     // Also the persistent pass budget, split across rounds
    std::size_t minReps = 3;                  // Per round
    std::size_t maxReps = 1000;               // Per engine, split across rounds
    std::uint64_t candidateLimit = 200000000; // Per step; larger steps are reported as capped
    double processError = 0.02;               // Between-process relative error (95%), snapshot model, calibrated
    double processErrorPersistent = 0.03;     // Same for the persistent model, calibrated
    std::uint64_t seed = 1;
    bool gate = false;                   // Evaluate the frozen limits after the run
    std::vector<std::string> gateFiles;  // --gate-files: evaluate published CSVs without running
    std::string gateSnapshot = "index";
    std::string gatePersistent = "index";
    std::string gateOrdered = "index-ordered";
    std::string gateOrderedBase = "index";
    std::string compare; // Previous CSV to compare against
    std::string diffOld; // --diff: compare two CSVs without running
    std::string diffNew;
    std::string csv;
    std::string raw;
    std::string args;
};

constexpr std::string_view kEngineNames[] = {"brute", "sweep", "grid", "grid-rebuild", "b2tree", "b2tree-fat",
    "qt-classic", "qt-loose", "qt-loose-sort", "qt-refs", "qt-refs-ordered", "grid-ordered", "index", "index-ordered"};

std::vector<std::string> splitList(std::string_view text)
{
    std::vector<std::string> items;
    while (!text.empty())
    {
        const std::size_t comma = text.find(',');
        items.emplace_back(text.substr(0, comma));
        if (comma == std::string_view::npos)
            break;
        text.remove_prefix(comma + 1);
    }
    return items;
}

std::vector<std::size_t> splitSizes(std::string_view text)
{
    std::vector<std::size_t> sizes;
    for (const std::string& item : splitList(text))
        sizes.push_back(static_cast<std::size_t>(std::strtoull(item.c_str(), nullptr, 10)));
    return sizes;
}

template<typename List>
bool selected(const List& list, std::string_view name)
{
    return list.empty() || std::find(list.begin(), list.end(), name) != list.end();
}

void usage()
{
    std::printf(
        "usage: quadtree-bench [options]\n"
        "  --model=M             snapshot, persistent or all (default snapshot)\n"
        "  --quick               smoke run of both models: few sizes and steps, all types\n"
        "  --types=LIST          int32,int64,float,double or all (default int32,float)\n"
        "  --scenes=LIST         scene names (default all; --list shows them)\n"
        "  --engines=LIST        brute,sweep,grid,grid-rebuild,b2tree,b2tree-fat,qt-classic,qt-loose,\n"
        "                        qt-loose-sort,qt-refs,\n"
        "                        qt-refs-ordered,grid-ordered,index,index-ordered (default all)\n"
        "  --leaf-capacity=N --max-depth=N   quadtree prototypes (default 8, 24)\n"
        "  --sizes=LIST          snapshot entry counts (default 100,1000,10000,100000)\n"
        "  --small-sizes=LIST    batched snapshot sizes (default 0,1,2,4,8,16,32,64; empty to skip)\n"
        "  --batch=N             scenes per small-size sample (default 1024)\n"
        "  --persistent-sizes=LIST  persistent entry counts (default 1000,10000,100000)\n"
        "  --workloads=LIST      persistent workloads (default all; --list shows them)\n"
        "  --modes=LIST          full,moved (default both)\n"
        "  --steps=N             persistent steps per pass (default 64)\n"
        "  --rounds=N            interleaved rounds for error estimates (default 5)\n"
        "  --min-time=S --max-time=S   seconds per engine (default 0.5, 4)\n"
        "  --min-reps=N          snapshot warm repetitions per round (default 3)\n"
        "  --max-reps=N          snapshot warm repetitions per engine (default 1000)\n"
        "  --candidate-limit=N   candidate cap per step (default 2e8)\n"
        "  --process-error=F     between-process relative 95%% error added in quadrature (default 0.02)\n"
        "  --process-error-persistent=F   same for the persistent model (default 0.03)\n"
        "  --seed=N              fixture base seed (default 1)\n"
        "  --compare=FILE        compare this run with a previous --csv file\n"
        "  --diff=OLD,NEW        compare two --csv files without running\n"
        "  --gate                evaluate the frozen acceptance limits (bench/MANIFEST.md) after the run\n"
        "  --gate-files=LIST     evaluate them on --csv files without running\n"
        "  --gate-snapshot=E --gate-persistent=E --gate-ordered=E:BASE   candidate engines\n"
        "                        (default index, index, index-ordered:index)\n"
        "  --csv=FILE --raw=FILE results and per-repetition samples\n"
        "  --list                list scenes and workloads\n");
}

bool parse(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        options.args += (i > 1 ? " " : "") + std::string(arg);
        const std::size_t eq = arg.find('=');
        const std::string_view key = arg.substr(0, eq);
        const std::string_view value = eq == std::string_view::npos ? std::string_view{} : arg.substr(eq + 1);
        const auto number = [&] { return std::strtod(std::string(value).c_str(), nullptr); };

        if (key == "--quick")
        {
            options.snapshot = true;
            options.persistent = true;
            options.types = {"int32", "int64", "float", "double"};
            options.sizes = {100, 1000};
            options.smallSizes = {0, 1, 2, 8, 64};
            options.persistentSizes = {100, 1000};
            options.batch = 16;
            options.steps = 4;
            options.rounds = 2;
            options.minTime = 0;
            options.minReps = 1;
            options.maxReps = 2;
        }
        else if (key == "--model")
        {
            options.snapshot = value == "snapshot" || value == "all";
            options.persistent = value == "persistent" || value == "all";
            if (!options.snapshot && !options.persistent)
            {
                std::fprintf(stderr, "unknown model: %s\n", std::string(value).c_str());
                return false;
            }
        }
        else if (key == "--types")
            options.types = value == "all" ? std::vector<std::string>{"int32", "int64", "float", "double"}
                                           : splitList(value);
        else if (key == "--scenes")
            options.scenes = splitList(value);
        else if (key == "--engines")
            options.engines = splitList(value);
        else if (key == "--workloads")
            options.workloads = splitList(value);
        else if (key == "--modes")
            options.modes = splitList(value);
        else if (key == "--sizes")
            options.sizes = splitSizes(value);
        else if (key == "--small-sizes")
            options.smallSizes = splitSizes(value);
        else if (key == "--persistent-sizes")
            options.persistentSizes = splitSizes(value);
        else if (key == "--batch")
            options.batch = std::max<std::size_t>(1, static_cast<std::size_t>(number()));
        else if (key == "--steps")
            options.steps = std::max<std::size_t>(1, static_cast<std::size_t>(number()));
        else if (key == "--rounds")
            options.rounds = std::max<std::size_t>(1, static_cast<std::size_t>(number()));
        else if (key == "--min-time")
            options.minTime = number();
        else if (key == "--max-time")
            options.maxTime = number();
        else if (key == "--min-reps")
            options.minReps = std::max<std::size_t>(1, static_cast<std::size_t>(number()));
        else if (key == "--max-reps")
            options.maxReps = std::max<std::size_t>(1, static_cast<std::size_t>(number()));
        else if (key == "--leaf-capacity")
            PrototypeConfig::leafCapacity = static_cast<std::uint32_t>(number());
        else if (key == "--max-depth")
            PrototypeConfig::maxDepth = static_cast<std::uint32_t>(number());
        else if (key == "--candidate-limit")
            options.candidateLimit = static_cast<std::uint64_t>(number());
        else if (key == "--process-error")
            options.processError = std::max(0.0, number());
        else if (key == "--process-error-persistent")
            options.processErrorPersistent = std::max(0.0, number());
        else if (key == "--compare")
            options.compare = value;
        else if (key == "--diff")
        {
            const auto files = splitList(value);
            if (files.size() != 2)
            {
                std::fprintf(stderr, "--diff needs OLD,NEW\n");
                return false;
            }
            options.diffOld = files[0];
            options.diffNew = files[1];
        }
        else if (key == "--gate")
            options.gate = true;
        else if (key == "--gate-files")
            options.gateFiles = splitList(value);
        else if (key == "--gate-snapshot")
            options.gateSnapshot = value;
        else if (key == "--gate-persistent")
            options.gatePersistent = value;
        else if (key == "--gate-ordered")
        {
            const std::size_t colon = value.find(':');
            if (colon == std::string_view::npos)
            {
                std::fprintf(stderr, "--gate-ordered needs ENGINE:BASE\n");
                return false;
            }
            options.gateOrdered = value.substr(0, colon);
            options.gateOrderedBase = value.substr(colon + 1);
        }
        else if (key == "--seed")
            options.seed = std::strtoull(std::string(value).c_str(), nullptr, 0);
        else if (key == "--csv")
            options.csv = value;
        else if (key == "--raw")
            options.raw = value;
        else if (key == "--list")
        {
            std::printf("snapshot scenes:\n");
            for (const SceneInfo& scene : kScenes)
                std::printf("  %-22s %s\n", std::string(scene.name).c_str(), std::string(scene.description).c_str());
            std::printf("persistent scenes:");
            for (const std::string_view scene : kPersistentScenes)
                std::printf(" %s", std::string(scene).c_str());
            std::printf("\npersistent workloads:");
            for (const Workload& workload : kWorkloads)
                std::printf(" %s", std::string(workload.name).c_str());
            std::printf("\n");
            std::exit(0);
        }
        else
        {
            usage();
            return false;
        }
    }

    for (const std::string& scene : options.scenes)
        if (!findScene(scene))
        {
            std::fprintf(stderr, "unknown scene: %s\n", scene.c_str());
            return false;
        }
    for (const std::string& type : options.types)
        if (type != "int32" && type != "int64" && type != "float" && type != "double")
        {
            std::fprintf(stderr, "unknown type: %s\n", type.c_str());
            return false;
        }
    for (const std::string& engine : options.engines)
        if (std::find(std::begin(kEngineNames), std::end(kEngineNames), engine) == std::end(kEngineNames))
        {
            std::fprintf(stderr, "unknown engine: %s\n", engine.c_str());
            return false;
        }
    for (const std::string& name : options.workloads)
        if (std::none_of(std::begin(kWorkloads), std::end(kWorkloads),
                [&](const Workload& workload) { return workload.name == name; }))
        {
            std::fprintf(stderr, "unknown workload: %s\n", name.c_str());
            return false;
        }
    for (const std::string& mode : options.modes)
        if (mode != "full" && mode != "moved")
        {
            std::fprintf(stderr, "unknown mode: %s\n", mode.c_str());
            return false;
        }
    return true;
}

// ---------------------------------------------------------------------------
// Measurement primitives
// ---------------------------------------------------------------------------

// Step timer. On x86-64 with an invariant TSC: rdtsc (about 8 ns per read),
// calibrated against steady_clock. Otherwise steady_clock. Kernel clock
// sources such as HPET cost about 1 us per read and would dominate small steps.
namespace timer
{
bool gTsc = false;
double gNsPerTick = 1;
double gReadNs = 0;

bool invariantTsc()
{
#if defined(QT_BENCH_TSC) && defined(_MSC_VER)
    int registers[4] = {};
    __cpuid(registers, static_cast<int>(0x80000000u));
    if (static_cast<unsigned>(registers[0]) < 0x80000007u)
        return false;
    __cpuid(registers, static_cast<int>(0x80000007u));
    return (registers[3] >> 8 & 1) != 0;
#elif defined(QT_BENCH_TSC)
    unsigned a = 0, b = 0, c = 0, d = 0;
    return __get_cpuid(0x80000007u, &a, &b, &c, &d) != 0 && (d >> 8 & 1) != 0;
#else
    return false;
#endif
}

std::uint64_t steadyNs()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

inline std::uint64_t now() noexcept
{
#if defined(QT_BENCH_TSC)
    if (gTsc)
        return __rdtsc();
#endif
    return steadyNs();
}

// Selects the timer; with a TSC, measures its rate over 200 ms of steady_clock.
void calibrate()
{
    gTsc = invariantTsc();
    if (gTsc)
    {
        const std::uint64_t ns0 = steadyNs();
        const std::uint64_t tick0 = now();
        while (steadyNs() - ns0 < 200000000u)
        {
        }
        const std::uint64_t tick1 = now();
        const std::uint64_t ns1 = steadyNs();
        gNsPerTick = static_cast<double>(ns1 - ns0) / static_cast<double>(tick1 - tick0);
    }
    const std::uint64_t start = now();
    for (int i = 0; i < 100000; ++i)
        static_cast<void>(now());
    gReadNs = static_cast<double>(now() - start) * gNsPerTick / 100000;
}

std::string describe()
{
    char text[96];
    if (gTsc)
        std::snprintf(text, sizeof(text), "rdtsc %.6f GHz, %.1f ns per read", 1 / gNsPerTick, gReadNs);
    else
        std::snprintf(text, sizeof(text), "steady_clock, %.1f ns per read", gReadNs);
    return text;
}
} // namespace timer

using Tick = std::uint64_t;

Tick now() noexcept
{
    return timer::now();
}

std::int64_t elapsedNs(Tick from, Tick to) noexcept
{
    return static_cast<std::int64_t>(static_cast<double>(to - from) * timer::gNsPerTick);
}

double elapsedSeconds(Tick from, Tick to) noexcept
{
    return static_cast<double>(elapsedNs(from, to)) * 1e-9;
}

// Order-independent pair-set checksum; duplicates change it.
struct PairSink
{
    std::uint64_t pairs = 0;
    std::uint64_t checksum = 0;

    void operator()(std::uint32_t a, std::uint32_t b) noexcept
    {
        const std::uint64_t lo = std::min(a, b);
        const std::uint64_t hi = std::max(a, b);
        ++pairs;
        checksum += mix64((lo << 32) | hi);
    }
};

struct Sample
{
    std::int64_t totalNs = 0;
    std::int64_t prepareNs = -1; // Snapshot: build (unbatched only); persistent: update
    std::int64_t pairsNs = -1;
    std::uint64_t candidates = 0;
    std::uint64_t nodeVisits = 0;
    std::uint64_t cellTests = 0;
    std::uint64_t references = 0;
    std::uint32_t maxReferences = 0;
    std::uint64_t orderViolations = 0;
    std::uint64_t pairs = 0;
    std::uint64_t checksum = 0;
    std::size_t allocations = 0; // Engine allocations during the step
    std::size_t round = 0;
    bool capped = false;
};

// Point estimate with a 95% confidence half-width; error < 0 when unknown.
struct Interval
{
    double estimate = -1;
    double error = -1;
};

struct Result
{
    std::string model;
    std::string scene;
    std::string boundary;
    std::string type;
    std::string workload = "-";
    std::string mode = "full";
    std::size_t entries = 0;
    std::size_t batch = 1;
    std::string engine;
    std::string status = "ok";
    std::vector<Sample> samples;
    std::vector<double> roundMedians; // Per-scene step ns, one per round
    std::vector<double> initNs;       // Persistent initial insertion, one per round
    Interval time;                    // Mean of round medians; error includes the process term
    Interval init;
    double roundError = -1; // Round-to-round part of the time error
    double vsBest = -1;     // Paired ratio to the fastest engine
    double vsBestLo = -1;
    double vsBestHi = -1;
    Sample cold; // Snapshot: first cold step; persistent: totals of the first pass
    std::size_t passSteps = 0; // Persistent steps per pass, set by the first pass
    std::size_t warmAllocations = 0;
    double allocationsPerStep = 0;
    std::size_t peakBytes = 0;
    std::size_t retainedBytes = 0;
    std::uint64_t sceneHash = 0;
};

// Two-sided 95% Student t quantile.
double tQuantile95(std::size_t dof)
{
    static constexpr double table[] = {0, 12.706, 4.303, 3.182, 2.776, 2.571, 2.447, 2.365, 2.306, 2.262, 2.228,
        2.201, 2.179, 2.160, 2.145, 2.131, 2.120, 2.110, 2.101, 2.093, 2.086, 2.080, 2.074, 2.069, 2.064, 2.060,
        2.056, 2.052, 2.048, 2.045, 2.042};
    return dof < std::size(table) ? table[dof] : 1.96;
}

Interval meanInterval(const std::vector<double>& values)
{
    Interval interval;
    if (values.empty())
        return interval;

    double sum = 0;
    for (const double value : values)
        sum += value;
    const auto count = static_cast<double>(values.size());
    interval.estimate = sum / count;
    if (values.size() < 2)
        return interval;

    double squares = 0;
    for (const double value : values)
        squares += (value - interval.estimate) * (value - interval.estimate);
    interval.error = tQuantile95(values.size() - 1) * std::sqrt(squares / (count - 1) / count);
    return interval;
}

double median(std::vector<std::int64_t> values)
{
    std::sort(values.begin(), values.end());
    const std::size_t mid = values.size() / 2;
    return values.size() % 2 != 0 ? static_cast<double>(values[mid])
                                  : (static_cast<double>(values[mid - 1]) + static_cast<double>(values[mid])) / 2;
}

// Nearest-rank percentile.
std::int64_t percentile(std::vector<std::int64_t> values, double p)
{
    if (values.empty())
        return -1;
    std::sort(values.begin(), values.end());
    std::size_t rank = static_cast<std::size_t>(p * static_cast<double>(values.size()) + 0.999999);
    rank = std::clamp<std::size_t>(rank, 1, values.size());
    return values[rank - 1];
}

std::int64_t perScene(std::int64_t ns, std::size_t batch)
{
    return ns < 0 ? -1 : ns / static_cast<std::int64_t>(batch);
}

std::vector<std::int64_t> column(const Result& result, std::int64_t Sample::*field)
{
    std::vector<std::int64_t> values;
    for (const Sample& sample : result.samples)
        values.push_back(perScene(sample.*field, result.batch));
    return values;
}

bool sameCounts(const Sample& a, const Sample& b)
{
    return a.pairs == b.pairs && a.checksum == b.checksum && a.candidates == b.candidates;
}

// Calls fn with the stack start moved down by offset bytes. Not inlinable
// (alloca), so fn's frames really move.
template<typename F>
void withStackOffset(std::size_t offset, F&& fn)
{
#if defined(_MSC_VER)
    auto* shift = static_cast<volatile unsigned char*>(_alloca(offset + 1));
#else
    auto* shift = static_cast<volatile unsigned char*>(__builtin_alloca(offset + 1));
#endif
    shift[0] = 0;
    shift[offset] = 0;
    fn();
}

// Heap and stack placement of round `round` for engine `engine`: 64-byte heap
// steps up to 64 KiB, 16-byte stack steps up to 4 KiB.
std::uint64_t layoutSeed(std::size_t round, std::size_t engines, std::size_t engine)
{
    return mix64(round * engines + engine);
}

bool timed(const Result& result)
{
    return result.status == "ok" || result.status == "time-capped";
}

// Estimates, paired ratios to the fastest engine, and the pair-set cross-check
// over the engines of one scene/type/size (and workload/mode). Time-capped
// persistent engines are ranked like complete ones; they ran a prefix of the
// same step sequence.
void finishResults(std::span<Result> results, const Options& options)
{
    const double process_error =
        !results.empty() && results[0].model == "persistent" ? options.processErrorPersistent : options.processError;
    const Result* best = nullptr;
    for (Result& result : results)
    {
        if (result.roundMedians.empty())
            continue;
        result.time = meanInterval(result.roundMedians);
        result.roundError = result.time.error;
        result.time.error = std::hypot(std::max(result.time.error, 0.0), process_error * result.time.estimate);
        result.init = meanInterval(result.initNs);
        result.allocationsPerStep = static_cast<double>(result.warmAllocations) /
            static_cast<double>(result.samples.size() * result.batch);
        if (timed(result) && (!best || result.time.estimate < best->time.estimate))
            best = &result;
    }

    // Paired per-round ratios cancel drift shared by engines in a round.
    for (Result& result : results)
    {
        if (!best || !timed(result) || result.roundMedians.size() != best->roundMedians.size())
            continue;
        std::vector<double> logs;
        for (std::size_t r = 0; r < result.roundMedians.size(); ++r)
            logs.push_back(std::log(std::max(result.roundMedians[r], 1.0) / std::max(best->roundMedians[r], 1.0)));
        const Interval ratio = meanInterval(logs);
        const double error = &result == best ? 0 : std::hypot(std::max(ratio.error, 0.0), process_error);
        result.vsBest = std::exp(ratio.estimate);
        result.vsBestLo = std::exp(ratio.estimate - error);
        result.vsBestHi = std::exp(ratio.estimate + error);
    }

    // Engines must agree on the pair multiset: snapshot per cold step,
    // persistent per step of the first pass over the steps all engines ran.
    std::size_t common_steps = ~std::size_t(0);
    for (const Result& result : results)
        if (timed(result))
            common_steps = std::min(common_steps, result.passSteps);
    const Result* reference = nullptr;
    for (Result& result : results)
    {
        if (!timed(result))
            continue;
        if (!reference)
        {
            reference = &result;
            continue;
        }
        bool same = true;
        if (result.model == "persistent")
        {
            for (std::size_t i = 0; i < common_steps; ++i)
                same = same && result.samples[i].pairs == reference->samples[i].pairs &&
                    result.samples[i].checksum == reference->samples[i].checksum;
        }
        else
            same = result.cold.pairs == reference->cold.pairs && result.cold.checksum == reference->cold.checksum;
        if (!same)
            result.status = "MISMATCH";
    }
}

// ---------------------------------------------------------------------------
// Snapshot model
// ---------------------------------------------------------------------------

template<typename T>
struct Fixture
{
    std::vector<std::vector<Box<T>>> scenes; // One per batch member
    std::size_t entries = 0;                 // Per scene
    std::uint64_t hash = 0;
};

template<typename Engine, typename T>
Sample runStep(Engine& engine, const Fixture<T>& fixture, Boundary boundary, std::uint64_t limit)
{
    Sample sample;
    const bool split = fixture.scenes.size() == 1;
    const std::size_t allocations = memory::allocations();
    const auto start = now();
    for (std::size_t k = 0; k < fixture.scenes.size(); ++k)
    {
        PairSink sink;
        engine.build(fixture.scenes[k], boundary);
        const auto built = split ? now() : start;
        const Counters counters = engine.pairs(sink, limit);
        if (split)
        {
            sample.prepareNs = elapsedNs(start, built);
            sample.pairsNs = elapsedNs(built, now());
        }
        sample.candidates += counters.candidates;
        sample.nodeVisits += counters.nodeVisits;
        sample.cellTests += counters.cellTests;
        sample.references += counters.references;
        sample.maxReferences = std::max(sample.maxReferences, counters.maxReferences);
        sample.orderViolations += counters.orderViolations;
        sample.pairs += sink.pairs;
        sample.checksum += mix64(sink.checksum + k);
        sample.capped = sample.capped || counters.capped;
    }
    sample.totalNs = elapsedNs(start, now());
    sample.allocations = memory::allocations() - allocations;
    return sample;
}

template<typename T>
class EngineRunner
{
public:
    virtual ~EngineRunner() = default;
    virtual void reset() = 0; // Fresh engine instance
    virtual Sample step(const Fixture<T>& fixture, Boundary boundary, std::uint64_t limit) = 0;
};

template<typename Engine, typename T>
class Runner final: public EngineRunner<T>
{
public:
    void reset() override { mEngine = std::make_unique<Engine>(); }

    Sample step(const Fixture<T>& fixture, Boundary boundary, std::uint64_t limit) override
    {
        return runStep(*mEngine, fixture, boundary, limit);
    }

private:
    std::unique_ptr<Engine> mEngine;
};

// One round of one engine: cold step on a fresh instance, then warm steps.
template<typename T>
void runRound(EngineRunner<T>& runner, const Fixture<T>& fixture, Boundary boundary, const Options& options,
    std::size_t round, Result& result)
{
    const auto rounds = static_cast<double>(options.rounds);
    const double min_time = options.minTime / rounds;
    const double max_time = options.maxTime / rounds;
    const std::size_t max_reps = std::max<std::size_t>(1, options.maxReps / options.rounds);

    runner.reset();
    const std::size_t live = memory::liveBytes();
    memory::resetPeak();
    const Sample cold = runner.step(fixture, boundary, options.candidateLimit);
    if (cold.orderViolations != 0)
        result.status = "UNORDERED";
    if (round == 0)
    {
        result.cold = cold;
        result.peakBytes = memory::peakBytes() - live;
        result.retainedBytes = memory::liveBytes() - live;
        if (cold.capped)
        {
            result.status = "capped";
            return;
        }
    }
    else if (!sameCounts(cold, result.cold))
        result.status = "NONDETERMINISTIC";

    // Warm steps reuse retained capacity.
    std::vector<std::int64_t> times;
    const auto start = now();
    while (times.size() < max_reps)
    {
        Sample sample = runner.step(fixture, boundary, options.candidateLimit);
        sample.round = round;
        if (!sameCounts(sample, result.cold))
            result.status = "NONDETERMINISTIC";
        result.warmAllocations += sample.allocations;
        times.push_back(sample.totalNs);
        result.samples.push_back(sample);

        const double elapsed = elapsedSeconds(start, now());
        if ((times.size() >= options.minReps && elapsed >= min_time) || elapsed >= max_time)
            break;
    }
    result.roundMedians.push_back(median(times) / static_cast<double>(result.batch));
}

// Runs all engines in interleaved rounds, in rotated order. Each round gives
// every engine a fresh instance and its own fixture copy at a shifted heap
// position, with a shifted stack start. Time drift and memory-layout effects
// (which differ between processes) thus show up as round-to-round variation.
// The first cold step supplies memory and count metrics.
template<typename T>
void measureRounds(const Fixture<T>& fixture, Boundary boundary, const Options& options,
    std::vector<std::unique_ptr<EngineRunner<T>>>& runners, std::span<Result> results)
{
    const std::size_t engines = runners.size();
    for (std::size_t round = 0; round < options.rounds; ++round)
    {
        for (std::size_t k = 0; k < engines; ++k)
        {
            const std::size_t e = (k + round) % engines;
            if (results[e].status == "capped")
                continue;

            const std::uint64_t layout = layoutSeed(round, engines, e);
            std::vector<unsigned char> pad(64 * (1 + layout % 1024));
            const Fixture<T> copy = fixture;
            withStackOffset(16 * ((layout >> 32) % 256),
                [&] { runRound(*runners[e], copy, boundary, options, round, results[e]); });
        }
    }
    finishResults(results, options);
}

template<typename T>
Fixture<T> makeFixture(const SceneInfo& scene, std::size_t n, std::size_t batch, std::uint64_t seed)
{
    Fixture<T> fixture;
    fixture.entries = n;
    fixture.hash = n;
    for (std::size_t k = 0; k < batch; ++k)
    {
        const auto boxes = generate(scene.name, n, sceneSeed(seed, scene.name, n, k));
        fixture.hash = mix64(fixture.hash ^ sceneHash(boxes));
        fixture.scenes.push_back(convert<T>(boxes));
    }
    return fixture;
}

Result makeResult(std::string_view model, const SceneInfo& scene, std::string_view type, std::size_t n,
    std::size_t batch, std::string_view engine, std::uint64_t hash)
{
    Result result;
    result.model = model;
    result.scene = scene.name;
    result.boundary = scene.boundary == Boundary::Touch ? "touch" : "default";
    result.type = type;
    result.entries = n;
    result.batch = batch;
    result.engine = engine;
    result.sceneHash = hash;
    return result;
}

template<typename Engine, typename T>
void addEngine(const SceneInfo& scene, std::string_view type, const Fixture<T>& fixture, const Options& options,
    std::vector<std::unique_ptr<EngineRunner<T>>>& runners, std::vector<Result>& results)
{
    if (!selected(options.engines, Engine::name))
        return;
    runners.push_back(std::make_unique<Runner<Engine, T>>());
    results.push_back(
        makeResult("snapshot", scene, type, fixture.entries, fixture.scenes.size(), Engine::name, fixture.hash));
}

template<typename T>
void runSnapshotSize(const SceneInfo& scene, std::string_view type, std::size_t n, std::size_t batch,
    const Options& options, std::vector<Result>& results)
{
    if (n > scene.maxEntries)
    {
        Result capped = makeResult("snapshot", scene, type, n, batch, "*", 0);
        capped.status = "capped";
        results.push_back(capped);
        return;
    }

    const std::size_t first = results.size();
    const Fixture<T> fixture = makeFixture<T>(scene, n, batch, options.seed);
    std::vector<std::unique_ptr<EngineRunner<T>>> runners;
    addEngine<BruteForce<T>>(scene, type, fixture, options, runners, results);
    addEngine<SweepAndPrune<T>>(scene, type, fixture, options, runners, results);
    addEngine<UniformGrid<T>>(scene, type, fixture, options, runners, results);
    addEngine<RegionQuadtree<T, false>>(scene, type, fixture, options, runners, results);
    addEngine<LooseQuadtree<T>>(scene, type, fixture, options, runners, results);
    addEngine<LooseSortedQuadtree<T>>(scene, type, fixture, options, runners, results);
    addEngine<LibraryIndex<T>>(scene, type, fixture, options, runners, results);
    addEngine<LibraryIndex<T, true>>(scene, type, fixture, options, runners, results);
    addEngine<RegionQuadtree<T, true>>(scene, type, fixture, options, runners, results);
    addEngine<OrderedEngine<T, OrderedRefs<T>>>(scene, type, fixture, options, runners, results);
    addEngine<OrderedEngine<T, OrderedGrid<T>>>(scene, type, fixture, options, runners, results);
    measureRounds(fixture, scene.boundary, options, runners, std::span<Result>(results).subspan(first));
}

template<typename T>
void runSnapshot(std::string_view type, const Options& options, std::vector<Result>& results)
{
    for (const SceneInfo& scene : kScenes)
    {
        if (!selected(options.scenes, scene.name))
            continue;
        if (scene.smallSizes)
            for (const std::size_t n : options.smallSizes)
                runSnapshotSize<T>(scene, type, n, options.batch, options, results);
        for (const std::size_t n : options.sizes)
            runSnapshotSize<T>(scene, type, n, 1, options, results);
    }
}

// ---------------------------------------------------------------------------
// Persistent model
// ---------------------------------------------------------------------------

template<typename T>
struct PersistentFixture
{
    std::vector<IBox> initial;
    std::vector<Box<T>> boxes;
    IBox world{};
    EngineSetup<T> setup;
    std::uint64_t seed = 0;
    std::uint64_t hash = 0;
};

// One pass: fresh engine, initial insertion, then all steps of the workload.
struct PassResult
{
    std::vector<Sample> steps;
    Sample totals; // Candidates, node visits, pairs, checksum over the pass
    std::int64_t initNs = 0;
    std::size_t peakBytes = 0;
    std::size_t retainedBytes = 0; // After initial insertion
    bool timeCapped = false;
};

template<typename T>
class PersistentRunnerBase
{
public:
    virtual ~PersistentRunnerBase() = default;
    // Runs up to `steps` steps; stops after `budget` seconds when budget > 0.
    virtual PassResult pass(const std::vector<Box<T>>& initial, MotionScript<T>& script, const EngineSetup<T>& setup,
        PairMode mode, const Options& options, std::size_t steps, double budget, std::vector<Update<T>>& updates,
        std::vector<std::uint32_t>& changed) = 0;
};

template<typename Engine, typename T>
class PersistentRunner final: public PersistentRunnerBase<T>
{
public:
    PassResult pass(const std::vector<Box<T>>& initial, MotionScript<T>& script, const EngineSetup<T>& setup,
        PairMode mode, const Options& options, std::size_t steps, double budget, std::vector<Update<T>>& updates,
        std::vector<std::uint32_t>& changed) override
    {
        PassResult result;
        result.steps.reserve(steps);
        script.restart();
        const std::uint64_t limit = options.candidateLimit;

        const std::size_t live = memory::liveBytes();
        memory::resetPeak();
        auto engine = std::make_unique<Engine>();
        const auto init_start = now();
        engine->reset(setup);
        for (std::size_t id = 0; id < initial.size(); ++id)
            engine->insert(static_cast<std::uint32_t>(id), initial[id]);
        engine->finishUpdates();
        result.initNs = elapsedNs(init_start, now());
        result.retainedBytes = memory::liveBytes() - live;

        const auto pass_start = now();
        for (std::size_t step = 0; step < steps; ++step)
        {
            script.next(updates, changed);
            const std::size_t allocations = memory::allocations();
            const auto start = now();
            for (const Update<T>& update : updates)
            {
                switch (update.op)
                {
                case Update<T>::Op::Move:
                    engine->move(update.id, update.box);
                    break;
                case Update<T>::Op::Insert:
                    engine->insert(update.id, update.box);
                    break;
                case Update<T>::Op::Remove:
                    engine->remove(update.id);
                    break;
                }
            }
            engine->finishUpdates();
            const auto updated = now();

            PairSink sink;
            const Counters counters = mode == PairMode::Full
                ? engine->pairs(sink, limit)
                : engine->pairsInvolving(std::span<const std::uint32_t>(changed), sink, limit);
            const auto end = now();

            Sample sample;
            sample.totalNs = elapsedNs(start, end);
            sample.prepareNs = elapsedNs(start, updated);
            sample.pairsNs = elapsedNs(updated, end);
            sample.candidates = counters.candidates;
            sample.nodeVisits = counters.nodeVisits;
            sample.cellTests = counters.cellTests;
            sample.references = counters.references;
            sample.pairs = sink.pairs;
            sample.checksum = sink.checksum;
            sample.allocations = memory::allocations() - allocations;
            result.steps.push_back(sample);

            result.totals.candidates += counters.candidates;
            result.totals.nodeVisits += counters.nodeVisits;
            result.totals.cellTests += counters.cellTests;
            result.totals.references += counters.references;
            result.totals.pairs += sink.pairs;
            result.totals.checksum += mix64(sink.checksum + step);
            result.totals.allocations += sample.allocations;
            if (counters.capped)
            {
                result.totals.capped = true;
                break;
            }
            if (budget > 0 && step + 1 < steps && elapsedSeconds(pass_start, now()) > budget)
            {
                result.timeCapped = true;
                break;
            }
        }
        result.peakBytes = memory::peakBytes() - live;
        return result;
    }
};

void recordPass(Result& result, const PassResult& pass, std::size_t round)
{
    if (round == 0)
    {
        result.cold = pass.totals;
        result.passSteps = pass.steps.size();
        result.peakBytes = pass.peakBytes;
        result.retainedBytes = pass.retainedBytes;
        if (pass.totals.capped)
            result.status = "capped";
        else if (pass.timeCapped)
            result.status = "time-capped";
    }
    else if (!sameCounts(pass.totals, result.cold))
        result.status = "NONDETERMINISTIC";
    if (pass.totals.capped)
        return;

    std::vector<std::int64_t> times;
    for (Sample sample : pass.steps)
    {
        sample.round = round;
        times.push_back(sample.totalNs);
        result.warmAllocations += sample.allocations;
        result.samples.push_back(sample);
    }
    result.roundMedians.push_back(median(times));
    result.initNs.push_back(static_cast<double>(pass.initNs));
}

// Interleaved rounds as in the snapshot model; each pass replays the same
// update sequence from the initial scene. The first pass may stop at the time
// budget (time-capped); later passes replay exactly as many steps, so all passes
// are comparable. Candidate-capped engines skip later rounds.
template<typename T>
void measurePersistent(const PersistentFixture<T>& fixture, const Workload& workload, PairMode mode,
    const Options& options, std::vector<std::unique_ptr<PersistentRunnerBase<T>>>& runners, std::span<Result> results)
{
    MotionScript<T> script(fixture.initial, fixture.world, workload, options.steps,
        fixture.seed ^ fnv1a(workload.name));
    EngineSetup<T> setup = fixture.setup;
    setup.idCapacity = script.idCapacity();
    std::vector<Update<T>> updates;
    std::vector<std::uint32_t> changed;
    updates.reserve(2 * setup.idCapacity);
    changed.reserve(setup.idCapacity);

    const std::size_t engines = runners.size();
    for (std::size_t round = 0; round < options.rounds; ++round)
    {
        for (std::size_t k = 0; k < engines; ++k)
        {
            const std::size_t e = (k + round) % engines;
            if (results[e].status == "capped")
                continue;

            const std::size_t steps = round == 0 ? options.steps : results[e].passSteps;
            const double budget = round == 0 ? options.maxTime / static_cast<double>(options.rounds) : 0;
            const std::uint64_t layout = layoutSeed(round, engines, e);
            std::vector<unsigned char> pad(64 * (1 + layout % 1024));
            const std::vector<Box<T>> copy = fixture.boxes;
            withStackOffset(16 * ((layout >> 32) % 256), [&]
            {
                recordPass(results[e],
                    runners[e]->pass(copy, script, setup, mode, options, steps, budget, updates, changed), round);
            });
        }
    }
    finishResults(results, options);
}

// Box2D trees store float; only exact conversions are benchmarked.
template<typename T>
constexpr bool kBox2DType = std::is_same_v<T, std::int32_t> || std::is_same_v<T, float>;

template<typename Engine, typename T>
void addPersistentEngine(const SceneInfo& scene, std::string_view type, const PersistentFixture<T>& fixture,
    const Workload& workload, PairMode mode, const Options& options,
    std::vector<std::unique_ptr<PersistentRunnerBase<T>>>& runners, std::vector<Result>& results)
{
    if (!selected(options.engines, Engine::name) || (mode == PairMode::Moved && !Engine::supportsMoved))
        return;
    runners.push_back(std::make_unique<PersistentRunner<Engine, T>>());
    Result result = makeResult("persistent", scene, type, fixture.initial.size(), 1, Engine::name, fixture.hash);
    result.workload = workload.name;
    result.mode = mode == PairMode::Full ? "full" : "moved";
    results.push_back(result);
}

template<typename T>
PersistentFixture<T> makePersistentFixture(const SceneInfo& scene, std::size_t n, const Options& options)
{
    PersistentFixture<T> fixture;
    fixture.seed = sceneSeed(options.seed, scene.name, n, 0);
    fixture.initial = generate(scene.name, n, fixture.seed);
    fixture.boxes = convert<T>(fixture.initial);
    fixture.world = sceneWorld(scene.name, n);
    fixture.hash = sceneHash(fixture.initial);

    // Grid cell side as in the snapshot grid, over the fixed world.
    double extent = 0;
    for (const IBox& box : fixture.initial)
        extent += static_cast<double>((box.maxX - box.minX) + (box.maxY - box.minY));
    const double area = static_cast<double>(fixture.world.maxX - fixture.world.minX) *
        static_cast<double>(fixture.world.maxY - fixture.world.minY);
    const double count = static_cast<double>(std::max<std::size_t>(n, 1));
    fixture.setup.cellSize = std::max({extent / count, std::sqrt(area / (4 * count)), 1.0});
    fixture.setup.initialCount = n;
    fixture.setup.world = toBox<T>(fixture.world);
    fixture.setup.boundary = scene.boundary;
    return fixture;
}

template<typename T>
void runPersistent(std::string_view type, const Options& options, std::vector<Result>& results)
{
    for (const std::string_view name : kPersistentScenes)
    {
        if (!selected(options.scenes, name))
            continue;
        const SceneInfo& scene = *findScene(name);
        for (const std::size_t n : options.persistentSizes)
        {
            const PersistentFixture<T> fixture = makePersistentFixture<T>(scene, n, options);
            for (const Workload& workload : kWorkloads)
            {
                if (!selected(options.workloads, workload.name))
                    continue;
                for (const PairMode mode : {PairMode::Full, PairMode::Moved})
                {
                    if ((mode == PairMode::Moved && !hasMovedMode(workload)) ||
                        !selected(options.modes, mode == PairMode::Full ? "full" : "moved"))
                        continue;

                    const std::size_t first = results.size();
                    std::vector<std::unique_ptr<PersistentRunnerBase<T>>> runners;
                    addPersistentEngine<BrutePersistent<T>>(scene, type, fixture, workload, mode, options, runners, results);
#if defined(QT_BENCH_HAVE_BOX2D)
                    if constexpr (kBox2DType<T>)
                    {
                        addPersistentEngine<Box2DTree<T, false>>(scene, type, fixture, workload, mode, options, runners,
                            results);
                        addPersistentEngine<Box2DTree<T, true>>(scene, type, fixture, workload, mode, options, runners,
                            results);
                    }
#endif
                    addPersistentEngine<SweepPersistent<T>>(scene, type, fixture, workload, mode, options, runners, results);
                    addPersistentEngine<GridPersistent<T>>(scene, type, fixture, workload, mode, options, runners, results);
                    addPersistentEngine<GridRebuild<T>>(scene, type, fixture, workload, mode, options, runners, results);
                    addPersistentEngine<LoosePersistent<T>>(scene, type, fixture, workload, mode, options, runners, results);
                    addPersistentEngine<LibraryIndex<T>>(scene, type, fixture, workload, mode, options, runners, results);
                    addPersistentEngine<LibraryIndex<T, true>>(scene, type, fixture, workload, mode, options, runners, results);
                    if (!runners.empty())
                        measurePersistent(fixture, workload, mode, options, runners,
                            std::span<Result>(results).subspan(first));
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

std::string readFirstLine(const char* path, std::string_view prefix)
{
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line))
        if (line.compare(0, prefix.size(), prefix) == 0)
        {
            const std::size_t colon = line.find(':');
            return colon == std::string::npos ? line : line.substr(line.find_first_not_of(" \t", colon + 1));
        }
    return "unknown";
}

std::vector<std::string> metadata(const Options& options)
{
    char date[32] = {};
    const std::time_t now = std::time(nullptr);
    std::strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));

#if defined(__clang__)
    const std::string compiler = std::string("clang ") + __clang_version__;
#elif defined(__GNUC__)
    const std::string compiler = std::string("gcc ") + __VERSION__;
#elif defined(_MSC_VER)
    const std::string compiler = "msvc " + std::to_string(_MSC_FULL_VER);
#else
    const std::string compiler = "unknown";
#endif
#if defined(__OPTIMIZE__) || (defined(_MSC_VER) && !defined(_DEBUG))
    const char* optimized = "yes";
#else
    const char* optimized = "NO - results are not representative";
#endif
#if defined(QT_BENCH_HAVE_BOX2D)
    const std::string box2d = QT_BENCH_BOX2D;
#else
    const std::string box2d = "not built";
#endif

    std::vector<std::string> lines = {
        "date: " + std::string(date),
        "cpu: " + readFirstLine("/proc/cpuinfo", "model name"),
        "timer: " + timer::describe() + "; clock source " +
            readFirstLine("/sys/devices/system/clocksource/clocksource0/current_clocksource", ""),
        "governor: " + readFirstLine("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", ""),
        "energy preference: " +
            readFirstLine("/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference", ""),
        "compiler: " + compiler,
        "config: " + std::string(QT_BENCH_CONFIG),
        "flags: " + std::string(QT_BENCH_FLAGS),
        "optimized: " + std::string(optimized),
        "box2d: " + box2d,
        "seed: " + std::to_string(options.seed),
        "args: " + options.args,
        "times: ns per step; snapshot counts are batch totals, persistent counts are pass totals",
    };
    // The process terms were calibrated under the "performance" preference.
    const std::string preference =
        readFirstLine("/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference", "");
    if (preference != "performance" && preference != "unknown")
        lines.push_back("WARNING: energy preference \"" + preference +
            "\"; error calibration assumes \"performance\"");
    return lines;
}

void writeCsv(const Options& options, const std::vector<Result>& results)
{
    if (!options.csv.empty())
    {
        std::FILE* file = std::fopen(options.csv.c_str(), "w");
        if (!file)
        {
            std::fprintf(stderr, "cannot write %s\n", options.csv.c_str());
            return;
        }
        for (const std::string& line : metadata(options))
            std::fprintf(file, "# %s\n", line.c_str());
        std::fprintf(file, "model,scene,boundary,type,n,batch,workload,mode,engine,status,rounds,reps,estimate_ns,"
                           "err_ns,round_err_ns,vs_best,vs_best_lo,vs_best_hi,median_ns,p95_ns,min_ns,"
                           "prepare_median_ns,pairs_median_ns,init_ns,candidates,node_visits,cell_tests,references,max_refs,pairs,"
                           "checksum,allocs_per_step,cold_allocs,peak_bytes,retained_bytes,scene_hash\n");
        for (const Result& r : results)
        {
            const auto total = column(r, &Sample::totalNs);
            std::fprintf(file,
                "%s,%s,%s,%s,%zu,%zu,%s,%s,%s,%s,%zu,%zu,%.1f,%.1f,%.1f,%.4f,%.4f,%.4f,%lld,%lld,%lld,%lld,%lld,"
                "%.1f,%llu,%llu,%llu,%llu,%u,%llu,%016llx,%.3f,%zu,%zu,%zu,%016llx\n",
                r.model.c_str(), r.scene.c_str(), r.boundary.c_str(), r.type.c_str(), r.entries, r.batch,
                r.workload.c_str(), r.mode.c_str(), r.engine.c_str(), r.status.c_str(), r.roundMedians.size(),
                r.samples.size(), r.time.estimate, r.time.error, r.roundError, r.vsBest, r.vsBestLo, r.vsBestHi,
                static_cast<long long>(percentile(total, 0.5)), static_cast<long long>(percentile(total, 0.95)),
                static_cast<long long>(percentile(total, 0)),
                static_cast<long long>(percentile(column(r, &Sample::prepareNs), 0.5)),
                static_cast<long long>(percentile(column(r, &Sample::pairsNs), 0.5)), r.init.estimate,
                static_cast<unsigned long long>(r.cold.candidates), static_cast<unsigned long long>(r.cold.nodeVisits),
                static_cast<unsigned long long>(r.cold.cellTests), static_cast<unsigned long long>(r.cold.references),
                r.cold.maxReferences, static_cast<unsigned long long>(r.cold.pairs),
                static_cast<unsigned long long>(r.cold.checksum), r.allocationsPerStep, r.cold.allocations,
                r.peakBytes, r.retainedBytes, static_cast<unsigned long long>(r.sceneHash));
        }
        std::fclose(file);
    }

    if (!options.raw.empty())
    {
        std::FILE* file = std::fopen(options.raw.c_str(), "w");
        if (!file)
        {
            std::fprintf(stderr, "cannot write %s\n", options.raw.c_str());
            return;
        }
        for (const std::string& line : metadata(options))
            std::fprintf(file, "# %s\n", line.c_str());
        std::fprintf(file, "model,scene,type,n,batch,workload,mode,engine,round,rep,step_ns,prepare_ns,pairs_ns\n");
        for (const Result& r : results)
            for (std::size_t i = 0; i < r.samples.size(); ++i)
                std::fprintf(file, "%s,%s,%s,%zu,%zu,%s,%s,%s,%zu,%zu,%lld,%lld,%lld\n", r.model.c_str(),
                    r.scene.c_str(), r.type.c_str(), r.entries, r.batch, r.workload.c_str(), r.mode.c_str(),
                    r.engine.c_str(), r.samples[i].round, i,
                    static_cast<long long>(perScene(r.samples[i].totalNs, r.batch)),
                    static_cast<long long>(perScene(r.samples[i].prepareNs, r.batch)),
                    static_cast<long long>(perScene(r.samples[i].pairsNs, r.batch)));
        std::fclose(file);
    }
}

void printTable(const std::vector<Result>& results)
{
    std::printf("%-40s %-6s %7s %5s %-12s %12s %7s %20s %13s %11s %8s %9s %s\n", "scene[:workload:mode]", "type", "n",
        "batch", "engine", "time_us", "+-err%", "vs_best [95% CI]", "candidates", "pairs", "alloc/st", "peak_KiB",
        "status");
    for (const Result& r : results)
    {
        const std::string label = r.model == "persistent" ? r.scene + ":" + r.workload + ":" + r.mode : r.scene;
        char time[16] = "-";
        if (r.time.estimate >= 0)
            std::snprintf(time, sizeof(time), "%.3f", r.time.estimate * 1e-3);
        char error[16] = "-";
        if (r.time.error >= 0)
            std::snprintf(error, sizeof(error), "%.1f", 100 * r.time.error / std::max(r.time.estimate, 1.0));
        char ratio[32] = "-";
        if (r.vsBest >= 0)
            std::snprintf(ratio, sizeof(ratio), "%.2f [%.2f-%.2f]", r.vsBest, r.vsBestLo, r.vsBestHi);
        std::printf("%-40s %-6s %7zu %5zu %-12s %12s %7s %20s %13llu %11llu %8.2f %9zu %s\n", label.c_str(),
            r.type.c_str(), r.entries, r.batch, r.engine.c_str(), time, error, ratio,
            static_cast<unsigned long long>(r.cold.candidates), static_cast<unsigned long long>(r.cold.pairs),
            r.allocationsPerStep, r.peakBytes / 1024, r.status.c_str());
    }
}

// ---------------------------------------------------------------------------
// Comparison of two runs
// ---------------------------------------------------------------------------

struct Measurement
{
    std::string model;
    std::string scene;
    std::string type;
    std::size_t n = 0;
    std::string workload = "-";
    std::string mode = "full";
    std::string engine;
    std::string status;
    double estimate = -1; // ns per step; < 0 when not timed (capped)
    double error = -1;
    std::uint64_t candidates = 0;
    std::uint64_t pairs = 0;
    double allocationsPerStep = 0;

    bool timed() const noexcept { return (status == "ok" || status == "time-capped") && estimate > 0; }
};

using Measurements = std::map<std::string, Measurement>; // Key: model,scene,type,n,workload,mode,engine

std::string measurementKey(std::string_view model, std::string_view scene, std::string_view type, std::size_t n,
    std::string_view workload, std::string_view mode, std::string_view engine)
{
    return std::string(model) + "," + std::string(scene) + "," + std::string(type) + "," + std::to_string(n) + "," +
        std::string(workload) + "," + std::string(mode) + "," + std::string(engine);
}

std::string measurementKey(const Measurement& m)
{
    return measurementKey(m.model, m.scene, m.type, m.n, m.workload, m.mode, m.engine);
}

Measurements fromResults(const std::vector<Result>& results)
{
    Measurements measurements;
    for (const Result& r : results)
    {
        const Measurement m{r.model, r.scene, r.type, r.entries, r.workload, r.mode, r.engine, r.status,
            r.time.estimate, r.time.error, r.cold.candidates, r.cold.pairs, r.allocationsPerStep};
        measurements[measurementKey(m)] = m;
    }
    return measurements;
}

// Reads a --csv file; files without model/workload/mode columns are snapshot runs.
bool loadCsv(const std::string& path, Measurements& measurements)
{
    std::ifstream file(path);
    if (!file)
    {
        std::fprintf(stderr, "cannot read %s\n", path.c_str());
        return false;
    }
    std::map<std::string, std::size_t> columns;
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        const std::vector<std::string> fields = splitList(line);
        if (columns.empty())
        {
            for (std::size_t i = 0; i < fields.size(); ++i)
                columns[fields[i]] = i;
            for (const char* name : {"scene", "type", "n", "engine", "status", "estimate_ns", "err_ns"})
                if (!columns.count(name))
                {
                    std::fprintf(stderr, "%s: missing column %s\n", path.c_str(), name);
                    return false;
                }
            continue;
        }
        if (fields.size() != columns.size())
            continue;
        const auto field = [&](const char* name, const char* fallback) -> std::string
        {
            const auto it = columns.find(name);
            return it == columns.end() ? fallback : fields[it->second];
        };
        Measurement m;
        m.model = field("model", "snapshot");
        m.scene = field("scene", "");
        m.type = field("type", "");
        m.n = static_cast<std::size_t>(std::strtoull(field("n", "0").c_str(), nullptr, 10));
        m.workload = field("workload", "-");
        m.mode = field("mode", "full");
        m.engine = field("engine", "");
        m.status = field("status", "");
        m.estimate = std::strtod(field("estimate_ns", "-1").c_str(), nullptr);
        m.error = std::strtod(field("err_ns", "-1").c_str(), nullptr);
        m.candidates = std::strtoull(field("candidates", "0").c_str(), nullptr, 10);
        m.pairs = std::strtoull(field("pairs", "0").c_str(), nullptr, 10);
        m.allocationsPerStep = std::strtod(field("allocs_per_step", "0").c_str(), nullptr);
        measurements[measurementKey(m)] = m;
    }
    return true;
}

// Prints new/old per measurement with a 95% interval from both errors.
void compareRuns(const Measurements& before, const Measurements& after)
{
    std::printf("%-64s %12s %12s %22s %s\n", "model,scene,type,n,workload,mode,engine", "old_us", "new_us",
        "new/old [95% CI]", "verdict");
    std::size_t faster = 0;
    std::size_t slower = 0;
    std::size_t same = 0;
    double log_sum = 0;
    for (const auto& [key, old] : before)
    {
        const auto it = after.find(key);
        if (it == after.end() || !old.timed() || !it->second.timed())
            continue;
        const Measurement& now = it->second;
        const double log_ratio = std::log(now.estimate / old.estimate);
        const double error = std::hypot(std::max(old.error, 0.0) / old.estimate, std::max(now.error, 0.0) / now.estimate);
        const char* verdict = log_ratio + error < 0 ? "faster" : log_ratio - error > 0 ? "SLOWER" : "same";
        (log_ratio + error < 0 ? faster : log_ratio - error > 0 ? slower : same)++;
        log_sum += log_ratio;
        std::printf("%-64s %12.3f %12.3f %8.3f [%.3f-%.3f] %s\n", key.c_str(), old.estimate * 1e-3,
            now.estimate * 1e-3, std::exp(log_ratio), std::exp(log_ratio - error), std::exp(log_ratio + error), verdict);
    }
    const std::size_t total = faster + slower + same;
    std::printf("compared %zu: faster %zu, slower %zu, same within error %zu; geometric mean new/old %.3f\n", total,
        faster, slower, same, total ? std::exp(log_sum / static_cast<double>(total)) : 1.0);
}

// ---------------------------------------------------------------------------
// Acceptance gates (bench/MANIFEST.md, "Frozen acceptance limits")
// ---------------------------------------------------------------------------

enum class Verdict
{
    Pass,
    Fail,
    Inconclusive,
};

struct GateRow
{
    std::string id;
    bool hard = true;
    std::string subject;
    double value = 0;
    double lo = 0;
    double hi = 0;
    std::string limit;
    Verdict verdict = Verdict::Pass;
};

class Gate
{
public:
    Gate(const Measurements& measurements, const Options& options): mMeasurements(measurements), mOptions(options) {}

    void run()
    {
        correctness();
        for (const auto& [key, m] : mMeasurements)
        {
            if (m.model == "snapshot" && m.engine == mOptions.gateSnapshot)
                snapshot(m);
            else if (m.model == "snapshot" && m.engine == mOptions.gateOrdered)
                ordered(m);
            else if (m.model == "persistent" && m.engine == mOptions.gatePersistent)
                persistent(m);
        }
    }

    // 0: every hard gate passes; 3: a hard gate fails; 4: none fails, some inconclusive.
    int print() const
    {
        static constexpr const char* names[] = {"pass", "FAIL", "inconclusive"};
        std::printf("%-4s %-4s %-80s %26s %8s %s\n", "gate", "kind", "subject", "value [95% CI]", "limit", "verdict");
        std::map<std::string, std::array<std::size_t, 3>> counts;
        bool hard_fail = false;
        bool hard_open = false;
        std::vector<const GateRow*> rows;
        for (const GateRow& row : mRows)
            rows.push_back(&row);
        std::stable_sort(rows.begin(), rows.end(), [](const GateRow* a, const GateRow* b) { return a->id < b->id; });
        for (const GateRow* entry : rows)
        {
            const GateRow& row = *entry;
            char value[64];
            if (row.lo == row.hi)
                std::snprintf(value, sizeof(value), "%.3f", row.value);
            else
                std::snprintf(value, sizeof(value), "%.3f [%.3f-%.3f]", row.value, row.lo, row.hi);
            std::printf("%-4s %-4s %-80s %26s %8s %s\n", row.id.c_str(), row.hard ? "hard" : "soft", row.subject.c_str(),
                value, row.limit.c_str(), names[static_cast<int>(row.verdict)]);
            ++counts[row.id + (row.hard ? " hard" : " soft")][static_cast<std::size_t>(row.verdict)];
            hard_fail = hard_fail || (row.hard && row.verdict == Verdict::Fail);
            hard_open = hard_open || (row.hard && row.verdict == Verdict::Inconclusive);
        }
        std::printf("summary (pass / fail / inconclusive):\n");
        for (const auto& [id, c] : counts)
            std::printf("  %-8s %5zu %5zu %5zu\n", id.c_str(), c[0], c[1], c[2]);
        const char* overall = hard_fail ? "FAIL" : hard_open ? "INCONCLUSIVE" : "PASS";
        std::printf("gates: %s (candidates: snapshot %s, persistent %s, ordered %s vs %s)\n", overall,
            mOptions.gateSnapshot.c_str(), mOptions.gatePersistent.c_str(), mOptions.gateOrdered.c_str(),
            mOptions.gateOrderedBase.c_str());
        return hard_fail ? 3 : hard_open ? 4 : 0;
    }

private:
    const Measurements& mMeasurements;
    const Options& mOptions;
    std::vector<GateRow> mRows;

    static bool representative(std::string_view scene)
    {
        return scene == "uniform" || scene == "clustered" || scene == "split-line" ||
            scene == "split-line-translated" || scene == "mixed-size";
    }

    const Measurement* find(const Measurement& like, std::size_t n, std::string_view workload,
        std::string_view engine, std::string_view scene = {}) const
    {
        const auto it = mMeasurements.find(measurementKey(like.model, scene.empty() ? like.scene : scene, like.type,
            n, workload, like.mode, engine));
        return it == mMeasurements.end() || !it->second.timed() ? nullptr : &it->second;
    }

    const Measurement* best(const Measurement& like, std::initializer_list<std::string_view> engines) const
    {
        const Measurement* result = nullptr;
        for (const std::string_view engine : engines)
            if (const Measurement* m = find(like, like.n, like.workload, engine))
                if (!result || m->estimate < result->estimate)
                    result = m;
        return result;
    }

    static std::string subject(const Measurement& m, std::string_view detail)
    {
        std::string text = m.model + " " + m.type + " " + m.scene + " n=" + std::to_string(m.n);
        if (m.model == "persistent")
            text += " " + m.workload + "/" + m.mode;
        if (!detail.empty())
            text += " " + std::string(detail);
        return text;
    }

    // value <= limit, with an interval: pass if hi <= limit, fail if lo > limit.
    void upper(std::string id, bool hard, std::string what, double value, double lo, double hi, double limit)
    {
        char text[32];
        std::snprintf(text, sizeof(text), "<= %g", limit);
        const Verdict verdict = hi <= limit ? Verdict::Pass : lo > limit ? Verdict::Fail : Verdict::Inconclusive;
        mRows.push_back(GateRow{std::move(id), hard, std::move(what), value, lo, hi, text, verdict});
    }

    // 1/limit <= value <= limit.
    void within(std::string id, bool hard, std::string what, double value, double lo, double hi, double limit)
    {
        char text[32];
        std::snprintf(text, sizeof(text), "x/%g", limit);
        const Verdict verdict = lo >= 1 / limit && hi <= limit ? Verdict::Pass
            : hi < 1 / limit || lo > limit                     ? Verdict::Fail
                                                               : Verdict::Inconclusive;
        mRows.push_back(GateRow{std::move(id), hard, std::move(what), value, lo, hi, text, verdict});
    }

    // Ratio a / b with the log-scale half-width sqrt(rel_a^2 + rel_b^2).
    void ratio(const std::string& id, bool hard, const Measurement& a, const Measurement& b, double limit,
        std::string_view against)
    {
        const double value = a.estimate / b.estimate;
        const double h = std::hypot(std::max(a.error, 0.0) / a.estimate, std::max(b.error, 0.0) / b.estimate);
        upper(id, hard, subject(a, "vs " + std::string(against) + " (" + b.engine + ")"), value, value * std::exp(-h),
            value * std::exp(h), limit);
    }

    void correctness()
    {
        for (const auto& [key, m] : mMeasurements)
            if (m.status == "MISMATCH" || m.status == "NONDETERMINISTIC" || m.status == "UNORDERED")
                mRows.push_back(GateRow{"G0", true, subject(m, m.engine + " " + m.status), 1, 1, 1, "none",
                    Verdict::Fail});
    }

    void snapshot(const Measurement& m)
    {
        if (!m.timed())
            return;
        // G1: small scenes against brute force.
        if (m.n <= 64 && (m.scene == "uniform" || m.scene == "clustered"))
            if (const Measurement* brute = find(m, m.n, "-", "brute"))
                ratio("G1", true, m, *brute, 2, "brute force");
        // G2: representative scenes against the best baseline.
        if (m.n >= 100 && representative(m.scene))
            if (const Measurement* base = best(m, {"brute", "sweep", "grid"}))
                ratio("G2", true, m, *base, m.n >= 10000 ? 2 : 4, "best baseline");
        // G3: growth per tenfold n of candidates per (pairs + n), so that
        // output growth of the fixture does not count as wasted work.
        if (m.n >= 100 && (m.scene == "split-line" || m.scene == "split-line-translated" || m.scene == "mixed-size"))
            if (const Measurement* next = find(m, m.n * 10, "-", m.engine))
            {
                const auto work = [](const Measurement& x)
                { return static_cast<double>(x.candidates) / static_cast<double>(x.pairs + x.n); };
                const double growth = work(*next) / work(m);
                upper("G3", true, subject(*next, "candidates/(pairs+n) vs n=" + std::to_string(m.n)), growth, growth,
                    growth, 1.5);
            }
        // G4: translation invariance of candidates and time.
        if (m.n >= 100 && m.scene == "split-line-translated")
            if (const Measurement* original = find(m, m.n, "-", m.engine, "split-line"))
            {
                const double count = static_cast<double>(m.candidates) / static_cast<double>(std::max<std::uint64_t>(1, original->candidates));
                within("G4", true, subject(m, "candidates vs split-line"), count, count, count, 1.5);
                const double value = m.estimate / original->estimate;
                const double h = std::hypot(m.error / m.estimate, original->error / original->estimate);
                within("G4", true, subject(m, "time vs split-line"), value, value * std::exp(-h), value * std::exp(h), 1.5);
            }
        // G6: no warm allocations.
        upper("G6", true, subject(m, "allocations per warm step"), m.allocationsPerStep, m.allocationsPerStep,
            m.allocationsPerStep, 0);
    }

    void ordered(const Measurement& m)
    {
        // G9: ordered step against the same engine's unordered step.
        if (!m.timed() || m.n < 1000 || !representative(m.scene))
            return;
        if (const Measurement* base = find(m, m.n, "-", mOptions.gateOrderedBase))
            ratio("G9", true, m, *base, 4, "unordered");
    }

    void persistent(const Measurement& m)
    {
        if (!m.timed())
            return;
        // G6: no allocations per step.
        upper("G6", true, subject(m, "allocations per step"), m.allocationsPerStep, m.allocationsPerStep,
            m.allocationsPerStep, 0);
        if (m.mode == "full")
        {
            // G7: full enumeration; hard against the dynamic tree, soft against the grid.
            if (const Measurement* tree = best(m, {"b2tree", "b2tree-fat"}))
                ratio("G7", true, m, *tree, 2, "dynamic tree");
            if (const Measurement* grid = best(m, {"grid", "grid-rebuild"}))
                ratio("G7", false, m, *grid, 4, "grid");
            return;
        }
        if (m.workload == "static")
        {
            // G8a: an empty changed set costs at most 1 us per step.
            upper("G8a", true, subject(m, "ns per step"), m.estimate, m.estimate - m.error, m.estimate + m.error, 1000);
        }
        else if (m.workload == "small-single" && m.n >= 100000)
        {
            // G8b: one mover at 1e5 costs at most 4x its 1e3 value.
            if (const Measurement* small = find(m, 1000, m.workload, m.engine))
            {
                const double value = m.estimate / small->estimate;
                const double h = std::hypot(m.error / m.estimate, small->error / small->estimate);
                upper("G8b", true, subject(m, "vs n=1000"), value, value * std::exp(-h), value * std::exp(h), 4);
            }
        }
        else if (m.workload == "small-1%" || m.workload == "teleport-1%")
        {
            // G8c: moved-set queries; hard against the dynamic tree, soft against the grid.
            if (const Measurement* tree = best(m, {"b2tree", "b2tree-fat"}))
                ratio("G8c", true, m, *tree, 2, "dynamic tree");
            if (const Measurement* grid = find(m, m.n, m.workload, "grid"))
                ratio("G8c", false, m, *grid, 4, "grid");
        }
    }
};

template<typename T>
void runType(std::string_view type, const Options& options, std::vector<Result>& results)
{
    if (options.snapshot)
        runSnapshot<T>(type, options, results);
    if (options.persistent)
        runPersistent<T>(type, options, results);
}
} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parse(argc, argv, options))
        return 2;

    if (!options.gateFiles.empty())
    {
        Measurements measurements;
        for (const std::string& file : options.gateFiles)
            if (!loadCsv(file, measurements))
                return 2;
        Gate gate(measurements, options);
        gate.run();
        return gate.print();
    }

    if (!options.diffOld.empty())
    {
        Measurements before;
        Measurements after;
        if (!loadCsv(options.diffOld, before) || !loadCsv(options.diffNew, after))
            return 2;
        compareRuns(before, after);
        return 0;
    }

#if defined(QT_BENCH_HAVE_BOX2D)
    installBox2DAllocator();
#endif
    timer::calibrate();

    for (const std::string& line : metadata(options))
        std::printf("# %s\n", line.c_str());

    std::vector<Result> results;
    for (const std::string& type : options.types)
    {
        if (type == "int32")
            runType<std::int32_t>(type, options, results);
        else if (type == "int64")
            runType<std::int64_t>(type, options, results);
        else if (type == "float")
            runType<float>(type, options, results);
        else
            runType<double>(type, options, results);
    }

    printTable(results);
    writeCsv(options, results);
    if (!options.compare.empty())
    {
        Measurements before;
        if (!loadCsv(options.compare, before))
            return 2;
        compareRuns(before, fromResults(results));
    }

    std::size_t failures = 0;
    for (const Result& result : results)
        if (result.status == "MISMATCH" || result.status == "NONDETERMINISTIC" || result.status == "UNORDERED")
            ++failures;
    if (failures != 0)
        std::fprintf(stderr, "%zu result(s) failed cross-checks\n", failures);
    int code = failures == 0 ? 0 : 1;
    if (options.gate)
    {
        const Measurements measurements = fromResults(results);
        Gate gate(measurements, options);
        gate.run();
        const int gate_code = gate.print();
        code = code != 0 ? code : gate_code;
    }
    return code;
}
