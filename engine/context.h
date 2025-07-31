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

#include "game/fwd.h"
#include "engine/types.h"
#include "engine/action.h"

// this header provides the interface for the native game logic
// to interact with the game engine.

namespace engine
{
    class ClassLibrary;
    class PhysicsEngine;
    class AudioEngine;

    class RuntimeContext
    {
    public:
        virtual const PhysicsEngine* GetPhysics() const noexcept = 0;
        virtual const ClassLibrary* GetClassLib() const noexcept = 0;
        virtual const AudioEngine* GetAudio() const noexcept = 0;

        virtual game::Scene* GetScene() noexcept = 0;

        virtual bool EditingMode() const noexcept = 0;
        virtual bool PreviewMode() const noexcept = 0;

        // Post a new game event that gets dispatched to the event action
        // handlers in entity and scene scripts.
        virtual void PostEvent(const GameEvent& event) = 0;

        virtual void DebugPrint(const std::string& message) = 0;
        virtual void DebugPrint(std::string&& message) = 0;

        // helpers
        inline auto FindEntityClass(const std::string& name) const
        {
            return GetClassLib()->FindEntityClassByName(name);
        }
    protected:
        virtual ~RuntimeContext() = default;
    private:
    };

    void SetContext(RuntimeContext* context);

    game::Entity* SpawnEntity(const game::EntityArgs& args, const std::string& klass_name, bool link_to_root = true);
    void PostEvent(const GameEvent& event);
    void DebugPrint(const std::string& message);
    void DebugPrint(std::string&& message);

} // namespace