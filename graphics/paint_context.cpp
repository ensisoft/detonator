// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#include <stack>

#include "base/utility.h"
#include "graphics/paint_context.h"

namespace {
std::stack<gfx::PaintContext*> context_stack;
} // namespace

namespace gfx {
PaintContext::PaintContext()
{
    context_stack.push(this);
}
PaintContext::~PaintContext()
{
    EndScope();
}

void PaintContext::EndScope() noexcept
{
    if (mActive)
    {
        ASSERT(context_stack.top() == this);
        context_stack.pop();
        mActive = false;
        if (mPropagateUp && !context_stack.empty())
        {
            auto* up = context_stack.top();
            base::AppendVector(up->mMessages, mMessages);
            up->mErrorCount += mErrorCount;
            up->mWarningCount += mWarningCount;
        }
    }
}

void PaintContext::ClearMessages() noexcept
{
    mMessages.clear();
    mErrorCount   = 0;
    mWarningCount = 0;
}

// static
PaintContext* PaintContext::GetContext()
{
    if (context_stack.empty())
        return nullptr;
    return context_stack.top();
}

void WritePaintContextLogMessage(PaintContext::LogEvent type, const char* file, int line, std::string message)
{
    auto* context = PaintContext::GetContext();
    if (context == nullptr)
    {
#if defined(BASE_LOGGING_ENABLE_LOG)
        if (base::IsLogEventEnabled(type))
        {
            base::WriteLogMessage(type, file, line, std::move(message));
        }
#endif
        return;
    }
    context->WriteLogMessage(type, file, line, std::move(message));
}

} // namespace