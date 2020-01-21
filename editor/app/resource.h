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
#  include <QVariant>
#  include <QJsonObject>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include "base/assert.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "utility.h"

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
        // Serialize the content into JSON
        virtual void Serialize(nlohmann::json& json) const =0;
        // Serialize additional non-content properties into JSON
        virtual void Serialize(QJsonObject& json) const = 0;
        // Returns true if resource has a property by the given name.
        virtual bool HasProperty(const QString& name) const = 0;
        // Set a property value. If the property exists already the previous
        // value is overwritten. Otherwise it's added.
        virtual void SetProperty(const QString& name, const QVariant& value) = 0;
        // Return the value of the property identied by name.
        // If the property doesn't exist returns default value.
        virtual QVariant GetProperty(const QString& name, const QVariant& def) const = 0;
        // Return the value of the property identified by name.
        // If the property doesn't exist returns a null variant.
        virtual QVariant GetProperty(const QString& name) const = 0;
        // Load the additional properies from the json object.
        virtual void LoadProperties(const QJsonObject& json) = 0;

        // helpers
        inline bool IsMaterial() const
        { return GetType() == Type::Material; }

        template<typename T>
        T GetProperty(const QString& name, const T& def) const
        {
            if (!HasProperty(name))
                return def;
            const auto& ret = GetProperty(name);
            return qvariant_cast<T>(ret);
        }

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
        GraphicsResource(const Content& content, const QString& name)
            : mContent(content)
            , mName(name)
        {}
        GraphicsResource(Content&& content, const QString& name)
            : mContent(std::move(content))
            , mName(name)
        {}
        virtual QString GetName() const override
        {
            return mName;
        }
        virtual Type GetType() const override
        {
            return type;
        }
        virtual void Serialize(nlohmann::json& json) const override
        {
            nlohmann::json content_json = mContent.ToJson();
            // tag some additional data with the content's JSON
            // in this case just the name so that we can map the object
            // to its additional properties.
            // note that we could basically put all the properties here as well
            // but then we'd have to serialize everything in QVariant manually.
            // and there could be possibilities for name conflicts
            ASSERT(content_json.contains("resource_name") == false);
            content_json["resource_name"] = app::ToUtf8(mName);

            if constexpr (type == Resource::Type::Material)
                json["materials"].push_back(content_json);
            else if (type == Resource::Type::ParticleSystem)
                json["particles"].push_back(content_json);
            else if (type == Resource::Type::Animation)
                json["animations"].push_back(content_json);
        }
        virtual void Serialize(QJsonObject& json) const override
        {
            if constexpr (type == Resource::Type::Material)
                json["material_" + mName]  = QJsonObject::fromVariantMap(mProps);
            else if (type == Resource::Type::ParticleSystem)
                json["particle_" + mName] = QJsonObject::fromVariantMap(mProps);
            else if (type == Resource::Type::Animation)
                json["animation_" + mName] = QJsonObject::fromVariantMap(mProps);
        }
        virtual bool HasProperty(const QString& name) const override
        { return mProps.contains(name); }
        virtual void SetProperty(const QString& name, const QVariant& value) override
        { mProps[name] = value; }
        virtual QVariant GetProperty(const QString& name, const QVariant& def) const override
        {
            QVariant ret = mProps[name];
            if (ret.isNull())
                return def;
            return ret;
        }
        virtual QVariant GetProperty(const QString& name) const override
        { return mProps[name]; }
        virtual void LoadProperties(const QJsonObject& object) override
        {
            if constexpr (type == Resource::Type::Material)
                mProps = object["material_" + mName].toObject().toVariantMap();
            else if (type == Resource::Type::ParticleSystem)
                mProps = object["material_" + mName].toObject().toVariantMap();
            else if (type == Resource::Type::Animation)
                mProps = object["material_" + mName].toObject().toVariantMap();
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
        QString mName;
        QVariantMap mProps;
    };

    using MaterialResource = GraphicsResource<gfx::Material, Resource::Type::Material>;

} // namespace
