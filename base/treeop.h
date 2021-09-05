// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "config.h"

#include "base/tree.h"

namespace base
{

// Search the tree for a route from parent to assumed child node.
// When node is a descendant of parent returns true. Otherwise
// false is returned and there's no path from parent to node.
// Optionally provide the path from root to the child.
template<typename Node>
bool SearchChild(const RenderTree<Node>& tree, const Node* node, const Node* parent = nullptr,
                 std::vector<const Node*>* path = nullptr)
{
    class ConstVisitor : public RenderTree<Node>::ConstVisitor {
    public:
        ConstVisitor(const Node* node,std::vector<const Node*>* path)
        : mNode(node)
        , mPath(path)
        {}
        virtual void EnterNode(const Node* node) override
        {
            if (!mFound && mPath)
                mPath->push_back(node);
            if (mNode == node)
                mFound = true;
        }
        virtual void LeaveNode(const Node* node) override
        {
            if (!mFound && mPath)
                mPath->pop_back();
        }
        virtual bool IsDone() const override
        { return mFound; }
        bool GetResult() const
        { return mFound; }
    private:
        const Node* mNode = nullptr;
        std::vector<const Node*>* mPath = nullptr;
        bool mFound = false;
    };
    ConstVisitor visitor(node, path);
    tree.PreOrderTraverse(visitor, parent);
    return visitor.GetResult();
}

template<typename Node>
bool SearchParent(const RenderTree<Node>& tree, const Node* node, const Node* parent = nullptr,
                  std::vector<const Node*>* path = nullptr)
{
    if (path)
        path->push_back(node);
    if (node == parent)
        return true;
    while (tree.HasParent(node))
    {
        const Node* p = tree.GetParent(node);
        if (path)
            path->push_back(p);
        if (p == parent)
            return true;
        node = p;
    }
    if (path)
        path->clear();
    return false;
}

} // namespace
