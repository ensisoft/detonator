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

#include "config.h"

#include "warnpush.h"
#  include <glm/glm.hpp> // for glm::inverse
#include "warnpop.h"

#include <map>
#include <cmath>

#include "base/logging.h"
#include "base/assert.h"
#include "base/utility.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/drawing.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "gamelib/animation.h"
#include "gamelib/classlib.h"

namespace {

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
                : mX(x), mY(y), mNode(node)
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

namespace game
{

AnimationNodeClass::AnimationNodeClass()
{
    mId = base::RandomString(10);
    mBitFlags.set(Flags::VisibleInEditor, true);
    mBitFlags.set(Flags::DoesRender, true);
    mBitFlags.set(Flags::UpdateMaterial, true);
    mBitFlags.set(Flags::UpdateDrawable, true);
    mBitFlags.set(Flags::RestartDrawable, true);
    mBitFlags.set(Flags::OverrideAlpha, false);
}

glm::mat4 AnimationNodeClass::GetNodeTransform() const
{
    // transformation order is the order they're
    // written here.
    gfx::Transform transform;
    transform.Scale(mScale);
    transform.Rotate(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}

glm::mat4 AnimationNodeClass::GetModelTransform() const
{
    gfx::Transform transform;
    transform.Scale(mSize);
    // offset the object so that the center of the shape is aligned
    // with the position parameter.
    transform.Translate(-mSize.x * 0.5f, -mSize.y * 0.5f);
    return transform.GetAsMatrix();
}

std::size_t AnimationNodeClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mMaterialId);
    hash = base::hash_combine(hash, mDrawableId);
    hash = base::hash_combine(hash, mPosition);
    hash = base::hash_combine(hash, mSize);
    hash = base::hash_combine(hash, mScale);
    hash = base::hash_combine(hash, mRotation);
    hash = base::hash_combine(hash, mAlpha);
    hash = base::hash_combine(hash, mLayer);
    hash = base::hash_combine(hash, mRenderPass);
    hash = base::hash_combine(hash, mRenderStyle);
    hash = base::hash_combine(hash, mLineWidth);
    hash = base::hash_combine(hash, mBitFlags.value());
    return hash;
}

void AnimationNodeClass::Update(float time, float dt)
{
}

void AnimationNodeClass::Reset()
{
}

AnimationNodeClass AnimationNodeClass::Clone() const
{
    AnimationNodeClass ret(*this);
    ret.mId = base::RandomString(10);
    return ret;
}

nlohmann::json AnimationNodeClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mId);
    base::JsonWrite(json, "name", mName);
    base::JsonWrite(json, "material", mMaterialId);
    base::JsonWrite(json, "drawable", mDrawableId);
    base::JsonWrite(json, "position", mPosition);
    base::JsonWrite(json, "size", mSize);
    base::JsonWrite(json, "scale", mScale);
    base::JsonWrite(json, "rotation", mRotation);
    base::JsonWrite(json, "alpha", mAlpha);
    base::JsonWrite(json, "layer", mLayer);
    base::JsonWrite(json, "render_pass", mRenderPass);
    base::JsonWrite(json, "render_style", mRenderStyle);
    base::JsonWrite(json, "linewidth", mLineWidth);
    base::JsonWrite(json, "bitflags", mBitFlags.value());
    return json;
}

// static
std::optional<AnimationNodeClass> AnimationNodeClass::FromJson(const nlohmann::json& object)
{
    std::uint32_t bitflags = 0;

    AnimationNodeClass ret;
    if (!base::JsonReadSafe(object, "id", &ret.mId) ||
        !base::JsonReadSafe(object, "name", &ret.mName) ||
        !base::JsonReadSafe(object, "material", &ret.mMaterialId) ||
        !base::JsonReadSafe(object, "drawable", &ret.mDrawableId) ||
        !base::JsonReadSafe(object, "position", &ret.mPosition) ||
        !base::JsonReadSafe(object, "size", &ret.mSize) ||
        !base::JsonReadSafe(object, "scale", &ret.mScale) ||
        !base::JsonReadSafe(object, "rotation", &ret.mRotation) ||
        !base::JsonReadSafe(object, "alpha", &ret.mAlpha) ||
        !base::JsonReadSafe(object, "layer", &ret.mLayer) ||
        !base::JsonReadSafe(object, "render_pass", &ret.mRenderPass) ||
        !base::JsonReadSafe(object, "render_style", &ret.mRenderStyle) ||
        !base::JsonReadSafe(object, "linewidth", &ret.mLineWidth) ||
        !base::JsonReadSafe(object, "bitflags", &bitflags))
        return std::nullopt;

    ret.mBitFlags.set_from_value(bitflags);
    return ret;
}

