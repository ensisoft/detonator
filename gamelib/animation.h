// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/mat4x4.hpp>
#  include <glm/vec2.hpp>
#  include <glm/glm.hpp>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include "base/assert.h"
#include "base/bitflag.h"
#include "base/utility.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "tree.h"

namespace gfx {
    class Drawable;
    class Material;
    class Painter;
    class Transform;
} // namespace

namespace game
{
    class GfxFactory;

    class AnimationNode
    {
    public:
        enum class RenderPass {
            Draw,
            Mask
        };
        using RenderStyle = gfx::Drawable::Style;

        enum class Flags {
            // Only pertains to editor (todo: maybe this flag should be removed)
            VisibleInEditor,
            // Whether the node should generate render packets or not.
            DoesRender,
            // Whether the node should update material or not
            UpdateMaterial,
            // Whether the node should update drawable or not
            UpdateDrawable,
        };

        // This will construct an "empty" node that cannot be
        // drawn yet. You'll need to se the various properties
        // for drawable/material etc.
        AnimationNode();

        // Set the drawable object (shape) for this component.
        // The name identifies the resource in the gfx resource loader.
        void SetDrawable(const std::string& name, std::shared_ptr<gfx::Drawable> drawable)
        {
            mDrawable = std::move(drawable);
            mDrawableName = name;
            if (mDrawable)
            {
                mDrawable->SetLineWidth(mLineWidth);
                mDrawable->SetStyle(mRenderStyle);
            }
        }
        // Set the material object for this component.
        // The name identifies the runtime material resource in the gfx resource loader.
        void SetMaterial(const std::string& name, std::shared_ptr<gfx::Material> material)
        {
            mMaterial = std::move(material);
            mMaterialName = name;
        }
        void SetTranslation(const glm::vec2& pos)
        { mPosition = pos; }
        void SetName(const std::string& name)
        { mName = name;}
        void SetScale(const glm::vec2& scale)
        { mScale = scale; }
        void SetSize(const glm::vec2& size)
        { mSize = size; }
        void SetLayer(int layer)
        { mLayer = layer; }
        void SetRenderPass(RenderPass pass)
        { mRenderPass = pass; }
        void SetRotation(float value)
        { mRotation = value; }
        void SetRenderStyle(RenderStyle style)
        {
            mRenderStyle = style;
            if (mDrawable)
                mDrawable->SetStyle(mRenderStyle);
        }
        void SetLineWidth(float value)
        {
            mLineWidth = value;
            if (mDrawable)
                mDrawable->SetLineWidth(mLineWidth);
        }
        void SetFlag(Flags f, bool on_off)
        { mBitFlags.set(f, on_off); }
        bool TestFlag(Flags f) const
        { return mBitFlags.test(f); }

        RenderPass GetRenderPass() const
        { return mRenderPass; }
        RenderStyle GetRenderStyle() const
        { return mRenderStyle; }
        int GetLayer() const
        { return mLayer; }
        std::string GetId() const
        { return mId; }
        std::string GetName() const
        { return mName; }
        std::shared_ptr<gfx::Material> GetMaterial()
        { return mMaterial; }
        std::shared_ptr<const gfx::Material> GetMaterial() const
        { return mMaterial; }
        std::string GetMaterialName() const
        { return mMaterialName; }
        std::shared_ptr<gfx::Drawable> GetDrawable()
        { return mDrawable; }
        std::shared_ptr<const gfx::Drawable> GetDrawable() const
        { return mDrawable; }
        std::string GetDrawableName() const
        { return mDrawableName; }
        glm::vec2 GetTranslation() const
        { return mPosition; }
        glm::vec2 GetSize() const
        { return mSize; }
        float GetRotation() const
        { return mRotation; }
        float GetLineWidth() const
        { return mLineWidth; }
        const base::bitflag<Flags>& GetFlags() const
        { return mBitFlags; }
        base::bitflag<Flags>& GetFlags()
        { return mBitFlags; }

        // Update node, i.e. its material and drawable if the relevant
        // update flags are set.
        void Update(float dt);

        // Reset node's accumulated time to 0.
        void ResetTime();

        // Get this node's transformation that applies
        // to the hieararchy of nodes.
        glm::mat4 GetNodeTransform() const;

        // Get this node's transformation that applies to this
        // node's drawable object(s).
        glm::mat4 GetModelTransform() const;

        // Prepare this animation component for rendering
        // by loading all the needed runtime resources.
        void Prepare(const GfxFactory& loader);

        // serialize the component properties into JSON.
        nlohmann::json ToJson() const;

        // Load a component and it's properties from a JSON object.
        // Note that this does not yet create/load any runtime objects
        // such as materials or such but they are loaded later when the
        // component is prepared.
        static std::optional<AnimationNode> FromJson(const nlohmann::json& object);
    private:
        // generic properties.
        std::string mId;
        // this is the human readable name and can be used when for example
        // programmatically looking up an animation node.
        std::string mName;
        // visual properties. we keep the material/drawable names
        // around so that we we know which resources to load at runtime.
        std::string mMaterialName;
        std::string mDrawableName;
        std::shared_ptr<gfx::Material> mMaterial;
        std::shared_ptr<gfx::Drawable> mDrawable;
        // timewise properties.
        float mLifetime  = 0.0f;
        float mStartTime = 0.0f;
        float mTime      = 0.0f;
        // transformation properties.
        // translation offset relative to the animation.
        glm::vec2 mPosition;
        // size is the size of this object in some units
        // (for example pixels)
        glm::vec2 mSize = {1.0f, 1.0f};
        // scale applies an additional scale to this hiearchy.
        glm::vec2 mScale = {1.0f, 1.0f};
        // rotation around z axis positive rotation is CW
        float mRotation = 0.0f;
        // rendering properties. which layer and wich pass.
        int mLayer = 0;
        RenderPass mRenderPass = RenderPass::Draw;
        RenderStyle mRenderStyle = RenderStyle::Solid;
        // applies only when RenderStyle is either Outline or Wireframe
        float mLineWidth = 1.0f;
        // bitflags that apply to node.
        base::bitflag<Flags> mBitFlags;
    };


