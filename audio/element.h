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

#include <string>
#include <memory>
#include <vector>
#include <variant>
#include <unordered_map>

#include "audio/format.h"
#include "audio/elements/element.h"
#include "audio/elements/effect.h"
#include "audio/elements/file_source.h"
#include "audio/elements/stereo_maker.h"

namespace audio
{
    using ElementArg = std::variant<std::string,
            float, unsigned, bool,
            SampleType,
            Format,
            FileSource::IOStrategy,
            StereoMaker::Channel,
            Effect::Kind>;

    struct ElementDesc {
        std::vector<PortDesc> input_ports;
        std::vector<PortDesc> output_ports;
        std::unordered_map<std::string, ElementArg> args;
    };

    struct ElementCreateArgs {
        std::string id;
        std::string name;
        std::string type;
        std::unordered_map<std::string, ElementArg> args;
        std::vector<PortDesc> input_ports;
        std::vector<PortDesc> output_ports;
    };

    template<typename T> inline
    const T* FindElementArg(const std::unordered_map<std::string, ElementArg>& args,
                            const std::string& arg_name)
    {
        if (const auto* variant = base::SafeFind(args, arg_name))
        {
            if (const auto* value = std::get_if<T>(variant))
                return value;
        }
        return nullptr;
    }

    const ElementDesc* FindElementDesc(const std::string& type);

    std::vector<std::string> ListAudioElements();

    std::unique_ptr<Element> CreateElement(const ElementCreateArgs& desc);

    void ClearCaches();

} // namespace
