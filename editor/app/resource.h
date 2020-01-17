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
#  include <QString>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include "graphics/drawable.h"
#include "graphics/material.h"

namespace app
{
    // Editor app resource object.
    class Resource
    {
    public:
        // Type of the resource.
        enum class Type {
            // It's a material
            Material,
            // It's a particle system
            ParticleSystem,
            // It's an animation
            Animation,
            // It's a scene description
            Scene,
            // It's an audio track
            AudioTrack
        };
        virtual ~Resource() = default;
        // Get the human readable name of the resource.
        virtual QString GetName() const = 0;
        // Get the type of the resource.
        virtual Type GetType() const = 0;
        // Serialize into JSON
        virtual void Serialize(nlohmann::json& json) const =0;
        // helpers
        inline bool IsMaterial() const
        { return GetType() == Type::Material; }

        template<typename Content>
        bool GetContent(Content** content)
        {
            *content = static_cast<Content*>(GetIf(typeid(Content)));
            return *content != nullptr;
        }
        template<typename Content>
        bool GetContent(const Content** content) const
        {
            *content = static_cast<const Content*>(GetIf(typeid(Content)));
            return *content != nullptr;
        }
    protected:
        virtual void* GetIf(const std::type_info& expected_type) = 0;
        virtual const void* GetIf(const std::type_info& expected_type) const = 0;
    private:
    };

    template<typename Content, Resource::Type type>
    class GraphicsResource : public Resource
    {
    public:
        GraphicsResource(const Content& content)
            : mContent(content)
        {}
        GraphicsResource(Content&& content)
            : mContent(std::move(content))
        {}
        virtual QString GetName() const override
        {
            return app::fromUtf8(mContent.GetName());
        }
        virtual Type GetType() const override
        {
            return type;
        }
        virtual void Serialize(nlohmann::json& json) const override
        {
            if constexpr (type == Resource::Type::Material)
                json["materials"].push_back(mContent.ToJson());
            else if constexpr (type == Resource::Type::ParticleSystem)
                json["particles"].push_back(mContent.ToJson());
            else if constexpr (type == Resource::Type::Animation)
                json["animations"].push_back(mContent.ToJson());
        }
    protected:
        virtual void* GetIf(const std::type_info& expected_type) override
        {
            if (typeid(Content) == expected_type)
                return &mContent;
            return nullptr;
        }
        virtual const void* GetIf(const std::type_info& expected_type) const override
        {
            if (typeid(Content) == expected_type)
                return &mContent;
            return nullptr;
        }
    private:
        Content mContent;
    };

    using MaterialResource = GraphicsResource<gfx::Material, Resource::Type::Material>;

} // namespace
