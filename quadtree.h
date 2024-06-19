#pragma once

#include <vector>
#include <algorithm>
#include <type_traits>

namespace QuadTree
{
template<typename T>
concept IntType = std::is_integral_v<T>;

template<typename T>
concept FloatType = std::is_floating_point_v<T>;

template<typename RectT>
struct Rect { RectT x, y, w, h; };

template<typename ItemT, typename RectT>
class Tree
{
public:
    using ItemRectFunc = const Rect<RectT>& (*)(const ItemT&);

    constexpr Tree(Rect<RectT> rect, ItemRectFunc getRect, std::size_t nodeItems = 16, std::size_t depth = 4):
        mGetRect(getRect), mRoot(std::move(rect), nodeItems), mNodeItems(nodeItems), mDepth(depth)
    {
        constexpr std::size_t root_depth = 0;
        init(mRoot, root_depth + 1);
    }

    constexpr virtual ~Tree()
    {
        constexpr std::size_t root_depth = 0;
        cleanup(mRoot, root_depth + 1);
    }

    constexpr void add(const ItemT& item)
    {
        constexpr std::size_t root_depth = 0;
        addItem(mRoot, root_depth, item, mGetRect(item));
    }

    constexpr void remove(const ItemT& item)
    {
        removeItem(mRoot, item, mGetRect(item));
    }

    constexpr void query(const Rect<RectT>& rect, std::vector<ItemT>& items) const
    {
        queryNode(mRoot, rect, items);
    }

    constexpr void queryAll(std::vector<std::pair<ItemT, ItemT>>& itemPairs) const
    {
        queryNodeAll(mRoot, itemPairs);
    }

#ifdef _DEBUG
    using EnumCallbackT = void (*)(int type, int level, int quadrant, const Rect<RectT>& rect, bool hasChildren, const ItemT*);

    constexpr void enumerateNodes(EnumCallbackT enumCallback)
    {
        enumerateNode(mRoot, 0, 0, enumCallback);
    }
#endif

protected:

    static constexpr std::size_t QUAD_CHILDREN = 4;

    struct Node
    {
        constexpr Node(Rect<RectT> aRect, std::size_t reserve) requires IntType<RectT>: rect(std::move(aRect)) 
        {
            centerX = (rect.x + rect.w / 2) + (rect.w % 2);
            centerY = (rect.y + rect.h / 2) + (rect.h % 2);

            items.reserve(reserve);
        }

        constexpr Node(Rect<RectT> aRect, std::size_t reserve) requires FloatType<RectT>: rect(std::move(aRect))
        {
            centerX = (rect.x + rect.w / 2);
            centerY = (rect.y + rect.h / 2);

            items.reserve(reserve);
        }

        bool hasChildren = false;

        Node* children[QUAD_CHILDREN];
        std::vector<ItemT> items;
        const Rect<RectT> rect;
        RectT centerX;
        RectT centerY;
    };

    ItemRectFunc mGetRect;
    Node mRoot;
    const std::size_t mNodeItems;
    const std::size_t mDepth;
    std::equal_to<ItemT> mItemEqual;

    constexpr void init(Node& node, std::size_t depth)
    {
        std::size_t i = 1;
        for (auto& child : node.children)
        {
            child = new Node(computeBox(node.rect, i++), mNodeItems);
            if (depth < mDepth)
                init(*child, depth + 1);
        }
    }

    constexpr void cleanup(Node& node, std::size_t depth)
    {
        for (auto& child : node.children)
        {
            if (depth < mDepth)
                cleanup(*child, depth + 1);
            delete child;
        }
    }

    constexpr void queryNode(const Node& node, const Rect<RectT>& queryRect, std::vector<ItemT>& items) const
    {
        for (const auto& item : node.items)
            if (rectIntersects(queryRect, mGetRect(item)))
                items.push_back(item);

        if (node.hasChildren)
            for (const auto& child : node.children)
                if (rectIntersects(queryRect, child->rect))
                    queryNode(*child, queryRect, items);
    }

    constexpr void queryNodeAll(const Node& node, std::vector<std::pair<ItemT, ItemT>>& itemPairs) const
    {   
        // Check for intersection between items in current node
        for (auto item_a = node.items.cbegin(); item_a != node.items.cend(); item_a++)
        {
            const Rect<RectT>& item_rect = mGetRect(*item_a);
            for (auto item_b = std::next(item_a, 1); item_b != node.items.cend(); item_b++)
                if (rectIntersects(item_rect, mGetRect(*item_b)))
                    itemPairs.emplace_back(*item_a, *item_b);
        }
    
        if (node.hasChildren)
        {
            // Check for intersection between items and sub-items
            for (const auto& child : node.children)
            {
                for (const auto& item : node.items)
                    queryNodeItems(*child, item, itemPairs);
            }

            // Do the same for all other children
            for (const auto& child : node.children)
                queryNodeAll(*child, itemPairs);
        }
    }

    constexpr void queryNodeItems(const Node& node, const ItemT& item, std::vector<std::pair<ItemT, ItemT>>& itemPairs) const
    {
        const Rect<RectT>& item_rect = mGetRect(item);
        for (const auto& sub_item : node.items)
        {
            if (rectIntersects(mGetRect(sub_item), item_rect))
                itemPairs.emplace_back(item, sub_item);
        }

        if (node.hasChildren)
        {
            for (const auto& child : node.children)
                queryNodeItems(*child, item, itemPairs);
        }
    }

