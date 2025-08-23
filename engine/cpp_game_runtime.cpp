// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-20215 Ensisoft http://www.ensisoft.com
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

#include <queue>

#include "base/logging.h"
#include "game/entity.h"
#include "game/entity_class.h"
#include "game/scene.h"
#include "game/scene_class.h"
#include "engine/classlib.h"
#include "engine/entity_script.h"
#include "engine/game.h"
#include "engine/context.h"
#include "engine/cpp_game_runtime.h"

namespace engine
{

class CppRuntime::Context : public RuntimeContext {
public:
    const PhysicsEngine* GetPhysics() const noexcept override
    {
        return mPhysics;
    }
    const AudioEngine* GetAudio() const noexcept override
    {
        return mAudio;
    }
    const ClassLibrary* GetClassLib() const noexcept override
    {
        return mClassLib;
    }

    game::Scene* GetScene() noexcept override
    {
        return mScene;
    }

    bool EditingMode() const noexcept override
    {
        return mEditMode;
    }
    bool PreviewMode() const noexcept override
    {
        return mPreviewMode;
    }

    void PostEvent(const GameEvent& event) override
    {
        PostEventAction action;
        action.event = event;
        mActionQueue.push(std::move(action));
    }

    void DebugPrint(const std::string& message) override
    {
        DebugPrintAction action;
        action.message = message;
        mActionQueue.push(std::move(action));
    }
    void DebugPrint(std::string&& message) override
    {
        DebugPrintAction action;
        action.message = std::move(message);
        mActionQueue.push(std::move(action));
    }
public:
    const ClassLibrary* mClassLib = nullptr;
    PhysicsEngine* mPhysics = nullptr;
    AudioEngine* mAudio = nullptr;
    bool mEditMode = false;
    bool mPreviewMode = false;
public:
    game::Scene* mScene = nullptr;
public:
    std::queue<Action> mActionQueue;
};

CppRuntime::CppRuntime()
{
    DEBUG("Create cpp runtime");
    mContext = std::make_unique<CppRuntime::Context>();
    SetContext(mContext.get());
}

CppRuntime::~CppRuntime()
{
    SetContext(nullptr);

    DEBUG("Destroy cpp runtime");
}

void CppRuntime::SetClassLibrary(const ClassLibrary* classlib)
{
    mClassLib = classlib;
    mContext->mClassLib = classlib;
}
void CppRuntime::SetPhysicsEngine(PhysicsEngine* engine)
{
    mContext->mPhysics = engine;
}

void CppRuntime::SetAudioEngine(AudioEngine* audio)
{
    mContext->mAudio = audio;
}

void CppRuntime::SetEditingMode(bool editing)
{
    mContext->mEditMode = editing;
}
void CppRuntime::SetPreviewMode(bool preview)
{
    mContext->mPreviewMode = preview;
}

void CppRuntime::Init()
{
    std::vector<EntityScriptRegistration> scripts;
    GetEntityScripts(&scripts);

    for (auto& reg : scripts)
    {
        const auto& classId = reg.classId;
        const auto& klass = mClassLib->FindEntityClassById(classId);
        if (klass)
        {
            DEBUG("Loading native entity script. [id=%1, name=%2]",
                  classId, klass->GetClassName());
            mEntityScripts[classId] = std::move(reg.script);
        }
        else
        {
            WARN("Loading native entity script failed. No such entity class. [id=%1]", classId);
        }
    }
}

void CppRuntime::BeginLoop()
{
    if (mEntityScripts.empty())
        return;

    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto& entity = mScene->GetEntity(i);
        if (!entity.TestFlag(game::Entity::ControlFlags::Spawned))
            continue;

        if (auto* script = GetScript(entity))
        {
            script->BeginPlay(entity, mScene, mTilemap);
        }
    }
}

void CppRuntime::EndLoop()
{
    if (mEntityScripts.empty())
        return;

    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto& entity = mScene->GetEntity(i);
        if (!entity.TestFlag(game::Entity::ControlFlags::Killed))
            continue;

        if (auto* script = GetScript(entity))
        {
            script->EndPlay(entity, mScene, mTilemap);
        }
    }
}

void CppRuntime::BeginPlay(game::Scene* scene, game::Tilemap* map)
{
    mScene = scene;
    mTilemap = map;
    mContext->mScene = scene;

    if (mEntityScripts.empty())
        return;

    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto& entity = mScene->GetEntity(i);
        if (auto* script = GetScript(entity))
        {
            script->BeginPlay(entity, mScene, map);
        }
    }
}
void CppRuntime::EndPlay(game::Scene* scene, game::Tilemap* map)
{

    mTilemap = nullptr;
    mScene = nullptr;

    mContext->mScene = nullptr;
}
void CppRuntime::Update(double game_time, double dt)
{
    if (!mScene)
        return;

    if (mEntityScripts.empty())
        return;

    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto& entity = mScene->GetEntity(i);
        if (!entity.TestFlag(game::Entity::Flags::UpdateEntity))
            continue;

        if (auto* script = GetScript(entity))
        {
            script->Update(entity, game_time, dt);
        }
    }
}

void CppRuntime::Tick(double game_time, double dt)
{
    if (!mScene)
        return;

    if (mEntityScripts.empty())
        return;

    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto& entity = mScene->GetEntity(i);
        if (!entity.TestFlag(game::Entity::Flags::TickEntity))
            continue;

        if (auto* script = GetScript(entity))
        {
            script->Tick(entity, game_time, dt);
        }
    }
}

bool CppRuntime::GetNextAction(Action* out)
{
    if (mContext->mActionQueue.empty())
        return false;
    *out = std::move(mContext->mActionQueue.front());
    mContext->mActionQueue.pop();
    return true;
}

EntityScript* CppRuntime::GetScript(const game::Entity& entity)
{
    if (auto* ptr = base::SafeFind(mEntityScripts, entity.GetClassId()))
        return ptr;
    return nullptr;
}

} // namespace