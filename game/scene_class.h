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

#include "config.h"

#pragma once

#include "warnpush.h"
#  include <glm/mat4x4.hpp>
#  include <glm/vec2.hpp>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <string>
#include <optional>
#include <memory>
#include <variant>
#include <vector>

#include "base/utility.h"
#include "data/fwd.h"
#include "game/tree.h"
#include "game/scriptvar.h"
#include "game/entity_placement.h"

namespace game
{
        // SceneClass provides the initial structure of the scene with
    // initial placement of entities etc.
    class SceneClass
    {
    public:
        using RenderTree      = game::RenderTree<EntityPlacement>;
        using RenderTreeNode  = EntityPlacement;
        using RenderTreeValue = EntityPlacement;

        struct BloomFilter {
            float threshold = 0.98f;
            float red   = 0.2126f;
            float green = 0.7252f;
            float blue  = 0.0722f;
        };

        struct Fog {
            enum class Mode {
                Linear, Exp1, Exp2
            };

            base::Color4f color;
            float start_dist = 10.0f;
            float end_dist   = 100.0f;
            float density    = 1.0f;
            Mode mode = Mode::Linear;
        };

        struct RenderingArgs {
            enum class ShadingMode {
                Flat, BasicLight
            };
            ShadingMode shading = ShadingMode::Flat;
            std::optional<BloomFilter> bloom;
            std::optional<Fog> fog;
        };

        enum class SpatialIndex {
            Disabled,
            QuadTree,
            DenseGrid
        };
        struct QuadTreeArgs {
            unsigned max_items = 4;
            unsigned max_levels = 3;
        };
        struct DenseGridArgs {
            unsigned num_rows = 1;
            unsigned num_cols = 1;
        };
        using SpatialIndexArgs = std::variant<QuadTreeArgs, DenseGridArgs>;

        // for testing purposes, make it easier to create a scene class with
        // a known ID
        explicit SceneClass(std::string id);

        SceneClass();
        // Copy construct a deep copy of the scene.
        SceneClass(const SceneClass& other);

        // Place a new entity into the scene.
        // Returns a pointer to the placement that was added to the scene.
        // Note that the entity is not yet added to the scene graph
        // and as such will not be considered for rendering etc.
        // You probably want to link the node to some other node.
        // See LinkChild.
        EntityPlacement* PlaceEntity(const EntityPlacement& placement);
        EntityPlacement* PlaceEntity(EntityPlacement&& placement);
        EntityPlacement* PlaceEntity(std::unique_ptr<EntityPlacement> placement);

        // Get the entity placement by index. The index must be valid.
        EntityPlacement& GetPlacement(size_t index);
        // Find scene node by name. Returns nullptr if
        // no such node could be found.
        EntityPlacement* FindPlacementByName(const std::string& name);
        // Find scene node by id. Returns nullptr if
        // no such node could be found.
        EntityPlacement* FindPlacementById(const std::string& id);
        // Get the entity placement by index. The index must be valid.
        const EntityPlacement& GetPlacement(size_t index) const;
        // Find scene node by class name. Returns nullptr if
        // no such node could be found.
        const EntityPlacement* FindPlacementByName(const std::string& name) const;
        // Find scene node by class id. Returns nullptr if
        // no such node could be found.
        const EntityPlacement* FindPlacementById(const std::string& id) const;

        // Link the given child node with the parent.
        // The parent may be a nullptr in which case the child
        // is added to the root of the entity. The child node needs
        // to be a valid node and needs to point to node that is not
        // yet any part of the render tree and is a node that belongs
        // to this entity class object.
        void LinkChild(EntityPlacement* parent, EntityPlacement* child);
        // Break a child node away from its parent. The child node needs
        // to be a valid node and needs to point to a node that is added
        // to the render tree and belongs to this scene class object.
        // The child (and all of its children) that has been broken still
        // exists in the entity but is removed from the render tree.
        // You can then either DeletePlacement to completely delete it or
        // LinkChild to insert it into another part of the render tree.
        void BreakChild(EntityPlacement* child, bool keep_world_transform = true);
        // Re-parent a child node from its current parent to another parent.
        // Both the child node and the parent node to be a valid nodes and
        // need to point to nodes that are part of the render tree and belong
        // to this entity class object. This will move the whole hierarchy of
        // nodes starting from child under the new parent.
        // If keep_world_transform is true the child will be transformed such
        // that it's current world transformation remains the same. I.e  it's
        // position and rotation in the world don't change.
        void ReparentChild(EntityPlacement* parent, EntityPlacement* child, bool keep_world_transform = true);

