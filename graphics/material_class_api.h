// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#include <memory>

#include "graphics/material_uniform.h"
#include "graphics/material_class.h"

namespace gfx
{
    namespace detail {
        class MaterialClassBase
        {
        public:
            MaterialClass& GetClass() noexcept
            {
                return *mClass;
            }
            const MaterialClass& GetClass() const noexcept
            {
                return *mClass;
            }
            MaterialClass* operator->() noexcept
            {
                return mClass;
            }
            const MaterialClass* operator->() const noexcept
            {
                return mClass;
            }
            operator MaterialClass& () noexcept
            {
                return *mClass;
            }
            operator const MaterialClass () const noexcept
            {
                return *mClass;
            }
            void SetSurfaceType(MaterialClass::SurfaceType surface) const
            { mClass->SetSurfaceType(surface); }

            auto GetSurfaceType() const noexcept
            { return mClass->GetSurfaceType(); }

            void SetStatic(bool value) noexcept
            { mClass->SetStatic(value); }

            void Imitate(const MaterialClass& other)
            {
                *mClass = other;
            }

        protected:
            MaterialClassBase(MaterialClass& klass) noexcept
              : mClass(&klass)
            {}
            MaterialClassBase(MaterialClass::Type type)
            {
                mPClass = std::make_unique<MaterialClass>(type);
                mClass = mPClass.get();
            }

        protected:
            MaterialClass* mClass = nullptr;
        private:
            std::unique_ptr<MaterialClass> mPClass;
        };

        class MaterialClassBaseTexture : public MaterialClassBase
        {
        protected:
            MaterialClassBaseTexture(MaterialClass& klass) noexcept
              : MaterialClassBase(klass)
            {}
            MaterialClassBaseTexture(MaterialClass::Type type)
            : MaterialClassBase(type)
            {}
        public:
            using TextureWrapping = MaterialClass::TextureWrapping;
            using MinFilter = MaterialClass::MinTextureFilter;
            using MagFilter = MaterialClass::MagTextureFilter;

            void SetTextureWrapX(TextureWrapping wrapping) const
            { mClass->SetTextureWrapX(wrapping); }
            void SetTextureWrapY(TextureWrapping wrapping) const
            { mClass->SetTextureWrapY(wrapping); }

            void SetTextureMinFilter(MinFilter filter) const
            { mClass->SetTextureMinFilter(filter); }
            void SetTextureMagFilter(MagFilter filter) const
            { mClass->SetTextureMagFilter(filter); }

            void SetParticleEffect(ParticleEffect effect) const
            {mClass->SetUniform<kParticleEffect>(effect); }
            void SetBaseColor(const Color4f& color) const
            { mClass->SetUniform<kBaseColor>(color); }
            void SetAlphaCutoff(float cutoff) const
            { mClass->SetUniform<kAlphaCutoff>(cutoff); }
            void SetTextureScale(glm::vec2 scale) const
            { mClass->SetUniform<kTextureScale>(scale); }
            void SetTextureVelocity(glm::vec3 velocity) const
            { mClass->SetUniform<kTextureVelocity>(velocity); }
            void SetTextureRotation(float radians) const
            { mClass->SetUniform<kTextureRotation>(radians); }

            auto GetParticleEffect() const noexcept
            { return mClass->GetUniformValue<kParticleEffect>(); }
            auto GetBaseColor() const noexcept
            { return mClass->GetUniformValue<kBaseColor>(); }
            auto GetAlphaCutoff() const noexcept
            { return mClass->GetUniformValue<kAlphaCutoff>(); }
            auto GetTextureScale() const noexcept
            { return mClass->GetUniformValue<kTextureScale>(); }
            auto GetTextureVelocity() const noexcept
            { return mClass->GetUniformValue<kTextureVelocity>(); }
            auto GetTextureRotation() const noexcept
            { return mClass->GetUniformValue<kTextureRotation>(); }

            float GetTextureVelocityX() const noexcept
            { return GetTextureVelocity().x; }
            float GetTextureVelocityY() const noexcept
            { return GetTextureVelocity().y; }
            float GetTextureVelocityZ() const noexcept
            { return GetTextureVelocity().z; }

            void SetTextureScaleX(float x) const noexcept
            {
                auto scale = GetTextureScale();
                scale.x = x;
                SetTextureScale(scale);
            }
            void SetTextureScaleY(float y) const noexcept
            {
                auto scale = GetTextureScale();
                scale.y = y;
                SetTextureScale(scale);
            }

