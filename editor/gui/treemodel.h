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

#include "editor/app/utility.h"
#include "editor/gui/treewidget.h"

namespace gui
{
    // Generic tree model implementation for the tree widget.
    template<typename TreeModelType>
    class RenderTreeModel : public TreeWidget::TreeModel
    {
    public:
        RenderTreeModel(TreeModelType& model) : mModel(model)
        {}
        void Flatten(std::vector<TreeWidget::TreeItem>& list) override
        {
            auto& root = mModel.GetRenderTree();
            using RenderTree = typename TreeModelType::RenderTree;
            using TreeNode   = typename TreeModelType::RenderTreeValue;

            class Visitor : public RenderTree::Visitor
            {
            public:
                Visitor(std::vector<TreeWidget::TreeItem>& list) : mList(list)
                {}
                void EnterNode(TreeNode* node) override
                {
                    TreeWidget::TreeItem item;
                    item.SetId(node ? node->GetId() : "root");
                    item.SetText(node ? node->GetName() : "Root");
                    item.SetUserData(node);
                    item.SetLevel(mLevel);
                    if (node)
                    {
                        const auto visible = node->TestFlag(TreeNode::Flags::VisibleInEditor);
                        const auto locked = node->TestFlag(TreeNode::Flags::LockedInEditor);
                        if (!visible)
                            item.SetVisibilityIcon(QIcon("icons:crossed_eye.png"));
                        if (locked)
                            item.SetLockedIcon(QIcon("icons:lock.png"));
                    }
                    else
                    {
                        item.SetVisibilityIcon(QIcon("icons:eye.png"));
                        item.SetLockedIcon(QIcon("icons:lock.png"));
                    }
                    mList.push_back(item);
                    mLevel++;
                }
                void LeaveNode(TreeNode* node) override
                { mLevel--; }
            private:
                unsigned mLevel = 0;
                std::vector<TreeWidget::TreeItem>& mList;
            };
            Visitor visitor(list);
            root.PreOrderTraverse(visitor);
        }
    private:
        TreeModelType& mModel;
    };
} // namespace