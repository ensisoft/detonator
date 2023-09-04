// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include <locale>
#include <sstream>
#include <string>

#include "base/assert.h"
#include "base/types.h"

namespace base
{
    namespace detail {
        class InputStream
        {
        public:
            explicit InputStream(const std::string& str) noexcept
              : mInput(str)
            {}
            inline int ReadAt(size_t index) const noexcept
            {
                ASSERT(index < mInput.size());
                return mInput[index];
            }
            inline size_t Size() const noexcept
            { return mInput.size(); }

        private:
            const std::string& mInput;
        };

        class Input
        {
        public:
            explicit Input(const InputStream& stream, size_t pos = 0) noexcept
              : mStream(&stream)
              , mPos(pos)
            {}
            inline int Get() const noexcept
            { return mStream->ReadAt(mPos); }
            inline int GetNext() noexcept
            { return mStream->ReadAt(mPos++); }
            inline bool AtEnd() const noexcept
            { return mPos == mStream->Size(); }
            inline bool Next() noexcept
            { return ++mPos != mStream->Size(); }
            inline bool HasNext() const noexcept
            { return (mPos + 1) < mStream->Size(); }
            inline int PeekNext() noexcept
            { return mStream->ReadAt(mPos + 1); }
        private:
            const InputStream* mStream;
            std::size_t mPos = 0;
        };

        bool ScanValue(Input& in, const char* literal)
        {
            const auto len = std::strlen(literal);

            size_t index = 0;
            while (!in.AtEnd() && index < len) {
                const auto val = in.GetNext();
                if (literal[index++] != val)
                    return false;
            }
            if (index == len)
                return true;
            return false;
        }

        bool ScanValue(Input& in, float* value)
        {
            std::string str;
            while (!in.AtEnd())
            {
                const auto val = in.Get();
                if (val == '-' || val == '.' || std::isdigit(val))
                {
                    str.push_back(val);
                    in.Next();
                }
                else break;
            }
            std::stringstream ss;
            ss.imbue(std::locale("C"));
            ss << str;
            ss >> *value;
            return !ss.fail();
        }
        bool ScanValue(Input& in, int* value)
        {
            std::string str;
            while (!in.AtEnd())
            {
                const auto val = in.Get();
                if (val == '-' || std::isdigit(val))
                {
                    str.push_back(val);
                    in.Next();
                }
                else break;
            }

            std::stringstream ss;
            ss.imbue(std::locale("C"));
            ss << str;
            ss >> *value;
            return !ss.fail();
        }
        bool ScanValue(Input& in, std::string* value)
        {
            if (in.Get() != '\'')
                return false;

            in.Next();

            std::string str;
            while (!in.AtEnd())
            {
                const auto val = in.Get();
                if (val == '\'')
                {
                    if (!str.empty() && str.back() == '\\')
                    {
                        str.pop_back();
                        str.push_back(val);
                    }
                    else  break;
                }
                else
                {
                    str.push_back(val);
                }
                in.Next();
            }
            if (in.Get() != '\'')
                return false;
            in.Next();
            *value = std::move(str);
            return true;
        }

        template<typename T>
        bool ScanNextValue(Input& in, T* value)
        {
            while (!in.AtEnd())
            {
                const auto val = in.Get();
                if (!std::isspace(val))
                    break;

                in.Next();
            }
            if (in.AtEnd())
                return false;

            return ScanValue(in, value);
        }

        bool ScanNext(Input& in, Color4f* color)
        {
            float r, g, b, a;
            if (!ScanNextValue(in, &r) ||
                !ScanNextValue(in, ",") ||
                !ScanNextValue(in, &g) ||
                !ScanNextValue(in, ",") ||
                !ScanNextValue(in, &b) ||
                !ScanNextValue(in, ",") ||
                !ScanNextValue(in, &a))
                return false;

            *color = Color4f(r, g, b, a);
            return true;
        }

        bool ScanNext(Input& in, FSize* size)
        {
            float w, h;
            if (!ScanNextValue(in, &w) ||
                !ScanNextValue(in, ",") ||
                !ScanNextValue(in, &h))
                return false;

            *size = FSize(w, h);
            return true;
        }
        bool ScanNext(Input& in, FPoint* point)
        {
            float x, y;
            if (!ScanNextValue(in, &x) ||
                !ScanNextValue(in, ",") ||
                !ScanNextValue(in, &y))
                return false;
            *point = FPoint(x, y);
            return true;
        }

        bool ScanNext(Input& in, bool* boolean)
        {
            {
                Input tmp(in);
                if (ScanNextValue(tmp, "true"))
                {
                    *boolean = true;
                    in = tmp;
                    return true;
                }

            }
            {
                Input tmp(in);
                if (ScanNextValue(tmp, "false"))
                {
                    *boolean = false;
                    in = tmp;
                    return true;
                }
            }
            {
                int val = 0;
                if (ScanNextValue(in, &val))
                {
                    *boolean = val != 0;
                    return true;
                }
            }
            return false;
        }

        template<typename T>
        bool ScanNext(Input& in, T* value)
        {
            return ScanNextValue(in, value);
        }

        template<typename T, typename... Args>
        bool ScanNext(Input& in, T first, Args... rest)
        {
            if (!ScanNext(in, first))
                return false;
            return ScanNext(in, rest...);
        }

    } // namespace detail

    template<typename... Args>
    bool Scanf(const std::string& str, Args... args)
    {
        detail::InputStream stream(str);
        detail::Input input(stream);
        return detail::ScanNext(input, args...);
    }

} // namespace
