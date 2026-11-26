#include "index-mixed-macros.h"

using I = QuadTree::Index<int, int, QuadTree::Checked>;

I::HandleType indexTu2Insert(I& tree)
{
    return tree.insert(QuadTree::Box<int>{0, 0, 2, 2}, 2).handle;
}

bool indexTu2Valid(const I& tree, I::HandleType handle)
{
    return tree.valid(handle);
}
