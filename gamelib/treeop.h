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
#  include <glm/glm.hpp> // for glm::inverse
#include "warnpop.h"

#include <vector>
#include <algorithm> // for min/max

#include "graphics/types.h"
#include "graphics/transform.h"
#include "gamelib/tree.h"
#include "gamelib/types.h"

namespace game
{
    template<typename Node>
    struct RenderTreeFunctions {
        using RenderTree = game::TreeNode<Node>;

        static void CoarseHitTest(RenderTree& tree, float x, float y, std::vector<Node*>* hits, std::vector<glm::vec2>* hitbox_positions)
        {
            class Visitor : public RenderTree::Visitor
            {
            public:
                Visitor(const glm::vec4& point, std::vector<Node*>* hits, std::vector<glm::vec2>* boxes)
                        : mHitPoint(point)
                        , mHits(hits)
                        , mBoxes(boxes)
                {}

                virtual void EnterNode(Node* node) override
                {
                    if (!node)
                        return;

                    mTransform.Push(node->GetNodeTransform());
                    // using the model transform will put the coordinates in the drawable coordinate
                    // space, i.e. normalized coordinates.
                    mTransform.Push(node->GetModelTransform());

                    const auto& animation_to_node = glm::inverse(mTransform.GetAsMatrix());
                    const auto& hitpoint_in_node = animation_to_node * mHitPoint;

                    if (hitpoint_in_node.x >= 0.0f &&
                        hitpoint_in_node.x < 1.0f &&
                        hitpoint_in_node.y >= 0.0f &&
                        hitpoint_in_node.y < 1.0f)
                    {
                        mHits->push_back(node);
                        if (mBoxes)
                        {
                            const auto& size = node->GetSize();
                            mBoxes->push_back(glm::vec2(hitpoint_in_node.x * size.x, hitpoint_in_node.y * size.y));
                        }
                    }
                    mTransform.Pop();
                }
                virtual void LeaveNode(Node* node) override
                {
                    if (!node)
                        return;

                    mTransform.Pop();
                }
            private:
                const glm::vec4 mHitPoint;
                gfx::Transform mTransform;
                std::vector<Node*>* mHits = nullptr;
                std::vector<glm::vec2>* mBoxes = nullptr;
            };

            Visitor visitor(glm::vec4(x, y, 1.0f, 1.0f), hits, hitbox_positions);
            tree.PreOrderTraverse(visitor);
        }

        static void CoarseHitTest(const RenderTree& tree, float x, float y, std::vector<const Node*>* hits, std::vector<glm::vec2>* hitbox_positions)
        {
            class Visitor : public RenderTree::ConstVisitor
            {
            public:
                Visitor(const glm::vec4& point, std::vector<const Node*>* hits, std::vector<glm::vec2>* boxes)
                        : mHitPoint(point)
                        , mHits(hits)
                        , mBoxes(boxes)
                {}

                virtual void EnterNode(const Node* node) override
                {
                    if (!node)
                        return;

                    mTransform.Push(node->GetNodeTransform());
                    // using the model transform will put the coordinates in the drawable coordinate
                    // space, i.e. normalized coordinates.
                    mTransform.Push(node->GetModelTransform());

                    const auto& animation_to_node = glm::inverse(mTransform.GetAsMatrix());
                    const auto& hitpoint_in_node = animation_to_node * mHitPoint;

                    if (hitpoint_in_node.x >= 0.0f &&
                        hitpoint_in_node.x < 1.0f &&
                        hitpoint_in_node.y >= 0.0f &&
                        hitpoint_in_node.y < 1.0f)
                    {
                        mHits->push_back(node);
                        if (mBoxes)
                        {
                            const auto& size = node->GetSize();
                            mBoxes->push_back(glm::vec2(hitpoint_in_node.x * size.x, hitpoint_in_node.y * size.y));
                        }
                    }
                    mTransform.Pop();
                }
                virtual void LeaveNode(const Node* node) override
                {
                    if (!node)
                        return;

                    mTransform.Pop();
                }
            private:
                const glm::vec4 mHitPoint;
                gfx::Transform mTransform;
                std::vector<const Node*>* mHits = nullptr;
                std::vector<glm::vec2>* mBoxes = nullptr;
            };

            Visitor visitor(glm::vec4(x, y, 1.0f, 1.0f), hits, hitbox_positions);
            tree.PreOrderTraverse(visitor);
        }

