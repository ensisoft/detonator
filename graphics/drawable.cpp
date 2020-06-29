// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
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

#include "config.h"

#include <functional> // for hash
#include <cmath>

#include "base/utility.h"
#include "drawable.h"
#include "device.h"
#include "shader.h"
#include "geometry.h"
#include "text.h"

namespace gfx
{

Shader* Arrow::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}
Geometry* Arrow::Upload(Device& device) const
{
    const auto* name = mStyle == Style::Outline   ? "ArrowOutline" :
                      (mStyle == Style::Wireframe ? "ArrowWireframe" : "Arrow");
    Geometry* geom = device.FindGeometry(name);
    if (!geom)
    {
        if (mStyle == Style::Outline)
        {
            const Vertex verts[] = {
                {{0.0f, -0.25f}, {0.0f, 0.75f}},
                {{0.0f, -0.75f}, {0.0f, 0.25f}},
                {{0.7f, -0.75f}, {0.7f, 0.25f}},
                {{0.7f, -1.0f}, {0.7f, 0.0f}},
                {{1.0f, -0.5f}, {1.0f, 0.5f}},
                {{0.7f, -0.0f}, {0.7f, 1.0f}},
                {{0.7f, -0.25f}, {0.7f, 0.75f}},
            };
            geom = device.MakeGeometry(name);
            geom->Update(verts, 7);
        }
        else if (mStyle == Style::Solid)
        {
            const Vertex verts[] = {
                // body
                {{0.0f, -0.25f}, {0.0f, 0.75f}},
                {{0.0f, -0.75f}, {0.0f, 0.25f}},
                {{0.7f, -0.25f}, {0.7f, 0.75f}},
                // body
                {{0.7f, -0.25f}, {0.7f, 0.75f}},
                {{0.0f, -0.75f}, {0.0f, 0.25f}},
                {{0.7f, -0.75f}, {0.7f, 0.25f}},

                // arrow head
                {{0.7f, -0.0f}, {0.7f, 1.0f}},
                {{0.7f, -1.0f}, {0.7f, 0.0f}},
                {{1.0f, -0.5f}, {1.0f, 0.5f}},
            };
            geom = device.MakeGeometry(name);
            geom->Update(verts, 9);
        }
        else if (mStyle == Style::Wireframe)
        {
            const Vertex verts[] = {
                // body
                {{0.0f, -0.25f}, {0.0f, 0.75f}},
                {{0.0f, -0.75f}, {0.0f, 0.25f}},
                {{0.0f, -0.75f}, {0.0f, 0.25f}},
                {{0.7f, -0.25f}, {0.7f, 0.75f}},
                {{0.7f, -0.25f}, {0.7f, 0.75f}},
                {{0.0f, -0.25f}, {0.0f, 0.75f}},

                // body
                {{0.0f, -0.75f}, {0.0f, 0.25f}},
                {{0.7f, -0.75f}, {0.7f, 0.25f}},
                {{0.7f, -0.75f}, {0.7f, 0.25f}},
                {{0.7f, -0.25f}, {0.0f, 0.75f}},

                // arrow head
                {{0.7f, -0.0f}, {0.7f, 1.0f}},
                {{0.7f, -1.0f}, {0.7f, 0.0f}},
                {{0.7f, -1.0f}, {0.7f, 0.0f}},
                {{1.0f, -0.5f}, {1.0f, 0.5f}},
                {{1.0f, -0.5f}, {1.0f, 0.5f}},
                {{0.7f, -0.0f}, {0.7f, 1.0f}},
            };
            geom = device.MakeGeometry(name);
            geom->Update(verts, 16);

        }
    }
    if (mStyle == Style::Solid)
        geom->SetDrawType(Geometry::DrawType::Triangles);
    else if (mStyle == Style::Wireframe)
        geom->SetDrawType(Geometry::DrawType::Lines);
    else if (mStyle == Style::Outline)
        geom->SetDrawType(Geometry::DrawType::LineLoop);

    geom->SetLineWidth(mLineWidth);
    return geom;
}

