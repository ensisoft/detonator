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

#include "config.h"

#include <fstream>

#include "base/utility.h"
#include "data/writer.h"
#include "data/io.h"

namespace data
{
bool FileDevice::Open(const std::string& file)
{
    mFile = base::OpenBinaryOutputStream(file);
    if (mFile.is_open())
        return true;
    return false;
}
void FileDevice::Close()
{
    mFile.close();
}

bool FileDevice::WriteBytes(const void* data, size_t bytes)
{
    mFile.write((const char*)data, bytes);
    if (mFile.fail())
        return false;
    return true;
}

std::tuple<bool, std::string> WriteFile(const Writer& chunk, const std::string& file)
{
    FileDevice io;
    if (!io.Open(file))
        return std::make_tuple(false, "failed to open file: " + file);

    if (!chunk.Dump(io))
        return std::make_tuple(false, "file write failed.");

    io.Close();
    return std::make_tuple(true, "");
}

} // namespace
