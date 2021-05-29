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

// Copyright (c) 2013 Sami Väisänen, Ensisoft
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

#include <cstdint>
#include <cassert>
#include <initializer_list>

namespace base
{
    template<typename Enum,
        typename Bits = std::uint32_t>
    class bitflag
    {
    public:
        enum {
            BitCount = sizeof(Bits) * 8
        };
        bitflag() = default;
        bitflag(Enum initial)
        {
            set(initial);
        }
        bitflag(Bits value) : bits_(value)
        {}
        bitflag(const std::initializer_list<Enum>& values)
        {
            for (auto e : values)
            {
                set(e, true);
            }
        }

        bitflag& set(Enum value, bool on = true)
        {
            const auto b = bittify(value);

            if (on)
                bits_ |= b;
            else bits_ &= ~b;
            return *this;
        }

        bitflag& operator |= (bitflag other)
        {
            bits_ |= other.bits_;
            return *this;
        }

        bitflag& operator &= (bitflag other)
        {
            bits_ &= other.bits_;
            return *this;
        }

        void flip(Enum value)
        {
            const auto b = bittify(value);
            bits_ ^= b;
        }

        void flip(unsigned index)
        {
            const auto b = bittify((Enum)index);
            bits_ ^= b;
        }

        // test a particular value.
        bool test(Enum value) const
        {
            const auto b = bittify(value);
            return (bits_ & b) == b;
        }

        // test for any value.
        bool test(bitflag values) const
        { return bits_ & values.bits_; }

        // test of the nth bith.
        bool test(unsigned index) const
        {
            const auto b = bittify((Enum)index);
            return (bits_ & b);
        }

        void clear()
        { bits_ = 0x0; }

        bool any_bit() const
        { return bits_ != 0; }

        Bits value() const
        { return bits_; }

        void set_from_value(Bits b)
        { bits_ = b; }

    private:
        Bits bittify(Enum value) const
        {
            assert((unsigned)value < BitCount &&
                "The value of enum member is too large to fit in the bitset."
                "You need to use larger underlying type.");
            return Bits(1) << Bits(value);
        }

    private:
        Bits bits_ = 0;
    };

    // we only provide this operator, since its global
    // this also covers Enum | bitflag<Enum> and bitflag<Enum> | Enum
    // through implicit conversion to bitflag<Enum>
    template<typename Enum, typename Bits>
    auto operator | (bitflag<Enum, Bits> lhs, bitflag<Enum, Bits> rhs) -> decltype(lhs)
    {
        return { lhs.value() | rhs.value() };
    }

    template<typename Enum, typename Bits>
    auto operator | (bitflag<Enum, Bits> lhs, Enum e) -> decltype(lhs)
    {
        return lhs | bitflag<Enum, Bits>(e);
    }

    template<typename Enum, typename Bits>
    auto operator & (bitflag<Enum, Bits> lhs, bitflag<Enum, Bits> rhs) -> decltype(lhs)
    {
        return bitflag<Enum, Bits>(lhs.value() & rhs.value());
    }

    template<typename Enum, typename Bits>
    auto operator & (bitflag<Enum, Bits> lhs, Enum e) -> decltype(lhs)
    {
        return lhs & bitflag<Enum, Bits>(e);
    }

    template<typename Enum, typename Bits>
    bool  operator == (bitflag<Enum, Bits> lhs, bitflag<Enum, Bits> rhs)
    {
        return lhs.value() == rhs.value();
    }

} // base


