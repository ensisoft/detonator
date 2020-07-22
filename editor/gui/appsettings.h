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
#  include <QString>
#include "warnpop.h"

namespace gui
{
    // Collection of global application settings.
    struct AppSettings {
        // the path to the currently set external image editor application.
        // for example /usr/bin/gimp
        QString image_editor_executable;
        // the arguments for the image editor. The special argument ${file}
        // is expanded to contain the full native absolute path to the
        // file that user waats to work with.
        QString image_editor_arguments;
        // the path to the currently set external editor for shader (.glsl) files.
        // for example /usr/bin/gedit
        QString shader_editor_executable;
        // the arguments for the shader editor. The special argument ${file}
        // is expanded to contain the full native absolute path to the
        // file that the user wants to open.
        QString shader_editor_arguments;
    };

} // namespace