void AnimationNode::Reset()
{
    // initialize instance state based on the class's initial state.
    mPosition = mClass->GetTranslation();
    mSize     = mClass->GetSize();
    mScale    = mClass->GetScale();
    mRotation = mClass->GetRotation();
    mAlpha    = mClass->GetAlpha();
}

void AnimationNode::Update(float time, float dt)
{
}

glm::mat4 AnimationNode::GetNodeTransform() const
{
    // transformation order is the order they're
    // written here.
    gfx::Transform transform;
    transform.Scale(mScale);
    transform.Rotate(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}

glm::mat4 AnimationNode::GetModelTransform() const
{
    gfx::Transform transform;
    transform.Scale(mSize);
    // offset the object so that the center of the shape is aligned
    // with the position parameter.
    transform.Translate(-mSize.x * 0.5f, -mSize.y * 0.5f);
    return transform.GetAsMatrix();
}

void AnimationTrack::Update(float dt)
{
    const auto duration = mClass->GetDuration();

    mCurrentTime = math::clamp(0.0f, duration, mCurrentTime + dt);
}

void AnimationTrack::Apply(AnimationNode& node) const
{
    const auto duration = mClass->GetDuration();
    const auto pos = mCurrentTime / duration;

    // todo: keep the tracks in some smarter data structure or perhaps
    // in a sorted vector and then binary search.
    for (auto& track : mTracks)
    {
        if (track.node != node.GetClassId())
            continue;

        const auto start = track.actuator->GetStartTime();
        const auto len   = track.actuator->GetDuration();
        const auto end   = math::clamp(0.0f, 1.0f, start + len);
        if (pos < start)
            continue;
        else if (pos >= end)
        {
            if (!track.ended)
            {
                track.actuator->Finish(node);
                track.ended = true;
            }
            continue;
        }
        if (!track.started)
        {
            track.actuator->Start(node);
            track.started = true;
        }
        const auto t = math::clamp(0.0f, 1.0f, (pos - start) / len);
        track.actuator->Apply(node, t);
    }
}

void AnimationTrack::Restart()
{
    for (auto& track : mTracks)
    {
        ASSERT(track.started);
        ASSERT(track.ended);
        track.started = false;
        track.ended   = false;
    }
    mCurrentTime = 0.0f;
}

bool AnimationTrack::IsComplete() const
{
    for (const auto& track : mTracks)
    {
        if (!track.ended)
            return false;
    }
    if (mCurrentTime >= mClass->GetDuration())
        return true;
    return false;
}

AnimationClass::AnimationClass(const AnimationClass& other)
{
    mId = other.mId;

    // make a deep copy of the nodes.o
    for (const auto& node : other.mNodes)
    {
        mNodes.push_back(std::make_unique<AnimationNodeClass>(*node));
    }

    // make a deep copy of the animation tracks
    for (const auto& track : other.mAnimationTracks)
    {
        mAnimationTracks.push_back(std::make_unique<AnimationTrackClass>(*track));
    }

    // use the json serialization setup the copy of the
    // render tree.
    nlohmann::json json = other.mRenderTree.ToJson(other);
    // build our render tree.
    mRenderTree = RenderTree::FromJson(json, *this).value();
}

AnimationNodeClass* AnimationClass::AddNode(AnimationNodeClass&& node)
{
#if true
    for (const auto& old : mNodes)
    {
        ASSERT(old->GetClassId() != node.GetClassId());
    }
#endif
    mNodes.push_back(std::make_shared<AnimationNodeClass>(std::move(node)));
    return mNodes.back().get();
}
AnimationNodeClass* AnimationClass::AddNode(const AnimationNodeClass& node)
{
#if true
    for (const auto& old : mNodes)
    {
        ASSERT(old->GetClassId() != node.GetClassId());
    }
#endif
    mNodes.push_back(std::make_shared<AnimationNodeClass>(node));
    return mNodes.back().get();
}

AnimationNodeClass* AnimationClass::AddNode(std::unique_ptr<AnimationNodeClass> node)
{
#if true
    for (const auto& old : mNodes)
    {
        ASSERT(old->GetClassId() != node->GetClassId());
    }
#endif
    mNodes.push_back(std::move(node));
    return mNodes.back().get();
}

void AnimationClass::DeleteNodeByIndex(size_t i)
{
    ASSERT(i < mNodes.size());
    auto it = std::begin(mNodes);
    std::advance(it, i);
    mNodes.erase(it);
}

bool AnimationClass::DeleteNodeById(const std::string& id)
{
    for (auto it = mNodes.begin(); it != mNodes.end(); ++it)
    {
        if ((*it)->GetClassId() == id)
        {
            mNodes.erase(it);
            return true;
        }
    }
    return false;
}

bool AnimationClass::DeleteNodeByName(const std::string& name)
{
    for (auto it = mNodes.begin(); it != mNodes.end(); ++it)
    {
        if ((*it)->GetName() == name)
        {
            mNodes.erase(it);
            return true;
        }
    }
    return false;
}

AnimationNodeClass& AnimationClass::GetNode(size_t i)
{
    ASSERT(i < mNodes.size());
    return *mNodes[i];
}
AnimationNodeClass* AnimationClass::FindNodeByName(const std::string& name)
{
    for (auto& node : mNodes)
        if (node->GetName() == name) return node.get();
    return nullptr;
}
AnimationNodeClass* AnimationClass::FindNodeById(const std::string& id)
{
    for (auto& node : mNodes)
        if (node->GetClassId() == id) return node.get();
    return nullptr;
}

const AnimationNodeClass& AnimationClass::GetNode(size_t i) const
{
    ASSERT(i < mNodes.size());
    return *mNodes[i];
}
const AnimationNodeClass* AnimationClass::FindNodeByName(const std::string& name) const
{
    for (const auto& node : mNodes)
        if (node->GetName() == name) return node.get();
    return nullptr;
}
const AnimationNodeClass* AnimationClass::FindNodeById(const std::string& id) const
{
    for (const auto& node : mNodes)
        if (node->GetClassId() == id) return node.get();
    return nullptr;
}

std::shared_ptr<const AnimationNodeClass> AnimationClass::GetSharedAnimationNodeClass(size_t i) const
{
    ASSERT(i < mNodes.size());
    return mNodes[i];
}

AnimationTrackClass* AnimationClass::AddAnimationTrack(AnimationTrackClass&& track)
{
    mAnimationTracks.push_back(std::make_shared<AnimationTrackClass>(std::move(track)));
    return mAnimationTracks.back().get();
}

AnimationTrackClass* AnimationClass::AddAnimationTrack(const AnimationTrackClass& track)
{
    mAnimationTracks.push_back(std::make_shared<AnimationTrackClass>(track));
    return mAnimationTracks.back().get();
}

AnimationTrackClass* AnimationClass::AddAnimationTrack(std::unique_ptr<AnimationTrackClass> track)
{
    mAnimationTracks.push_back(std::move(track));
    return mAnimationTracks.back().get();
}

void AnimationClass::DeleteAnimationTrack(size_t i)
{
    ASSERT(i < mAnimationTracks.size());
    auto it = mAnimationTracks.begin();
    std::advance(it, i);
    mAnimationTracks.erase(it);
}

bool AnimationClass::DeleteAnimationTrackByName(const std::string& name)
{
    for (auto it = mAnimationTracks.begin(); it != mAnimationTracks.end(); ++it)
    {
        if ((*it)->GetName() == name) {
            mAnimationTracks.erase(it);
            return true;
        }
    }
    return false;
}

bool AnimationClass::DeleteAnimationTrackById(const std::string& id)
{
    for (auto it = mAnimationTracks.begin(); it != mAnimationTracks.end(); ++it)
    {
        if ((*it)->GetId() == id) {
            mAnimationTracks.erase(it);
            return true;
        }
    }
    return false;
}

AnimationTrackClass& AnimationClass::GetAnimationTrack(size_t i)
{
    ASSERT(i < mAnimationTracks.size());
    return *mAnimationTracks[i].get();
}

AnimationTrackClass* AnimationClass::FindAnimationTrackByName(const std::string& name)
{
    for (const auto& klass : mAnimationTracks)
    {
        if (klass->GetName() == name)
            return klass.get();
    }
    return nullptr;
}

const AnimationTrackClass& AnimationClass::GetAnimationTrack(size_t i) const
{
    ASSERT(i < mAnimationTracks.size());
    return *mAnimationTracks[i].get();
}

const AnimationTrackClass* AnimationClass::FindAnimationTrackByName(const std::string& name) const
{
    for (const auto& klass : mAnimationTracks)
    {
        if (klass->GetName() == name)
            return klass.get();
    }
    return nullptr;
}

std::shared_ptr<const AnimationTrackClass> AnimationClass::GetSharedAnimationTrackClass(size_t i) const
{
    ASSERT(i < mAnimationTracks.size());
    return mAnimationTracks[i];
}

void AnimationClass::Update(float time, float dt)
{
    for (auto& node : mNodes)
        node->Update(time, dt);
}

void AnimationClass::Reset()
{
    for (auto& node : mNodes)
        node->Reset();
}

void AnimationClass::CoarseHitTest(float x, float y, std::vector<AnimationNodeClass*>* hits,
    std::vector<glm::vec2>* hitbox_positions)
{
    RenderTreeFunctions<AnimationNodeClass>::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}
void AnimationClass::CoarseHitTest(float x, float y, std::vector<const AnimationNodeClass*>* hits,
    std::vector<glm::vec2>* hitbox_positions) const
{
    RenderTreeFunctions<AnimationNodeClass>::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}
glm::vec2 AnimationClass::MapCoordsFromNode(float x, float y, const AnimationNodeClass* node) const
{
    return RenderTreeFunctions<AnimationNodeClass>::MapCoordsFromNode(mRenderTree, x, y, node);
}

glm::vec2 AnimationClass::MapCoordsToNode(float x, float y, const AnimationNodeClass* node) const
{
    return RenderTreeFunctions<AnimationNodeClass>::MapCoordsToNode(mRenderTree, x, y, node);
}

gfx::FRect AnimationClass::GetBoundingRect(const AnimationNodeClass* node) const
{
    return RenderTreeFunctions<AnimationNodeClass>::GetBoundingRect(mRenderTree, node);
}

gfx::FRect AnimationClass::GetBoundingRect() const
{
    return RenderTreeFunctions<AnimationNodeClass>::GetBoundingRect(mRenderTree);
}

std::size_t AnimationClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    // include the node hashes in the animation hash
    // this covers both the node values and their traversal order
    mRenderTree.PreOrderTraverseForEach([&](const AnimationNodeClass* node) {
        if (node == nullptr)
            return;
        hash = base::hash_combine(hash, node->GetHash());
    });
    for (const auto& track : mAnimationTracks)
        hash = base::hash_combine(hash, track->GetHash());
    return hash;
}

