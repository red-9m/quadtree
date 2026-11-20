#include "quadtree-index.h"

#include <array>
#include <cstring>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using I = QuadTree::Index<int, int, QuadTree::Checked>;
    auto created = I::create();
    if (!created.ok()) return 1;
    auto tree = std::move(created).value();
    const auto added = tree.insert(QuadTree::Box<int>{0, 0, 2, 2}, 1);
    if (!added.ok()) return 1;
    auto visitor = [](const auto&, const auto&) noexcept { return QuadTree::Stop; };
    if (std::strcmp(argv[1], "stale-moved") == 0)
    {
        if (tree.remove(added.handle) != QuadTree::Status::Ok) return 1;
        (void)tree.forEachPairInvolving(std::array{added.handle}, visitor);
    }
    else if (std::strcmp(argv[1], "foreign-moved") == 0)
    {
        auto other = I::create();
        if (!other.ok()) return 1;
        (void)other.value().forEachPairInvolving(std::array{added.handle}, visitor);
    }
    else if (std::strcmp(argv[1], "default-partners") == 0)
        (void)tree.queryPartners(I::HandleType{}, [](const auto&) noexcept { return QuadTree::Stop; });
    else if (std::strcmp(argv[1], "cleared-update") == 0)
    {
        tree.clear();
        (void)tree.update(added.handle, QuadTree::Box<int>{0, 0, 1, 1});
    }
    else return 2;
    return 7;
}
