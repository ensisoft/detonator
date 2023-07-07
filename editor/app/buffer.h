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

#include "warnpush.h"
#  include <QString>
#  include <QByteArray>
#  include <QFile>
#include "warnpop.h"

#include <memory>
#include <vector>

#include "editor/app/utility.h"
#include "graphics/resource.h"
#include "audio/loader.h"
#include "game/loader.h"
#include "engine/data.h"

namespace app
{
    namespace detail {
        bool LoadArrayBuffer(const QString& file, QByteArray* data);

        template<typename Interface>
        class FileBuffer : public Interface
        {
        public:
            FileBuffer(QString filename, QByteArray data)
                : mFileName(filename)
                , mFileData(data)
            {}
            virtual const void* GetData() const override
            {
                if (mFileData.isEmpty())
                    return nullptr;
                return (const void*)mFileData.data();
            }
            virtual std::size_t GetByteSize() const override
            { return mFileData.size(); }
            virtual std::string GetSourceName() const override
            { return app::ToUtf8(mFileName); }

            static std::shared_ptr<const FileBuffer> LoadFromFile(const QString& file)
            {
                QByteArray data;
                if (!detail::LoadArrayBuffer(file, &data))
                    return nullptr;
                return std::make_shared<FileBuffer>(file, data);
            }
        private:
            const QString mFileName;
            const QByteArray mFileData;
        };
    } // detail

    // This implementation for tilemap data is used for read/write access.
    // The changes are kept in memory since the original data file is the
    // source of data as created by the designer and cannot be changed by
    // the running game instance.
    class TilemapBuffer : public game::TilemapData
    {
    public:
        TilemapBuffer(QString filename, QByteArray data)
          : mFile(filename)
          , mBytes(data)
        {}

        virtual void Write(const void* ptr, size_t bytes, size_t offset) override;
        virtual void Read(void* ptr, size_t bytes, size_t offset) const override;
        virtual size_t AppendChunk(size_t bytes) override;
        virtual size_t GetByteCount() const override;
        virtual void Resize(size_t bytes) override;
        virtual void ClearChunk(const void* value, size_t value_size, size_t offset, size_t num_values) override;

        static std::shared_ptr<TilemapBuffer> LoadFromFile(const QString& file);
    private:
        const QString mFile;
        QByteArray mBytes;
    };

    // This implementation of tilemap data is used for read-only access.
    // This allows for an improved performance by directly mapping the
    // original map data created by the designer in the editor since
    // only read access is allowed.
    class TilemapMemoryMap : public game::TilemapData
    {
    public:
        TilemapMemoryMap(const uchar* addr, quint64 size, std::unique_ptr<QFile> file)
          : mMapAddr(addr)
          , mSize(size)
          , mFile(std::move(file))
        {}
       ~TilemapMemoryMap()
        {
            ASSERT(mFile->unmap((uchar*)mMapAddr));
        }

        virtual void Write(const void* ptr, size_t bytes, size_t offset) override;
        virtual void Read(void* ptr, size_t bytes, size_t offset) const override;
        virtual size_t AppendChunk(size_t bytes) override;
        virtual size_t GetByteCount() const override;
        virtual void Resize(size_t bytes) override;
        virtual void ClearChunk(const void* value, size_t value_size, size_t offset, size_t num_values) override;

        static std::shared_ptr<TilemapMemoryMap> OpenFilemap(const QString& file);
    private:
        const uchar* const mMapAddr = nullptr;
        const quint64 mSize = 0;
        std::unique_ptr<QFile> mFile;
    };


    // Currently, both gfx::Resource and game::EngineData share
    // the same interface so the impl can be shared.
    using GraphicsBuffer = detail::FileBuffer<gfx::Resource>;
    using EngineBuffer = detail::FileBuffer<engine::EngineData>;

} // namespace
