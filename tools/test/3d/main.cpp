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

#include "warnpush.h"
#  include <glm/mat4x4.hpp>
#  include <glm/gtc/matrix_transform.hpp>
#  include <glm/gtx/euler_angles.hpp>
#  include <glm/gtx/intersect.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <cmath>

#include "base/logging.h"
#include "base/utility.h"
#include "base/json.h"
#include "graphics/algo.h"
#include "graphics/framebuffer.h"
#include "graphics/drawing.h"
#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/shaderpass.h"
#include "graphics/utility.h"
#include "graphics/transform.h"
#include "graphics/device.h"
#include "device/device.h"
#include "engine/ui.h"
#include "engine/data.h"
#include "engine/camera.h"
#include "uikit/state.h"
#include "uikit/widget.h"
#include "uikit/window.h"
#include "wdk/opengl/config.h"
#include "wdk/opengl/context.h"
#include "wdk/opengl/surface.h"
#include "wdk/window.h"
#include "wdk/events.h"
#include "wdk/system.h"

namespace  engine {
    class Camera
    {
    public:
        Camera() = default;
        explicit Camera(engine::GameView perspective)
        {
            SetFromPerspective(perspective);
            Update();
        }

        // Set camera position and view direction vector from a predefined
        // perspective setting. However keep in mind that setting the camera
        // perspective is not by itself enough to create the final rendering.
        // For example with dimetric rendering the projection matrix also needs
        // to be set to a orthographic projection.
        inline void SetFromPerspective(engine::GameView perspective) noexcept
        {
            if (perspective == engine::GameView::Dimetric)
            {
                // jump to a position for dimetric projection.
                // 45 degrees around the UP axis (yaw) and 30 degrees down (pitch)
                SetPosition({0.0f, 0.0f, 0.0f});
                SetYaw(-90.0f - 45.0f);
                SetPitch(-30.0f);
            }
            else if (perspective == engine::GameView::AxisAligned)
            {
                SetPosition({0.0f, 0.0f, 0.0f});
                SetDirection({0.0f, 0.0f, -1.0f});
            }
        }

        // Rotate the camera around the Y axis (vertical, yaw) and around the
        // X (horizontal, pitch, tilt) axis. The order of transformations is
        // first yaw then pitch.
        // Negative pitch = look down, positive pitch = look up
        // Negative yaw = look left, positive yaw = look right
        inline void SetRotation(float yaw, float pitch) noexcept
        {
            mYaw = yaw;
            mPitch = pitch;
        }
        // Set the current camera position in world coordinates. This is
        // the vantage point from which the camera looks to the specified
        // camera direction (forward) vector.
        inline void SetPosition(const glm::vec3& position) noexcept
        { mPosition = position; }
        // Set the direction the camera is looking at. Direction should
        // a normalized, i.e. unit length direction vector.
        inline void SetDirection(const glm::vec3& dir) noexcept
        {
            // atan is the tangent value of 2 arguments y,x, (range (-pi pi] ) so using z vector as the "y"
            // and x as the x means the angle from our z-axis towards the x axis (looking down on the Y axis)
            // which is the same as rotation around the up axis (aka yaw)
            mYaw   = glm::degrees(glm::atan(dir.z, dir.x));
            mPitch = glm::degrees(glm::asin(dir.y));
        }
        inline void LookAt(const glm::vec3& pos) noexcept
        {
            SetDirection(glm::normalize(pos - mPosition));
        }
        // Set the camera yaw (in degrees), i.e. rotation around the vertical axis.
        inline void SetYaw(float yaw) noexcept
        { mYaw = yaw; }
        // Set the camera pitch (aka tilt) (in degrees), i.e. rotation around the horizontal axis.
        inline void SetPitch(float pitch) noexcept
        { mPitch = pitch; }
        // Translate the camera by accumulating a change in position by some delta.
        inline void Translate(const glm::vec3& delta) noexcept
        { mPosition += delta; }
        // Translate the camera by accumulating a change in position by some delta values on each axis.
        inline void Translate(float dx, float dy, float dz) noexcept
        { mPosition += glm::vec3(dx, dy, dz); }
        inline void SetX(float x) noexcept
        { mPosition.x = x; }
        inline void SetY(float y) noexcept
        { mPosition.y = y; }
        inline void SetZ(float z) noexcept
        { mPosition.z = z; }
        // Change the camera yaw in degrees by some delta value.
        inline void Yaw(float d) noexcept
        { mYaw += d; }
        // Change the camera pitch in degrees by some delta value.
        inline void Pitch(float d) noexcept
        { mPitch += d; }

        inline float GetYaw() const noexcept
        { return mYaw; }
        // Get the the current camera pitch in degrees. Positive value indicates
        // that the camera is looking upwards (towards the sky) and negative value
        // indicates that the camera is looking downwards (towards the floor)
        inline float GetPitch() const noexcept
        { return mPitch; }
        //
        inline glm::vec3 GetViewVector() const noexcept
        { return mViewDir; }
        inline glm::vec3 GetRightVector() const noexcept
        { return mRight; }
        inline glm::vec3 GetPosition() const noexcept
        { return mPosition; }

