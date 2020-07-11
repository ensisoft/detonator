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

#include "config.h"

#include "format.h"

namespace app
{

QString toString(QFile::FileError error)
{
    using e = QFile::FileError;
    switch (error)
    {
        case e::NoError:          return "No error occrred.";
        case e::ReadError:        return "An error occurred when reading from the file.";
        case e::WriteError:       return "An error occurred when writing to the file.";
        case e::FatalError:       return "A fatal error occurred.";
        case e::ResourceError:    return "A resource error occurred.";
        case e::OpenError:        return "The file could not be opened.";
        case e::AbortError:       return "The operation was aborted.";
        case e::TimeOutError:     return "A timeout occurred.";
        case e::UnspecifiedError: return "An unspecified error occurred.";
        case e::RemoveError:      return "The file could not be removed.";
        case e::RenameError:      return "The file could not be renamed.";
        case e::PositionError:    return "The position in file could not be changed.";
        case e::ResizeError:      return "The file could not be resized.";
        case e::PermissionsError: return "The file could not be accessed (no permission).";
        case e::CopyError:        return "The file could not be copied.";
    }
    return "???";
}

} // app
