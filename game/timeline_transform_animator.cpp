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
#include "base/logging.h"
#include "base/hash.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/timeline_transform_animator.h"
#include "game/entity_node.h"
#include "game/entity_node_rigid_body.h"

namespace game
{

TransformAnimatorClass::TransformAnimatorClass()
{
    mTransformations.set(Transformations::Resize, true);
    mTransformations.set(Transformations::Rotate, true);
    mTransformations.set(Transformations::Scale, true);
    mTransformations.set(Transformations::Translate, true);
}

void TransformAnimatorClass::IntoJson(data::Writer& data) const
{
    data.Write("id",        mId);
    data.Write("name",      mName);
    data.Write("node",      mNodeId);
    data.Write("timeline",  mTimelineId);
    data.Write("method",    mInterpolation);
    data.Write("starttime", mStartTime);
    data.Write("duration",  mDuration);
    data.Write("position",  mEndPosition);
    data.Write("size",      mEndSize);
    data.Write("scale",     mEndScale);
    data.Write("rotation",  mEndRotation);
    data.Write("flags",     mFlags);
    data.Write("transformations", mTransformations);
}

bool TransformAnimatorClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",        &mId);
    ok &= data.Read("name",      &mName);
    ok &= data.Read("node",      &mNodeId);
    ok &= data.Read("timeline",  &mTimelineId);
    ok &= data.Read("starttime", &mStartTime);
    ok &= data.Read("duration",  &mDuration);
    ok &= data.Read("position",  &mEndPosition);
    ok &= data.Read("size",      &mEndSize);
    ok &= data.Read("scale",     &mEndScale);
    ok &= data.Read("rotation",  &mEndRotation);
    ok &= data.Read("method",    &mInterpolation);
    ok &= data.Read("flags",     &mFlags);
    ok &= data.Read("transformations", &mTransformations);
    return ok;
}

std::size_t TransformAnimatorClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mTimelineId);
    hash = base::hash_combine(hash, mInterpolation);
    hash = base::hash_combine(hash, mStartTime);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mEndPosition);
    hash = base::hash_combine(hash, mEndSize);
    hash = base::hash_combine(hash, mEndScale);
    hash = base::hash_combine(hash, mEndRotation);
    hash = base::hash_combine(hash, mFlags);
    hash = base::hash_combine(hash, mTransformations);
    return hash;
}

TransformAnimator::TransformAnimator(std::shared_ptr<const TransformAnimatorClass> klass) noexcept
    : mClass(std::move(klass))
{
    if (!mClass->TestFlag(Flags::StaticInstance))
    {
        Instance instance;
        instance.end_position = mClass->GetEndPosition();
        instance.end_size     = mClass->GetEndSize();
        instance.end_scale    = mClass->GetEndScale();
        instance.end_rotation = mClass->GetEndRotation();
        mDynamicInstance      = instance;
    }
}

void TransformAnimator::Start(EntityNode& node)
{
    mStartPosition = node.GetTranslation();
    mStartSize     = node.GetSize();
    mStartScale    = node.GetScale();
    mStartRotation = node.GetRotation();
}
void TransformAnimator::Apply(EntityNode& node, float t)
{
    const auto& inst = GetInstance();

    // apply interpolated state on the node.
    const auto method = mClass->GetInterpolation();
    const auto bits = mClass->GetTransformationBits();

    if (bits.test(Transformations::Translate))
    {
        const auto& p = math::interpolate(mStartPosition, inst.end_position, t, method);
        node.SetTranslation(p);
    }

    if (bits.test(Transformations::Resize))
    {
        const auto& s = math::interpolate(mStartSize, inst.end_size, t, method);
        node.SetSize(s);
    }

    if (bits.test(Transformations::Rotate))
    {
        const auto& r = math::interpolate(mStartRotation, inst.end_rotation, t, method);
        node.SetRotation(r);
    }

    if (bits.test(Transformations::Scale))
    {
        const auto& f = math::interpolate(mStartScale, inst.end_scale, t, method);
        node.SetScale(f);
    }

    if (auto* rigid_body = node.GetRigidBody())
        rigid_body->ResetTransform();

}
void TransformAnimator::Finish(EntityNode& node)
{
    const auto& inst = GetInstance();

    const auto bits = mClass->GetTransformationBits();
    if (bits.test(Transformations::Translate))
        node.SetTranslation(inst.end_position);
    if (bits.test(Transformations::Rotate))
        node.SetRotation(inst.end_rotation);
    if (bits.test(Transformations::Resize))
        node.SetSize(inst.end_size);
    if (bits.test(Transformations::Scale))
        node.SetScale(inst.end_scale);

    if (auto* rigid_body = node.GetRigidBody())
        rigid_body->ResetTransform();
}

void TransformAnimator::SetEndPosition(const glm::vec2& pos)
{
    if (mClass->TestFlag(Flags::StaticInstance))
    {
        WARN("Ignoring transform animator position set on static animator instance. [name=%1]", mClass->GetName());
        return;
    }
    mDynamicInstance.value().end_position = pos;
}
void TransformAnimator::SetEndScale(const glm::vec2& scale)
{
    if (mClass->TestFlag(Flags::StaticInstance))
    {
        WARN("Ignoring transform animator scale set on static animator instance. [name=%1]", mClass->GetName());
        return;
    }
    mDynamicInstance.value().end_scale = scale;
}
void TransformAnimator::SetEndSize(const glm::vec2& size)
{
    if (mClass->TestFlag(Flags::StaticInstance))
    {
        WARN("Ignoring transform animator size set on static animator instance. [name=%1]", mClass->GetName());
        return;
    }
    mDynamicInstance.value().end_size = size;
}

void TransformAnimator::SetEndRotation(float angle)
{
    if (mClass->TestFlag(Flags::StaticInstance))
    {
        WARN("Ignoring transform animator rotation set on static animator instance. [name=%1]", mClass->GetName());
        return;
    }
    mDynamicInstance.value().end_rotation = angle;
}

TransformAnimator::Instance TransformAnimator::GetInstance() const
{
    if (mDynamicInstance.has_value())
        return mDynamicInstance.value();
    Instance inst;
    inst.end_size     = mClass->GetEndSize();
    inst.end_scale    = mClass->GetEndScale();
    inst.end_rotation = mClass->GetEndRotation();
    inst.end_position = mClass->GetEndPosition();
    return inst;
}


} // namespace