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

std::string TranslateEnum(SceneClass::RenderingArgs::ShadingMode mode)
{
    using M = SceneClass::RenderingArgs::ShadingMode;
    if (mode == M::Flat)
        return "Flat Color";
    else if (mode == M::BasicLight)
        return "Basic Light";
    else BUG("Bug on translation");
    return "???";
}
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
        return "Text String";
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
    else if (name == P::BasicLight_Direction)
        return "Basic Light Direction";
    else if (name == P::BasicLight_Translation)
        return "Basic Light Translation";
    else if (name == P::BasicLight_AmbientColor)
        return "Basic Light Ambient Color";
    else if (name == P::BasicLight_DiffuseColor)
        return "Basic Light Diffuse Color";
    else if (name == P::BasicLight_SpecularColor)
        return "Basic Light Specular Color";
    else if (name == P::BasicLight_SpotHalfAngle)
        return "Basic Light Spot Half-Angle";
    else if (name == P::BasicLight_LinearAttenuation)
        return "Basic Light Linear Attenuation";
    else if (name == P::BasicLight_ConstantAttenuation)
        return "Basic Light Constant Attenuation";
    else if (name == P::BasicLight_QuadraticAttenuation)
        return "Basic Light Quadratic Attenuation";
    else BUG("Missing translation");
    return "???";
}
std::string TranslateEnum(BooleanPropertyAnimatorClass::PropertyName name)
{
    using P = BooleanPropertyAnimatorClass::PropertyName;
    if (name == P::Drawable_VisibleInGame)
        return "Set Drawable Visibility in Game";
    else if (name == P::Drawable_UpdateMaterial)
        return "Set Drawable Material Update";
    else if (name == P::Drawable_UpdateDrawable)
        return "Set Drawable Shape Update";
    else if (name == P::Drawable_Restart)
        return "Set Drawable Restart";
    else if (name == P::Drawable_FlipHorizontally)
        return "Enable Horizontal Flip for Drawable";
    else if (name == P::Drawable_FlipVertically)
        return "Enable Vertical Flip for Drawable";
    else if (name == P::Drawable_DoubleSided)
        return "Enable Double-Sided Drawable";
    else if (name == P::Drawable_DepthTest)
        return "Enable Depth Test for Drawable";
    else if (name == P::Drawable_PPEnableBloom)
        return "Enable Bloom for Drawable";
    else if (name == P::RigidBody_Bullet)
        return "Enable Rigid Body as Bullet";
    else if (name == P::RigidBody_Sensor)
        return "Enable Rigid Body as Sensor";
    else if (name == P::RigidBody_Enabled)
        return "Enable Rigid Body Simulation";
    else if (name == P::RigidBody_CanSleep)
        return "Enable Rigid Body Sleeping";
    else if (name == P::RigidBody_DiscardRotation)
        return "Enable Rigid Body Rotation Discard";
    else if (name == P::TextItem_VisibleInGame)
        return "Set Text Visibility in Game";
    else if (name == P::TextItem_Blink)
        return "Enable Text Blinking";
    else if (name == P::TextItem_Underline)
        return "Enable Text Underlining";
    else if (name == P::TextItem_PPEnableBloom)
        return "Enable Bloom for Text";
    else if (name == P::SpatialNode_Enabled)
        return "Enable Spatial Node";
    else if (name == P::Transformer_Enabled)
        return "Enable Transformer";
    else if (name == P::RigidBodyJoint_EnableLimits)
        return "Enable Joint Limits";
    else if (name == P::RigidBodyJoint_EnableMotor)
        return "Enable Joint Motor";
    else if (name == P::BasicLight_Enabled)
        return "Enable Basic Light";
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

std::string TranslateEnum(BasicLightClass::LightType light)
{
    using L = BasicLightClass::LightType;
    if (light == L::Ambient)
        return "Ambient";
    else if (light == L::Directional)
        return "Directional";
    else if (light == L::Spot)
        return "Spot";
    else if (light == L::Point)
        return "Point";
    else BUG("Missing translation.");
    return "???";
}

std::string TranslateEnum(RenderPass pass)
{
    using P = RenderPass;
    if (pass == P::MaskExpose)
        return "Stencil Mask Expose";
    else if (pass == P::MaskCover)
        return "Stencil Mask Cover";
    else if (pass == P::DrawColor)
        return "Color Buffer Write";
    else BUG("Missing translation");
    return "???";
}
std::string TranslateEnum(CoordinateSpace space)
{
    using S = CoordinateSpace;
    if (space == S::Scene)
        return "Scene Space";
    else if (space == S::Camera)
        return "Camera Space";
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

std::string TranslateEnum(gfx::MaterialClass::GradientType gradient)
{
    using T = gfx::MaterialClass::GradientType;
    if (gradient == T::Bilinear)
        return "Bilinear Gradient";
    else if (gradient == T::Radial)
        return "Radial Gradient";
    else if (gradient == T::Conical)
        return "Conical Gradient";
    else BUG("Missing translation");
    return "???";
}

} // namespace

namespace audio
{
std::string TranslateEnum(audio::SampleType sampletype)
{
    if (sampletype == audio::SampleType::Float32)
        return "Float 32bits";
    else if (sampletype == audio::SampleType::Int16)
        return "Signed Int 16bit";
    else if (sampletype == audio::SampleType::Int32)
        return "Signed Int 32bit";
    else BUG("Missing translation");
    return "???";
}
std::string TranslateEnum(audio::IOStrategy io)
{
    if (io == audio::IOStrategy::Stream)
        return "File Stream";
    else if (io == audio::IOStrategy::Automatic)
        return "Automatic";
    else if (io == audio::IOStrategy::Default)
        return "Project Default";
    else if (io == audio::IOStrategy::Buffer)
        return "Memory Buffer";
    else if (io == audio::IOStrategy::Memmap)
        return "Memory Map";
    else BUG("Missing translation");
    return "???";
}

} // audio


namespace engine
{

std::string TranslateEnum(engine::RenderingStyle style)
{
    if (style == engine::RenderingStyle::FlatColor)
        return "Flat Color";
    else if (style == engine::RenderingStyle::BasicShading)
        return "Basic Shading";
    else if (style == engine::RenderingStyle::Wireframe)
        return "Wireframe";
    BUG("Missing translation");
    return "???";
}

} // namespace engine