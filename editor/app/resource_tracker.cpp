// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include "editor/app/buffer.h"
#include "editor/app/utility.h"
#include "editor/app/resource_tracker.h"

namespace app
{

bool ResourceTracker::CopyFile(const AnyString& uri, const AnyString& dir)
{
    RecordURI(uri);
    return true;
}

bool ResourceTracker::WriteFile(const AnyString& uri, const AnyString& dir, const void* data, size_t len)
{
    RecordURI(uri);
    return true;
}

bool ResourceTracker::ReadFile(const AnyString& uri, QByteArray* bytes) const
{
    const auto& file = MapUriToFile(uri, mWorkspaceDir);

    return app::detail::LoadArrayBuffer(file, bytes);
}

void ResourceTracker::RecordURI(const AnyString& uri)
{
    if (uri.EndsWith("png", Qt::CaseInsensitive) ||
        uri.EndsWith("jpg", Qt::CaseInsensitive) ||
        uri.EndsWith("jpeg", Qt::CaseInsensitive)  ||
        uri.EndsWith("bmp", Qt::CaseInsensitive))
    {
        const auto image_file = MapUriToFile(uri, mWorkspaceDir);
        const auto image_desc = FindImageJsonFile(image_file);
        if (!image_desc.isEmpty())
        {
            mResultSet->insert(MapFileToUri(image_desc, mWorkspaceDir));
        }
    }

    if (IsBitmapFontJsonUri(uri))
        mResultSet->insert(FontBitmapUriFromJsonUri(uri));

    mResultSet->insert(uri); // keep track of the URIs we're seeing
}

} // namespace