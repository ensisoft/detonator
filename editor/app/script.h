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
#include "editor/app/types.h"

namespace app
{
    namespace detail {
        // Generic file based resource.
        template<unsigned Discriminator>
        class FileResource
        {
        public:
            enum class TypeTag {
                Unspecified,
                External,
                TilemapData,
                ScriptData
            };

            FileResource()
            { mId = base::RandomString(10); }
            TypeTag GetTypeTag() const
            { return mTypeTag; }
            std::string GetId() const
            { return mId; }
            std::string GetName() const
            { return mName; }
            std::string GetFileURI() const
            { return mFileURI; }
            std::string GetOwnerId() const
            { return mOwnerId; }
            void SetName(const std::string& name)
            { mName = name; }
            void SetFileURI(const AnyString& uri)
            { mFileURI = uri; }
            void SetOwnerId(const AnyString& id)
            { mOwnerId = id; }
            void SetTypeTag(TypeTag tag)
            { mTypeTag = tag; }
            void IntoJson(data::Writer& data) const
            {
                data.Write("id",    mId);
                data.Write("name",  mName);
                data.Write("uri",   mFileURI);
                data.Write("owner", mOwnerId);
                data.Write("type",  mTypeTag);
            }
            std::unique_ptr<FileResource> Clone() const
            {
                FileResource ret(*this);
                ret.mId = base::RandomString(10);
                return std::make_unique<FileResource>(ret);
            }
            static std::optional<FileResource> FromJson(const data::Reader& data)
            {
                FileResource ret;
                data.Read("id",    &ret.mId);
                data.Read("name",  &ret.mName);
                data.Read("uri",   &ret.mFileURI);
                data.Read("owner", &ret.mOwnerId);
                data.Read("type",  &ret.mTypeTag);
                return ret;
            }
        public:
            // ID of the resource.
            std::string mId;
            // Human-readable name of the resource.
            std::string mName;
            // UTF8 encoded file URI. (Workspace file mapping).
            std::string mFileURI;
            // ID of the "owner" resource, i.e. for example tile map layer
            std::string mOwnerId;
            TypeTag mTypeTag = TypeTag::Unspecified;
        };
    } // detail
    using Script = detail::FileResource<0>;
    using DataFile = detail::FileResource<1>;

} // namespace
