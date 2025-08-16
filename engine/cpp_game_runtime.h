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

#pragma once

#include "config.h"

#include <unordered_map>

#include "game/fwd.h"
#include "game/types.h"
#include "game/timeline_animation.h"
#include "engine/runtime.h"
#include "engine/action.h"
#include "engine/entity_script.h"

namespace engine
{
    class CppRuntime final : public GameRuntime
    {
    public:
        CppRuntime();
       ~CppRuntime();

       void SetClassLibrary(const ClassLibrary* classlib) override;
       void SetPhysicsEngine(PhysicsEngine* physics) override;
       void SetAudioEngine(AudioEngine* audio) override;
       void SetEditingMode(bool editing) override;
       void SetPreviewMode(bool preview) override;

        void Init() override;

        void BeginLoop() override;
        void EndLoop() override;

        void BeginPlay(game::Scene* scene, game::Tilemap* map) override;
        void EndPlay(game::Scene* scene, game::Tilemap* map) override;

        void Update(double game_time, double dt) override;
        void Tick(double game_Time, double dt) override;

        bool GetNextAction(Action* out) override;

    private:
        class Context;

    private:
        EntityScript* GetScript(const game::Entity& entity);
    private:
        const ClassLibrary* mClassLib = nullptr;

        game::Scene* mScene = nullptr;
        game::Tilemap* mTilemap = nullptr;
        std::unique_ptr<Context> mContext;
        std::unordered_map<std::string, std::unique_ptr<EntityScript>> mEntityScripts;
    };
} // namespace
