// Copyright (c) 2014 Sami Väisänen, Ensisoft
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
