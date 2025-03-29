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

#include <unordered_map>
#include <stack>

#include "base/assert.h"
#include "base/logging.h"
#include "base/trace.h"
#include "device/graphics.h"
#include "graphics/device.h"
#include "graphics/texture.h"
#include "graphics/drawcmd.h"
#include "graphics/device_texture.h"
#include "graphics/device_geometry.h"
#include "graphics/device_shader.h"
#include "graphics/device_framebuffer.h"
#include "graphics/device_program.h"
#include "graphics/device_instance.h"

namespace {
class GraphicsDevice : public gfx::Device {
public:
    GraphicsDevice(std::shared_ptr<dev::GraphicsDevice> device) noexcept;
    GraphicsDevice(dev::GraphicsDevice* device) noexcept;
   ~GraphicsDevice() override;

    void ClearColor(const gfx::Color4f& color, gfx::Framebuffer* fbo, ColorAttachment attachment) const override;
    void ClearStencil(int value, gfx::Framebuffer* fbo) const override;
    void ClearDepth(float value, gfx::Framebuffer* fbo) const override;
    void ClearColorDepth(const gfx::Color4f& color, float depth, gfx::Framebuffer* fbo, ColorAttachment attachment) const override;
    void ClearColorDepthStencil(const gfx::Color4f& color, float depth, int stencil, gfx::Framebuffer* fbo, ColorAttachment attachment) const override;

    void SetDefaultTextureFilter(MinFilter filter) override
    {
        mDefaultMinTextureFilter = filter;
    }

    void SetDefaultTextureFilter(MagFilter filter) override
    {
        mDefaultMagTextureFilter = filter;
    }

    gfx::ShaderPtr FindShader(const std::string& id) override;
    gfx::ShaderPtr CreateShader(const std::string& id, const gfx::Shader::CreateArgs& args) override;
    gfx::ProgramPtr FindProgram(const std::string& id) override;
    gfx::ProgramPtr CreateProgram(const std::string& id, const gfx::Program::CreateArgs& args) override;
    gfx::GeometryPtr FindGeometry(const std::string& id) override;
    gfx::GeometryPtr CreateGeometry(const std::string& id, gfx::Geometry::CreateArgs args) override;
    gfx::InstancedDrawPtr FindInstancedDraw(const std::string& id) override;
    gfx::InstancedDrawPtr CreateInstancedDraw(const std::string& id, gfx::InstancedDraw::CreateArgs args) override;
    gfx::Texture* FindTexture(const std::string& name) override;
    gfx::Texture* MakeTexture(const std::string& name) override;
    gfx::Framebuffer* FindFramebuffer(const std::string& name) override;
    gfx::Framebuffer* MakeFramebuffer(const std::string& name) override;

    void DeleteShaders() override;
    void DeletePrograms() override;
    void DeleteGeometries() override;
    void DeleteTextures() override;
    void DeleteFramebuffers() override;
    void DeleteFramebuffer(const std::string& id) override;
    void DeleteTexture(const std::string& id) override;

    StateKey PushState() override;
    void PopState(StateKey key) override;
    void SetViewportState(const ViewportState& state) const override;
    void SetColorDepthStencilState(const ColorDepthStencilState& state) const override;

    void Draw(const gfx::Program& program, const gfx::ProgramState& program_state,
              const gfx::GeometryDrawCommand& geometry, const RasterState& state, gfx::Framebuffer* fbo) override;

    void CleanGarbage(size_t max_num_idle_frames, unsigned flags) override;
    void BeginFrame() override;
    void EndFrame(bool display = true) override;
    void GetResourceStats(ResourceStats* stats) const override;
    void GetDeviceCaps(DeviceCaps* caps) const override;
    gfx::Bitmap<gfx::Pixel_RGBA> ReadColorBuffer(unsigned width, unsigned height,
                                                 gfx::Framebuffer* fbo) const override;
    gfx::Bitmap<gfx::Pixel_RGBA> ReadColorBuffer(unsigned x, unsigned y, unsigned width, unsigned height,
                                                 gfx::Framebuffer* fbo) const override;

private:
    dev::Framebuffer SetupFBO(gfx::Framebuffer* fbo) const;
    bool IsTextureFBOTarget(const gfx::Texture* texture) const;

private:
    std::shared_ptr<dev::GraphicsDevice> mDeviceImpl;
    dev::GraphicsDevice* mDevice = nullptr;
    MinFilter mDefaultMinTextureFilter = MinFilter::Nearest;
    MagFilter mDefaultMagTextureFilter = MagFilter::Nearest;

