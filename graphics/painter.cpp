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

#include "config.h"

#include "warnpush.h"
#  include <glm/gtc/type_ptr.hpp>
#  include <glm/gtc/matrix_transform.hpp>
#include "warnpop.h"

#include <string>
#include <map>

#include "base/logging.h"
#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/types.h"

namespace gfx
{

class StandardPainter : public Painter
{
public:
    StandardPainter(std::shared_ptr<Device> device)
      : mDeviceInst(device)
      , mDevice(device.get())
    {}
    StandardPainter(Device* device)
      : mDevice(device)
    {}
    virtual void SetEditingMode(bool on_off) override
    {
        mEditingMode = on_off;
    }
    virtual void SetPixelRatio(const glm::vec2& ratio) override
    {
        mPixelRatio = ratio;
    }
    virtual void SetSurfaceSize(const USize& size) override
    {
        mSurfacesize = ISize(size);
    }
    virtual void SetViewport(const IRect& viewport) override
    {
        mViewport = viewport;
    }
    virtual void SetScissor(const IRect& scissor) override
    {
        mScissor = scissor;
    }
    virtual void ClearScissor() override
    {
        mScissor = IRect(0, 0, 0, 0);
    }
    virtual void SetProjectionMatrix(const glm::mat4& proj) override
    {
        mProjection = proj;
    }
    virtual void SetViewMatrix(const glm::mat4& view) override
    {
        mViewMatrix = view;
    }
    virtual void Clear(const Color4f& color) override
    {
        mDevice->ClearColor(color);
    }

    virtual void Draw(const Drawable& drawable, const Transform& transform, const Material& material) override
    {
        // create simple orthographic projection matrix.
        // 0,0 is the window top left, x grows left and y grows down
        const auto& kModelMatrix = transform.GetAsMatrix();
        const auto style = drawable.GetStyle();

        Drawable::Environment drawable_env;
        drawable_env.editing_mode = mEditingMode;
        drawable_env.pixel_ratio  = mPixelRatio;
        drawable_env.proj_matrix  = &mProjection;
        drawable_env.view_matrix  = &mViewMatrix;
        drawable_env.model_matrix = &kModelMatrix;
        Geometry* geom = drawable.Upload(drawable_env, *mDevice);
        if (geom == nullptr)
            return;

        Material::Environment material_env;
        material_env.editing_mode  = mEditingMode;
        material_env.render_points = style == Drawable::Style::Points;
        Program* prog = GetProgram(drawable, material, drawable_env, material_env);
        if (prog == nullptr)
            return;

        Material::RasterState material_raster_state;
        material.ApplyDynamicState(material_env, *mDevice, *prog, material_raster_state);

        Drawable::RasterState drawable_raster_state;
        drawable.ApplyDynamicState(drawable_env, *prog, drawable_raster_state);

        Device::State state;
        state.viewport     = MapToDevice(mViewport);
        state.scissor      = MapToDevice(mScissor);
        state.bWriteColor  = true;
        state.stencil_func = Device::State::StencilFunc::Disabled;
        state.blending     = material_raster_state.blending;
        state.premulalpha  = material_raster_state.premultiplied_alpha;
        state.line_width   = drawable_raster_state.line_width;
        state.culling      = drawable_raster_state.culling;
        mDevice->Draw(*prog, *geom, state);
    }

    virtual void Draw(const Drawable& drawShape, const Transform& drawTransform,
                      const Drawable& maskShape, const Transform& maskTransform,
                      const Material& material) override
    {
        const auto& dm = drawTransform.GetAsMatrix();
        const auto& mm = maskTransform.GetAsMatrix();
        DrawShape draw;
        draw.drawable = &drawShape;
        draw.material = &material;
        draw.transform = &dm;
        std::vector<DrawShape> drawlist;
        drawlist.push_back(draw);

        MaskShape mask;
        mask.drawable  = &maskShape;
        mask.transform = &mm;
        std::vector<MaskShape> masklist;
        masklist.push_back(mask);
        Draw(drawlist, masklist);
    }

