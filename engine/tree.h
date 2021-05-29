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

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>
#include <unordered_map>

#include "base/assert.h"
#include "base/utility.h"

namespace game
{
    // Non-intrusive, non-owning tree structure for maintaining
    // parent-child relationships. This can be used to defined
    // things such as the scene's render hierarchy (i.e. the
    // scene graph)
    // The root of the tree can be denoted by using a special
    // pointer value, the nullptr.
    template<typename Element>
    class RenderTree
    {
    public:
        template<typename T>
        class TVisitor
        {
        public:
            virtual ~TVisitor() = default;
            // Called when the tree traversal algorithm enters a node.
            virtual void EnterNode(T* node) {}
            // Called when the tree traversal algorithm leaves a node.
            virtual void LeaveNode(T* node) {}
            // Called to check whether the tree traversal can finish early
            // without visiting the remainder of the nodes. When true is
            // returned the rest of the nodes are skipped and the algorithm
            // returns early. On false the tree traversal continues.
            virtual bool IsDone() const { return false; }
        private:
        };

        // Mutating visitor for visiting nodes in the tree.
        using Visitor = TVisitor<Element>;
        // Non-mutating visitor for visiting nodes in the tree.
        using ConstVisitor = TVisitor<const Element>;

        // Clear the tree. After this the tree is empty
        // and contains no nodes.
        void Clear()
        {
            mParents.clear();
            mChildren.clear();
        }

        void PreOrderTraverse(Visitor& visitor, Element* parent = nullptr)
        { PreOrderTraverse<Element>(visitor, parent); }

        void PreOrderTraverse(ConstVisitor& visitor, const Element* parent = nullptr) const
        { PreOrderTraverse<const Element>(visitor, parent); }

        template<typename Function>
        void PreOrderTraverseForEach(Function callback, Element* parent = nullptr)
        { PreOrderTraverseForEach<Element>(std::move(callback), parent); }

        template<typename Function>
        void PreOrderTraverseForEach(Function callback, const Element* parent = nullptr) const
        { PreOrderTraverseForEach<const Element>(std::move(callback), parent); }

        // Convenience operation for moving a child node to a new parent.
        void ReparentChild(const Element* parent, const Element* child)
        {
            BreakChild(child);
            LinkChild(parent, child);
        }
        // Delete the node and all of its ascendants from the tree.
        // If the child doesn't exist in the tree then nothing is done.
        void DeleteNode(const Element* child)
        {
            auto it = mParents.find(child);
            if (it == mParents.end())
                return;

            auto& children = mChildren[it->second];
            for (auto it = children.begin(); it != children.end(); ++it)
            {
                const Element* value = *it;
                if (value == child)
                {
                    DeleteChildren(value);
                    children.erase(it);
                    break;
                }
            }
            mParents.erase(child);
        }

        // Delete all the children of a parent.
        // If the parent doesn't exist in the tree nothing is done.
        void DeleteChildren(const Element* parent)
        {
            auto it = mChildren.find(parent);
            if (it == mChildren.end())
                return;
            auto& children = it->second;
            for (auto* child : children)
            {
                DeleteChildren(child);
                mParents.erase(child);
            }
            children.clear();
        }

        // Link a child node to a parent node.
        // The child must not be linked to any parent. It'd be
        // illegal to have a child with two parents nodes.
        // If the child should be moved from one parent to another
        // use ReparentChild or BreakChild followed by LinkChild.
        void LinkChild(const Element* parent, const Element* child)
        {
            ASSERT(mParents.find(child) == mParents.end());
            mChildren[parent].push_back(child);
            mParents[child] = parent;
        }
        // Break a child node away from its parent. The descendants
        // of child are still retained as node's children.
        // If the child node is currently not linked to any parent
        // nothing is done.
        void BreakChild(const Element* child)
        {
            auto it = mParents.find(child);
            if (it == mParents.end())
                return;

            const auto* parent = it->second;
            auto& children = mChildren[parent];
            for (auto it = children.begin(); it != children.end(); ++it)
            {
                const Element* value = *it;
                if (value == child)
                {
                    children.erase(it);
                    break;
                }
            }
            mParents.erase(it);
        }

