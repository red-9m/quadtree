// Two coincident ranked double points at 1e100 with reserved build scratch:
// an ordered full-pair query must report one pair without raising FP flags,
// and must not fault with overflow/inexact/invalid trapping enabled (glibc).
#include <cfenv>
#include <cstdio>
#include <vector>
#include "quadtree-index.h"

int main(int argc, char**)
{
    using I = QuadTree::Index<int, double>;
    auto made = I::create({.smallSetThreshold = 0});
    auto& index = made.value();
    std::vector<QuadTree::BuildEntry<int, double>> input{
        {QuadTree::Box<double>{1e100, 1e100, 1e100, 1e100}, 0, {}, 1},
        {QuadTree::Box<double>{1e100, 1e100, 1e100, 1e100}, 1, {}, 2}};
    QuadTree::Workspace<double> scratch;
    if (scratch.reserve(input.size()) != QuadTree::Status::Ok || index.build(input, {.workspace = &scratch}) != QuadTree::Status::Ok)
        return 2;
    std::feclearexcept(FE_ALL_EXCEPT);
    if (argc > 1)
        feenableexcept(FE_OVERFLOW | FE_INEXACT | FE_INVALID);
    int pairs = 0;
    const auto result = index.forEachPair([&](const auto&, const auto&) noexcept { ++pairs; return QuadTree::Continue; },
        {.ordered = true, .workspace = &scratch});
    fedisableexcept(FE_ALL_EXCEPT);
    const int raised = std::fetestexcept(FE_ALL_EXCEPT);
    std::printf("pairs %d, status %u, flags%s%s%s%s%s\n", pairs, unsigned(result.status), raised ? "" : " none",
        raised & FE_OVERFLOW ? " FE_OVERFLOW" : "", raised & FE_INEXACT ? " FE_INEXACT" : "",
        raised & FE_INVALID ? " FE_INVALID" : "", raised & FE_UNDERFLOW ? " FE_UNDERFLOW" : "");
    return pairs == 1 && !raised ? 0 : 1;
}
