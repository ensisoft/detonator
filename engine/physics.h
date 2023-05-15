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
#  include <glm/glm.hpp>
#  include <box2d/box2d.h>
#include "warnpop.h"

#include <string>
#include <memory>
#include <vector>
#include <tuple>
#include <unordered_map>

#include "graphics/fwd.h"
#include "game/fwd.h"

class b2World;

namespace engine
{
    class ClassLibrary;

    struct ContactEvent {
        enum class Type {
            BeginContact,
            EndContact
        };
        Type type = Type::BeginContact;
        game::EntityNode* nodeA = nullptr;
        game::EntityNode* nodeB = nullptr;
    };

    class PhysicsEngine
    {
    public:
        PhysicsEngine(const ClassLibrary* classlib = nullptr);

        // Set the class lib object for loading runtime resources.
        void SetClassLibrary(const ClassLibrary* classlib)
        { mClassLib = classlib; }
        // Set the gravity vector, i.e. the direction and
        // magnitude of the gravitational pull. Defaults to
        // x=0.0f, y=1.0f
        void SetGravity(const glm::vec2& gravity)
        { mGravity = gravity; }
        // Set the scaling factor for transforming scene objects
        // into physics world. The underlying physics engine (box2d)
        // has been tuned to work well moving shapes between 0.1 and 10
        // units (meters). Static shapes may be up to 50 units.
        // The scaling factor is applied when scene objects are added
        // and transformed into the physics world while the inverse of
        // the scale is applied when updating the scene from physics state.
        // The default is x=1.0f, y=1.0f
        void SetScale(const glm::vec2& scale)
        { mScale = scale; }
        // Set the time step for stepping the physics simulation forward.
        // If time step is for example 1/60.0f then for 1 second of
        // real time simulation one need to call Step 60 times.
        // The higher the frequency of steps (with smaller time steps)
        // the better the simulation in general. However, the increased
        // simulation accuracy comes at a higher computational cost.
        // The default is 1/60.0f.
        void SetTimestep(float step)
        { mTimestep = step; }
        void SetNumVelocityIterations(unsigned iter)
        { mNumVelocityIterations = iter; }
        void SetNumPositionIterations(unsigned iter)
        { mNumPositionIterations = iter; }
        float GetTimeStep() const
        { return mTimestep; }
        unsigned GetNumVelocityIterations() const
        { return mNumVelocityIterations; }
        unsigned GetNumPositionIterations() const
        { return mNumPositionIterations; }
        // Get the current scaling vector.
        glm::vec2 GetScale() const
        { return mScale; }
        // Get the current gravity vector.
        glm::vec2 GetGravity() const
        { return mGravity; }
        // Map vector (either direction or position) from game world to the
        // physics world using the current scaling vector.
        glm::vec2 MapVectorFromGame(const glm::vec2& scene_vector) const
        { return scene_vector / mScale; }
        // Map vector (either direction of position) from physics world to the
        // game world using the current scaling vector.
        glm::vec2 MapVectorToGame(const glm::vec2& phys_world_vector) const
        { return phys_world_vector * mScale; }
        // Map an angle (in radians) from game world to physics world.
        float MapAngleFromGame(float radians) const
        { return radians; }
        // Map an angle (in radians) from physics world to game world.
        float MapAngleToGame(float radians) const
        { return radians; }
        // Map length (in game units) from game world to physics world.
        float MapLengthFromGame(float length) const;
        float MapLengthToGame(float meters) const;
        // Returns if we have a current world simulation.
        bool HaveWorld() const
        { return !!mWorld; }

        // Update the physics world based on the changes in the scene.
        void UpdateWorld(const game::Scene& scene);
        // Update the physics world based on the changes in the entity.
        void UpdateWorld(const game::Entity& entity);

        // Update the scene with the changes from the physics simulation.
        void UpdateScene(game::Scene& scene);
        // Update a single entity with the changes from the physics simulation.
        // This is intended to be used when the world is created with a single
        // entity instance for simulating the entity only.
        void UpdateEntity(game::Entity& entity);

        // Step the physics simulation forward by one time step.
        void Step(std::vector<ContactEvent>* contacts = nullptr);

        // Delete all physics bodies currently in the system.
        void DeleteAll();
        // Delete a physics body with the given node id.
        void DeleteBody(const std::string& node);
        void DeleteBody(const game::EntityNode& node);

        // Apply an impulse (defined as a vector with magnitude and direction) to
        // the center of the node's rigid body. The body must be dynamic in order for
        // this to work. Newtons per seconds or Kgs per meters per second.
        // Returns true if impulse was applied.
        bool ApplyImpulseToCenter(const game::EntityNode& node, const glm::vec2& impulse);
        bool ApplyImpulseToCenter(const std::string& node, const glm::vec2& impulse);
        // todo:
        bool ApplyForceToCenter(const game::EntityNode& node, const glm::vec2& force);
        bool ApplyForceToCenter(const std::string& node, const glm::vec2& force);
        // todo:
        bool SetLinearVelocity(const game::EntityNode& node, const glm::vec2& velocity);
        bool SetLinearVelocity(const std::string& node, const glm::vec2& velocity);