        inline glm::mat4 GetViewMatrix() const noexcept
        {
            return glm::lookAt(mPosition, mPosition + mViewDir, glm::vec3{0.0f, 1.0f, 0.0f});
        }

        // Call this after adjusting any camera parameters in order to
        // recompute the view direction vector and the camera right vector.
        void Update() noexcept
        {
            constexpr const static glm::vec3 WorldUp = {0.0f, 1.0f, 0.0f};
            mViewDir.x = std::cos(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));
            mViewDir.y = std::sin(glm::radians(mPitch));
            mViewDir.z = std::sin(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));
            mViewDir = glm::normalize(mViewDir);
            mRight = glm::normalize(glm::cross(mViewDir, WorldUp));
        }

    private:
        // Camera's local (relative to it's base node) translation.
        glm::vec3 mPosition = {0.0f, 0.0f, 0.0f};
        // Camera's local right vector.
        glm::vec3 mRight    = {0.0f, 0.0f, 0.0f};
        // Camera's local view direction vector. Remember this
        // actually the inverse of the "objects" forward vector.
        glm::vec3 mViewDir  = {0.0f, 0.0f, 0.0f};
        // Camera rotation around the vertical axis.
        float mYaw   = 0.0f;
        // Aka tilt, camera rotation around the horizontal axis.
        float mPitch = 0.0f;
    };
} // engine

enum class FocusLayer {
    Scene, UI
};

enum class GridDensity {
    Grid10x10   = 10,
    Grid20x20   = 20,
    Grid50x50   = 50,
    Grid100x100 = 100
};


struct State{
    FocusLayer focus = FocusLayer::Scene;
    engine::Camera camera;
    float zoom = 1.0f;
    glm::vec2 window = {1500, 1500};
    glm::vec2 scale  = {1.0f, 1.0f};
    glm::vec3 cube_rotation = {0.0f, 0.0f, 0.0f};
    GridDensity grid = GridDensity::Grid100x100;

    std::optional<engine::GameView> perspective;
    glm::mat4 projection;

    glm::vec3 mouse_pos = {0.0f, 0.0f, 0.0f};
    glm::vec3 camera_pos = {0.0f, 0.0f, 0.0f};
    bool tracking = false;

    engine::UIStyle ui_style;
    engine::UIPainter ui_painter;
    uik::Window ui_window;
    uik::TransientState ui_state;

    gfx::Texture* depth;
    gfx::Device* device;

    float dt = 0.0;
};

glm::mat4 GetViewMatrix(const State& state, bool translate_camera = true, bool apply_zoom = true)
{
    if (!state.perspective.has_value())
    {
        glm::mat4 mat;
        mat = state.camera.GetViewMatrix();
        mat = glm::translate(mat, state.camera_pos*-1.0f);
        mat = glm::scale(mat, glm::vec3{0.01f, 0.01f, 0.01f});
        return mat;
    }


    // remember that if you use operator *= as in a *= b; it's the same as
    // a = a * b;
    // this means that multiple statements such as
    // mat *= foo;
    // mat *= bar;
    // will be the same as mat = foo * bar. This means that when this matrix
    // is used to transform the vertices the bar operation will take place
    // first, then followed by foo.

    glm::mat4 mat;
    mat = state.camera.GetViewMatrix();
    if (apply_zoom)
        mat = glm::scale(mat, glm::vec3{state.zoom, state.zoom, state.zoom});

    if (translate_camera)
        mat = glm::translate(mat, state.camera_pos*-1.0f); // invert camera pos

    if (const auto* p = base::GetOpt(state.perspective))
    {
        if (*p == engine::GameView::Dimetric)
            mat = glm::rotate(mat, glm::radians(90.0f), glm::vec3{1.0f, 0.0f, 0.0f});
    }
    return mat;
}

// (not yet working properly!)
// Try to reconstruct a world space position from a window coordinate + depth value.
glm::vec4 MapWindowCoordinate(const glm::mat4& projection,
                              const glm::mat4& view,
                              const glm::vec2& window_coord,
                              const glm::vec2& window_size,
                              float w,
                              float depth_value = -1.0f)
{
    // normalize the window coordinate. remember to flip the Y axis.
    glm::vec2 norm;
    norm.x = window_coord.x / (window_size.x*0.5) - 1.0f;
    norm.y = 1.0f - (window_coord.y / (window_size.y*0.5));

    // remember this is in the NDC, so on z axis -1.0 is less depth
    // i.e. closer to the viewer and 1.0 is more depth, farther away
    const glm::vec4 ndc = {norm.x, norm.y, depth_value, 1.0f};
    // transform into clip space. remember that when the clip space
    // coordinate is transformed into NDC (by OpenGL) the clip space
    // vectors are divided by the w component to yield normalized
    // coordinates.
    const glm::vec4 clip = ndc * w;

    const auto& view_to_clip = projection;
    const auto& clip_to_view = glm::inverse(view_to_clip);
    const auto& world_to_view = view;
    const auto& view_to_world = glm::inverse(world_to_view);

    return view_to_world * clip_to_view * clip;
}

