// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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
#  include <glm/mat4x4.hpp>
#  include <glm/mat3x3.hpp>
#  include <glm/mat2x2.hpp>
#  include <glm/gtc/type_ptr.hpp>
#include "warnpop.h"

#include <cstring>
#include <vector>
#include <variant>
#include <memory>

#include "graphics/color4f.h"

namespace gfx
{
    class Shader;
    class Texture;

    class ProgramState
    {
    public:
        struct Sampler {
            std::string name;
            unsigned unit = 0;
            const Texture* texture = nullptr;
        };
        struct Uniform {
            std::string name;
            std::variant<int, float,
                    glm::ivec2,
                    glm::vec2,
                    glm::vec3,
                    glm::vec4,
                    gfx::Color4f,
                    glm::mat2,
                    glm::mat3,
                    glm::mat4> value;
        };

        // Set scalar uniform.
        inline void SetUniform(const char* name, int x)
        {
            mUniforms.push_back({ name, x });
        }
        // Set scalar uniform.
        inline void SetUniform(const char* name, int x, int y)
        {
            mUniforms.push_back({ name, glm::ivec2{x, y} });
        }

        // Set scalar uniform.
        inline void SetUniform(const char* name, float x)
        {
            mUniforms.push_back({ name, x });
        }
        // Set vec2 uniform.
        inline void SetUniform(const char* name, float x, float y)
        {
            mUniforms.push_back({ name, glm::vec2{x, y} });
        }
        // Set vec3 uniform.
        inline void SetUniform(const char* name, float x, float y, float z)
        {
            mUniforms.push_back({ name, glm::vec3{x, y, z} });
        }
        // Set vec4 uniform.
        inline void SetUniform(const char* name, float x, float y, float z, float w)
        {
            mUniforms.push_back({ name, glm::vec4{x, y, z, w} });
        }
        // set Color uniform
        inline void SetUniform(const char* name, const Color4f& color)
        {
            mUniforms.push_back({name, color});
        }

        inline void SetUniform(const char* name, const glm::vec2& vector)
        {
            mUniforms.push_back({name, vector});
        }

        inline void SetUniform(const char* name, const glm::vec3& vector)
        {
            mUniforms.push_back({name, vector});
        }

        inline void SetUniform(const char* name, const glm::vec4& vector)
        {
            mUniforms.push_back({name, vector});
        }

        // Matrix memory layout is as follows:
        // {xx xy xz}
        // {yx yy yz}
        // {zx zy zz}
        // or {{xx xy xz}, {yx yy yz}, {zx zy zz}}
        // In other words the first 3 floats are the X vector followed by the
        // Y vector followed by the Z vector.

        // set 2x2 matrix uniform.
        inline void SetUniform(const char* name, const glm::mat2& matrix)
        {
            mUniforms.push_back({name, matrix });
        }
        // Set 3x3 matrix uniform.
        inline void SetUniform(const char* name, const glm::mat3& matrix)
        {
            mUniforms.push_back({name, matrix });
        }
        // Set 4x4 matrix uniform.
        inline void SetUniform(const char* name, const glm::mat4& matrix)
        {
            mUniforms.push_back({name, matrix });
        }


        // Set a texture sampler.
        // Sampler is the name of the texture sampler in the shader.
        // It's possible to sample multiple textures in the program by setting each
        // texture to a different texture unit.
        void SetTexture(const char* sampler, unsigned unit, const Texture& texture)
        {
            // in OpenGL the expected memory layout of texture data that
            // is given to glTexImage2D doesn't match the "typical" layout
            // that is used by many toolkits/libraries. The order of scan lines
            // is reversed and glTexImage expects the first scanline (in memory)
            // to be the bottom scanline of the image.
            // There are several ways to deal with this dilemma:
            // - flip all images on the horizontal axis before calling glTexImage2D.
            // - use a matrix to transform the texture coordinates to counter this.
            // - flip texture coordinates and assume that Y=0.0f means the top of
            //   the texture (first scan row) and Y=1.0f means the bottom of the
            //   texture (last scan row).
            //
            //
            // this comment and the below kDeviceTextureMatrix is here only for posterity.
            // Currently, we have flipped the texture coordinates like explained above.
            //
            // If the program being used to render stuff is using a texture
            // we helpfully set up here a "device texture matrix" that will be provided
            // for the program and should be used to transform the texture coordinates
            // before sampling textures.
            // This will avoid having to do any image flips which is especially
            // handy when dealing with data that gets often (re)uploaded, e.g.
            // dynamically changing/procedural texture data.
            //
            // It should also be possible to use the device texture matrix for example
            // in cases where the device would bake some often used textures into an atlas
            // and implicitly alter the texture coordinates.
            //
            /* no longer used.
            static const float kDeviceTextureMatrix[3][3] = {
                {1.0f, 0.0f, 0.0f},
                {0.0f, -1.0f, 0.0f},
                {0.0f, 1.0f, 0.0f}
            };
            SetUniform("kDeviceTextureMatrix", kDeviceTextureMatrix);
            */
            if (unit >= mSamplers.size())
                mSamplers.resize(unit + 1);

            mSamplers[unit].texture = &texture;
            mSamplers[unit].name    = sampler;
            mSamplers[unit].unit    = unit;
        }
        // Set the number of textures used by the next draw.
        // Todo: this api and the SetTexture are potentially bug prone, it'd be better to
        // combine both into a single api call that takes the whole array of textures.
        inline void SetTextureCount(unsigned count)
        {
            mSamplers.resize(count);
        }

