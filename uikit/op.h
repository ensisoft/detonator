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

#include "warnpush.h"

#include "warnpop.h"

#include <stack>
#include <utility>
#include <vector>
#include <cstddef>

#include "base/tree.h"
#include "base/treeop.h"
#include "data/writer.h"
#include "data/reader.h"
#include "uikit/types.h"
#include "uikit/widget.h"

// functions that operate on a widget hierarchy.

namespace uik
{

using RenderTree = base::RenderTree<Widget>;

static
void RenderTreeIntoJson(const RenderTree& tree, data::Writer& data, const Widget* widget = nullptr)
{
    auto chunk = data.NewWriteChunk();
    if (widget) {
        chunk->Write("type", widget->GetType());
        widget->IntoJson(*chunk);
    }
    tree.ForEachChild([&tree, &chunk](const Widget* child) {
        RenderTreeIntoJson(tree, *chunk, child);
    }, widget);
    data.AppendChunk("widgets", std::move(chunk));
}

static
bool RenderTreeFromJson(const data::Reader& data, RenderTree& tree,
    std::vector<std::unique_ptr<Widget>>& container,
    uik::Widget* parent = nullptr)
{
    std::unique_ptr<Widget> widget;
    Widget::Type type;
    if (data.Read("type", &type))
        widget = uik::CreateWidget(type);
    if (widget && !widget->FromJson(data))
        return false;

    if (widget)
    {
        tree.LinkChild(parent, widget.get());
        parent = widget.get();
        container.push_back(std::move(widget));
    }

    for (unsigned i=0; i<data.GetNumChunks("widgets"); ++i)
    {
        const auto& chunk = data.GetReadChunk("widgets", i);
        if (!RenderTreeFromJson(*chunk, tree, container, parent))
            return false;
    }
    return true;
}

static
Widget* DuplicateWidget(RenderTree& tree, const Widget* widget, std::vector<std::unique_ptr<Widget>>* clones)
{
    // mark the index of the item that will be the first dupe we create
    // we'll return this later since it's the root of the new hierarchy.
    const size_t first = clones->size();
    // do a deep copy of a hierarchy of nodes starting from
    // the selected node and add the new hierarchy as a new
    // child of the selected node's parent
    if (tree.HasNode(widget))
    {
        const auto* parent = tree.GetParent(widget);
        class ConstVisitor : public RenderTree::ConstVisitor {
        public:
            ConstVisitor(const Widget* parent, std::vector<std::unique_ptr<Widget>>& clones)
                : mClones(clones)
            {
                mParents.push(parent);
            }
            virtual void EnterNode(const Widget* node) override
            {
                const auto* parent = mParents.top();

                auto clone = node->Clone();
                mParents.push(clone.get());
                mLinks[clone.get()] = parent;
                mClones.push_back(std::move(clone));
            }
            virtual void LeaveNode(const Widget* node) override
            {
                mParents.pop();
            }
            void LinkChildren(RenderTree& tree)
            {
                for (const auto& p : mLinks)
                {
                    const Widget* child  = p.first;
                    const Widget* parent = p.second;
                    tree.LinkChild(parent, child);
                }
            }
        private:
            std::stack<const Widget*> mParents;
            std::unordered_map<const Widget*, const Widget*> mLinks;
            std::vector<std::unique_ptr<Widget>>& mClones;
        };
        ConstVisitor visitor(parent, *clones);
        tree.PreOrderTraverse(visitor, widget);
        visitor.LinkChildren(tree);
    }
    else
    {
        clones->emplace_back(widget->Clone());
    }
    return (*clones)[first].get();
}

} // namespace