glm::vec3 MapToWorldPlane(const glm::mat4& projection,
                          const glm::mat4& view,
                          const glm::vec2& window_coord,
                          const glm::vec2& window_size)
{
    constexpr const auto plane_origin_world = glm::vec4 {0.0f, 0.0f, 0.0f, 1.0f};
    constexpr const auto plane_normal_world = glm::vec4 {0.0f, 0.0f, 1.0f, 0.0f};
    const auto plane_origin_view = view * plane_origin_world;
    const auto plane_normal_view = glm::normalize(view * plane_normal_world);

    // normalize the window coordinate. remember to flip the Y axis.
    glm::vec2 norm;
    norm.x = window_coord.x / (window_size.x*0.5) - 1.0f;
    norm.y = 1.0f - (window_coord.y / (window_size.y*0.5));

    constexpr auto depth_value = -1.0f; // maps to view volume near plane
    // remember this is in the NDC, so on z axis -1.0 is less depth
    // i.e. closer to the viewer and 1.0 is more depth, farther away
    const auto ndc = glm::vec4 {norm.x, norm.y, depth_value, 1.0f};
    // transform into clip space. remember that when the clip space
    // coordinate is transformed into NDC (by OpenGL) the clip space
    // vectors are divided by the w component to yield normalized
    // coordinates.
    constexpr auto w = 1.0f;
    const auto clip = ndc * w;

    const auto& view_to_clip = projection;
    const auto& clip_to_view = glm::inverse(view_to_clip);

    // original window coordinate in view space on near plane.
    const auto& view_pos = clip_to_view * clip;

    const auto& ray_origin = view_pos;
    // do a ray cast from the view_pos towards the depth
    // i.e. the ray is collinear with the -z vector
    constexpr const auto ray_direction = glm::vec4 {0.0f, 0.0f, -1.0f, 0.0f};

    float intersection_distance = 0.0f;
    glm::intersectRayPlane(ray_origin,
                           ray_direction,
                           plane_origin_view,
                           plane_normal_view,
                           intersection_distance);

    const auto& view_to_world = glm::inverse(view);

    const auto& intersection_point_view  = ray_origin + ray_direction * intersection_distance;
    const auto& intersection_point_world = view_to_world * intersection_point_view;
    return intersection_point_world;
}

class RenderPass
{
public:
    virtual ~RenderPass() = default;
    virtual void Draw(const glm::mat4& model_to_world,
                      const gfx::Drawable& drawable,
                      const gfx::Material& material,
                      gfx::Painter& painter,
                      bool depth_test = true) = 0;
private:
};

class ColorPass : public RenderPass
{
public:
    ColorPass(bool depth_test)
      : mDepthTest(depth_test)
    {}

    virtual void Draw(const glm::mat4& model_to_world,
                      const gfx::Drawable& drawable,
                      const gfx::Material& material,
                      gfx::Painter& painter,
                      bool depth_test) override
    {
        gfx::Painter::DrawState state;
        state.write_color  = true;
        state.depth_test   = gfx::Painter::DepthTest::Disabled;
        state.stencil_func = gfx::Painter::StencilFunc::Disabled;
        if (mDepthTest && depth_test)
            state.depth_test = gfx::Painter::DepthTest::LessOrEQual;

        painter.Draw(drawable, model_to_world, material, state, gfx::detail::GenericShaderProgram());
    }
private:
    const bool mDepthTest;
};

class DepthTextureShader : public gfx::ShaderProgram
{
public:
    virtual std::string GetShaderId(const gfx::Material& material, const gfx::Material::Environment& env) const override
    {
        return "DepthToColor";
    }
    virtual std::string GetShader(const gfx::Material& material, const gfx::Material::Environment& env, const gfx::Device& device) const override
    {
        // this shader maps the interpolated fragment depth (the .z component)
        // to a color value linearly. (important to keep this in mind when using
        // the output values, if rendering to a texture if the sRGB encoding happens
        // then the depth values are no longer linear!)
        //
        // Remember that in the OpenGL pipeline by default the NDC values (-1.0 to 1.0 on all axis)
        // are mapped to depth values so that -1.0 is least depth and 1.0 is maximum depth.
        // (OpenGL and ES3 have glDepthRange for modifying this mapping.)
        constexpr const auto* depth_to_color = R"(
#version 100
precision highp float;

void main() {
   gl_FragColor.rgb = vec3(gl_FragCoord.z);
   gl_FragColor.a = 1.0;
}
)";
        return depth_to_color;
    }
    virtual std::string GetName() const override
    { return "DepthTextureShader"; }
private:

};