nlohmann::json AnimationClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mId);
    for (const auto& node : mNodes)
    {
        json["nodes"].push_back(node->ToJson());
    }
    for (const auto& track : mAnimationTracks)
    {
        json["tracks"].push_back(track->ToJson());
    }

    using Serializer = AnimationClass;
    json["render_tree"] = mRenderTree.ToJson<Serializer>();
    return json;
}

AnimationNodeClass* AnimationClass::TreeNodeFromJson(const nlohmann::json& json)
{
    if (!json.contains("id")) // root node has no id
        return nullptr;

    const std::string& id = json["id"];
    for (auto& it : mNodes)
        if (it->GetClassId() == id) return it.get();

    BUG("No such node found.");
    return nullptr;
}

// static
std::optional<AnimationClass> AnimationClass::FromJson(const nlohmann::json& object)
{
    AnimationClass ret;
    if (!base::JsonReadSafe(object, "id", &ret.mId))
        return std::nullopt;

    if (object.contains("nodes"))
    {
        for (const auto& json : object["nodes"].items())
        {
            std::optional<AnimationNodeClass> comp = AnimationNodeClass::FromJson(json.value());
            if (!comp.has_value())
                return std::nullopt;
            ret.mNodes.push_back(std::make_shared<AnimationNodeClass>(std::move(comp.value())));
        }
    }
    if (object.contains("tracks"))
    {
        for (const auto& json : object["tracks"].items())
        {
            std::optional<AnimationTrackClass> track = AnimationTrackClass::FromJson(json.value());
            if (!track.has_value())
                return std::nullopt;
            ret.mAnimationTracks.push_back(std::make_shared<AnimationTrackClass>(std::move(track.value())));
        }
    }

    auto& serializer = ret;

    auto render_tree = RenderTree::FromJson(object["render_tree"], serializer);
    if (!render_tree.has_value())
        return std::nullopt;
    ret.mRenderTree = std::move(render_tree.value());
    return ret;
}

