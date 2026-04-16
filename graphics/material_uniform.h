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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#include "warnpop.h"

#include "graphics/enum.h"
#include "graphics/types.h"

namespace gfx {
    struct kTileSize {
        using T = glm::vec2;
        static constexpr const char* const uniform_name = "kTileSize";
        static constexpr glm::vec2 default_value = {0.0f, 0.0f};
    };
    struct kTileOffset {
        using T = glm::vec2;
        static constexpr const char* const uniform_name = "kTileOffset";
        static constexpr glm::vec2 default_value = {0.0f, 0.0f};
    };
    struct kTilePadding {
        using T = glm::vec2;
        static constexpr const char* const uniform_name = "kTilePadding";
        static constexpr glm::vec2 default_value = {0.0f, 0.0f};
    };
    struct kAlphaCutoff {
        using T = float;
        static constexpr const char* const uniform_name = "kAlphaCutoff";
        static constexpr float default_value = -1.0f;
    };
    struct kParticleStartColor {
        using T = Color4f;
        static constexpr const char* const uniform_name = "kParticleStartColor";
        static constexpr Color4f default_value = Color::White;
    };
    struct kParticleEndColor {
        using T = Color4f;
        static constexpr const char* const uniform_name = "kParticleEndColor";
        static constexpr Color4f default_value = Color::White;
    };
    struct kParticleMidColor {
        using T = Color4f;
        static constexpr const char* const uniform_name = "kParticleMidColor";
        static constexpr Color4f default_value = Color::White;
    };
    struct kParticleRotation {
        using T = ParticleRotation;
        static constexpr const char* const uniform_name = "kParticleRotation";
        static constexpr auto default_value = ParticleRotation::None;
    };
    struct kParticleBaseRotation {
        using T = float;
        static constexpr const char* const uniform_name = "kParticleBaseRotation";
        static constexpr float default_value = 0.0f;
    };

    struct kAmbientColor {
        using T = Color4f;
        static constexpr const char* const uniform_name = "kAmbientColor";
        static constexpr Color4f default_value = Color::Gray;
    };
    struct kDiffuseColor {
        using T = Color4f;
        static constexpr const char* const uniform_name = "kDiffuseColor";
        static constexpr Color4f default_value = Color::Gray;
    };
    struct kSpecularColor {
        using T = Color4f;
        static constexpr const char* const uniform_name = "kSpecularColor";
        static constexpr Color4f default_value = Color::White;
    };
    struct kSpecularExponent {
        using T = float;
        static constexpr const char* const uniform_name = "kSpecularExponent";
        static constexpr float default_value = 4.0f;
    };

    struct kBaseColor {
        using T = Color4f;
        static constexpr const char* const uniform_name = "kBaseColor";
        static constexpr Color4f default_value = Color::White;
    };

    struct kGradientColor0 {
        using T = Color4f;
        static constexpr const char* const uniform_name = "kGradientColor0";
        static constexpr Color4f default_value = Color::White;
    };
    struct kGradientColor1 {
        using T = Color4f;
        static constexpr const char* const uniform_name = "kGradientColor1";
        static constexpr Color4f default_value = Color::White;
    };
    struct kGradientColor2 {
        using T = Color4f;
        static constexpr const char* const uniform_name = "kGradientColor2";
        static constexpr Color4f default_value = Color::White;
    };
    struct kGradientColor3 {
        using T = Color4f;
        static constexpr const char* const uniform_name = "kGradientColor3";
        static constexpr Color4f default_value = Color::White;
    };
    struct kGradientWeight {
        using T = glm::vec2;
        static constexpr const char* const uniform_name = "kGradientWeight";
        static constexpr glm::vec2 default_value = {0.5f, 0.5f};
    };
    struct kGradientType {
        using T = GradientType;
        static constexpr const char* const uniform_name = "kGradientType";
        static constexpr auto default_value = GradientType::Bilinear;
    };
    struct kGradientGamma {
        using T = float;
        static constexpr const char* const uniform_name = "kGradientGamma";
        static constexpr auto default_value = 1.0f;
    };

    struct kTextureScale {
        using T = glm::vec2;
        static constexpr const char* const uniform_name = "kTextureScale";
        static constexpr glm::vec2 default_value = {1.0f, 1.0f};
    };
    struct kTextureVelocity {
        using T = glm::vec3;
        static constexpr const char* const uniform_name = "kTextureVelocity";
        static constexpr glm::vec3 default_value = {0.0f, 0.0f, 0.0f};
    };
    struct kTextureRotation {
        using T = float;
        static constexpr const char* const uniform_name = "kTextureRotation";
        static constexpr auto default_value = 0.0f;
    };

    // used with texture and sprite materials when they're applied
    // on particles.
    struct kParticleEffect {
        using T = ParticleEffect;
        static constexpr const char* const uniform_name = "kParticleEffect";
        static constexpr auto default_value = ParticleEffect::None;
    };

    template<typename T>
    bool UniformValueEquals(const T& default_value, const T& new_value)
    {
        return default_value == new_value;
    }
    template<> inline
    bool UniformValueEquals<Color4f>(const Color4f& default_value, const Color4f& new_value)
    {
        return base::Equals(default_value, new_value);
    }

    template<typename kUniform>
    bool UniformEquals(const typename kUniform::T& value) noexcept
    {
        return UniformValueEquals(kUniform::default_value, value);
    }

} // namespace