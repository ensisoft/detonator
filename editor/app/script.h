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
    // script resource. Currently this resource isn't part of any of
    // the actual subsystem (gamelib, graphics, audio etc). It's only
    // relevant to the editor for the purposes of showing the scripts
    // in the resource table and for packing the resources.
    class Script
    {
    public:
        Script()
        { mScriptId = base::RandomString(10); }
        std::string GetId() const
        { return mScriptId; }
        std::string GetFileURI() const
        { return mFileURI; }
        void SetFileURI(const std::string& uri)
        { mFileURI = uri; }
        nlohmann::json ToJson()
        {
            nlohmann::json json;
            base::JsonWrite(json, "uri", mFileURI);
            return json;
        }
        std::unique_ptr<Script> Clone() const
        {
            Script ret;
            ret.mFileURI = mFileURI;
            return std::make_unique<Script>(ret);
        }
        static std::optional<Script> FromJson(const nlohmann::json& json)
        {
            Script ret;
            if (!base::JsonReadSafe(json, "uri", &ret.mFileURI))
                return std::nullopt;
            return ret;
        }
    private:
        std::string mScriptId;
        // UTF8 encoded file URI. (Workspace file mapping).
        std::string mFileURI;
    };

} // namespace
