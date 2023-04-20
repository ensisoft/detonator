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
#  include <glm/mat4x4.hpp>
#  include <glm/vec2.hpp>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <optional>
#include <variant>

#include "base/bitflag.h"
#include "base/utility.h"
#include "base/math.h"
#include "base/hash.h"
#include "data/fwd.h"
#include "game/tree.h"
#include "game/types.h"
#include "game/enum.h"
#include "game/animation.h"
#include "game/color.h"
#include "game/scriptvar.h"

namespace game
{
    class Scene;

    namespace detail {
        // Selection for collision shapes when the collision shape detection
        // is set to manual.
        enum class CollisionShape {
            // The collision shape is a box based on the size of node's box.
            Box,
            // The collision shape is a circle based on the largest extent of
            // the node's box.
            Circle,
            // The collision shape is a right-angled triangle where the
            // height of the triangle is the height of the box and the
            // width is the width of the node's box
            RightTriangle,
            // Isosceles triangle
            IsoscelesTriangle,
            // Trapezoid
            Trapezoid,
            //
            Parallelogram,
            // The collision shape is the upper half of a circle.
            SemiCircle,
            // The collision shape is a convex polygon. The polygon shape id
            // must then be selected in order to be able to extract the
            // polygon's convex hull.
            Polygon
        };
    } // namespace