    std::unordered_map<std::string, std::shared_ptr<gfx::InstancedDraw>> mInstances;
    std::unordered_map<std::string, std::shared_ptr<gfx::Geometry>> mGeoms;
    std::unordered_map<std::string, std::shared_ptr<gfx::Shader>> mShaders;
    std::unordered_map<std::string, std::shared_ptr<gfx::Program>> mPrograms;
    std::unordered_map<std::string, std::unique_ptr<gfx::Texture>> mTextures;
    std::unordered_map<std::string, std::unique_ptr<gfx::Framebuffer>> mFBOs;
    std::size_t mFrameNumber = 0;

    struct State {
        ViewportState vs;
        ColorDepthStencilState ds;
    };
    mutable std::stack<State> mStateStack;
};


GraphicsDevice::GraphicsDevice(std::shared_ptr<dev::GraphicsDevice> device) noexcept
  : mDeviceImpl(std::move(device))
  , mDevice(mDeviceImpl.get())
{
    DEBUG("Create gfx::Device");
    mStateStack.push({});
}
GraphicsDevice::GraphicsDevice(dev::GraphicsDevice* device) noexcept
  : mDevice(device)
{
    DEBUG("Create gfx::Device");
    mStateStack.push({});
}

GraphicsDevice::~GraphicsDevice()
{
    DEBUG("Destroy gfx::Device");
    // make sure our cleanup order is specific so that the
    // resources are deleted before the context is deleted.
    mFBOs.clear();
    mTextures.clear();
    mShaders.clear();
    mPrograms.clear();
    mGeoms.clear();
    mInstances.clear();
}

void GraphicsDevice::ClearColor(const gfx::Color4f& color, gfx::Framebuffer* fbo, ColorAttachment attachment) const
{
    const auto& framebuffer = SetupFBO(fbo);
    if (!framebuffer.IsValid())
        return;

    mDevice->ClearColor(color, framebuffer, attachment);
}
void GraphicsDevice::ClearStencil(int value, gfx::Framebuffer* fbo) const
{
    const auto& framebuffer = SetupFBO(fbo);
    if (!framebuffer.IsValid())
        return;

    mDevice->ClearStencil(value, framebuffer);
}
void GraphicsDevice::ClearDepth(float value, gfx::Framebuffer* fbo) const
{
    const auto& framebuffer = SetupFBO(fbo);
    if (!framebuffer.IsValid())
        return;

    mDevice->ClearDepth(value, framebuffer);
}
void GraphicsDevice::ClearColorDepth(const gfx::Color4f& color, float depth, gfx::Framebuffer* fbo, ColorAttachment attachment) const
{
    const auto& framebuffer = SetupFBO(fbo);
    if (!framebuffer.IsValid())
        return;

    mDevice->ClearColorDepth(color, depth, framebuffer, attachment);
}
void GraphicsDevice::ClearColorDepthStencil(const gfx::Color4f& color, float depth, int stencil, gfx::Framebuffer* fbo, ColorAttachment attachment) const
{
    const auto& framebuffer = SetupFBO(fbo);
    if (!framebuffer.IsValid())
        return;

    mDevice->ClearColorDepthStencil(color, depth, stencil, framebuffer, attachment);
}

gfx::ShaderPtr GraphicsDevice::FindShader(const std::string& name)
{
    auto it = mShaders.find(name);
    if (it == std::end(mShaders))
        return nullptr;
    return it->second;
}

gfx::ShaderPtr GraphicsDevice::CreateShader(const std::string& id, const gfx::Shader::CreateArgs& args)
{
    auto shader = std::make_shared<gfx::DeviceShader>(mDevice);
    shader->SetName(args.name);
    shader->CompileSource(args.source, args.debug);
    mShaders[id] = shader;
    return shader;
}

gfx::ProgramPtr GraphicsDevice::FindProgram(const std::string& id)
{
    auto it = mPrograms.find(id);
    if (it == std::end(mPrograms))
        return nullptr;
    return it->second;
}

gfx::ProgramPtr GraphicsDevice::CreateProgram(const std::string& id, const gfx::Program::CreateArgs& args)
{
    auto program = std::make_shared<gfx::DeviceProgram>(mDevice);

    std::vector<gfx::ShaderPtr> shaders;
    shaders.push_back(args.vertex_shader);
    shaders.push_back(args.fragment_shader);

    program->SetId(id);
    program->SetName(args.name);
    program->Build(shaders);

    if (program->IsValid()) {
        // set the initial uniform state
        program->ApplyUniformState(args.state);
    }

    mPrograms[id] = program;
    program->SetFrameStamp(mFrameNumber);
    return program;
}

gfx::GeometryPtr GraphicsDevice::FindGeometry(const std::string& id)
{
    auto it = mGeoms.find(id);
    if (it == std::end(mGeoms))
        return nullptr;
    return it->second;
}

gfx::GeometryPtr GraphicsDevice::CreateGeometry(const std::string& id, gfx::Geometry::CreateArgs args)
{
    auto geometry = std::make_shared<gfx::DeviceGeometry>(mDevice);
    geometry->SetFrameStamp(mFrameNumber);
    geometry->SetName(args.content_name);
    geometry->SetDataHash(args.content_hash);
    geometry->SetUsage(args.usage);
    geometry->SetBuffer(std::move(args.buffer));
    geometry->Upload();

    mGeoms[id] = geometry;
    return geometry;
}

gfx::InstancedDrawPtr GraphicsDevice::FindInstancedDraw(const std::string& id)
{
    auto it = mInstances.find(id);
    if (it == std::end(mInstances))
        return nullptr;
    return it->second;
}

gfx::InstancedDrawPtr GraphicsDevice::CreateInstancedDraw(const std::string& id, gfx::InstancedDraw::CreateArgs args)
{
    auto instance = std::make_shared<gfx::DeviceDrawInstanceBuffer>(mDevice);
    instance->SetFrameStamp(mFrameNumber);
    instance->SetContentName(args.content_name);
    instance->SetContentHash(args.content_hash);
    instance->SetUsage(args.usage);
    instance->SetBuffer(std::move(args.buffer));
    instance->Upload();

    mInstances[id] = instance;
    return instance;
}

gfx::Texture* GraphicsDevice::FindTexture(const std::string& name)
{
    auto it = mTextures.find(name);
    if (it == std::end(mTextures))
        return nullptr;
    return it->second.get();
}

gfx::Texture* GraphicsDevice::MakeTexture(const std::string& name)
{
    auto texture = std::make_unique<gfx::DeviceTexture>(mDevice, name);
    auto* ret = texture.get();
    mTextures[name] = std::move(texture);
    // technically not "use" but we need to track the number of frames
    // the texture has been unused for cleaning up purposes by computing
    // the delta between when the texture was last used and how many
    // frames the device has rendered. if we don't set this then a texture
    // that is not used will get immediately cleaned away when the current
    // device frame number exceeds the maximum number of idle frames.
    ret->SetFrameStamp(mFrameNumber);
    return ret;
}

gfx::Framebuffer* GraphicsDevice::FindFramebuffer(const std::string& name)
{
    auto it = mFBOs.find(name);
    if (it == std::end(mFBOs))
        return nullptr;
    return it->second.get();
}

gfx::Framebuffer* GraphicsDevice::MakeFramebuffer(const std::string& name)
{
    auto fbo = std::make_unique<gfx::DeviceFramebuffer>(mDevice, name);
    auto* ret = fbo.get();
    mFBOs[name] = std::move(fbo);
    return ret;
}

void GraphicsDevice::DeleteShaders()
{
    mShaders.clear();
}
void GraphicsDevice::DeletePrograms()
{
    mPrograms.clear();
}
void GraphicsDevice::DeleteGeometries()
{
    mGeoms.clear();
}
void GraphicsDevice::DeleteTextures()
{
    mTextures.clear();
}

void GraphicsDevice::DeleteFramebuffers()
{
    mFBOs.clear();
}
void GraphicsDevice::DeleteFramebuffer(const std::string& id)
{
    mFBOs.erase(id);
}
void GraphicsDevice::DeleteTexture(const std::string& id)
{
    auto it = mTextures.find(id);
    if (it == mTextures.end())
        return;

    auto* texture = static_cast<gfx::DeviceTexture*>(it->second.get());
    ASSERT(IsTextureFBOTarget(texture) == false);

    mTextures.erase(it);
}

GraphicsDevice::StateKey GraphicsDevice::PushState()
{
    mStateStack.push({});
    return static_cast<StateKey>(mStateStack.size());
}

void GraphicsDevice::PopState(StateKey key)
{
    ASSERT(key == static_cast<StateKey>(mStateStack.size()));
    mStateStack.pop();

    ASSERT(!mStateStack.empty());
    const auto& top = mStateStack.top();

    mDevice->SetViewportState(top.vs);
    mDevice->SetColorDepthStencilState(top.ds);
}

void GraphicsDevice::SetViewportState(const ViewportState& state) const
{
    ASSERT(!mStateStack.empty());
    auto& top = mStateStack.top();
    top.vs = state;
    mDevice->SetViewportState(state);
}
void GraphicsDevice::SetColorDepthStencilState(const ColorDepthStencilState& state) const
{
    ASSERT(!mStateStack.empty());
    auto& top = mStateStack.top();
    top.ds = state;
    mDevice->SetColorDepthStencilState(state);
}

void GraphicsDevice::Draw(const gfx::Program& program,
                          const gfx::ProgramState& program_state,
                          const gfx::GeometryDrawCommand& geometry, const Device::RasterState& state, gfx::Framebuffer* fbo)
{
    dev::Framebuffer framebuffer = SetupFBO(fbo);
    if (!framebuffer.IsValid())
        return;

    const auto* myprog = static_cast<const gfx::DeviceProgram*>(&program);
    const auto* mygeom = static_cast<const gfx::DeviceGeometry*>(geometry.GetGeometry());
    const auto* myinst = static_cast<const gfx::DeviceDrawInstanceBuffer*>(geometry.GetInstance());
    myprog->SetFrameStamp(mFrameNumber);
    mygeom->SetFrameStamp(mFrameNumber);
    if (myinst)
        myinst->SetFrameStamp(mFrameNumber);

    // this will also call glUseProgram
    TRACE_CALL("SetUniforms", myprog->ApplyUniformState(program_state));

    // this check is fine for any draw case because even when drawing with
    // indices there should be vertex data. if there isn't that means the
    // geometry is dummy, i.e. contains not vertex data.
    // todo: is this a BUG actually??
    if (mygeom->IsEmpty())
        return;

    TRACE_CALL("SetRasterState", mDevice->SetRasterState(state));

    // set program texture bindings
    const auto num_textures = program_state.GetSamplerCount();

    TRACE_ENTER(BindTextures);
    for (size_t i=0; i<num_textures; ++i)
    {
        const auto& sampler = program_state.GetSamplerSetting(i);

        const auto* texture = static_cast<const gfx::DeviceTexture*>(sampler.texture);
        // if the program sampler/texture setting is using a discontinuous set of
        // texture units we might end up with "holes" in the program texture state
        // and then the texture object is actually a nullptr.
        if (texture == nullptr)
            continue;

        texture->SetFrameStamp(mFrameNumber);

        auto texture_min_filter = texture->GetMinFilter();
        auto texture_mag_filter = texture->GetMagFilter();
        if (texture_min_filter == dev::TextureMinFilter::Default)
            texture_min_filter = static_cast<dev::TextureMinFilter>(mDefaultMinTextureFilter);
        if (texture_mag_filter == dev::TextureMagFilter::Default)
            texture_mag_filter = static_cast<dev::TextureMagFilter>(mDefaultMagTextureFilter);

        const auto texture_wrap_x = texture->GetWrapX();
        const auto texture_wrap_y = texture->GetWrapY();
        const auto texture_name = texture->GetName();
        const auto texture_unit = i;

        dev::GraphicsDevice::BindWarnings warnings;

        mDevice->BindTexture2D(texture->GetTexture(), myprog->GetProgram(), sampler.name,
                               texture_unit, texture_wrap_x, texture_wrap_y,
                               texture_min_filter, texture_mag_filter, &warnings);

        if (!texture->IsTransient() && texture->WarnOnce())
        {
            if (warnings.force_min_linear)
                WARN("Forcing GL_LINEAR on texture without mip maps. [texture='%1']", texture_name);
            if (warnings.force_clamp_x)
                WARN("Forcing GL_CLAMP_TO_EDGE on NPOT texture. [texture='%1']", texture_name);
            if (warnings.force_clamp_y)
                WARN("Forcing GL_CLAMP_TO_EDGE on NPOT texture. [texture='%1']", texture_name);
        }
    }
    TRACE_LEAVE(BindTextures);

    // start drawing geometry.

    constexpr auto DrawMax = std::numeric_limits<uint32_t>::max();

    TRACE_ENTER(DrawCommands);

    mDevice->BindFramebuffer(framebuffer);
    mDevice->BindVertexBuffer(mygeom->GetVertexBuffer(),
                              myprog->GetProgram(),
                              mygeom->GetVertexLayout());

    if (mygeom->UsesIndexBuffer())
    {
        mDevice->BindIndexBuffer(mygeom->GetIndexBuffer());

        const auto draw_command_count      = geometry.GetNumDrawCmds();
        const auto index_buffer_offset     = mygeom->GetIndexBufferByteOffset();
        const auto index_buffer_type       = mygeom->GetIndexBufferType();
        const auto index_buffer_item_count = mygeom->GetIndexCount();
        const auto index_item_byte_size    = mygeom->GetIndexByteSize();

        if (myinst)
        {
            mDevice->BindVertexBuffer(myinst->GetVertexBuffer(),
                                      myprog->GetProgram(),
                                      myinst->GetVertexLayout());

            const auto instance_count = myinst->GetInstanceCount();

            for (size_t i = 0; i < draw_command_count; ++i)
            {
                const auto& draw_cmd = geometry.GetDrawCmd(i);
                const auto draw_cmd_primitive_count =
                        draw_cmd.count == DrawMax ? index_buffer_item_count : draw_cmd.count;
                const auto draw_cmd_primitive_type = draw_cmd.type;
                const auto draw_cmd_offset = draw_cmd.offset;

                // draw elements instanced
                mDevice->Draw(draw_cmd_primitive_type, index_buffer_type, draw_cmd_primitive_count,
                              index_buffer_offset + draw_cmd_offset * index_item_byte_size, instance_count);
            }
        }
        else
        {
            for (size_t i=0; i<draw_command_count; ++i)
            {
                const auto& draw_cmd = geometry.GetDrawCmd(i);
                const auto draw_cmd_primitive_count =
                        draw_cmd.count == DrawMax ? index_buffer_item_count : draw_cmd.count;
                const auto draw_cmd_primitive_type  = draw_cmd.type;
                const auto draw_cmd_offset = draw_cmd.offset;

                // draw elements
                mDevice->Draw(draw_cmd_primitive_type, index_buffer_type, draw_cmd_primitive_count,
                              index_buffer_offset + draw_cmd_offset * index_item_byte_size);
            }
        }
    }
    else
    {
        const auto draw_command_count = geometry.GetNumDrawCmds();
        const auto vertex_buffer_item_count = mygeom->GetVertexCount();

        if (myinst)
        {
            mDevice->BindVertexBuffer(myinst->GetVertexBuffer(),
                                      myprog->GetProgram(),
                                      myinst->GetVertexLayout());

            const auto instance_count = myinst->GetInstanceCount();

            for (size_t i = 0; i < draw_command_count; ++i)
            {
                const auto& draw_cmd = geometry.GetDrawCmd(i);
                const auto draw_cmd_primitive_count =
                        draw_cmd.count == DrawMax ? vertex_buffer_item_count : draw_cmd.count;
                const auto draw_cmd_primitive_type = draw_cmd.type;
                const auto draw_cmd_offset = draw_cmd.offset;

                // draw arrays instanced
                mDevice->Draw(draw_cmd_primitive_type, draw_cmd_offset, draw_cmd_primitive_count, instance_count);

            }
        }
        else
        {
            for (size_t i=0; i<draw_command_count; ++i)
            {
                const auto& draw_cmd = geometry.GetDrawCmd(i);
                const auto draw_cmd_primitive_count =
                        draw_cmd.count == DrawMax ? vertex_buffer_item_count : draw_cmd.count;
                const auto draw_cmd_primitive_type  = draw_cmd.type;
                const auto draw_cmd_offset = draw_cmd.offset;

                // draw arrays
                mDevice->Draw(draw_cmd_primitive_type, draw_cmd_offset, draw_cmd_primitive_count);
            }
        }
    }

    TRACE_LEAVE(DrawCommands);
}

void GraphicsDevice::CleanGarbage(size_t max_num_idle_frames, unsigned flags)
{
    if (flags &  GCFlags::FBOs)
    {
        const auto did_have_fbos = mFBOs.empty();

        for (auto it = mFBOs.begin(); it != mFBOs.end(); )
        {
            auto* device_framebuffer = static_cast<gfx::DeviceFramebuffer*>(it->second.get());

            const auto last_used_frame_number = device_framebuffer->GetFrameStamp();
            if (mFrameNumber - last_used_frame_number >= max_num_idle_frames)
                it = mFBOs.erase(it);
            else ++it;
        }
        if (did_have_fbos && mFBOs.empty())
            DEBUG("All GPU frame-buffers were deleted.");
    }

    if (flags & GCFlags::Programs)
    {
        const auto did_have_programs = !mPrograms.empty();

        for (auto it = mPrograms.begin(); it != mPrograms.end();)
        {
            auto* device_program = static_cast<gfx::DeviceProgram*>(it->second.get());

            const auto last_used_frame_number = device_program->GetFrameStamp();
            if (mFrameNumber - last_used_frame_number >= max_num_idle_frames)
                it = mPrograms.erase(it);
            else ++it;
        }
        if (did_have_programs && mPrograms.empty())
            INFO("All GPU program objects were deleted.");
    }

    if (flags & GCFlags::Textures)
    {
        // use texture grouping to clean up (or not) groups of
        // textures and not just individual textures.
        // this is done because a sprite cycle could have any number
        // of textures and not all of them are used all the time.
        // yet all of them will be used and needed  to render the whole cycle,
        // and we should not clean away textures mid-cycle.
        std::unordered_map<std::string, size_t> group_last_use;
        for (auto& pair : mTextures)
        {
            const auto& texture = pair.second;
            const auto* impl  = static_cast<gfx::DeviceTexture*>(texture.get());
            const auto& group = impl->GetGroup();
            if (group.empty())
                continue;
            const auto last_used  = impl->GetFrameStamp();
            group_last_use[group] = std::max(group_last_use[group], last_used);
        }

        const bool did_have_textures = !mTextures.empty();

        for (auto it = mTextures.begin(); it != mTextures.end();)
        {
            const auto* impl = static_cast<gfx::DeviceTexture*>(it->second.get());
            const auto& group = impl->GetGroup();
            const auto group_last_used = group_last_use[group];
            const auto this_last_used  = impl->GetFrameStamp();
            const auto last_used = std::max(group_last_used, this_last_used);
            const auto is_expired = mFrameNumber - last_used >= max_num_idle_frames;

            if (is_expired && impl->GarbageCollect() && !IsTextureFBOTarget(impl))
            {

                // delete the texture
                it = mTextures.erase(it);
            } else ++it;
        }

        if (did_have_textures && mTextures.empty())
            INFO("All GPU texture objects were deleted.");
    }

    if (flags & GCFlags::Geometries)
    {
        const auto did_have_geometries = !mGeoms.empty();
        const auto did_have_instances = !mInstances.empty();

        for (auto it = mGeoms.begin(); it != mGeoms.end();)
        {
            auto* impl = static_cast<gfx::DeviceGeometry*>(it->second.get());
            const auto last_used_frame_number = impl->GetFrameStamp();
            if (mFrameNumber - last_used_frame_number >= max_num_idle_frames)
                it = mGeoms.erase(it);
            else ++it;
        }

        for (auto it = mInstances.begin(); it != mInstances.end(); )
        {
            auto* impl = static_cast<gfx::DeviceDrawInstanceBuffer*>(it->second.get());
            const auto last_used_frame_number = impl->GetFrameStamp();
            if (mFrameNumber - last_used_frame_number >= max_num_idle_frames)
                it = mInstances.erase(it);
            else ++it;
        }

        if (did_have_geometries && mGeoms.empty())
            INFO("All GPU geometries were deleted.");
        if (did_have_instances && mInstances.empty())
            INFO("All GPU geometry instances were deleted.");
    }
}

void GraphicsDevice::BeginFrame()
{
    mDevice->BeginFrame();
}

void GraphicsDevice::EndFrame(bool display)
{
    mDevice->EndFrame(display);

    mFrameNumber++;

    const auto max_num_idle_frames = 120;

    // clean up expired transient textures.
    for (auto it = mTextures.begin(); it != mTextures.end();)
    {
        auto* impl = static_cast<gfx::DeviceTexture*>(it->second.get());
        const auto last_used_frame_number = impl->GetFrameStamp();
        const auto is_expired = mFrameNumber - last_used_frame_number >= max_num_idle_frames;

        if (is_expired && impl->IsTransient() && !IsTextureFBOTarget(impl))
        {
            size_t unit = 0;

            // delete the texture
            it = mTextures.erase(it);
        } else ++it;
    }
}


gfx::Bitmap<gfx::Pixel_RGBA> GraphicsDevice::ReadColorBuffer(unsigned width, unsigned height, gfx::Framebuffer* fbo) const
{
    gfx::Bitmap<gfx::Pixel_RGBA> bmp;
    dev::Framebuffer framebuffer = SetupFBO(fbo);
    if (!framebuffer.IsValid())
        return bmp;

    bmp.Resize(width, height);
    mDevice->ReadColor(width, height, framebuffer, bmp.GetDataPtr());

    // by default the scan row order is reversed to what we expect.
    bmp.FlipHorizontally();
    return bmp;
}

gfx::Bitmap<gfx::Pixel_RGBA> GraphicsDevice::ReadColorBuffer(unsigned x, unsigned y,
                                                             unsigned width, unsigned height, gfx::Framebuffer* fbo) const
{
    gfx::Bitmap<gfx::Pixel_RGBA> bmp;
    dev::Framebuffer framebuffer = SetupFBO(fbo);
    if (!framebuffer.IsValid())
        return bmp;

    bmp.Resize(width, height);
    mDevice->ReadColor(x, y, width, height, framebuffer, bmp.GetDataPtr());

    // by default the scan row order is reversed to what we expect.
    bmp.FlipHorizontally();
    return bmp;
}

void GraphicsDevice::GetResourceStats(ResourceStats* stats) const
{
    mDevice->GetResourceStats(stats);
}
void GraphicsDevice::GetDeviceCaps(DeviceCaps* caps) const
{
    mDevice->GetDeviceCaps(caps);
}

dev::Framebuffer GraphicsDevice::SetupFBO(gfx::Framebuffer* framebuffer) const
{
    dev::Framebuffer invalid_handle;
    if (framebuffer)
    {
        auto* device_framebuffer = static_cast<gfx::DeviceFramebuffer*>(framebuffer);
        if (device_framebuffer->IsReady())
        {
            if (!device_framebuffer->Complete())
                return invalid_handle;
        }
        else
        {
            if (!device_framebuffer->Create())
                return invalid_handle;
            if (!device_framebuffer->Complete())
                return invalid_handle;
        }
        device_framebuffer->SetFrameStamp(mFrameNumber);
        return device_framebuffer->GetFramebuffer();
    }
    return mDevice->GetDefaultFramebuffer();
}

bool GraphicsDevice::IsTextureFBOTarget(const gfx::Texture* texture) const
{
    for (const auto& pair: mFBOs)
    {
        const auto* fbo = static_cast<const gfx::DeviceFramebuffer*>(pair.second.get());
        for (unsigned i = 0; i < fbo->GetClientTextureCount(); ++i)
        {
            const auto* client_texture = fbo->GetClientTexture(i);
            if (client_texture == nullptr)
                continue;
            if (client_texture == texture)
                return true;
        }
    }
    return false;
}

} // namespace

namespace gfx {
std::shared_ptr<Device> CreateDevice(std::shared_ptr<dev::GraphicsDevice> device)
{
    return std::make_shared<GraphicsDevice>(device);
}
std::shared_ptr<Device> CreateDevice(dev::GraphicsDevice* device)
{
    return std::make_shared<GraphicsDevice>(device);
}

} // namespace