class DepthTexturePass : public RenderPass
{
public:
    virtual void Draw(const glm::mat4& model_to_world,
                      const gfx::Drawable& drawable,
                      const gfx::Material& material,
                      gfx::Painter& painter,
                      bool depth_test) override
    {
        gfx::Painter::DrawState state;
        state.write_color  = true; // writing depth to texture so this must be true!
        state.depth_test   = gfx::Painter::DepthTest::LessOrEQual;
        state.stencil_func = gfx::Painter::StencilFunc::Disabled;
        painter.Draw(drawable, model_to_world, material, state, DepthTextureShader());
    }
private:
};


void DrawBasisVectors(RenderPass& pass, gfx::Painter& painter, const glm::mat4& model_to_world_parent)
{
    glm::mat4 model_to_world = glm::mat4(1.0f); // I
    model_to_world *= model_to_world_parent;
    const glm::vec3 origin = {0.0f, 0.0f ,0.0f};
    const glm::vec3 x = {100.0f, 0.0f ,0.0f};
    const glm::vec3 y = {0.0f, 100.0f, 0.0f};
    const glm::vec3 z = {0.0f, 0.0f, 100.0f};
    pass.Draw(model_to_world, gfx::DynamicLine3D(origin, x, 1.0f),
         gfx::MaterialClassInst(gfx::CreateMaterialClassFromColor(gfx::Color::Green)), painter, false);
    pass.Draw(model_to_world, gfx::DynamicLine3D(origin, y, 1.0f),
         gfx::MaterialClassInst(gfx::CreateMaterialClassFromColor(gfx::Color::Red)), painter, false);
    pass.Draw(model_to_world, gfx::DynamicLine3D(origin, z, 1.0f),
         gfx::MaterialClassInst(gfx::CreateMaterialClassFromColor(gfx::Color::Blue)), painter, false);
}

void DrawScene(RenderPass& pass, gfx::Painter& painter, const State& state)
{
    // reference grid
    {
        const int grid_size = std::max(state.window.x/state.scale.x,
                                       state.window.y/state.scale.y) / state.zoom * 2.0f;
        const auto cell_size_units = static_cast<int>(state.grid);
        const auto num_grid_lines = (grid_size / cell_size_units) - 1;
        const auto num_cells = num_grid_lines + 1;
        const auto cell_size_normalized = 1.0f / num_cells;
        const auto cell_scale_factor = cell_size_units / cell_size_normalized;
        const auto grid_width = cell_size_units * num_cells;
        const auto grid_height = cell_size_units * num_cells;

        gfx::Grid grid_0(num_grid_lines, num_grid_lines, true);
        gfx::Grid grid_1(num_grid_lines, num_grid_lines, false);
        static gfx::MaterialClassInst material(gfx::CreateMaterialClassFromColor(gfx::Color::HotPink));

        float grid_origin_x = 0.0f;
        float grid_origin_y = 0.0f;
        const auto* perspective = base::GetOpt(state.perspective);
        if (perspective && *perspective == engine::GameView::Dimetric)
        {
            grid_origin_x = (int)state.camera_pos.x / cell_size_units * cell_size_units;
            grid_origin_y = (int)state.camera_pos.z / cell_size_units * cell_size_units;
        }
        else if (perspective && *perspective == engine::GameView::AxisAligned)
        {
            grid_origin_x = (int)state.camera_pos.x / cell_size_units * cell_size_units;
            grid_origin_y = (int)state.camera_pos.y / cell_size_units * cell_size_units;
        }

        gfx::Transform transform;
        //transform.Scale(state.zoom, state.zoom, 0.0f);

        transform.Push();
           transform.Scale(cell_scale_factor, cell_scale_factor);

           transform.Translate(grid_origin_x, grid_origin_y);
               pass.Draw(transform.GetAsMatrix(), grid_0, material, painter);

           transform.Translate(-grid_width, 0.0f, 0.0f);
               pass.Draw(transform.GetAsMatrix(), grid_1, material, painter);

           transform.Translate(0.0f, -grid_height, 0.0f);
               pass.Draw(transform.GetAsMatrix(), grid_0, material, painter);

           transform.Translate(grid_width, 0.0f, 0.0f);
               pass.Draw(transform.GetAsMatrix(), grid_1, material, painter);
        transform.Pop();
    }

    {
        DrawBasisVectors(pass, painter, glm::mat4(1.0f));
    }
    // content drawing.

    // cube
    {
        static gfx::MaterialClassInst material(gfx::CreateMaterialClassFromImage("textures/Checkerboard.png"));

        gfx::Transform transform;
        transform.Push();
           transform.Scale(100.0f, 100.0f, 100.0f);
           transform.RotateAroundZ(glm::radians(state.cube_rotation.z));
           transform.RotateAroundX(glm::radians(state.cube_rotation.x));
           transform.RotateAroundY(glm::radians(state.cube_rotation.y));
               pass.Draw(transform.GetAsMatrix(), gfx::Cube(), material, painter);
               //DrawBasisVectors(pass, painter, transform.GetAsMatrix());
        transform.Pop();
    }
}