Shader* Circle::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}
Geometry* Circle::Upload(Device& device) const
{
    // todo: we could use some information here about the
    // eventual transform on the screen and use that to compute
    // some kind of "LOD" value for figuring out how many slices we should have.
    const auto slices = 100;
    const auto* name = mStyle == Style::Outline   ? "CircleOutline" :
                      (mStyle == Style::Wireframe ? "CircleWireframe" : "Circle");

    Geometry* geom = device.FindGeometry(name);
    if (!geom)
    {
        std::vector<Vertex> vs;

        // center point for triangle fan.
        Vertex center;
        center.aPosition.x =  0.5;
        center.aPosition.y = -0.5;
        center.aTexCoord.x = 0.5;
        center.aTexCoord.y = 0.5;
        if (mStyle == Style::Solid)
        {
            vs.push_back(center);
        }

        const float angle_increment = (float)(math::Pi * 2.0f) / slices;
        float angle = 0.0f;

        for (unsigned i=0; i<=slices; ++i)
        {
            const auto x = std::cos(angle) * 0.5f;
            const auto y = std::sin(angle) * 0.5f;
            angle += angle_increment;
            Vertex v;
            v.aPosition.x = x + 0.5f;
            v.aPosition.y = y - 0.5f;
            v.aTexCoord.x = x + 0.5f;
            v.aTexCoord.y = y + 0.5f;
            vs.push_back(v);

            if (mStyle == Style::Wireframe)
            {
                const auto x = std::cos(angle) * 0.5f;
                const auto y = std::sin(angle) * 0.5f;
                Vertex v;
                v.aPosition.x = x + 0.5f;
                v.aPosition.y = y - 0.5f;
                v.aTexCoord.x = x + 0.5f;
                v.aTexCoord.y = y + 0.5f;
                vs.push_back(v);
                vs.push_back(center);
            }
        }
        geom = device.MakeGeometry(name);
        geom->Update(&vs[0], vs.size());

    }
    if (mStyle == Style::Solid)
        geom->SetDrawType(Geometry::DrawType::TriangleFan);
    else if (mStyle == Style::Outline)
        geom->SetDrawType(Geometry::DrawType::LineLoop);
    else if (mStyle == Style::Wireframe)
        geom->SetDrawType(Geometry::DrawType::LineLoop);
    geom->SetLineWidth(mLineWidth);
    return geom;
}

Shader* Rectangle::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}

Geometry* Rectangle::Upload(Device& device) const
{
    Geometry* geom = device.FindGeometry(mStyle == Style::Outline ? "RectangleOutline" : "Rectangle");
    if (!geom)
    {
        if (mStyle == Style::Outline)
        {
            const Vertex verts[6] = {
                { {0.0f,  0.0f}, {0.0f, 1.0f} },
                { {0.0f, -1.0f}, {0.0f, 0.0f} },
                { {1.0f, -1.0f}, {1.0f, 0.0f} },
                { {1.0f,  0.0f}, {1.0f, 1.0f} }
            };
            geom = device.MakeGeometry("RectangleOutline");
            geom->Update(verts, 4);
        }
        else
        {
            const Vertex verts[6] = {
                { {0.0f,  0.0f}, {0.0f, 1.0f} },
                { {0.0f, -1.0f}, {0.0f, 0.0f} },
                { {1.0f, -1.0f}, {1.0f, 0.0f} },

                { {0.0f,  0.0f}, {0.0f, 1.0f} },
                { {1.0f, -1.0f}, {1.0f, 0.0f} },
                { {1.0f,  0.0f}, {1.0f, 1.0f} }
            };
            geom = device.MakeGeometry("Rectangle");
            geom->Update(verts, 6);
        }
    }
    geom->SetLineWidth(mLineWidth);
    if (mStyle == Style::Solid)
        geom->SetDrawType(Geometry::DrawType::Triangles);
    else if (mStyle == Style::Wireframe)
        geom->SetDrawType(Geometry::DrawType::LineLoop);
    else if (mStyle== Style::Outline)
        geom->SetDrawType(Geometry::DrawType::LineLoop);
    return geom;
}

Shader* Triangle::GetShader(Device& device) const
{
    Shader* s = device.FindShader("vertex_array.glsl");
    if (s == nullptr || !s->IsValid())
    {
        if (s == nullptr)
            s = device.MakeShader("vertex_array.glsl");
        if (!s->CompileFile("shaders/es2/vertex_array.glsl"))
            return nullptr;
    }
    return s;
}

