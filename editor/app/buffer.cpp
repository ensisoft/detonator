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

#define LOGTAG "workspace"

#include "config.h"

#include "warnpush.h"
  #include <QFile>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "editor/app/buffer.h"

namespace app
{
// static
bool detail::FileBufferImpl::LoadData(const QString& file, QByteArray* data)
{
    QFile io;
    io.setFileName(file);
    io.open(QIODevice::ReadOnly);
    if (!io.isOpen())
    {
        ERROR("Failed to open '%1' for reading (%2).", file, io.error());
        return false;
    }
    *data = io.readAll();
    DEBUG("Loaded %1 bytes from file '%2'.", data->size(), file);
    return true;
}

} // namespace
