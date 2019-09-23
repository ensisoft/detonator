// Copyright (c) 2014-2019 Sami Väisänen, Ensisoft
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

#include "config.h"

#include <memory>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

#include "audio/device.h"
#include "audio/player.h"
#include "audio/sample.h"
#include "base/logging.h"

// audio test application

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::printf("You need to give the path the test WAV folder as a parameter.\n");
        return 1;
    }
    const std::string p(argv[1]);

    bool ogg_files = false;
    bool pcm_8bit_files = false;
    bool pcm_16bit_files = false;
    bool pcm_24bit_files = false;
    for (int i=2; i<argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ogg"))
            ogg_files = true;
        else if (!std::strcmp(argv[i], "--8bit"))
            pcm_8bit_files = true;
        else if (!std::strcmp(argv[i], "--16bit"))
            pcm_16bit_files = true;
        else if (!std::strcmp(argv[i], "--24bit"))
            pcm_24bit_files = true;
    }

    base::CursesLogger logger;
    base::SetGlobalLog(&logger);
    base::EnableDebugLog(true);
    
    audio::AudioPlayer player(audio::AudioDevice::create("audio_test"));

    std::vector<std::string> test_files;
    if (ogg_files)
    {
        test_files.push_back("OGG/a2002011001-e02-128k.ogg");
        test_files.push_back("OGG/a2002011001-e02-32k.ogg");
        test_files.push_back("OGG/a2002011001-e02-64k.ogg");
        test_files.push_back("OGG/a2002011001-e02-96k.ogg");
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
    

    for (const auto& t : test_files)
    {
        const auto& file = p + t;
        INFO("Testing: '%1'", file);
        base::FlushGlobalLog();

        auto sample = std::make_shared<audio::AudioSample>(file, "test");
        const auto looping = false;
        player.play(sample, looping);

        for (;;) 
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            audio::AudioPlayer::TrackEvent e;
            if (player.get_event(&e))
                break;
        }
    }
    return 0;
}