// static
nlohmann::json AnimationClass::TreeNodeToJson(const AnimationNodeClass* node)
{
    // do only shallow serialization of the animation node,
    // i.e. only record the id so that we can restore the node
    // later on load based on the ID.
    nlohmann::json ret;
    if (node)
        ret["id"] = node->GetClassId();
    return ret;
}

AnimationClass AnimationClass::Clone() const
{
    AnimationClass ret;

    struct Serializer {
        AnimationNodeClass* TreeNodeFromJson(const nlohmann::json& json)
        {
            if (!json.contains("id")) // root node has no id
                return nullptr;
            const std::string& old_id = json["id"];
            const std::string& new_id = idmap[old_id];
            auto* ret = nodes[new_id];
            ASSERT(ret != nullptr && "No such node found.");
            return ret;
        }
        std::unordered_map<std::string, std::string> idmap;
        std::unordered_map<std::string, AnimationNodeClass*> nodes;
    };
    Serializer serializer;

    // make a deep copy of the nodes.
    for (const auto& node : mNodes)
    {
        auto clone = std::make_unique<AnimationNodeClass>(node->Clone());
        serializer.idmap[node->GetClassId()] = clone->GetClassId();
        serializer.nodes[clone->GetClassId()] = clone.get();
        ret.mNodes.push_back(std::move(clone));
    }

    // make a deep copy of the animation tracks
    for (const auto& track : mAnimationTracks)
    {
        ret.mAnimationTracks.push_back(std::make_unique<AnimationTrackClass>(track->Clone()));
    }

    // use the json serialization setup the copy of the
    // render tree.
    nlohmann::json json = mRenderTree.ToJson(*this);
    // build our render tree.
    ret.mRenderTree = RenderTree::FromJson(json, serializer).value();
    return ret;
}

