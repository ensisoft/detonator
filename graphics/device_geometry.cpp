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
#include "graphics/device_geometry.h"

namespace gfx
{

DeviceGeometry::~DeviceGeometry()
{
    if (mVertexBuffer.IsValid())
    {
        mDevice->FreeBuffer(mVertexBuffer);
    }
    if (mIndexBuffer.IsValid())
    {
        mDevice->FreeBuffer(mIndexBuffer);
    }
    if (mUsage == gfx::BufferUsage::Static)
        DEBUG("Deleted geometry object. [name='%1']", mName);

}

void DeviceGeometry::Upload() const
{
    if (!mPendingUpload)
        return;

    const auto upload = std::move(mPendingUpload);

    mPendingUpload.reset();

    const auto vertex_bytes = upload->GetVertexBytes();
    const auto index_bytes  = upload->GetIndexBytes();
    const auto vertex_ptr   = upload->GetVertexDataPtr();
    const auto index_ptr    = upload->GetIndexDataPtr();
    if (vertex_bytes == 0)
        return;

    if (mVertexBuffer.buffer_bytes != vertex_bytes)
    {
        mVertexBuffer = mDevice->AllocateBuffer(vertex_bytes, mUsage, dev::BufferType::VertexBuffer);
        if (mUsage == gfx::BufferUsage::Static)
        {
            DEBUG("Uploaded geometry vertices. [name='%1', bytes='%2', usage='%3']", mName, vertex_bytes, mUsage);
        }
    }

    mDevice->UploadBuffer(mVertexBuffer, vertex_ptr, vertex_bytes);
    mVertexLayout = upload->GetLayout();
    mDrawCommands = upload->GetDrawCommands();

    if (index_bytes == 0)
        return;

    if (mIndexBuffer.buffer_bytes != index_bytes)
    {
        mIndexBuffer = mDevice->AllocateBuffer(index_bytes, mUsage, dev::BufferType::IndexBuffer);
        if (mUsage == gfx::BufferUsage::Static)
        {
            DEBUG("Uploaded geometry indices. [name='%1', bytes='%2', usage='%3']", mName, index_bytes, mUsage);
        }
    }

    mDevice->UploadBuffer(mIndexBuffer, index_ptr, index_bytes);
    mIndexBufferType = upload->GetIndexType();
}

} // namespace