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

#include "game/animator.h"
#include "game/transform_animator.h"
#include "game/kinematic_animator.h"
#include "game/material_animator.h"
#include "game/property_animator.h"

namespace game
{
KinematicAnimator* Animator::AsKinematicAnimator()
{
    return Cast<KinematicAnimator>(Type::KinematicAnimator);
}

const KinematicAnimator* Animator::AsKinematicAnimator() const
{
    return Cast<KinematicAnimator>(Type::KinematicAnimator);
}

TransformAnimator* Animator::AsTransformAnimator()
{
    return Cast<TransformAnimator>(Type::TransformAnimator);
}

const TransformAnimator* Animator::AsTransformAnimator() const
{
    return Cast<TransformAnimator>(Type::TransformAnimator);
}

MaterialAnimator* Animator::AsMaterialAnimator()
{
    return Cast<MaterialAnimator>(Type::MaterialAnimator);
}

const MaterialAnimator* Animator::AsMaterialAnimator() const
{
    return Cast<MaterialAnimator>(Type::MaterialAnimator);
}

PropertyAnimator* Animator::AsPropertyAnimator()
{
    return Cast<PropertyAnimator>(Type::PropertyAnimator);
}

const PropertyAnimator* Animator::AsPropertyAnimator() const
{
    return Cast<PropertyAnimator>(Type::PropertyAnimator);
}

BooleanPropertyAnimator* Animator::AsBooleanPropertyAnimator()
{
    return Cast<BooleanPropertyAnimator>(Type::BooleanPropertyAnimator);
}

const BooleanPropertyAnimator* Animator::AsBooleanPropertyAnimator() const
{
    return Cast<BooleanPropertyAnimator>(Type::BooleanPropertyAnimator);
}


template<typename T>
T* Animator::Cast(Type desire)
{
    if (GetType() == desire)
        return static_cast<T*>(this);
    return nullptr;
}

template<typename T>
const T* Animator::Cast(Type desire) const
{
    if (GetType() == desire)
        return static_cast<const T*>(this);
    return nullptr;
}

} // namespace