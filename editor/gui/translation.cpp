// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include "base/assert.h"
#include "editor/gui/translation.h"

namespace app
{
std::string TranslateEnum(Workspace::ProjectSettings::CanvasPresentationMode mode)
{
    using M = Workspace::ProjectSettings::CanvasPresentationMode;
    if (mode == M::Normal)
        return "Present as HTML element";
    else if (mode == M::FullScreen)
        return "Present in Fullscreen";
    else BUG("Missing translation");
    return "???";
}
std::string TranslateEnum(Workspace::ProjectSettings::CanvasFullScreenStrategy strategy)
{
    using S = Workspace::ProjectSettings::CanvasFullScreenStrategy;
    if (strategy == S::SoftFullScreen)
        return "Maximize Element on Page";
    else if (strategy == S::RealFullScreen)
        return "Use The Entire Screen";
    else BUG("Missing translation");
    return "???";
}

std::string TranslateEnum(Workspace::ProjectSettings::PowerPreference power)
{
    using P = Workspace::ProjectSettings::PowerPreference;
    if (power == P::Default)
        return "Browser Default";
    else if (power == P::HighPerf)
        return "High Performance";
    else if (power == P::LowPower)
        return "Battery Saving";
    else BUG("Missing translation");
    return "???";
}

std::string TranslateEnum(Workspace::ProjectSettings::WindowMode wm)
{
    using M = Workspace::ProjectSettings::WindowMode;
    if (wm == M::Windowed)
        return "Present in Window";
    else if (wm == M::Fullscreen)
        return "Present in Fullscreen";
    else BUG("Missing translation");
    return "???";
}

} // namespace

namespace engine
{

std::string TranslateEnum(FileResourceLoader::DefaultAudioIOStrategy strategy)
{
    using S = FileResourceLoader::DefaultAudioIOStrategy;
    if (strategy == S::Automatic)
        return "Automatic";
    else if (strategy == S::Memmap)
        return "Memory Mapping";
    else if (strategy == S::Stream)
        return "File Streaming";
    else if (strategy == S::Buffer)
        return "Memory Buffering";
    else BUG("Missing translation");
    return "???";
}

} // namespace