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
#  include <nlohmann/json_fwd.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <optional>

#include "base/bitflag.h"
#include "base/utility.h"
#include "base/math.h"
#include "base/hash.h"
#include "graphics/drawable.h"
#include "engine/tree.h"
#include "engine/types.h"
#include "engine/enum.h"
#include "engine/animation.h"
#include "engine/color.h"

namespace game
{
    class RigidBodyItemClass
    {
    public:
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

        // Selection for collision shapes when the collision shape detection
        // is set to manual.
        enum class CollisionShape {
            // The collision shape is a box based on the size of node's box.
            Box,
            // The collision shape is a circle based on the largest extent of
            // the node's box.
            Circle,
            // The collision shape is a right angled triangle where the
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
        RigidBodyItemClass()
        {
            mBitFlags.set(Flags::Enabled, true);
            mBitFlags.set(Flags::CanSleep, true);
        }

        std::size_t GetHash() const;

        Simulation GetSimulation() const
        { return mSimulation; }
        CollisionShape GetCollisionShape() const
        { return mCollisionShape; }
        float GetFriction() const
        { return mFriction; }
        float GetRestitution() const
        { return mRestitution; }
        float GetAngularDamping() const
        { return mAngularDamping; }
        float GetLinearDamping() const
        { return mLinearDamping; }
        float GetDensity() const
        { return mDensity; }
        bool TestFlag(Flags flag) const
        { return mBitFlags.test(flag); }
        std::string GetPolygonShapeId() const
        { return mPolygonShapeId; }
        glm::vec2 GetLinearVelocity() const
        { return mLinearVelocity; }
        float GetAngularVelocity() const
        { return mAngularVelocity; }
        void ResetPolygonShapeId()
        { mPolygonShapeId.clear(); }
        base::bitflag<Flags> GetFlags() const
        { return mBitFlags; }

        void SetCollisionShape(CollisionShape shape)
        { mCollisionShape = shape; }
        void SetSimulation(Simulation simulation)
        { mSimulation = simulation; }
        void SetFlag(Flags flag, bool on_off)
        { mBitFlags.set(flag, on_off); }
        void SetFriction(float value)
        { mFriction = value; }
        void SetRestitution(float value)
        { mRestitution = value; }
        void SetAngularDamping(float value)
        { mAngularDamping = value; }
        void SetLinearDamping(float value)
        { mLinearDamping = value; }
        void SetDensity(float value)
        { mDensity = value; }
        void SetAngularVelocity(float value)
        { mAngularVelocity = value; }
        void SetPolygonShapeId(const std::string& id)
        { mPolygonShapeId = id; }
        void SetLinearVelocity(const glm::vec2& velocity)
        { mLinearVelocity = velocity; }

        void IntoJson(nlohmann::json& json) const;

        static std::optional<RigidBodyItemClass> FromJson(const nlohmann::json& json);
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
        // Initial linear velocity vector in meters per second.
        // Pertains to kinematic bodies.
        glm::vec2 mLinearVelocity = {0.0f, 0.0f};
        // Initial angular velocity of rotation around the
        // center of mass. Pertains to kinematic bodies.
        float mAngularVelocity = 0.0f;
    };

    // Drawable item defines a drawable item and its material and
    // properties that affect the rendering of the entity node
    class DrawableItemClass
    {
    public:
        using RenderPass = game::RenderPass;
        using RenderStyle = gfx::Drawable::Style;
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
            // Whether the item should override the material alpha value.
            OverrideAlpha,
            // Whether to flip (mirror) the item about Y axis
            FlipVertically,
        };
        DrawableItemClass()
        {
            mBitFlags.set(Flags::VisibleInGame, true);
            mBitFlags.set(Flags::UpdateDrawable, true);
            mBitFlags.set(Flags::UpdateMaterial, true);
            mBitFlags.set(Flags::RestartDrawable, true);
            mBitFlags.set(Flags::OverrideAlpha, false);
            mBitFlags.set(Flags::FlipVertically, false);
        }

        std::size_t GetHash() const;

        // class setters.
        void SetDrawableId(const std::string& klass)
        { mDrawableId = klass; }
        void SetMaterialId(const std::string& klass)
        { mMaterialId = klass; }
        void SetLayer(int layer)
        { mLayer = layer; }
        void ResetMaterial()
        { mMaterialId.clear(); }
        void ResetDrawable()
        { mDrawableId.clear(); }
        void SetFlag(Flags flag, bool on_off)
        { mBitFlags.set(flag, on_off); }
        void SetAlpha(float alpha)
        { mAlpha = math::clamp(0.0f, 1.0f, alpha); }
        void SetLineWidth(float width)
        { mLineWidth = width; }
        void SetRenderPass(RenderPass pass)
        { mRenderPass = pass; }
        void SetRenderStyle(RenderStyle style)
        { mRenderStyle = style; }
        void SetTimeScale(float scale)
        { mTimeScale = scale; }

