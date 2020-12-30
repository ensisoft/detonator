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

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>

#include "base/assert.h"
#include "base/utility.h"

namespace game
{
    // Non-owning treenode template for tree structure and traversal.
    // Useful for example rendering (scene graph) tree.
    template<typename T>
    class TreeNode
    {
    public:
        TreeNode() = default;
        TreeNode(T* value) : mNodeValue(value)
        {}

        // A tree visitor but with the ability to visit
        // (and possibly mutate) the tree nodes itself.
        class TreeVisitor
        {
        public:
            virtual void EnterNode(TreeNode* node) {};
            virtual void LeaveNode(TreeNode* node) {};
        protected:
            ~TreeVisitor() = default;
        };
        void PreOrderTraverse(TreeVisitor& visitor)
        {
            visitor.EnterNode(this);
            for (auto& child : mChildren)
            {
                child.PreOrderTraverse(visitor);
            }
            visitor.LeaveNode(this);
        }
        template<typename Function>
        void PreOrderTraverseForEachTreeNode(Function callback)
        {
            class PrivateTreeVisitor : public TreeVisitor
            {
            public:
                PrivateTreeVisitor(Function callback) : mCallback(std::move(callback))
                {}
                virtual void EnterNode(TreeNode* node)
                {
                    mCallback(node);
                }
            private:
                Function mCallback;
            } visitor(std::move(callback));

            PreOrderTraverse(visitor);
        }

        class Visitor
        {
        public:
            // Called when the tree traversal algorithm enters a node.
            virtual void EnterNode(T* node) {}
            // Called when the tree traversal algorithm leaves a node.
            virtual void LeaveNode(T* node) {}
            // Called to check whether the tree traversal can finish early
            // without visiting the remainder of the nodes. When true is
            // returned the rest of the nodes are skipped and the algorithm
            // returns early. On false the tree traversal continues.
            virtual bool IsDone() const { return false; }
        protected:
            ~Visitor() = default;
        };

        class ConstVisitor
        {
        public:
            // Called when the tree traversal algorithm enters a node.
            virtual void EnterNode(const T* node) {}
            // Called when the tree traversal algorithm leaves a node.
            virtual void LeaveNode(const T* node) {}
            // Called to check whether the tree traversal can finish early
            // without visiting the remainder of the nodes. When true is
            // returned the rest of the nodes are skipped and the algorithm
            // returns early. On false the tree traversal continues.
            virtual bool IsDone() const { return false; }
        protected:
            ~ConstVisitor() = default;
        };
        // Pre-order traverse the tree. Pre-order means
        // entering the current node first and then ascending to the
        // children starting from the leftmost (0th child).
        void PreOrderTraverse(Visitor& visitor)
        {
            visitor.EnterNode(mNodeValue);
            for (auto& child : mChildren)
            {
                child.PreOrderTraverse(visitor);
                if (visitor.IsDone())
                    break;
            }
            visitor.LeaveNode(mNodeValue);
        }
        void PreOrderTraverse(ConstVisitor& visitor) const
        {
            visitor.EnterNode(const_cast<const T*>(mNodeValue));
            for (const auto& child : mChildren)
            {
                child.PreOrderTraverse(visitor);
                if (visitor.IsDone())
                    break;
            }
            visitor.LeaveNode(const_cast<const T*>(mNodeValue));
        }

        template<typename Function>
        void PreOrderTraverseForEach(Function callback)
        {
            class PrivateVisitor : public Visitor
            {
            public:
                PrivateVisitor(Function callback) : mCallback(callback)
                {}
                virtual void EnterNode(T* node) override
                {
                    mCallback(node);
                }
            private:
                Function mCallback;
            };
            PrivateVisitor visitor(std::move(callback));
            PreOrderTraverse(visitor);
        }

