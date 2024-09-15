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

#pragma once

#include "config.h"

#include <string>
#include <memory>

#include "data/fwd.h"

#include "base/snafu.h"

namespace game
{
    // AnimatorClass defines an interface for classes of animators.
    // Animators are objects that modify the state of some object
    // (such as an entity node) over time. For example a transform
    // animator will  animate the object by manipulating its transform
    // matrix over time.
    class AnimatorClass
    {
    public:
        // The type of the animator class.
        enum class Type {
            // TransformAnimator animators modify the transform state of the node
            // i.e. the translation, scale and rotation variables directly.
            TransformAnimator,
            // Kinematic animators modify the kinematic physics properties
            // for example, linear or angular velocity, of the node's rigid body.
            // This will result in a kinematically driven change in the nodes
            // transform.
            KinematicAnimator,
            // PropertyAnimator animators sets some parameter to the specific value on the node.
            PropertyAnimator,
            // SetFlag animators sets a binary flag to the specific state on the node.
            BooleanPropertyAnimator,
            // Material animator changes material parameters
            MaterialAnimator
        };
        enum class Flags {
            StaticInstance
        };
        // dtor.
        virtual ~AnimatorClass() = default;
        // Get human-readable name of the class.
        virtual std::string GetName() const = 0;
        // Get the id of this class
        virtual std::string GetId() const = 0;
        // Get the ID of the node affected by this animator.
        virtual std::string GetNodeId() const = 0;
        // Get the hash of the object state.
        virtual std::size_t GetHash() const = 0;
        // Create an exact copy of this class object.
        virtual std::unique_ptr<AnimatorClass> Copy() const = 0;
        // Create a new class instance with same property values
        // than this object but with a unique id.
        virtual std::unique_ptr<AnimatorClass> Clone() const = 0;
        // Get the dynamic type of the represented animator.
        virtual Type GetType() const = 0;
        // Get the normalized start time when this animator starts.
        virtual float GetStartTime() const = 0;
        // Get the normalized duration of this animator.
        virtual float GetDuration() const = 0;
        // Set a class flag to on/off.
        virtual void SetFlag(Flags flag, bool on_off) = 0;
        // Test a class flag.
        virtual bool TestFlag(Flags flag) const = 0;
        // Set a new normalized start time for the animator.
        // The value will be clamped to [0.0f, 1.0f].
        virtual void SetStartTime(float start) = 0;
        // Set a new normalized duration value for the animator.
        // The value will be clamped to [0.0f, 1.0f].
        virtual void SetDuration(float duration) = 0;
        // Set the ID of the node affected by this animator.
        virtual void SetNodeId(const std::string& id) = 0;
        // Set the human-readable name of the animator class.
        virtual void SetName(const std::string& name) = 0;
        // Serialize the class object into JSON.
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load the class object state from JSON. Returns true when
        // successful otherwise false and the object is not valid state.
        virtual bool FromJson(const data::Reader& data) = 0;
    private:
    };

    class EntityNode;

    // An instance of AnimatorClass object.
    class Animator
    {
    public:
        using Type  = AnimatorClass::Type;
        using Flags = AnimatorClass::Flags;
        virtual ~Animator() = default;
        // Start the action/transition to be applied by this animator.
        // The node is the node in question that the changes will be applied to.
        virtual void Start(EntityNode& node) = 0;
        // Apply an interpolation of the state based on the time value t onto to the node.
        virtual void Apply(EntityNode& node, float t) = 0;
        // Finish the action/transition to be applied by this animator.
        // The node is the node in question that the changes will (were) applied to.
        virtual void Finish(EntityNode& node) = 0;
        // Get the normalized start time when this animator begins to take effect.
        virtual float GetStartTime() const = 0;
        // Get the normalized duration of the duration of the animator's transformation.
        virtual float GetDuration() const = 0;
        // Get the id of the node that will be modified by this animator.
        virtual std::string GetNodeId() const = 0;
        // Get the class ID.
        virtual std::string GetClassId() const = 0;
        // Get the class name.
        virtual std::string GetClassName() const = 0;
        // Create an exact copy of this animator object.
        virtual std::unique_ptr<Animator> Copy() const = 0;
        // Get the dynamic type of the animator.
        virtual Type GetType() const = 0;
    private:
    };

    template<typename Dst, typename Src>
    Dst* AnimatorCast(game::AnimatorClass::Type desire, Src* src)
    {
        if (src->GetType() == desire)
            return static_cast<Dst*>(src);
        return nullptr;
    }

    template<typename Dst, typename Src>
    const Dst* AnimatorCast(game::AnimatorClass::Type desire, const Src* src)
    {
        if (src->GetType() == desire)
            return static_cast<const Dst*>(src);
        return nullptr;
    }

#define ANIMATOR_INSTANCE_CASTING_MACROS(Class, Type)                                        \
    inline Class * As##Class(Animator* animator) noexcept {                                  \
        return AnimatorCast<Class>(Type, animator);                                          \
    }                                                                                        \
    inline const Class * As##Class(const Animator* animator) noexcept {                      \
        return AnimatorCast<Class>(Type, animator);                                          \
    }                                                                                        \

#define ANIMATOR_CLASS_CASTING_MACROS(Class, Type)                                           \
    inline Class * As##Class(AnimatorClass* animator) noexcept {                             \
        return AnimatorCast<Class>(Type, animator);                                          \
    }                                                                                        \
    inline const Class * As##Class(const AnimatorClass* animator) noexcept {                 \
        return AnimatorCast<Class>(Type, animator);                                          \
    }                                                                                        \

} // namespace