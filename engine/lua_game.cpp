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

#include "config.h"

#include "warnpush.h"
#  include <sol/sol.hpp>
#include "warnpop.h"

#include "base/logging.h"
#include "audio/graph.h"
#include "engine/lua.h"
#include "engine/lua_array.h"
#include "engine/audio.h"
#include "engine/state.h"
#include "engine/event.h"
#include "engine/classlib.h"
#include "engine/camera.h"
#include "engine/physics.h"
#include "graphics/material.h"
#include "data/json.h"
#include "game/animation.h"
#include "game/entity.h"
#include "game/scriptvar.h"
#include "game/tilemap.h"
#include "uikit/window.h"
#include "uikit/widget.h"

using namespace engine::lua;
using namespace game;

namespace {
template<typename Actuator> inline
Actuator* ActuatorCast(game::Actuator* actuator)
{ return dynamic_cast<Actuator*>(actuator); }

template<typename Actuator>
void BindActuatorInterface(sol::usertype<Actuator>& actuator)
{
    actuator["GetClassId"]   = &Actuator::GetClassId;
    actuator["GetClassName"] = &Actuator::GetClassName;
    actuator["GetNodeId"]    = &Actuator::GetNodeId;
    actuator["GetStartTime"] = &Actuator::GetStartTime;
    actuator["GetDuration"]  = &Actuator::GetDuration;
}

sol::object GetAnimatorVar(const game::Animator& animator, const char* key, sol::this_state this_state)
{
    sol::state_view state(this_state);
    if (const auto* ptr = animator.FindValue(key))
    {
        if (const auto* p = std::get_if<bool>(ptr))
            return sol::make_object(state, *p);
        else if (const auto* p = std::get_if<int>(ptr))
            return sol::make_object(state, *p);
        else if (const auto* p = std::get_if<float>(ptr))
            return sol::make_object(state, *p);
        else if (const auto* p = std::get_if<std::string>(ptr))
            return sol::make_object(state, *p);
        else if (const auto* p = std::get_if<glm::vec2>(ptr))
            return sol::make_object(state, *p);
        else BUG("Missing animator value type.");
    }
    return sol::nil;
}
void SetAnimatorVar(game::Animator& animator, const char* key, sol::object value, sol::this_state this_state)
{
    if (value.is<bool>())
        animator.SetValue(key, value.as<bool>());
    else if (value.is<int>())
        animator.SetValue(key, value.as<int>());
    else if (value.is<float>())
        animator.SetValue(key, value.as<float>());
    else if (value.is<std::string>())
        animator.SetValue(key, value.as<std::string>());
    else if (value.is<glm::vec2>())
        animator.SetValue(key, value.as<glm::vec2>());
    else throw GameError("Unsupported animator value type.");
}

sol::object ObjectFromScriptVarValue(const ScriptVar& var, sol::this_state state)
{
    sol::state_view lua(state);
    const auto type = var.GetType();
    const auto read_only = var.IsReadOnly();
    if (type == ScriptVar::Type::Boolean)
    {
        using ArrayType = ArrayInterface<bool, ArrayDataPointer>;
        if (var.IsArray())
            return sol::make_object(lua, ArrayType(read_only, &var.GetArray<bool>()));
        return sol::make_object(lua, var.GetValue<bool>());
    }
    else if (type == ScriptVar::Type::Float)
    {
        using ArrayType = ArrayInterface<float, ArrayDataPointer>;
        if (var.IsArray())
            return sol::make_object(lua, ArrayType(read_only, &var.GetArray<float>()));
        return sol::make_object(lua, var.GetValue<float>());
    }
    else if (type == ScriptVar::Type::String)
    {
        using ArrayType = ArrayInterface<std::string, ArrayDataPointer>;
        if (var.IsArray())
            return sol::make_object(lua,  ArrayType(read_only, &var.GetArray<std::string>()));
        return sol::make_object(lua, var.GetValue<std::string>());
    }
    else if (type == ScriptVar::Type::Integer)
    {
        using ArrayType = ArrayInterface<int, ArrayDataPointer>;
        if (var.IsArray())
            return sol::make_object(lua, ArrayType(read_only, &var.GetArray<int>()));
        return sol::make_object(lua, var.GetValue<int>());
    }
    else if (type == ScriptVar::Type::Vec2)
    {
        using ArrayType = ArrayInterface<glm::vec2, ArrayDataPointer>;
        if (var.IsArray())
            return sol::make_object(lua, ArrayType(read_only, &var.GetArray<glm::vec2>()));
        return sol::make_object(lua, var.GetValue<glm::vec2>());
    } else BUG("Unhandled ScriptVar type.");
}

template<typename T>
sol::object ResolveEntityReferences(const T& whatever, const ScriptVar& var, sol::state_view& state)
{
    // this is a generic resolver for cases when a reference cannot
    // be resolved. for example if entity contains entity references
    // we can't resolve those. only scene can resolve entity references.
    // this generic resolver will simply just return the string IDs.
    if (var.IsArray())
    {
        using ArrayType = ArrayInterface<std::string, ArrayDataObject>;

        const auto& refs = var.GetArray<ScriptVar::EntityReference>();

        std::vector<std::string> strs;
        for (const auto& ref : refs)
            strs.push_back(ref.id);

        return sol::make_object(state, ArrayType(true, std::move(strs)));
    }

    const auto& ref = var.GetValue<ScriptVar::EntityReference>();

    return sol::make_object(state, ref.id);
}

sol::object ResolveEntityReferences(game::Scene& scene, const ScriptVar& var, sol::state_view& state)
{
    // the reference to the entity is placed during design time
    // and the scene placement node has an ID which becomes the
    // entity instance ID when the scene is instantiated.
    if (var.IsArray())
    {
        using ArrayType = ArrayInterface<Entity*, ArrayObjectReference>;

        return sol::make_object(state, ArrayType(var.IsReadOnly(), &var, &scene));
    }
    const auto& ref = var.GetValue<ScriptVar::EntityReference>();

    return sol::make_object(state, scene.FindEntityByInstanceId(ref.id));
}

template<typename T>
sol::object ResolveNodeReferences(const T& whatever, const ScriptVar& var, sol::state_view& state)
{
    // this is a generic resolver for cases when a reference cannot be resolved.
    // for example if a scene contains entity node references we can't resolve those,
    // but only entity can resolve entity node references.
    // this generic resolver will simply just return the string IDs
    if (var.IsArray())
    {
        using ArrayType = ArrayInterface<std::string, ArrayDataObject>;

        const auto& refs = var.GetArray<ScriptVar::EntityNodeReference>();

        std::vector<std::string> strs;
        for (const auto& ref : refs)
            strs.push_back(ref.id);

        return sol::make_object(state, ArrayType(true, std::move(strs)));
    }
    const auto& ref = var.GetValue<ScriptVar::EntityNodeReference>();

    return sol::make_object(state, ref.id);

}

sol::object ResolveNodeReferences(game::Entity& entity, const ScriptVar& var, sol::state_view& state)
{
    // The entity node instance IDs are dynamic. since the
    // reference is placed during design time it's based on the class IDs
    if (var.IsArray())
    {
        using ArrayType = ArrayInterface<EntityNode*, ArrayObjectReference>;

        return sol::make_object(state, ArrayType(var.IsReadOnly(), &var, &entity));
    }

    const auto& ref = var.GetValue<ScriptVar::EntityNodeReference>();

    return sol::make_object(state, entity.FindNodeByClassId(ref.id));
}

template<typename Type>
sol::object ResolveMaterialReferences(Type& object, const ScriptVar& var, sol::state_view& state)
{
    engine::ClassLibrary* lib = state["ClassLib"];

    if (var.IsArray())
    {
        using MaterialClassHandle = engine::ClassLibrary::ClassHandle<const gfx::MaterialClass>;

        using ArrayType = ArrayInterface<MaterialClassHandle, ArrayDataObject>;

        const auto& refs = var.GetArray<ScriptVar::MaterialReference>();

        std::vector<MaterialClassHandle> objects;
        for (const auto& ref : refs)
        {
            objects.push_back(lib->FindMaterialClassById(ref.id));
        }

        return sol::make_object(state, ArrayType(true, std::move(objects)));
    }

    const auto& ref = var.GetValue<ScriptVar::MaterialReference>();

    return sol::make_object(state, lib->FindMaterialClassById(ref.id));
}

template<typename Type>
sol::object GetScriptVar(Type& object, const char* key, sol::this_state state, sol::this_environment this_env)
{
    using namespace engine;
    sol::state_view lua(state);
    const ScriptVar* var = object.FindScriptVarByName(key);
    if (!var)
        throw GameError(base::FormatString("No such variable: '%1' in '%2'", key, object.GetClassName()));
    if (var->IsPrivate())
    {
        // looks like a sol2 bug. this_env is sometimes nullopt!
        // https://github.com/ThePhD/sol2/issues/1464
        if (this_env)
        {
            sol::environment& environment(this_env);
            const std::string script_id = environment["__script_id__"];
            if (object.GetScriptFileId() != script_id)
                throw GameError(base::FormatString("Trying to access private variable: '%1' in '%2'", key, object.GetClassName()));
        }
    }

    if (var->GetType() == game::ScriptVar::Type::EntityReference)
    {
        return ResolveEntityReferences(object, *var, lua);
    }
    else if (var->GetType() == game::ScriptVar::Type::EntityNodeReference)
    {
        return ResolveNodeReferences(object, *var, lua);
    }
    else if (var->GetType() == game::ScriptVar::Type::MaterialReference)
    {
        return ResolveMaterialReferences(object, *var, lua);
    }

    else return ObjectFromScriptVarValue(*var, state);
}
template<typename Type>
void SetScriptVar(Type& object, const char* key, sol::object value, sol::this_state state, sol::this_environment this_env)
{
    using namespace engine;
    const ScriptVar* var = object.FindScriptVarByName(key);
    if (var == nullptr)
        throw GameError(base::FormatString("No such variable '%1' in '%2' ", key, object.GetClassName()));
    else if (var->IsReadOnly())
        throw GameError(base::FormatString("Trying to write to a read only variable: '%1'", key));
    else if (var->IsPrivate())
    {
        // looks like a sol2 bug. this_env is sometimes nullopt!
        // https://github.com/ThePhD/sol2/issues/1464
        if (this_env)
        {
            sol::environment& environment(this_env);
            const std::string script_id = environment["__script_id__"];
            if (object.GetScriptFileId() != script_id)
                throw GameError(base::FormatString("Trying to access private variable: '%1' in '%2'", key, object.GetClassName()));
        }
    }

    if (value.is<int>() && var->HasType<int>())
        var->SetValue(value.as<int>());
    else if (value.is<float>() && var->HasType<float>())
        var->SetValue(value.as<float>());
    else if (value.is<bool>() && var->HasType<bool>())
        var->SetValue(value.as<bool>());
    else if (value.is<std::string>() && var->HasType<std::string>())
        var->SetValue(value.as<std::string>());
    else if (value.is<glm::vec2>() && var->HasType<glm::vec2>())
        var->SetValue(value.as<glm::vec2>());
    else if (value.is<game::EntityNode*>() && var->HasType<game::ScriptVar::EntityNodeReference>())
        var->SetValue(game::ScriptVar::EntityNodeReference{value.as<game::EntityNode*>()->GetClassId()});
    else if (value.is<game::Entity*>() && var->HasType<game::ScriptVar::EntityReference>())
        var->SetValue(game::ScriptVar::EntityReference{value.as<game::Entity*>()->GetId()});
    else if (!value.valid()) {
        if (var->HasType<game::ScriptVar::EntityNodeReference>())
            var->SetValue(game::ScriptVar::EntityNodeReference{""});
        else if (var->HasType<game::ScriptVar::EntityReference>())
            var->SetValue(game::ScriptVar::EntityReference{""});
    }
    else throw GameError(base::FormatString("Variable type mismatch. '%1' expects: '%2'", key, var->GetType()));
}

// WAR. G++ 10.2.0 has internal segmentation fault when using the Get/SetScriptVar helpers
// directly in the call to create new usertype. adding these specializations as a workaround.
template sol::object GetScriptVar<game::Scene>(game::Scene&, const char*, sol::this_state, sol::this_environment);
template sol::object GetScriptVar<game::Entity>(game::Entity&, const char*, sol::this_state, sol::this_environment);
template void SetScriptVar<game::Scene>(game::Scene&, const char* key, sol::object, sol::this_state, sol::this_environment);
template void SetScriptVar<game::Entity>(game::Entity&, const char* key, sol::object, sol::this_state, sol::this_environment);

void SetKvValue(engine::KeyValueStore& kv, const char* key, sol::object value)
{
    if (value.is<bool>())
        kv.SetValue(key, value.as<bool>());
    else if (value.is<int>())
        kv.SetValue(key, value.as<int>());
    else if (value.is<float>())
        kv.SetValue(key, value.as<float>());
    else if (value.is<std::string>())
        kv.SetValue(key, value.as<std::string>());
    else if (value.is<glm::vec2>())
        kv.SetValue(key, value.as<glm::vec2>());
    else if (value.is<glm::vec3>())
        kv.SetValue(key, value.as<glm::vec3>());
    else if (value.is<glm::vec4>())
        kv.SetValue(key, value.as<glm::vec4>());
    else if (value.is<base::Color4f>())
        kv.SetValue(key, value.as<base::Color4f>());
    else if (value.is<base::FSize>())
        kv.SetValue(key, value.as<base::FSize>());
    else if (value.is<base::FRect>())
        kv.SetValue(key, value.as<base::FRect>());
    else if (value.is<base::FPoint>())
        kv.SetValue(key, value.as<base::FPoint>());
    else throw GameError("Unsupported key-value store value type.");
}

template<typename T>
class ResultSet
{
public:
    using Set = std::set<T>;
    ResultSet(Set&& result)
      : mResult(std::move(result))
    { mBegin = mResult.begin(); }
    ResultSet(const Set& result)
      : mResult(result)
    { mBegin = mResult.begin(); }
    ResultSet()
    { mBegin = mResult.begin(); }
    T GetCurrent() const
    {
        if (mBegin == mResult.end())
            throw GameError("ResultSet iteration error.");
        return *mBegin;
    }
    T GetNext()
    {
        if (mBegin == mResult.end())
            throw GameError("ResultSet iteration error.");
        return *mBegin++;
    }
    void BeginIteration()
    { mBegin = mResult.begin(); }
    bool HasNext() const
    { return mBegin != mResult.end(); }
    bool IsEmpty() const
    { return mResult.empty(); }
    bool Next()
    {
        ++mBegin;
        return mBegin != mResult.end();
    }
    void EraseCurrent()
    {
        ASSERT(mBegin != mResult.end());
        mBegin = mResult.erase(mBegin);
    }
private:
    Set mResult;
    typename Set::iterator mBegin;
};

template<typename T>
class ResultVector
{
public:
    using Vector = std::vector<T>;
    ResultVector(Vector&& result)
      : mResult(std::move(result))
    { mBegin = mResult.begin(); }
    ResultVector(const Vector& result)
      : mResult(result)
    { mBegin = mResult.begin(); }
    ResultVector()
    { mBegin = mResult.begin(); }
    void BeginIteration()
    { mBegin = mResult.begin(); }
    bool HasNext() const
    { return mBegin != mResult.end(); }
    bool IsEmpty() const
    { return mResult.empty(); }
    bool Next()
    {  return ++mBegin != mResult.end(); }
    const T& GetCurrent() const
    {
        if (mBegin == mResult.end())
            throw GameError("ResultVector iteration error.");
        return *mBegin;
    }
    const T& GetNext()
    {
        if (mBegin == mResult.end())
            throw GameError("ResultVector iteration error.");
        return *mBegin++;
    }
    const T& GetAt(size_t index) const
    {
        if (index >= mResult.size())
            throw GameError("ResultVector index out of bounds.");
        return mResult[index];
    }
    size_t GetSize() const
    { return mResult.size(); }

