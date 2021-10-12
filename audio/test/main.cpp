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

#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <cstdio>
#include <cstdlib>

#include "base/logging.h"
#include "audio/device.h"
#include "audio/player.h"
#include "audio/source.h"
#include "audio/element.h"
#include "audio/loader.h"
#include "audio/graph.h"

// audio test application

int main(int argc, char* argv[])
{
    audio::Source::Format format = audio::Source::Format::Float32;

    std::string file;
    unsigned loops = 1;
    bool mp3_files = false;
    bool ogg_files = false;
    bool pcm_8bit_files = false;
    bool pcm_16bit_files = false;
    bool pcm_24bit_files = false;
    bool sine = false;
    bool graph = false;
    for (int i=1; i<argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ogg"))
            ogg_files = true;
        else if (!std::strcmp(argv[i], "--mp3"))
            mp3_files = true;
        else if (!std::strcmp(argv[i], "--8bit"))
            pcm_8bit_files = true;
        else if (!std::strcmp(argv[i], "--16bit"))
            pcm_16bit_files = true;
        else if (!std::strcmp(argv[i], "--24bit"))
            pcm_24bit_files = true;
        else if (!std::strcmp(argv[i], "--sine"))
            sine = true;
        else if (!std::strcmp(argv[i], "--graph"))
            graph = true;
        else if (!std::strcmp(argv[i], "--loops"))
            loops = std::stoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--int16"))
            format = audio::Source::Format::Int16;
        else if (!std::strcmp(argv[i], "--int32"))
            format = audio::Source::Format::Int32;
        else if (!std::strcmp(argv[i], "--file"))
            file = argv[++i];
    }

    if (!(ogg_files || mp3_files || pcm_8bit_files || pcm_16bit_files || pcm_24bit_files || sine || graph) && file.empty())
    {
        std::printf("You haven't actually opted to play anything.\n"
            "You have the following options:\n"
            "\t--ogg\t\tTest Ogg Vorbis encoded files.\n"
            "\t--mp3\t\tTest MP3 encoded files.\n"
            "\t--8bit\t\tTest 8bit PCM encoded files.\n"
            "\t--16bit\t\tTest 16bit PCM encoded files.\n"
            "\t--24bit\t\tTest 24bit PCM encoded files.\n"
            "\t--sine\t\tTest procedural audio (sine wave).\n"
            "\t--graph\t\tTest audio graph.\n"
            "\t--loops\t\tNumber of loops to use to play each file.\n"
            "\t--file\t\tA specific test file to add\n");
        std::printf("Have a good day.\n");
        return 0;
    }

    base::LockedLogger<base::OStreamLogger> logger((base::OStreamLogger(std::cout)));
    base::SetGlobalLog(&logger);
    base::EnableDebugLog(true);

    audio::Player player(audio::Device::Create("audio_test"));
    if (sine)
    {
        auto source = std::make_unique<audio::SineGenerator>(500, format);
        const auto id = player.Play(std::move(source));
        DEBUG("New sine wave stream = %1", id);        
        INFO("Playing procedural sine audio for 10 seconds.");
        std::this_thread::sleep_for(std::chrono::seconds(10));
        player.Cancel(id);
    }

    if (graph)
    {
        class Buffer : public audio::SourceBuffer {
        public:
            Buffer(const std::string& file)
              : mBuffer(base::LoadBinaryFile(file))
            {}
            // Get the read pointer for the contents of the buffer.
            virtual const void* GetData() const override
            { return (const void*)mBuffer.data(); }
            // Get the size of the buffer's contents in bytes.
            virtual size_t GetSize() const override
            { return mBuffer.size(); }
        private:
            std::vector<char> mBuffer;
        };

        class Loader : public audio::Loader {
        public:
            virtual std::ifstream OpenAudioStream(const std::string& file) const override
            { return base::OpenBinaryInputStream(file); }
            virtual std::shared_ptr<const audio::SourceBuffer> LoadAudioBuffer(const std::string& file) const override
            { return std::make_shared<Buffer>(file); }
        private:
        };
        Loader loader;

        auto graph = std::make_unique<audio::AudioGraph>("graph");
        audio::Format sine_format;
        sine_format.sample_type = audio::SampleType::Float32;
        sine_format.channel_count = 1;
        sine_format.sample_rate   = 44100;
        auto* sine     = (*graph)->AddElement(audio::SineSource("sine", sine_format, 500, 5000));
        auto* file     = (*graph)->AddElement(audio::FileSource("file", "OGG/testshort.ogg",audio::SampleType::Float32));
        auto* gain     = (*graph)->AddElement(audio::Gain("gain", 1.0f));
        auto* mixer    = (*graph)->AddElement(audio::Mixer("mixer", 2));
        auto* splitter = (*graph)->AddElement(audio::StereoSplitter("split"));
        auto* null     = (*graph)->AddElement(audio::Null("null"));
        auto* resamp   = (*graph)->AddElement(audio::Resampler("resamp", 16000));

        ASSERT((*graph)->LinkElements("file", "out", "split", "in"));
        ASSERT((*graph)->LinkElements("split", "left", "mixer", "in0"));
        ASSERT((*graph)->LinkElements("split", "right", "null", "in"));
        ASSERT((*graph)->LinkElements("sine", "out", "mixer", "in1"));
        ASSERT((*graph)->LinkElements("mixer", "out", "gain", "in"));
        ASSERT((*graph)->LinkElements("gain", "out", "resamp", "in"));
        ASSERT((*graph)->LinkGraph("resamp", "out"));
        ASSERT(graph->Prepare(loader));

        const auto& desc = (*graph)->Describe();
        for (const auto& str : desc)
            DEBUG(str);

        const auto id = player.Play(std::move(graph));
        std::this_thread::sleep_for(std::chrono::seconds(10));
        player.Cancel(id);
    }

    std::vector<std::string> test_files;
    if (!file.empty())
        test_files.push_back(file);

    if (ogg_files)
    {
        // https://github.com/UniversityRadioYork/ury-playd/issues/111
        test_files.push_back("OGG/testshort.ogg");
        test_files.push_back("OGG/a2002011001-e02-128k.ogg");
        test_files.push_back("OGG/a2002011001-e02-32k.ogg");
        test_files.push_back("OGG/a2002011001-e02-64k.ogg");
        test_files.push_back("OGG/a2002011001-e02-96k.ogg");
    }
    if (mp3_files)
    {
        test_files.push_back("MP3/Kalimba_short.mp3");
    }
    if (pcm_8bit_files)
    {
        test_files.push_back("WAV/PCM 8 bit/pcm mono 8 bit 11025Hz.wav");
        test_files.push_back("WAV/PCM 8 bit/pcm mono 8 bit 16kHz.wav");
        test_files.push_back("WAV/PCM 8 bit/pcm mono 8 bit 22050Hz.wav");
        test_files.push_back("WAV/PCM 8 bit/pcm mono 8 bit 32kHz.wav");
        test_files.push_back("WAV/PCM 8 bit/pcm mono 8 bit 44.1kHz.wav");
        test_files.push_back("WAV/PCM 8 bit/pcm mono 8 bit 48kHz.wav");
        test_files.push_back("WAV/PCM 8 bit/pcm mono 8 bit 8kHz.wav");
        test_files.push_back("WAV/PCM 8 bit/pcm stereo 8 bit 11025Hz.wav");
        test_files.push_back("WAV/PCM 8 bit/pcm stereo 8 bit 16kHz.wav");
        test_files.push_back("WAV/PCM 8 bit/pcm stereo 8 bit 22050Hz.wav");
        test_files.push_back("WAV/PCM 8 bit/pcm stereo 8 bit 32kHz.wav");
        test_files.push_back("WAV/PCM 8 bit/pcm stereo 8 bit 44.1kHz.wav");
        test_files.push_back("WAV/PCM 8 bit/pcm stereo 8 bit 48kHz.wav");
        test_files.push_back("WAV/PCM 8 bit/pcm stereo 8 bit 8kHz.wav");
    }

    if (pcm_16bit_files)
    {
        test_files.push_back("WAV/PCM 16 bit/pcm mono 16 bit 11025Hz.wav");
        test_files.push_back("WAV/PCM 16 bit/pcm mono 16 bit 16kHz.wav");
        test_files.push_back("WAV/PCM 16 bit/pcm mono 16 bit 22050Hz.wav");
        test_files.push_back("WAV/PCM 16 bit/pcm mono 16 bit 32kHz.wav");
        test_files.push_back("WAV/PCM 16 bit/pcm mono 16 bit 44.1kHz.wav");
        test_files.push_back("WAV/PCM 16 bit/pcm mono 16 bit 48kHz.wav");
        test_files.push_back("WAV/PCM 16 bit/pcm mono 16 bit 8kHz.wav");
        test_files.push_back("WAV/PCM 16 bit/pcm stereo 16 bit 11025Hz.wav");
        test_files.push_back("WAV/PCM 16 bit/pcm stereo 16 bit 16kHz.wav");
        test_files.push_back("WAV/PCM 16 bit/pcm stereo 16 bit 22050Hz.wav");
        test_files.push_back("WAV/PCM 16 bit/pcm stereo 16 bit 32kHz.wav");
        test_files.push_back("WAV/PCM 16 bit/pcm stereo 16 bit 44.1kHz.wav");
        test_files.push_back("WAV/PCM 16 bit/pcm stereo 16 bit 48kHz.wav");
        test_files.push_back("WAV/PCM 16 bit/pcm stereo 16 bit 8kHz.wav");
    }

    if (pcm_24bit_files)
    {
        test_files.push_back("WAV/PCM 24 bit/pcm mono 24 bit 11025Hz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm mono 24 bit 16kHz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm mono 24 bit 22050Hz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm mono 24 bit 32kHz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm mono 24 bit 44.1kHz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm mono 24 bit 48kHz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm mono 24 bit 88.2kHz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm mono 24 bit 8kHz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm mono 24 bit 96kHz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm stereo 24 bit 11025Hz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm stereo 24 bit 16kHz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm stereo 24 bit 22050Hz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm stereo 24 bit 32kHz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm stereo 24 bit 44.1kHz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm stereo 24 bit 48kHz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm stereo 24 bit 88.2kHz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm stereo 24 bit 8kHz.wav");
        test_files.push_back("WAV/PCM 24 bit/pcm stereo 24 bit 96kHz.wav");
    }

    for (const auto& file : test_files)
    {
        INFO("Testing: '%1'", file);
        base::FlushGlobalLog();

        auto source = std::make_unique<audio::AudioFile>(file, "test", format);
        if (!source->Open())
        {
            ERROR("Failed to open '%1'.", file);
            continue;
        }
        source->SetLoopCount(loops);
        const auto id = player.Play(std::move(source));
        DEBUG("New audio track (ID=%1).", id);

        bool track_done = false;
        while (!track_done)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            audio::Player::Event e;
            if (!player.GetEvent(&e))
                continue;
            else if (!std::holds_alternative<audio::Player::SourceCompleteEvent>(e))
                continue;

            const auto& track_event = std::get<audio::Player::SourceCompleteEvent>(e);
            INFO("Audio track (ID=%1) status event %2.", track_event.id, track_event.status);
            track_done = true;
        }
    }
    return 0;
}   
