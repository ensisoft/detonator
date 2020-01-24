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
        class Component
        {
        public:
            Component() = default;
            Component(const std::string& name,
                      const std::string& drawable_name,
                      const std::string& material_name)
              : mName(name)
              , mDrawableName(drawable_name)
              , mMaterialName(material_name)
            {}

            void Draw(const gfx::Transform& parent, gfx::Painter& painter) const;

            void SetDrawable(std::shared_ptr<gfx::Drawable> drawable)
            { mDrawable = std::move(drawable); }
            void SetMaterial(std::shared_ptr<gfx::Material> material)
            { mMaterial = std::move(material); }
            void SetDrawable(const std::string& drawable_name)
            { mDrawableName = drawable_name; }
            void SetMaterial(const std::string& material_name)
            { mMaterialName = material_name; }
            void SetTranslation(const glm::vec2& pos)
            { mPosition = pos; }
            void SetName(const std::string& name)
            { mName = name;}

            std::string GetName() const
            { return mName; }

            bool Update(float dt);

            nlohmann::json ToJson() const
            {
                nlohmann::json json;
                base::JsonWrite(json, "name", mName);
                base::JsonWrite(json, "drawable", mDrawableName);
                base::JsonWrite(json, "material", mMaterialName);
                base::JsonWrite(json, "lifetime", mLifetime);
                base::JsonWrite(json, "starttime", mStartTime);
                return json;
            }
            bool Prepare(const GfxFactory& factory);

        private:
            std::string mName;
            std::string mDrawableName;
            std::string mMaterialName;
            std::shared_ptr<gfx::Material> mMaterial;
            std::shared_ptr<gfx::Drawable> mDrawable;
            float mLifetime  = 0.0f;
            float mStartTime = 0.0f;
            float mTime      = 0.0f;
            // translation offset relative to the animation.
            glm::vec2 mPosition;
            glm::vec2 mScale;
        };

        void Draw(gfx::Painter& painter) const;

        bool Update(float dt);

        bool IsExpired() const;

        bool Prepare(const GfxFactory& factory);

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
