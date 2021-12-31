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
#include <unordered_map>
#include <stack>

#include "game/types.h"
#include "engine/game.h"
#include "engine/action.h"

namespace engine
{
    // Implementation for the main game interface that
    // simply delegates the calls to a Lua script.
    class LuaGame : public Game
    {
    public:
        LuaGame(const std::string& lua_path,
                const std::string& game_script,
                const std::string& game_home,
                const std::string& game_name);
       ~LuaGame();
        virtual void SetStateStore(KeyValueStore* store) override;
        virtual void SetPhysicsEngine(const PhysicsEngine* engine) override;
        virtual void SetAudioEngine(const AudioEngine* engine) override;
        virtual void SetDataLoader(const Loader* loader) override;
        virtual void SetClassLibrary(const ClassLibrary* classlib) override;
        virtual bool LoadGame() override;
        virtual void StartGame() override;
        virtual void Tick(double game_time, double dt) override;
        virtual void Update(double game_time,  double dt) override;
        virtual void BeginPlay(game::Scene* scene) override;
        virtual void EndPlay(game::Scene* scene) override;
        virtual void SaveGame() override;
        virtual void StopGame() override;
        virtual bool GetNextAction(Action* out) override;
        virtual FRect GetViewport() const override;
        virtual void OnUIOpen(uik::Window* ui) override;
        virtual void OnUIClose(uik::Window* ui, int result) override;
        virtual void OnUIAction(uik::Window* ui, const uik::Window::WidgetAction& action) override;
        virtual void OnContactEvent(const ContactEvent& contact) override;
        virtual void OnAudioEvent(const AudioEvent& event) override;
        virtual void OnGameEvent(const GameEvent& event) override;
        virtual void OnKeyDown(const wdk::WindowEventKeyDown& key) override;
        virtual void OnKeyUp(const wdk::WindowEventKeyUp& key) override;
        virtual void OnChar(const wdk::WindowEventChar& text) override;
        virtual void OnMouseMove(const MouseEvent& mouse) override;
        virtual void OnMousePress(const MouseEvent& mouse) override;
        virtual void OnMouseRelease(const MouseEvent& mouse) override;
        void PushAction(Action action)
        { mActionQueue.push(std::move(action)); }
        const ClassLibrary* GetClassLib() const
        { return mClasslib; }
    private:
        const std::string mLuaPath;
        const std::string mGameScript;
        const ClassLibrary* mClasslib = nullptr;
        const PhysicsEngine* mPhysicsEngine = nullptr;
        const AudioEngine* mAudioEngine = nullptr;
        const Loader* mLoader = nullptr;
        KeyValueStore* mStateStore = nullptr;
        std::unique_ptr<sol::state> mLuaState;
        std::queue<Action> mActionQueue;
        FRect mView;
        game::Scene* mScene = nullptr;
        std::stack<uik::Window*> mWindowStack;
    };


    class ScriptEngine
    {
    public:
        using Action = engine::Action;
        ScriptEngine(const std::string& lua_path);
       ~ScriptEngine();
        void SetClassLibrary(const ClassLibrary* classlib)
        { mClassLib = classlib; }
        void SetPhysicsEngine(const PhysicsEngine* engine)
        { mPhysicsEngine = engine; }
        void SetAudioEngine(const AudioEngine* engine)
        { mAudioEngine = engine; }
        void SetDataLoader(const Loader* loader)
        { mDataLoader = loader; }
        void SetStateStore(KeyValueStore* store)
        { mStateStore = store; }
        void BeginPlay(game::Scene* scene);
        void EndPlay(game::Scene* scene);
        void Tick(double game_time, double dt);
        void Update(double game_time, double dt);
        void PostUpdate(double game_time);
        void BeginLoop();
        void EndLoop();
        bool GetNextAction(Action* out);
        void OnContactEvent(const ContactEvent& contact);
        void OnGameEvent(const GameEvent& event);
        void OnKeyDown(const wdk::WindowEventKeyDown& key);
        void OnKeyUp(const wdk::WindowEventKeyUp& key);
        void OnChar(const wdk::WindowEventChar& text);
        void OnMouseMove(const MouseEvent& mouse);
        void OnMousePress(const MouseEvent& mouse);
        void OnMouseRelease(const MouseEvent& mouse);
        void PushAction(Action action)
        { mActionQueue.push(std::move(action)); }
        const ClassLibrary* GetClassLib() const
        { return mClassLib; }
        bool HasAction() const
        { return !mActionQueue.empty(); }
        size_t GetNumActions() const
        { return mActionQueue.size(); }
    private:
        sol::environment* GetTypeEnv(const game::EntityClass& klass);
        template<typename KeyEvent>
        void DispatchKeyboardEvent(const std::string& method, const KeyEvent& key);
        void DispatchMouseEvent(const std::string& method, const MouseEvent& mouse);
    private:
        const std::string mLuaPath;
        const ClassLibrary* mClassLib = nullptr;
        const PhysicsEngine* mPhysicsEngine = nullptr;
        const AudioEngine* mAudioEngine = nullptr;
        const Loader* mDataLoader = nullptr;
        KeyValueStore* mStateStore = nullptr;
        std::unique_ptr<sol::state> mLuaState;
        std::unordered_map<std::string, std::shared_ptr<sol::environment>> mTypeEnvs;
        std::queue<Action> mActionQueue;
        game::Scene* mScene = nullptr;
        std::unique_ptr<sol::environment> mSceneEnv;
    };


    void BindUtil(sol::state& L);
    void BindBase(sol::state& L);
    void BindData(sol::state& L);
    void BindGLM(sol::state& L);
    void BindGFX(sol::state& L);
    void BindWDK(sol::state& L);
    void BindUIK(sol::state& L);
    void BindGameLib(sol::state& L);

} // namespace
