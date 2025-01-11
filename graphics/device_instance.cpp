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

#include "base/logging.h"
#include "graphics/device_instance.h"

namespace gfx {

DeviceDrawInstanceBuffer::~DeviceDrawInstanceBuffer()
{
    if (mBuffer.IsValid())
    {
        mDevice->FreeBuffer(mBuffer);
    }
    if (mUsage == Usage::Static)
        DEBUG("Deleted instanced draw object. [name='%1']", mContentName);
}

void DeviceDrawInstanceBuffer::Upload() const
{
    if (!mPendingUpload.has_value())
        return;

    auto upload = std::move(mPendingUpload.value());

    mPendingUpload.reset();

    const auto vertex_bytes = upload.GetInstanceDataSize();
    const auto vertex_ptr   = upload.GetVertexDataPtr();
    if (vertex_bytes == 0)
        return;

    mBuffer = mDevice->AllocateBuffer(vertex_bytes, mUsage, dev::BufferType::VertexBuffer);
    mDevice->UploadBuffer(mBuffer, vertex_ptr, vertex_bytes);

    mLayout = std::move(upload.GetInstanceDataLayout());
    if (mUsage == Usage::Static)
    {
        DEBUG("Uploaded geometry instance buffer data. [name='%1', bytes='%2', usage='%3']", mContentName, vertex_bytes, mUsage);
    }
}

} // namespace