    virtual void Draw(const std::vector<DrawShape>& draw_list, const std::vector<MaskShape>& mask_list) override
    {
        mDevice->ClearStencil(1);

        Device::State state;
        state.viewport      = MapToDevice(mViewport);
        state.scissor       = MapToDevice(mScissor);
        state.stencil_func  = Device::State::StencilFunc::PassAlways;
        state.stencil_dpass = Device::State::StencilOp::WriteRef;
        state.stencil_ref   = 0;
        state.bWriteColor   = false;
        state.blending      = Device::State::BlendOp::None;

        MaterialClassInst mask_material(CreateMaterialFromColor(gfx::Color::White));
        // do the masking pass
        for (const auto& mask : mask_list)
        {
            Drawable::Environment drawable_env;
            drawable_env.editing_mode = mEditingMode;
            drawable_env.pixel_ratio  = mPixelRatio;
            drawable_env.view_matrix  = &mViewMatrix;
            drawable_env.proj_matrix  = &mProjection;
            drawable_env.model_matrix = mask.transform;
            Geometry* geom = mask.drawable->Upload(drawable_env, *mDevice);
            if (geom == nullptr)
                continue;

            Material::Environment material_env;
            material_env.editing_mode  = mEditingMode;
            material_env.render_points = mask.drawable->GetStyle() == Drawable::Style::Points;
            Program* prog = GetProgram(*mask.drawable, mask_material, drawable_env,material_env);
            if (prog == nullptr)
                continue;

            Material::RasterState material_raster_state;
            mask_material.ApplyDynamicState(material_env, *mDevice, *prog, material_raster_state);

            Drawable::RasterState drawable_raster_state;
            mask.drawable->ApplyDynamicState(drawable_env, *prog, drawable_raster_state);
            state.culling     = drawable_raster_state.culling;
            state.line_width  = drawable_raster_state.line_width;
            state.premulalpha = false;
            mDevice->Draw(*prog, *geom, state);
        }

        state.stencil_func  = Device::State::StencilFunc::RefIsEqual;
        state.stencil_dpass = Device::State::StencilOp::WriteRef;
        state.stencil_ref   = 1;
        state.bWriteColor   = true;

        // do the render pass.
        for (const auto& draw : draw_list)
        {
            Drawable::Environment drawable_env;
            drawable_env.editing_mode = mEditingMode;
            drawable_env.pixel_ratio  = mPixelRatio;
            drawable_env.view_matrix  = &mViewMatrix;
            drawable_env.proj_matrix  = &mProjection;
            drawable_env.model_matrix = draw.transform;
            Geometry* geom = draw.drawable->Upload(drawable_env, *mDevice);
            if (geom == nullptr)
                continue;

            Material::Environment material_env;
            material_env.editing_mode  = mEditingMode;
            material_env.render_points = draw.drawable->GetStyle() == Drawable::Style::Points;
            Program* prog = GetProgram(*draw.drawable, *draw.material, drawable_env, material_env);
            if (prog == nullptr)
                continue;

            Material::RasterState material_raster_state;
            draw.material->ApplyDynamicState(material_env, *mDevice, *prog, material_raster_state);
            state.blending    = material_raster_state.blending;
            state.premulalpha = material_raster_state.premultiplied_alpha;

            Drawable::RasterState drawable_raster_state;
            draw.drawable->ApplyDynamicState(drawable_env, *prog, drawable_raster_state);
            state.line_width = drawable_raster_state.line_width;
            state.culling    = drawable_raster_state.culling;
            mDevice->Draw(*prog, *geom, state);
        }
    }

    virtual void Draw(const std::vector<DrawShape>& shapes) override
    {
        for (const auto& draw : shapes)
        {
            Drawable::Environment drawable_env;
            drawable_env.editing_mode = mEditingMode;
            drawable_env.pixel_ratio  = mPixelRatio;
            drawable_env.proj_matrix  = &mProjection;
            drawable_env.view_matrix  = &mViewMatrix;
            drawable_env.model_matrix = draw.transform;
            Geometry* geom = draw.drawable->Upload(drawable_env, *mDevice);
            if (geom == nullptr)
                continue;

            Material::Environment material_env;
            material_env.editing_mode  = mEditingMode;
            material_env.render_points = draw.drawable->GetStyle() == Drawable::Style::Points;
            Program* program = GetProgram(*draw.drawable, *draw.material, drawable_env, material_env);
            if (program == nullptr)
                continue;

            Material::RasterState material_raster_state;
            draw.material->ApplyDynamicState(material_env, *mDevice, *program, material_raster_state);

            Drawable::RasterState drawable_raster_state;
            draw.drawable->ApplyDynamicState(drawable_env, *program, drawable_raster_state);

            Device::State state;
            state.viewport     = MapToDevice(mViewport);
            state.scissor      = MapToDevice(mScissor);
            state.bWriteColor  = true;
            state.stencil_func = Device::State::StencilFunc::Disabled;
            state.blending     = material_raster_state.blending;
            state.premulalpha  = material_raster_state.premultiplied_alpha;
            state.culling      = drawable_raster_state.culling;
            state.line_width   = drawable_raster_state.line_width;
            mDevice->Draw(*program, *geom, state);
        }
    }

private:
    Program* GetProgram(const Drawable& drawable,
                        const Material& material,
                        const Drawable::Environment& drawable_environment,
                        const Material::Environment& material_environment)
    {
        const std::string& name = drawable.GetProgramId() + "/" + material.GetProgramId();
        Program* prog = mDevice->FindProgram(name);
        if (!prog)
        {
            Shader* drawable_shader = drawable.GetShader(*mDevice);
            if (!drawable_shader || !drawable_shader->IsValid())
                return nullptr;
            Shader* material_shader = material.GetShader(material_environment, *mDevice);
            if (!material_shader || !material_shader->IsValid())
                return nullptr;

            std::vector<const Shader*> shaders;
            shaders.push_back(drawable_shader);
            shaders.push_back(material_shader);
            prog = mDevice->MakeProgram(name);
            prog->Build(shaders);
            if (prog->IsValid())
            {
                material.ApplyStaticState(*mDevice, *prog);
            }
        }
        if (prog->IsValid())
            return prog;
        return nullptr;
    }