    static ResultVector Join(const ResultVector& lhs, const ResultVector& rhs)
    {
        Vector vec;
        base::AppendVector(vec, lhs.mResult);
        base::AppendVector(vec, rhs.mResult);
        return  ResultVector(std::move(vec));
    }
private:
    Vector mResult;
    typename Vector::iterator mBegin;
};

} // namespace

namespace engine
{

void BindGameLib(sol::state& L)
{
    auto table = L["game"].get_or_create<sol::table>();
    table["X"] = glm::vec2(1.0f, 0.0f);
    table["Y"] = glm::vec2(0.0f, 1.0f);

    auto classlib = table.new_usertype<ClassLibrary>("ClassLibrary");
    classlib["FindEntityClassByName"]     = &ClassLibrary::FindEntityClassByName;
    classlib["FindEntityClassById"]       = &ClassLibrary::FindEntityClassById;
    classlib["FindSceneClassByName"]      = &ClassLibrary::FindSceneClassByName;
    classlib["FindSceneClassById"]        = &ClassLibrary::FindSceneClassById;
    classlib["FindUIByName"]              = &ClassLibrary::FindUIByName;
    classlib["FindUIById"]                = &ClassLibrary::FindUIById;
    classlib["FindAudioGraphClassByName"] = &ClassLibrary::FindAudioGraphClassByName;
    classlib["FindAudioGraphClassById"]   = &ClassLibrary::FindAudioGraphClassById;

    auto drawable = table.new_usertype<DrawableItem>("Drawable");
    drawable["SetMaterial"] = sol::overload(
        [](DrawableItem& item, const std::string& name, sol::this_state this_state) {
            sol::state_view L(this_state);
            ClassLibrary* lib = L["ClassLib"];
            const auto ret = lib->FindMaterialClassByName(name);
            if (ret)
                item.SetMaterialId(ret->GetId());
            else WARN("No such material class. [name='%1']", name);
            return !!ret;
        },
        [](DrawableItem& item, const std::shared_ptr<const gfx::MaterialClass>& klass) {
            if (!klass)
                throw GameError("Nil material class in SetMaterial.");
            item.SetMaterialId(klass->GetId());
        });

    drawable["SetActiveTextureMap"] = [](DrawableItem& item, const std::string& name, sol::this_state this_state) {
        sol::state_view L(this_state);
        ClassLibrary* lib = L["ClassLib"];
        const auto ret = lib->FindMaterialClassById(item.GetMaterialId());
        if (!ret)
        {
            WARN("No such material class. [name='%1']", name);
            return false;
        }

        for (unsigned i=0; i<ret->GetNumTextureMaps(); ++i) {
            const auto* map = ret->GetTextureMap(i);
            if (map->GetName() == name)
            {
                item.SetActiveTextureMap(map->GetId());
                return true;
            }
        }
        WARN("No such texture map. [name='%1']", name);
        return false;
    };
    drawable["SetMaterialId"] = &DrawableItem::SetMaterialId;
    drawable["GetMaterialId"] = &DrawableItem::GetMaterialId;
    drawable["GetDrawableId"] = &DrawableItem::GetDrawableId;
    drawable["GetLayer"]      = &DrawableItem::GetLayer;
    drawable["GetLineWidth"]  = &DrawableItem::GetLineWidth;
    drawable["GetTimeScale"]  = &DrawableItem::GetTimeScale;
    drawable["SetTimeScale"]  = &DrawableItem::SetTimeScale;
    drawable["TestFlag"]      = &TestFlag<DrawableItem>;
    drawable["SetFlag"]       = &SetFlag<DrawableItem>;
    drawable["IsVisible"]     = &DrawableItem::IsVisible;
    drawable["SetVisible"]    = &DrawableItem::SetVisible;
    drawable["SetUniform"]    = [](DrawableItem& item, const char* name, sol::object value) {
        if (value.is<float>())
            item.SetMaterialParam(name, value.as<float>());
        else if (value.is<int>())
            item.SetMaterialParam(name, value.as<int>());
        else if (value.is<base::Color4f>())
            item.SetMaterialParam(name, value.as<base::Color4f>());
        else if (value.is<glm::vec2>())
            item.SetMaterialParam(name, value.as<glm::vec2>());
        else if (value.is<glm::vec3>())
            item.SetMaterialParam(name, value.as<glm::vec3>());
        else if (value.is<glm::vec4>())
            item.SetMaterialParam(name, value.as<glm::vec4>());
        else throw GameError("Unsupported material uniform type.");
    };
    drawable["FindUniform"] = [](const DrawableItem& item, const char* name, sol::this_state state) {
        sol::state_view L(state);
        if (const auto* value = item.FindMaterialParam(name))
            return sol::make_object(L, *value);
        return sol::make_object(L, sol::nil);
    };
    drawable["HasUniform"]    = &DrawableItem::HasMaterialParam;
    drawable["DeleteUniform"] = &DrawableItem::DeleteMaterialParam;
    drawable["ClearUniforms"]  = &DrawableItem::ClearMaterialParams;
    drawable["AdjustMaterialTime"]        = &DrawableItem::AdjustMaterialTime;
    drawable["HasMaterialTimeAdjustment"] = &DrawableItem::HasMaterialTimeAdjustment;
    drawable["GetMaterialTime"]    = &DrawableItem::GetCurrentMaterialTime;

    auto body = table.new_usertype<RigidBodyItem>("RigidBody");
    body["IsEnabled"]             = &RigidBodyItem::IsEnabled;
    body["IsSensor"]              = &RigidBodyItem::IsSensor;
    body["IsBullet"]              = &RigidBodyItem::IsBullet;
    body["CanSleep"]              = &RigidBodyItem::CanSleep;
    body["DiscardRotation"]       = &RigidBodyItem::DiscardRotation;
    body["GetFriction"]           = &RigidBodyItem::GetFriction;
    body["GetRestitution"]        = &RigidBodyItem::GetRestitution;
    body["GetAngularDamping"]     = &RigidBodyItem::GetAngularDamping;
    body["GetLinearDamping"]      = &RigidBodyItem::GetLinearDamping;
    body["GetDensity"]            = &RigidBodyItem::GetDensity;
    body["GetPolygonShapeId"]     = &RigidBodyItem::GetPolygonShapeId;
    body["GetLinearVelocity"]     = &RigidBodyItem::GetLinearVelocity;
    body["GetAngularVelocity"]    = &RigidBodyItem::GetAngularVelocity;
    body["Enable"]                = &RigidBodyItem::Enable;
    body["ApplyImpulse"]          = sol::overload(
        [](RigidBodyItem& body, const glm::vec2& impulse) {
            body.ApplyLinearImpulseToCenter(impulse);
        },
        [](RigidBodyItem& body, float x, float y) {
            body.ApplyLinearImpulseToCenter(glm::vec2(x, y));
        });

    body["AdjustLinearVelocity"]  = sol::overload(
        [](RigidBodyItem& body, const glm::vec2& velocity) {
            body.AdjustLinearVelocity(velocity);
        },
        [](RigidBodyItem& body, float x, float y) {
            body.AdjustLinearVelocity(glm::vec2(x, y));
        });
    body["AdjustAngularVelocity"] = &RigidBodyItem::AdjustAngularVelocity;
    body["TestFlag"]              = &TestFlag<RigidBodyItem>;
    body["SetFlag"]               = &SetFlag<RigidBodyItem>;
    body["GetSimulationType"]     = [](const RigidBodyItem* item) {
        return magic_enum::enum_name(item->GetSimulation());
    };
    body["GetCollisionShapeType"] = [](const RigidBodyItem* item) {
        return magic_enum::enum_name(item->GetCollisionShape());
    };

    auto text = table.new_usertype<TextItem>("TextItem");
    text["GetText"]       = &TextItem::GetText;
    text["GetColor"]      = &TextItem::GetTextColor;
    text["GetLayer"]      = &TextItem::GetLayer;
    text["GetFontName"]   = &TextItem::GetFontName;
    text["GetFontSize"]   = &TextItem::GetFontSize;
    text["GetLineHeight"] = &TextItem::GetLineHeight;
    text["SetText"]       = (void(TextItem::*)(const std::string&))&TextItem::SetText;
    text["SetColor"]      = &TextItem::SetTextColor;
    text["TestFlag"]      = &TestFlag<TextItem>;
    text["SetFlag"]       = &SetFlag<TextItem>;

    auto spn = table.new_usertype<SpatialNode>("SpatialNode");
    spn["GetShape"] = [](const SpatialNode* node) {
        return magic_enum::enum_name(node->GetShape());
    };
    spn["IsEnabled"] = &SpatialNode::IsEnabled;
    spn["Enable"]    = &SpatialNode::Enable;

    auto entity_node = table.new_usertype<EntityNode>("EntityNode");
    entity_node["GetId"]          = &EntityNode::GetId;
    entity_node["GetName"]        = &EntityNode::GetName;
    entity_node["GetTag"]         = &EntityNode::GetTag;
    entity_node["GetClassId"]     = &EntityNode::GetClassId;
    entity_node["GetClassName"]   = &EntityNode::GetClassName;
    entity_node["GetClassTag"]    = &EntityNode::GetClassTag;
    entity_node["GetTranslation"] = &EntityNode::GetTranslation;
    entity_node["GetSize"]        = &EntityNode::GetSize;
    entity_node["GetScale"]       = &EntityNode::GetScale;
    entity_node["GetRotation"]    = &EntityNode::GetRotation;
    entity_node["HasRigidBody"]   = &EntityNode::HasRigidBody;
    entity_node["HasTextItem"]    = &EntityNode::HasTextItem;
    entity_node["HasDrawable"]    = &EntityNode::HasDrawable;
    entity_node["HasSpatialNode"] = &EntityNode::HasSpatialNode;
    entity_node["GetDrawable"]    = (DrawableItem*(EntityNode::*)(void))&EntityNode::GetDrawable;
    entity_node["GetRigidBody"]   = (RigidBodyItem*(EntityNode::*)(void))&EntityNode::GetRigidBody;
    entity_node["GetTextItem"]    = (TextItem*(EntityNode::*)(void))&EntityNode::GetTextItem;
    entity_node["GetSpatialNode"] = (SpatialNode*(EntityNode::*)(void))&EntityNode::GetSpatialNode;
    entity_node["GetEntity"]      = (Entity*(EntityNode::*)(void))&EntityNode::GetEntity;
    entity_node["SetName"]        = &EntityNode::SetName;
    entity_node["SetRotation"]    = &EntityNode::SetRotation;
    entity_node["SetScale"] = sol::overload(
        [](EntityNode& node, const glm::vec2& scale) { node.SetScale(scale); },
        [](EntityNode& node, float sx, float sy) { node.SetScale(sx, sy); });
    entity_node["SetSize"] = sol::overload(
        [](EntityNode& node, const glm::vec2& size) { node.SetSize(size); },
        [](EntityNode& node, float width, float height) { node.SetSize(width, height); });
    entity_node["SetTranslation"] = sol::overload(
        [](EntityNode& node, const glm::vec2& position) { node.SetTranslation(position); },
        [](EntityNode& node, float x, float y) { node.SetTranslation(x, y); });
    entity_node["Translate"] = sol::overload(
        [](EntityNode& node, const glm::vec2& delta) { node.Translate(delta); },
        [](EntityNode& node, float dx, float dy) { node.Translate(dx, dy); });
    entity_node["Rotate"] = &EntityNode::Rotate;
    entity_node["Grow"] = sol::overload(
        [](EntityNode& node, const glm::vec2& size) { node.Grow(size); },
        [](EntityNode& node, float dx, float dy) { node.Grow(dx, dy); } );

    auto entity_class = table.new_usertype<EntityClass>("EntityClass",
       sol::meta_function::index, &GetScriptVar<EntityClass>);
    entity_class["GetId"]       = &EntityClass::GetId;
    entity_class["GetName"]     = &EntityClass::GetName;
    entity_class["GetLifetime"] = &EntityClass::GetLifetime;
    entity_class["GetTag"]      = &EntityClass::GetTag;

    auto actuator_class = table.new_usertype<ActuatorClass>("ActuatorClass");
    actuator_class["GetName"]       = &ActuatorClass::GetName;
    actuator_class["GetId"]         = &ActuatorClass::GetId;
    actuator_class["GetNodeId"]     = &ActuatorClass::GetNodeId;
    actuator_class["GetStartTime"]  = &ActuatorClass::GetStartTime;
    actuator_class["GetDuration"]   = &ActuatorClass::GetDuration;
    actuator_class["GetType"]       = [](const ActuatorClass* klass) {
        return magic_enum::enum_name(klass->GetType());
    };

    auto actuator = table.new_usertype<Actuator>("Actuator");
    BindActuatorInterface<Actuator>(actuator);
    actuator["AsKinematicActuator"] = &ActuatorCast<KinematicActuator>;
    actuator["AsFlagActuator"]      = &ActuatorCast<SetFlagActuator>;
    actuator["AsValueActuator"]     = &ActuatorCast<SetValueActuator>;
    actuator["AsTransformActuator"] = &ActuatorCast<TransformActuator>;
    actuator["AsMaterialActuator"]  = &ActuatorCast<MaterialActuator>;

    auto kinematic_actuator = table.new_usertype<KinematicActuator>("KinematicActuator");
    BindActuatorInterface<KinematicActuator>(kinematic_actuator);

    auto setflag_actuator = table.new_usertype<SetFlagActuator>("SetFlagActuator");
    BindActuatorInterface<SetFlagActuator>(setflag_actuator);

    auto setvalue_actuator = table.new_usertype<SetValueActuator>("SetValueActuator");
    BindActuatorInterface<SetValueActuator>(setvalue_actuator);

    auto transform_actuator = table.new_usertype<TransformActuator>("TransformActuator");
    BindActuatorInterface<TransformActuator>(transform_actuator);
    transform_actuator["SetEndPosition"] = sol::overload(
        [](TransformActuator* actuator, const glm::vec2& position) {
            actuator->SetEndPosition(position);
        },
        [](TransformActuator* actuator, float x, float y) {
            actuator->SetEndPosition(x, y);
        });
    transform_actuator["SetEndSize"] = sol::overload(
        [](TransformActuator* actuator, const glm::vec2& size) {
            actuator->SetEndSize(size);
        },
        [](TransformActuator* actuator, float x, float y) {
            actuator->SetEndSize(x, y);
        });
    transform_actuator["SetEndScale"] = sol::overload(
        [](TransformActuator* actuator, const glm::vec2& scale) {
            actuator->SetEndScale(scale);
        },
        [](TransformActuator* actuator, float x, float y) {
            actuator->SetEndScale(x, y);
        });
    transform_actuator["SetEndRotation"] = &TransformActuator::SetEndRotation;

    auto material_actuator = table.new_usertype<MaterialActuator>("MaterialActuator");
    BindActuatorInterface<MaterialActuator>(material_actuator);

    auto animator = table.new_usertype<Animator>("Animator",
        sol::meta_function::index,     &GetAnimatorVar,
        sol::meta_function::new_index, &SetAnimatorVar);
    animator["GetName"]   = &Animator::GetName;
    animator["GetTime"]   = &Animator::GetTime;
    animator["HasValue"]  = &Animator::HasValue;
    animator["SetValue"]  = &SetAnimatorVar;
    animator["FindValue"] = &GetAnimatorVar;

    auto anim_class = table.new_usertype<AnimationClass>("AnimationClass");
    anim_class["GetName"]     = &AnimationClass::GetName;
    anim_class["GetId"]       = &AnimationClass::GetId;
    anim_class["GetDuration"] = &AnimationClass::GetDuration;
    anim_class["GetDelay"]    = &AnimationClass::GetDelay;
    anim_class["IsLooping"]   = &AnimationClass::IsLooping;

    auto anim = table.new_usertype<Animation>("Animation");
    anim["GetClassName"]       =  &Animation::GetClassName;
    anim["GetClassId"]         =  &Animation::GetClassId;
    anim["IsComplete"]         =  &Animation::IsComplete;
    anim["IsLooping"]          =  &Animation::IsLooping;
    anim["SetDelay"]           =  &Animation::SetDelay;
    anim["GetDelay"]           =  &Animation::GetDelay;
    anim["GetCurrentTime"]     =  &Animation::GetCurrentTime;
    anim["GetDuration"]        =  &Animation::GetDuration;
    anim["GetClass"]           =  &Animation::GetClass;
    anim["FindActuatorById"]   = sol::overload(
        [](game::Animation* animation, const std::string& name) {
            return animation->FindActuatorById(name);
        },
        [](game::Animation* animation, const std::string& name, const std::string& type, sol::this_state state) {
            sol::state_view lua(state);
            auto* actuator = animation->FindActuatorById(name);
            if (!actuator)
                return sol::make_object(lua, sol::nil);
            const auto enum_val = magic_enum::enum_cast<game::ActuatorClass::Type>(type);
            if (!enum_val.has_value())
                throw GameError("No such Actuator type: " + type);
            const auto value = enum_val.value();
            if (value == game::ActuatorClass::Type::Transform)
                return sol::make_object(lua, actuator->AsTransformActuator());
            else if (value == game::ActuatorClass::Type::Material)
                return sol::make_object(lua, actuator->AsMaterialActuator());
            else if (value== game::ActuatorClass::Type::Kinematic)
                return sol::make_object(lua, actuator->AsKinematicActuator());
            else if (value == game::ActuatorClass::Type::SetFlag)
                return sol::make_object(lua, actuator->AsFlagActuator());
            else if (value == game::ActuatorClass::Type::SetValue)
                return sol::make_object(lua, actuator->AsValueActuator());
            else BUG("Unhandled actuator type.");
        });
    anim["FindActuatorByName"] = sol::overload(
        [](game::Animation* animation, const std::string& name) {
           return animation->FindActuatorByName(name);
        },
        [](game::Animation* animation, const std::string& name, const std::string& type, sol::this_state state) {
            sol::state_view lua(state);
            auto* actuator = animation->FindActuatorByName(name);
            if (!actuator)
                return sol::make_object(lua, sol::nil);
            const auto enum_val = magic_enum::enum_cast<game::ActuatorClass::Type>(type);
            if (!enum_val.has_value())
                throw GameError("No such Actuator type: " + type);
            const auto value = enum_val.value();
            if (value == game::ActuatorClass::Type::Transform)
                return sol::make_object(lua, actuator->AsTransformActuator());
            else if (value == game::ActuatorClass::Type::Material)
                return sol::make_object(lua, actuator->AsMaterialActuator());
            else if (value== game::ActuatorClass::Type::Kinematic)
                return sol::make_object(lua, actuator->AsKinematicActuator());
            else if (value == game::ActuatorClass::Type::SetFlag)
                return sol::make_object(lua, actuator->AsFlagActuator());
            else if (value == game::ActuatorClass::Type::SetValue)
                return sol::make_object(lua, actuator->AsValueActuator());
            else BUG("Unhandled actuator type.");
        });

    auto entity = table.new_usertype<Entity>("Entity",
        sol::meta_function::index,     &GetScriptVar<Entity>,
        sol::meta_function::new_index, &SetScriptVar<Entity>);
    entity["GetName"]              = &Entity::GetName;
    entity["GetId"]                = &Entity::GetId;
    entity["GetTag"]               = &Entity::GetTag;
    entity["GetClassName"]         = &Entity::GetClassName;
    entity["GetClassId"]           = &Entity::GetClassId;
    entity["GetClass"]             = &Entity::GetClass;
    entity["GetNumNodes"]          = &Entity::GetNumNodes;
    entity["GetTime"]              = &Entity::GetTime;
    entity["GetLayer"]             = &Entity::GetLayer;
    entity["SetLayer"]             = &Entity::SetLayer;
    entity["IsDying"]              = &Entity::IsDying;
    entity["IsVisible"]            = &Entity::IsVisible;
    entity["IsAnimating"]          = &Entity::IsAnimating;
    entity["HasExpired"]           = &Entity::HasExpired;
    entity["HasBeenKilled"]        = &Entity::HasBeenKilled;
    entity["HasBeenSpawned"]       = &Entity::HasBeenSpawned;
    entity["HasAnimator"]          = &Entity::HasAnimator;
    entity["GetAnimator"]          = (Animator*(Entity::*)(void))&Entity::GetAnimator;
    entity["GetScene"]             = (Scene*(Entity::*)(void))&Entity::GetScene;
    entity["GetNode"]              = (EntityNode&(Entity::*)(size_t))&Entity::GetNode;
    entity["FindScriptVarById"]    = (ScriptVar*(Entity::*)(const std::string&))&Entity::FindScriptVarById;
    entity["FindScriptVarByName"]  = (ScriptVar*(Entity::*)(const std::string&))&Entity::FindScriptVarByName;
    entity["FindNodeByClassName"]  = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByClassName;
    entity["FindNodeByClassId"]    = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByClassId;
    entity["FindNodeByInstanceId"] = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByInstanceId;
    entity["PlayIdle"]             = &Entity::PlayIdle;
    entity["PlayAnimationByName"]  = &Entity::PlayAnimationByName;
    entity["PlayAnimationById"]    = &Entity::PlayAnimationById;
    entity["Die"]                  = &Entity::Die;
    entity["SetTag"]               = &Entity::SetTag;
    entity["TestFlag"]             = &TestFlag<Entity>;
    entity["SetFlag"]              = &SetFlag<Entity>;
    entity["SetVisible"]           = &Entity::SetVisible;
    entity["SetTimer"]             = &Entity::SetTimer;
    entity["PostEvent"]            = sol::overload(
        [](Entity* entity, const std::string& message, const std::string& sender, sol::object value) {
            Entity::PostedEvent event;
            event.message = message;
            event.sender  = sender;
            if (value.is<bool>())
                event.value = value.as<bool>();
            else if (value.is<int>())
                event.value = value.as<int>();
            else if (value.is<float>())
                event.value = value.as<float>();
            else if (value.is<std::string>())
                event.value = value.as<std::string>();
            else if (value.is<glm::vec2>())
                event.value = value.as<glm::vec2>();
            else if (value.is<glm::vec3>())
                event.value = value.as<glm::vec3>();
            else if (value.is<glm::vec4>())
                event.value = value.as<glm::vec4>();
            else throw GameError("Unsupported event event value type.");
            entity->PostEvent(std::move(event));
        },
        [](Entity* entity, const std::string& message, const std::string& sender) {
            Entity::PostedEvent event;
            event.message = message;
            event.sender  = sender,
            entity->PostEvent(std::move(event));
        },
        [](Entity* entity, const Entity::PostedEvent& event) {
            entity->PostEvent(event);
        });

    auto entity_posted_event = table.new_usertype<Entity::PostedEvent>("EntityEvent", sol::constructors<Entity::PostedEvent()>());
    entity_posted_event["message"] = &Entity::PostedEvent::message;
    entity_posted_event["sender"]  = &Entity::PostedEvent::sender;
    entity_posted_event["value"]   = &Entity::PostedEvent::value;

    auto entity_args = table.new_usertype<EntityArgs>("EntityArgs", sol::constructors<EntityArgs()>());
    entity_args["id"]       = sol::property(&EntityArgs::id);
    entity_args["class"]    = sol::property(&EntityArgs::klass);
    entity_args["name"]     = sol::property(&EntityArgs::name);
    entity_args["scale"]    = sol::property(&EntityArgs::scale);
    entity_args["position"] = sol::property(&EntityArgs::position);
    entity_args["rotation"] = sol::property(&EntityArgs::rotation);
    entity_args["logging"]  = sol::property(&EntityArgs::enable_logging);
    entity_args["layer"]    = sol::property(&EntityArgs::layer);

    using DynamicSpatialQueryResultSet = ResultSet<EntityNode*>;
    auto query_result_set = table.new_usertype<DynamicSpatialQueryResultSet>("SpatialQueryResultSet");
    query_result_set["IsEmpty"] = &DynamicSpatialQueryResultSet::IsEmpty;
    query_result_set["HasNext"] = &DynamicSpatialQueryResultSet::HasNext;
    query_result_set["Next"]    = &DynamicSpatialQueryResultSet::Next;
    query_result_set["Begin"]   = &DynamicSpatialQueryResultSet::BeginIteration;
    query_result_set["Get"]     = &DynamicSpatialQueryResultSet::GetCurrent;
    query_result_set["GetNext"] = &DynamicSpatialQueryResultSet::GetNext;
    query_result_set["Find"]    = [](DynamicSpatialQueryResultSet& results, const sol::function& predicate) {
        while (results.HasNext()) {
            EntityNode* node = results.GetNext();
            const bool this_is_it = predicate(node);
            if (this_is_it)
                return node;
        }
        return (EntityNode*)nullptr;
    };
    query_result_set["Filter"] = [](DynamicSpatialQueryResultSet& results, const sol::function& predicate) {
        results.BeginIteration();
        while (results.HasNext()) {
            EntityNode* node = results.GetCurrent();
            const bool keep_this = predicate(node);
            if (keep_this) {
                results.Next();
            } else  results.EraseCurrent();
        }
        results.BeginIteration();
    };

    auto script_var = table.new_usertype<ScriptVar>("ScriptVar");
    script_var["GetValue"]   = ObjectFromScriptVarValue;
    script_var["GetName"]    = &ScriptVar::GetName;
    script_var["GetId"]      = &ScriptVar::GetId;
    script_var["IsReadOnly"] = &ScriptVar::IsReadOnly;
    script_var["IsArray"]    = &ScriptVar::IsArray;
    script_var["IsPrivate"]  = &ScriptVar::IsPrivate;

    auto scene_class = table.new_usertype<SceneClass>("SceneClass",
        sol::meta_function::index,     &GetScriptVar<SceneClass>);
    scene_class["GetName"] = &SceneClass::GetName;
    scene_class["GetId"]   = &SceneClass::GetId;
    scene_class["GetNumScriptVars"] = &SceneClass::GetNumScriptVars;
    scene_class["GetScriptVar"]     = (const ScriptVar&(SceneClass::*)(size_t)const)&SceneClass::GetScriptVar;
    scene_class["FindScriptVarById"] = (const ScriptVar*(SceneClass::*)(const std::string&)const)&SceneClass::FindScriptVarById;
    scene_class["FindScriptVarByName"] = (const ScriptVar*(SceneClass::*)(const std::string&)const)&SceneClass::FindScriptVarByName;
    scene_class["GetLeftBoundary"] = [](const SceneClass& klass, sol::this_state state) {
        const float* val = klass.GetLeftBoundary();
        sol::state_view lua(state);
        return val ? sol::make_object(lua, *val)
                   : sol::make_object(lua, sol::nil);
    };
    scene_class["GetRightBoundary"] = [](const SceneClass& klass, sol::this_state state) {
        const float* val = klass.GetRightBoundary();
        sol::state_view lua(state);
        return val ? sol::make_object(lua, *val)
                   : sol::make_object(lua, sol::nil);
    };
    scene_class["GetTopBoundary"]  = [](const SceneClass& klass, sol::this_state state) {
        const float* val = klass.GetTopBoundary();
        sol::state_view lua(state);
        return val ? sol::make_object(lua, *val)
                   : sol::make_object(lua, sol::nil);
    };
    scene_class["GetBottomBoundary"] = [](const SceneClass& klass, sol::this_state state) {
        const float* val = klass.GetBottomBoundary();
        sol::state_view lua(state);
        return val ? sol::make_object(lua, *val)
                   : sol::make_object(lua, sol::nil);
    };

    using EntityList = ResultVector<Entity*>;
    auto entity_list = table.new_usertype<EntityList>("EntityList");
    entity_list["IsEmpty"] = &EntityList::IsEmpty;
    entity_list["HasNext"] = &EntityList::HasNext;
    entity_list["Next"]    = &EntityList::Next;
    entity_list["Begin"]   = &EntityList::BeginIteration;
    entity_list["Get"]     = &EntityList::GetCurrent;
    entity_list["GetAt"]   = &EntityList::GetAt;
    entity_list["Size"]    = &EntityList::GetSize;
    entity_list["GetNext"] = &EntityList::GetNext;
    entity_list["Join"]    = &EntityList::Join;
    entity_list["ForEach"] = [](EntityList& list, const sol::function& callback, sol::variadic_args args) {
        list.BeginIteration();
        while (list.HasNext()) {
            Entity* entity = list.GetNext();
            callback(entity, args);
        }
    };

    auto layer = table.new_usertype<TilemapLayer>("MapLayer");
    layer["GetClassName"]     = &TilemapLayer::GetClassName;
    layer["GetClassId"]       = &TilemapLayer::GetClassId;
    layer["GetWidth"]         = &TilemapLayer::GetWidth;
    layer["GetHeight"]        = &TilemapLayer::GetHeight;
    layer["GetTileSizeScale"] = &TilemapLayer::GetTileSizeScaler;
    layer["GetType"]          = [](TilemapLayer* map) { return base::ToString(map->GetType()); };


    auto map = table.new_usertype<Tilemap>("Map");
    map["GetClassName"]         = &Tilemap::GetClassName;
    map["GetClassId"]           = &Tilemap::GetClassId;
    map["GetNumLayers"]         = &Tilemap::GetNumLayers;
    map["GetMapWidth"]          = &Tilemap::GetMapWidth;
    map["GetMapHeight"]         = &Tilemap::GetMapHeight;
    map["GetTileWidth"]         = &Tilemap::GetTileWidth;
    map["GetTileHeight"]        = &Tilemap::GetTileHeight;
    map["GetPerspective"]       = [](Tilemap& map) { return base::ToString(map.GetPerspective()); };
    map["GetLayer"]             = (TilemapLayer&(Tilemap::*)(size_t))&Tilemap::GetLayer;
    map["FindLayerByClassName"] = (TilemapLayer*(Tilemap::*)(const std::string&))&Tilemap::FindLayerByClassName;
    map["FindLayerByClassId"]   = (TilemapLayer*(Tilemap::*)(const std::string&))&Tilemap::FindLayerByClassId;
    map["MapToTile"]            = sol::overload(
        [](Tilemap& map, TilemapLayer& layer, const glm::vec2& point) {
            const auto tile_width = map.GetTileWidth() * layer.GetTileSizeScaler();
            const auto tile_height = map.GetTileHeight() * layer.GetTileSizeScaler();
            const int row = tile_height / tile_height;
            const int col = tile_width / tile_width;
            return std::make_tuple(row, col);
        },
        [](Tilemap& map, TilemapLayer& layer, const base::FPoint& point) {
            const auto tile_width = map.GetTileWidth() * layer.GetTileSizeScaler();
            const auto tile_height = map.GetTileHeight() * layer.GetTileSizeScaler();
            const int row = tile_height / tile_height;
            const int col = tile_width / tile_width;
            return std::make_tuple(row, col);
        },
        [](Tilemap& map, TilemapLayer& layer, float x, float y) {
            const auto tile_width = map.GetTileWidth() * layer.GetTileSizeScaler();
            const auto tile_height = map.GetTileHeight() * layer.GetTileSizeScaler();
            const int row = tile_height / tile_height;
            const int col = tile_width / tile_width;
            return std::make_tuple(row, col);
        });
    map["ClampRowCol"] = [](Tilemap& map, TilemapLayer& layer, int row, int col) {
        const int max_cols = layer.GetWidth();
        const int max_rows = layer.GetHeight();
        row = math::clamp(0, max_rows-1, row);
        col = math::clamp(0, max_cols-1, col);
        return std::make_tuple(row, col);
    };
    map["MapPointFromScene"] = sol::overload(
        [](Tilemap& map, const glm::vec2& point) {
            const glm::vec2 ret = engine::SceneToTilePlane(glm::vec4{point.x, point.y, 0.0f, 1.0f}, map.GetPerspective());
            return ret;
        },
        [](Tilemap& map, const base::FPoint& point) {
            const glm::vec2 ret = engine::SceneToTilePlane(glm::vec4{point.GetX(), point.GetY(), 0.0f, 1.0f}, map.GetPerspective());
            return base::FPoint(ret.x, ret.y);
        });
    map["MapPointToScene"] = sol::overload(
        [](Tilemap& map, const glm::vec2& point) {
            const glm::vec2 ret = engine::TilePlaneToScene(glm::vec4{point.x, point.y, 0.0f, 1.0f}, map.GetPerspective());
            return ret;
        },
        [](Tilemap& map, const base::FPoint& point) {
            const glm::vec2 ret = engine::TilePlaneToScene(glm::vec4{point.GetX(), point.GetY(), 0.0f, 1.0f}, map.GetPerspective());
            return base::FPoint(ret.x, ret.y);
        });
    map["MapVectorFromScene"] = sol::overload(
        [](Tilemap& map, const glm::vec2& vector) {
            const glm::vec2 ret = engine::SceneToTilePlane(glm::vec4{vector.x, vector.y, 0.0f, 1.0f}, map.GetPerspective());
            return ret;
        });
    map["MapVectorToScene"] = sol::overload(
        [](Tilemap& map, const glm::vec2& vector) {
            const glm::vec2 ret = engine::TilePlaneToScene(glm::vec4{vector.x, vector.y, 0.0f, 1.0f}, map.GetPerspective());
            return ret;
        });

    auto scene = table.new_usertype<Scene>("Scene",
       sol::meta_function::index,     &GetScriptVar<Scene>,
       sol::meta_function::new_index, &SetScriptVar<Scene>);
    scene["ListEntitiesByClassName"]    = [](Scene& scene, const std::string& name) {
        return EntityList(scene.ListEntitiesByClassName(name));
    };
    scene["ListEntitiesByTag"] = [](Scene& scene, const std::string& tag) {
        return EntityList(scene.ListEntitiesByTag(tag));
    };
    scene["GetMap"]                     = (Tilemap*(Scene::*)())&Scene::GetMap;
    scene["GetTime"]                    = &Scene::GetTime;
    scene["GetClassName"]               = &Scene::GetClassName;
    scene["GetClassId"]                 = &Scene::GetClassId;
    scene["GetClass"]                   = &Scene::GetClass;
    scene["GetNumEntities"]             = &Scene::GetNumEntities;
    scene["GetEntity"]                  = (Entity&(Scene::*)(size_t))&Scene::GetEntity;
    scene["FindEntityByInstanceId"]     = (Entity*(Scene::*)(const std::string&))&Scene::FindEntityByInstanceId;
    scene["FindEntityByInstanceName"]   = (Entity*(Scene::*)(const std::string&))&Scene::FindEntityByInstanceName;
    scene["FindScriptVarById"]          = (ScriptVar*(Scene::*)(const std::string&))&Scene::FindScriptVarById;
    scene["FindScriptVarByName"]        = (ScriptVar*(Scene::*)(const std::string&))&Scene::FindScriptVarByName;
    scene["KillEntity"]                 = &Scene::KillEntity;
    scene["FindEntityTransform"]        = &Scene::FindEntityTransform;
    scene["FindEntityNodeTransform"]    = &Scene::FindEntityNodeTransform;
    scene["FindEntityBoundingRect"]     = &Scene::FindEntityBoundingRect;
    scene["FindEntityNodeBoundingRect"] = &Scene::FindEntityNodeBoundingRect;
    scene["FindEntityNodeBoundingBox"]  = &Scene::FindEntityNodeBoundingBox;
    scene["MapVectorFromEntityNode"]    = &Scene::MapVectorFromEntityNode;
    scene["MapPointFromEntityNode"]     = sol::overload(
        [](Scene& scene, const Entity* entity, const EntityNode* node, const base::FPoint& point) {
            return scene.MapPointFromEntityNode(entity, node, point);
        },
        [](Scene& scene, const Entity* entity, const EntityNode* node, const glm::vec2& point) {
            return scene.MapPointFromEntityNode(entity, node, point);
        });
    scene["SpawnEntity"] = sol::overload(
        [](Scene& scene, const EntityArgs& args) {
            if (args.klass == nullptr)
            {
                ERROR("Failed to spawn entity. Entity class is nil.");
                return (game::Entity*)nullptr;
            }
            return scene.SpawnEntity(args, true);
        },
        [](Scene& scene, const EntityArgs& args, bool link) {
            return scene.SpawnEntity(args, link);
        },
        [](Scene& scene, const std::string& klass, sol::this_state this_state) {
            sol::state_view L(this_state);
            ClassLibrary* classlib = L["ClassLib"];

            game::EntityArgs args;
            args.klass = classlib->FindEntityClassByName(klass);
            if (!args.klass)
            {
                ERROR("Failed to spawn entity. No such entity class. [klass='%1']", klass);
                return (game::Entity*)nullptr;
            }
            return scene.SpawnEntity(args);
        },
        [](Scene& scene, const std::string& klass, const sol::table& args_table, sol::this_state this_state) {
            sol::state_view L(this_state);
            ClassLibrary* classlib = L["ClassLib"];

            game::EntityArgs args;
            args.klass = classlib->FindEntityClassByName(klass);
            if (!args.klass)
            {
                ERROR("Failed to spawn entity. No such entity class. [klass='%1']", klass);
                return (game::Entity*)nullptr;
            }

            args.id             = args_table.get_or("id", std::string(""));
            args.name           = args_table.get_or("name", std::string(""));
            args.layer          = args_table.get_or("layer", 0);
            args.scale.x        = args_table.get_or("sx", 1.0f);
            args.scale.y        = args_table.get_or("sy", 1.0f);
            args.position.x     = args_table.get_or("x", 0.0f);
            args.position.y     = args_table.get_or("y", 0.0f);
            args.rotation       = args_table.get_or("r", 0.0f);
            args.enable_logging = args_table.get_or("logging", false);

            const bool link_to_root = args_table.get_or("link", true);
            const glm::vec2* pos = nullptr;
            const glm::vec2* scale = nullptr;
            if (const auto* ptr = args_table.get_or("pos", pos))
                args.position = *ptr;
            if (const auto* ptr = args_table.get_or("scale", scale))
                args.scale = *ptr;

            return scene.SpawnEntity(args, link_to_root);
        });

    scene["QuerySpatialNodes"] = sol::overload(
        [](Scene& scene, const base::FPoint& a, const base::FPoint& b, const std::string& mode) {
            const auto enum_val = magic_enum::enum_cast<game::Scene::SpatialQueryMode>(mode);
            if (!enum_val.has_value())
                throw GameError("No such spatial query mode: " + mode);

            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(a, b, &result, enum_val.value());
            return std::make_unique<DynamicSpatialQueryResultSet>(std::move(result));
        },
        [](Scene& scene, const glm::vec2& a, const glm::vec2& b, const std::string& mode) {
            const auto enum_val = magic_enum::enum_cast<game::Scene::SpatialQueryMode>(mode);
            if (!enum_val.has_value())
                throw GameError("No such spatial query mode: " + mode);

            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(base::FPoint(a.x, a.y), base::FPoint(b.x, b.y), &result, enum_val.value());
            return std::make_unique<DynamicSpatialQueryResultSet>(std::move(result));
        },

        [](Scene& scene, const base::FPoint& point, const std::string& mode) {
            const auto enum_val = magic_enum::enum_cast<game::Scene::SpatialQueryMode>(mode);
            if (!enum_val.has_value())
                throw GameError("No such spatial query mode: " + mode);

            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(point, &result, enum_val.value());
            return std::make_unique<DynamicSpatialQueryResultSet>(std::move(result));
        },
        [](Scene& scene, const base::FPoint& point, float radius, const std::string& mode) {
            const auto enum_val = magic_enum::enum_cast<game::Scene::SpatialQueryMode>(mode);
            if (!enum_val.has_value())
                throw GameError("No such spatial query mode: " + mode);

            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(point, radius, &result, enum_val.value());
            return std::make_unique<DynamicSpatialQueryResultSet>(std::move(result));
        },
        [](Scene& scene, const glm::vec2& point, const std::string& mode) {
            const auto enum_val = magic_enum::enum_cast<game::Scene::SpatialQueryMode>(mode);
            if (!enum_val.has_value())
                throw GameError("No such spatial query mode: " + mode);

            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(base::FPoint(point.x, point.y), &result, enum_val.value());
            return std::make_unique<DynamicSpatialQueryResultSet>(std::move(result));
        },
        [](Scene& scene, const glm::vec2& point, float radius, const std::string& mode) {
            const auto enum_val = magic_enum::enum_cast<game::Scene::SpatialQueryMode>(mode);
            if (!enum_val.has_value())
                throw GameError("No such spatial query mode: " + mode);

            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(base::FPoint(point.x, point.y), radius, &result, enum_val.value());
            return std::make_unique<DynamicSpatialQueryResultSet>(std::move(result));
        },
        [](Scene& scene, const base::FRect& rect) {
            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(rect, &result);
            return std::make_unique<DynamicSpatialQueryResultSet>(std::move(result));
        }
    );

    auto physics = table.new_usertype<PhysicsEngine>("Physics");
    physics["ApplyImpulseToCenter"] = sol::overload(
        [](PhysicsEngine& self, const std::string& id, const glm::vec2& vec) {
            return self.ApplyImpulseToCenter(id, vec);
        },
        [](PhysicsEngine& self, const EntityNode& node, const glm::vec2& vec) {
            return self.ApplyImpulseToCenter(node, vec);
        });
    physics["ApplyForceToCenter"] = sol::overload(
        [](PhysicsEngine& self, const std::string& id, const glm::vec2& vec) {
            return self.ApplyForceToCenter(id, vec);
        },
        [](PhysicsEngine& self, const EntityNode& node, const glm::vec2& vec) {
            return self.ApplyForceToCenter(node, vec);
        });
    physics["SetLinearVelocity"] = sol::overload(
        [](PhysicsEngine& self, const std::string& id, const glm::vec2& vector) {
            return self.SetLinearVelocity(id, vector);
        },
        [](PhysicsEngine& self, const EntityNode& node, const glm::vec2& vector) {
            return self.SetLinearVelocity(node, vector);
        });
    physics["FindCurrentLinearVelocity"] = sol::overload(
        [](PhysicsEngine& self, const std::string& id) {
            return self.FindCurrentLinearVelocity(id);
        },
        [](PhysicsEngine& self, const EntityNode& node) {
            return self.FindCurrentLinearVelocity(node);
        }
    );
    physics["FindCurrentAngularVelocity"] = sol::overload(
        [](PhysicsEngine& self, const std::string& id) {
            return self.FindCurrentAngularVelocity(id);
        },
        [](PhysicsEngine& self, const EntityNode& node) {
            return self.FindCurrentAngularVelocity(node);
        }
    );
    physics["FindMass"] = sol::overload(
        [](PhysicsEngine& self, const std::string& id) {
            return self.FindMass(id);
        },
        [](PhysicsEngine& self, const EntityNode& node) {
            return self.FindMass(node);
        }
    );

    auto ray_cast_result = table.new_usertype<PhysicsEngine::RayCastResult>("RayCastResult");
    ray_cast_result["node"]     = &PhysicsEngine::RayCastResult::node;
    ray_cast_result["point"]    = &PhysicsEngine::RayCastResult::point;
    ray_cast_result["normal"]   = &PhysicsEngine::RayCastResult::normal;
    ray_cast_result["fraction"] = &PhysicsEngine::RayCastResult::fraction;

    using RayCastResultVector = ResultVector<PhysicsEngine::RayCastResult>;
    auto ray_cast_result_vector = table.new_usertype<RayCastResultVector>("RayCastResultVector");
    ray_cast_result_vector["IsEmpty"] = &RayCastResultVector::IsEmpty;
    ray_cast_result_vector["HasNext"] = &RayCastResultVector::HasNext;
    ray_cast_result_vector["Next"]    = &RayCastResultVector::Next;
    ray_cast_result_vector["Begin"]   = &RayCastResultVector::BeginIteration;
    ray_cast_result_vector["Get"]     = &RayCastResultVector::GetCurrent;
    ray_cast_result_vector["GetAt"]   = &RayCastResultVector::GetAt;
    ray_cast_result_vector["Size"]    = &RayCastResultVector::GetSize;
    ray_cast_result_vector["GetNext"] = &RayCastResultVector::GetNext;

    physics["RayCast"] = sol::overload(
        [](PhysicsEngine& self, const glm::vec2& start, const glm::vec2& end, const std::string& mode) {
            const auto enum_val = magic_enum::enum_cast<PhysicsEngine::RayCastMode>(mode);
            if (!enum_val.has_value())
                throw GameError("No such ray cast mode: " + mode);
            std::vector<PhysicsEngine::RayCastResult> result;
            self.RayCast(start, end, &result, enum_val.value());
            return std::make_unique<RayCastResultVector>(std::move(result));
        },
        [](PhysicsEngine& self, const glm::vec2& start, const glm::vec2& end) {
            std::vector<PhysicsEngine::RayCastResult> result;
            self.RayCast(start, end, &result, PhysicsEngine::RayCastMode::All);
            return std::make_unique<RayCastResultVector>(std::move(result));
        });

    physics["GetScale"]   = &PhysicsEngine::GetScale;
    physics["GetGravity"] = &PhysicsEngine::GetGravity;
    physics["GetTimeStep"] = &PhysicsEngine::GetTimeStep;
    physics["GetNumPositionIterations"] = &PhysicsEngine::GetNumPositionIterations;
    physics["GetNumVelocityIterations"] = &PhysicsEngine::GetNumVelocityIterations;
    physics["MapVectorFromGame"] = &PhysicsEngine::MapVectorFromGame;
    physics["MapVectorToGame"]   = &PhysicsEngine::MapVectorToGame;
    physics["MapAngleFromGame"]  = &PhysicsEngine::MapAngleFromGame;
    physics["MapAngleToGame"]    = &PhysicsEngine::MapAngleToGame;
    physics["SetGravity"]        = &PhysicsEngine::SetGravity;
    physics["SetScale"]          = &PhysicsEngine::SetScale;

    auto audio = table.new_usertype<AudioEngine>("Audio");
    audio["PrepareMusicGraph"] = sol::overload(
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass) {
                if (!klass)
                {
                    ERROR("Failed to prepare music graph. Audio graph is nil.");
                    return false;
                }
                return engine.PrepareMusicGraph(klass);
            },
            [](AudioEngine& engine, const std::string& name) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);

