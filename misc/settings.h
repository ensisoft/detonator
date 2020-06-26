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

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <string>
#include <fstream>
#include <vector>

#include "base/utility.h"
#include "base/platform.h"

namespace misc
{
    // Load game settings from a "settings" file (currently JSON)
    // Expects that the file is "known" and has good content and is accessible
    // i.e. no provisions are made for content/access problems.
    class Settings
    {
    public:
        Settings() = default;
        Settings(const std::string& file)
        {
            LoadFromFile(file);
        }
        // Load settings data from the given file.
        // The file name is expected to be UTF-8 encoded.
        // If anything fails an exception gets thrown, the contents are
        // expected to be *good* and the file *accessible*.
        void LoadFromFile(const std::string& file)
        {
            // this class expects that the file is "known" and "good"
            // i.e. the content's are good (not junk/tampered with) and
            // that the file is accessible.
            // todo: see if there's something more portable/standard already
            // to deal with file encodings already!
            #if defined(WINDOWS_OS)
                std::fstream in(base::FromUtf8(file)); // msvs extension
            #elif defined(POSIX_OS)
                std::fstream in(file);
            #endif
            if (!in.is_open())
                throw std::runtime_error("failed to open: " + file);
            mJson = nlohmann::json::parse(in);
        }
        // Save the settings data to the given file.
        // The file name is expected to be UTF-8 encoded.
        // If anythign fails an exception gets thrown.
        void SaveToFile(const std::string& file)
        {
            // this class expects that the file is "known" and "good"
            // i.e. the content's are good (not junk/tampered with) and
            // that the file is accessible.
            // todo: see if there's something more portable/standard already
            // to deal with file encodings already!
            #if defined(WINDOWS_OS)
                std::ofstream out(base::FromUtf8(file), std::ios::out | std::ios::trunc);
            #elif defined(POSIX_OS)
                std::ofstream out(file, std::ios::out | std::ios::trunc);
            #endif
            if (!out.is_open())
                throw std::runtime_error("failed to open: " + file);
            const auto& content = mJson.dump(2);
            if (!out.write(content.data(), content.size()))
                throw std::runtime_error("failed to write JSON in: " + file);
        }

        // Set a new value of type T under the given module/key pair.
        // If the value already exists it's overwritten and any previous
        // value is lost.
        template<typename T>
        void SetValue(const std::string& module, const std::string& key, const T& value)
        {
            auto& obj = mJson[module];
            base::JsonWrite(obj, key.c_str(), value);
        }

        template<typename T>
        void SetValue(const std::string& module, const std::string& key, const std::vector<T>& values)
        {
            auto& obj = mJson[module][key];
            for (const auto& value : values)
                base::JsonAppend(obj, value);
        }

        // Get the value under module/key as an object of type T.
        // If the module/key pair doesn't actually map to a value of a type OR
        // the module/key pair doesn't exist then  T the default_value is returned instead.
        template<typename T>
        T GetValue(const std::string& module, const std::string& key, const T& default_value) const
        {
            T value;
            if (!mJson.contains(module))
                return default_value;
            if (!base::JsonReadSafe(mJson[module], key.c_str(), &value))
                return default_value;
            return value;
        }

        template<typename T>
        std::vector<T> GetValue(const std::string& module, const std::string& key, const std::vector<T>& default_values) const
        {
            if (!mJson.contains(module))
                return default_values;
            auto& obj = mJson[module];
            if (!obj.contains(key) || !obj[key].is_array())
                return default_values;

            std::vector<T> ret;
            for (const auto& json : obj[key].items())
            {
                T value;
                if (!base::JsonReadSafe(json.value(), &value))
                    return default_values;
                ret.push_back(std::move(value));
            }
            return ret;
        }

        // Get the "string" value by mapping const char* std::string.
        std::string GetValue(const std::string& module, const std::string& key, const char* string) const
        {
            // special case to deal with const char* -> string to make life simpler.
            return GetValue(module, key, std::string(string));
        }

        // Returns true if the given value exists under by the given module/key,
        // otherwise returns false.
        bool HasValue(const std::string& module, const std::string& key) const
        {
            if (!mJson.contains(module) || !mJson[module].is_object())
                return false;
            if (!mJson[module].contains(key))
                return false;
            return true;
        }

        // Clear the settings and remove all keys and values.
        void Clear()
        {
            mJson.clear();
        }
    private:
        nlohmann::json mJson;
    };

} // namespace
