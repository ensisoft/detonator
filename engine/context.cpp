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

#include "base/logging.h"
#include "game/scene.h"
#include "game/entity.h"
#include "engine/context.h"

namespace {
    engine::RuntimeContext* context;
} // namespace

namespace engine
{

void SetContext(RuntimeContext* ctx)
{
    context = ctx;
}

game::Entity* SpawnEntity(const game::EntityArgs& args,
                          const std::string& klass_name, bool link_to_root)
{
    auto klass = context->FindEntityClass(klass_name);
    if (!klass)
    {
        ERROR("Failed to spawn entity. No such entity class. [klass='%1']", klass_name);
        return nullptr;
    }
    auto a = args;
    a.klass = klass;
    return context->GetScene()->SpawnEntity(a, link_to_root);

}
void PostEvent(const GameEvent& event)
{
    context->PostEvent(event);
}
void DebugPrint(const std::string& message)
{
    context->DebugPrint(message);
}
void DebugPrint(std::string&& message)
{
    context->DebugPrint(std::move(message));
}

} // namespace

