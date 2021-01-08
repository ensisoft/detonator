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
#  include <glm/glm.hpp>
#  include <box2d/box2d.h>
#include "warnpop.h"

#include <string>
#include <memory>
#include <unordered_map>

class b2World;

namespace gfx {
    class Painter;
    class Transform;
}

namespace game
{
    class ClassLibrary;
    class EntityNode;
    class Entity;
    class SceneNode;
    class Scene;
    class FBox;

    class PhysicsEngine
    {
    public:
        PhysicsEngine(const ClassLibrary* loader = nullptr);

        // Set the loader object for loading runtime resources.
        void SetLoader(const ClassLibrary* loader)
        { mLoader = loader; }
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
        // Set the time step for setting the physics simulation forward.
        // If time step is for example 1/60.0f then for 1 second of
        // real time simulation one need to call Tick 60 times.
        // The higher the frequency of ticks (with smaller) time step
        // the better the simulation in general. However the increased
        // simulation accuracy comes at a higher computational cost.
        // The default is 1/60.0f.
        void SetTimestep(float step)
        { mTimestep = step; }
        void SetNumVelocityIterations(unsigned iter)
        { mNumVelocityIterations = iter; }
        void SetNumPositionIterations(unsigned iter)
        { mNumPositionIterations = iter; }

        // Returns if we have a current world simulation.
        bool HaveWorld() const
        { return !!mWorld; }

        // Update the scene with the changes from the physics simulation.
        void UpdateScene(Scene& scene);

        // Tick the physics simulation forward by one time step.
        void Tick();

        // Delete all physics bodies currently in the system.
        void DeleteAll();
        // Delete a physics body with the given node id.
        void DeleteBody(const std::string& node);
        void DeleteBody(const EntityNode& node);

        // Apply an impulse (defined as a vector with magnitude and direction) to
        // the center of the node's rigid body. The body must be dynamic in order for
        // this to work. Newtons per seconds or Kgs per meters per second.
        void ApplyImpulseToCenter(const EntityNode& node, const glm::vec2& impulse) const;
        void ApplyImpulseToCenter(const std::string& node, const glm::vec2& impulse) const;

        // Initialize the physics world based on the scene.
        // The scene is traversed and then for each scene entity
        // that has a rigid bodies a physics simulation body is
        // created based on the rigid body definition. This will
        // create a new physics world object. So you should make
        // sure to set all the desired parameters (such as gravity)
        // before calling this.
        void CreateWorld(const Scene& scene);

#if defined(GAMESTUDIO_ENABLE_PHYSICS_DEBUG)
        // Visualize the physics world object's by drawing OOBs around them.
        void DebugDrawObjects(gfx::Painter& painter, gfx::Transform& view);
#endif
    private:
        void UpdateEntity(const glm::mat4& model_to_world, Entity& scene);
        void AddEntity(const glm::mat4& model_to_world, const Entity& entity);
        void AddPhysicsNode(const glm::mat4& model_to_world, const EntityNode& node);
    private:
        // The class loader instance for loading resources.
        const ClassLibrary* mLoader = nullptr;
        // Physics node.
        struct PhysicsNode {
            // The instance id of the scene node in the scene.
            std::string instance;
            // the extents (box) of the scene node.
            glm::vec2 world_extents;
              // the corresponding box2d physics body for this node.
            b2Body* world_body = nullptr;
        };
        // The nodes represented in the physics simulation.
        std::unordered_map<std::string, PhysicsNode> mNodes;
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

