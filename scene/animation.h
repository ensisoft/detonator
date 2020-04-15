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
#  include <glm/vec2.hpp>
#  include <glm/glm.hpp>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include "base/assert.h"
#include "base/utility.h"
#include "graphics/material.h"
#include "graphics/drawable.h"

namespace gfx {
    class Drawable;
    class Material;
    class Painter;
    class Transform;
} // namespace

namespace scene
{
    class GfxFactory;

    class AnimationNode
    {
    public:
        enum class RenderPass {
            Draw,
            Mask
        };
        AnimationNode() = default;
        AnimationNode(const std::string& name,
                    const std::string& material_name,
                    const std::string& drawable_name,
                    std::shared_ptr<gfx::Drawable> drawable,
                    std::shared_ptr<gfx::Material> material)
            : mName(name)
            , mMaterialName(material_name)
            , mDrawableName(drawable_name)
            , mMaterial(material)
            , mDrawable(drawable)
        {}

        // Draw this component relative to the given parent transformation.
        void Draw(gfx::Painter& painter, gfx::Transform& transform) const;

        // Set the drawable object (shape) for this component.
        // The name identifies the resource in the gfx resource loader.
        void SetDrawable(const std::string& name, std::shared_ptr<gfx::Drawable> drawable)
        {
            mDrawable = std::move(drawable);
            mDrawableName = name;
        }
        // Set the material object for this component.
        // The name identifies the runtime material resource in the gfx resource loader.
        void SetMaterial(const std::string& name, std::shared_ptr<gfx::Material> material)
        {
            mMaterial = std::move(material);
            mMaterialName = name;
        }
        void SetTranslation(const glm::vec2& pos)
        { mPosition = pos; }
        void SetName(const std::string& name)
        { mName = name;}
        void SetScale(const glm::vec2& scale)
        { mScale = scale; }
        void SetSize(const glm::vec2& size)
        { mSize = size; }
        void SetLayer(int layer)
        { mLayer = layer; }
        void SetRenderPass(RenderPass pass)
        { mRenderPass = pass; }
        void SetRotation(float value)
        { mRotation = value; }

        RenderPass GetRenderPass() const
        { return mRenderPass; }
        int GetLayer() const
        { return mLayer; }
        std::string GetName() const
        { return mName; }
        std::string GetMaterialName() const
        { return mMaterialName; }
        std::string GetDrawableName() const
        { return mDrawableName; }
        glm::vec2 GetTranslation() const
        { return mPosition; }
        glm::vec2 GetSize() const
        { return mSize; }
        float GetRotation() const
        { return mRotation; }

        bool Update(float dt);

        void Reset();

        // Prepare this animation component for rendering
        // by loading all the needed runtime resources.
        // Returns true if succesful false if some resource
        // was not available.
        bool Prepare(const GfxFactory& loader);

        // serialize the component properties into JSON.
        nlohmann::json ToJson() const;

        // Load a component and it's properties from a JSON object.
        // Note that this does not yet create/load any runtime objects
        // such as materials or such but they are loaded later when the
        // component is prepared.
        static std::optional<AnimationNode> FromJson(const nlohmann::json& object);
    private:
        // generic properties.
        std::string mName;
        // visual properties. we keep the material/drawable names
        // around so that we we know which resources to load at runtime.
        std::string mMaterialName;
        std::string mDrawableName;
        std::shared_ptr<gfx::Material> mMaterial;
        std::shared_ptr<gfx::Drawable> mDrawable;
        // timewise properties.
        float mLifetime  = 0.0f;
        float mStartTime = 0.0f;
        float mTime      = 0.0f;
        // transformation properties.
        // translation offset relative to the animation.
        glm::vec2 mPosition;
        // size is the size of this object in some units
        // (for example pixels)
        glm::vec2 mSize = {1.0f, 1.0f};
        // scale applies an additional scale to this hiearchy.
        glm::vec2 mScale = {1.0f, 1.0f};
        // rotation around z axis positive rotation is CW
        float mRotation = 0.0f;
        // rendering properties. which layer and wich pass.
        int mLayer = 0;
        RenderPass mRenderPass = RenderPass::Draw;
    };


    class Animation
    {
    public:
        // Draw the animation and its components.
        // Each component is transformed relative to the parent transformation "trans".
        void Draw(gfx::Painter& painter, gfx::Transform& trans) const;

        bool Update(float dt);

        bool IsExpired() const;

        // Add a new component
        void AddNode(AnimationNode&& component)
        { mNodes.push_back(std::move(component)); }

        // Delete a component by the given index.
        void DelNode(size_t i)
        {
            ASSERT(i < mNodes.size());
            auto it = std::begin(mNodes);
            std::advance(it, i);
            mNodes.erase(it);
        }

        AnimationNode& GetNode(size_t i)
        {
            ASSERT(i < mNodes.size());
            return mNodes[i];
        }
        const AnimationNode& GetNode(size_t i) const
        {
            ASSERT(i < mNodes.size());
            return mNodes[i];
        }

        std::size_t GetNumNodes() const
        { return mNodes.size(); }

        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            for (const auto& component : mNodes)
            {
                json["nodes"].push_back(component.ToJson());
            }
            return json;
        }

        static std::optional<Animation> FromJson(const nlohmann::json& object)
        {
            Animation ret;

            for (const auto& json : object["nodes"].items())
            {
                std::optional<AnimationNode> comp = AnimationNode::FromJson(json.value());
                if (!comp.has_value())
                    return std::nullopt;
                ret.mNodes.push_back(std::move(comp.value()));
            }
            return ret;
        }

        void Reset();

        // Prepare and load the runtime resources if not yet loaded.
        void Prepare(const GfxFactory& loader);

    private:
        // The list of components that are to be drawn as part
        // of the animation. Each component has a unique transform
        // relative to the animation.
        std::vector<AnimationNode> mNodes;

    };
} // namespace
