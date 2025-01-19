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

#include <set>

#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/entity_node_light.h"

namespace game
{

BasicLightClass::BasicLightClass()
{
    mFlags.set(Flags::Enabled, true);
    mSpotHalfAngle = FDegrees { 30.0f };
}

void BasicLightClass::IntoJson(data::Writer& data) const
{
    data.Write("type", mLightType);
    data.Write("flags", mFlags);
    data.Write("layer", mLayer);
    data.Write("direction", mDirection);
    data.Write("translation", mTranslation);
    data.Write("ambient_color", mAmbientColor);
    data.Write("diffuse_color", mDiffuseColor);
    data.Write("specular_color", mSpecularColor);
    data.Write("spot_half_angle", mSpotHalfAngle);
    data.Write("constant_attenuation", mConstantAttenuation);
    data.Write("linear_attenuation", mLinearAttenuation);
    data.Write("quadratic_attenuation", mQuadraticAttenuation);

}
bool BasicLightClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("type",  &mLightType);
    ok &= data.Read("flags", &mFlags);
    ok &= data.Read("layer", &mLayer);
    ok &= data.Read("direction", &mDirection);
    ok &= data.Read("translation", &mTranslation);
    ok &= data.Read("ambient_color", &mAmbientColor);
    ok &= data.Read("diffuse_color", &mDiffuseColor);
    ok &= data.Read("specular_color", &mSpecularColor);
    ok &= data.Read("spot_half_angle", &mSpotHalfAngle);
    ok &= data.Read("constant_attenuation", &mConstantAttenuation);
    ok &= data.Read("linear_attenuation", &mLinearAttenuation);
    ok &= data.Read("quadratic_attenuation", &mQuadraticAttenuation);
    return ok;
}

size_t BasicLightClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mFlags);
    hash = base::hash_combine(hash, mLightType);
    hash = base::hash_combine(hash, mLayer);
    hash = base::hash_combine(hash, mDirection);
    hash = base::hash_combine(hash, mTranslation);
    hash = base::hash_combine(hash, mAmbientColor);
    hash = base::hash_combine(hash, mDiffuseColor);
    hash = base::hash_combine(hash, mSpecularColor);
    hash = base::hash_combine(hash, mSpotHalfAngle);
    hash = base::hash_combine(hash, mConstantAttenuation);
    hash = base::hash_combine(hash, mLinearAttenuation);
    hash = base::hash_combine(hash, mQuadraticAttenuation);
    return hash;
}

} // namespace