void DrawCrossHair(State& state, gfx::Painter& painter)
{
    painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 0, state.window.x, state.window.y));
    painter.SetViewport(0, 0, state.window.x, state.window.y);
    painter.ResetViewMatrix();

    gfx::DebugDrawLine(painter,
                       gfx::FPoint(state.window.x * 0.5, 0.0f),
                       gfx::FPoint(state.window.x * 0.5, state.window.y), gfx::Color4f(gfx::Color::Green, 0.2f));
    gfx::DebugDrawLine(painter,
                       gfx::FPoint(0.0f, state.window.y * 0.5),
                       gfx::FPoint(state.window.x, state.window.y * 0.5), gfx::Color4f(gfx::Color::Green, 0.2f));
}

void DrawUI(State& state, gfx::Painter& painter)
{
    const auto& rect   = state.ui_window.GetBoundingRect();
    const float width  = rect.GetWidth();
    const float height = rect.GetHeight();
    const float scale  = std::min(state.window.x / width, state.window.y / height);
    const float device_viewport_width = width * scale;
    const float device_viewport_height = height * scale;

    painter.SetPixelRatio(glm::vec2(1.0f, 1.0f));
    painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 0, width, height));
    // Set the actual device viewport for rendering into the window surface.
    // the viewport retains the UI's aspect ratio and is centered in the middle
    // of the rendering surface.
    //painter->SetViewport((state.window.x - device_viewport_width)*0.5,
    //                     (state.window.y - device_viewport_height)*0.5,
    //                     device_viewport_width, device_viewport_height);
    painter.SetViewport(0, 0, width, height);
    painter.ResetViewMatrix();
    state.ui_window.Paint(state.ui_state, state.ui_painter, base::GetTime(), nullptr);
}

void BuildUI(uik::Window& window)
{
    uik::Widget* left_panel  =  nullptr;
    uik::Widget* right_panel = nullptr;
    {
        uik::Form form;
        form.SetName("left_panel");
        form.SetPosition(0.0f, 0.0f);
        form.SetSize(500.0f, 1000.0f);
        left_panel = window.AddWidget(form);
        window.LinkChild(nullptr, left_panel);
    }

    {
        uik::Form form;
        form.SetName("right_panel");
        form.SetPosition(1000.0f, 0.0f);
        form.SetSize(250.0f, 1000.0f);
        //right_panel = window.AddWidget(form);
        //window.LinkChild(nullptr, right_panel);
    }
}