        static glm::vec2 MapCoordsFromNode(const RenderTree& tree, float x, float y, const Node* node)
        {
            class Visitor : public RenderTree::ConstVisitor
            {
            public:
                Visitor(float x, float y, const Node* node)
                        : mX(x)
                        , mY(y)
                        , mNode(node)
                {}
                virtual void EnterNode(const Node* node) override
                {
                    if (!node)
                        return;

                    mTransform.Push(node->GetNodeTransform());

                    // if it's the node we're interested in
                    if (node == mNode)
                    {
                        const auto& size = mNode->GetSize();
                        mTransform.Push();
                        // offset the drawable size, don't use the scale operation
                        // because then the input would need to be in the model space (i.e. [0.0f, 1.0f])
                        mTransform.Translate(-size.x*0.5f, -size.y*0.5f);
                        const auto& mat = mTransform.GetAsMatrix();
                        const auto& vec = mat * glm::vec4(mX, mY, 1.0f, 1.0f);
                        mResult = glm::vec2(vec.x, vec.y);
                        mTransform.Pop();
                    }
                }
                virtual void LeaveNode(const Node* node) override
                {
                    if (!node)
                        return;
                    mTransform.Pop();
                }
                glm::vec2 GetResult() const
                { return mResult; }
            private:
                const float mX = 0.0f;
                const float mY = 0.0f;
                const Node* mNode = nullptr;
                glm::vec2 mResult;
                gfx::Transform mTransform;
            };

            Visitor visitor(x, y, node);
            tree.PreOrderTraverse(visitor);
            return visitor.GetResult();
        }

        static glm::vec2 MapCoordsToNode(const RenderTree& tree, float x, float y, const Node* node)
        {
            class Visitor : public RenderTree::ConstVisitor
            {
            public:
                Visitor(float x, float y, const Node* node)
                        : mCoords(x, y, 1.0f, 1.0f)
                        , mNode(node)
                {}
                virtual void EnterNode(const Node* node) override
                {
                    if (!node)
                        return;
                    mTransform.Push(node->GetNodeTransform());

                    if (node == mNode)
                    {
                        const auto& size = mNode->GetSize();
                        mTransform.Push();
                        // offset the drawable size, don't use the scale operation
                        // because then the output would be in the model space (i.e. [0.0f, 1.0f])
                        mTransform.Translate(-size.x*0.5f, -size.y*0.5f);
                        const auto& animation_to_node = glm::inverse(mTransform.GetAsMatrix());
                        const auto& vec = animation_to_node * mCoords;
                        mResult = glm::vec2(vec.x, vec.y);
                        mTransform.Pop();
                    }
                }
                virtual void LeaveNode(const Node* node) override
                {
                    if (!node)
                        return;
                    mTransform.Pop();
                }
                glm::vec2 GetResult() const
                { return mResult; }

            private:
                const glm::vec4 mCoords;
                const Node* mNode = nullptr;
                glm::vec2 mResult;
                gfx::Transform mTransform;
            };

            Visitor visitor(x, y, node);
            tree.PreOrderTraverse(visitor);
            return visitor.GetResult();
        }

        static FBox GetBoundingBox(const RenderTree& tree, const Node* node)
        {
            class Visitor : public RenderTree::ConstVisitor
            {
            public:
                Visitor(const Node* node) : mNode(node)
                {}
                virtual void EnterNode(const Node* node) override
                {
                    if (!node) return;

                    mTransform.Push(node->GetNodeTransform());
                    if (node == mNode)
                    {
                        mTransform.Push(node->GetModelTransform());
                        mBox.Transform(mTransform.GetAsMatrix());
                        mTransform.Pop();
                    }
                }
                virtual void LeaveNode(const Node* node) override
                {
                    if (!node) return;

                    mTransform.Pop();
                }
                FBox GetResult() const
                { return mBox; }
            private:
                const Node* const mNode = nullptr;
                gfx::Transform mTransform;
                FBox mBox;
            };
            Visitor visitor(node);
            tree.PreOrderTraverse(visitor);
            return visitor.GetResult();
        }