AnimationClass& AnimationClass::operator=(const AnimationClass& other)
{
    if (this == &other)
        return *this;

    AnimationClass copy(other);
    mId = std::move(copy.mId);
    mNodes = std::move(copy.mNodes);
    mAnimationTracks = std::move(copy.mAnimationTracks);
    mRenderTree = copy.mRenderTree;
    return *this;
}

Animation::Animation(const std::shared_ptr<const AnimationClass>& klass)
    : mClass(klass)
{
    // build render tree, first ceate instances of all node classes
    // then build the render tree based on the node instances
    for (size_t i=0; i<mClass->GetNumNodes(); ++i)
    {
        mNodes.push_back(std::make_unique<AnimationNode>(mClass->GetSharedAnimationNodeClass(i)));
    }
    // rebuild the render tree through JSON serialization
    nlohmann::json json = mClass->GetRenderTree().ToJson(*mClass);

    mRenderTree = RenderTree::FromJson(json, *this).value();

}
Animation::Animation(const AnimationClass& klass)
    : Animation(std::make_shared<AnimationClass>(klass))
{}

void Animation::Update(float dt)
{
    mCurrentTime += dt;

    // note that the order here is important.
    // The animation track objects may alter the state
    // of some nodes (for example the alpha override value)
    // so first update all the nodes and then apply
    // animation track changes.

    for (auto& node : mNodes)
    {
        node->Update(mCurrentTime, dt);
    }
    if (!mAnimationTrack)
        return;

    mAnimationTrack->Update(dt);
    for (auto& node : mNodes)
    {
        mAnimationTrack->Apply(*node);
    }

    if (!mAnimationTrack->IsComplete())
        return;

    DEBUG("AnimationTrack '%1' completed.", mAnimationTrack->GetName());

    if (mAnimationTrack->IsLooping())
    {
        mAnimationTrack->Restart();
        for (auto& node : mNodes)
        {
            node->Reset();
        }
        return;
    }
    mAnimationTrack.reset();
}