        // Delete a placement from the scene. The given placement and all of its
        // children will be removed from the scene graph and then deleted.
        void DeletePlacement(EntityPlacement* placement);

        // Duplicate an entire placement hierarchy starting at the given placement,
        // and add the resulting hierarchy to node's parent.
        // Returns the root node of the new placement hierarchy.
        EntityPlacement* DuplicatePlacement(const EntityPlacement* placement);

        // Value aggregate for scene nodes that represent placement
        // of entities in the scene.
        struct ConstSceneNode {
            // The transform matrix that applies to this entity (node)
            // in order to transform it to the scene.
            glm::mat4 node_to_scene;
            // The entity representation in the scene.
            std::shared_ptr<const EntityClass> entity;
            // The entity object in the scene.
            const EntityPlacement* placement = nullptr;
        };
        // Collect nodes from the scene into a flat list.
        std::vector<ConstSceneNode> CollectNodes() const;

        // Value aggregate for the nodes that represent placement
        // of entities in the scene.
        struct SceneNode {
            // The transform matrix that applies to this entity (node)
            // in order to transform it to the scene.
            glm::mat4 node_to_scene;
            // The entity representation in the scene.
            std::shared_ptr<const EntityClass> entity;
            // The entity object in the scene.
            EntityPlacement* placement = nullptr;
        };
        // Collect nodes from the scene into a flat list.
        std::vector<SceneNode> CollectNodes();

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node in the scene.
        // The testing is coarse in the sense that it's done against the node's
        // size box only. The hit nodes are stored in the hits vector and the
        // positions with the nodes' hitboxes are (optionally) stored in the
        // hitbox_positions vector.
        void CoarseHitTest(const Float2& point, std::vector<EntityPlacement*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(const Float2& point, std::vector<const EntityPlacement*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in node's OOB space into entity coordinate space. The origin of
        // the OOB space is relative to the "TopLeft" corner of the OOB of the node.
        Float2 MapCoordsFromNodeBox(const Float2& coordinates, const EntityPlacement* node) const;
        // Map coordinates in scene coordinate space into node's OOB coordinate space.
        Float2 MapCoordsToNodeBox(const Float2& coordinates, const EntityPlacement* node) const;

        glm::mat4 FindEntityTransform(const EntityPlacement* placement) const;

        FRect FindEntityBoundingRect(const EntityPlacement* placement) const;

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
        ScriptVar* FindScriptVarById(const std::string& id);
        // Get the scripting variable at the given index.
        // The index must be a valid index.
        const ScriptVar& GetScriptVar(size_t index) const;
        // Find a scripting variable with the given name. If no such variable
        // exists then nullptr is returned.
        const ScriptVar* FindScriptVarByName(const std::string& name) const;
        const ScriptVar* FindScriptVarById(const std::string& id) const;

        // Get the object hash value based on the property values.
        std::size_t GetHash() const;
        // Return number of scene nodes contained in the scene.
        std::size_t GetNumNodes() const noexcept
        { return mNodes.size();}
        std::size_t GetNumScriptVars() const noexcept
        { return mScriptVars.size(); }
        // Get the scene class object id.
        const std::string& GetId() const noexcept
        { return mClassId; }
        // Get the human-readable name of the class.
        const std::string& GetName() const noexcept
        { return mName; }
        const std::string& GetClassName() const noexcept
        { return mName; }
        // Get the associated script file ID.
        const std::string& GetScriptFileId() const noexcept
        { return mScriptFile; }
        const std::string& GetTilemapId() const noexcept
        { return mTilemap; }
        // Get the scene's render tree (scene graph). The render tree defines
        // the relative transformations and the transformation hierarchy of the
        // scene class nodes in the scene.
        RenderTree& GetRenderTree() noexcept
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const noexcept
        { return mRenderTree; }
        SpatialIndex GetDynamicSpatialIndex() const noexcept
        { return mDynamicSpatialIndex; }
        const QuadTreeArgs* GetQuadTreeArgs() const;
        const DenseGridArgs* GetDenseGridArgs() const;
        const float* GetLeftBoundary() const;
        const float* GetRightBoundary() const;
        const float* GetTopBoundary() const;
        const float* GetBottomBoundary() const;

        void SetDynamicSpatialIndex(SpatialIndex index);
        void SetDynamicSpatialIndexArgs(const DenseGridArgs& args);
        void SetDynamicSpatialIndexArgs(const QuadTreeArgs& args);

        void SetBloom(const BloomFilter& bloom) noexcept
        { mRenderingArgs.bloom = bloom; }
        const BloomFilter* GetBloom() const noexcept
        { return base::GetOpt(mRenderingArgs.bloom); }
        BloomFilter* GetBloom() noexcept
        { return base::GetOpt(mRenderingArgs.bloom); }
        void ResetBloom() noexcept
        { mRenderingArgs.bloom.reset(); }

        void SetFog(const Fog& fog) noexcept
        { mRenderingArgs.fog = fog; }
        const Fog* GetFog() const noexcept
        { return base::GetOpt(mRenderingArgs.fog); }
        Fog* GetFog() noexcept
        { return base::GetOpt(mRenderingArgs.fog); }
        void ResetFog() noexcept
        { mRenderingArgs.fog.reset(); }
        void SetShadingMode(RenderingArgs::ShadingMode shading) noexcept
        { mRenderingArgs.shading = shading; }
        auto GetShadingMode() const noexcept
        { return mRenderingArgs.shading; }

        void SetLeftBoundary(float value)  noexcept
        { mLeftBoundary = value; }
        void SetRightBoundary(float value) noexcept
        { mRightBoundary = value; }
        void SetTopBoundary(float value) noexcept
        { mTopBoundary = value; }
        void SetBottomBoundary(float value) noexcept
        { mBottomBoundary = value; }
        void SetName(const std::string& name) noexcept
        { mName = name; }
        void SetScriptFileId(const std::string& file)
        { mScriptFile = file; }
        void SetTilemapId(const std::string& map)
        { mTilemap = map; }
        bool HasScriptFile() const noexcept
        { return !mScriptFile.empty(); }
        bool HasTilemap() const noexcept
        { return !mTilemap.empty(); }
        bool IsDynamicSpatialIndexEnabled() const noexcept
        { return mDynamicSpatialIndex != SpatialIndex::Disabled; }
        bool HasLeftBoundary() const noexcept
        { return mLeftBoundary.has_value(); }
        bool HasRightBoundary() const noexcept
        { return mRightBoundary.has_value(); }
        bool HasTopBoundary() const noexcept
        { return mTopBoundary.has_value(); }
        bool HasBottomBoundary() const noexcept
        { return mBottomBoundary.has_value(); }
        void ResetScriptFile() noexcept
        { mScriptFile.clear(); }
        void ResetTilemap() noexcept
        { mTilemap.clear(); }
        void ResetLeftBoundary() noexcept
        { mLeftBoundary.reset(); }
        void ResetRightBoundary() noexcept
        { mRightBoundary.reset(); }
        void ResetTopBoundary() noexcept
        { mTopBoundary.reset(); }
        void ResetBottomBoundary() noexcept
        { mBottomBoundary.reset(); }

        // Serialize the scene into JSON.
        void IntoJson(data::Writer& data) const;
        // Load the SceneClass from JSON
        bool FromJson(const data::Reader& data);

        // Make a clone of this scene. The cloned scene will
        // have all the same property values as its source
        // but a unique class id.
        SceneClass Clone() const;

        // Make a deep copy on assignment.
        SceneClass& operator=(const SceneClass& other);

    private:
        // the class / resource of this class.
        std::string mClassId;
        // the human-readable name of the class.
        std::string mName;
        // the ID of the associated script file (if any)
        std::string mScriptFile;
        // the ID of the associated tilemap (if any)
        std::string mTilemap;
        // storing by unique ptr so that the pointers
        // given to the render tree don't become invalid
        // when new nodes are added to the scene.
        std::vector<std::unique_ptr<EntityPlacement>> mNodes;
        // scene-graph / render tree for hierarchical traversal
        // and transformation of the scene nodes. the tree defines
        // the parent-child transformation hierarchy.
        RenderTree mRenderTree;
        // Scripting variables.
        std::vector<ScriptVar> mScriptVars;
        // Dynamic spatial index (for entity nodes with spatial nodes) setting.
        SpatialIndex mDynamicSpatialIndex = SpatialIndex::Disabled;
        // Spatial index type specific arguments for the data structure.
        std::optional<SpatialIndexArgs> mDynamicSpatialIndexArgs;
        // Bounds of the scene if any.
        std::optional<float> mLeftBoundary;
        std::optional<float> mRightBoundary;
        std::optional<float> mTopBoundary;
        std::optional<float> mBottomBoundary;

        RenderingArgs mRenderingArgs;
    };

} // namespace game