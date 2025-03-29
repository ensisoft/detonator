// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include <memory>

#include "base/threadpool.h"
#include "audio/elements/element.h"

namespace audio
{
    class Decoder;

    class FileSource : public Element
    {
    public:
        using IOStrategy = audio::IOStrategy;

        FileSource(std::string name,
                   std::string file,
                   SampleType type = SampleType::Int16,
                   unsigned loops = 1);
        FileSource(std::string name,
                   std::string id,
                   std::string file,
                   SampleType type = SampleType::Int16,
                   unsigned loops = 1);
        FileSource(FileSource&& other);
       ~FileSource();
        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        std::string GetType() const override
        { return "FileSource"; }
        bool Prepare(const Loader& loader, const PrepareParams& params) override;
        void Process(Allocator& allocator, EventQueue& events, unsigned milliseconds) override;
        void Shutdown() override;
        bool IsSourceDone() const override;
        bool IsSource() const override
        { return true; }
        unsigned GetNumOutputPorts() const override
        { return 1; }
        Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mPort;
            BUG("No such output port.");
        }
        std::string GetFileName() const
        { return mFile; }

        void SetFileName(const std::string& file)
        { mFile = file; }
        void SetLoopCount(unsigned count)
        { mLoopCount = count; }
        void EnablePcmCaching(bool on_off)
        { mEnablePcmCaching = on_off; }
        void EnableFileCaching(bool on_off)
        { mEnableFileCaching = on_off; }
        void SetIOStrategy(IOStrategy strategy)
        { mIOStrategy = strategy; }
        struct FileInfo {
            unsigned channels    = 0;
            unsigned frames      = 0;
            unsigned sample_rate = 0;
            float seconds = 0;
            // File size (compressed) in bytes.
            unsigned bytes = 0;
        };
        static bool ProbeFile(const std::string& file, FileInfo* info);
        static void ClearCache();
    private:
        class  PCMDecoder;
        struct PCMBuffer;
        using PCMCache = std::unordered_map<std::string,
            std::shared_ptr<PCMBuffer>>;
        using FileInfoCache = std::unordered_map<std::string, FileInfo>;

        static PCMCache pcm_cache;
        static FileInfoCache file_info_cache;
    private:
        const std::string mName;
        const std::string mId;
        std::string mFile;
        std::unique_ptr<Decoder> mDecoder;
        std::shared_ptr<PCMBuffer> mPCMBuffer;
        SingleSlotPort mPort;
        Format mFormat;
        unsigned mFramesRead = 0;
        unsigned mPlayCount  = 0;
        unsigned mLoopCount  = 1;
        bool mEnablePcmCaching  = false;
        bool mEnableFileCaching = false;
        IOStrategy mIOStrategy = IOStrategy::Default;
        base::TaskHandle mOpenDecoderTask;
    };

} // namespace