        // The ray cast mode is used to determine which objects to look for
        // and to consider when casting the ray in the physics world.
        enum class RayCastMode {
            // Find the closest object.
            // Should return a single result only or nothing if nothing was
            // hit in that direction of the ray.
            Closest,
            // Return the first object hit. not necessarily the closest!
            First,
            // Return all objects hit by the ray.
            All
        };
        // An aggregate result of the ray casting with information per
        // each object that was hit by the ray.
        struct RayCastResult {
            // the entity node (with a rigid body) that intersected with the ray.
            game::EntityNode* node = nullptr;
            // the point of intersection in physics world space.
            glm::vec2 point;
            // the rigid body surface normal at the point of ray/body intersection
            glm::vec2 normal;
            // the normalized fractional distance along the ray from ray start until the hit point.
            float fraction = 0.0f;
        };
        // Perform ray casting by looking up and finding physics objects that
        // intersect with the ray. The ray's starting and ending position are
        // vectors in the physics world space. To convert from game space to
        // physics space use the MapVectorFromGame function.
        void RayCast(const glm::vec2& start, const glm::vec2& end, std::vector<RayCastResult>* results,
                     RayCastMode mode = RayCastMode::All) const;

        // Initialize the physics world based on the scene.
        // The scene is traversed and then for each rigid body
        // a physics body is created.
        // Calling this function will create a new physics world
        // and destroy any previous world. Any physics parameters
        // such as gravity should be set before calling this function.
        void CreateWorld(const game::Scene& scene);

        // Initialize the physics world based on a single entity.
        // This is mostly useful when visualizing the effect of
        // rigid bodies on the entity and their interaction when
        // combined with joints. The world is created relative
        // to the entity's coordinate space, i.e. entity's origin
        // is the physics world origin.
        // This will create a new physics world, so you should make
        // sure to set all the desired physics parameters (such as gravity)
        // before calling this.
        void CreateWorld(const game::Entity& entity);

        void DestroyWorld();

        std::tuple<bool, glm::vec2> FindCurrentLinearVelocity(const game::EntityNode& node) const;
        std::tuple<bool, glm::vec2> FindCurrentLinearVelocity(const std::string& node) const;
        std::tuple<bool, float> FindCurrentAngularVelocity(const game::EntityNode& node) const;
        std::tuple<bool, float> FindCurrentAngularVelocity(const std::string& node) const;
        std::tuple<bool, float> FindMass(const game::EntityNode& node) const;
        std::tuple<bool, float> FindMass(const std::string& node) const;

#if defined(GAMESTUDIO_ENABLE_PHYSICS_DEBUG)
        // Visualize the physics world object's by drawing OOBs around them.
        void DebugDrawObjects(gfx::Painter& painter, gfx::Transform& view) const;
#endif
    private:
        struct RigidBodyData;
        struct FixtureData;

        void UpdateEntity(const glm::mat4& model_to_world, game::Entity& scene);
        void KillEntity(const game::Entity& entity);
        void UpdateWorld(const glm::mat4& model_to_world, const game::Entity& entity);
        void AddEntity(const glm::mat4& model_to_world, const game::Entity& entity);
        void AddEntityNode(const glm::mat4& model_to_world, const game::Entity& entity, const game::EntityNode& node);
    private:
        // The class loader instance for loading resources.
        const ClassLibrary* mClassLib = nullptr;
        // Physics node.
        struct RigidBodyData {
            std::string debug_name;
            // The entity node instance associated with this physics node
            game::EntityNode* node = nullptr;
            // the extents (box) of the scene node.
            glm::vec2 world_extents;
            // the corresponding box2d physics body for this node.
            b2Body* world_body = nullptr;
            // cached rigid body flags
            unsigned flags = 0;
        };
        struct FixtureData {
            std::string debug_name;
            game::EntityNode* node = nullptr;
            glm::vec2 shape_size;
            glm::vec2 shape_offset;
            float shape_rotation = 0.0f;
        };
        // The nodes represented in the physics simulation.
        std::unordered_map<std::string, RigidBodyData> mNodes;
        // the fixtures in the physics world that map to nodes.
        std::unordered_map<b2Fixture*, FixtureData> mFixtures;
        // The current physics world if any.
        std::unique_ptr<b2World> mWorld;
        // Gravity vector of the world.
        glm::vec2 mGravity = {0.0f, 1.0f};
        // The scaling factor for transforming nodes into
        // physics world.
        glm::vec2 mScale = {1.0f, 1.0f};
        // The timestep for physics simulation steps.
        float mTimestep = 1.0/60.0f;
        unsigned mNumVelocityIterations = 8;
        unsigned mNumPositionIterations = 3;
    };

} // namespace

