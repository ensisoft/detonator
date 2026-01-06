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
#include <memory>

#include "base/bitflag.h"
#include "graphics/enum.h"
#include "graphics/types.h"
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

    class Texture;
    class Device;

    class GenericShaderProgram : public ShaderProgram
    {
    public:
        static constexpr auto MAX_LIGHTS = 10;

        enum class ShadingFeatures {
            BasicLight, BasicFog, BasicShadows
        };

        enum class OutputFeatures {
            WriteColorTarget, WriteBloomTarget
        };
        enum class LightProjectionType {
            Invalid, Orthographic, Perspective
        };

        using LightType = BasicLightType;
        using FogMode   = BasicFogMode;
        using Light     = BasicLight;
        using Fog       = BasicFog;

        GenericShaderProgram() noexcept
        {
            mOutputFeatures.set(OutputFeatures::WriteColorTarget, true);
        }

        explicit GenericShaderProgram(std::string program_name,
                                      std::string renderer_name = "") noexcept
          : mProgramName(std::move(program_name))
         , mRendererName(std::move(renderer_name))
        {
            mOutputFeatures.set(OutputFeatures::WriteColorTarget, true);
        }

        auto GetLightCount() const noexcept
        { return mLights.size(); }

        const auto& GetLight(size_t index) const noexcept
        { return base::SafeIndex(mLights, index); }
        auto GetLight(size_t index) noexcept
        { return *base::SafeIndex(mLights, index); }

        auto GetShadowMapWidth() const noexcept
        { return static_cast<unsigned>(mShadowMapWidth); }
        auto GetShadowMapHeight() const noexcept
        { return static_cast<unsigned>(mShadowMapHeight); }

        void AddLight(const Light& light)
        { mLights.push_back(MakeSharedLight(light)); }
        void AddLight(Light&& light)
        { mLights.push_back(MakeSharedLight(std::move(light))); }
        void AddLight(std::shared_ptr<const Light> light)
        { mLights.push_back(std::move(light)); }

        void SetLight(const Light& light, size_t index) noexcept
        { base::SafeIndex(mLights, index) = MakeSharedLight(light); }
        void SetLight(Light&& light, size_t index) noexcept
        { base::SafeIndex(mLights, index) = MakeSharedLight(std::move(light)); }

        void ClearLights() noexcept
        { mLights.clear(); }

        void SetFog(const Fog& fog) noexcept
        { mFog = fog; }

        void SetBloomColor(const gfx::Color4f& color) noexcept
        { mBloomColor = color; }
        void SetBloomThreshold(float threshold) noexcept
        { mBloomThreshold = threshold; }

        void SetCameraCenter(glm::vec3 center) noexcept
        { mCameraCenter = center; }
        void SetCameraCenter(float x, float y, float z) noexcept
        { mCameraCenter = glm::vec3 {x, y, z }; }

        void EnableFeature(ShadingFeatures feature, bool on_off) noexcept
        { mShadingFeatures.set(feature, on_off); }

        void EnableFeature(OutputFeatures features, bool on_off) noexcept
        { mOutputFeatures.set(features, on_off); }

        bool TestFeature(ShadingFeatures feature) const noexcept
        { return mShadingFeatures.test(feature); }

        bool TestFeature(OutputFeatures feature) const noexcept
        { return mOutputFeatures.test(feature); }

        void SetShadowMapSize(unsigned width, unsigned height)
        {
            mShadowMapWidth = static_cast<float>(width);
            mShadowMapHeight = static_cast<float>(height);
        }

        std::string GetName() const override
        { return mProgramName; }

        std::string GetShaderId(const Material& material, const Material::Environment& env) const override;
        std::string GetShaderId(const Drawable& drawable, const Drawable::Environment& env) const override;
        ShaderSource GetShader(const Material& material, const Material::Environment& env, const Device& device, ShaderSourceError* error) const override;
        ShaderSource GetShader(const Drawable& drawable, const Drawable::Environment& env, const Device& device, ShaderSourceError* error) const override;

        using ShaderProgram::GetShader;

        void InitializeResources(Device &device) const override;
        void ApplyDynamicState(const Device& device, ProgramState& program) const override;
        void ApplyDynamicState(const Device &device, const Environment &env, ProgramState &program, Device::RasterState &state, void *user) const override;
        void ApplyLightState(const Device& device, ProgramState& program) const;
        void ApplyFogState(const Device& device, ProgramState& program) const;

        glm::mat4 GetLightViewMatrix(unsigned light_index) const;
        glm::mat4 GetLightProjectionMatrix(unsigned light_index) const;
        LightProjectionType GetLightProjectionType(unsigned light_index) const;

        float GetLightProjectionNearPlane(unsigned light_index) const;
        float GetLightProjectionFarPlane(unsigned light_index) const;

        const Texture* GetLightShadowMap(const Device& device) const;

        static std::shared_ptr<const Light> MakeSharedLight(const Light& data)
        {
            return std::make_shared<Light>(data);
        }
        static std::shared_ptr<const Light> MakeSharedLight(Light&& light)
        {
            return std::make_shared<Light>(std::move(light));
        }
    private:
        void AddDebugInfo(ShaderSource& source, RenderPass rp) const;
    private:
        std::string mProgramName;
        std::string mRendererName;
        std::vector<std::shared_ptr<const Light>> mLights;
        glm::vec3 mCameraCenter = {0.0f, 0.0f, 0.0f};
        gfx::Color4f mBloomColor;
        float mBloomThreshold = 0.0f;
        float mShadowMapWidth = 1024.0f;
        float mShadowMapHeight = 1024.0f;
        float mLightNearPlane = 1.0f;
        float mLightFarPlane  = 10.0f;
        base::bitflag<ShadingFeatures> mShadingFeatures;
        base::bitflag<OutputFeatures> mOutputFeatures;
        Fog mFog;
    };
} // namespace