    class Animation
    {
    public:
        using RenderTree = TreeNode<AnimationNode>;
        using RenderTreeNode = TreeNode<AnimationNode>;

        Animation() = default;
        Animation(const Animation& other);

        struct DrawPacket {
            // shortcut to the node's material.
            std::shared_ptr<const gfx::Material> material;
            // shortcut to the node's drawable.
            std::shared_ptr<const gfx::Drawable> drawable;
            // transform that pertains to the draw.
            glm::mat4 transform;
            // the animation layer this draw belongs to.
            int layer = 0;
            // the render pass this draw belongs to.
            AnimationNode::RenderPass pass = AnimationNode::RenderPass::Draw;
        };

        class DrawHook
        {
        public:
            virtual ~DrawHook() = default;
            // This is a hook function to inspect and  modify the the draw packet produced by the
            // given animation node. The return value can be used to indicate filtering.
            // If the function returns false thepacket is dropped. Otherwise it's added to the
            // current drawlist with any possible modifications.
            virtual bool InspectPacket(const AnimationNode* node, DrawPacket& packet) { return true; }
            // This is a hook function to append extra draw packets to the current drawlist
            // based on the node.
            // Transform is the combined transformation hierarchy containing the transformations
            // from this current node to "view".
            virtual void AppendPackets(const AnimationNode* node, gfx::Transform& trans, std::vector<DrawPacket>& packets) {}
        protected:
        };

        // Draw the animation and its components.
        // Each component is transformed relative to the parent transformation "trans".
        // Optional draw hook can be used to modify the draw packets before submission to the
        // paint device.
        void Draw(gfx::Painter& painter, gfx::Transform& trans, DrawHook* hook = nullptr) const;

        // Update the animation and it's nodes.
        // Triggers the actions and events specified on the timeline.
        void Update(float dt);

        // Add a new animation node. Returns pointer to the node
        // that was added to the animation.
        AnimationNode* AddNode(AnimationNode&& node);

        // Add a new animation node. Returns a pointer to the node
        // that was added to the anímation.
        AnimationNode* AddNode(const AnimationNode& node);

        // Delete a node by the given index.
        void DeleteNodeByIndex(size_t i);

        // Delete a node by the given id. THe node is expected to exist.
        void DeleteNodeById(const std::string& id);

        AnimationNode& GetNode(size_t i)
        {
            ASSERT(i < mNodes.size());
            return *mNodes[i];
        }
        const AnimationNode& GetNode(size_t i) const
        {
            ASSERT(i < mNodes.size());
            return *mNodes[i];
        }
        std::size_t GetNumNodes() const
        { return mNodes.size(); }
        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node's drawable in the animation.
        // The testing is coarse in the sense that it's done against the node's
        // drawable shapes size box and ignores things such as transparency.
        // The hit nodes are stored in the hits vector and the positions with the
        // nodes' hitboxes are (optionally) strored in the hitbox_positions vector.
        void CoarseHitTest(float x, float y, std::vector<AnimationNode*>* hits,
            std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(float x, float y, std::vector<const AnimationNode*>* hits,
            std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in some AnimationNode's (see AnimationNode::GetNodeTransform) space
        // into animation coordinate space.
        glm::vec2 MapCoordsFromNode(float x, float y, const AnimationNode* node) const;
        // Map coordinates in animation coordinate space into some AnimationNode's coordinate space.
        glm::vec2 MapCoordsToNode(float x, float y, const AnimationNode* node) const;

        // Reset the state of the animation to initial state.
        void Reset();

        // Prepare and load the runtime resources if not yet loaded.
        void Prepare(const GfxFactory& loader);

        // Serialize the animation into JSON.
        nlohmann::json ToJson() const;

        // Lookup a AnimationNode based on the serialized ID in the JSON.
        AnimationNode* TreeNodeFromJson(const nlohmann::json& json);

        // Create an animation object based on the JSON. Returns nullopt if
        // deserialization failed.
        static std::optional<Animation> FromJson(const nlohmann::json& object);

        // Serialize an animation node contained in the RenderTree to JSON by doing
        // a shallow (id only) based serialization.
        // Later in TreeNodeFromJson when reading back the render tree we simply
        // look up the node based on the ID.
        static nlohmann::json TreeNodeToJson(const AnimationNode* node);

        Animation& operator=(const Animation& other);

    private:
        // The list of components that are to be drawn as part
        // of the animation. Each component has a unique transform
        // relative to the animation.
        // we're allocating the AnimationNods on the free store
        // so that the pointers remain valid even if the vector is resized
        std::vector<std::unique_ptr<AnimationNode>> mNodes;
        // scenegraph / render tree for hierarchical
        // traversal and transformation of the animation nodes.
        // i.e. the tree defines the parent-child transformation
        // hiearchy.
        RenderTree mRenderTree;
    };
} // namespace
