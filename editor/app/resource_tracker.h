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

#pragma once

#include "config.h"

#include <unordered_set>

#include "editor/app/resource_packer.h"

namespace app
{
    class ResourceTracker : public ResourcePacker
    {
    public:
        using UriSet = std::unordered_set<AnyString>;

        ResourceTracker(const AnyString& ws_dir, UriSet* result_set)
          : mWorkspaceDir(ws_dir)
          , mResultSet(result_set)
        {}
        bool CopyFile(const AnyString& uri, const AnyString& dir) override;
        bool WriteFile(const AnyString& uri, const AnyString& dir, const void* data, size_t len) override;
        bool ReadFile(const AnyString& uri, QByteArray* bytes) const override;

        AnyString MapUri(const AnyString& uri) const override
        { return uri; }
        bool HasMapping(const AnyString& uri) const override
        { return true; }
        bool IsReleasePackage() const override
        { return false; }

    private:
        void RecordURI(const AnyString& uri);

    private:
        QString mWorkspaceDir;
        mutable UriSet* mResultSet = nullptr;
    };

} // namespace