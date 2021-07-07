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

// audio test application

int main(int argc, char* argv[])
{
    std::string path(".");
    unsigned loops = 1;
    bool ogg_files = false;
    bool pcm_8bit_files = false;
    bool pcm_16bit_files = false;
    bool pcm_24bit_files = false;
    bool sine = false;
    for (int i=1; i<argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ogg"))
            ogg_files = true;
        else if (!std::strcmp(argv[i], "--8bit"))
            pcm_8bit_files = true;
        else if (!std::strcmp(argv[i], "--16bit"))
            pcm_16bit_files = true;
        else if (!std::strcmp(argv[i], "--24bit"))
            pcm_24bit_files = true;
        else if (!std::strcmp(argv[i], "--sine"))
            sine = true;
        else if (!std::strcmp(argv[i], "--path"))
            path = argv[++i];
        else if (!std::strcmp(argv[i], "--loops"))
            loops = std::stoi(argv[++i]);
        
    }



    if (!(ogg_files || pcm_8bit_files || pcm_16bit_files || pcm_24bit_files || sine))
    {
        std::printf("You haven't actually opted to play anything.\n"
            "You have the following options:\n"
            "\t--ogg\t\tTest Ogg Vorbis encoded files.\n"
            "\t--8bit\t\tTest 8bit PCM encoded files.\n"
            "\t--16bit\t\tTest 16bit PCM encoded files.\n"
            "\t--24bit\t\tTest 24bit PCM encoded files.\n"
            "\t--sine\t\tTest procedural audio (sine wave).\n"
            "\t--graph1\t\tTest audio graph 1.\n");
        std::printf("Have a good day.\n");
        return 0;
    }

    base::LockedLogger<base::OStreamLogger> logger((base::OStreamLogger(std::cout)));
    base::SetGlobalLog(&logger);
    base::EnableDebugLog(true);
    
    audio::Player player(audio::Device::Create("audio_test"));
    if (sine)
    {
        auto sample = std::make_unique<audio::SineGenerator>(500);
        const auto id = player.Play(std::move(sample), false /*looping*/);
        DEBUG("New sine wave stream = %1", id);        
        INFO("Playing procedural sine audio for 10 seconds.");
        std::this_thread::sleep_for(std::chrono::seconds(10));
        player.Cancel(id);
    }


    std::vector<std::string> test_files;
    if (ogg_files)
    {
        // https://github.com/UniversityRadioYork/ury-playd/issues/111
        test_files.push_back("/OGG/testshort.ogg");
        test_files.push_back("/OGG/a2002011001-e02-128k.ogg");
        test_files.push_back("/OGG/a2002011001-e02-32k.ogg");
        test_files.push_back("/OGG/a2002011001-e02-64k.ogg");
        test_files.push_back("/OGG/a2002011001-e02-96k.ogg");
    }
    if (pcm_8bit_files)
    {
        test_files.push_back("/WAV/PCM 8 bit/pcm mono 8 bit 11025Hz.wav");
        test_files.push_back("/WAV/PCM 8 bit/pcm mono 8 bit 16kHz.wav");
        test_files.push_back("/WAV/PCM 8 bit/pcm mono 8 bit 22050Hz.wav");
        test_files.push_back("/WAV/PCM 8 bit/pcm mono 8 bit 32kHz.wav");
        test_files.push_back("/WAV/PCM 8 bit/pcm mono 8 bit 44.1kHz.wav");
        test_files.push_back("/WAV/PCM 8 bit/pcm mono 8 bit 48kHz.wav");
        test_files.push_back("/WAV/PCM 8 bit/pcm mono 8 bit 8kHz.wav");
        test_files.push_back("/WAV/PCM 8 bit/pcm stereo 8 bit 11025Hz.wav");
        test_files.push_back("/WAV/PCM 8 bit/pcm stereo 8 bit 16kHz.wav");
        test_files.push_back("/WAV/PCM 8 bit/pcm stereo 8 bit 22050Hz.wav");
        test_files.push_back("/WAV/PCM 8 bit/pcm stereo 8 bit 32kHz.wav");
        test_files.push_back("/WAV/PCM 8 bit/pcm stereo 8 bit 44.1kHz.wav");
        test_files.push_back("/WAV/PCM 8 bit/pcm stereo 8 bit 48kHz.wav");
        test_files.push_back("/WAV/PCM 8 bit/pcm stereo 8 bit 8kHz.wav");
    }

    if (pcm_16bit_files)
    {
        test_files.push_back("/WAV/PCM 16 bit/pcm mono 16 bit 11025Hz.wav");
        test_files.push_back("/WAV/PCM 16 bit/pcm mono 16 bit 16kHz.wav");
        test_files.push_back("/WAV/PCM 16 bit/pcm mono 16 bit 22050Hz.wav");
        test_files.push_back("/WAV/PCM 16 bit/pcm mono 16 bit 32kHz.wav");
        test_files.push_back("/WAV/PCM 16 bit/pcm mono 16 bit 44.1kHz.wav");
        test_files.push_back("/WAV/PCM 16 bit/pcm mono 16 bit 48kHz.wav");
        test_files.push_back("/WAV/PCM 16 bit/pcm mono 16 bit 8kHz.wav");
        test_files.push_back("/WAV/PCM 16 bit/pcm stereo 16 bit 11025Hz.wav");
        test_files.push_back("/WAV/PCM 16 bit/pcm stereo 16 bit 16kHz.wav");
        test_files.push_back("/WAV/PCM 16 bit/pcm stereo 16 bit 22050Hz.wav");
        test_files.push_back("/WAV/PCM 16 bit/pcm stereo 16 bit 32kHz.wav");
        test_files.push_back("/WAV/PCM 16 bit/pcm stereo 16 bit 44.1kHz.wav");
        test_files.push_back("/WAV/PCM 16 bit/pcm stereo 16 bit 48kHz.wav");
        test_files.push_back("/WAV/PCM 16 bit/pcm stereo 16 bit 8kHz.wav");
    }

    if (pcm_24bit_files)
    {
        test_files.push_back("/WAV/PCM 24 bit/pcm mono 24 bit 11025Hz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm mono 24 bit 16kHz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm mono 24 bit 22050Hz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm mono 24 bit 32kHz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm mono 24 bit 44.1kHz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm mono 24 bit 48kHz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm mono 24 bit 88.2kHz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm mono 24 bit 8kHz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm mono 24 bit 96kHz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm stereo 24 bit 11025Hz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm stereo 24 bit 16kHz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm stereo 24 bit 22050Hz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm stereo 24 bit 32kHz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm stereo 24 bit 44.1kHz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm stereo 24 bit 48kHz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm stereo 24 bit 88.2kHz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm stereo 24 bit 8kHz.wav");
        test_files.push_back("/WAV/PCM 24 bit/pcm stereo 24 bit 96kHz.wav");
    }
    

    for (const auto& file : test_files)
    {
        const auto& filename = path + file;
        INFO("Testing: '%1'", filename);
        base::FlushGlobalLog();

        auto sample = std::make_unique<audio::AudioFile>(filename, "test");
        const auto looping = loops > 1;
        const auto id = player.Play(std::move(sample), looping);
        DEBUG("New track. id = %1", id);

        unsigned completed_loop_count = 0;
        while (completed_loop_count != loops)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            audio::Player::TrackEvent e;
            if (player.GetEvent(&e))
            {
                DEBUG("Track id = %1 event", e.id);
                INFO("Track status %1", e.status == audio::Player::TrackStatus::Failure
                    ? "Failed" : "Success");
                completed_loop_count++;
            }
        }
        if (looping)
            player.Cancel(id);
    }
    return 0;
}   