void Animation::Play(std::unique_ptr<AnimationTrack> track)
{
    // todo: what to do if there's a previous track ?
    // possibilities: reset or queue?
    mAnimationTrack = std::move(track);
}

void Animation::PlayByName(const std::string& name)
{
    for (size_t i=0; i<mClass->GetNumTracks(); ++i)
    {
        const auto& klass = mClass->GetSharedAnimationTrackClass(i);
        if (klass->GetName() != name)
            continue;
        auto track = std::make_unique<AnimationTrack>(klass);
        Play(std::move(track));
        return;
    }
}

void Animation::PlayById(const std::string& id)
{
    for (size_t i=0; i<mClass->GetNumTracks(); ++i)
    {
        const auto& klass = mClass->GetSharedAnimationTrackClass(i);
        if (klass->GetId() != id)
            continue;
        auto track = std::make_unique<AnimationTrack>(klass);
        Play(std::move(track));
        return;
    }
}

bool Animation::IsPlaying() const
{
    return !!mAnimationTrack;
}

void Animation::CoarseHitTest(float x, float y, std::vector<AnimationNode*>* hits,
    std::vector<glm::vec2>* hitbox_positions)
{
    RenderTreeFunctions<AnimationNode>::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}

void Animation::CoarseHitTest(float x, float y, std::vector<const AnimationNode*>* hits,
    std::vector<glm::vec2>* hitbox_positions) const
{
    RenderTreeFunctions<AnimationNode>::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}

glm::vec2 Animation::MapCoordsFromNode(float x, float y, const AnimationNode* node) const
{
    return RenderTreeFunctions<AnimationNode>::MapCoordsFromNode(mRenderTree, x, y, node);
}

glm::vec2 Animation::MapCoordsToNode(float x, float y, const AnimationNode* node) const
{
    return RenderTreeFunctions<AnimationNode>::MapCoordsToNode(mRenderTree, x, y, node);
}

gfx::FRect Animation::GetBoundingRect(const AnimationNode* node) const
{
    return RenderTreeFunctions<AnimationNode>::GetBoundingRect(mRenderTree, node);
}

gfx::FRect Animation::GetBoundingRect() const
{
    return RenderTreeFunctions<AnimationNode>::GetBoundingRect(mRenderTree);
}

void Animation::Reset()
{
    for (auto& node : mNodes)
    {
        node->Reset();
    }
    mCurrentTime = 0.0f;
    mAnimationTrack.release();
}

AnimationNode* Animation::FindNodeByName(const std::string& name)
{
    for (auto& node : mNodes)
        if (node->GetName() == name) return node.get();
    return nullptr;
}
AnimationNode* Animation::FindNodeById(const std::string& id)
{
    for (auto& node : mNodes)
        if (node->GetClassId() == id) return node.get();
    return nullptr;
}
const AnimationNode* Animation::FindNodeByName(const std::string& name) const
{
    for (const auto& node : mNodes)
        if (node->GetName() == name) return node.get();
    return nullptr;
}
const AnimationNode* Animation::FindNodeById(const std::string& id) const
{
    for (const auto& node : mNodes)
        if (node->GetClassId() == id) return node.get();
    return nullptr;
}

AnimationNode* Animation::TreeNodeFromJson(const nlohmann::json& json)
{
    if (!json.contains("id")) // root node has no id
        return nullptr;
    const std::string& id = json["id"];
    for (auto& it : mNodes)
        if (it->GetClassId() == id) return it.get();

    BUG("No such node found");
    return nullptr;
}

std::unique_ptr<Animation> CreateAnimationInstance(const std::shared_ptr<const AnimationClass>& klass)
{
    return std::make_unique<Animation>(klass);
}

std::unique_ptr<AnimationTrack> CreateAnimationTrackInstance(const std::shared_ptr<const AnimationTrackClass>& klass)
{
    return std::make_unique<AnimationTrack>(klass);
}

} // namespace