    IRect MapToDevice(const IRect& rect) const
    {
        if (rect.IsEmpty())
            return rect;
        // map from window coordinates (top left origin) to device
        // coords (bottom left origin)
        const auto bottom = rect.GetY() + rect.GetHeight();
        const auto x = rect.GetX();
        const auto y = mSurfacesize.GetHeight() - bottom;
        return IRect(x, y, rect.GetWidth(), rect.GetHeight());
    }
private:
    std::shared_ptr<Device> mDeviceInst;
    Device* mDevice = nullptr;
private:
    bool mEditingMode = false;
    // Expected Size of the rendering surface.
    ISize mSurfacesize;
    // the viewport setting for mapping the NDC coordinates
    // into some region of the rendering surface.
    IRect mViewport;
    // the current scissor setting to be applied
    // on the rendering surface.
    IRect mScissor;
   // the ratio of rendering surface pixels to game units.
    glm::vec2 mPixelRatio = {1.0f, 1.0f};
    // current (orthographic) projection matrix.
    glm::mat4 mProjection;
    // current additional view matrix that gets multiplied
    // with the draw transforms. convenient for cases when
    // when everything that is to be drawn needs to get
    // transformed in some additional way.
    glm::mat4 mViewMatrix {1.0f};
};

// static
glm::mat4 Painter::MakeOrthographicProjection(const FRect& rect)
{
    const auto left   = rect.GetX();
    const auto right  = rect.GetWidth() + left;
    const auto top    = rect.GetY();
    const auto bottom = rect.GetHeight() + top;
    return glm::ortho(left , right , bottom , top);
}

// static
glm::mat4 Painter::MakeOrthographicProjection(float left, float top, float width, float height)
{
    return glm::ortho(left, left + width, top + height, top);
}
glm::mat4 Painter::MakeOrthographicProjection(float width, float height)
{
    return glm::ortho(0.0f, width, height, 0.0f);
}

// static
std::unique_ptr<Painter> Painter::Create(std::shared_ptr<Device> device)
{
    return std::make_unique<StandardPainter>(device);
}
// static
std::unique_ptr<Painter> Painter::Create(Device* device)
{
    return std::make_unique<StandardPainter>(device);
}

void Painter::SetOrthographicProjection(const FRect& view)
{
    SetProjectionMatrix(MakeOrthographicProjection(view));
}
void Painter::SetOrthographicProjection(float left, float top, float width, float height)
{
    const FRect view(left, top, width, height);
    SetProjectionMatrix(MakeOrthographicProjection(view));
}
// Set the logical viewport for "top left" origin based drawing.
void Painter::SetOrthographicProjection(float width, float height)
{
    const FRect view(0.0f, 0.0f, width, height);
    SetProjectionMatrix(MakeOrthographicProjection(view));
}

void Painter::SetViewport(int x, int y, unsigned int width, unsigned int height)
{
    SetViewport(IRect(x, y, width, height));
}
void Painter::SetScissor(int x, int y, unsigned int width, unsigned int height)
{
    SetScissor(IRect(x, y, width, height));
}
void Painter::SetSurfaceSize(unsigned width, unsigned height)
{
    SetSurfaceSize(USize(width, height));
}

} // namespace