        inline size_t GetUniformCount() const noexcept
        {
            return mUniforms.size();
        }
        inline size_t GetSamplerCount() const noexcept
        {
            return mSamplers.size();
        }

        inline const Sampler& GetSamplerSetting(size_t index) const noexcept
        {
            return mSamplers[index];
        }
        inline const Uniform& GetUniformSetting(size_t index) const noexcept
        {
            return mUniforms[index];
        }

        template<typename T>
        bool GetUniform(const char* name, T* value) noexcept
        {
            for (const auto& u : mUniforms)
            {
                if (u.name != name)
                    continue;
                ASSERT(std::holds_alternative<T>(u.value));
                *value = std::get<T>(u.value);
                return true;
            }
            return false;
        }
        inline bool HasUniform(const char* name) noexcept
        {
            for (const auto& u : mUniforms)
            {
                if (u.name == name)
                    return true;
            }
            return false;
        }

        inline void Clear() noexcept
        {
            mUniforms.clear();
            mSamplers.clear();
        }

        const Sampler* FindTextureBinding(const char* name) const
        {
            for (const auto& s : mSamplers)
            {
                if (s.name == name)
                    return &s;
            }
            return nullptr;
        }


    private:
        std::vector<Sampler> mSamplers;
        std::vector<Uniform> mUniforms;
    };


    // Program object interface. Program objects are device
    // specific graphics programs that are built from shaders
    // and then uploaded and executed on the device.
    class Program
    {
    public:
        struct CreateArgs {
            // The program state that is applied initially on the program
            // once when created. Note that this only applies to uniforms!
            ProgramState state;
            // The program human-readable debug name
            std::string name;
            // mandatory fragment shader. Must be valid
            std::shared_ptr<const Shader> fragment_shader;
            // mandatory vertex shader. Must be valid.
            std::shared_ptr<const Shader> vertex_shader;
        };
        virtual ~Program() = default;

        // Returns true if the program is valid or not I.e. it has been
        // successfully build and can be used for drawing.
        virtual bool IsValid() const = 0;


        // this API exists here to facilitate migration towards design where
        // the program state (i.e. uniform and texture sampler state) is separate
        // from the program object.
        inline void SetUniform(const char* name, int x) const
        {
            GetState().SetUniform(name, x);
        }
        inline void SetUniform(const char* name, int x, int y) const
        {
            GetState().SetUniform(name, x, y);
        }
        inline void SetUniform(const char* name, float x) const
        {
            GetState().SetUniform(name, x);
        }
        inline void SetUniform(const char* name, float x, float y) const
        {
            GetState().SetUniform(name, x, y);
        }
        inline void SetUniform(const char* name, float x, float y, float z) const
        {
            GetState().SetUniform(name, x, y, z);
        }
        inline void SetUniform(const char* name, float x, float y, float z, float w) const
        {
            GetState().SetUniform(name, x, y, z, w);
        }
        inline void SetUniform(const char* name, const Color4f& color) const
        {
            GetState().SetUniform(name, color);
        }
        inline void SetUniform(const char* name, const glm::vec2& vector) const
        {
            GetState().SetUniform(name, vector);
        }
        inline void SetUniform(const char* name, const glm::vec3& vector) const
        {
            GetState().SetUniform(name, vector);
        }
        inline void SetUniform(const char* name, const glm::vec4& vector) const
        {
            GetState().SetUniform(name, vector);
        }

        inline void SetUniform(const char* name, const glm::mat2& matrix) const
        {
            GetState().SetUniform(name, matrix);
        }
        inline void SetUniform(const char* name, const glm::mat3& matrix) const
        {
            GetState().SetUniform(name, matrix);
        }
        inline void SetUniform(const char* name, const glm::mat4& matrix) const
        {
            GetState().SetUniform(name, matrix);
        }
        inline void SetTextureCount(unsigned count) const
        {
            GetState().SetTextureCount(count);
        }
        inline void SetTexture(const char* sampler, unsigned unit, const Texture& texture) const
        {
            GetState().SetTexture(sampler, unit, texture);
        }

        std::shared_ptr<const ProgramState> TransferState() const
        {
            auto ret = mState;
            mState = {};
            return ret;
        }
        size_t GetPendingUniformCount() const
        {
            return mState ? mState->GetUniformCount() : 0;
        }
        size_t GetPendingSamplerCount() const
        {
            return mState ? mState->GetSamplerCount() : 0;
        }

        ProgramState& GetState() const
        {
            if (!mState)
                mState = std::make_shared<ProgramState>();
            return *mState;
        }
    private:
        mutable std::shared_ptr<ProgramState> mState;
    };

    using ProgramPtr = std::shared_ptr<const Program>;

} //
