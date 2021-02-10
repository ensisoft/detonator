// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include "warnpush.h"
#  define SOL_ALL_SAFETIES_ON 1
#  include <sol/sol.hpp>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <memory>
#include <queue>
#include <unordered_map>

#include "engine/game.h"
#include "engine/types.h"

namespace game
{
    // Implementation for the main game interface that
    // simply delegates the calls to a Lua script.
    class LuaGame : public Game
    {
    public:
        LuaGame(std::shared_ptr<sol::state> state);
        LuaGame(const std::string& lua_path);
        virtual void SetPhysicsEngine(const PhysicsEngine* engine) override;
        virtual void LoadGame(const ClassLibrary* loader) override;
        virtual void LoadBackgroundDone(Scene* background) override;
        virtual void Tick(double wall_time, double tick_time, double dt) override;
        virtual void Update(double wall_time, double game_time,  double dt) override;
        virtual void BeginPlay(Scene* scene) override;
        virtual void EndPlay() override;
        virtual void SaveGame() override;
        virtual bool GetNextAction(Action* out) override;
        virtual FRect GetViewport() const override;
        virtual void OnContactEvent(const ContactEvent& contact) override;
        virtual void OnKeyDown(const wdk::WindowEventKeydown& key) override;
        virtual void OnKeyUp(const wdk::WindowEventKeyup& key) override;
        virtual void OnChar(const wdk::WindowEventChar& text) override;
        virtual void OnMouseMove(const wdk::WindowEventMouseMove& mouse) override;
        virtual void OnMousePress(const wdk::WindowEventMousePress& mouse) override;
        virtual void OnMouseRelease(const wdk::WindowEventMouseRelease& mouse) override;
    private:
        const ClassLibrary* mClasslib = nullptr;
        const PhysicsEngine* mPhysicsEngine = nullptr;
        std::shared_ptr<sol::state> mLuaState;
        std::queue<Action> mActionQueue;
        FRect mView;
        Scene* mScene = nullptr;
    };


    class ScriptEngine
    {
    public:
        struct PrintDebugStrAction {
            std::string message;
        };
        using Action = std::variant<PrintDebugStrAction>;

        ScriptEngine(const std::string& lua_path);
        void SetLoader(const ClassLibrary* loader)
        { mClassLib = loader; }
        void SetPhysicsEngine(const PhysicsEngine* engine)
        { mPhysicsEngine = engine; }
        void BeginPlay(Scene* scene);
        void EndPlay();
        void Tick(double wall_time, double tick_time, double dt);
        void Update(double wall_time, double game_time, double dt);
        bool GetNextAction(Action* out);

        void OnContactEvent(const ContactEvent& contact);
        void OnKeyDown(const wdk::WindowEventKeydown& key);
        void OnKeyUp(const wdk::WindowEventKeyup& key);
        void OnChar(const wdk::WindowEventChar& text);
        void OnMouseMove(const wdk::WindowEventMouseMove& mouse);
        void OnMousePress(const wdk::WindowEventMousePress& mouse);
        void OnMouseRelease(const wdk::WindowEventMouseRelease& mouse);
    private:
        sol::environment* GetTypeEnv(const EntityClass& klass);
    private:
        const std::string mLuaPath;
        const ClassLibrary* mClassLib = nullptr;
        const PhysicsEngine* mPhysicsEngine = nullptr;
        std::unique_ptr<sol::state> mLuaState;
        std::unordered_map<std::string, std::unique_ptr<sol::environment>> mTypeEnvs;
        std::queue<Action> mActionQueue;
        Scene* mScene = nullptr;
    };


    void BindBase(sol::state& L);
    void BindGLM(sol::state& L);
    void BindGFX(sol::state& L);
    void BindWDK(sol::state& L);
    void BindGameLib(sol::state& L);

} // namespace