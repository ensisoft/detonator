// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
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