Geometry* Triangle::Upload(Device& device) const
{
    Geometry* geom = device.FindGeometry("Triangle");
    if (!geom)
    {
        const Vertex verts[3] = {
            { {0.5f,  0.0f}, {0.5f, 1.0f} },
            { {0.0f, -1.0f}, {0.0f, 0.0f} },
            { {1.0f, -1.0f}, {1.0f, 0.0f} }
        };
        geom = device.MakeGeometry("Triangle");
        geom->Update(verts, 3);
    }
    geom->SetLineWidth(mLineWidth);
    if (mStyle == Style::Solid)
        geom->SetDrawType(Geometry::DrawType::Triangles);
    else if (mStyle == Style::Outline)
        geom->SetDrawType(Geometry::DrawType::LineLoop); // this is not a mistake.
    else if (mStyle == Style::Wireframe)
        geom->SetDrawType(Geometry::DrawType::LineLoop); // this is not a mistake.
    return geom;
}

Shader* Grid::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}

Geometry* Grid::Upload(Device& device) const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mNumVerticalLines);
    hash = base::hash_combine(hash, mNumHorizontalLines);
    const auto& name = std::to_string(hash);

    Geometry* geom = device.FindGeometry(name);
    if (!geom)
    {
        std::vector<Vertex> verts;

        const float yadvance = 1.0f / (mNumHorizontalLines + 1);
        const float xadvance = 1.0f / (mNumVerticalLines + 1);
        for (unsigned i=1; i<=mNumVerticalLines; ++i)
        {
            const float x = i * xadvance;
            const Vertex line[2] = {
                {{x,  0.0f}, {x, 1.0f}},
                {{x, -1.0f}, {x, 0.0f}}
            };
            verts.push_back(line[0]);
            verts.push_back(line[1]);
        }
        for (unsigned i=1; i<=mNumHorizontalLines; ++i)
        {
            const float y = i * yadvance * -1.0f;
            const Vertex line[2] = {
                {{0.0f, y}, {0.0f, 1.0f + y}},
                {{1.0f, y}, {1.0f, 1.0f + y}},
            };
            verts.push_back(line[0]);
            verts.push_back(line[1]);
        }
        if (mBorderLines)
        {
            const Vertex corners[4] = {
                // top left
                {{0.0f, 0.0f}, {0.0f, 1.0f}},
                // top right
                {{1.0f, 0.0f}, {1.0f, 1.0f}},

                // bottom left
                {{0.0f, -1.0f}, {0.0f, 0.0f}},
                // bottom right
                {{1.0f, -1.0f}, {1.0f, 0.0f}}
            };
            verts.push_back(corners[0]);
            verts.push_back(corners[1]);
            verts.push_back(corners[2]);
            verts.push_back(corners[3]);
            verts.push_back(corners[0]);
            verts.push_back(corners[2]);
            verts.push_back(corners[1]);
            verts.push_back(corners[3]);
        }
        geom = device.MakeGeometry(name);
        geom->Update(std::move(verts));
        geom->SetDrawType(Geometry::DrawType::Lines);
    }
    geom->SetLineWidth(mLineWidth);
    return geom;
}


Shader* KinematicsParticleEngine::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}
// Drawable implementation. Upload particles to the device.
Geometry* KinematicsParticleEngine::Upload(Device& device) const
{
    Geometry* geom = device.FindGeometry("particle-buffer");
    if (!geom)
    {
        geom = device.MakeGeometry("particle-buffer");
    }
    std::vector<Vertex> verts;
    for (const auto& p : mParticles)
    {
        // in order to use vertex_arary.glsl we need to convert the points to
        // lower right quadrant coordinates.
        Vertex v;
        v.aPosition.x = p.position.x / mParams.max_xpos;
        v.aPosition.y = p.position.y / mParams.max_ypos * -1.0f;
        v.aTexCoord.x = p.pointsize >= 0.0f ? p.pointsize : 0.0f;
        verts.push_back(v);
    }

    geom->Update(std::move(verts));
    geom->SetDrawType(Geometry::DrawType::Points);
    return geom;
}

// Update the particle simulation.
void KinematicsParticleEngine::Update(float dt)
{
    mTime += dt;

    // update each particle
    for (size_t i=0; i<mParticles.size();)
    {
        if (UpdateParticle(i, dt))
        {
            ++i;
            continue;
        }
        KillParticle(i);
    }

    // Spawn new particles if needed.
    if (mParams.mode == SpawnPolicy::Maintain)
    {
        const auto num_particles_always = size_t(mParams.num_particles);
        const auto num_particles_now = mParticles.size();
        const auto num_particles_needed = num_particles_always - num_particles_now;
        InitParticles(num_particles_needed);
    }
    else if (mParams.mode == SpawnPolicy::Continuous)
    {
        // the number of particles is taken as the rate of particles per
        // second. fractionally cumulate partciles and then
        // spawn when we have some number non-fractional particles.
        mNumParticlesHatching += mParams.num_particles * dt;
        const auto num = size_t(mNumParticlesHatching);
        InitParticles(num);
        mNumParticlesHatching -= num;
    }
}

