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

    class Animation
    {
    public:
        enum class RenderPass {
            Draw,
            Mask
        };

        class Component
        {
        public:
            Component() = default;
            Component(const std::string& name,
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
            void SetLayer(int layer)
            { mLayer = layer; }
            void SetRenderPass(RenderPass pass)
            { mRenderPass = pass; }
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

            bool Update(float dt);

            void Reset();

            nlohmann::json ToJson() const
            {
                nlohmann::json json;
                base::JsonWrite(json, "name", mName);
                base::JsonWrite(json, "lifetime", mLifetime);
                base::JsonWrite(json, "starttime", mStartTime);
                base::JsonWrite(json, "material", mMaterialName);
                base::JsonWrite(json, "drawable", mDrawableName);
                return json;
            }

        private:
            std::string mName;
            std::string mMaterialName;
            std::string mDrawableName;
            std::shared_ptr<gfx::Material> mMaterial;
            std::shared_ptr<gfx::Drawable> mDrawable;
            float mLifetime  = 0.0f;
            float mStartTime = 0.0f;
            float mTime      = 0.0f;
            // translation offset relative to the animation.
            glm::vec2 mPosition;
            glm::vec2 mScale = {1.0f, 1.0f};
            int mLayer = 0;
            RenderPass mRenderPass = RenderPass::Draw;
        };

        // Draw the animation and its components.
        // Each component is transformed relative to the parent transformation "trans".
        void Draw(gfx::Painter& painter, gfx::Transform& trans) const;

        bool Update(float dt);

        bool IsExpired() const;

        // Add a new component
        void AddComponent(Component&& component)
        { mComponents.push_back(std::move(component)); }

        // Delete a component by the given index.
        void DelComponent(size_t i)
        {
            ASSERT(i < mComponents.size());
            auto it = std::begin(mComponents);
            std::advance(it, i);
            mComponents.erase(it);
        }

        Component& GetComponent(size_t i)
        {
            ASSERT(i < mComponents.size());
            return mComponents[i];
        }
        const Component& GetComponent(size_t i) const
        {
            ASSERT(i < mComponents.size());
            return mComponents[i];
        }

        std::size_t GetNumComponents() const
        { return mComponents.size(); }

        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            for (const auto& component : mComponents)
            {
                json["components"].push_back(component.ToJson());
            }
            return json;
        }

        void Reset();

    private:
        // The list of components that are to be drawn as part
        // of the animation. Each component has a unique transform
        // relative to the animation.
        std::vector<Component> mComponents;
        // Current animation runtime (seconds).
        float mTime = 0.0f;
        // Expected animation duration (seconds).
        float mDuration = 0.0f;
    };
} // namespace
