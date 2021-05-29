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

#include "warnpop.h"
#  include <nlohmann/json.hpp>
#include "warnpush.h"

#include <string>
#include <memory>

#include "base/utility.h"

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
            nlohmann::json ToJson()
            {
                nlohmann::json json;
                base::JsonWrite(json, "id", mId);
                base::JsonWrite(json, "uri", mFileURI);
                return json;
            }
            std::unique_ptr<FileResource> Clone() const
            {
                FileResource ret;
                ret.mFileURI = mFileURI;
                return std::make_unique<FileResource>(ret);
            }
            static std::optional<FileResource> FromJson(const nlohmann::json& json)
            {
                FileResource ret;
                if (!base::JsonReadSafe(json, "id", &ret.mId) ||
                    !base::JsonReadSafe(json, "uri", &ret.mFileURI))
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