            void SetTextureVelocityX(float x) const noexcept
            {
                auto velocity = GetTextureVelocity();
                velocity.x = x;
                SetTextureVelocity(velocity);
            }
            void SetTextureVelocityY(float y) const noexcept
            {
                auto velocity = GetTextureVelocity();
                velocity.y = y;
                SetTextureVelocity(velocity);
            }
            void SetTextureVelocityZ(float angle_radians) const noexcept
            {
                auto velocity = GetTextureVelocity();
                velocity.z = angle_radians;
                SetTextureVelocity(velocity);
            }
        private:
        };
    }

    class ColorMaterialClass : public detail::MaterialClassBase
    {
    public:
        explicit ColorMaterialClass(MaterialClass& klass) noexcept
          : MaterialClassBase(klass)
        {}
        ColorMaterialClass()
            : MaterialClassBase(MaterialClass::Type::Color)
        {}
        void SetBaseColor(Color4f color) const
        { mClass->SetUniform<kBaseColor>(color); }

        auto GetBaseColor() const noexcept
        { return mClass->GetUniformValue<kBaseColor>(); }
    private:
    };

    class GradientMaterialClass : public detail::MaterialClassBase
    {
    public:
        explicit GradientMaterialClass(MaterialClass& klass) noexcept
         : MaterialClassBase(klass)
        {}
        GradientMaterialClass()
            : MaterialClassBase(MaterialClass::Type::Gradient)
        {}
        void SetGradientGamma(float gamma) const
        { mClass->SetUniform<kGradientGamma>(gamma); }
        void SetGradientColor0(Color4f color) const
        { mClass->SetUniform<kGradientColor0>(color); }
        void SetGradientColor1(Color4f color) const
        { mClass->SetUniform<kGradientColor1>(color); }
        void SetGradientColor2(Color4f color) const
        { mClass->SetUniform<kGradientColor2>(color); }
        void SetGradientColor3(Color4f color) const
        { mClass->SetUniform<kGradientColor3>(color); }
        void SetGradientWeight(glm::vec2 weight) const
        { mClass->SetUniform<kGradientWeight>(weight); }
        void SetGradientType(GradientType type) const
        { mClass->SetUniform<kGradientType>(type); }


        auto GetGradientColor0() const noexcept
        { return mClass->GetUniformValue<kGradientColor0>(); }
        auto GetGradientColor1() const noexcept
        { return mClass->GetUniformValue<kGradientColor1>(); }
        auto GetGradientColor2() const noexcept
        { return mClass->GetUniformValue<kGradientColor2>(); }
        auto GetGradientColor3() const noexcept
        { return mClass->GetUniformValue<kGradientColor3>(); }
        auto GetGradientGamma() const noexcept
        { return mClass->GetUniformValue<kGradientGamma>(); }
        auto GetGradientWeight() const noexcept
        { return mClass->GetUniformValue<kGradientWeight>(); }
        auto GetGradientType() const noexcept
        { return mClass->GetUniformValue<kGradientType>(); }
    private:

    };

    class TextureMaterialClass : public detail::MaterialClassBaseTexture
    {
    public:
        explicit TextureMaterialClass(MaterialClass& klass) noexcept
          : MaterialClassBaseTexture(klass)
        {}
        TextureMaterialClass()
            : MaterialClassBaseTexture(MaterialClass::Type::Texture)
        {}
        void SetTexture(std::unique_ptr<TextureSource> texture)
        { mClass->SetTexture(std::move(texture)); }
        void SetTextureRect(const FRect& rect)
        { mClass->SetTextureRect(rect); }
    private:
    };

    class SpriteMaterialClass : public detail::MaterialClassBaseTexture
    {
    public:
        explicit SpriteMaterialClass(MaterialClass& klass) noexcept
          : MaterialClassBaseTexture(klass)
        {}
        SpriteMaterialClass()
            : MaterialClassBaseTexture(MaterialClass::Type::Sprite)
        {}
        void AddTexture(std::unique_ptr<TextureSource> texture)
        { mClass->AddTexture(std::move(texture)); }
    private:

    };

    class TilemapMaterialClass : public detail::MaterialClassBase
    {
    public:
        explicit TilemapMaterialClass(MaterialClass& klass) noexcept
          : MaterialClassBase(klass)
        {}
        TilemapMaterialClass()
            : MaterialClassBase(MaterialClass::Type::Tilemap)
        {}
        void SetBaseColor(const Color4f& color) const
        { mClass->SetUniform<kBaseColor>(color); }
        void SetAlphaCutoff(float cutoff) const
        { mClass->SetUniform<kAlphaCutoff>(cutoff); }

        void SetTileSize(const glm::vec2& size) const
        { mClass->SetUniform<kTileSize>(size); }
        void SetTileOffset(const glm::vec2& offset) const
        { mClass->SetUniform<kTileOffset>(offset); }
        void SetTilePadding(const glm::vec2& padding) const
        { mClass->SetUniform<kTilePadding>(padding); }