        // class getters.
        std::string GetDrawableId() const
        { return mDrawableId; }
        std::string GetMaterialId() const
        { return mMaterialId; }
        int GetLayer() const
        { return mLayer; }
        float GetAlpha() const
        {return mAlpha; }
        float GetLineWidth() const
        { return mLineWidth; }
        float GetTimeScale() const
        { return mTimeScale; }
        bool TestFlag(Flags flag) const
        { return mBitFlags.test(flag); }
        RenderPass GetRenderPass() const
        { return mRenderPass; }
        RenderStyle GetRenderStyle() const
        { return mRenderStyle; }
        base::bitflag<Flags> GetFlags() const
        { return mBitFlags; }

        void IntoJson(nlohmann::json& json) const;

        static std::optional<DrawableItemClass> FromJson(const nlohmann::json& json);
    private:
        // item's bit flags.
        base::bitflag<Flags> mBitFlags;
        // class id of the material.
        std::string mMaterialId;
        // class id of the drawable shape.
        std::string mDrawableId;
        // the layer in which this node should be drawn.
        int mLayer = 0;
        // override alpha value, 0.0f = fully transparent, 1.0f = fully opaque.
        // only works with materials that enable alpha blending (transparency)
        float mAlpha = 1.0f;
        // linewidth for rasterizing the shape with lines.
        float mLineWidth = 1.0f;
        // scaler value for changing the time delta values
        // applied to the drawable (material)
        float mTimeScale = 1.0f;
        RenderPass mRenderPass = RenderPass::Draw;
        RenderStyle  mRenderStyle = RenderStyle::Solid;
    };