    class SpatialNodeClass
    {
    public:
        enum class Shape {
            AABB
        };
        enum class Flags {
            Enabled,
            ReportOverlap
        };
        SpatialNodeClass();
        std::size_t GetHash() const;
        Shape GetShape() const noexcept
        { return mShape; }
        void SetShape(Shape shape) noexcept
        { mShape = shape; }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }
        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        base::bitflag<Flags> GetFlags() const noexcept
        { return mFlags; }

        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);
    private:
        Shape mShape = Shape::AABB;
        base::bitflag<Flags> mFlags;
    };

    class FixtureClass
    {
    public:
        using CollisionShape = detail::CollisionShape;

        enum class Flags {
            // Sensor only flag enables this fixture to only be
            // used as a sensor, i.e. it doesn't affect the body's
            // simulation under forces etc.
            Sensor
        };
        FixtureClass() noexcept
        {
            mBitFlags.set(Flags::Sensor, true);
        }
        void SetPolygonShapeId(const std::string& id)
        { mPolygonShapeId = id; }
        void SetRigidBodyNodeId(const std::string& id)
        { mRigidBodyNodeId = id; }
        void SetCollisionShape(CollisionShape shape) noexcept
        { mCollisionShape = shape; }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mBitFlags.set(flag, on_off); }
        void SetFriction(float value) noexcept
        { mFriction = value; }
        void SetDensity(float value) noexcept
        { mDensity = value; }
        void SetRestitution(float value) noexcept
        { mRestitution = value; }
        bool TestFlag(Flags flag) const noexcept
        { return mBitFlags.test(flag); }
        bool HasFriction() const noexcept
        { return mFriction.has_value(); }
        bool HasDensity() const noexcept
        { return mDensity.has_value(); }
        bool HasRestitution() const noexcept
        { return mRestitution.has_value(); }
        bool HasPolygonShapeId() const noexcept
        { return !mPolygonShapeId.empty(); }
        const float* GetFriction() const noexcept
        { return base::GetOpt(mFriction); }
        const float* GetDensity() const noexcept
        { return base::GetOpt(mDensity); }
        const float* GetRestitution() const noexcept
        { return base::GetOpt(mRestitution); }
        base::bitflag<Flags> GetFlags() const noexcept
        { return mBitFlags; }
        CollisionShape GetCollisionShape() const noexcept
        { return mCollisionShape; }
        const std::string& GetPolygonShapeId() const noexcept
        { return mPolygonShapeId; }
        const std::string& GetRigidBodyNodeId() const noexcept
        {return mRigidBodyNodeId; }
        void ResetRigidBodyId() noexcept
        { mRigidBodyNodeId.clear(); }
        void ResetPolygonShapeId() noexcept
        { mPolygonShapeId.clear(); }
        void ResetFriction() noexcept
        { mFriction.reset(); }
        void ResetDensity() noexcept
        { mDensity.reset(); }
        void ResetRestitution() noexcept
        { mRestitution.reset(); }

        std::size_t GetHash() const;

        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);
    private:
        CollisionShape  mCollisionShape = CollisionShape::Box;
        base::bitflag<Flags> mBitFlags;
        // The ID of the custom polygon shape id. Only used when the
        // collision shape is set to Polygon.
        std::string mPolygonShapeId;
        // The ID of the node that has the rigid body to which this
        // fixture is attached to. If
        std::string mRigidBodyNodeId;
        // Fixture specific friction parameter. if not set then
        // the friction from the associated rigid body is used.
        std::optional<float> mFriction;
        // Fixture specific density parameter. if not set then
        // the density from the associated rigid body is used.
        std::optional<float> mDensity;
        // Fixture specific restitution parameter. If not set then
        // the restitution from the associated rigid body is used.
        std::optional<float> mRestitution;
    };


    class RigidBodyItemClass
    {
    public:
        using CollisionShape = detail::CollisionShape;
        // Simulation parameter determines the type of physics
        // simulation (or the lack of simulation) applied to the
        // rigid body by the physics engine.
        enum class Simulation {
            // Static bodies remain static in the physics simulation.
            // i.e the body exists in the physics world but no forces
            // are applied onto it.
            Static,
            // Kinematic bodies are driven by simple kinematic motion
            // i.e. by velocity of the body. No forces are applied to it.
            Kinematic,
            // Dynamic body is completely driven by the physics simulation.
            // I.e.the body is moved by the physical forces being applied to it.
            Dynamic
        };

        enum class Flags {
            // Enable bullet physics, i.e. expect the object to be
            // fast moving object. this will increase the computational
            // effort required but will mitigate issues with fast traveling
            // objects.
            Bullet,
            // Sensor only flag enables object to only be used to
            // report collisions.
            Sensor,
            // Whether the rigid body simulation is enabled or not
            // for this body.
            Enabled,
            // Whether the rigid body can go to sleep (i.e. simulation stops)
            // when the body comes to a halt
            CanSleep,
            // Discard rotational component of physics simulation for this body.
            // Useful for things such as player character that should stay "upright
            DiscardRotation
        };
        RigidBodyItemClass() noexcept
        {
            mBitFlags.set(Flags::Enabled, true);
            mBitFlags.set(Flags::CanSleep, true);
        }

        std::size_t GetHash() const;

        Simulation GetSimulation() const noexcept
        { return mSimulation; }
        CollisionShape GetCollisionShape() const noexcept
        { return mCollisionShape; }
        float GetFriction() const noexcept
        { return mFriction; }
        float GetRestitution() const noexcept
        { return mRestitution; }
        float GetAngularDamping() const noexcept
        { return mAngularDamping; }
        float GetLinearDamping() const noexcept
        { return mLinearDamping; }
        float GetDensity() const noexcept
        { return mDensity; }
        bool TestFlag(Flags flag) const noexcept
        { return mBitFlags.test(flag); }
        const std::string& GetPolygonShapeId() const noexcept
        { return mPolygonShapeId; }
        void ResetPolygonShapeId() noexcept
        { mPolygonShapeId.clear(); }
        base::bitflag<Flags> GetFlags() const noexcept
        { return mBitFlags; }

        void SetCollisionShape(CollisionShape shape) noexcept
        { mCollisionShape = shape; }
        void SetSimulation(Simulation simulation) noexcept
        { mSimulation = simulation; }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mBitFlags.set(flag, on_off); }
        void SetFriction(float value) noexcept
        { mFriction = value; }
        void SetRestitution(float value) noexcept
        { mRestitution = value; }
        void SetAngularDamping(float value) noexcept
        { mAngularDamping = value; }
        void SetLinearDamping(float value) noexcept
        { mLinearDamping = value; }
        void SetDensity(float value) noexcept
        { mDensity = value; }
        void SetPolygonShapeId(const std::string& id)
        { mPolygonShapeId = id; }

        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);
    private:
        Simulation mSimulation = Simulation::Dynamic;
        CollisionShape mCollisionShape = CollisionShape::Box;
        base::bitflag<Flags> mBitFlags;
        std::string mPolygonShapeId;
        float mFriction = 0.3f;
        float mRestitution = 0.5f;
        float mAngularDamping = 0.5f;
        float mLinearDamping = 0.5f;
        float mDensity = 1.0f;
    };

    // Drawable item defines a drawable item and its material and
    // properties that affect the rendering of the entity node
    class DrawableItemClass
    {
    public:
        // Variant of material params that can be set
        // on this drawable item.
        using MaterialParam = std::variant<float, int,
                Color4f,
                glm::vec2, glm::vec3, glm::vec4>;
        // Key-value map of material params.
        using MaterialParamMap = std::unordered_map<std::string, MaterialParam>;
        using RenderPass  = game::RenderPass;
        using RenderStyle = game::RenderStyle;
        enum class Flags {
            // Whether the item is currently visible or not.
            VisibleInGame,
            // Whether the item should update material or not
            UpdateMaterial,
            // Whether the item should update drawable or not
            UpdateDrawable,
            // Whether the item should restart drawables that have
            // finished, for example particle engines.
            RestartDrawable,
            // Whether to flip (mirror) the item about the central vertical axis.
            // This changes the rendering on the direction from left to right.
            FlipHorizontally,
            // Whether to flip (mirror) the item about the central horizontal axis.
            // This changes the rendering on the direction from top to bottom.
            FlipVertically,
            // Contribute to bloom post-processing effect.
            PP_EnableBloom
        };
        DrawableItemClass();

        std::size_t GetHash() const;

        // class setters.
        void SetDrawableId(const std::string& klass)
        { mDrawableId = klass; }
        void SetMaterialId(const std::string& klass)
        { mMaterialId = klass; }
        void SetLayer(int layer) noexcept
        { mLayer = layer; }
        void ResetMaterial() noexcept
        {
            mMaterialId.clear();
            mMaterialParams.clear();
        }
        void ResetDrawable() noexcept
        { mDrawableId.clear(); }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mBitFlags.set(flag, on_off); }
        void SetLineWidth(float width) noexcept
        { mLineWidth = width; }
        void SetRenderPass(RenderPass pass) noexcept
        { mRenderPass = pass; }
        void SetRenderStyle(RenderStyle style) noexcept
        { mRenderStyle = style; }
        void SetTimeScale(float scale) noexcept
        { mTimeScale = scale; }
        void SetMaterialParam(const std::string& name, const MaterialParam& value)
        { mMaterialParams[name] = value; }
        void SetMaterialParams(const MaterialParamMap& params)
        { mMaterialParams = params; }
        void SetMaterialParams(MaterialParamMap&& params)
        { mMaterialParams = std::move(params); }

        // class getters.
        const std::string& GetDrawableId() const noexcept
        { return mDrawableId; }
        const std::string& GetMaterialId() const noexcept
        { return mMaterialId; }
        int GetLayer() const noexcept
        { return mLayer; }
        float GetLineWidth() const noexcept
        { return mLineWidth; }
        float GetTimeScale() const noexcept
        { return mTimeScale; }
        bool TestFlag(Flags flag) const noexcept
        { return mBitFlags.test(flag); }
        RenderPass GetRenderPass() const noexcept
        { return mRenderPass; }
        RenderStyle GetRenderStyle() const noexcept
        { return mRenderStyle; }
        base::bitflag<Flags> GetFlags() const noexcept
        { return mBitFlags; }
        MaterialParamMap GetMaterialParams() noexcept
        { return mMaterialParams; }
        const MaterialParamMap& GetMaterialParams() const noexcept
        { return mMaterialParams; }
        bool HasMaterialParam(const std::string& name) const noexcept
        { return base::SafeFind(mMaterialParams, name) != nullptr; }
        MaterialParam* FindMaterialParam(const std::string& name) noexcept
        { return base::SafeFind(mMaterialParams, name); }
        const MaterialParam* FindMaterialParam(const std::string& name) const noexcept
        { return base::SafeFind(mMaterialParams, name); }
        template<typename T>
        T* GetMaterialParamValue(const std::string& name) noexcept
        {
            if (auto* ptr = base::SafeFind(mMaterialParams, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        template<typename T>
        const T* GetMaterialParamValue(const std::string& name) const noexcept
        {
            if (auto* ptr = base::SafeFind(mMaterialParams, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        void DeleteMaterialParam(const std::string& name) noexcept
        { mMaterialParams.erase(name); }
        void ClearMaterialParams() noexcept
        { mMaterialParams.clear(); }
        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);
    private:
        // item's bit flags.
        base::bitflag<Flags> mBitFlags;
        // class id of the material.
        std::string mMaterialId;
        // class id of the drawable shape.
        std::string mDrawableId;
        // the layer in which this node should be drawn.
        int mLayer = 0;
        // linewidth for rasterizing the shape with lines.
        float mLineWidth = 1.0f;
        // scaler value for changing the time delta values
        // applied to the drawable (material)
        float mTimeScale = 1.0f;
        RenderPass mRenderPass = RenderPass::DrawColor;
        RenderStyle  mRenderStyle = RenderStyle::Solid;
        MaterialParamMap mMaterialParams;
    };

    // TextItem allows human-readable text entity node attachment with
    // some simple properties that defined how the text should look.
    class TextItemClass
    {
    public:
        // How to align the text inside the node vertically.
        enum class HorizontalTextAlign {
            // Align to the node's left edge.
            Left,
            // Align around center of the node.
            Center,
            // Align to the node's right edge.
            Right
        };
        // How to align the text inside the node vertically.
        enum class VerticalTextAlign {
            // Align to the top of the node.
            Top,
            // Align around the center of the node.
            Center,
            // Align to the bottom of the node.
            Bottom
        };
        enum class Flags {
            // Whether the item is currently visible or not.
            VisibleInGame,
            // Make the text blink annoyingly
            BlinkText,
            // Set text to underline
            UnderlineText,
            // Static content, i.e. the text/color/ etc. properties
            // are not expected to change.
            StaticContent,
            // Contribute to bloom post-processing effect.
            PP_EnableBloom
        };
        TextItemClass()
        {
            mBitFlags.set(Flags::VisibleInGame, true);
            mBitFlags.set(Flags::PP_EnableBloom, true);
        }
        std::size_t GetHash() const;

        // class setters
        void SetText(const std::string& text)
        { mText = text; }
        void SetText(std::string&& text) noexcept
        { mText = std::move(text); }
        void SetFontName(const std::string& font)
        { mFontName = font; }
        void SetFontSize(unsigned size) noexcept
        { mFontSize = size; }
        void SetLayer(int layer) noexcept
        { mLayer = layer; }
        void SetLineHeight(float height) noexcept
        { mLineHeight = height; }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mBitFlags.set(flag, on_off); }
        void SetAlign(VerticalTextAlign align) noexcept
        { mVAlign = align; }
        void SetAlign(HorizontalTextAlign align) noexcept
        { mHAlign = align; }
        void SetTextColor(const Color4f& color) noexcept
        { mTextColor = color; }
        void SetRasterWidth(unsigned width) noexcept
        { mRasterWidth = width;}
        void SetRasterHeight(unsigned height) noexcept
        { mRasterHeight = height; }

        // class getters
        bool TestFlag(Flags flag) const noexcept
        { return mBitFlags.test(flag); }
        bool IsStatic() const noexcept
        { return TestFlag(Flags::StaticContent); }
        const Color4f& GetTextColor() const noexcept
        { return mTextColor; }
        const std::string& GetText() const noexcept
        { return mText; }
        const std::string& GetFontName() const noexcept
        { return mFontName; }
        int GetLayer() const noexcept
        { return mLayer; }
        float GetLineHeight() const noexcept
        { return mLineHeight; }
        unsigned GetFontSize() const noexcept
        { return mFontSize; }
        unsigned GetRasterWidth() const noexcept
        { return mRasterWidth; }
        unsigned GetRasterHeight() const noexcept
        { return mRasterHeight; }
        base::bitflag<Flags> GetFlags() const noexcept
        { return mBitFlags; }
        HorizontalTextAlign GetHAlign() const noexcept
        { return mHAlign; }
        VerticalTextAlign GetVAlign() const noexcept
        { return mVAlign; }

        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);
    private:
        // item's bit flags
        base::bitflag<Flags> mBitFlags;
        HorizontalTextAlign mHAlign = HorizontalTextAlign::Center;
        VerticalTextAlign mVAlign = VerticalTextAlign::Center;
        int mLayer = 0;
        std::string mText;
        std::string mFontName;
        unsigned mFontSize = 0;
        unsigned mRasterWidth = 0;
        unsigned mRasterHeight = 0;
        float mLineHeight = 1.0f;
        Color4f mTextColor = Color::White;
    };


    class DrawableItem
    {
    public:
        using MaterialParam    = DrawableItemClass::MaterialParam;
        using MaterialParamMap = DrawableItemClass::MaterialParamMap;
        using Flags       = DrawableItemClass::Flags;
        using RenderPass  = DrawableItemClass::RenderPass;
        using RenderStyle = DrawableItemClass::RenderStyle ;

        DrawableItem(std::shared_ptr<const DrawableItemClass> klass)
          : mClass(klass)
          , mInstanceFlags(mClass->GetFlags())
          , mMaterialParams(mClass->GetMaterialParams())
        {}
        const std::string& GetMaterialId() const noexcept
        { return mClass->GetMaterialId(); }
        const std::string& GetDrawableId() const noexcept
        { return mClass->GetDrawableId(); }
        int GetLayer() const noexcept
        { return mClass->GetLayer(); }
        float GetLineWidth() const noexcept
        { return mClass->GetLineWidth(); }
        RenderPass GetRenderPass() const noexcept
        { return mClass->GetRenderPass(); }
        RenderStyle GetRenderStyle() const noexcept
        { return mClass->GetRenderStyle(); }
        bool TestFlag(Flags flag) const noexcept
        { return mInstanceFlags.test(flag); }
        bool IsVisible() const noexcept
        { return mInstanceFlags.test(Flags::VisibleInGame); }
        void SetVisible(bool visible) noexcept
        { mInstanceFlags.set(Flags::VisibleInGame, visible); }
        float GetTimeScale() const noexcept
        { return mInstanceTimeScale; }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mInstanceFlags.set(flag, on_off); }
        void SetTimeScale(float scale) noexcept
        { mInstanceTimeScale = scale; }
        void SetMaterialParam(const std::string& name, const MaterialParam& value)
        { mMaterialParams[name] = value; }
        MaterialParamMap GetMaterialParams()
        { return mMaterialParams; }
        const MaterialParamMap& GetMaterialParams() const noexcept
        { return mMaterialParams; }
        bool HasMaterialParam(const std::string& name) const noexcept
        { return base::SafeFind(mMaterialParams, name) != nullptr; }
        MaterialParam* FindMaterialParam(const std::string& name) noexcept
        { return base::SafeFind(mMaterialParams, name); }
        const MaterialParam* FindMaterialParam(const std::string& name) const noexcept
        { return base::SafeFind(mMaterialParams, name); }
        template<typename T>
        T* GetMaterialParamValue(const std::string& name) noexcept
        {
            if (auto* ptr = base::SafeFind(mMaterialParams, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        template<typename T>
        const T* GetMaterialParamValue(const std::string& name) const noexcept
        {
            if (auto* ptr = base::SafeFind(mMaterialParams, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        void DeleteMaterialParam(const std::string& name) noexcept
        { mMaterialParams.erase(name); }

        const DrawableItemClass& GetClass() const noexcept
        { return *mClass.get(); }
        const DrawableItemClass* operator->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const DrawableItemClass> mClass;
        base::bitflag<Flags> mInstanceFlags;
        float mInstanceTimeScale = 1.0f;
        MaterialParamMap mMaterialParams;
    };

    class Fixture
    {
    public:
        using Flags = FixtureClass::Flags;
        using CollisionShape = FixtureClass::CollisionShape;
        Fixture(std::shared_ptr<const FixtureClass> klass)
          : mClass(klass)
        {}
        const std::string& GetPolygonShapeId() const noexcept
        { return mClass->GetPolygonShapeId(); }
        const std::string& GetRigidBodyNodeId() const noexcept
        { return mClass->GetRigidBodyNodeId(); }
        const float* GetFriction() const noexcept
        { return mClass->GetFriction(); }
        const float* GetDensity() const noexcept
        { return mClass->GetDensity(); }
        const float* GetRestitution() const noexcept
        { return mClass->GetRestitution(); }
        base::bitflag<Flags> GetFlags() const noexcept
        { return mClass->GetFlags(); }
        CollisionShape GetCollisionShape() const noexcept
        { return mClass->GetCollisionShape(); }
        bool TestFlag(Flags flag) const noexcept
        { return mClass->TestFlag(flag); }
        const FixtureClass& GetClass() const noexcept
        { return *mClass; }
        const FixtureClass* operator->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const FixtureClass> mClass;

    };

    class RigidBodyItem
    {
    public:
        using Simulation = RigidBodyItemClass::Simulation;
        using Flags      = RigidBodyItemClass::Flags;
        using CollisionShape = RigidBodyItemClass::CollisionShape;

        RigidBodyItem(std::shared_ptr<const RigidBodyItemClass> klass)
          : mClass(klass)
          , mInstanceFlags(mClass->GetFlags())
        {}

        Simulation GetSimulation() const noexcept
        { return mClass->GetSimulation(); }
        CollisionShape GetCollisionShape() const noexcept
        { return mClass->GetCollisionShape(); }
        float GetFriction() const noexcept
        { return mClass->GetFriction(); }
        float GetRestitution() const noexcept
        { return mClass->GetRestitution(); }
        float GetAngularDamping() const noexcept
        { return mClass->GetAngularDamping(); }
        float GetLinearDamping() const noexcept
        { return mClass->GetLinearDamping(); }
        float GetDensity() const noexcept
        { return mClass->GetDensity(); }
        bool TestFlag(Flags flag) const noexcept
        { return mInstanceFlags.test(flag); }
        const std::string& GetPolygonShapeId() const noexcept
        { return mClass->GetPolygonShapeId(); }
        base::bitflag<Flags> GetFlags() const noexcept
        { return mInstanceFlags; }
        // Get the current instantaneous linear velocity of the
        // rigid body under the physics simulation.
        // The linear velocity expresses how fast the object is
        // moving and to which direction. It's expressed in meters per second
        // using the physics world coordinate space.
        const glm::vec2& GetLinearVelocity() const noexcept
        { return mLinearVelocity; }
        // Get the current instantaneous angular velocity of the
        // rigid body under the physics simulation.
        // The angular velocity expresses how fast the object is rotating
        // around its own center in radians per second.
        float GetAngularVelocity() const noexcept
        { return mAngularVelocity; }
        // Update the current linear velocity in m/s.
        // The updates are coming from the physics engine.
        void SetLinearVelocity(const glm::vec2& velocity) noexcept
        { mLinearVelocity = velocity; }
        // Update the current angular velocity in r/s
        // The updates are coming from the physics engine.
        void SetAngularVelocity(float velocity) noexcept
        { mAngularVelocity = velocity; }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mInstanceFlags.set(flag, on_off); }

        void ApplyLinearImpulseToCenter(const glm::vec2& impulse) noexcept
        { mCenterImpulse = impulse; }

        // Set a new linear velocity adjustment to be applied
        // on the next update of the physics engine. The velocity
        // is in meters per second.
        void AdjustLinearVelocity(const glm::vec2& velocity) noexcept
        { mLinearVelocityAdjustment = velocity; }
        // Set a new angular velocity adjustment to be applied
        // on the next update of the physics engine. The velocity
        // is in radians per second.
        void AdjustAngularVelocity(float radians) noexcept
        { mAngularVelocityAdjustment = radians; }
        bool HasCenterImpulse() const noexcept
        { return mCenterImpulse.has_value(); }
        bool HasLinearVelocityAdjustment() const noexcept
        { return mLinearVelocityAdjustment.has_value(); }
        bool HasAngularVelocityAdjustment() const noexcept
        { return mAngularVelocityAdjustment.has_value(); }
        float GetAngularVelocityAdjustment() const noexcept
        { return mAngularVelocityAdjustment.value_or(0.0f); }
        glm::vec2 GetLinearVelocityAdjustment() const noexcept
        { return mLinearVelocityAdjustment.value_or(glm::vec2(0.0f, 0.0f)); }
        glm::vec2 GetLinearImpulseToCenter() const noexcept
        { return mCenterImpulse.value_or(glm::vec2(0.0f, 0.0f)); };
        void ClearPhysicsAdjustments() const noexcept
        {
            mLinearVelocityAdjustment.reset();
            mAngularVelocityAdjustment.reset();
            mCenterImpulse.reset();
        }
        void Enable(bool value) noexcept
        { SetFlag(Flags::Enabled, value); }
        bool IsEnabled() const noexcept
        { return TestFlag(Flags::Enabled); }
        bool IsSensor() const noexcept
        { return TestFlag(Flags::Sensor); }
        bool IsBullet() const noexcept
        { return TestFlag(Flags::Bullet); }
        bool CanSleep() const noexcept
        { return TestFlag(Flags::CanSleep); }
        bool DiscardRotation() const noexcept
        { return TestFlag(Flags::DiscardRotation); }

        const RigidBodyItemClass& GetClass() const noexcept
        { return *mClass.get(); }
        const RigidBodyItemClass* operator->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const RigidBodyItemClass> mClass;
        // Current linear velocity in meters per second.
        // For dynamically driven bodies
        // the physics engine will update this value, whereas for
        // kinematic bodies the animation system can set this value
        // and the physics engine will read it.
        glm::vec2 mLinearVelocity = {0.0f, 0.0f};
        // Current angular velocity in radians per second.
        // For dynamically driven bodies the physics engine
        // will update this value, whereas for kinematic bodies
        // the animation system can provide a new value which will
        // then be set in the physics engine.
        float mAngularVelocity = 0.0f;
        // Flags specific to this instance.
        base::bitflag<Flags> mInstanceFlags;
        // Current adjustment to be made to the body's linear velocity.
        mutable std::optional<glm::vec2> mLinearVelocityAdjustment;
        // Current adjustment to be made to the body's angular velocity.
        mutable std::optional<float> mAngularVelocityAdjustment;
        // Current pending impulse in the center of the body.
        mutable std::optional<glm::vec2> mCenterImpulse;
    };

    class TextItem
    {
    public:
        using Flags = TextItemClass::Flags;
        using VerticalTextAlign = TextItemClass::VerticalTextAlign;
        using HorizontalTextAlign = TextItemClass::HorizontalTextAlign;
        TextItem(std::shared_ptr<const TextItemClass> klass)
          : mClass(klass)
        {
            mText  = mClass->GetText();
            mFlags = mClass->GetFlags();
            mTextColor = mClass->GetTextColor();
        }

        // instance getters.
        const Color4f& GetTextColor() const noexcept
        { return mTextColor; }
        const std::string& GetText() const noexcept
        { return mText; }
        const std::string& GetFontName() const noexcept
        { return mClass->GetFontName(); }
        unsigned GetFontSize() const noexcept
        { return mClass->GetFontSize(); }
        float GetLineHeight() const noexcept
        { return mClass->GetLineHeight(); }
        int GetLayer() const
        { return mClass->GetLayer(); }
        unsigned GetRasterWidth() const noexcept
        { return mClass->GetRasterWidth(); }
        unsigned GetRasterHeight() const noexcept
        { return mClass->GetRasterHeight(); }
        HorizontalTextAlign GetHAlign() const noexcept
        { return mClass->GetHAlign(); }
        VerticalTextAlign GetVAlign() const noexcept
        { return mClass->GetVAlign(); }
        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        bool IsStatic() const noexcept
        { return TestFlag(Flags::StaticContent); }
        std::size_t GetHash() const noexcept
        {
            size_t hash = 0;
            hash = base::hash_combine(hash, mText);
            hash = base::hash_combine(hash, mTextColor);
            hash = base::hash_combine(hash, mFlags);
            return hash;
        }

        // instance setters.
        void SetText(const std::string& text)
        { mText = text; }
        void SetText(std::string&& text) noexcept
        { mText = std::move(text); }
        void SetTextColor(const Color4f& color) noexcept
        { mTextColor = color; }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }

        // class access
        const TextItemClass& GetClass() const noexcept
        { return *mClass.get(); }
        const TextItemClass* operator->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const TextItemClass> mClass;
        // instance text.
        std::string mText;
        // instance text color
        Color4f mTextColor;
        // instance flags.
        base::bitflag<Flags> mFlags;
    };

    class SpatialNode
    {
    public:
        using Flags = SpatialNodeClass::Flags;
        using Shape = SpatialNodeClass::Shape;
        SpatialNode(std::shared_ptr<const SpatialNodeClass> klass) noexcept
          : mClass(klass)
          , mFlags(klass->GetFlags())
        {}
        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        Shape GetShape() const noexcept
        { return mClass->GetShape(); }
        // class access
        const SpatialNodeClass& GetClass() const noexcept
        { return *mClass; }
        const SpatialNodeClass* operator->() const noexcept
        { return mClass.get(); }
        inline bool IsEnabled() const noexcept
        { return mFlags.test(Flags::Enabled); }
        void Enable(bool value) noexcept
        { mFlags.set(Flags::Enabled, value); }
    private:
        std::shared_ptr<const SpatialNodeClass> mClass;
        base::bitflag<Flags> mFlags;
    };

    class EntityNodeClass
    {
    public:
        using DrawableItemType = DrawableItemClass;

        enum class Flags {
            // Only pertains to editor (todo: maybe this flag should be removed)
            VisibleInEditor
        };

        EntityNodeClass();
        EntityNodeClass(const EntityNodeClass& other);
        EntityNodeClass(EntityNodeClass&& other);

        // Get the class id.
        const std::string& GetId() const noexcept
        { return mClassId; }
        // Get the human-readable name for this class.
        const std::string& GetName() const noexcept
        { return mName; }
        // Get the hash value based on the class object properties.
        std::size_t GetHash() const;

        // Get the node's translation relative to its parent node.
        const glm::vec2& GetTranslation() const noexcept
        { return mPosition; }
        // Get the node's scale factor. The scale factor applies to
        // whole hierarchy of nodes.
        const glm::vec2& GetScale() const noexcept
        { return mScale; }
        // Get the node's box size.
        const glm::vec2& GetSize() const noexcept
        { return mSize;}
        // Get node's rotation relative to its parent node.
        float GetRotation() const noexcept
        { return mRotation; }
        // Set the human-readable node name.
        void SetName(const std::string& name)
        { mName = name; }
        // Set the node's scale. The scale applies to all
        // the subsequent hierarchy, i.e. all the nodes that
        // are in the tree under this node.
        void SetScale(const glm::vec2& scale) noexcept
        { mScale = scale; }
        void SetScale(float sx, float sy) noexcept
        { mScale = glm::vec2(sx, sy); }
        // Set the node's translation relative to the parent
        // of this node.
        void SetTranslation(const glm::vec2& vec) noexcept
        { mPosition = vec; }
        void SetTranslation(float x, float y) noexcept
        { mPosition = glm::vec2(x, y); }
        // Set the node's containing box size.
        // The size is used to for example to figure out
        // the dimensions of rigid body collision shape (if any)
        // and to resize the drawable object.
        void SetSize(const glm::vec2& size) noexcept
        { mSize = size; }
        void SetSize(float width, float height) noexcept
        { mSize = glm::vec2(width, height); }
        // Set the starting rotation in radians around the z axis.
        void SetRotation(float angle) noexcept
        { mRotation = angle;}
        void SetFlag(Flags flag, bool on_off) noexcept
        { mBitFlags.set(flag, on_off); }
        bool TestFlag(Flags flag) const noexcept
        { return mBitFlags.test(flag); }

        // Attach a rigid body to this node class.
        void SetRigidBody(const RigidBodyItemClass& body);
        // Attach a simple static drawable item to this node class.
        void SetDrawable(const DrawableItemClass& drawable);
        // Attach a text item to this node class.
        void SetTextItem(const TextItemClass& text);
        // Attach a spatial index node to this node class.
        void SetSpatialNode(const SpatialNodeClass& node);
        // Attach a rigid body fixture to this node class.
        void SetFixture(const FixtureClass& fixture);
        // Create and attach a rigid body with default settings.
        void CreateRigidBody();
        // Create and attach a drawable with default settings.
        void CreateDrawable();
        // Create and attach a text item with default settings.
        void CreateTextItem();
        // Create and attach spatial index  node with default settings.
        void CreateSpatialNode();
        // Create and attach a fixture with default settings.
        void CreateFixture();

        void RemoveDrawable() noexcept
        { mDrawable.reset(); }
        void RemoveRigidBody() noexcept
        { mRigidBody.reset(); }
        void RemoveTextItem() noexcept
        { mTextItem.reset(); }
        void RemoveSpatialNode() noexcept
        { mSpatialNode.reset(); }
        void RemoveFixture() noexcept
        { mFixture.reset(); }

        // Get the rigid body shared class object if any.
        std::shared_ptr<const RigidBodyItemClass> GetSharedRigidBody() const noexcept
        { return mRigidBody; }
        // Get the drawable shared class object if any.
        std::shared_ptr<const DrawableItemClass> GetSharedDrawable() const noexcept
        { return mDrawable; }
        // Get the text item class object if any.
        std::shared_ptr<const TextItemClass> GetSharedTextItem() const noexcept
        { return mTextItem; }
        // Get the spatial index node if any.
        std::shared_ptr<const SpatialNodeClass> GetSharedSpatialNode() const noexcept
        { return mSpatialNode; }
        // Get the fixture class if any
        std::shared_ptr<const FixtureClass> GetSharedFixture() const noexcept
        { return mFixture; }

        // Returns true if a rigid body has been set for this class.
        bool HasRigidBody() const noexcept
        { return !!mRigidBody; }
        // Returns true if a drawable object has been set for this class.
        bool HasDrawable() const noexcept
        { return !!mDrawable; }
        bool HasTextItem() const noexcept
        { return !!mTextItem; }
        bool HasSpatialNode() const noexcept
        { return !!mSpatialNode; }
        bool HasFixture() const noexcept
        { return !!mFixture; }

        // Get the rigid body object if any. If no rigid body class object
        // has been set then returns nullptr.
        RigidBodyItemClass* GetRigidBody() noexcept
        { return mRigidBody.get(); }
        // Get the drawable shape object if any. If no drawable shape class object
        // has been set then returns nullptr.
        DrawableItemClass* GetDrawable() noexcept
        { return mDrawable.get(); }
        // Get the text item object if any. If no text item class object
        // has been set then returns nullptr:
        TextItemClass* GetTextItem() noexcept
        { return mTextItem.get(); }
        // Get the spatial index node if any. If no spatial index node object
        // has been set then returns nullptr.
        SpatialNodeClass* GetSpatialNode() noexcept
        { return mSpatialNode.get(); }
        // Get the fixture class object if any. If no fixture has been attached
        // then returns nullptr.
        FixtureClass* GetFixture() noexcept
        { return mFixture.get(); }
        // Get the rigid body object if any. If no rigid body class object
        // has been set then returns nullptr.
        const RigidBodyItemClass* GetRigidBody() const noexcept
        { return mRigidBody.get(); }
        // Get the drawable shape object if any. If no drawable shape class object
        // has been set then returns nullptr.
        const DrawableItemClass* GetDrawable() const noexcept
        { return mDrawable.get(); }
        // Get the text item object if any. If no text item class object
        // has been set then returns nullptr:
        const TextItemClass* GetTextItem() const noexcept
        { return mTextItem.get(); }
        // Get the spatial index node if any. If no spatial index node object
        // has been set then returns nullptr.
        const SpatialNodeClass* GetSpatialNode() const noexcept
        { return mSpatialNode.get(); }
        // Get the fixture class object if any. If no fixture has been attached
        // then returns nullptr.
        const FixtureClass* GetFixture() const noexcept
        { return mFixture.get(); }

        // Get the transform that applies to this node
        // and the subsequent hierarchy of nodes.
        glm::mat4 GetNodeTransform() const;
        // Get this drawable item's model transform that applies
        // to the node's box based items such as drawables
        // and rigid bodies.
        glm::mat4 GetModelTransform() const;

        int GetLayer() const noexcept
        { return mDrawable ? mDrawable->GetLayer() : 0; }

        void Update(float time, float dt);
        // Serialize the node into JSON.
        void IntoJson(data::Writer& data) const;
        // Load the node's properties from the given JSON object.
        bool FromJson(const data::Reader& data);
        // Make a new unique copy of this node class object
        // with all the same properties but with a different/unique ID.
        EntityNodeClass Clone() const;

        EntityNodeClass& operator=(const EntityNodeClass& other);

    private:
        // the resource id.
        std::string mClassId;
        // human-readable name of the class.
        std::string mName;
        // translation of the node relative to its parent.
        glm::vec2 mPosition = {0.0f, 0.0f};
        // Nodes scaling factor. applies to all children.
        glm::vec2 mScale = {1.0f, 1.0f};
        // Size of the node's containing box.
        glm::vec2 mSize  = {1.0f, 1.0f};
        // Rotation around z axis in radians.
        float mRotation = 0.0f;
        // rigid body if any.
        std::shared_ptr<RigidBodyItemClass> mRigidBody;
        // drawable item if any.
        std::shared_ptr<DrawableItemClass> mDrawable;
        // text item if any.
        std::shared_ptr<TextItemClass> mTextItem;
        // spatial node if any.
        std::shared_ptr<SpatialNodeClass> mSpatialNode;
        // fixture if any.
        std::shared_ptr<FixtureClass> mFixture;
        // bitflags that apply to node.
        base::bitflag<Flags> mBitFlags;
    };
    class Entity;

    class EntityNode
    {
    public:
        using Flags = EntityNodeClass::Flags;
        using DrawableItemType = DrawableItem;

        EntityNode(std::shared_ptr<const EntityNodeClass> klass);
        EntityNode(const EntityNodeClass& klass);
        EntityNode(const EntityNode& other);
        EntityNode(EntityNode&& other);

        // instance setters.
        void SetScale(const glm::vec2& scale) noexcept
        { mScale = scale; }
        void SetScale(float sx, float sy) noexcept
        { mScale = glm::vec2(sx, sy); }
        void SetSize(const glm::vec2& size) noexcept
        { mSize = size; }
        void SetSize(float width, float height) noexcept
        { mSize = glm::vec2(width, height); }
        void SetTranslation(const glm::vec2& pos) noexcept
        { mPosition = pos; }
        void SetTranslation(float x, float y) noexcept
        { mPosition = glm::vec2(x, y); }
        void SetRotation(float rotation) noexcept
        { mRotation = rotation; }
        void SetName(const std::string& name)
        { mName = name; }
        void SetEntity(Entity* entity) noexcept
        { mEntity = entity; }
        void Translate(const glm::vec2& vec) noexcept
        { mPosition += vec; }
        void Translate(float dx, float dy) noexcept
        { mPosition += glm::vec2(dx, dy); }
        void Rotate(float dr) noexcept
        { mRotation += dr; }
        void Grow(const glm::vec2& vec) noexcept
        { mSize += vec; }
        void Grow(float dx, float dy) noexcept
        { mSize += glm::vec2(dx, dy); }

        // instance getters.
        const std::string& GetId() const noexcept
        { return mInstId; }
        const std::string& GetName() const noexcept
        { return mName; }
        const glm::vec2& GetTranslation() const noexcept
        { return mPosition; }
        const glm::vec2& GetScale() const noexcept
        { return mScale; }
        const glm::vec2& GetSize() const noexcept
        { return mSize; }
        float GetRotation() const noexcept
        { return mRotation; }
        bool TestFlag(Flags flags) const noexcept
        { return mClass->TestFlag(flags); }
        Entity* GetEntity() noexcept
        { return mEntity; }
        const Entity* GetEntity() const noexcept
        { return mEntity; }

        // Get the node's drawable item if any. If no drawable
        // item is set then returns nullptr.
        DrawableItem* GetDrawable();
        // Get the node's rigid body item if any. If no rigid body
        // item is set then returns nullptr.
        RigidBodyItem* GetRigidBody();
        // Get the node's text item if any. If no text item
        // is set then returns nullptr.
        TextItem* GetTextItem();
        // Get the node's fixture if any. If no fixture is et
        // then returns nullptr.
        Fixture* GetFixture();
        // Get the node's drawable item if any. If now drawable
        // item is set then returns nullptr.
        const DrawableItem* GetDrawable() const;
        // Get the node's rigid body item if any. If no rigid body
        // item is set then returns nullptr.
        const RigidBodyItem* GetRigidBody() const;
        // Get the node's text item if any. If no text item
        // is set then returns nullptr.
        const TextItem* GetTextItem() const;
        // Get the node's spatial node if any. If no spatial node
        // is set then returns nullptr.
        const SpatialNode* GetSpatialNode() const;
        // Get the node's fixture if any. If no fixture is et
        // then returns nullptr.
        const Fixture* GetFixture() const;

        bool HasRigidBody() const noexcept
        { return !!mRigidBody; }
        bool HasDrawable() const noexcept
        { return !!mDrawable; }
        bool HasTextItem() const noexcept
        { return !!mTextItem; }
        bool HasSpatialNode() const noexcept
        { return !!mSpatialNode; }
        bool HasFixture() const noexcept
        { return !!mFixture; }

        // shortcut for class getters.
        const std::string& GetClassId() const noexcept
        { return mClass->GetId(); }
        const std::string& GetClassName() const noexcept
        { return mClass->GetName(); }
        int GetLayer() const noexcept
        { return mClass->GetLayer(); }

        // Reset node's state to initial class state.
        void Reset();
        // Get the transform that applies to this node
        // and the subsequent hierarchy of nodes.
        glm::mat4 GetNodeTransform() const;
        // Get this drawable item's model transform that applies
        // to the node's box based items such as drawables
        // and rigid bodies.
        glm::mat4 GetModelTransform() const;

        const EntityNodeClass& GetClass() const noexcept
        { return *mClass.get(); }
        const EntityNodeClass* operator->() const noexcept
        { return mClass.get(); }
    private:
        // the class object.
        std::shared_ptr<const EntityNodeClass> mClass;
        // the instance id.
        std::string mInstId;
        // the instance name.
        std::string mName;
        // translation of the node relative to its parent.
        glm::vec2 mPosition = {0.0f, 0.0f};
        // Nodes scaling factor. applies to this node
        // and all of its children.
        glm::vec2 mScale = {1.0f, 1.0f};
        // node's box size. used to generate collision shapes
        // and to resize the drawable shape (if any)
        glm::vec2 mSize = {1.0f, 1.0f};
        // Rotation around z axis in radians relative to parent.
        float mRotation = 0.0f;
        // rigid body if any.
        std::unique_ptr<RigidBodyItem> mRigidBody;
        // drawable if any.
        std::unique_ptr<DrawableItem> mDrawable;
        // text item if any
        std::unique_ptr<TextItem> mTextItem;
        // spatial node if any
        std::unique_ptr<SpatialNode> mSpatialNode;
        // fixture if any
        std::unique_ptr<Fixture> mFixture;
        // The entity that owns this node.
        Entity* mEntity = nullptr;
    };

    class EntityClass
    {
    public:
        using RenderTree      = game::RenderTree<EntityNodeClass>;
        using RenderTreeNode  = EntityNodeClass;
        using RenderTreeValue = EntityNodeClass;

        enum class Flags {
            // Only pertains to editor (todo: maybe this flag should be removed)
            VisibleInEditor,
            // node is visible in the game or not.
            // Even if this is true the node will still need to have some
            // renderable items attached to it such as a shape or
            // animation item.
            VisibleInGame,
            // Limit the lifetime to some maximum amount
            // after which the entity is killed.
            LimitLifetime,
            // Whether to automatically kill entity when it reaches
            // its end of lifetime.
            KillAtLifetime,
            // Whether to automatically kill entity when it reaches (goes past)
            // the border of the scene
            KillAtBoundary,
            // Invoke the tick function on the entity
            TickEntity,
            // Invoke the update function on the entity
            UpdateEntity,
            // Whether to pass keyboard events to the entity or not
            WantsKeyEvents,
            // Whether to pass mouse events to the entity or not.
            WantsMouseEvents,
        };

        enum class PhysicsJointType {
            Distance
        };

        struct DistanceJointParams {
            std::optional<float> min_distance;
            std::optional<float> max_distance;
            float stiffness = 0.0f;
            float damping   = 0.0f;
            DistanceJointParams()
            {
                min_distance.reset();
                max_distance.reset();
            }
        };

        using PhysicsJointParams = std::variant<DistanceJointParams>;

        // PhysicsJoint defines an optional physics engine constraint
        // between two bodies in the physics world. In other words
        // between two entity nodes that both have a rigid body
        // attachment. The two entity nodes are identified by their
        // node IDs. The distinction between "source" and "destination"
        // is arbitrary and not relevant.
        struct PhysicsJoint {
            // The type of the joint.
            PhysicsJointType type = PhysicsJointType::Distance;
            // The source node ID.
            std::string src_node_id;
            // The destination node ID.
            std::string dst_node_id;
            // the anchor point within the body
            glm::vec2 src_node_anchor_point = {0.0f, 0.0f};
            // the anchor point within the body.
            glm::vec2 dst_node_anchor_point = {0.0f, 0.0f};
            // ID of this joint.
            std::string id;
            // human-readable name of the joint.
            std::string name;
            // PhysicsJoint parameters (depending on the type)
            PhysicsJointParams params;
        };

        EntityClass();
        EntityClass(const EntityClass& other);

        // Add a new node to the entity.
        // Returns a pointer to the node that was added to the entity.
        // The returned EntityNodeClass object pointer is only guaranteed
        // to be valid until the next call to AddNode/Delete. It is not
        // safe for the caller to hold on to it long term. Instead, the
        // assumed use of the pointer is to simplify for example calling
        // LinkChild for linking the node to the entity's render tree.
        EntityNodeClass* AddNode(const EntityNodeClass& node);
        EntityNodeClass* AddNode(EntityNodeClass&& node);
        EntityNodeClass* AddNode(std::unique_ptr<EntityNodeClass> node);

        // PhysicsJoint lifetimes.
        // For any API function returning a PhysicsJoint* (or const PhysicsJoint*) it's
        // safe to assume that the returned object pointer is valid only
        // until the next call to modify the list of joints. I.e. any call to
        // AddJoint or DeleteJoint can invalidate any previously returned PhysicsJoint*
        // pointer. The caller should not hold on to these pointers long term.

        // Add a new joint definition linking 2 entity nodes with rigid bodies
        // together using a physics joint. The joints must refer to valid nodes
        // that already exist (i.e. the src and dst node class IDst must be valid).
        PhysicsJoint* AddJoint(const PhysicsJoint& joint);
        PhysicsJoint* AddJoint(PhysicsJoint&& joint);
        // Respecify the details of a joint at the given index.
        void SetJoint(size_t index, const PhysicsJoint& joint);
        void SetJoint(size_t index, PhysicsJoint&& joint);
        // Get the joint at the given index. The index must be valid.
        PhysicsJoint& GetJoint(size_t index);
        // Find a joint by the given joint ID. Returns nullptr if no
        // such joint could be found.
        PhysicsJoint* FindJointById(const std::string& id);
        // Find a joint by a node id, i.e. when the joint specifies a
        // connection between either src or dst node having the given
        // node ID. In case of multiple joints connecting between the
        // same nodes the returned node is the first one found.
        PhysicsJoint* FindJointByNodeId(const std::string& id);
        // Get the joint at the given index. The index must be valid.
        const PhysicsJoint& GetJoint(size_t index) const;
        // Find a joint by the given joint ID. Returns nullptr if no
        // such joint could be found.
        const PhysicsJoint* FindJointById(const std::string& id) const;
        // Find a joint by a node id, i.e. when the joint specifies a
        // connection between either src or dst node having the given
        // node ID. In case of multiple joints connecting between the
        // same nodes the returned node is the first one found.
        const PhysicsJoint* FindJointByNodeId(const std::string& id) const;
        // Delete a joint by the given ID.
        void DeleteJointById(const std::string& id);
        // Delete a joint by the given index. The index must be valid.
        void DeleteJoint(std::size_t index);
        // todo:
        void DeleteInvalidJoints();
        // todo:
        void FindInvalidJoints(std::vector<PhysicsJoint*>* invalid);
        // todo:
        void DeleteInvalidFixtures();

        // Get the node by index. The index must be valid.
        EntityNodeClass& GetNode(size_t index);
        // Find entity node by name. Returns nullptr if
        // no such node could be found.
        EntityNodeClass* FindNodeByName(const std::string& name);
        // Find entity node by id. Returns nullptr if
        // no such node could be found.
        EntityNodeClass* FindNodeById(const std::string& id);
        // Get the entity node by index. The index must be valid.
        const EntityNodeClass& GetNode(size_t index) const;
        // Find entity node by name. Returns nullptr if
        // no such node could be found.
        const EntityNodeClass* FindNodeByName(const std::string& name) const;
        // Find entity node by id. Returns nullptr if
        // no such node could be found.
        const EntityNodeClass* FindNodeById(const std::string& id) const;

        // Add a new animation class object. Returns a pointer to the animation
        // that was added to the entity class.
        AnimationClass* AddAnimation(AnimationClass&& track);
        // Add a new animation class object. Returns a pointer to the animation
        // that was added to the entity class.
        AnimationClass* AddAnimation(const AnimationClass& track);
        // Add a new animation class object. Returns a pointer to the animation
        // that was added to the entity class.
        AnimationClass* AddAnimation(std::unique_ptr<AnimationClass> track);
        // Delete an animation by the given index. The index must be valid.
        void DeleteAnimation(size_t index);
        // Delete an animation by the given name.
        // Returns true if a track was deleted, otherwise false.
        bool DeleteAnimationByName(const std::string& name);
        // Delete an animation by the given id.
        // Returns true if a track was deleted, otherwise false.
        bool DeleteAnimationById(const std::string& id);
        // Get the animation class object by index. The index must be valid.
        AnimationClass& GetAnimation(size_t index);
        // Find an animation class object by name.
        // Returns nullptr if no such animation could be found.
        // Returns the first animation object with a matching name.
        AnimationClass* FindAnimationByName(const std::string& name);
        // Get the animation class object by index. The index must be valid.
        const AnimationClass& GetAnimation(size_t index) const;
        // Find an animation class object by name.
        // Returns nullptr if no such animation could be found.
        // Returns the first animation object with a matching name.
        const AnimationClass* FindAnimationByName(const std::string& name) const;

        // Add a new animator class object. Returns a pointer to the animator
        // that was added to the entity class.
        AnimatorClass* AddAnimator(AnimatorClass&& animator);
        AnimatorClass* AddAnimator(const AnimatorClass& animator);
        AnimatorClass* AddAnimator(const std::shared_ptr<AnimatorClass>& animator);

        void DeleteAnimator(size_t index);
        bool DeleteAnimatorByName(const std::string& name);
        bool DeleteAnimatorById(const std::string& id);

        AnimatorClass& GetAnimator(size_t index);
        AnimatorClass* FindAnimatorByName(const std::string& name);
        AnimatorClass* FindAnimatorById(const std::string& id);

        const AnimatorClass& GetAnimator(size_t index) const;
        const AnimatorClass* FindAnimatorByName(const std::string& name) const;
        const AnimatorClass* FindAnimatorById(const std::string& id) const;

        // Link the given child node with the parent.
        // The parent may be a nullptr in which case the child
        // is added to the root of the entity. The child node needs
        // to be a valid node and needs to point to node that is not
        // yet any part of the render tree and is a node that belongs
        // to this entity.
        void LinkChild(EntityNodeClass* parent, EntityNodeClass* child);

        // Break a child node away from its parent. The child node needs
        // to be a valid node and needs to point to a node that is added
        // to the render tree and belongs to this entity class object.
        // The child (and all of its children) that has been broken still
        // exists in the entity but is removed from the render tree.
        // You can then either DeleteNode to completely delete it or
        // LinkChild to insert it into another part of the render tree.
        void BreakChild(EntityNodeClass* child, bool keep_world_transform = true);

        // Re-parent a child node from its current parent to another parent.
        // Both the child node and the parent node to be a valid nodes and
        // need to point to nodes that are part of the render tree and belong
        // to this entity class object. This will move the whole hierarchy of
        // nodes starting from child under the new parent.
        // If keep_world_transform is true the child will be transformed such
        // that it's current world transformation remains the same. I.e  it's
        // position and rotation in the world don't change.
        void ReparentChild(EntityNodeClass* parent, EntityNodeClass* child, bool keep_world_transform = true);

        // Delete a node from the entity. The given node and all of its
        // children will be removed from the entity render tree and then deleted.
        void DeleteNode(EntityNodeClass* node);

        // Duplicate an entire node hierarchy starting at the given node
        // and add the resulting hierarchy to node's parent.
        // Returns the root node of the new node hierarchy.
        EntityNodeClass* DuplicateNode(const EntityNodeClass* node);

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node's box in the entity.
        // The testing is coarse in the sense that it's done against the node's
        // size box only. The hit nodes are stored in the hits vector and the
        // positions with the nodes' hitboxes are (optionally) stored in the
        // hitbox_positions vector.
        void CoarseHitTest(float x, float y, std::vector<EntityNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(const glm::vec2& pos, std::vector<EntityNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(float x, float y, std::vector<const EntityNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;
        void CoarseHitTest(const glm::vec2& pos, std::vector<const EntityNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in node's OOB space into entity coordinate space. The origin of
        // the OOB space is relative to the "TopLeft" corner of the OOB of the node.
        glm::vec2 MapCoordsFromNodeBox(float x, float y, const EntityNodeClass* node) const;
        glm::vec2 MapCoordsFromNodeBox(const glm::vec2& pos, const EntityNodeClass* node) const;
        // Map coordinates in entity coordinate space into node's OOB coordinate space.
        glm::vec2 MapCoordsToNodeBox(float x, float y, const EntityNodeClass* node) const;
        glm::vec2 MapCoordsToNodeBox(const glm::vec2& pos, const EntityNodeClass* node) const;

        // Compute the axis aligned bounding box (AABB) for the whole entity.
        FRect GetBoundingRect() const;
        // Compute the axis aligned bounding box (AABB) for the given entity node.
        FRect FindNodeBoundingRect(const EntityNodeClass* node) const;
        // Compute the oriented bounding box (OOB) for the given entity node.
        FBox FindNodeBoundingBox(const EntityNodeClass* node) const;

        // todo:
        glm::mat4 FindNodeTransform(const EntityNodeClass* node) const;
        glm::mat4 FindNodeModelTransform(const EntityNodeClass* node) const;

        // Add a new scripting variable to the list of variables.
        // No checks are made to whether a variable by that name
        // already exists.
        void AddScriptVar(const ScriptVar& var);
        void AddScriptVar(ScriptVar&& var);
        // Delete the scripting variable at the given index.
        // The index must be a valid index.
        void DeleteScriptVar(size_t index);
        // Set the properties (copy over) the scripting variable at the given index.
        // The index must be a valid index.
        void SetScriptVar(size_t index, const ScriptVar& var);
        void SetScriptVar(size_t index, ScriptVar&& var);
        // Get the scripting variable at the given index.
        // The index must be a valid index.
        ScriptVar& GetScriptVar(size_t index);
        // Find a scripting variable with the given name. If no such variable
        // exists then nullptr is returned.
        ScriptVar* FindScriptVarByName(const std::string& name);
        // Find a scripting variable with the given id. If no such variable
        // exists then nullptr is returned.
        ScriptVar* FindScriptVarById(const std::string& id);
        // Get the scripting variable at the given index.
        // The index must be a valid index.
        const ScriptVar& GetScriptVar(size_t index) const;
        // Find a scripting variable with the given name. If no such variable
        // exists then nullptr is returned.
        const ScriptVar* FindScriptVarByName(const std::string& name) const;
        // Find a scripting variable with the given id. If no such variable
        // exists then nullptr is returned.
        const ScriptVar* FindScriptVarById(const std::string& id) const;

        void SetLifetime(float value) noexcept
        { mLifetime = value;}
        void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }
        void SetName(const std::string& name)
        { mName = name; }
        void SetTag(const std::string& tag)
        { mTag = tag; }
        void SetIdleTrackId(const std::string& id)
        { mIdleTrackId = id; }
        void SetSriptFileId(const std::string& file)
        { mScriptFile = file; }
        void ResetIdleTrack() noexcept
        { mIdleTrackId.clear(); }
        void ResetScriptFile() noexcept
        { mScriptFile.clear(); }
        bool HasIdleTrack() const noexcept
        { return !mIdleTrackId.empty(); }
        bool HasScriptFile() const noexcept
        { return !mScriptFile.empty(); }
        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        int GetLayer() const noexcept /* stub*/
        { return 0; }

        RenderTree& GetRenderTree() noexcept
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const noexcept
        { return mRenderTree; }

        std::size_t GetHash() const;
        std::size_t GetNumNodes() const noexcept
        { return mNodes.size(); }
        std::size_t GetNumAnimators() const noexcept
        { return mAnimators.size(); }
        std::size_t GetNumAnimations() const noexcept
        { return mAnimations.size(); }
        std::size_t GetNumScriptVars() const noexcept
        { return mScriptVars.size(); }
        std::size_t GetNumJoints() const noexcept
        { return mJoints.size(); }
        const std::string& GetId() const noexcept
        { return mClassId; }
        const std::string& GetIdleTrackId() const noexcept
        { return mIdleTrackId; }
        const std::string& GetName() const noexcept
        { return mName; }
        const std::string& GetTag() const noexcept
        { return mTag; }
        const std::string& GetScriptFileId() const noexcept
        { return mScriptFile; }
        float GetLifetime() const noexcept
        { return mLifetime; }
        const base::bitflag<Flags>& GetFlags() const noexcept
        { return mFlags; }

        std::shared_ptr<const EntityNodeClass> GetSharedEntityNodeClass(size_t index) const noexcept
        { return mNodes[index]; }
        std::shared_ptr<const AnimationClass> GetSharedAnimationClass(size_t index) const noexcept
        { return mAnimations[index]; }
        std::shared_ptr<const ScriptVar> GetSharedScriptVar(size_t index) const noexcept
        { return mScriptVars[index]; }
        std::shared_ptr<const PhysicsJoint> GetSharedJoint(size_t index) const noexcept
        { return mJoints[index]; }
        std::shared_ptr<const AnimatorClass> GEtSharedAnimatorClass(size_t index) const noexcept
        { return mAnimators[index]; }

        // Serialize the entity into JSON.
        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);

        EntityClass Clone() const;

        EntityClass& operator=(const EntityClass& other);
    private:
        // The class/resource id of this class.
        std::string mClassId;
        // the human-readable name of the class.
        std::string mName;
        // Arbitrary tag string.
        std::string mTag;
        // the track ID of the idle track that gets played when nothing
        // else is going on. can be empty in which case no animation plays.
        std::string mIdleTrackId;
        // the list of animation tracks that are pre-defined with this
        // type of animation.
        std::vector<std::shared_ptr<AnimationClass>> mAnimations;
        // the list of nodes that belong to this entity.
        std::vector<std::shared_ptr<EntityNodeClass>> mNodes;
        // the list of joints that belong to this entity.
        std::vector<std::shared_ptr<PhysicsJoint>> mJoints;
        // the list of animators
        std::vector<std::shared_ptr<AnimatorClass>> mAnimators;
        // The render tree for hierarchical traversal and
        // transformation of the entity and its nodes.
        RenderTree mRenderTree;
        // Scripting variables. read-only variables are
        // shareable with each entity instance.
        std::vector<std::shared_ptr<ScriptVar>> mScriptVars;
        // the name of the associated script if any.
        std::string mScriptFile;
        // entity class flags.
        base::bitflag<Flags> mFlags;
        // maximum lifetime after which the entity is
        // deleted if LimitLifetime flag is set.
        float mLifetime = 0.0f;
    };

    // Collection of arguments for creating a new entity
    // with some initial state. The immutable arguments must
    // go here (i.e. the ones that cannot be changed after
    // the entity has been created).
    struct EntityArgs {
        // the class object that defines the type of the entity.
        std::shared_ptr<const EntityClass> klass;
        // the entity instance id that is to be used.
        std::string id;
        // the entity instance name that is to be used.
        std::string name;
        // the transformation parents are relative to the parent
        // of the entity.
        // the instance scale to be used. note that if the entity
        // has rigid body changing the scale dynamically later on
        // after the physics simulation object has been created may
        // not work correctly. Therefore, it's important to use the
        // scaling factor here to set the scale when creating a new 
        // entity.
        glm::vec2  scale    = {1.0f, 1.0f};
        // the entity position relative to parent.
        glm::vec2  position = {0.0f, 0.0f};
        // the entity rotation relative to parent
        float rotation = 0.0f;
        // flag to indicate whether to log events
        // pertaining to this entity or not.
        bool enable_logging = true;
        // The scene layer for the entity.
        int layer = 0;
    };

    class Entity
    {
    public:
        // Runtime management flags
        enum class ControlFlags {
            // The entity has been killed and will be deleted
            // at the end of the current game loop iteration.
            Killed,
            // The entity has been just spawned through a
            // call to Scene::SpawnEntity. The flag will be
            // cleared at the end of the main loop iteration
            // and is thus on only from spawn until the end of
            // loop iteration.
            Spawned,
            // Flag to enable logging related to the entity.
            // Turning off logging is useful when spawning a large
            // number of entities continuously and doing that would
            // create excessive logging.
            EnableLogging,
            // The entity wants to die and be killed and removed
            // from the scene.
            WantsToDie
        };
        using Flags = EntityClass::Flags;
        using RenderTree      = game::RenderTree<EntityNode>;
        using RenderTreeNode  = EntityNode;
        using RenderTreeValue = EntityNode;

        // Construct a new entity with the initial state based
        // on the entity class object's state.
        explicit Entity(const EntityArgs& args);
        explicit Entity(const std::shared_ptr<const EntityClass>& klass);
        explicit Entity(const EntityClass& klass);
        Entity(const Entity& other) = delete;
        
        // Get the entity node by index. The index must be valid.
        EntityNode& GetNode(size_t index);
        // Find entity node by class name. Returns nullptr if no such node could be found.
        // Note that there could be multiple nodes with the same name. In this
        // case it's undefined which of the nodes would be returned.
        EntityNode* FindNodeByClassName(const std::string& name);
        // Find entity node by class id. Returns nullptr if no such node could be found.
        // Note that there could be multiple nodes with the same class id. In this
        // case it's undefined which of the nodes would be returned.
        EntityNode* FindNodeByClassId(const std::string& id);
        // Find a entity node by node's instance id. Returns nullptr if no such node could be found.
        EntityNode* FindNodeByInstanceId(const std::string& id);
        // Find a entity node by its instance name. Returns nullptr if no such node could be found.
        EntityNode* FindNodeByInstanceName(const std::string& name);
        // Get the entity node by index. The index must be valid.
        const EntityNode& GetNode(size_t index) const;
        // Find entity node by name. Returns nullptr if no such node could be found.
        // Note that there could be multiple nodes with the same name. In this
        // case it's undefined which of the nodes would be returned.
        const EntityNode* FindNodeByClassName(const std::string& name) const;
        // Find entity node by class id. Returns nullptr if no such node could be found.
        // Note that there could be multiple nodes with the same class id. In this
        // case it's undefined which of the nodes would be returned.
        const EntityNode* FindNodeByClassId(const std::string& id) const;
        // Find entity node by node's instance id. Returns nullptr if no such node could be found.
        const EntityNode* FindNodeByInstanceId(const std::string& id) const;
        // Find a entity node by its instance name. Returns nullptr if no such node could be found.
        const EntityNode* FindNodeByInstanceName(const std::string& name) const;

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node's box in the entity.
        // The testing is coarse in the sense that it's done against the node's
        // size box only. The hit nodes are stored in the hits vector and the
        // positions with the nodes' hitboxes are (optionally) stored in the
        // hitbox_positions vector.
        void CoarseHitTest(float x, float y, std::vector<EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(const glm::vec2& pos, std::vector<EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(float x, float y, std::vector<const EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;
        void CoarseHitTest(const glm::vec2& pos, std::vector<const EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in node's OOB space into entity coordinate space. The origin of
        // the OOB space is relative to the "TopLeft" corner of the OOB of the node.
        glm::vec2 MapCoordsFromNodeBox(float x, float y, const EntityNode* node) const;
        glm::vec2 MapCoordsFromNodeBox(const glm::vec2& pos, const EntityNode* node) const;
        // Map coordinates in entity coordinate space into node's OOB coordinate space.
        glm::vec2 MapCoordsToNodeBox(float x, float y, const EntityNode* node) const;
        glm::vec2 MapCoordsToNodeBox(const glm::vec2& pos, const EntityNode* node) const;

        // Compute the axis aligned bounding box (AABB) for the whole entity.
        FRect GetBoundingRect() const;
        // Compute the axis aligned bounding box (AABB) for the given entity node.
        FRect FindNodeBoundingRect(const EntityNode* node) const;
        // Compute the oriented bounding box (OOB) for the given entity node.
        FBox FindNodeBoundingBox(const EntityNode* node) const;

        // todo:
        glm::mat4 FindNodeTransform(const EntityNode* node) const;
        glm::mat4 FindNodeModelTransform(const EntityNode* node) const;
        glm::mat4 FindRelativeTransform(const EntityNode* parent, const EntityNode* child) const;

        void Die();

        using PostedEventValue = std::variant<
            bool, int, float,
            std::string,
            glm::vec2, glm::vec3, glm::vec4>;
        struct PostedEvent {
            std::string message;
            std::string sender;
            PostedEventValue value;
        };

        struct TimerEvent {
            std::string name;
            float jitter = 0.0f;
        };
        using Event = std::variant<TimerEvent, PostedEvent>;

        void Update(float dt, std::vector<Event>* events = nullptr);

        using AnimatorAction = game::Animator::Action;
        using AnimationState = game::AnimationState;
        using AnimationTransition = game::AnimationTransition;

        void UpdateAnimator(float dt, std::vector<AnimatorAction>* actions);
        void UpdateAnimator(const AnimationTransition* transition, const AnimationState* next);

        const AnimationState* GetCurrentAnimatorState() noexcept;
        const AnimationTransition* GetCurrentAnimationTransition() noexcept;

        // Play the given animation immediately. If there's any
        // current animation it is replaced.
        Animation* PlayAnimation(std::unique_ptr<Animation> animation);
        // Create a copy of the given animation and play it immediately.
        Animation* PlayAnimation(const Animation& animation);
        // Create a copy of the given animation and play it immediately.
        Animation* PlayAnimation(Animation&& animation);
        // Play a previously recorded animation identified by name.
        // Returns a pointer to the animation instance or nullptr if
        // no such animation could be found.
        Animation* PlayAnimationByName(const std::string& name);
        // Play a previously recorded animation identified by its ID.
        // Returns a pointer to the animation instance or nullptr if
        // no such animation could be found.
        Animation* PlayAnimationById(const std::string& id);
        // Play the designated idle animation if there is a designated
        // idle animation and if no other animation is currently playing.
        // Returns a pointer to the animation instance or nullptr if
        // idle animation was started.
        Animation* PlayIdle();
        // Returns true if someone killed the entity.
        bool IsDying() const;
        // Returns true if an animation is currently playing.
        bool IsAnimating() const;
        // Returns true if the entity lifetime has been exceeded.
        bool HasExpired() const;
        // Returns true if the kill control flag has been set.
        bool HasBeenKilled() const;
        // Returns true the spawn control flag has been set.
        bool HasBeenSpawned() const;
        // Returns true if entity contains entity nodes that have rigid bodies.
        bool HasRigidBodies() const;
        // Returns true if the entity contains entity nodes that have spatial nodes.
        bool HasSpatialNodes() const;
        // Returns true if the entity should be killed at the scene boundary.
        bool KillAtBoundary() const;
        // Returns true if the entity did just finish an animation.
        bool DidFinishAnimation() const;
        // Returns true if the entity contains nodes that have rendering
        // attachments, i.d. drawables or text items.
        bool HasRenderableItems() const;

        using PhysicsJointClass = EntityClass::PhysicsJoint;
        using PhysicsJointType  = EntityClass::PhysicsJointType;
        using PhysicsJointParams = EntityClass::PhysicsJointParams;
        class PhysicsJoint
        {
        public:
            PhysicsJoint(const std::shared_ptr<const PhysicsJointClass>& joint,
                         std::string joint_id,
                         EntityNode* src_node,
                         EntityNode* dst_node)
              : mClass(joint)
              , mId(std::move(joint_id))
              , mSrcNode(src_node)
              , mDstNode(dst_node)
            {}
            PhysicsJointType GetType() const noexcept
            { return mClass->type; }
            const EntityNode* GetSrcNode() const noexcept
            { return mSrcNode; }
            const EntityNode* GetDstNode() const noexcept
            { return mDstNode; }
            const std::string& GetSrcId() const noexcept
            { return mDstNode->GetId(); }
            const std::string& GetDstId() const noexcept
            { return mDstNode->GetId(); }
            const std::string& GetId() const noexcept
            { return mId; }
            const std::string& GetName() const noexcept
            { return mClass->name; }
            const glm::vec2& GetSrcAnchorPoint() const noexcept
            { return mClass->src_node_anchor_point; }
            const glm::vec2& GetDstAnchorPoint() const noexcept
            { return mClass->dst_node_anchor_point; }
            const PhysicsJointClass* operator->() const noexcept
            { return mClass.get(); }
            const PhysicsJointClass& GetClass() const noexcept
            { return *mClass; }
            const PhysicsJointParams& GetParams() const noexcept
            { return mClass->params; }
        private:
            std::shared_ptr<const PhysicsJointClass> mClass;
            std::string mId;
            EntityNode* mSrcNode = nullptr;
            EntityNode* mDstNode = nullptr;
        };

        const PhysicsJoint& GetJoint(std::size_t index) const;
        const PhysicsJoint* FindJointById(const std::string& id) const;
        const PhysicsJoint* FindJointByNodeId(const std::string& id) const;

        // Find a scripting variable.
        // Returns nullptr if there was no variable by this name.
        // Note that the const here only implies that the object
        // may not change in terms of c++ semantics. The actual *value*
        // can still be changed as long as the variable is not read only.
        const ScriptVar* FindScriptVarByName(const std::string& name) const;
        const ScriptVar* FindScriptVarById(const std::string& id) const;

        void SetTag(const std::string& tag)
        { mInstanceTag = tag; }
        void SetFlag(ControlFlags flag, bool on_off) noexcept
        { mControlFlags.set(flag, on_off); }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }
        void SetParentNodeClassId(const std::string& id)
        { mParentNodeId = id; }
        void SetIdleTrackId(const std::string& id)
        { mIdleTrackId = id; }
        void SetLayer(int layer) noexcept
        { mLayer = layer; }
        void SetLifetime(double lifetime) noexcept
        { mLifetime = lifetime; }
        void SetScene(Scene* scene) noexcept
        { mScene = scene; }
        void SetVisible(bool on_off) noexcept
        { SetFlag(Flags::VisibleInGame, on_off); }
        void SetTimer(const std::string& name, double when)
        { mTimers.push_back({name, when}); }
        void PostEvent(const PostedEvent& event)
        { mEvents.push_back(event); }
        void PostEvent(PostedEvent&& event)
        { mEvents.push_back(std::move(event)); }


        // Get the current track if any. (when IsAnimating is true)
        Animation* GetCurrentAnimation() noexcept
        { return mCurrentAnimation.get(); }
        // Get the current track if any. (when IsAnimating is true)
        const Animation* GetCurrentAnimation() const noexcept
        { return mCurrentAnimation.get(); }
        // Get the previously completed animation track (if any).
        // This is a transient state that only exists for one
        // iteration of the game loop after the animation is done.
        const Animation* GetFinishedAnimation() const noexcept
        { return mFinishedAnimation.get(); }
        // Get the current scene.
        const Scene* GetScene() const noexcept
        { return mScene; }
        Scene* GetScene() noexcept
        { return mScene; }
        double GetLifetime() const noexcept
        { return mLifetime; }
        double GetTime() const noexcept
        { return mCurrentTime; }
        const std::string& GetIdleTrackId() const noexcept
        { return mIdleTrackId; }
        const std::string& GetParentNodeClassId() const noexcept
        { return mParentNodeId; }
        const std::string& GetClassId() const noexcept
        { return mClass->GetId(); }
        const std::string& GetId() const noexcept
        { return mInstanceId; }
        const std::string& GetClassName() const noexcept
        { return mClass->GetName(); }
        const std::string& GetName() const noexcept
        { return mInstanceName; }
        const std::string& GetTag() const noexcept
        { return mInstanceTag; }
        const std::string& GetScriptFileId() const noexcept
        { return mClass->GetScriptFileId(); }
        std::size_t GetNumNodes() const noexcept
        { return mNodes.size(); }
        std::size_t GetNumJoints() const noexcept
        { return mJoints.size(); }
        int GetLayer() const noexcept
        { return mLayer; }
        bool TestFlag(ControlFlags flag) const noexcept
        { return mControlFlags.test(flag); }
        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        bool IsVisible() const noexcept
        { return TestFlag(Flags::VisibleInGame); }
        bool HasIdleTrack() const noexcept
        { return !mIdleTrackId.empty() || mClass->HasIdleTrack(); }
        bool HasAnimator() const noexcept
        { return mAnimator.has_value(); }
        const Animator* GetAnimator() const
        { return base::GetOpt(mAnimator); }
        Animator* GetAnimator()
        { return base::GetOpt(mAnimator); }
        RenderTree& GetRenderTree() noexcept
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const noexcept
        { return mRenderTree; }
        const EntityClass& GetClass() const noexcept
        { return *mClass.get(); }
        const EntityClass* operator->() const noexcept
        { return mClass.get(); }
        Entity& operator=(const Entity&) = delete;
    private:
        // the class object.
        std::shared_ptr<const EntityClass> mClass;
        // The entity instance id.
        std::string mInstanceId;
        // the entity instance name (if any)
        std::string mInstanceName;
        // the entity instance tag
        std::string mInstanceTag;
        // When the entity is linked (parented)
        // to another entity this id is the node in the parent
        // entity's render tree that is to be used as the parent
        // of this entity's nodes.
        std::string mParentNodeId;
        std::optional<game::Animator> mAnimator;
        // The current animation if any.
        std::unique_ptr<Animation> mCurrentAnimation;
        // the list of nodes that are in the entity.
        std::vector<std::unique_ptr<EntityNode>> mNodes;
        // the list of read-write script variables. read-only ones are
        // shared between all instances and the EntityClass.
        std::vector<ScriptVar> mScriptVars;
        // list of physics joints between nodes with rigid bodies.
        std::vector<PhysicsJoint> mJoints;
        // the render tree for hierarchical traversal and transformation
        // of the entity and its nodes.
        RenderTree mRenderTree;
        // Current entity time.
        double mCurrentTime = 0.0;
        // Entity's max lifetime.
        double mLifetime = 0.0;
        // the render layer index.
        int mLayer = 0;
        // entity bit flags
        base::bitflag<Flags> mFlags;
        // id of the idle animation track.
        std::string mIdleTrackId;
        // control flags for the engine itself
        base::bitflag<ControlFlags> mControlFlags;
        // the previously finished animation track (if any)
        std::unique_ptr<Animation> mFinishedAnimation;
        // the current scene.
        Scene* mScene = nullptr;

        struct Timer {
            std::string name;
            double when = 0.0f;
        };
        std::vector<Timer> mTimers;
        std::vector<PostedEvent> mEvents;
    };

    std::unique_ptr<Entity> CreateEntityInstance(std::shared_ptr<const EntityClass> klass);
    std::unique_ptr<Entity> CreateEntityInstance(const EntityClass& klass);
    std::unique_ptr<Entity> CreateEntityInstance(const EntityArgs& args);
    std::unique_ptr<EntityNode> CreateEntityNodeInstance(std::shared_ptr<const EntityNodeClass> klass);

} // namespace