// ParticleEngine implementation.
bool KinematicsParticleEngine::IsAlive() const
{
    if (mParams.mode == SpawnPolicy::Continuous)
        return true;
    return !mParticles.empty();
}

// ParticleEngine implementation. Restart the simulation
// with the previous parameters if possible to do so.
void KinematicsParticleEngine::Restart()
{
    if (!mParticles.empty())
        return;
    if (mParams.mode != SpawnPolicy::Once)
        return;
    InitParticles(size_t(mParams.num_particles));
    mTime = 0.0f;
    mNumParticlesHatching = 0;
}

nlohmann::json KinematicsParticleEngine::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "motion", mParams.motion);
    base::JsonWrite(json, "mode", mParams.mode);
    base::JsonWrite(json, "boundary", mParams.boundary);
    base::JsonWrite(json, "num_particles", mParams.num_particles);
    base::JsonWrite(json, "min_lifetime", mParams.min_lifetime);
    base::JsonWrite(json, "max_lifetime", mParams.max_lifetime);
    base::JsonWrite(json, "max_xpos", mParams.max_xpos);
    base::JsonWrite(json, "max_ypos", mParams.max_ypos);
    base::JsonWrite(json, "init_rect_xpos", mParams.init_rect_xpos);
    base::JsonWrite(json, "init_rect_ypos", mParams.init_rect_ypos);
    base::JsonWrite(json, "init_rect_width", mParams.init_rect_width);
    base::JsonWrite(json, "init_rect_height", mParams.init_rect_height);
    base::JsonWrite(json, "min_velocity", mParams.min_velocity);
    base::JsonWrite(json, "max_velocity", mParams.max_velocity);
    base::JsonWrite(json, "direction_sector_start_angle", mParams.direction_sector_start_angle);
    base::JsonWrite(json, "direction_sector_size", mParams.direction_sector_size);
    base::JsonWrite(json, "min_point_size", mParams.min_point_size);
    base::JsonWrite(json, "max_point_size", mParams.max_point_size);
    base::JsonWrite(json, "growth_over_time", mParams.rate_of_change_in_size_wrt_time);
    base::JsonWrite(json, "growth_over_dist", mParams.rate_of_change_in_size_wrt_dist);
    return json;
}

// static
std::optional<KinematicsParticleEngine> KinematicsParticleEngine::FromJson(const nlohmann::json& object)
{
    Params params;
    if (!base::JsonReadSafe(object, "motion", &params.motion) ||
        !base::JsonReadSafe(object, "mode", &params.mode) ||
        !base::JsonReadSafe(object, "boundary", &params.boundary) ||
        !base::JsonReadSafe(object, "num_particles", &params.num_particles) ||
        !base::JsonReadSafe(object, "min_lifetime", &params.min_lifetime) ||
        !base::JsonReadSafe(object, "max_lifetime", &params.max_lifetime) ||
        !base::JsonReadSafe(object, "max_xpos", &params.max_xpos) ||
        !base::JsonReadSafe(object, "max_ypos", &params.max_ypos) ||
        !base::JsonReadSafe(object, "init_rect_xpos", &params.init_rect_xpos) ||
        !base::JsonReadSafe(object, "init_rect_ypos", &params.init_rect_ypos) ||
        !base::JsonReadSafe(object, "init_rect_width", &params.init_rect_width) ||
        !base::JsonReadSafe(object, "init_rect_height", &params.init_rect_height) ||
        !base::JsonReadSafe(object, "min_velocity", &params.min_velocity) ||
        !base::JsonReadSafe(object, "max_velocity", &params.max_velocity) ||
        !base::JsonReadSafe(object, "direction_sector_start_angle", &params.direction_sector_start_angle) ||
        !base::JsonReadSafe(object, "direction_sector_size", &params.direction_sector_size) ||
        !base::JsonReadSafe(object, "min_point_size", &params.min_point_size) ||
        !base::JsonReadSafe(object, "max_point_size", &params.max_point_size) ||
        !base::JsonReadSafe(object, "growth_over_time", &params.rate_of_change_in_size_wrt_time) ||
        !base::JsonReadSafe(object, "growth_over_dist", &params.rate_of_change_in_size_wrt_dist))
            return std::nullopt;
    return KinematicsParticleEngine(params);
}