    constexpr void addItem(Node& node, std::size_t depth, const ItemT& item, const Rect<RectT>& itemRect)
    {
        Node* roll_node = &node;

        for (std::size_t i = depth; i <= mDepth; i++)
        {
            if (i < mDepth)
            {
                auto direction = getQuadrant(roll_node->centerX, roll_node->centerY, itemRect);
                if (direction > 0)
                {
                    roll_node->hasChildren = true;
                    roll_node = roll_node->children[direction - 1];
                    continue;
                }
            }

            roll_node->items.push_back(item);
            break;
        }
    }

    constexpr bool removeItem(Node& node, const ItemT& item, const Rect<RectT>& itemRect)
    {
        if (!node.hasChildren)
            doRemoveItem(node, item);
        else 
        {
            auto direction = getQuadrant(node.centerX, node.centerY, itemRect);
            if (direction > 0)
            {
                if (removeItem(*node.children[direction - 1], item, itemRect))
                {
                    // Node was removed, check if need to update hasChildren
                    bool has_children = false;
                    for (const auto& child : node.children)
                    {
                        if (child->items.size())
                        {
                            has_children = true;
                            break;
                        }
                    }
                    node.hasChildren = has_children;
                }
            } else
                doRemoveItem(node, item);
        }

        return (!node.hasChildren)&&(!node.items.size());
    }

    constexpr void doRemoveItem(Node& node, const ItemT& item)
    {
        // Find the item
        auto it = std::find_if(std::cbegin(node.items), std::cend(node.items),
            [this, &item](const auto& rhs){ return mItemEqual(item, rhs); });
        node.items.erase(it);
    }

    constexpr std::size_t getQuadrant(RectT center_x, RectT center_y, const Rect<RectT>& itemRect) const noexcept
    {
        if (itemRect.x + itemRect.w <= center_x)
        {
            // West
            if (itemRect.y + itemRect.h <= center_y)
                return 1; // NW
            else if (itemRect.y >= center_y)
                return 3; // SW
        } else if (itemRect.x >= center_x)
        {
            // East
            if (itemRect.y + itemRect.h <= center_y)
                return 2; // NE
            else if (itemRect.y >= center_y)
                return 4; // SE
        }

        return 0; // None
    }

    constexpr Rect<RectT> computeBox(const Rect<RectT>& rect, std::size_t direction) const noexcept requires IntType<RectT>
    {
        auto child_size_w = rect.w / 2;
        auto w_rest = rect.w % 2;
        auto child_size_h = rect.h / 2;
        auto h_rest = rect.h % 2;

        switch (direction)
        {
            // NW
            case 1:
                return Rect<RectT>{rect.x, rect.y, child_size_w + w_rest, child_size_h + h_rest};
            // NE
            case 2:
                return Rect<RectT>{rect.x + child_size_w + w_rest, rect.y, child_size_w, child_size_h + h_rest};
            // SW
            case 3:
                return Rect<RectT>{rect.x, rect.y + child_size_h + h_rest, child_size_w + w_rest, child_size_h};
            // SE
            case 4:
                return Rect<RectT>{rect.x + child_size_w + w_rest, rect.y + child_size_h + h_rest, child_size_w, child_size_h};
            default:
                return Rect<RectT>{0, 0, 0, 0};
        }
    }

    constexpr Rect<RectT> computeBox(const Rect<RectT>& rect, std::size_t direction) const noexcept requires FloatType<RectT>
    {
        auto child_size_w = rect.w / 2;
        auto child_size_h = rect.h / 2;

        switch (direction)
        {
            // NW
            case 1:
                return Rect<RectT>{rect.x, rect.y, child_size_w, child_size_h};
            // NE
            case 2:
                return Rect<RectT>{rect.x + child_size_w, rect.y, child_size_w, child_size_h};
            // SW
            case 3:
                return Rect<RectT>{rect.x, rect.y + child_size_h, child_size_w, child_size_h};
            // SE
            case 4:
                return Rect<RectT>{rect.x + child_size_w, rect.y + child_size_h, child_size_w, child_size_h};
            default:
                return Rect<RectT>{0, 0, 0, 0};
        }
    }

    inline constexpr bool rectIntersects(const Rect<RectT>& rect1, const Rect<RectT>& rect2) const noexcept
    {
        return !(
            rect1.w == 0 || rect1.h == 0 || rect2.w == 0 || rect2.h == 0 ||
            rect1.x >= rect2.x + rect2.w || rect1.x + rect1.w <= rect2.x ||
            rect1.y >= rect2.y + rect2.h || rect1.y + rect1.h <= rect2.y);
    }

#ifdef _DEBUG
    constexpr void enumerateNode(Node& node, int level, int quadrant, EnumCallbackT enumCallback)
    {
        enumCallback(0, level, quadrant, node.rect, node.hasChildren, NULL);
        for (const auto &item: node.items)
           enumCallback(1, level, quadrant, node.rect, 0, &item);

        enumCallback(2, level, quadrant, node.rect, 0, NULL);

        if (node.hasChildren)
        {
            for (int i = 0; i < QUAD_CHILDREN; i++)
                enumerateNode(*node.children[i], level+1, i, enumCallback);
        }
    }
#endif
};

} // namespace QuadTree
