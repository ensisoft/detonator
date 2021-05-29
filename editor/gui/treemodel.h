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
        virtual void Flatten(std::vector<TreeWidget::TreeItem>& list)
        {
            auto& root = mModel.GetRenderTree();
            using RenderTree = typename TreeModelType::RenderTree;
            using TreeNode   = typename TreeModelType::RenderTreeValue;

            class Visitor : public RenderTree::Visitor
            {
            public:
                Visitor(std::vector<TreeWidget::TreeItem>& list) : mList(list)
                {}
                virtual void EnterNode(TreeNode* node)
                {
                    TreeWidget::TreeItem item;
                    item.SetId(node ? app::FromUtf8(node->GetId()) : "root");
                    item.SetText(node ? app::FromUtf8(node->GetName()) : "Root");
                    item.SetUserData(node);
                    item.SetLevel(mLevel);
                    if (node)
                    {
                        item.SetIcon(QIcon("icons:eye.png"));
                        if (!node->TestFlag(TreeNode::Flags::VisibleInEditor))
                            item.SetIconMode(QIcon::Disabled);
                        else item.SetIconMode(QIcon::Normal);
                    }
                    mList.push_back(item);
                    mLevel++;
                }
                virtual void LeaveNode(TreeNode* node)
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