        void SetActiveTextureMap(std::string id) const
        { mClass->SetActiveTextureMap(std::move(id)); }
        void SetNumTextureMaps(unsigned count) const
        { mClass->SetNumTextureMaps(count); }
        void SetTextureMap(unsigned index, TextureMap2D map) const
        { mClass->SetTextureMap(index, std::move(map)); }
        void SetTextureMinFilter(MaterialClass::MinTextureFilter filter) const
        { mClass->SetTextureMinFilter(filter); }
        void SetTextureMagFilter(MaterialClass::MagTextureFilter filter) const
        { mClass->SetTextureMagFilter(filter); }

        auto GetSurfaceType() const noexcept
        { return mClass->GetSurfaceType(); }
        auto GetBaseColor() const noexcept
        { return mClass->GetUniformValue<kBaseColor>(); }
        auto GetAlphaCutoff() const noexcept
        { return mClass->GetUniformValue<kAlphaCutoff>(); }

        auto GetTileSize() const noexcept
        { return mClass->GetUniformValue<kTileSize>();; }
        auto GetTileOffset() const noexcept
        { return mClass->GetUniformValue<kTileOffset>(); }
        auto GetTilePadding() const noexcept
        { return mClass->GetUniformValue<kTilePadding>(); }
    private:
    };

    class Particle2DMaterialClass : public detail::MaterialClassBase
    {
    public:
        using ParticleEffect   = MaterialClass::ParticleEffect;
        using ParticleRotation = MaterialClass::ParticleRotation;

        explicit Particle2DMaterialClass(MaterialClass& klass) noexcept
          : MaterialClassBase(klass)
        {}
        Particle2DMaterialClass()
            : MaterialClassBase(MaterialClass::Type::Particle2D)
        {}
        void SetAlphaCutoff(float cutoff) const
        { mClass->SetUniform<kAlphaCutoff>(cutoff); }

        void SetParticleBaseRotation(kParticleBaseRotation::T value) const
        { mClass->SetUniform<kParticleBaseRotation>(value); }
        void SetParticleStartColor(Color4f color) const
        { mClass->SetUniform<kParticleStartColor>(color); }
        void SetParticleEndColor(Color4f color) const
        { mClass->SetUniform<kParticleEndColor>(color); }
        void SetParticleMidColor(Color4f color) const
        { mClass->SetUniform<kParticleMidColor>(color); }
        void SetParticleRotation(ParticleRotation rotation) const
        { mClass->SetUniform<kParticleRotation>(rotation); }

        auto GetAlphaCutoff() const noexcept
        { return mClass->GetUniformValue<kAlphaCutoff>(); }

        auto GetParticleStartColor() const noexcept
        { return mClass->GetUniformValue<kParticleStartColor>(); }
        auto GetParticleEndColor() const noexcept
        { return mClass->GetUniformValue<kParticleEndColor>(); }
        auto GetParticleMidColor() const noexcept
        { return mClass->GetUniformValue<kParticleMidColor>(); }
        auto GetParticleRotation() const noexcept
        { return mClass->GetUniformValue<kParticleRotation>(); }
        auto GetParticleBaseRotation() const noexcept
        { return mClass->GetUniformValue<kParticleBaseRotation>(); }
    private:
    };

    class BasicLightMaterialClass : public detail::MaterialClassBase
    {
    public:
        explicit BasicLightMaterialClass(MaterialClass& klass) noexcept
          : MaterialClassBase(klass)
        {}
        BasicLightMaterialClass()
            : MaterialClassBase(MaterialClass::Type::BasicLight)
        {}
        void SetAlphaCutoff(float cutoff) const
        { mClass->SetUniform<kAlphaCutoff>(cutoff); }

        void SetAmbientColor(Color4f color) const
        { mClass->SetUniform<kAmbientColor>(color); }
        void SetDiffuseColor(Color4f color) const
        { mClass->SetUniform<kDiffuseColor>(color); }
        void SetSpecularColor(Color4f color) const
        { mClass->SetUniform<kSpecularColor>(color); }
        void SetSpecularExponent(float exponent) const
        { mClass->SetUniform<kSpecularExponent>(exponent); }

        auto GetAlphaCutoff() const noexcept
        { return mClass->GetUniformValue<kAlphaCutoff>(); }

        auto GetAmbientColor() const noexcept
        { return mClass->GetUniformValue<kAmbientColor>(); }
        auto GetDiffuseColor() const noexcept
        { return mClass->GetUniformValue<kDiffuseColor>(); }
        auto GetSpecularColor() const noexcept
        { return mClass->GetUniformValue<kSpecularColor>(); }
        auto GetSpecularExponent() const noexcept
        { return mClass->GetUniformValue<kSpecularExponent>(); }
    private:

    };

} // namespace