                if (!klass)
                {
                    ERROR("Failed to prepare music graph. No such audio graph class. [class='%1']", name);
                    return false;
                }
                return engine.PrepareMusicGraph(klass);
            });
    audio["PlayMusic"] = sol::overload(
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass) {
                if (!klass)
                {
                    ERROR("Failed to play music. Audio graph is nil.");
                    return false;
                }
                return engine.PlayMusic(klass);
            },
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass, unsigned when) {
                if (!klass)
                {
                    ERROR("Failed to play music. Audio graph is nil.");
                    return false;
                }
                return engine.PlayMusic(klass, when);
            },
            [](AudioEngine& engine, const std::string& name, unsigned when) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                {
                    ERROR("Failed to play music. No such audio graph class. [class='%1']", name);
                    return false;
                }
                return engine.PlayMusic(klass, when);
            },
            [](AudioEngine& engine, const std::string& name) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                {
                    ERROR("Failed to play music. No such audio graph class. [class='%1']", name);
                    return false;
                }
                return engine.PlayMusic(klass);
            });

    audio["ResumeMusic"] = sol::overload(
            [](AudioEngine& engine, const std::string& track, unsigned when) {
                engine.ResumeMusic(track, when);
            },
            [](AudioEngine& engine, const std::string& track) {
                engine.ResumeMusic(track, 0);
            });
    audio["PauseMusic"] = sol::overload(
            [](AudioEngine& engine, const std::string& track, unsigned when) {
                engine.PauseMusic(track, when);
            },
            [](AudioEngine& engine, const std::string& track) {
                engine.PauseMusic(track, 0);
            });
    audio["KillMusic"] = sol::overload(
            [](AudioEngine& engine, const std::string& track, unsigned when) {
                engine.KillMusic(track, when);
            },
            [](AudioEngine& engine, const std::string& track) {
                engine.KillMusic(track, 0);
            });

    audio["CancelMusicCmds"] = &AudioEngine::CancelMusicCmds;
    audio["SetMusicGain"]   = &AudioEngine::SetMusicGain;
    audio["SetMusicEffect"] = [](AudioEngine& engine,
                                 const std::string& track,
                                 const std::string& effect,
                                 unsigned duration) {
        const auto effect_value = magic_enum::enum_cast<AudioEngine::Effect>(effect);
        if (!effect_value.has_value())
            throw GameError("No such audio effect:" + effect);
        engine.SetMusicEffect(track, duration, effect_value.value());
    };
    audio["PlaySoundEffect"] = sol::overload(
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass, unsigned when) {
                if (!klass)
                {
                    ERROR("Failed to play audio effect. Audio graph is nil.");
                    return false;
                }
                return engine.PlaySoundEffect(klass, when);
            },
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass) {
                if (!klass)
                {
                    ERROR("Failed to play audio effect. Audio graph is nil.");
                    return false;
                }
                return engine.PlaySoundEffect(klass, 0);
            },
            [](AudioEngine& engine, const std::string& name, unsigned when) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                {
                    ERROR("Failed to play audio effect. No such audio effect graph. [name='%1']", name);
                    return false;
                }
                return engine.PlaySoundEffect(klass, when);
            },
            [](AudioEngine& engine, const std::string& name) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                {
                    ERROR("Failed to play audio effect. No such audio effect graph. [name='%1']", name);
                    return false;
                }
                return engine.PlaySoundEffect(klass, 0);
            });
    audio["SetSoundEffectGain"] = &AudioEngine::SetSoundEffectGain;
    audio["EnableEffects"]      = &AudioEngine::EnableEffects;

    auto audio_event = table.new_usertype<AudioEvent>("AudioEvent",
        sol::meta_function::index, [&L](const AudioEvent& event, const char* key) {
                sol::state_view lua(L);
                if (!std::strcmp(key, "type"))
                    return sol::make_object(lua, base::ToString(event.type));
                else if (!std::strcmp(key, "track"))
                    return sol::make_object(lua, event.track);
                else if (!std::strcmp(key, "source"))
                    return sol::make_object(lua, event.source);
                throw GameError(base::FormatString("No such audio event index: %1", key));
            }
    );

    auto mouse_event = table.new_usertype<MouseEvent>("MouseEvent");
    mouse_event["window_coord"] = &MouseEvent::window_coord;
    mouse_event["scene_coord"]  = &MouseEvent::scene_coord;
    mouse_event["map_coord"]    = &MouseEvent::map_coord;
    mouse_event["button"]       = &MouseEvent::btn;
    mouse_event["modifiers"]    = &MouseEvent::mods;
    mouse_event["over_scene"]   = &MouseEvent::over_scene;

    auto game_event = table.new_usertype<GameEvent>("GameEvent", sol::constructors<GameEvent()>(),
        sol::meta_function::index, [&L](const GameEvent& event, const char* key) {
            sol::state_view lua(L);
            if (!std::strcmp(key, "from"))
                return sol::make_object(lua, event.from);
            else if (!std::strcmp(key, "to"))
                return sol::make_object(lua, event.to);
            else if (!std::strcmp(key ,"message"))
                return sol::make_object(lua, event.message);

            const auto* val = base::SafeFind(event.values, std::string(key));
            return val ? sol::make_object(lua, *val)
                       : sol::make_object(lua, sol::nil);
        },
        sol::meta_function::new_index, [&L](GameEvent& event, const char* key, sol::object value) {
            if (!std::strcmp(key, "from")) {
                if (value.is<std::string>())
                    event.from = value.as<std::string>();
                else if (value.is<game::Scene*>())
                    event.from = value.as<game::Scene*>();
                else if (value.is<game::Entity*>())
                    event.from = value.as<game::Entity*>();
                else throw GameError("Unsupported game event from field type.");
            } else if (!std::strcmp(key, "to")) {
                if (value.is<std::string>())
                    event.to = value.as<std::string>();
                else if (value.is<game::Scene*>())
                    event.to = value.as<game::Scene*>();
                else if (value.is<game::Entity*>())
                    event.to = value.as<game::Entity*>();
                else throw GameError("Unsupported game event to field type.");
            } else if (!std::strcmp(key, "message")) {
                if (value.is<std::string>())
                    event.message = value.as<std::string>();
                else throw GameError("Unsupported game event message field type.");
            } else {
                const auto& k = std::string(key);
                if (value.is<bool>())
                    event.values[key] = value.as<bool>();
                else if (value.is<int>())
                    event.values[key] = value.as<int>();
                else if (value.is<float>())
                    event.values[key] = value.as<float>();
                else if (value.is<std::string>())
                    event.values[key] = value.as<std::string>();
                else if(value.is<glm::vec2>())
                    event.values[key] = value.as<glm::vec2>();
                else if (value.is<glm::vec3>())
                    event.values[key] = value.as<glm::vec3>();
                else if (value.is<glm::vec4>())
                    event.values[key] = value.as<glm::vec4>();
                else if (value.is<base::Color4f>())
                    event.values[key] = value.as<base::Color4f>();
                else if (value.is<base::FSize>())
                    event.values[key] = value.as<base::FSize>();
                else if (value.is<base::FRect>())
                    event.values[key] = key, value.as<base::FRect>();
                else if (value.is<base::FPoint>())
                    event.values[key] = value.as<base::FPoint>();
                else if (value.is<game::Entity*>())
                    event.values[key] = value.as<game::Entity*>();
                else if (value.is<game::Scene*>())
                    event.values[key] = value.as<game::Scene*>();
                else throw GameError("Unsupported game event value type.");
            }
        });

    auto kvstore = table.new_usertype<KeyValueStore>("KeyValueStore", sol::constructors<KeyValueStore()>(),
        sol::meta_function::index, [](const KeyValueStore& kv, const char* key, sol::this_state state) {
            sol::state_view lua(state);
            KeyValueStore::Value value;
            if (!kv.GetValue(key, &value))
                throw GameError("No such key value store index: " + std::string(key));
            return sol::make_object(lua, value);
        },
        sol::meta_function::new_index, [&L](KeyValueStore& kv, const char* key, sol::object value) {
            SetKvValue(kv, key, value);
        }
    );
    kvstore["SetValue"] = SetKvValue;
    kvstore["HasValue"] = &KeyValueStore::HasValue;
    kvstore["Clear"]    = &KeyValueStore::Clear;
    kvstore["Persist"]  = sol::overload(
        [](const KeyValueStore& kv, data::JsonObject& json) {
            kv.Persist(json);
        },
        [](const KeyValueStore& kv, data::Writer& writer) {
            kv.Persist(writer);
        });

    kvstore["Restore"]  = sol::overload(
            [](KeyValueStore& kv, const data::JsonObject& json) {
                return kv.Restore(json);
            },
            [](KeyValueStore& kv, const data::Reader& reader) {
                return kv.Restore(reader);
            });
    kvstore["GetValue"] = sol::overload(
        [](const KeyValueStore& kv, const char* key, sol::this_state state) {
            sol::state_view lua(state);
            KeyValueStore::Value value;
            if (!kv.GetValue(key, &value))
                throw GameError("No such key value key: " + std::string(key));
            return sol::make_object(lua, value);
        },
        [](KeyValueStore& kv, const char* key, sol::this_state state, sol::object value) {
            sol::state_view lua(state);
            KeyValueStore::Value val;
            if (kv.GetValue(key, &val))
                return sol::make_object(lua, val);
            SetKvValue(kv, key, value);
            return value;
        });
    kvstore["InitValue"] = [](KeyValueStore& kv, const char* key, sol::object value) {
        if (kv.HasValue(key))
            return;
        SetKvValue(kv, key, value);
    };
}

} // namespace