// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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
#  include <glm/vec3.hpp>
#include "warnpop.h"

#include <string>
#include <vector>

#include "base/bitflag.h"
#include "graphics/shader_program.h"

namespace gfx
{
    //
    // Light Types and Components:
    // -----------------------------------------------------------------------------------------------------------
    // | Light Type       | Description                                           | Ambient | Diffuse | Specular |
    // |------------------|-------------------------------------------------------|---------|---------|----------|
    // | Ambient Light    | Simulates global illumination, uniform lighting.      |   Yes   |   No    |    No    |
    // | Directional Light| Parallel rays, simulates distant sources (e.g., sun). |   Yes   |   Yes   |    Yes   |
    // | Point Light      | Emits light from a point source in all directions.    |   Yes   |   Yes   |    Yes   |
    // | Spotlight        | Point light constrained to a cone, with attenuation.  |   Yes   |   Yes   |    Yes   |
    // -----------------------------------------------------------------------------------------------------------
    //
    // Light Components Description:
    // - Ambient Component:
    //   * A constant illumination applied equally to all objects.
    //   * Simulates indirect lighting, ensuring no part of the scene is completely dark.
    //
    // - Diffuse Component:
    //   * Illumination depends on the angle between the light direction and surface normal.
    //   * Creates shading that gives objects a sense of shape and depth.
    //
    // - Specular Component:
    //   * A shiny highlight dependent on view direction and light reflection.
    //   * Simulates reflective properties of materials.
    //
    // Light Types Description:
    // - Ambient Light:
    //   * A global light source that evenly illuminates all objects without direction or distance considerations.
    //
    // - Directional Light:
    //   * Light with parallel rays, typically used to simulate sunlight.
    //   * Does not attenuate with distance as the source is infinitely far away.
    //
    // - Point Light:
    //   * Emits light in all directions from a single point in 3D space.
    //   * Includes attenuation to simulate the light weakening over distance.
    //
    // - Spotlight:
    //   * A point light constrained to a cone.
    //   * Includes direction, cutoff angle (cone angle), and attenuation for realistic spotlight effects.
    //


    class GenericShaderProgram : public ShaderProgram
    {
    public:
        static constexpr auto MAX_LIGHTS = 10;

        enum class ShadingFeatures {
            BasicLight, BasicFog
        };

        enum class OutputFeatures {
            WriteColorTarget, WriteBloomTarget
        };

        enum class LightType : int32_t {
            Ambient,
            Directional,
            Spot,
            Point
        };

        enum class FogMode : uint32_t {
            Linear, Exponential1, Exponential2
        };

        struct Light {
            LightType type = LightType::Ambient;
            // lights position in view space. in other words the
            // result of transforming the light with the light's model
            // view matrix.
            glm::vec3 position = {0.0f, 0.0f, 0.0f};
            // light's direction vector that applies to spot and
            // directional lights.
            glm::vec3 direction = {0.0f, 0.0f, -1.0f};
            gfx::Color4f ambient_color;
            gfx::Color4f diffuse_color;
            gfx::Color4f specular_color;
            gfx::FDegrees spot_half_angle;
            float constant_attenuation = 1.0f;
            float linear_attenuation = 0.0f;
            float quadratic_attenuation = 0.0f;
        };

        struct Fog {
            gfx::Color4f color;
            float start_dist = 10.0f;
            float end_dist = 100.0f;
            float density = 1.0f;
            FogMode mode = FogMode::Linear;
        };

        GenericShaderProgram() noexcept
        {
            mOutputFeatures.set(OutputFeatures::WriteColorTarget, true);
        }

        explicit GenericShaderProgram(std::string name, RenderPass renderPass) noexcept
          : mName(std::move(name))
        {
            mOutputFeatures.set(OutputFeatures::WriteColorTarget, true);
        }

        inline auto GetLightCount() const noexcept
        { return mLights.size(); }

        inline const auto& GetLight(size_t index) const noexcept
        { return base::SafeIndex(mLights, index); }
        inline auto& GetLight(size_t index) noexcept
        { return base::SafeIndex(mLights, index); }

        inline void AddLight(const Light& light)
        { mLights.push_back(light); }
        inline void AddLight(Light&& light)
        { mLights.push_back(std::move(light)); }

        inline void SetLight(const Light& light, size_t index) noexcept
        { base::SafeIndex(mLights, index) = light; }
        inline void SetLight(Light&& light, size_t index) noexcept
        { base::SafeIndex(mLights, index) = std::move(light); }

        inline void SetFog(const Fog& fog) noexcept
        { mFog = fog; }

        inline void SetBloomColor(const gfx::Color4f& color) noexcept
        { mBloomColor = color; }
        inline void SetBloomThreshold(float threshold) noexcept
        { mBloomThreshold = threshold; }

        inline void SetCameraCenter(glm::vec3 center) noexcept
        { mCameraCenter = center; }
        inline void SetCameraCenter(float x, float y, float z) noexcept
        { mCameraCenter = glm::vec3 {x, y, z }; }

        inline void EnableFeature(ShadingFeatures feature, bool on_off) noexcept
        { mShadingFeatures.set(feature, on_off); }

        inline void EnableFeature(OutputFeatures features, bool on_off) noexcept
        { mOutputFeatures.set(features, on_off); }

        inline bool TestFeature(ShadingFeatures feature) const noexcept
        { return mShadingFeatures.test(feature); }

        inline bool TestFeature(OutputFeatures feature) const noexcept
        { return mOutputFeatures.test(feature); }

        std::string GetName() const override
        { return mName; }

        RenderPass GetRenderPass() const override
        { return mRenderPass; }

        std::string GetShaderId(const Material& material, const Material::Environment& env) const override;
        std::string GetShaderId(const Drawable& drawable, const Drawable::Environment& env) const override;
        ShaderSource GetShader(const Material& material, const Material::Environment& env, const Device& device) const override;
        ShaderSource GetShader(const Drawable& drawable, const Drawable::Environment& env, const Device& device) const override;

        void ApplyDynamicState(const Device& device, ProgramState& program) const override;
        void ApplyLightState(const Device& device, ProgramState& program) const;
        void ApplyFogState(const Device& device, ProgramState& program) const;

    private:
        std::string mName;
        std::vector<Light> mLights;
        glm::vec3 mCameraCenter = {0.0f, 0.0f, 0.0f};
        gfx::Color4f mBloomColor;
        float mBloomThreshold = 0.0f;
        base::bitflag<ShadingFeatures> mShadingFeatures;
        base::bitflag<OutputFeatures> mOutputFeatures;
        RenderPass mRenderPass = RenderPass::ColorPass;
        Fog mFog;
    };
} // namespace
