// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include "base/assert.h"
#include "editor/app/types.h"

namespace app
{
    class ResourcePacker
    {
    public:
        enum class Operation {
            Deploy, Import, Export, Track
        };

        virtual ~ResourcePacker() = default;
        virtual bool CopyFile(const AnyString& uri, const AnyString& dir) = 0;
        virtual bool WriteFile(const AnyString& uri, const AnyString& dir, const void* data, size_t len) = 0;
        virtual bool ReadFile(const AnyString& uri, QByteArray* bytes) const = 0;
        virtual bool HasMapping(const AnyString& uri) const = 0;
        virtual AnyString MapUri(const AnyString& uri) const = 0;
        virtual Operation GetOp() const = 0;

        bool IsReleasePackage() const
        {
            return GetOp() == Operation::Deploy;
        }
        bool NeedsRemapping() const
        {
            const auto op = GetOp();
            if (op == Operation::Deploy || op == Operation::Import ||
                op == Operation::Export)
                return true;
            else if (op == Operation::Track)
                return false;
            BUG("Bug on resource packer operation.");
        }
    private:
    };

} // namespace
