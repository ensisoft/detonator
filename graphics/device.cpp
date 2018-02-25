// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "config.h"

#include "warnpush.h"
#  include <QtGui/QOpenGLFunctions>
#include "warnpop.h"

#include <cstdio>
#include <cassert>
#include <cstring> // for memcpy
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <string>

#include "base/assert.h"
#include "base/logging.h"
#include "shader.h"
#include "program.h"
#include "device.h"
#include "geometry.h"

#define GL_CHECK(x) \
    do {                                                                        \
        x;                                                                      \
        {                                                                       \
            const auto err = glGetError();                                      \
            if (err != GL_NO_ERROR) {                                           \
                std::printf("GL Error 0x%04x '%s' @ %s,%d\n", err, GLEnumToStr(err),  \
                    __FILE__, __LINE__);                                        \
                std::abort();                                                   \
            }                                                                   \
        }                                                                       \
    } while(0)


namespace
{

const char* GLEnumToStr(GLenum eval)
{
#define CASE(x) case x: return #x
    switch (eval)
    {
        CASE(GL_NO_ERROR);
        CASE(GL_INVALID_ENUM);
        CASE(GL_INVALID_VALUE);
        CASE(GL_INVALID_OPERATION);
        CASE(GL_OUT_OF_MEMORY);
        CASE(GL_STATIC_DRAW);
        CASE(GL_STREAM_DRAW);
        CASE(GL_ELEMENT_ARRAY_BUFFER);
        CASE(GL_ARRAY_BUFFER);

        // framebuffer
        CASE(GL_FRAMEBUFFER_COMPLETE);
        CASE(GL_FRAMEBUFFER_UNDEFINED);
        CASE(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
        CASE(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
        CASE(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER);
        CASE(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER);
        CASE(GL_FRAMEBUFFER_UNSUPPORTED);
        CASE(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE);
        CASE(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS);

        // ARB_debug_output
        CASE(GL_DEBUG_SOURCE_API_ARB);
        CASE(GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB);
        CASE(GL_DEBUG_SOURCE_SHADER_COMPILER_ARB);
        CASE(GL_DEBUG_SOURCE_THIRD_PARTY_ARB);
        CASE(GL_DEBUG_SOURCE_APPLICATION_ARB);
        CASE(GL_DEBUG_SOURCE_OTHER_ARB);
        CASE(GL_DEBUG_TYPE_ERROR_ARB);
        CASE(GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB);
        CASE(GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB);
        CASE(GL_DEBUG_TYPE_PORTABILITY_ARB);
        CASE(GL_DEBUG_TYPE_PERFORMANCE_ARB);
        CASE(GL_DEBUG_TYPE_OTHER_ARB);
        CASE(GL_DEBUG_SEVERITY_HIGH_ARB);
        CASE(GL_DEBUG_SEVERITY_MEDIUM_ARB);
        CASE(GL_DEBUG_SEVERITY_LOW_ARB);

        CASE(GL_FRAGMENT_SHADER);
        CASE(GL_VERTEX_SHADER);
    }
    return "???";
#undef CASE
}

template<typename To, typename From>
const To* SafeCast(const From* from)
{
    const auto* ptr = dynamic_cast<const To*>(from);
    ASSERT(ptr);
    return ptr;
}

} // namespace

namespace invaders
{

//
// OpenGL ES 2.0 based custom graphics device implementation
// try to keep this implementantation free of Qt in
// order to promote portability to possibly emscripten
// or Qt free implementation.
class OpenGLES2GraphicsDevice : public GraphicsDevice
{
public:
    virtual void Clear(const Color4f& color) override
    {
        GL_CHECK(glClearDepth(1.0f));
        GL_CHECK(glClearColor(color.Red(), color.Green(), color.Blue(), color.Alpha()));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
    }

    virtual std::unique_ptr<Shader> NewShader() override
    { return std::make_unique<ShaderImpl>(); }

    virtual std::unique_ptr<Program> NewProgram() override
    { return std::make_unique<ProgImpl>(); }

    virtual std::unique_ptr<Geometry> NewGeometry() override
    { return std::make_unique<GeomImpl>(); }

    virtual void Draw(const Program& program, const Geometry& geometry, const State& state) override
    {
        SetState(state);

        auto* myprog = (ProgImpl*)(&program);
        auto* mygeom = (GeomImpl*)(&geometry);
        myprog->SetState();
        mygeom->Draw(myprog->GetName());
    }

    virtual Type GetDeviceType() const override
    { return Type::OpenGL_ES2; }

    virtual void GetState(StateBuffer* state) const
    {
        NativeState s;
        glGetIntegerv(GL_BLEND, &s.gl_blend_enabled);
        glGetIntegerv(GL_BLEND_SRC_RGB,   &s.gl_blend_src_rgb);
        glGetIntegerv(GL_BLEND_DST_RGB,   &s.gl_blend_dst_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &s.gl_blend_src_alpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &s.gl_blend_dst_alpha);

        state->resize(sizeof(s));
        std::memcpy(&(*state)[0], &s, sizeof(s));

        GL_CHECK((void)0);
    }
    virtual void SetState(const StateBuffer& state) override
    {
        // there's a bunch of implicit state which should be made explicit
        // - polygon front face
        // - stencil test, stencil func
        // - depth test, depth func
        // - blend test, blend function
        // - face culling
        // - write masks
        // - scissor, viewport
        // - etc.
        //
        // The problem is that Qt painter restores some of this set
        // in a call to endNativePainting but not all of this state.
        // additionally the state managed by Qt can change between
        // versions.
        // So really what this means is that the custom device should
        // be able to deal with state so that the state perceived by Qt
        // does not change.
        // This means reading back any state values that we're planning to
        // change in our drawing, saving them for the duration of custom
        // drawing and restoring before using Qt to paint again.
        //
        // Ok, you might think maybe it'd be easier to just create own context.
        // And yeah that would (even with share groups for resource sharing)
        // except that the Qt widget is backed by a FBO and FBOs are not shareable
        // between contexts. Douh!
        //
        // Anyway for the time being we're lazy and just leave most of the state to
        // "higher powers" and hope that it'd work. Excpect bugs.

        ASSERT(state.size() == sizeof(NativeState));
        NativeState s;
        std::memcpy((void*)&s, &state[0], sizeof(NativeState));

        Toggle(GL_BLEND, s.gl_blend_enabled);
        glBlendFunc(s.gl_blend_src_rgb,   s.gl_blend_dst_rgb);
        glBlendFunc(s.gl_blend_src_alpha, s.gl_blend_dst_alpha);

        GL_CHECK((void)0);
    }
private:
    struct NativeState {
        int gl_blend_src_rgb = 0;
        int gl_blend_dst_rgb = 0;
        int gl_blend_src_alpha = 0;
        int gl_blend_dst_alpha = 0;
        int gl_blend_enabled = 0;
    };

    void Toggle(GLenum flag, bool on_off)
    {
        if (on_off)
            glEnable(flag);
        else glDisable(flag);
    }

    void SetState(const State& state)
    {
        GL_CHECK(glViewport(state.viewport.x, state.viewport.y,
            state.viewport.width, state.viewport.height));

        if (state.bEnableBlend)
        {
            GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            GL_CHECK(glEnable(GL_BLEND));
        }
        else
        {
            GL_CHECK(glDisable(GL_BLEND));
        }

    }

private:
    class GeomImpl : public Geometry, protected QOpenGLFunctions
    {
    public:
        GeomImpl()
        {
            initializeOpenGLFunctions();
        }
        virtual void Update(const Vertex* verts, std::size_t count) override
        {
            mData.clear();
            mData.resize(count);
            for (size_t i=0; i<count; ++i)
            {
                mData[i] = verts[i];
            }
        }
        void Draw(GLuint program)
        {
            GLint pos = glGetAttribLocation(program,  "aPosition");
            GL_CHECK(glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), &mData[0]));
            GL_CHECK(glEnableVertexAttribArray(pos));
            GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, mData.size()));
        }
    private:
        std::vector<Vertex> mData;
    };

    class ProgImpl : public Program, protected QOpenGLFunctions
    {
    public:
        ProgImpl()
        {
            initializeOpenGLFunctions();
        }
       ~ProgImpl()
        {
            if (mProgram)
            {
                GL_CHECK(glDeleteProgram(mProgram));
            }
        }
        virtual bool Build(const std::vector<const Shader*>& shaders) override
        {
            GLuint prog = glCreateProgram();
            DEBUG("New program %1", prog);

            for (const auto* shader : shaders)
            {
                GL_CHECK(glAttachShader(prog,
                    static_cast<const ShaderImpl*>(shader)->GetName()));
            }
            GL_CHECK(glLinkProgram(prog));
            GL_CHECK(glValidateProgram(prog));

            GLint link_status = 0;
            GLint valid_status = 0;
            GL_CHECK(glGetProgramiv(prog, GL_LINK_STATUS, &link_status));
            GL_CHECK(glGetProgramiv(prog, GL_VALIDATE_STATUS, &valid_status));

            GLint length = 0;
            GL_CHECK(glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &length));

            std::string build_info;
            build_info.resize(length);
            GL_CHECK(glGetProgramInfoLog(prog, length, nullptr, &build_info[0]));

            if ((link_status == 0) || (valid_status == 0))
            {
                ERROR("Program build error: %1", build_info);
                GL_CHECK(glDeleteProgram(prog));
                return false;
            }

            DEBUG("Program was built succesfully!");
            DEBUG("Program info: %1", build_info);
            if (mProgram)
            {
                GL_CHECK(glDeleteProgram(mProgram));
                GL_CHECK(glUseProgram(0));
            }
            mProgram = prog;
            mVersion++;
            return true;
        }
        virtual bool IsValid() const override
        { return mProgram != 0; }

        virtual void SetUniform(const char* name, float x) override
        {
            auto ret = glGetUniformLocation(mProgram, name);
            if (ret == -1)
                return;
            GL_CHECK(glUseProgram(mProgram));
            GL_CHECK(glUniform1f(ret, x));
        }
        virtual void SetUniform(const char* name, float x, float y) override
        {
            glUseProgram(mProgram);

            auto ret = glGetUniformLocation(mProgram, name);
            if (ret == -1)
                return;
            GL_CHECK(glUseProgram(mProgram));
            GL_CHECK(glUniform2f(ret, x, y));
        }

        void SetState() //const
        {
            GL_CHECK(glUseProgram(mProgram));
        }
        GLuint GetName() const
        { return mProgram; }

    private:
        GLuint mProgram = 0;
        GLuint mVersion = 0;

    };

    class ShaderImpl : public Shader, protected QOpenGLFunctions
    {
    public:
        ShaderImpl()
        {
            initializeOpenGLFunctions();
        }
       ~ShaderImpl()
        {
            if (mShader)
            {
                GL_CHECK(glDeleteShader(mShader));
            }
        }
        virtual bool CompileSource(const std::string& source) override
        {
            GLenum type = GL_NONE;
            std::stringstream ss(source);
            std::string line;
            while (std::getline(ss, line) && type == GL_NONE)
            {
                if (line.find("gl_Position") != std::string::npos)
                    type = GL_VERTEX_SHADER;
                else if (line.find("gl_FragColor") != std::string::npos)
                    type = GL_FRAGMENT_SHADER;
            }
            ASSERT(type != GL_NONE);

            GLint status = 0;
            GLint shader = glCreateShader(type);
            DEBUG("New shader %1 %2", shader, GLEnumToStr(type));

            const char* source_ptr = source.c_str();
            GL_CHECK(glShaderSource(shader, 1, &source_ptr, nullptr));
            GL_CHECK(glCompileShader(shader));
            GL_CHECK(glGetShaderiv(shader, GL_COMPILE_STATUS, &status));

            GLint length = 0;
            GL_CHECK(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length));

            std::string compile_info;
            compile_info.resize(length);
            GL_CHECK(glGetShaderInfoLog(shader, length, nullptr, &compile_info[0]));

            if (status == 0)
            {
                GL_CHECK(glDeleteShader(shader));
                ERROR("Shader compile error %1", compile_info);
                return false;
            }
            else
            {
                DEBUG("Shader was built succesfully!");
                DEBUG("Shader info: %1", compile_info);
            }

            if (mShader)
            {
                GL_CHECK(glDeleteShader(mShader));
            }
            mShader = shader;
            mVersion++;
            return true;
        }
        virtual bool IsValid() const override
        { return mShader != 0; }

        GLuint GetName() const
        { return mShader; }
    private:
        GLuint mShader  = 0;
        GLuint mVersion = 0;
    };
private:

};

// static
std::shared_ptr<GraphicsDevice> GraphicsDevice::Create(Type type)
{
    return std::make_shared<OpenGLES2GraphicsDevice>();
}


} // namespace