        static gfx::FRect GetBoundingRect(const RenderTree& tree, const Node* node)
        {
            class Visitor : public RenderTree::ConstVisitor
            {
            public:
                Visitor(const Node* node) : mNode(node)
                {}
                virtual void EnterNode(const Node* node) override
                {
                    if (!node)
                        return;
                    mTransform.Push(node->GetNodeTransform());
                    if (node == mNode)
                    {
                        mTransform.Push(node->GetModelTransform());
                        // node to animation matrix
                        // for each corner of a bounding volume compute new positions per
                        // the transformation matrix and then choose the min/max on each axis.
                        const auto& mat = mTransform.GetAsMatrix();
                        const auto& top_left  = mat * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                        const auto& top_right = mat * glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
                        const auto& bot_left  = mat * glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
                        const auto& bot_right = mat * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

                        // choose min/max on each axis.
                        const auto left = std::min(std::min(top_left.x, top_right.x),
                                                   std::min(bot_left.x, bot_right.x));
                        const auto right = std::max(std::max(top_left.x, top_right.x),
                                                    std::max(bot_left.x, bot_right.x));
                        const auto top = std::min(std::min(top_left.y, top_right.y),
                                                  std::min(bot_left.y, bot_right.y));
                        const auto bottom = std::max(std::max(top_left.y, top_right.y),
                                                     std::max(bot_left.y, bot_right.y));
                        mResult = gfx::FRect(left, top, right - left, bottom - top);

                        mTransform.Pop();
                    }
                }
                virtual void LeaveNode(const Node* node) override
                {
                    if (!node)
                        return;
                    mTransform.Pop();
                }
                gfx::FRect GetResult() const
                { return mResult; }
            private:
                const Node* mNode = nullptr;
                gfx::FRect mResult;
                gfx::Transform mTransform;
            };

            Visitor visitor(node);
            tree.PreOrderTraverse(visitor);
            return visitor.GetResult();
        }

        static gfx::FRect GetBoundingRect(const RenderTree& tree)
        {
            class Visitor : public RenderTree::ConstVisitor
            {
            public:
                virtual void EnterNode(const Node* node) override
                {
                    if (!node)
                        return;
                    mTransform.Push(node->GetNodeTransform());
                    mTransform.Push(node->GetModelTransform());
                    // node to animation matrix.
                    // for each corner of a bounding volume compute new positions per
                    // the transformation matrix and then choose the min/max on each axis.
                    const auto& mat = mTransform.GetAsMatrix();
                    const auto& top_left  = mat * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                    const auto& top_right = mat * glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
                    const auto& bot_left  = mat * glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
                    const auto& bot_right = mat * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

                    // choose min/max on each axis.
                    const auto left = std::min(std::min(top_left.x, top_right.x),
                                               std::min(bot_left.x, bot_right.x));
                    const auto right = std::max(std::max(top_left.x, top_right.x),
                                                std::max(bot_left.x, bot_right.x));
                    const auto top = std::min(std::min(top_left.y, top_right.y),
                                              std::min(bot_left.y, bot_right.y));
                    const auto bottom = std::max(std::max(top_left.y, top_right.y),
                                                 std::max(bot_left.y, bot_right.y));
                    const auto box = gfx::FRect(left, top, right - left, bottom - top);
                    if (mResult.IsEmpty())
                        mResult = box;
                    else mResult = Union(mResult, box);

                    mTransform.Pop();
                }
                virtual void LeaveNode(const Node* node) override
                {
                    if (!node)
                        return;
                    mTransform.Pop();
                }
                gfx::FRect GetResult() const
                { return mResult; }
            private:
                gfx::FRect mResult;
                gfx::Transform mTransform;
            };

            Visitor visitor;
            tree.PreOrderTraverse(visitor);
            return visitor.GetResult();
        }
    }; // RenderTreeFunctions

} // namespace