    // TextItem allows human readable text entity node attachment with
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
            UnderlineText
        };
        TextItemClass()
        {
            mBitFlags.set(Flags::VisibleInGame, true);
        }
        std::size_t GetHash() const;

        // class setters
        void SetText(const std::string& text)
        { mText = text; }
        void SetText(std::string&& text)
        { mText = std::move(text); }
        void SetFontName(const std::string& font)
        { mFontName = font; }
        void SetFontSize(unsigned size)
        { mFontSize = size; }
        void SetLayer(int layer)
        { mLayer = layer; }
        void SetLineHeight(float height)
        { mLineHeight = height; }
        void SetFlag(Flags flag, bool on_off)
        { mBitFlags.set(flag, on_off); }
        void SetAlign(VerticalTextAlign align)
        { mVAlign = align; }
        void SetAlign(HorizontalTextAlign align)
        { mHAlign = align; }
        void SetTextColor(const Color4f& color)
        { mTextColor = color; }

        // class getters
        bool TestFlag(Flags flag) const
        { return mBitFlags.test(flag); }
        const Color4f& GetTextColor() const
        { return mTextColor; }
        const std::string& GetText() const
        { return mText; }
        const std::string& GetFontName() const
        { return mFontName; }
        int GetLayer() const
        { return mLayer; }
        float GetLineHeight() const
        { return mLineHeight; }
        unsigned GetFontSize() const
        { return mFontSize; }
        base::bitflag<Flags> GetFlags() const
        { return mBitFlags; }
        HorizontalTextAlign GetHAlign() const
        { return mHAlign; }
        VerticalTextAlign GetVAlign() const
        { return mVAlign; }

        void IntoJson(nlohmann::json& json) const;

        static std::optional<TextItemClass> FromJson(const nlohmann::json& json);
    private:
        // item's bit flags
        base::bitflag<Flags> mBitFlags;
        HorizontalTextAlign mHAlign = HorizontalTextAlign::Center;
        VerticalTextAlign mVAlign = VerticalTextAlign::Center;
        int mLayer = 0;
        std::string mText;
        std::string mFontName;
        unsigned mFontSize = 0;
        float mLineHeight = 1.0f;
        Color4f mTextColor = Color::White;
    };


    class DrawableItem
    {
    public:
        using Flags       = DrawableItemClass::Flags;
        using RenderPass  = DrawableItemClass::RenderPass;
        using RenderStyle = DrawableItemClass::RenderStyle ;

        DrawableItem(std::shared_ptr<const DrawableItemClass> klass)
          : mClass(klass)
        {
            mInstanceAlpha = mClass->GetAlpha();
            mInstanceFlags = mClass->GetFlags();
            mInstanceTimeScale = mClass->GetTimeScale();
        }
        std::string GetMaterialId() const
        { return mClass->GetMaterialId(); }
        std::string GetDrawableId() const
        { return mClass->GetDrawableId(); }
        int GetLayer() const
        { return mClass->GetLayer(); }
        float GetLineWidth() const
        { return mClass->GetLineWidth(); }
        RenderPass GetRenderPass() const
        { return mClass->GetRenderPass(); }
        RenderStyle GetRenderStyle() const
        { return mClass->GetRenderStyle(); }
        bool TestFlag(Flags flag) const
        { return mInstanceFlags.test(flag); }
        float GetAlpha() const
        { return mInstanceAlpha; }
        float GetTimeScale() const
        { return mInstanceTimeScale; }

        void SetFlag(Flags flag, bool on_off)
        { mInstanceFlags.set(flag, on_off); }
        void SetAlpha(float alpha)
        { mInstanceAlpha = alpha; }
        void SetTimeScale(float scale)
        { mInstanceTimeScale = scale; }

        const DrawableItemClass& GetClass() const
        { return *mClass.get(); }
        const DrawableItemClass* operator->() const
        { return mClass.get(); }
    private:
        std::shared_ptr<const DrawableItemClass> mClass;
        base::bitflag<Flags> mInstanceFlags;
        float mInstanceAlpha = 1.0f;
        float mInstanceTimeScale = 1.0f;
    };

    class RigidBodyItem
    {
    public:
        using Simulation = RigidBodyItemClass::Simulation;
        using Flags      = RigidBodyItemClass::Flags;
        using CollisionShape = RigidBodyItemClass::CollisionShape;

        RigidBodyItem(std::shared_ptr<const RigidBodyItemClass> klass)
            : mClass(klass)
        {
            mLinearVelocity = mClass->GetLinearVelocity();
            mAngularVelocity = mClass->GetAngularVelocity();
            mInstanceFlags = mClass->GetFlags();
        }

        Simulation GetSimulation() const
        { return mClass->GetSimulation(); }
        CollisionShape GetCollisionShape() const
        { return mClass->GetCollisionShape(); }
        float GetFriction() const
        { return mClass->GetFriction(); }
        float GetRestitution() const
        { return mClass->GetRestitution(); }
        float GetAngularDamping() const
        { return mClass->GetAngularDamping(); }
        float GetLinearDamping() const
        { return mClass->GetLinearDamping(); }
        float GetDensity() const
        { return mClass->GetDensity(); }
        bool TestFlag(Flags flag) const
        { return mInstanceFlags.test(flag); }
        std::string GetPolygonShapeId() const
        { return mClass->GetPolygonShapeId(); }

        // Get the instantaneous current velocities of the
        // rigid body under the simulation.
        // linear velocity is expressed in meters per second
        // and angular velocity is radians per second.
        // ! The velocities are expressed in the world coordinate space !
        glm::vec2 GetLinearVelocity() const
        { return mLinearVelocity; }
        float GetAngularVelocity() const
        { return mAngularVelocity; }

        // Set the instantaneous current velocities of the
        // rigid body under the simulation.
        // linear velocity is expressed in meters per second
        // and angular velocity is radians per second.
        // ! The velocities are expressed in the world coordinate space !
        void SetLinearVelocity(const glm::vec2& velocity)
        { mLinearVelocity = velocity; }
        void SetAngularVelocity(float velocity)
        { mAngularVelocity = velocity; }
        void SetFlag(Flags flag, bool on_off)
        { mInstanceFlags.set(flag, on_off); }

        const RigidBodyItemClass& GetClass() const
        { return *mClass.get(); }
        const RigidBodyItemClass* operator->() const
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
        const Color4f& GetTextColor() const
        { return mTextColor; }
        const std::string& GetText() const
        { return mText; }
        const std::string& GetFontName() const
        { return mClass->GetFontName(); }
        unsigned GetFontSize() const
        { return mClass->GetFontSize(); }
        float GetLineHeight() const
        { return mClass->GetLineHeight(); }
        int GetLayer() const
        { return mClass->GetLayer(); }
        HorizontalTextAlign GetHAlign() const
        { return mClass->GetHAlign(); }
        VerticalTextAlign GetVAlign() const
        { return mClass->GetVAlign(); }
        bool TestFlag(Flags flag) const
        { return mFlags.test(flag); }
        std::size_t GetHash() const
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
        void SetText(std::string&& text)
        { mText = std::move(text); }
        void SetFlag(Flags flag, bool on_off)
        { mFlags.set(flag, on_off); }

        // class access
        const TextItemClass& GetClass() const
        { return *mClass.get(); }
        const TextItemClass* operator->() const
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

    class EntityNodeClass
    {
    public:
        using DrawableItemType = DrawableItemClass;

        enum class Flags {
            // Only pertains to editor (todo: maybe this flag should be removed)
            VisibleInEditor
        };

        EntityNodeClass()
        {
            mClassId = base::RandomString(10);
            mBitFlags.set(Flags::VisibleInEditor, true);
        }
        EntityNodeClass(const EntityNodeClass& other);
        EntityNodeClass(EntityNodeClass&& other);

        // Get the class id.
        std::string GetId() const
        { return mClassId; }
        // Get the human readable name for this class.
        std::string GetName() const
        { return mName; }
        // Get the hash value based on the class object properties.
        std::size_t GetHash() const;

        // Get the node's translation relative to its parent node.
        glm::vec2 GetTranslation() const
        { return mPosition; }
        // Get the node's scale factor. The scale factor applies to
        // whole hierarchy of nodes.
        glm::vec2 GetScale() const
        { return mScale; }
        // Get the node's box size.
        glm::vec2 GetSize() const
        { return mSize;}
        // Get node's rotation relative to its parent node.
        float GetRotation() const
        { return mRotation; }
        // Set the human readable node name.
        void SetName(const std::string& name)
        { mName = name; }
        // Set the node's scale. The scale applies to all of
        // the subsequent hierarchy, i.e. all the nodes that
        // are in the tree under this node.
        void SetScale(const glm::vec2& scale)
        { mScale = scale; }
        // Set the node's translation relative to the parent
        // of the this node.
        void SetTranslation(const glm::vec2& vec)
        { mPosition = vec; }
        // Set the node's containing box size.
        // The size is used to for example to figure out
        // the dimensions of rigid body collision shape (if any)
        // and to resize the drawable object.
        void SetSize(const glm::vec2& size)
        { mSize = size; }
        // Set the starting rotation in radians around the z axis.
        void SetRotation(float angle)
        { mRotation = angle;}
        void SetFlag(Flags flag, bool on_off)
        { mBitFlags.set(flag, on_off); }
        bool TestFlag(Flags flag) const
        { return mBitFlags.test(flag); }

        // Attach a rigid body to this node class.
        void SetRigidBody(const RigidBodyItemClass& body);
        // Attach a simple static drawable item to this node class.
        void SetDrawable(const DrawableItemClass& drawable);
        // Attach a text item to this node class.
        void SetTextItem(const TextItemClass& text);

        void RemoveDrawable()
        { mDrawable.reset(); }
        void RemoveRigidBody()
        { mRigidBody.reset(); }
        void RemoveTextItem()
        { mTextItem.reset(); }

        // Get the rigid body shared class object if any.
        std::shared_ptr<const RigidBodyItemClass> GetSharedRigidBody() const
        { return mRigidBody; }
        // Get the drawable shared class object if any.
        std::shared_ptr<const DrawableItemClass> GetSharedDrawable() const
        { return mDrawable; }
        // Get the text item class object if any.
        std::shared_ptr<const TextItemClass> GetSharedTextItem() const
        { return mTextItem; }

        // Returns true if a rigid body has been set for this class.
        bool HasRigidBody() const
        { return !!mRigidBody; }
        // Returns true if a drawable object has been set for this class.
        bool HasDrawable() const
        { return !!mDrawable; }
        bool HasTextItem() const
        { return !!mTextItem; }

        // Get the rigid body object if any. If no rigid body class object
        // has been set then returns nullptr.
        RigidBodyItemClass* GetRigidBody()
        { return mRigidBody.get(); }
        // Get the drawable shape object if any. If no drawable shape class object
        // has been set then returns nullptr.
        DrawableItemClass* GetDrawable()
        { return mDrawable.get(); }
        // Get the text item object if any. If no text item class object
        // has been set then returns nullptr:
        TextItemClass* GetTextItem()
        { return mTextItem.get(); }
        // Get the rigid body object if any. If no rigid body class object
        // has been set then returns nullptr.
        const RigidBodyItemClass* GetRigidBody() const
        { return mRigidBody.get(); }
        // Get the drawable shape object if any. If no drawable shape class object
        // has been set then returns nullptr.
        const DrawableItemClass* GetDrawable() const
        { return mDrawable.get(); }
        // Get the text item object if any. If no text item class object
        // has been set then returns nullptr:
        const TextItemClass* GetTextItem() const
        { return mTextItem.get(); }

        // Get the transform that applies to this node
        // and the subsequent hierarchy of nodes.
        glm::mat4 GetNodeTransform() const;
        // Get this drawable item's model transform that applies
        // to the node's box based items such as drawables
        // and rigid bodies.
        glm::mat4 GetModelTransform() const;

        int GetLayer() const
        { return mDrawable ? mDrawable->GetLayer() : 0; }

        void Update(float time, float dt);
        // Serialize the node into JSON.
        void IntoJson(nlohmann::json& json) const;
        // Load the node's properties from the given JSON object.
        static std::optional<EntityNodeClass> FromJson(const nlohmann::json& json);
        // Make a new unique copy of this node class object
        // with all the same properties but with a different/unique ID.
        EntityNodeClass Clone() const;

        EntityNodeClass& operator=(const EntityNodeClass& other);
    private:
        // the resource id.
        std::string mClassId;
        // human readable name of the class.
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
        // bitflags that apply to node.
        base::bitflag<Flags> mBitFlags;
    };


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
        void SetScale(const glm::vec2& scale)
        { mScale = scale; }
        void SetSize(const glm::vec2& size)
        { mSize = size; }
        void SetTranslation(const glm::vec2& pos)
        { mPosition = pos; }
        void SetRotation(float rotation)
        { mRotation = rotation; }
        void SetName(const std::string& name)
        { mName = name; }
        void Translate(const glm::vec2& vec)
        { mPosition += vec; }
        void Translate(float dx, float dy)
        { mPosition += glm::vec2(dx, dy); }
        void Rotate(float dr)
        { mRotation += dr; }

        // instance getters.
        std::string GetId() const
        { return mInstId; }
        std::string GetName() const
        { return mName; }
        glm::vec2 GetTranslation() const
        { return mPosition; }
        glm::vec2 GetScale() const
        { return mScale; }
        glm::vec2 GetSize() const
        { return mSize; }
        float GetRotation() const
        { return mRotation; }
        bool TestFlag(Flags flags) const
        { return mClass->TestFlag(flags); }

        // Get the node's drawable item if any. If no drawable
        // item is set then returns nullptr.
        DrawableItem* GetDrawable();
        // Get the node's rigid body item if any. If no rigid body
        // item is set then returns nullptr.
        RigidBodyItem* GetRigidBody();
        // Get the node's text item if any. If no text item
        // is set then returns nullptr.
        TextItem* GetTextItem();
        // Get the node's drawable item if any. If now drawable
        // item is set then returns nullptr.
        const DrawableItem* GetDrawable() const;
        // Get the node's rigid body item if any. If no rigid body
        // item is set then returns nullptr.
        const RigidBodyItem* GetRigidBody() const;
        // Get the node's text item if any. If no text item
        // is set then returns nullptr.
        const TextItem* GetTextItem() const;

        bool HasRigidBody() const
        { return !!mRigidBody; }
        bool HasDrawable() const
        { return !!mDrawable; }
        bool HasTextItem() const
        { return !!mTextItem; }

        // shortcut for class getters.
        std::string GetClassId() const
        { return mClass->GetId(); }
        std::string GetClassName() const
        { return mClass->GetName(); }
        int GetLayer() const
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

        const EntityNodeClass& GetClass() const
        { return *mClass.get(); }
        const EntityNodeClass* operator->() const
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
            // it's end of lifetime.
            KillAtLifetime
        };

        EntityClass()
        {
            mClassId = base::RandomString(10);
            mFlags.set(Flags::VisibleInEditor, true);
            mFlags.set(Flags::VisibleInGame, true);
            mFlags.set(Flags::LimitLifetime, false);
            mFlags.set(Flags::KillAtLifetime, true);
        }
        EntityClass(const EntityClass& other);

        // Add a new node to the entity.
        // Returns a pointer to the node that was added to the entity.
        EntityNodeClass* AddNode(const EntityNodeClass& node);
        EntityNodeClass* AddNode(EntityNodeClass&& node);
        EntityNodeClass* AddNode(std::unique_ptr<EntityNodeClass> node);

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

        // Add a new animation track class object. Returns a pointer to the node that
        // was added to the animation.
        AnimationTrackClass* AddAnimationTrack(AnimationTrackClass&& track);
        // Add a new animation track class object. Returns a pointer to the node that
        // was added to the animation.
        AnimationTrackClass* AddAnimationTrack(const AnimationTrackClass& track);
        // Add a new animation track class object. Returns a pointer to the node that
        // was added to the animation.
        AnimationTrackClass* AddAnimationTrack(std::unique_ptr<AnimationTrackClass> track);
        // Delete an animation track by the given index.
        void DeleteAnimationTrack(size_t i);
        // Delete an animation track by the given name.
        bool DeleteAnimationTrackByName(const std::string& name);
        // Delete an animation track by the given id.
        bool DeleteAnimationTrackById(const std::string& id);
        // Get the animation track class object by index.
        // The index must be valid.
        AnimationTrackClass& GetAnimationTrack(size_t i);
        // Find animation track class object by name. Returns nullptr if no such
        // track could be found.
        AnimationTrackClass* FindAnimationTrackByName(const std::string& name);
        // Get the animation track class object by index.
        // The index must be valid.
        const AnimationTrackClass& GetAnimationTrack(size_t i) const;
        // Find animation track class object by name. Returns nullptr if no such
        // track could be found.
        const AnimationTrackClass* FindAnimationTrackByName(const std::string& name) const;


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
        void CoarseHitTest(float x, float y, std::vector<const EntityNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in some node's (see EntityNode::FindNodeModelTransform) model space
        // into entity coordinate space.
        glm::vec2 MapCoordsFromNodeModel(float x, float y, const EntityNodeClass* node) const;
        // Map coordinates in entity coordinate space into some node's coordinate space.
        glm::vec2 MapCoordsToNodeModel(float x, float y, const EntityNodeClass* node) const;

        // Compute the axis aligned bounding rectangle for the whole entity.
        // i.e. including all the nodes at the current time.
        // This is a shortcut for getting the union of all the bounding rectangles
        // of all the entity nodes.
        FRect GetBoundingRect() const;

        // Compute the axis aligned bounding rectangle for the given node
        // at the current time.
        FRect FindNodeBoundingRect(const EntityNodeClass* node) const;

        // todo:
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
        ScriptVar* FindScriptVar(const std::string& name);
        // Get the scripting variable at the given index.
        // The index must be a valid index.
        const ScriptVar& GetScriptVar(size_t index) const;
        // Find a scripting variable with the given name. If no such variable
        // exists then nullptr is returned.
        const ScriptVar* FindScriptVar(const std::string& name) const;

        void SetLifetime(float value)
        { mLifetime = value;}
        void SetFlag(Flags flag, bool on_off)
        { mFlags.set(flag, on_off); }
        void SetName(const std::string& name)
        { mName = name; }
        void SetIdleTrackId(const std::string& id)
        { mIdleTrackId = id; }
        void SetSriptFileId(const std::string& file)
        { mScriptFile = file; }
        void ResetIdleTrack()
        { mIdleTrackId.clear(); }
        void ResetScriptFile()
        { mScriptFile.clear(); }
        bool HasIdleTrack() const
        { return !mIdleTrackId.empty(); }
        bool HasScriptFile() const
        { return !mScriptFile.empty(); }
        bool TestFlag(Flags flag) const
        { return mFlags.test(flag); }

        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }

        std::size_t GetHash() const;
        std::size_t GetNumNodes() const
        { return mNodes.size(); }
        std::size_t GetNumTracks() const
        { return mAnimationTracks.size(); }
        std::size_t GetNumScriptVars() const
        { return mScriptVars.size(); }
        std::string GetId() const
        { return mClassId; }
        std::string GetIdleTrackId() const
        { return mIdleTrackId; }
        std::string GetName() const
        { return mName; }
        std::string GetScriptFileId() const
        { return mScriptFile; }
        float GetLifetime() const
        { return mLifetime; }
        base::bitflag<Flags> GetFlags() const
        { return mFlags; }

        std::shared_ptr<const EntityNodeClass> GetSharedEntityNodeClass(size_t index) const
        { return mNodes[index]; }
        std::shared_ptr<const AnimationTrackClass> GetSharedAnimationTrackClass(size_t index) const
        { return mAnimationTracks[index]; }
        std::shared_ptr<const ScriptVar> GetSharedScriptVar(size_t index) const
        { return mScriptVars[index]; }

        // Serialize the entity into JSON.
        nlohmann::json ToJson() const;

        static std::optional<EntityClass> FromJson(const nlohmann::json& json);

        EntityClass Clone() const;

        EntityClass& operator=(const EntityClass& other);
    private:
        // The class/resource id of this class.
        std::string mClassId;
        // the human readable name of the class.
        std::string mName;
        // the track ID of the idle track that gets played when nothing
        // else is going on. can be empty in which case no animation plays.
        std::string mIdleTrackId;
        // the list of animation tracks that are pre-defined with this
        // type of animation.
        std::vector<std::shared_ptr<AnimationTrackClass>> mAnimationTracks;
        // the list of nodes that belong to this entity.
        std::vector<std::shared_ptr<EntityNodeClass>> mNodes;
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
        // not work correctly. therefore it's important to use the 
        // scaling factor here to set the scale when creating a new 
        // entity.
        glm::vec2  scale    = {1.0f, 1.0f};
        // the entity position relative to parent.
        glm::vec2  position = {0.0f, 0.0f};
        // the entity rotation relative to parent
        float      rotation = 0.0f;
        EntityArgs() {
            id = base::RandomString(10);
        }
    };

    class Entity
    {
    public:
        // Runtime management flags
        enum class ControlFlags {
            // The entity has been killed and will be
            // removed at the end of the update cycle
            Killed
        };
        using Flags = EntityClass::Flags;

        using RenderTree      = game::RenderTree<EntityNode>;
        using RenderTreeNode  = EntityNode;
        using RenderTreeValue = EntityNode;

        // Construct a new entity with the initial state based
        // on the entity class object's state.
        Entity(const EntityArgs& args);
        Entity(std::shared_ptr<const EntityClass> klass);
        Entity(const EntityClass& klass);
        Entity(const Entity& other) = delete;

        // Add a new node to the entity. Note that this doesn't yet insert the
        // node into the render tree. You can either use the render tree directly
        // to find a place where to insert the node or then use some of the
        // provided functions such as LinkChild.
        // The return value is the pointer of the new node that exists in the entity
        // after the call returns.
        EntityNode* AddNode(const EntityNode& node);
        EntityNode* AddNode(EntityNode&& node);
        EntityNode* AddNode(std::unique_ptr<EntityNode> node);

        // Link the given child node with the parent.
        // The parent may be a nullptr in which case the child
        // is added to the root of the entity. The child node needs
        // to be a valid node and needs to point to node that is not
        // yet any part of the render tree and is a node that belongs
        // to this entity.
        void LinkChild(EntityNode* parent, EntityNode* child);

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
        // Find a entity node by it's instance name. Returns nullptr if no such node could be found.
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
        // Find a entity node by it's instance name. Returns nullptr if no such node could be found.
        const EntityNode* FindNodeByInstanceName(const std::string& name) const;
        // Delete the node at the given index. This will also delete any
        // child nodes this node might have by recursing the render tree.
        void DeleteNode(EntityNode* node);

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node's box in the entity.
        // The testing is coarse in the sense that it's done against the node's
        // size box only. The hit nodes are stored in the hits vector and the
        // positions with the nodes' hitboxes are (optionally) stored in the
        // hitbox_positions vector.
        void CoarseHitTest(float x, float y, std::vector<EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(float x, float y, std::vector<const EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in some EntityNode's (see EntityNode::FindNodeModelTransform) model space
        // into entity coordinate space.
        glm::vec2 MapCoordsFromNodeModel(float x, float y, const EntityNode* node) const;
        // Map coordinates in entity coordinate space into some EntityNode's coordinate space.
        glm::vec2 MapCoordsToNodeModel(float x, float y, const EntityNode* node) const;

        // Compute the axis aligned bounding rectangle for the whole entity
        // i.e. including all the nodes at the current time of entity.
        // This is a shortcut for getting the union of all the bounding rectangles
        // of all the entity nodes.
        FRect GetBoundingRect() const;

        // Compute the axis aligned bounding rectangle for the give entity node
        // at the current time of the entity.
        FRect FindNodeBoundingRect(const EntityNode* node) const;

        // todo:
        FBox FindNodeBoundingBox(const EntityNode* node) const;

        // todo:
        glm::mat4 FindNodeTransform(const EntityNode* node) const;
        glm::mat4 FindNodeModelTransform(const EntityNode* node) const;

        void Update(float dt);

        // Play the given animation track.
        void Play(std::unique_ptr<AnimationTrack> track);
        void Play(const AnimationTrack& track)
        { Play(std::make_unique<AnimationTrack>(track)); }
        void Play(AnimationTrack&& track)
        { Play(std::make_unique<AnimationTrack>(std::move(track))); }
        // Play a previously recorded (stored in the animation class object)
        // animation track identified by name. Note that there could be
        // ambiguity between the names, i.e. multiple tracks with the same name.
        // Returns true if playback started or false when there's no such track.
        bool PlayAnimationByName(const std::string& name);
        // Play a previously recorded (stored in the animation class object)
        // animation track identified by its track id.
        // Returns true if playback started or false when there's no such track.
        bool PlayAnimationById(const std::string& id);
        // Play the designated idle track if any and if there's no current animation.
        bool PlayIdle();
        // Returns true if an animation track is still playing.
        bool IsPlaying() const;
        // Returns true if the lifetime has been exceeded.
        bool HasExpired() const;
        // Returns true if the kill control flag has been set.
        bool HasBeenKilled() const;

        // Find a scripting variable.
        // Returns nullptr if there was no variable by this name.
        // Note that the const here only implies that the object
        // may not change in terms of c++ semantics. The actual *value*
        // can still be changed as long as the variable is not read only.
        const ScriptVar* FindScriptVar(const std::string& name) const;

        void SetFlag(ControlFlags flag, bool on_off)
        { mControlFlags.set(flag, on_off); }
        void SetFlag(Flags flag, bool on_off)
        { mFlags.set(flag, on_off); }
        void SetParentNodeClassId(const std::string& id)
        { mParentNodeId = id; }
        void SetIdleTrackId(const std::string& id)
        { mIdleTrackId = id; }
        void SetLayer(int layer)
        { mLayer = layer; }

        // Get the current track if any. (when IsPlaying is true)
        AnimationTrack* GetCurrentTrack()
        { return mAnimationTrack.get(); }
        const AnimationTrack* GetCurrentTrack() const
        { return mAnimationTrack.get(); }

        double GetTime() const
        { return mCurrentTime; }
        std::string GetIdleTrackId() const
        { return mIdleTrackId; }
        std::string GetParentNodeClassId() const
        { return mParentNodeId; }
        std::string GetClassId() const
        { return mClass->GetId(); }
        std::string GetClassName() const
        { return mClass->GetName(); }
        std::size_t GetNumNodes() const
        { return mNodes.size(); }
        std::string GetName() const
        { return mInstanceName; }
        std::string GetId() const
        { return mInstanceId; }
        int GetLayer() const
        { return mLayer; }
        bool TestFlag(ControlFlags flag) const
        { return mControlFlags.test(flag); }
        bool TestFlag(Flags flag) const
        { return mFlags.test(flag); }
        bool HasIdleTrack() const
        { return !mIdleTrackId.empty() || mClass->HasIdleTrack(); }
        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }
        const EntityClass& GetClass() const
        { return *mClass.get(); }
        const EntityClass* operator->() const
        { return mClass.get(); }
        Entity& operator=(const Entity&) = delete;
    private:
        // the class object.
        std::shared_ptr<const EntityClass> mClass;
        // The entity instance id.
        std::string mInstanceId;
        // the entity instance name (if any)
        std::string mInstanceName;
        // When the entity is linked (parented)
        // to another entity this id is the node in the parent
        // entity's render tree that is to be used as the parent
        // of this entity's nodes.
        std::string mParentNodeId;
        // The current animation track if any.
        std::unique_ptr<AnimationTrack> mAnimationTrack;
        // the list of nodes that are in the entity.
        std::vector<std::unique_ptr<EntityNode>> mNodes;
        // the list of script variables. read-only ones can
        // be shared between all instances and the EntityClass.
        std::vector<ScriptVar> mScriptVars;
        // the render tree for hierarchical traversal and transformation
        // of the entity and its nodes.
        RenderTree mRenderTree;
        // Current entity time.
        double mCurrentTime = 0.0;
        // the render layer index.
        int mLayer = 0;
        // entity bit flags
        base::bitflag<Flags> mFlags;
        // id of the idle animation track.
        std::string mIdleTrackId;
        // control flags for the engine itself
        base::bitflag<ControlFlags> mControlFlags;
    };

    std::unique_ptr<Entity> CreateEntityInstance(std::shared_ptr<const EntityClass> klass);
    std::unique_ptr<Entity> CreateEntityInstance(const EntityClass& klass);
    std::unique_ptr<Entity> CreateEntityInstance(const EntityArgs& args);
    std::unique_ptr<EntityNode> CreateEntityNodeInstance(std::shared_ptr<const EntityNodeClass> klass);

} // namespace
