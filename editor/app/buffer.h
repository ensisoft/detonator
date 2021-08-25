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
#include "warnpop.h"

#include <memory>

#include "editor/app/utility.h"
#include "graphics/resource.h"
#include "engine/data.h"

namespace app
{
    namespace detail {
        class FileBufferImpl
        {
        public:
            static bool LoadData(const QString& file, QByteArray* data);
        private:
        };

        template<typename Interface>
        class FileBuffer : public Interface,
                           private FileBufferImpl
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
            virtual std::size_t GetSize() const override
            { return mFileData.size(); }
            virtual std::string GetName() const override
            { return app::ToUtf8(mFileName); }

            static std::shared_ptr<const FileBuffer> LoadFromFile(const QString& file)
            {
                QByteArray data;
                if (!FileBufferImpl::LoadData(file, &data))
                    return nullptr;
                return std::make_shared<FileBuffer>(file, data);
            }
        private:
            const QString mFileName;
            const QByteArray mFileData;
        };
    } // detail

    // currently both gfx::Resource and game::GameData share
    // the same interface so the impl can be shared.
    using GraphicsFileBuffer = detail::FileBuffer<gfx::Resource>;
    using GameDataFileBuffer = detail::FileBuffer<engine::GameData>;

} // namespace
