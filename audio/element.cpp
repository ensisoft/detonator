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

#include "config.h"

#include "base/assert.h"
#include "base/utility.h"
#include "base/math.h"
#include "base/logging.h"
#include "audio/element.h"
#include "audio/elements/delay.h"
#include "audio/elements/effect.h"
#include "audio/elements/file_source.h"
#include "audio/elements/gain.h"
#include "audio/elements/mixer.h"
#include "audio/elements/mixer_source.h"
#include "audio/elements/null.h"
#include "audio/elements/playlist.h"
#include "audio/elements/queue.h"
#include "audio/elements/resampler.h"
#include "audio/elements/sine_source.h"
#include "audio/elements/splitter.h"
#include "audio/elements/stereo_joiner.h"
#include "audio/elements/stereo_maker.h"
#include "audio/elements/stereo_splitter.h"
#include "audio/elements/stream_source.h"
#include "audio/elements/zero_source.h"

namespace audio
{

template<typename T>
const T* GetArg(const std::unordered_map<std::string, ElementArg>& args,
                const std::string& arg_name,
                const std::string& elem)
{
    if (const auto* variant = base::SafeFind(args, arg_name))
    {
        if (const auto* value = std::get_if<T>(variant))
            return value;
        else ERROR("Mismatch in audio element argument type. [elem=%1, arg=%2]", elem, arg_name);
    } else ERROR("Missing audio element argument. [elem=%1, arg=%2]", elem, arg_name);
    return nullptr;
}

template<typename T>
const T* GetOptionalArg(const std::unordered_map<std::string, ElementArg>& args,
                        const std::string& arg_name,
                        const std::string& elem)
{
    if (const auto* variant = base::SafeFind(args, arg_name))
    {
        if (const auto* value = std::get_if<T>(variant))
            return value;
        else WARN("Mismatch in audio element argument type. [elem=%1, arg=%2]", elem, arg_name);
    }
    return nullptr;
}

template<typename Type, typename Arg0> inline
std::unique_ptr<Type> Construct(const std::string& name,
                                   const std::string& id,
                                   const Arg0* arg0)
{
    if (!arg0) return nullptr;
    return std::make_unique<Type>(name, id, *arg0);
}
template<typename Type, typename Arg0, typename Arg1> inline
std::unique_ptr<Type> Construct(const std::string& name,
                                   const std::string& id,
                                   const Arg0* arg0,
                                   const Arg1* arg1)
{
    if (!arg0 || !arg1) return nullptr;
    return std::make_unique<Type>(name, id, *arg0, *arg1);
}
template<typename Type, typename Arg0, typename Arg1, typename Arg2> inline
std::unique_ptr<Type> Construct(const std::string& name,
                                   const std::string& id,
                                   const Arg0* arg0,
                                   const Arg1* arg1,
                                   const Arg2* arg2)
{
    if (!arg0 || !arg1 || !arg2) return nullptr;
    return std::make_unique<Type>(name, id, *arg0, *arg1, *arg2);
}

std::vector<std::string> ListAudioElements()
{
    static std::vector<std::string> list;
    if (list.empty())
    {
        list.push_back("SineSource");
        list.push_back("ZeroSource");
        list.push_back("FileSource");
        list.push_back("Resampler");
        list.push_back("Effect");
        list.push_back("Gain");
        list.push_back("Null");
        list.push_back("StereoSplitter");
        list.push_back("StereoJoiner");
        list.push_back("StereoMaker");
        list.push_back("Splitter");
        list.push_back("Mixer");
        list.push_back("Delay");
        list.push_back("Playlist");
        list.push_back("Queue");
    }
    return list;
}

const ElementDesc* FindElementDesc(const std::string& type)
{
    static std::unordered_map<std::string, ElementDesc> map;
    if (map.empty())
    {
        {
            ElementDesc playlist;
            playlist.input_ports.push_back({"in0"});
            playlist.input_ports.push_back({"in1"});
            playlist.output_ports.push_back({"out"});
            map["Playlist"] = playlist;
        }
        {
            ElementDesc test;
            test.args["frequency"] = 2000u;
            test.args["duration"]  = 0u;
            test.args["format"]    = audio::Format {audio::SampleType::Float32, 44100, 2};
            test.output_ports.push_back({"out"});
            map["SineSource"] = test;
        }
        {
            ElementDesc zero;
            zero.args["format"] = audio::Format{audio::SampleType::Float32, 44100, 2 };
            zero.output_ports.push_back({"out"});
            map["ZeroSource"] = zero;
        }
        {
            ElementDesc file;
            file.args["file"]  = std::string();
            file.args["type"]  = audio::SampleType::Float32;
            file.args["loops"] = 1u;
            file.args["pcm_caching"] = false;
            file.args["file_caching"] = false;
            file.args["io_strategy"] = FileSource::IOStrategy::Default;
            file.output_ports.push_back({"out"});
            map["FileSource"] = file;
        }
        {
            ElementDesc resampler;
            resampler.args["sample_rate"] = 44100u;
            resampler.input_ports.push_back({"in"});
            resampler.output_ports.push_back({"out"});
            map["Resampler"] = resampler;
        }
        {
            ElementDesc gain;
            gain.args["gain"] = 1.0f;
            gain.input_ports.push_back({"in"});
            gain.output_ports.push_back({"out"});
            map["Gain"] = gain;
        }
        {
            ElementDesc effect;
            effect.args["time"] = 0u;
            effect.args["duration"] = 0u;
            effect.args["effect"] = Effect::Kind::FadeIn;
            effect.input_ports.push_back({"in"});
            effect.output_ports.push_back({"out"});
            map["Effect"] = effect;
        }
        {
            ElementDesc null;
            null.input_ports.push_back({"in"});
            map["Null"] =  null;
        }
        {
            ElementDesc splitter;
            splitter.input_ports.push_back({"in"});
            splitter.output_ports.push_back({"left"});
            splitter.output_ports.push_back({"right"});
            map["StereoSplitter"] = splitter;
        }
        {
            ElementDesc joiner;
            joiner.input_ports.push_back({"left"});
            joiner.input_ports.push_back({"right"});
            joiner.output_ports.push_back({"out"});
            map["StereoJoiner"] = joiner;
        }
        {
            ElementDesc maker;
            maker.input_ports.push_back({"in"});
            maker.output_ports.push_back({"out"});
            maker.args["channel"] = StereoMaker::Channel::Both;
            map["StereoMaker"] = maker;
        }
        {
            ElementDesc mixer;
            mixer.args["num_srcs"] = 2u;
            mixer.input_ports.push_back({"in0"});
            mixer.input_ports.push_back({"in1"});
            mixer.output_ports.push_back({"out"});
            map["Mixer"] = mixer;
        }
        {
            ElementDesc delay;
            delay.args["delay"] = 0u;
            delay.input_ports.push_back({"in"});
            delay.output_ports.push_back({"out"});
            map["Delay"] = delay;
        }
        {
            ElementDesc splitter;
            splitter.args["num_outs"] = 2u;
            splitter.input_ports.push_back({"in"});
            splitter.output_ports.push_back({"out0"});
            splitter.output_ports.push_back({"out1"});
            map["Splitter"] = splitter;
        }

        {
            ElementDesc queue;
            queue.input_ports.push_back({"in"});
            queue.output_ports.push_back({"out"});
            map["Queue"] = std::move(queue);
        }
    }
    return base::SafeFind(map, type);
}

std::unique_ptr<Element> CreateElement(const ElementCreateArgs& desc)
{
    const auto& args = desc.args;
    const auto& name = desc.type + "/" + desc.name;
    if (desc.type == "Queue")
        return std::make_unique<Queue>(desc.name, desc.id);
    else if (desc.type == "Playlist")
        return Construct<Playlist>(desc.name, desc.id, &desc.input_ports);
    else if (desc.type == "StereoMaker")
        return Construct<StereoMaker>(desc.name, desc.id,
            GetArg<StereoMaker::Channel>(args, "channel", name));
    else if (desc.type == "StereoJoiner")
        return std::make_unique<StereoJoiner>(desc.name, desc.id);
    else if (desc.type == "StereoSplitter")
        return std::make_unique<StereoSplitter>(desc.name, desc.id);
    else if (desc.type == "Null")
        return std::make_unique<Null>(desc.name, desc.id);
    else if (desc.type == "Mixer")
        return Construct<Mixer>(desc.name, desc.id, &desc.input_ports);
    else if (desc.type == "Splitter")
        return Construct<Splitter>(desc.name, desc.id, &desc.output_ports);
    else if (desc.type == "Delay")
        return Construct<Delay>(desc.name, desc.id,
            GetArg<unsigned>(args, "delay", name));
    else if (desc.type == "Effect")
        return Construct<Effect>(desc.name, desc.id,
            GetArg<unsigned>(args, "time", name),
            GetArg<unsigned>(args, "duration", name),
            GetArg<Effect::Kind>(args, "effect", name));
    else if (desc.type == "Gain")
        return Construct<Gain>(desc.name, desc.id, 
            GetArg<float>(args, "gain", name));
    else if (desc.type == "Resampler")
        return Construct<Resampler>(desc.name, desc.id, 
            GetArg<unsigned>(args, "sample_rate", name));
    else if (desc.type == "FileSource")
    {
        auto ret = Construct<FileSource>(desc.name, desc.id,
            GetArg<std::string>(args, "file", name),
            GetArg<SampleType>(args, "type", name),
            GetArg<unsigned>(args, "loops", name));
        if (const auto* arg = GetOptionalArg<bool>(args, "pcm_caching", name))
            ret->EnablePcmCaching(*arg);
        if (const auto* arg = GetOptionalArg<bool>(args, "file_caching", name))
            ret->EnableFileCaching(*arg);
        if (const auto* arg = GetOptionalArg<FileSource::IOStrategy>(args, "io_strategy", name))
            ret->SetIOStrategy(*arg);
        return ret;
    }
    else if (desc.type == "ZeroSource")
        return Construct<ZeroSource>(desc.name, desc.id,
            GetArg<Format>(args, "format", name));
    else if (desc.type == "SineSource")
        return Construct<SineSource>(desc.name, desc.id,
            GetArg<Format>(args, "format", name),
            GetArg<unsigned>(args, "frequency", name),
            GetArg<unsigned>(args, "duration", name));
    else BUG("Unsupported audio Element construction.");
    return nullptr;
}

void ClearCaches()
{
    FileSource::ClearCache();
}


} // namespace

