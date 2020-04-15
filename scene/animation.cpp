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

#include <map>

#include "base/logging.h"
#include "base/assert.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/drawing.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "animation.h"
#include "gfxfactory.h"

namespace scene
{

void AnimationNode::Draw(gfx::Painter& painter, gfx::Transform& transform) const
{
    if (!mDrawable || !mMaterial)
        return;

    mMaterial->SetRuntime(mTime - mStartTime);

    // begin the transformation scope for this animation component.
    transform.Push();
    transform.Scale(mSize);
    transform.Translate(-mSize.x * 0.5f, -mSize.y * 0.5f);
    transform.Rotate(mRotation);
    transform.Translate(mSize.x * 0.5f, mSize.y * 0.5f);
    transform.Translate(mPosition);

    // if we had recusive structure i.e. component could contain
    // components we'd need to deal with the resizing so that it'd
    // only apply to this component and then possibly have a
    // scaling factor that would apply to the whole subtree
    // staring from this node.
    painter.Draw(*mDrawable, transform, *mMaterial);


    // pop our scope.
    transform.Pop();
}

bool AnimationNode::Update(float dt)
{
    // disable this for now, need to figure out the time/timeline
    // related functionality.
    /*
    mTime += dt;
    if (mTime < mStartTime)
        return true;
    if (mTime - mStartTime > mLifetime)
        return false;
    */
    mTime += dt;

    if (mDrawable)
        mDrawable->Update(dt);

    return true;
}

void AnimationNode::Reset()
{
    mTime = 0.0f;
}

bool AnimationNode::Prepare(const GfxFactory& loader)
{
    //            == About resource loading ==
    // User defined resources have a combination of type and name
    // where type is the underlying class type and name identifies
    // the set of resources that the user edits that instances of that
    // said type then use.
    // Primitive / (non user defined resources) don't need a name
    // since the name is irrelevant since the objects are stateless
    // in this sense and don't have properties that would change between instances.
    // For example with drawable rectangles (gfx::Rectangle) all rectangles
    // that we might want to draw are basically the same. We don't need to
    // configure each rectangle object with properties that would distinguish
    // it from other rectangles.
    // In fact there's basically even no need to create more than 1 instance
    // of such resource and then share it between all users.
    //
    // User defined resources on the other hand *can be* unique.
    // For example particle engines, the underlying object type is same for
    // particle engine A and B, but their defining properties are completely
    // different. To distinguish the set of properties the user gives each
    // particle engine a "name". Then when loading such objects we must
    // load them by name. Additionally the resources may or may not be
    // shared. For example when a fleet of alien spaceships are rendered
    // each spaceship might have their own particle engine (own simulation state)
    // thus producing a unique rendering for each spaceship. However the problem
    // is that this might be computationally heavy. For N ships we'd need to do
    // N particle engine simulations.
    // However it's also possible that the particle engines are shared and each
    // ship (of the same type) just refers to the same particle engine. Then
    // each ship will render the same particle stream.
    mDrawable = loader.MakeDrawable(mDrawableName);
    mMaterial = loader.MakeMaterial(mMaterialName);
    return mDrawable && mMaterial;
}

nlohmann::json AnimationNode::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "name", mName);
    base::JsonWrite(json, "material", mMaterialName);
    base::JsonWrite(json, "drawable", mDrawableName);
    base::JsonWrite(json, "position", mPosition);
    base::JsonWrite(json, "size", mSize);
    base::JsonWrite(json, "scale", mScale);
    base::JsonWrite(json, "rotation", mRotation);
    base::JsonWrite(json, "layer", mLayer);
    base::JsonWrite(json, "render_pass", mRenderPass);
    return json;
}

// static
std::optional<AnimationNode> AnimationNode::FromJson(const nlohmann::json& object)
{
    AnimationNode ret;
    if (!base::JsonReadSafe(object, "name", &ret.mName) ||
        !base::JsonReadSafe(object, "material", &ret.mMaterialName) ||
        !base::JsonReadSafe(object, "drawable", &ret.mDrawableName) ||
        !base::JsonReadSafe(object, "position", &ret.mPosition) ||
        !base::JsonReadSafe(object, "size", &ret.mSize) ||
        !base::JsonReadSafe(object, "scale", &ret.mScale) ||
        !base::JsonReadSafe(object, "rotation", &ret.mRotation) ||
        !base::JsonReadSafe(object, "layer", &ret.mLayer) ||
        !base::JsonReadSafe(object, "render_pass", &ret.mRenderPass))
        return std::nullopt;
    return ret;
}



void Animation::Draw(gfx::Painter& painter, gfx::Transform& transform) const
{
    // here we could apply operations that would apply to the whole
    // animation but currently we don't need such things.
    // if we did we could begin new transformation scope for this
    // by pushing a new scope in the transformation stack.
    // transfrom.Push();

    // implement "layers" by drawing in a sorted order as determined
    // by the layer value.
    std::multimap<int, const AnimationNode*> layer_map;

    // Ask each component to draw.
    for (const auto& component : mNodes)
    {
        layer_map.insert(std::make_pair(component.GetLayer(), &component));
    }

    for (auto pair : layer_map)
    {
        const auto* component = pair.second;
        component->Draw(painter, transform);
    }

    // if we used a new trańsformation scope pop it here.
    //transform.Pop();
}

bool Animation::Update(float dt)
{
    bool alive = false;

    for (auto& component : mNodes)
    {
        if (component.Update(dt))
            alive = true;
    }
    return alive;
}

bool Animation::IsExpired() const
{
    return false;
}

void Animation::Reset()
{
    for (auto& component : mNodes)
    {
        component.Reset();
    }
}

void Animation::Prepare(const GfxFactory& loader)
{
    for (auto& component : mNodes)
    {
        if (!component.Prepare(loader))
        {
            WARN("Component '%1' failed to prepare.", component.GetName());
        }
    }
}


} // namespace
