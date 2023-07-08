// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include <vector>

#include "engine/lua.h"
#include "game/entity.h"
#include "game/scene.h"
#include "game/scriptvar.h"

namespace engine {
namespace lua {

        // data policy class for ArrayInterface below
    // uses a non-owning pointer to data that comes from
    // somewhere else such as ScriptVar
    template<typename T>
    class ArrayDataPointer
    {
    public:
        ArrayDataPointer(std::vector<T>* array)

          : mArray(array)
        {}
    protected:
        ArrayDataPointer() = default;
        void CommitChanges()
        {
            // do nothing is intentional.
        }
        std::vector<T>& GetArray()
        { return *mArray; }
        const std::vector<T>& GetArray() const
        { return *mArray; }
    private:
        std::vector<T>* mArray = nullptr;
    };

    // data policy class for ArrayInterface below.
    // has a copy of the data and then exposes a pointer to it.
    template<typename T>
    class ArrayDataObject
    {
    public:
        ArrayDataObject(const std::vector<T>& array)
          : mArrayData(array)
        {}
        ArrayDataObject(std::vector<T>&& array)
          : mArrayData(array)
        {}
    protected:
        ArrayDataObject() = default;
        void CommitChanges()
        {
            // do nothing intentional.
        }
        std::vector<T>& GetArray()
        { return mArrayData; }
        const std::vector<T>& GetArray() const
        { return mArrayData; }
    private:
        std::vector<T> mArrayData;
    };

    // template to adapt an underlying std::vector
    // to sol2's Lua container interface. the vector
    // is not owned by the array interface object and
    // may come for example from a scene/entity scripting
    // variable when the variable is an array.
    template<typename T, template<typename> class DataPolicy = ArrayDataPointer>
    class ArrayInterface : public DataPolicy<T> {
    public:
        using value_type = typename std::vector<T>::value_type;
        // this was using const_iterator and i'm not sure where that
        // came from (perhaps from the sol3 "documentation"). Anyway,
        // using const_iterator makes emscripten build shit itself.
        using iterator   = typename std::vector<T>::iterator;
        using size_type  = typename std::vector<T>::size_type;

        using BaseClass = DataPolicy<T>;

        template<typename Arg0>
        ArrayInterface(bool read_only, Arg0&& arg0)
          : BaseClass(std::forward<Arg0>(arg0))
          , mReadOnly(read_only)
        {}
        template<typename Arg0, typename Arg1>
        ArrayInterface(bool read_only, Arg0&& arg0, Arg1&& arg1)
          : BaseClass(std::forward<Arg0>(arg0),
                      std::forward<Arg1>(arg1))
          , mReadOnly(read_only)
        {}

        iterator begin() noexcept
        { return BaseClass::GetArray().begin(); }
        iterator end() noexcept
        { return BaseClass::GetArray().end(); }
        size_type size() const noexcept
        { return BaseClass::GetArray().size(); }
        bool empty() const noexcept
        { return BaseClass::GetArray().empty(); }
        void push_back(const T& value)
        {
            BaseClass::GetArray().push_back(value);
            BaseClass::CommitChanges();
        }
        T GetItemFromLua(unsigned index) const
        {
            // lua uses 1 based indexing.
            index = index - 1;
            if (index >= size())
                throw GameError("ArrayInterface access out of bounds.");
            return BaseClass::GetArray()[index];
        }
        void SetItemFromLua(unsigned index, const T& item)
        {
            // Lua uses 1 based indexing
            index = index - 1;
            if (index >= size())
                throw GameError("ArrayInterface access out of bounds.");
            if (IsReadOnly())
                throw GameError("Trying to write to read only array.");
            BaseClass::GetArray()[index] = item;
            BaseClass::CommitChanges();
        }
        void PopBack()
        {
            if (IsReadOnly())
                throw GameError("Trying to modify read only array.");
            auto& arr = BaseClass::GetArray();
            if (arr.empty())
                return;
            arr.pop_back();
            BaseClass::CommitChanges();
        }
        void PopFront()
        {
            if (IsReadOnly())
                throw GameError("Trying to modify read only array.");
            auto& arr = BaseClass::GetArray();
            if (arr.empty())
                return;
            arr.erase(arr.begin());
            BaseClass::CommitChanges();
        }
        void Clear()
        {
            if (IsReadOnly())
                throw GameError("Trying to modify read only array.");
            BaseClass::GetArray().clear();
            BaseClass::CommitChanges();
        }

        T GetFirst() const
        {
            return GetItemFromLua(1);
        }
        T GetLast() const
        {
            return GetItemFromLua(size());
        }

        T GetItem(unsigned index) const
        { return base::SafeIndex(BaseClass::GetArray(), index); }

        bool IsReadOnly() const
        { return mReadOnly; }

    private:
        bool mReadOnly = false;
    };

    template<typename T>
    class ArrayObjectReference;

    template<>
    class ArrayObjectReference<game::Entity*>
    {
    public:
        ArrayObjectReference(const game::ScriptVar* var, game::Scene* scene)
          : mVar(var)
          , mScene(scene)
        {
            // resolve object IDs to the actual objects
            const auto& refs = mVar->GetArray<game::ScriptVar::EntityReference>();
            for (const auto& ref : refs)
                mEntities.push_back(mScene->FindEntityByInstanceId(ref.id));
        }
    protected:
        ArrayObjectReference() = default;
        void CommitChanges()
        {
            // resolve entity objects back into their IDs
            // and store the changes in the script variable.
            std::vector<game::ScriptVar::EntityReference> refs;
            for (auto* entity : mEntities)
            {
                game::ScriptVar::EntityReference ref;
                ref.id = entity ? entity->GetId() : "";
            }
            mVar->SetArray(std::move(refs));
        }
        std::vector<game::Entity*>& GetArray()
        { return mEntities; }
        const std::vector<game::Entity*>& GetArray() const
        { return mEntities; }
    private:
        const game::ScriptVar* mVar = nullptr;
        game::Scene* mScene = nullptr;
        std::vector<game::Entity*> mEntities;
    };

    template<>
    class ArrayObjectReference<game::EntityNode*>
    {
    public:
        ArrayObjectReference(const game::ScriptVar* var, game::Entity* entity)
          : mVar(var)
          , mEntity(entity)
        {
            const auto& refs = mVar->GetArray<game::ScriptVar::EntityNodeReference>();
            // The entity node instance IDs are dynamic. since the
            // reference is placed during design time it's based on the class IDs
            for (const auto& ref : refs)
                mNodes.push_back(mEntity->FindNodeByClassId(ref.id));
        }
    protected:
        ArrayObjectReference() = default;
        void CommitChanges()
        {
            std::vector<game::ScriptVar::EntityNodeReference> refs;
            for (auto* node : mNodes)
            {
                game::ScriptVar::EntityNodeReference ref;
                ref.id = node ? node->GetClassId() : "";
            }
            mVar->SetArray(std::move(refs));
        }
        std::vector<game::EntityNode*>& GetArray()
        { return mNodes; }
        const std::vector<game::EntityNode*>& GetArray() const
        { return mNodes; }
    private:
        const game::ScriptVar* mVar = nullptr;
        game::Entity* mEntity = nullptr;
        std::vector<game::EntityNode*> mNodes;
    };

} // namespace
} // namespace