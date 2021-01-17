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

#include <string>

namespace gui
{
    // there's the QClipboard but this clipboard is simpler
    // and only within this application.
    class Clipboard
    {
    public:
        void SetText(const std::string& text)
        {mText = text; }
        void SetText(std::string&& text)
        {mText = std::move(text); }
        void SetType(const std::string& type)
        { mType = type; }
        void SetType(std::string&& type)
        { mType = std::move(type); }
        bool IsEmpty() const
        { return mText.empty(); }
        void Clear()
        {
            mText.clear();
            mType.clear();
        }
        const std::string& GetType() const
        { return mType; }
        const std::string& GetText() const
        { return mText; }
    private:
        std::string mText;
        std::string mType;
    };

} //  namespace