void KinematicsParticleEngine::InitParticles(size_t num)
{
    for (size_t i=0; i<num; ++i)
    {
        const auto velocity = math::rand(mParams.min_velocity, mParams.max_velocity);
        const auto initx = math::rand(0.0f, mParams.init_rect_width);
        const auto inity = math::rand(0.0f, mParams.init_rect_height);
        const auto angle = math::rand(0.0f, mParams.direction_sector_size) +
            mParams.direction_sector_start_angle;

        Particle p;
        p.lifetime  = math::rand(mParams.min_lifetime, mParams.max_lifetime);
        p.pointsize = math::rand(mParams.min_point_size, mParams.max_point_size);
        p.position  = glm::vec2(mParams.init_rect_xpos + initx, mParams.init_rect_ypos + inity);
        // note that the velocity vector is baked into the
        // direction vector in order to save space.
        p.direction = glm::vec2(std::cos(angle), std::sin(angle)) * velocity;
        mParticles.push_back(p);
    }
}
void KinematicsParticleEngine::KillParticle(size_t i)
{
    const auto last = mParticles.size() - 1;
    std::swap(mParticles[i], mParticles[last]);
    mParticles.pop_back();
}

bool KinematicsParticleEngine::UpdateParticle(size_t i, float dt)
{
    auto& p = mParticles[i];

    // countdown to end of lifetime
    p.lifetime -= dt;
    if (p.lifetime <= 0.0f)
        return false;

    const auto p0 = p.position;

    // update change in position
    if (mParams.motion == Motion::Linear)
        p.position += (p.direction * dt);

    const auto& p1 = p.position;
    const auto& dp = p1 - p0;
    const auto  dd = glm::length(dp);

    // update change in size with respect to time.
    p.pointsize += (dt * mParams.rate_of_change_in_size_wrt_time);
    // update change in size with respect to distance
    p.pointsize += (dd * mParams.rate_of_change_in_size_wrt_dist);
    if (p.pointsize <= 0.0f)
        return false;
    // accumulate distance approximation
    p.distance += dd;

    // boundary conditions.
    if (mParams.boundary == BoundaryPolicy::Wrap)
    {
        p.position.x = math::wrap(0.0f, mParams.max_xpos, p.position.x);
        p.position.y = math::wrap(0.0f, mParams.max_ypos, p.position.y);
    }
    else if (mParams.boundary == BoundaryPolicy::Clamp)
    {
        p.position.x = math::clamp(0.0f, mParams.max_xpos, p.position.x);
        p.position.y = math::clamp(0.0f, mParams.max_ypos, p.position.y);
    }
    else if (mParams.boundary == BoundaryPolicy::Kill)
    {
        if (p.position.x < 0.0f || p.position.x > mParams.max_xpos)
            return false;
        else if (p.position.y < 0.0f || p.position.y > mParams.max_ypos)
            return false;
    }
    else if (mParams.boundary == BoundaryPolicy::Reflect)
    {
        glm::vec2 n;
        if (p.position.x <= 0.0f)
            n = glm::vec2(1.0f, 0.0f);
        else if (p.position.x >= mParams.max_xpos)
            n = glm::vec2(-1.0f, 0.0f);
        else if (p.position.y <= 0.0f)
            n = glm::vec2(0, 1.0f);
        else if (p.position.y >= mParams.max_ypos)
            n = glm::vec2(0, -1.0f);
        else return true;
        // compute new direction vector given the normal vector of the boundary
        // and then bake the velocity in the new direction
        const auto& d = glm::normalize(p.direction);
        const float v = glm::length(p.direction);
        p.direction = (d - 2 * glm::dot(d, n) * n) * v;

        // clamp the position in order to eliminate the situation
        // where the object has moved beyond the boundaries of the simulation
        // and is stuck there alternating it's direction vector
        p.position.x = math::clamp(0.0f, mParams.max_xpos, p.position.x);
        p.position.y = math::clamp(0.0f, mParams.max_ypos, p.position.y);
    }
    return true;
}


} // namespace
