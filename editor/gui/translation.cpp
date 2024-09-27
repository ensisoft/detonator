// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include "base/assert.h"
#include "editor/gui/translation.h"

namespace app
{
std::string TranslateEnum(Workspace::ProjectSettings::CanvasPresentationMode mode)
{
    using M = Workspace::ProjectSettings::CanvasPresentationMode;
    if (mode == M::Normal)
        return "Present as HTML element";
    else if (mode == M::FullScreen)
        return "Present in Fullscreen";
    else BUG("Missing translation");
    return "???";
}
std::string TranslateEnum(Workspace::ProjectSettings::CanvasFullScreenStrategy strategy)
{
    using S = Workspace::ProjectSettings::CanvasFullScreenStrategy;
    if (strategy == S::SoftFullScreen)
        return "Maximize Element on Page";
    else if (strategy == S::RealFullScreen)
        return "Use The Entire Screen";
    else BUG("Missing translation");
    return "???";
}

std::string TranslateEnum(Workspace::ProjectSettings::PowerPreference power)
{
    using P = Workspace::ProjectSettings::PowerPreference;
    if (power == P::Default)
        return "Browser Default";
    else if (power == P::HighPerf)
        return "High Performance";
    else if (power == P::LowPower)
        return "Battery Saving";
    else BUG("Missing translation");
    return "???";
}

std::string TranslateEnum(Workspace::ProjectSettings::WindowMode wm)
{
    using M = Workspace::ProjectSettings::WindowMode;
    if (wm == M::Windowed)
        return "Present in Window";
    else if (wm == M::Fullscreen)
        return "Present in Fullscreen";
    else BUG("Missing translation");
    return "???";
}

} // namespace

namespace engine
{

std::string TranslateEnum(FileResourceLoader::DefaultAudioIOStrategy strategy)
{
    using S = FileResourceLoader::DefaultAudioIOStrategy;
    if (strategy == S::Automatic)
        return "Automatic";
    else if (strategy == S::Memmap)
        return "Memory Mapping";
    else if (strategy == S::Stream)
        return "File Streaming";
    else if (strategy == S::Buffer)
        return "Memory Buffering";
    else BUG("Missing translation");
    return "???";
}

} // namespace

