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

#include "data/writer.h"
#include "data/reader.h"
namespace app
{
    namespace detail {
        // Generic file based resource.
        template<unsigned Discriminator>
        class FileResource
        {
        public:
            FileResource()
            { mId = base::RandomString(10); }
            std::string GetId() const
            { return mId; }
            std::string GetFileURI() const
            { return mFileURI; }
            void SetFileURI(const std::string& uri)
            { mFileURI = uri; }
            void IntoJson(data::Writer& data) const
            {
                data.Write("id", mId);
                data.Write("uri", mFileURI);
            }
            std::unique_ptr<FileResource> Clone() const
            {
                FileResource ret;
                ret.mFileURI = mFileURI;
                return std::make_unique<FileResource>(ret);
            }
            static std::optional<FileResource> FromJson(const data::Reader& data)
            {
                FileResource ret;
                if (!data.Read("id", &ret.mId) ||
                    !data.Read("uri", &ret.mFileURI))
                    return std::nullopt;
                return ret;
            }
        public:
            std::string mId;
            // UTF8 encoded file URI. (Workspace file mapping).
            std::string mFileURI;
        };
    } // detail
    using Script = detail::FileResource<0>;
    using AudioFile = detail::FileResource<1>;
    using DataFile = detail::FileResource<2>;

} // namespace
