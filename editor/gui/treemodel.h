// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

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
                    item.SetId(node ? app::FromUtf8(node->GetClassId()) : "root");
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