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

#include <variant>

#include "game/color.h"
#include "game/types.h"
#include "game/animator_base.h"

#include "base/snafu.h"

namespace game
{
    class MaterialAnimatorClass : public detail::AnimatorClassBase<MaterialAnimatorClass>
    {
    public:
        using Interpolation = math::Interpolation;
        using MaterialParam = std::variant<float, int,
                std::string,
                Color4f,
                glm::vec2, glm::vec3, glm::vec4>;
        using MaterialParamMap = std::unordered_map<std::string, MaterialParam>;

        Interpolation GetInterpolation() const
        { return mInterpolation; }
        void SetInterpolation(Interpolation method)
        { mInterpolation = method; }

        void SetMaterialParam(const std::string& name, const MaterialParam& value)
        { mMaterialParams[name] = value; }
        MaterialParamMap GetMaterialParams()
        { return mMaterialParams; }
        const MaterialParamMap& GetMaterialParams() const
        { return mMaterialParams; }
        bool HasMaterialParam(const std::string& name) const
        { return base::SafeFind(mMaterialParams, name) != nullptr; }
        MaterialParam* FindMaterialParam(const std::string& name)
        { return base::SafeFind(mMaterialParams, name); }
        const MaterialParam* FindMaterialParam(const std::string& name) const
        { return base::SafeFind(mMaterialParams, name); }
        template<typename T>
        T* GetMaterialParamValue(const std::string& name)
        {
            if (auto* ptr = base::SafeFind(mMaterialParams, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        template<typename T>
        const T* GetMaterialParamValue(const std::string& name) const
        {
            if (auto* ptr = base::SafeFind(mMaterialParams, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        void DeleteMaterialParam(const std::string& name)
        { mMaterialParams.erase(name); }
        void SetMaterialParams(const MaterialParamMap& map)
        { mMaterialParams = map; }
        void SetMaterialParams(MaterialParamMap&& map)
        { mMaterialParams = std::move(map); }

        virtual Type GetType() const override
        { return Type::MaterialAnimator; }
        virtual std::size_t GetHash() const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
    private:
        // Interpolation method used to change the value.
        Interpolation mInterpolation = Interpolation::Linear;

        MaterialParamMap mMaterialParams;
    };

    class MaterialAnimator final : public Animator
    {
    public:
        using Interpolation    = MaterialAnimatorClass::Interpolation;
        using MaterialParam    = MaterialAnimatorClass::MaterialParam;
        using MaterialParamMap = MaterialAnimatorClass::MaterialParamMap;
        MaterialAnimator(const std::shared_ptr<const MaterialAnimatorClass>& klass)
          : mClass(klass)
        {}
        MaterialAnimator(const MaterialAnimatorClass& klass)
          : mClass(std::make_shared<MaterialAnimatorClass>(klass))
        {}
        virtual void Start(EntityNode& node) override;
        virtual void Apply(EntityNode& node, float t) override;
        virtual void Finish(EntityNode& node) override;

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
        { return std::make_unique<MaterialAnimator>(*this); }
        virtual Type GetType() const override
        { return Type::MaterialAnimator; }
    private:
        template<typename T>
        T Interpolate(const MaterialParam& start, const MaterialParam& end, float t)
        {
            const auto method = mClass->GetInterpolation();
            ASSERT(std::holds_alternative<T>(end));
            ASSERT(std::holds_alternative<T>(start));
            return math::interpolate(std::get<T>(start), std::get<T>(end), t, method);
        }
    private:
        std::shared_ptr<const MaterialAnimatorClass> mClass;
        MaterialParamMap mStartValues;
    };


} // namespace