namespace game {
std::string TranslateEnum(AnimatorClass::Type type)
{
    using T = AnimatorClass::Type;
    if (type == T::TransformAnimator)
        return "Transform Animator";
    else if (type == T::KinematicAnimator)
        return "Kinematic Animator";
    else if (type == T::PropertyAnimator)
        return "Property Animator";
    else if (type == T::BooleanPropertyAnimator)
        return "Property Animator (Boolean)";
    else if (type == T::MaterialAnimator)
        return "Material Animator";
    else BUG("Missing translation");
    return "???";
}
std::string TranslateEnum(PropertyAnimatorClass::PropertyName name)
{
    using P = PropertyAnimatorClass::PropertyName;
    if (name == P::Drawable_TimeScale)
        return "Drawable Time Scale";
    else if (name == P::Drawable_RotationX)
        return "Drawable Rotation Around X Axis";
    else if (name == P::Drawable_RotationY)
        return "Drawable Rotation Around Y Axis";
    else if (name == P::Drawable_RotationZ)
        return "Drawable Rotation Around Z Axis";
    else if (name == P::Drawable_TranslationX)
        return "Drawable Translation Along X Axis";
    else if (name == P::Drawable_TranslationY)
        return "Drawable Translation Along Y Axis";
    else if (name == P::Drawable_TranslationZ)
        return "Drawable Translation Along Z Axis";
    else if (name == P::Drawable_SizeZ)
        return "Drawable Size in Z Dimension";
    else if (name == P::RigidBody_LinearVelocityX)
        return "Rigid Body Linear Velocity on X Axis";
    else if (name == P::RigidBody_LinearVelocityY)
        return "Rigid Body Linear Velocity on Y Axis";
    else if (name == P::RigidBody_LinearVelocity)
        return "Rigid Body Linear Velocity";
    else if (name == P::RigidBody_AngularVelocity)
        return "Rigid Body Angular Velocity";
    else if (name == P::TextItem_Text)
        return "Text";
    else if (name == P::TextItem_Color)
        return "Text Color";
    else if (name == P::Transformer_LinearVelocity)
        return "Transformer Velocity";
    else if (name == P::Transformer_LinearVelocityX)
        return "Transformer Velocity on X Axis";
    else if (name == P::Transformer_LinearVelocityY)
        return "Transformer Velocity on Y Axis";
    else if (name == P::Transformer_LinearAcceleration)
        return "Transformer Acceleration";
    else if (name == P::Transformer_LinearAccelerationX)
        return "Transformer Acceleration on X Axis";
    else if (name == P::Transformer_LinearAccelerationY)
        return "Transformer Acceleration on Y Axis";
    else if (name == P::Transformer_AngularVelocity)
        return "Transformer Angular Velocity";
    else if (name == P::Transformer_AngularAcceleration)
        return "Transformer Angular Acceleration";
    else if (name == P::RigidBodyJoint_MotorTorque)
        return "Rigid Body Joint Motor Torque";
    else if (name == P::RigidBodyJoint_MotorSpeed)
        return "Rigid Body Joint Motor Speed";
    else if (name == P::RigidBodyJoint_MotorForce)
        return "Rigid Body Joint Motor Force";
    else if (name == P::RigidBodyJoint_Damping)
        return "Rigid Body Joint Damping";
    else if (name == P::RigidBodyJoint_Stiffness)
        return "Rigid Body Joint Stiffness";
    else BUG("Missing translation");
    return "???";
}
std::string TranslateEnum(BooleanPropertyAnimatorClass::PropertyName name)
{
    using P = BooleanPropertyAnimatorClass::PropertyName;
    if (name == P::Drawable_VisibleInGame)
        return "Set Drawable Visible";
    else if (name == P::Drawable_UpdateMaterial)
        return "Update Drawable Material";
    else if (name == P::Drawable_UpdateDrawable)
        return "Update Drawable Shape";
    else if (name == P::Drawable_Restart)
        return "Set Drawable Restarting";
    else if (name == P::Drawable_FlipHorizontally)
        return "Set Drawable Horizontal Flip";
    else if (name == P::Drawable_FlipVertically)
        return "Set Drawable Vertical Flip";
    else if (name == P::Drawable_DoubleSided)
        return "Set Drawable Double Sided";
    else if (name == P::Drawable_DepthTest)
        return "Enable Drawable Depth Test";
    else if (name == P::Drawable_PPEnableBloom)
        return "Enable Bloom on Drawable";
    else if (name == P::RigidBody_Bullet)
        return "Set Rigid Body Bullet";
    else if (name == P::RigidBody_Sensor)
        return "Set Rigid Body Sensor";
    else if (name == P::RigidBody_Enabled)
        return "Enable Rigid Body Simulation";
    else if (name == P::RigidBody_CanSleep)
        return "Enable Rigid Body Sleeping";
    else if (name == P::RigidBody_DiscardRotation)
        return "Set Rigid Body Rotation Discard";
    else if (name == P::TextItem_VisibleInGame)
        return "Set Text Visible";
    else if (name == P::TextItem_Blink)
        return "Set Text Blinking";
    else if (name == P::TextItem_Underline)
        return "Set Text Underlining";
    else if (name == P::TextItem_PPEnableBloom)
        return "Enable Bloom on Text";
    else if (name == P::SpatialNode_Enabled)
        return "Enable Spatial Node";
    else if (name == P::Transformer_Enabled)
        return "Enable Node Transformer";
    else if (name == P::RigidBodyJoint_EnableLimits)
        return "Enable Joint Limits";
    else if (name == P::RigidBodyJoint_EnableMotor)
        return "Enable Joint Motor";
    else BUG("Missing translation");
    return "???";
}

std::string TranslateEnum(KinematicAnimatorClass::Target target)
{
    if (target == KinematicAnimatorClass::Target::RigidBody)
        return "Rigid Body";
    else if (target == KinematicAnimatorClass::Target::Transformer)
        return "Node Transformer";
    else BUG("Missing translation");
    return "???";
}

std::string TranslateEnum(EntityStateControllerClass::StateTransitionMode mode)
{
    using M = EntityStateControllerClass::StateTransitionMode;
    if (mode == M::Continuous)
        return "Evaluate Continuously";
    else if (mode == M::OnTrigger)
        return "Evaluate on Request Only";
    else BUG("Missing translation");
    return "???";
}

} // namespace