        // Get the parent node of a child node.
        // The child node must exist in the tree.
        Element* GetParent(const Element* child)
        {
            auto it = mParents.find(child);
            ASSERT(it != mParents.end());
            return const_cast<Element*>(it->second);
        }
        const Element* GetParent(const Element* child) const
        {
            auto it = mParents.find(child);
            ASSERT(it != mParents.end());
            return it->second;
        }

        // Returns true if this node exists in this tree.
        bool HasNode(const Element* node) const
        {
            // all nodes have a parent, thus if the node exists
            // in the tree it also exists in the child->Parent map
            return mParents.find(node) != mParents.end();
        }

        // Returns true if the node has a parent. All nodes except
        // for the *Root* node have a parent.
        bool HasParent(const Element* node) const
        {
            return mParents.find(node) != mParents.end();
        }

        template<typename Serializer>
        nlohmann::json ToJson(const Serializer& to_json, const Element* parent = nullptr) const
        {
            nlohmann::json json;
            json["node"] = to_json(parent);
            auto it = mChildren.find(parent);
            if (it != mChildren.end())
            {
                const auto& children = it->second;
                for (auto *child : children)
                {
                    json["children"].push_back(ToJson(to_json, child));
                }
            }
            return json;
        }
        // Build render tree from a JSON object. The serializer object
        // should create a new Element instance and return the pointer to it.
        template<typename Serializer>
        void FromJson(const nlohmann::json& json, const Serializer& from_json)
        {
            const Element* root = from_json(json["node"]);
            if (!json.contains("children"))
                return;
            for (const auto& js : json["children"].items())
                FromJson(js.value(), from_json, root);
        }

        // Build an equivalent tree (in terms of topology) based on the
        // source tree while remapping nodes from one instance to another
        // through the map function.
        template<typename T, typename MapFunction>
        void FromTree(const RenderTree<T>& tree, const MapFunction& map_node)
        {
            for (const auto& pair : tree.mChildren)
            {
                const auto* parent   = pair.first;
                const auto& children = pair.second;
                for (const auto* child : children)
                    LinkChild(map_node(parent), map_node(child));
            }
        }
    private:
        template<typename Serializer>
        void FromJson(const nlohmann::json& json, const Serializer& from_json, const Element* parent)
        {
            const Element* node = from_json(json["node"]);
            mChildren[parent].push_back(node);
            mParents[node] = parent;
            if (!json.contains("children"))
                return;

            for (const auto& js : json["children"].items())
            {
                FromJson(js.value(), from_json, node);
            }
        }
        template<typename T>
        void PreOrderTraverse(TVisitor<T>& visitor, T* parent = nullptr) const
        {
            visitor.EnterNode(parent);
            auto it = mChildren.find(parent);
            if (it != mChildren.end())
            {
                const auto& children = it->second;
                for (auto *child : children)
                {
                    PreOrderTraverse(visitor, const_cast<Element*>(child));
                    if (visitor.IsDone())
                        break;
                }
            }
            visitor.LeaveNode(parent);
        }
        template<typename T, typename Function>
        void PreOrderTraverseForEach(Function callback, T* parent = nullptr) const
        {
            class PrivateVisitor : public TVisitor<T> {
            public:
                PrivateVisitor(Function callback) : mCallback(std::move(callback))
                {}
                virtual void EnterNode(T* node) override
                { mCallback(node); }
            private:
                Function mCallback;
            };
            PrivateVisitor visitor(std::move(callback));
            PreOrderTraverse(visitor, parent);
        }
    private:
        using ChildList = std::vector<const Element*>;
        // lookup table for mapping parents to their children
        std::unordered_map<const Element*, ChildList> mChildren;
        // lookup table for mapping children to their parents
        std::unordered_map<const Element*, const Element*> mParents;

        template<typename T> friend class RenderTree;
    };

} // namespace