        template<typename Function>
        void PreOrderTraverseForEach(Function callback) const
        {
            class PrivateVisitor : public ConstVisitor
            {
            public:
                PrivateVisitor(Function callback) : mCallback(std::move(callback))
                {}
                virtual void EnterNode(const T* node) override
                {
                    mCallback(node);
                }
            private:
                Function mCallback;
            };
            PrivateVisitor visitor(std::move(callback));
            PreOrderTraverse(visitor);
        }

        // Get the value contained within this node if any.
        // For example the root node might not have any value.
        T* GetValue()
        { return mNodeValue; }
        const T* GetValue() const
        { return mNodeValue; }

        T* operator->()
        { return mNodeValue; }
        const T* operator->() const
        { return mNodeValue; }

        // Set the value referred to by this treenode.
        void SetValue(T* value)
        { mNodeValue = value; }

        // Get the number of child nodes this node has.
        size_t GetNumChildren() const
        { return mChildren.size(); }

        // Make a deep copy of this tree node.
        TreeNode Clone() const
        {
            return TreeNode(*this);
        }

        // get the number of nodes in the tree starting from this
        // node *including* this node itself.
        // I.e. if this node has no children then the num of nodes is 1.
        size_t GetNumNodes() const
        {
            size_t ret = 1;
            for (const auto& child : mChildren)
                ret += child.GetNumNodes();
            return ret;
        }

        // Get a reference to a child node by index.
        TreeNode& GetChildNode(size_t i)
        {
            ASSERT(i < mChildren.size());
            return mChildren[i];
        }
        TreeNode* FindChild(const T* value)
        {
            for (auto& child : mChildren)
            {
                if (child.mNodeValue == value)
                    return &child;
            }
            return nullptr;
        }
        // Get a reference to a child node by index.
        const TreeNode& GetChildNode(size_t i) const
        {
            ASSERT(i < mChildren.size());
            return mChildren[i];
        }
        const TreeNode* FindChild(const T* value) const
        {
            for (const auto& child : mChildren)
            {
                if (child.mNodeValue == value)
                    return &child;
            }
            return nullptr;
        }

        TreeNode* FindNodeByValue(const T* value)
        {
            if (mNodeValue == value)
                return this;
            for (auto& child : mChildren)
            {
                if (TreeNode* ret = child.FindNodeByValue(value))
                    return ret;
            }
            return nullptr;
        }
        const TreeNode* FindNodeByValue(const T* value) const
        {
            if (mNodeValue == value)
                return this;
            for (const auto& child : mChildren)
            {
                if (const TreeNode* ret = child.FindNodeByValue(value))
                    return ret;
            }
            return nullptr;
        }

        TreeNode* FindParent(TreeNode* child)
        {
            for (auto& maybe : mChildren)
            {
                if (&maybe == child)
                    return this;
            }
            for (auto& maybe : mChildren)
            {
                if (TreeNode* ret = maybe.FindParent(child))
                    return ret;
            }
            return nullptr;
        }
        const TreeNode* FindParent(const TreeNode* child) const
        {
            for (const auto& maybe : mChildren)
            {
                if (&maybe == child)
                    return this;
            }
            for (const auto&  maybe : mChildren)
            {
                if (const TreeNode* ret = maybe.FindParent(child))
                    return ret;
            }
            return nullptr;
        }

        // Append a new child node without any value to the list of children.
        TreeNode& AppendChild()
        {
            return AppendChild(TreeNode());
        }
        // Append a new child node to the list of children.
        // The new node is creaetd with the given value.
        TreeNode& AppendChild(T* value)
        {
            return AppendChild(TreeNode(value));
        }
        // Append the existing child node to the list of children.
        TreeNode& AppendChild(const TreeNode& node)
        {
            mChildren.push_back(node);
            return mChildren.back();
        }
        TreeNode& AppendChild(TreeNode&& node)
        {
            mChildren.push_back(std::move(node));
            return mChildren.back();
        }

