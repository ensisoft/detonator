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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include "base/bitflag.h"
#include "game/timeline_animator_base.h"

#include "base/snafu.h"

namespace game
{
    // TransformAnimatorClass holds the transform data for some
    // particular type of linear transform of a node.
    class TransformAnimatorClass : public detail::AnimatorClassBase<TransformAnimatorClass>
    {
    public:
        enum class Transformations {
            Scale, Translate, Rotate, Resize
        };

        // The interpolation method.
        using Interpolation = math::Interpolation;

        TransformAnimatorClass();

        Interpolation GetInterpolation() const
        { return mInterpolation; }
        glm::vec2 GetEndPosition() const
        { return mEndPosition; }
        glm::vec2 GetEndSize() const
        { return mEndSize; }
        glm::vec2 GetEndScale() const
        { return mEndScale; }
        float GetEndRotation() const
        { return mEndRotation; }

        void SetInterpolation(Interpolation interp)
        { mInterpolation = interp; }
        void SetEndPosition(const glm::vec2& pos)
        { mEndPosition = pos; }
        void SetEndPosition(float x, float y)
        { mEndPosition = glm::vec2(x, y); }
        void SetEndSize(const glm::vec2& size)
        { mEndSize = size; }
        void SetEndSize(float x, float y)
        { mEndSize = glm::vec2(x, y); }
        void SetEndRotation(float rot)
        { mEndRotation = rot; }
        void SetEndScale(const glm::vec2& scale)
        { mEndScale = scale; }
        void SetEndScale(float x, float y)
        { mEndScale = glm::vec2(x, y); }

        inline void EnableRotation(bool enabled) noexcept
        { mTransformations.set(Transformations::Rotate, enabled); }
        inline void EnableTranslation(bool enabled) noexcept
        { mTransformations.set(Transformations::Translate, enabled); ; }
        inline void EnableScaling(bool enabled) noexcept
        { mTransformations.set(Transformations::Scale, enabled); }
        inline void EnableResize(bool enabled) noexcept
        { mTransformations.set(Transformations::Resize, enabled); }

        inline auto IsRotationEnable() const noexcept
        { return mTransformations.test(Transformations::Rotate); }
        inline auto IsTranslationEnabled() const noexcept
        { return mTransformations.test(Transformations::Translate); }
        inline auto IsScalingEnabled() const noexcept
        { return mTransformations.test(Transformations::Scale); }
        inline auto IsResizeEnabled() const noexcept
        { return mTransformations.test(Transformations::Resize); }

        inline auto GetTransformationBits() const noexcept
        { return mTransformations; }
        void ClearTransformBits() noexcept
        { mTransformations.clear(); }

        virtual Type GetType() const override
        { return Type::TransformAnimator; }
        void IntoJson(data::Writer& data) const override;
        bool FromJson(const data::Reader& data) override;
        std::size_t GetHash() const override;
    private:
        // the interpolation method to be used.
        Interpolation mInterpolation = Interpolation::Linear;
        // the ending state of the transformation.
        // the ending position (translation relative to parent)
        glm::vec2 mEndPosition = {0.0f, 0.0f};
        // the ending size
        glm::vec2 mEndSize = {1.0f, 1.0f};
        // the ending scale.
        glm::vec2 mEndScale = {1.0f, 1.0f};
        // the ending rotation.
        float mEndRotation = 0.0f;
        base::bitflag<Transformations> mTransformations;
    };

    // Apply change to the target nodes' transform.
    class TransformAnimator final : public Animator
    {
    public:
        using Transformations = TransformAnimatorClass::Transformations;

        explicit TransformAnimator(std::shared_ptr<const TransformAnimatorClass> klass) noexcept;
        explicit TransformAnimator(const TransformAnimatorClass& klass)
            : TransformAnimator(std::make_shared<TransformAnimatorClass>(klass))
        {}
        void Start(EntityNode& node) override;
        void Apply(EntityNode& node, float t) override;
        void Finish(EntityNode& node) override;

        virtual float GetStartTime() const override
        { return mClass->GetStartTime(); }
        virtual float GetDuration() const override
        { return mClass->GetDuration(); }
        virtual std::string GetNodeId() const override
        { return mClass->GetNodeId(); }
        virtual std::string GetClassId() const override
        { return mClass->GetId(); }
        virtual std::string GetClassName() const override
        { return mClass->GetName(); }
        virtual std::unique_ptr<Animator> Copy() const override
        { return std::make_unique<TransformAnimator>(*this); }
        virtual Type GetType() const override
        { return Type::TransformAnimator; }

        void SetEndPosition(const glm::vec2& pos);
        void SetEndScale(const glm::vec2& scale);
        void SetEndSize(const glm::vec2& size);
        void SetEndRotation(float angle);

        inline void SetEndPosition(float x, float y)
        { SetEndPosition(glm::vec2(x, y)); }
        inline void SetEndScale(float x, float y)
        { SetEndScale(glm::vec2(x, y)); }
        inline void SetEndSize(float x, float y)
        { SetEndSize(glm::vec2(x, y)); }
    private:
        struct Instance {
            glm::vec2 end_position;
            glm::vec2 end_size;
            glm::vec2 end_scale;
            float end_rotation = 0.0f;
        };
        Instance GetInstance() const;
    private:
        std::shared_ptr<const TransformAnimatorClass> mClass;
        // exists only if StaticInstance is not set.
        std::optional<Instance> mDynamicInstance;
        // The starting state for the transformation.
        // the transform animator will then interpolate between the
        // current starting and expected ending state.
        glm::vec2 mStartPosition = {0.0f, 0.0f};
        glm::vec2 mStartSize  = {1.0f, 1.0f};
        glm::vec2 mStartScale = {1.0f, 1.0f};
        float mStartRotation  = 0.0f;
    };


    ANIMATOR_INSTANCE_CASTING_MACROS(TransformAnimator, Animator::Type::TransformAnimator)
    ANIMATOR_CLASS_CASTING_MACROS(TransformAnimatorClass, Animator::Type::TransformAnimator)

} // namespace