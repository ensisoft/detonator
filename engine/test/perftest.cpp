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

// engine subsystem performance tests

#include "config.h"

#include <iostream>
#include <string>
#include <any>

#include "base/cmdline.h"
#include "base/logging.h"
#include "base/test_help.h"
#include "engine/audio.h"
#include "audio/format.h"
#include "audio/loader.h"
#include "audio/graph.h"

namespace {
bool enable_pcm_caching=false;
bool enable_file_caching=false;
} // namespace

void audio_test_decode_file(const std::any& arg)
{
    audio::Loader loader;

    auto laser = std::make_shared<audio::GraphClass>("laser", "21828282");
    audio::ElementCreateArgs elem;
    elem.type = "FileSource";
    elem.name = "file";
    elem.id   = base::RandomString(10);
    elem.args["file"]  = std::any_cast<std::string>(arg);
    elem.args["type"]  = audio::SampleType::Float32;
    elem.args["loops"] = 1u;
    elem.args["cache"] = enable_pcm_caching;
    const auto& e = laser->AddElement(std::move(elem));
    laser->SetGraphOutputElementId(e.id);
    laser->SetGraphOutputElementPort("out");

    std::vector<char> buffer;
    buffer.resize(1024);

    audio::AudioGraph::PrepareParams p;
    p.enable_pcm_caching = true;

    for (unsigned i=0; i<100; ++i)
    {
        audio::AudioGraph graph("graph", audio::Graph("graph", laser));
        graph.Prepare(loader, p);
        std::uint64_t bytes = 0;
        while (graph.HasMore(bytes))
        {
            graph.FillBuffer(&buffer[0], buffer.size());
        }
    }
}

// typical assumed scenario. An audio graph that gets played over
// and over again in rapid succession. For example the sound of a
// player's weapon that gets fired when key is pressed/held pressed.
void audio_test_rapid_fire(const std::any& any)
{
    auto laser = std::make_shared<audio::GraphClass>("laser", "21828282");
    audio::ElementCreateArgs elem;
    elem.type = "FileSource";
    elem.name = "file";
    elem.id   = base::RandomString(10);
    elem.args["file"]  = "assets/sounds/Laser_09.mp3";
    elem.args["type"]  = audio::SampleType::Float32;
    elem.args["loops"] = 1u;
    elem.args["cache"] = enable_pcm_caching;
    const auto& e = laser->AddElement(std::move(elem));
    laser->SetGraphOutputElementId(e.id);
    laser->SetGraphOutputElementPort("out");

    audio::Loader loader;
    audio::Format format;
    format.channel_count = 2;
    format.sample_rate   = 44100;
    format.sample_type   = audio::SampleType::Float32;

    engine::AudioEngine engine("test");
    engine.SetBufferSize(20);
    engine.SetLoader(&loader);
    engine.SetFormat(format);
    engine.Start();

    for (int i=0; i<100; ++i)
    {
        engine.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        engine.PlaySoundEffect(laser, 0u);
    }
}

int test_main(int argc, char* argv[])
{
    struct TestCase {
        const char* name = nullptr;
        void (*test_function)(const std::any&);
        std::any arg;
    } tests[] = {
        {"audio-rapid-fire", audio_test_rapid_fire},
        {"audio-decode-mp3", audio_test_decode_file, std::string("assets/sounds/Laser_09.mp3")},
        {"audio-decode-ogg", audio_test_decode_file, std::string("assets/sounds/Laser_09.ogg")},
        {"audio-decode-wav", audio_test_decode_file, std::string("assets/sounds/Laser_09.wav")}
    };

    base::LockedLogger<base::OStreamLogger> logger((base::OStreamLogger(std::cout)));
    base::SetGlobalLog(&logger);
    DEBUG("Hello!");

    base::CommandLineArgumentStack args(argc-1, (const char**)&argv[1]);
    base::CommandLineOptions opt;
    opt.Add("--debug-log", "Enable debug level log.");
    opt.Add("--loops", "Number of test loop iterations.", 1u);
    opt.Add("--help", "Print this help.");
    opt.Add("--timing", "Perform timing on tests.");
    opt.Add("--pcm-cache", "Enable audio PCM caching.");
    opt.Add("--enable-file-caching", "Enable file caching.");
    for (const auto& test : tests)
        opt.Add(test.name, "Test case");

    std::string cmdline_error;
    if (!opt.Parse(args, &cmdline_error, true))
    {
        std::fprintf(stdout, "Error parsing args. [err='%s']\n", cmdline_error.c_str());
        return 0;
    }
    if (opt.WasGiven("--help"))
    {
        opt.Print(std::cout);
        return 0;
    }
    enable_pcm_caching = opt.WasGiven("--pcm-cache");
    enable_file_caching = opt.WasGiven("--file-cache");

    base::EnableDebugLog(opt.WasGiven("--debug-log"));

    const auto iterations = opt.GetValue<unsigned>("--loops");
    for (const auto& test : tests)
    {
        if (!opt.WasGiven(test.name))
            continue;
        if (opt.WasGiven("--timing"))
        {
            const auto& times = base::TimedTest(iterations, [&test]() {
                test.test_function(test.arg);
            } );
            base::PrintTestTimes(test.name, times);
        }
        else
        {
            test.test_function(test.arg);
        }
    }
    return 0;
}