int main(int argc, char* argv[])
{
    base::OStreamLogger logger(std::cout);
    logger.EnableTerminalColors(true);
    base::EnableDebugLog(true);
    base::SetGlobalLog(&logger);
    DEBUG("It's alive!");
    INFO("Copyright (c) 2020-2023 Sami Vaisanen");
    INFO("http://github.com/ensisoft/detonator");

    // context integration glue code that puts together
    // wdk::Context and gfx::Device
    class WindowContext : public dev::Context
    {
    public:
        WindowContext()
        {
            wdk::Config::Attributes attrs;
            attrs.red_size        = 8;
            attrs.green_size      = 8;
            attrs.blue_size       = 8;
            attrs.alpha_size      = 8;
            attrs.stencil_size    = 8;
            attrs.depth_size      = 24;
            attrs.surfaces.window = true;
            attrs.double_buffer   = true;
            attrs.sampling        = wdk::Config::Multisampling::MSAA4;
            attrs.srgb_buffer     = true;
            mConfig   = std::make_unique<wdk::Config>(attrs);
            mContext  = std::make_unique<wdk::Context>(*mConfig, 3, 0,  false /*debug*/, wdk::Context::Type::OpenGL_ES);
            mVisualID = mConfig->GetVisualID();
        }
        virtual void Display() override
        {
            mContext->SwapBuffers();
        }
        virtual void* Resolve(const char* name) override
        {
            return mContext->Resolve(name);
        }
        virtual void MakeCurrent() override
        {
            mContext->MakeCurrent(mSurface.get());
        }
        virtual Version GetVersion() const override
        {
            return Version::OpenGL_ES3;
        }
        virtual bool IsDebug() const override
        {
            return false;
        }
        wdk::uint_t GetVisualID() const
        { return mVisualID; }

        void SetWindowSurface(wdk::Window& window)
        {
            mSurface = std::make_unique<wdk::Surface>(*mConfig, window);
            mContext->MakeCurrent(mSurface.get());
            mConfig.reset();
        }
        void SetSwapInterval(int swap_interval)
        {
            mContext->SetSwapInterval(swap_interval);
        }
        void Dispose()
        {
            mContext->MakeCurrent(nullptr);
            mSurface->Dispose();
            mSurface.reset();
            mConfig.reset();
        }
    private:
        std::unique_ptr<wdk::Context> mContext;
        std::unique_ptr<wdk::Surface> mSurface;
        std::unique_ptr<wdk::Config>  mConfig;
        wdk::uint_t mVisualID = 0;
        bool mDebug = false;
    };

    auto context = std::make_shared<WindowContext>();
    auto dev_device  = dev::CreateDevice(context);
    auto gfx_device = dev_device->GetSharedGraphicsDevice();
    auto painter = gfx::Painter::Create(gfx_device);

    State state;
    state.camera_pos = {0.0f, 5.0f, 20.0f};
    state.camera.LookAt({0.0f, 0.0f, -1.0f});
    state.camera.Update();

    wdk::Window window;
    window.Create("Test", state.window.x, state.window.y, context->GetVisualID());

    window.OnResize = [&state](const wdk::WindowEventResize& resize) {
        state.window.x = resize.width;
        state.window.y = resize.height;
    };
    window.OnWantClose = [&window](const wdk::WindowEventWantClose& close) {
        window.Destroy();
    };
    window.OnKeyDown = [&window, &state](const wdk::WindowEventKeyDown& key) {
        bool top_level_key = true;
        if (key.symbol == wdk::Keysym::Escape)
        {
            if (state.focus == FocusLayer::UI)
                window.Destroy();
            else state.focus = FocusLayer::UI;
        }
        else if (key.symbol == wdk::Keysym::F1)
        {
            state.perspective.reset();
            state.camera_pos = {0.0f, 5.0f, 20.0f};
            state.camera.LookAt({0.0f, 0.0f, -1.0f});
            state.camera.Update();
        }
        else if (key.symbol == wdk::Keysym::F2)
        {
            state.camera.SetFromPerspective(engine::GameView::Dimetric);
            state.camera_pos = {0.0f, 0.0f, 0.0f};
            state.perspective = engine::GameView::Dimetric;
        }
        else if (key.symbol == wdk::Keysym::F3)
        {
            state.camera.SetFromPerspective(engine::GameView::AxisAligned);
            state.camera_pos = {0.0f, 0.0f, 0.0f};
            state.perspective = engine::GameView::AxisAligned;
        }
        else if (key.symbol == wdk::Keysym::KeyW && key.modifiers.test(wdk::Keymod::Control))
            window.Destroy();
        else if (key.symbol == wdk::Keysym::Key2)
        {
            const auto& depth = gfx::algo::ReadTexture(state.depth, state.device);
            gfx::WritePNG(*depth, "depth.png");
        }
        else top_level_key = false;

        if (top_level_key)
        {
            state.camera.Update();
            return;
        }

        if (state.focus == FocusLayer::UI)
        {


        }
        else if (state.focus == FocusLayer::Scene)
        {
            const auto free_cam = !state.perspective.has_value();
            const auto shift  = key.modifiers.test(wdk::Keymod::Shift);
            const auto& fwd   = free_cam ? state.camera.GetViewVector() : glm::vec3{0.0f, 0.0f, -1.0f};
            const auto& right = free_cam ? state.camera.GetRightVector() : glm::vec3{1.0f, 0.0f, 0.0f};
            const auto& up    = glm::vec3{0.0f, 1.0f, 0.0f};
            constexpr auto velocity = 50.0f;

            if (key.symbol == wdk::Keysym::KeyW && shift)
                state.camera_pos = state.camera_pos + up * state.dt * velocity;
            else if (key.symbol == wdk::Keysym::KeyS && shift)
                state.camera_pos = state.camera_pos - up * state.dt * velocity;
            else if (key.symbol == wdk::Keysym::KeyW)
                state.camera_pos = state.camera_pos + fwd * state.dt * velocity;
            else if (key.symbol == wdk::Keysym::KeyS)
                state.camera_pos = state.camera_pos - fwd * state.dt * velocity;
            else if (key.symbol == wdk::Keysym::KeyA)
                state.camera_pos = state.camera_pos + -right * state.dt * velocity;
            else if (key.symbol == wdk::Keysym::KeyD)
                state.camera_pos = state.camera_pos + right * state.dt * velocity;
            else if (key.symbol == wdk::Keysym::KeyX && shift)
                state.cube_rotation.x += 10.0f;
            else if (key.symbol == wdk::Keysym::KeyX)
                state.cube_rotation.x -= 10.0f;
            else if (key.symbol == wdk::Keysym::KeyY && shift)
                state.cube_rotation.y += 10.0f;
            else if (key.symbol == wdk::Keysym::KeyY)
                state.cube_rotation.y -= 10.0f;
            else if (key.symbol == wdk::Keysym::KeyZ && shift)
                state.cube_rotation.z += 10.0f;
            else if (key.symbol == wdk::Keysym::KeyZ)
                state.cube_rotation.z -= 10.0f;
            else if (key.symbol == wdk::Keysym::KeyR)
                state.cube_rotation = {0.0f, 0.0f, 0.0f};

            DEBUG("Camera %1", state.camera_pos);

            state.camera.Update();
        }
    };
    window.OnMouseMove = [&state](const wdk::WindowEventMouseMove& mickey) {
        if (state.focus == FocusLayer::UI)
        {

        }
        else
        {
            if (state.perspective.has_value())
            {

                const auto mouse_pos_window = glm::vec2{mickey.window_x, mickey.window_y};
                const auto mouse_pos_plane = MapToWorldPlane(state.projection,
                                                             GetViewMatrix(state, false),
                                                             mouse_pos_window,
                                                             state.window);
                //DEBUG("plane pos = %1", mouse_pos_plane);
                if (state.tracking)
                {
                    const auto mouse_move = mouse_pos_plane - state.mouse_pos;
                    const auto& perspective = state.perspective.value();
                    if (perspective == engine::GameView::Dimetric)
                    {
                        state.camera_pos.x -= mouse_move.x;
                        state.camera_pos.z -= mouse_move.y;
                    }
                    else if (perspective == engine::GameView::AxisAligned)
                    {
                        state.camera_pos.x -= mouse_move.x;
                        state.camera_pos.y -= mouse_move.y;
                    }
                    state.mouse_pos = mouse_pos_plane;
                }
            }
            else
            {
                const float half_width = state.window.x * 0.5f;
                const float half_height = state.window.y * 0.5f;
                const float x = mickey.window_x / half_width - 1.0f;
                const float y = 1.0f - mickey.window_y / half_height;
                state.camera.SetYaw(-90.0f + 100.0f * x);
                state.camera.SetPitch(0.0f + 30.0f * y);
                state.camera.Update();
                DEBUG("Camera yaw = %1 pitch = %2", state.camera.GetYaw(), state.camera.GetPitch());
            }
        }
    };
    window.OnMousePress = [&state](const wdk::WindowEventMousePress& mickey) {
        const auto shift = mickey.modifiers.test(wdk::Keymod::Shift);

        if (state.focus == FocusLayer::UI)
        {
            const auto window_point = uik::FPoint(mickey.window_x, mickey.window_y);
            // trivial mapping from window coordinates to UI coordinates
            // when the UI is placed at the top left corner of the window unscaled
            if (const auto* widget = state.ui_window.HitTest(window_point))
            {
                // todo: pass the press press to UI here.
            }
            else
            {
                state.focus = FocusLayer::Scene;
            }
        }
        else
        {
            const auto mouse_pos_window = glm::vec2{mickey.window_x, mickey.window_y};

            if (mickey.btn == wdk::MouseButton::Left)
            {
                const auto& plane_pos = MapToWorldPlane(state.projection,
                                                        GetViewMatrix(state, true, true),
                                                        mouse_pos_window,
                                                        state.window);
                DEBUG("Click pos = %1", plane_pos);
            }
            else if (mickey.btn == wdk::MouseButton::Right)
            {
                state.tracking = true;
                state.mouse_pos = MapToWorldPlane(state.projection,
                                                  GetViewMatrix(state, false),
                                                  mouse_pos_window,
                                                  state.window);
            }
            else if (mickey.btn == wdk::MouseButton::WheelScrollUp)
            {
                if (shift)
                {
                    state.zoom += 0.1f;
                    DEBUG("Zoom %1", state.zoom);
                }
                else
                {
                    if (state.grid == GridDensity::Grid10x10)
                        state.grid = GridDensity::Grid100x100;
                    else if (state.grid == GridDensity::Grid20x20)
                        state.grid = GridDensity::Grid10x10;
                    else if (state.grid == GridDensity::Grid50x50)
                        state.grid = GridDensity::Grid20x20;
                    else if (state.grid == GridDensity::Grid100x100)
                        state.grid = GridDensity::Grid50x50;
                    DEBUG("Grid setting %1", state.grid);
                }
            }
            else if (mickey.btn == wdk::MouseButton::WheelScrollDown)
            {
                if (shift)
                {
                    state.zoom -= 0.1f;
                    DEBUG("Zoom %1", state.zoom);
                }
                else
                {

                    if (state.grid == GridDensity::Grid10x10)
                        state.grid = GridDensity::Grid20x20;
                    else if (state.grid == GridDensity::Grid20x20)
                        state.grid = GridDensity::Grid50x50;
                    else if (state.grid == GridDensity::Grid50x50)
                        state.grid = GridDensity::Grid100x100;
                    else if (state.grid == GridDensity::Grid100x100)
                        state.grid = GridDensity::Grid10x10;

                    DEBUG("Grid setting %1", state.grid);
                }
            }
        }
    };
    window.OnMouseRelease = [&state](const wdk::WindowEventMouseRelease& mickey) {
        if (state.focus == FocusLayer::UI)
        {
            // todo: pass the mouse event to UI here.
        }
        else if (state.focus == FocusLayer::Scene)
        {
            if (mickey.btn == wdk::MouseButton::Right)
            {
                state.tracking = false;
                DEBUG("Camera pos %1", state.camera_pos);
            }
        }
    };

    context->SetWindowSurface(window);
    context->SetSwapInterval(1);

    const auto [json_ok, json, json_error] = base::JsonParseFile("ui/style/default.json");

    state.ui_style.SetDataLoader(nullptr);
    state.ui_style.SetClassLibrary(nullptr);
    state.ui_style.LoadStyle(json);

    state.ui_painter.SetPainter(painter.get());
    state.ui_painter.SetStyle(&state.ui_style);

    BuildUI(state.ui_window);
    state.ui_window.Style(state.ui_painter);
    state.ui_window.Open(state.ui_state);

    auto time = base::GetTime();

    gfx::Framebuffer* fbo = nullptr;
    gfx::Framebuffer::Config conf;
    conf.format = gfx::Framebuffer::Format::ColorRGBA8;
    conf.width  = 0; // irrelevant since using a texture target
    conf.height = 0; // irrelevant since using a texture target
    fbo = gfx_device->MakeFramebuffer("MainFBO");
    fbo->SetConfig(conf);

    gfx::Texture* depth = nullptr;
    depth = gfx_device->MakeTexture("DepthTexture");
    depth->SetFilter(gfx::Texture::MinFilter::Linear);
    depth->SetFilter(gfx::Texture::MagFilter::Linear);
    depth->SetGarbageCollection(false);

    state.depth = depth;
    state.device = gfx_device.get();

    while (window.DoesExist())
    {
        const auto now = base::GetTime();
        state.dt = now - time;

        gfx_device->BeginFrame();

        const unsigned surface_width  = state.window.x;
        const unsigned surface_height = state.window.y;
        if (depth->GetWidth() != surface_width || depth->GetHeight() != surface_height)
            depth->Allocate(surface_width, surface_height, gfx::Texture::Format::RGBA);

        if (const auto* perspective = base::GetOpt(state.perspective))
        {
            if (*perspective == engine::GameView::Dimetric)
            {
                //state.projection = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, -10000.0f, 10000.0f);
                //state.projection = glm::ortho(-5.0f, 5.0f, 5.0f, -5.0f, -10000.0f, 10000.0f);
                state.projection = glm::ortho(-state.window.x*0.5f, state.window.x*0.5f, -state.window.y*0.5f, state.window.y*0.5f, -10000.0f, 10000.0f);
            }
            else if (*perspective == engine::GameView::AxisAligned)
            {
                //state.projection = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 1.0f, 100.0f);
                //state.projection = glm::ortho(-5.0f, 5.0f, 5.0f, -5.0f, -1.0f, 1.0f); // !!
                state.projection = glm::ortho(-state.window.x*0.5f, state.window.x*0.5f, state.window.y*0.5f, -state.window.y*0.5f, -1.0f, 1.0f);
            }
        }
        else
        {
            const auto fov    = glm::radians(45.0f);
            const auto znear  = 1.0f;
            const auto zfar   = 100.0f;
            const auto aspect = state.window.x / state.window.y;
            state.projection = glm::perspective(fov, aspect, znear, zfar);
        }

        painter->SetEditingMode(false);
        painter->SetViewport(0, 0, state.window.x, state.window.y);
        painter->SetSurfaceSize(state.window.x, state.window.y);
        painter->SetProjectionMatrix(state.projection);
        painter->SetViewMatrix(GetViewMatrix(state));

        // draw to the depth texture.
        DepthTexturePass depth_pass;
        fbo->SetColorTarget(depth);
        painter->SetFramebuffer(fbo);
        painter->ClearColor(gfx::Color::Black);
        painter->ClearDepth(1.0f);
        DrawScene(depth_pass, *painter, state);

        const auto perspective_depth_test = !state.perspective.has_value();

        ColorPass color_pass(perspective_depth_test);
        painter->SetFramebuffer(nullptr);
        painter->ClearColor(gfx::Color4f(0x23, 0x23, 0x23, 0xff));
        painter->ClearDepth(1.0f);
        DrawScene(color_pass, *painter, state);

        // draw a focus rect
        if (state.focus == FocusLayer::Scene)
        {
            painter->SetViewport(0, 0, state.window.x, state.window.y);
            painter->SetSurfaceSize(state.window.x, state.window.y);
            painter->SetProjectionMatrix(gfx::MakeOrthographicProjection(state.window.x, state.window.y));
            painter->ResetViewMatrix();
            gfx::FRect rect(1.0f, 1.0f, state.window.x-2.0f, state.window.y-2.0f);
            gfx::DebugDrawRect(*painter, rect, gfx::Color::Blue, 1.0f);
        }

        DrawCrossHair(state, *painter);
        DrawUI(state, *painter);

        gfx_device->EndFrame(true /*display*/);
        gfx_device->CleanGarbage(120, gfx::Device::GCFlags::Textures);

        // process incoming (window) events
        wdk::native_event_t event;
        while (wdk::PeekEvent(event))
        {
            window.ProcessEvent(event);
        }


        time = now;
    }
    context->Dispose();
    return 0;
}