namespace math
{
std::string TranslateEnum(Interpolation i)
{
    using I = Interpolation;

    if (i == I::StepStart)
        return "Step at Start";
    else if (i == I::Step)
        return "Step at Middle";
    else if (i == I::StepEnd)
        return "Step at End";
    else if (i == I::Linear)
        return "Linear";
    else if (i == I::Cosine)
        return "Cosine";
    else if (i == I::SmoothStep)
        return "Smooth Step";
    else if (i == I::Acceleration)
        return "Acceleration";
    else if (i == I::Deceleration)
        return "Deceleration";
    else if (i == I::EaseInSine)
        return "Ease in Sine";
    else if (i == I::EaseOutSine)
        return "Ease out Sine";
    else if (i == I::EaseInOutSine)
        return "Ease in out Sine";
    else if (i == I::EaseInQuadratic)
        return "Ease in Quadratic";
    else if (i == I::EaseOutQuadratic)
        return "Ease out Quadratic";
    else if (i == I::EaseInOutQuadratic)
        return "Ease in out Quadratic";
    else if (i == I::EaseInCubic)
        return "Ease in Cubic";
    else if (i == I::EaseOutCubic)
        return "Ease out Cubic";
    else if (i == I::EaseInOutCubic)
        return "Ease in out Cubic";
    else if (i == I::EaseInBack)
        return "Ease in Back";
    else if (i == I::EaseOutBack)
        return "Ease out Back";
    else if (i == I::EaseInOutBack)
        return "Ease in out Back";
    else if (i == I::EaseInElastic)
        return "Ease in Elastic";
    else if (i == I::EaseOutElastic)
        return "Ease out Elastic";
    else if (i == I::EaseInOutElastic)
        return "Ease in out Elastic";
    else if (i == I::EaseInBounce)
        return "Ease in Bounce";
    else if (i == I::EaseOutBounce)
        return "Ease out Bounce";
    else if (i == I::EaseInOutBounce)
        return "Ease in out Bounce";
    else BUG("Missing translation");
    return "???";
}
} // namespace

namespace gfx
{
std::string TranslateEnum(gfx::ParticleEngineClass::CoordinateSpace space)
{
    using S = gfx::ParticleEngineClass::CoordinateSpace;
    if (space == S::Global)
        return "Global Coordinate Space";
    else if (space == S::Local)
        return "Local Coordinate Space";
    else BUG("Missing translation");
    return "???";
}
std::string TranslateEnum(gfx::ParticleEngineClass::Motion motion)
{
    using M = gfx::ParticleEngineClass::Motion;
    if (motion == M::Linear)
        return "Kinematic Motion";
    else if (motion == M::Projectile)
        return "Projectile Motion";
    else BUG("Missing translation");
    return "???";
}

std::string TranslateEnum(gfx::ParticleEngineClass::SpawnPolicy spawn)
{
    using S = gfx::ParticleEngineClass::SpawnPolicy;
    if (spawn == S::Once)
        return "Spawn Once";
    else if (spawn == S::Continuous)
        return "Spawn Continuously";
    else if (spawn == S::Maintain)
        return "Maintain Particle Count";
    else if (spawn == S::Command)
        return "Spawn On Command Only";
    else BUG("Missing translation");
    return "???";
}

} // namespace