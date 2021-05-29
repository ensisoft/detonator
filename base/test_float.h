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

#include <cstdint>
#include <cstdlib>
#include <cassert>

namespace real
{
    class float32
    {
    public:
        explicit
        float32(double d) : m_f(d)
        {}

        explicit
        float32(float f) :  m_f(f)
        {}

        explicit
        float32(uint32_t i) : m_i(i)
        {}

        int sign() const
        {
            // 1 for negative numbers, 0 for positive
            return (m_i >> 31);
        }
        int exponent() const
        {
            return ((m_i >> 23) & 0xff);
        }
        int mantissa() const
        {
            return m_i & ((1 << 23) - 1);
        }
        float as_float() const
        {
            return m_f;
        }
        uint32_t as_uint() const
        {
            return m_i;
        }
        float32 operator++(int)
        {
            float32 ret(*this);
            this->inc();
            return ret;
        }
        float32& operator++()
        {
            this->inc();
            return *this;
        }
        float32 operator--(int)
        {
            float32 ret(*this);
            this->dec();
            return ret;
        }
        float32& operator--()
        {
            this->dec();
            return *this;
        }

        void inc()
        {
            ++m_i;
        }
        void dec()
        {
            --m_i;
        }
        int ulps(const float32& other) const
        {
            return std::abs((long)m_i - (long)other.m_i);
        }
        bool is_zero() const
        {
            return !(m_i & 0x7FFFFFFF);
        }
        bool is_NaN() const
        {
            return exponent() == 255 && mantissa() != 0;
        }
        bool is_inf() const
        {
            return exponent() == 255 && mantissa() == 0;
        }
    private:
        // little bit unsafe...
        union {
            float    m_f;
            uint32_t m_i;
        };
    };

    inline bool equals(float a, float b)
    {
        const float32 a32(a);
        const float32 b32(b);

        assert(!a32.is_NaN() && !a32.is_inf());
        assert(!b32.is_NaN() && !b32.is_inf());

        if (a32.sign() != b32.sign())
        {
            if (a32.is_zero() && b32.is_zero())
                return true;
            return false;
        }

        const int ulps = a32.ulps(b32);
        return (ulps <= 1);
    }

    inline bool operator==(const float32& f32, float f)
    { return equals(f32.as_float(), f); }
    inline bool operator!=(const float32& f32, float f)
    { return !equals(f32.as_float(), f); }

    inline bool operator==(float f, const float32& f32)
    { return equals(f, f32.as_float()); }
    inline bool operator!=(float f, const float32& f32)
    { return !equals(f, f32.as_float()); }

    inline bool operator!=(const float32& lhs, const float32& rhs)
    { return !equals(lhs.as_float(), rhs.as_float()); }
    inline bool operator==(const float32& lhs, const float32& rhs)
    { return equals(lhs.as_float(), rhs.as_float()); }

#define F32(x) \
    real::float32(x)

} // namespace