        // Insert an existing child node to the list of children at a specific
        // loccation where the location is between 0 and N (N being the current
        // count of children). If the insertion index is equal to current count
        // of children the node is appended. Otherwise it's inserted into the
        // specific position. For example if the list of children is as follows.
        // A, B, C, D then A has index 0, B has index 1 etc.
        // inserting child E at index 1 creates the following list of children:
        // A, E, B, C, D
        // Function returns a reference to the new TreeNode that was created.
        TreeNode& InsertChild(const TreeNode& child, size_t i)
        {
            ASSERT(i <= mChildren.size());

            if (i == mChildren.size())
            {
                mChildren.push_back(child);
                return mChildren.back();
            }

            auto it = mChildren.begin();
            it += i;
            mChildren.insert(it, child);
            return mChildren[i];
        }
        TreeNode& InsertChild(size_t i)
        {
            return InsertChild(TreeNode(), i);
        }
        TreeNode& InsertChild(T* value, size_t i)
        {
            return InsertChild(TreeNode(value), i);
        }

        void DeleteChild(size_t i)
        {
            ASSERT(i < mChildren.size());
            auto it = mChildren.begin();
            std::advance(it, i);
            mChildren.erase(it);
        }
        void DeleteChild(TreeNode* child)
        {
            for (auto it = mChildren.begin(); it != mChildren.end(); ++it)
            {
                auto& maybe = *it;
                if (&maybe == child)
                {
                    mChildren.erase(it);
                    return;
                }
            }
        }

        TreeNode TakeChild(size_t i)
        {
            ASSERT(i < mChildren.size());
            auto ret = std::move(mChildren[i]);
            auto it = mChildren.begin();
            it += i;
            mChildren.erase(it);
            return ret;
        }

        void Clear()
        {
            mNodeValue = nullptr;
            mChildren.resize(0);
        }

        template<typename Serializer>
        nlohmann::json ToJson(const Serializer& serializer) const
        {
            nlohmann::json json;
            json["node"] = serializer.TreeNodeToJson(mNodeValue);
            for (const auto& child : mChildren)
            {
                // recurse
                json["children"].push_back(child.ToJson(serializer));
            }
            return json;
        }

        template<typename Serializer>
        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            json["node"] = Serializer::TreeNodeToJson(mNodeValue);
            for (const auto& child : mChildren)
            {
                // recurse
                json["children"].push_back(child.template ToJson<Serializer>());
            }
            return json;
        }


        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            json["node"] = TreeNodeToJson(mNodeValue);
            for (const auto& child : mChildren)
            {
                // recurse
                json["children"].push_back(child.ToJson());
            }
            return json;
        }

        static std::optional<TreeNode> FromJson(const nlohmann::json& json)
        {
            // using the type T as the serializer
            return TreeNode::FromJson<T>(json);
        }

        template<typename Serializer>
        static std::optional<TreeNode> FromJson(const nlohmann::json& json)
        {
            TreeNode ret;
            ret.mNodeValue = Serializer::TreeNodeFromJson(json["node"]);

            if (!json.contains("children"))
                return ret;

            for (const auto& json_c : json["children"].items())
            {
                auto child = TreeNode::FromJson<Serializer>(json_c.value());
                if (!child.has_value())
                    return std::nullopt;
                ret.mChildren.push_back(std::move(child.value()));
            }
            return ret;
        }

        template<typename Serializer>
        static std::optional<TreeNode> FromJson(const nlohmann::json& json, Serializer& serializer)
        {
            TreeNode ret;
            ret.mNodeValue = serializer.TreeNodeFromJson(json["node"]);

            if (!json.contains("children"))
                return ret;

            for (const auto& json_c : json["children"].items())
            {
                auto child = TreeNode::FromJson(json_c.value(), serializer);
                if (!child.has_value())
                    return std::nullopt;
                ret.mChildren.push_back(std::move(child.value()));
            }
            return ret;
        }
    private:
        // this is the actual value we refer to.
        // can be nullptr (for example for the root node).
        // we don't retain any ownership.
        T* mNodeValue = nullptr;
        // the child nodes.
        std::vector<TreeNode> mChildren;

    };
} // namespace
