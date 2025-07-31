// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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
#  include <sol/forward.hpp>
#include "warnpop.h"

#include <memory>
#include <queue>
#include <vector>
#include <unordered_map>

#include "game/types.h"
#include "game/animation.h"
#include "engine/runtime.h"
#include "engine/action.h"

namespace engine
{
    // Implementation of GameRuntime using Lua.
    // Delegates the calls to Lua scripts associated
    // with the entities, UIs and main game.
    class LuaRuntime final : public GameRuntime
    {
    public:
        using Action = engine::Action;
        LuaRuntime(const std::string& lua_path,
                   const std::string& game_script,
                   const std::string& game_home,
                   const std::string& game_name);
       ~LuaRuntime() override;

        void SetFrameNumber(unsigned frame) override;
        void SetSurfaceSize(unsigned width, unsigned height) override;
        void SetEditingMode(bool editing) override;
        void SetPreviewMode(bool preview) override;
        void SetClassLibrary(const ClassLibrary* classlib) override;
        void SetPhysicsEngine(PhysicsEngine* engine) override;
        void SetAudioEngine(AudioEngine* engine) override;
        void SetDataLoader(const Loader* loader) override;
        void SetStateStore(KeyValueStore* store) override;
        void SetCurrentUI(uik::Window* window) override;
        void Init() override;
        bool LoadGame() override;
        void StartGame() override;
        void SaveGame() override;
        void StopGame() override;
        void BeginPlay(game::Scene* scene, game::Tilemap* map) override;
        void EndPlay(game::Scene* scene, game::Tilemap* map) override;
        void Tick(double game_time, double dt) override;
        void Update(double game_time, double dt) override;
        void PostUpdate(double game_time) override;
        void BeginLoop() override;
        void EndLoop() override;
        bool GetNextAction(Action* out) override;
        bool GetCamera(Camera* cam) const override;
        void TransferDebugQueue(std::vector<DebugDrawCmd>* out) override;

        void OnContactEvent(const std::vector<ContactEvent>& contacts) override;
        void OnGameEvent(const GameEvent& event) override;
        void OnAudioEvent(const AudioEvent& event) override;
        void OnSceneEvent(const std::vector<game::Scene::Event>& events) override;
        void OnKeyDown(const wdk::WindowEventKeyDown& key) override;
        void OnKeyUp(const wdk::WindowEventKeyUp& key) override;
        void OnChar(const wdk::WindowEventChar& text) override;
        void OnMouseMove(const MouseEvent& mouse) override;
        void OnMousePress(const MouseEvent& mouse) override;
        void OnMouseRelease(const MouseEvent& mouse) override;
        void UpdateUI(uik::Window* ui, double game_time, double dt) override;
        void OnUIOpen(uik::Window* ui) override;
        void OnUIClose(uik::Window* ui, int result) override;
        void OnUIAction(uik::Window* ui, const WidgetActionList& actions) override;
        void OnContentClassUpdate(const ContentClass& klass) override;

        bool HasAction() const
        { return !mActionQueue.empty(); }
    private:
        sol::environment* GetTypeEnv(const game::EntityStateControllerClass& klass);
        sol::environment* GetTypeEnv(const game::EntityClass& klass);
        sol::environment* GetTypeEnv(const uik::Window& window);
        sol::object CallCrossEnvMethod(sol::object object, const std::string& method, sol::variadic_args va);
        template<typename KeyEvent>
        void DispatchKeyboardEvent(const char* method, const KeyEvent& key);
        void DispatchMouseEvent(const char* method, const MouseEvent& mouse);
    private:
        const std::string mLuaPath;
        const std::string mGameScript;
        const std::string mGameHome;
        const std::string mGameName;
        const ClassLibrary* mClassLib = nullptr;
        PhysicsEngine* mPhysicsEngine = nullptr;
        AudioEngine* mAudioEngine = nullptr;
        const Loader* mDataLoader = nullptr;
        KeyValueStore* mStateStore = nullptr;
        std::unique_ptr<sol::state> mLuaState;
        std::unordered_map<std::string, std::shared_ptr<sol::environment>> mAnimatorEnvs;
        std::unordered_map<std::string, std::shared_ptr<sol::environment>> mEntityEnvs;
        std::unordered_map<std::string, std::unique_ptr<sol::environment>> mWindowEnvs;
        std::unique_ptr<sol::environment> mSceneEnv;
        std::unique_ptr<sol::environment> mGameEnv;
        std::queue<Action> mActionQueue;
        std::vector<DebugDrawCmd> mDebugDrawQueue;
        game::Scene* mScene = nullptr;
        game::Tilemap* mTilemap = nullptr;
        uik::Window* mWindow = nullptr;
        Camera mCamera;

        bool mEditingMode = false;
        bool mPreviewMode = false;
    };

} // namespace
