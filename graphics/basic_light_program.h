// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include "warnpush.h"
#  include <glm/vec3.hpp>
#include "warnpop.h"

#include <vector>
#include <cstdint>

#include "base/utility.h"
#include "graphics/shaderprogram.h"
#include "graphics/color4f.h"
#include "graphics/types.h"

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

    class BasicLightProgram : public ShaderProgram
    {
    public:
        static constexpr auto MAX_LIGHTS = 10;

        enum class LightType : int32_t {
            Ambient,
            Directional,
            Spot,
            Point
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

        inline void SetCameraCenter(glm::vec3 center) noexcept
        { mCameraCenter = center; }
        inline void SetCameraCenter(float x, float y, float z) noexcept
        { mCameraCenter = glm::vec3 {x, y, z }; }

        RenderPass GetRenderPass() const override;
        std::string GetShaderId(const Material& material, const Material::Environment& env) const override;
        std::string GetShaderId(const Drawable& drawable, const Drawable::Environment& env) const override;
        std::string GetShaderName(const Material& material, const Material::Environment& env) const override;
        std::string GetShaderName(const Drawable& drawable, const Drawable::Environment& env) const override;
        std::string GetName() const override;
        ShaderSource GetShader(const Material& material, const Material::Environment& env, const Device& device) const override;
        ShaderSource GetShader(const Drawable& drawable, const Drawable::Environment& env, const Device& device) const override;

        void ApplyDynamicState(const Device& device, ProgramState& program) const override;
    private:
        std::vector<Light> mLights;
        glm::vec3 mCameraCenter = {0.0f, 0.0f, 0.0f};
    };

} // namespace gfx