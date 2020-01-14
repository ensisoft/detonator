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
#  include <neargye/magic_enum.hpp>
#  include <QComboBox>
#  include <QString>
#include "warnpop.h"

#include <string>
#include <string_view>

#include "base/assert.h"

namespace gui
{

template<typename EnumT>
void PopulateFromEnum(QComboBox* combo)
{
    combo->clear();
    constexpr auto& values = magic_enum::enum_values<EnumT>();
    for (const auto& val : values)
    {
        const std::string name(magic_enum::enum_name(val));
        combo->addItem(QString::fromStdString(name));
    }
}

template<typename EnumT>
EnumT EnumFromCombo(const QComboBox* combo)
{
    const auto& text = combo->currentText();
    const auto& name = text.toStdString();
    const auto& val = magic_enum::enum_cast<EnumT>(name);
    ASSERT(val.has_value());
    return val.value();
}

} // namespace