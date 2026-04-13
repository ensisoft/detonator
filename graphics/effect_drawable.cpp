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

#include "config.h"

#include "base/assert.h"
#include "base/logging.h"
#include "base/utility.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/effect_drawable.h"
#include "graphics/drawable_effect.h"

namespace gfx
{
EffectDrawable::EffectDrawable(std::shared_ptr<Drawable> drawable, std::string effectId, std::string effectName) noexcept
  : mDrawable(std::move(drawable))
  , mEffectId(std::move(effectId))
 , mEffectName(std::move(effectName))
{
    mSourceDrawable = mDrawable;
}

bool EffectDrawable::EnableEffect()
{
    if (mEnabled)
    {
        WARN("Drawable effect is already enabled. [effect='%1']", mEffectName);
        return false;
    }

    if (auto* args = std::get_if<MeshExplosionEffectArgs>(&mArgs))
    {
        DrawableEffect::MeshExplosion explosion;
        explosion.mesh_subdivision_count = args->mesh_subdivision_count;
        explosion.shard_linear_acceleration = args->shard_linear_acceleration;
        explosion.shard_linear_speed = args->shard_linear_speed;
        explosion.shard_rotational_acceleration = args->shard_rotational_acceleration;
        explosion.shard_rotational_speed = args->shard_rotational_speed;
        if (mEffectDrawable)
        {
            mEnabled = mEffectDrawable->SetEffect(DrawableEffect(explosion));
            if (mEnabled)
                mDrawable = mEffectDrawable;
        }
        else
        {
            mEnabled = mDrawable->SetEffect(DrawableEffect(explosion));
        }
    }
    else BUG("Unhandled effect drawable effect type.");
    return mEnabled;
}

bool EffectDrawable::DisableEffect()
{
    if (!mEnabled)
    {
        WARN("Drawable effect is already disabled. [effect='%1']", mEffectName);
        return false;
    }

    mDrawable->DeleteEffect();

    mEnabled = false;
    if (mSourceDrawable)
        mDrawable = mSourceDrawable;

    return true;
}

bool EffectDrawable::ApplyDynamicState(const Environment& env, const DrawCall& draw, const DrawGeometryHandle& geometry,
    Device& device, ProgramState& program, RasterState&  state) const
{
    return mDrawable->ApplyDynamicState(env, draw, geometry, device, program, state);
}

ShaderSource EffectDrawable::GetShader(const Environment& env, const Device& device) const
{
    return mDrawable->GetShader(env, device);
}
std::string EffectDrawable::GetShaderId(const Environment& env) const
{
    return mDrawable->GetShaderId(env);
}
std::string EffectDrawable::GetShaderName(const Environment& env) const
{
    return mDrawable->GetShaderName(env);
}

DrawGeometryHandle EffectDrawable::GetGeometry(const Environment& env, Device& device) const
{
    return mDrawable->GetGeometry(env, device);
}

DrawGeometryBuffer EffectDrawable::Construct(const Environment& env) const
{
    return mDrawable->Construct(env);
}

void EffectDrawable::Update(const Environment& env, float dt)
{
    mDrawable->Update(env, dt);
}

void EffectDrawable::Restart(const Environment& env)
{
    mDrawable->Restart(env);
}

Drawable::DrawPrimitive EffectDrawable::GetDrawPrimitive() const
{
    return mDrawable->GetDrawPrimitive();
}

SpatialMode EffectDrawable::GetSpatialMode() const
{
    return mDrawable->GetSpatialMode();
}

bool EffectDrawable::IsAlive() const
{
    return mDrawable->IsAlive();
}
Drawable::Type EffectDrawable::GetType() const
{
    return Type::EffectsDrawable;
}

void EffectDrawable::Execute(const Environment& env, const Command& command)
{
    if (command.name == "EnableMeshEffect")
    {
        DEBUG("Received mesh effect command. [effect='%1', cmd='%2']", mEffectName, command.name);
        if (const auto* ptr = base::SafeFind(command.args, std::string("state")))
        {
            if (const auto* state = std::get_if<std::string>(ptr))
            {
                bool state_now = mEnabled;

                if (*state == "toggle")
                    state_now = !state_now;
                else if (*state == "on")
                    state_now = true;
                else if (*state == "off")
                    state_now = false;
                else WARN("Ignoring enable mesh effect command with unexpected state parameter. [effect='%1', state='%2']", mEffectName, state);

                if (state_now != mEnabled)
                {
                    if (state_now)
                        EnableEffect();
                    else DisableEffect();
                }
            }
            else
            {
                WARN("Ignoring enable mesh effect command with unexpected 'state' parameter type. Expected 'string'. [effect='%1']", mEffectName);
            }
        } else WARN("Ignoring enable mesh effect command without 'state' parameter. [effect='%1']", mEffectName);
    }
    mDrawable->Execute(env, command);
}
Drawable::DrawCmd EffectDrawable::GetDrawCmd() const
{
    return mDrawable->GetDrawCmd();
}

// static
void EffectDrawable::SetRandomGenerator(std::function<float(float min, float max)> rf)
{
    DrawableEffect::SetRandomGenerator(std::move(rf));
}

} // namespace
