// Copyright (c) 2016 Sami Väisänen, Ensisoft
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

#include <chrono>
#include <memory> // for unique_ptr
#include <string>
#include <locale>
#include <codecvt>
#include <cwctype>

namespace base
{

inline double GetRuntimeSec() 
{
    using steady_clock = std::chrono::steady_clock;
    static const auto start = steady_clock::now();
    const auto now = steady_clock::now();
    const auto gone = now - start;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(gone);
    return ms.count() / 1000.0; 
}

template<typename T, typename Deleter>
std::unique_ptr<T, Deleter> make_unique_ptr(T* ptr, Deleter del)
{
    return std::unique_ptr<T, Deleter>(ptr, del);
}

inline
std::string ToUtf8(const std::wstring& str)
{
    // this way of converting is depcreated since c++17 but
    // this works good enough for now so we'll go with it.
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;            
    std::string converted_str = converter.to_bytes(str);
    return converted_str;    
}

inline 
std::wstring FromUtf8(const std::string& str)
{
    // this way of converting is deprecated since c++17 but
    // this works good enough for now so we'll go with it.
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;                
    std::wstring converted_str = converter.from_bytes(str);
    return converted_str;
}

inline
std::wstring ToUpper(const std::wstring& str)
{
    std::wstring ret;
    for (auto c : str)
        ret.push_back(std::towupper(c));
    return ret;
}


} // base