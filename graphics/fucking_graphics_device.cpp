// Copyright (c) 2010-2019 Sami Väisänen, Ensisoft
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
#include <map>

#include "base/assert.h"
#include "base/logging.h"
#include "shader.h"
#include "program.h"
#include "device.h"
#include "geometry.h"
#include "texture.h"

#include "stb_image.h"

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
class OpenGLES2GraphicsDevice : public GraphicsDevice, protected QOpenGLFunctions
{
public:
    OpenGLES2GraphicsDevice()
    {
        initializeOpenGLFunctions();
        // it'd make sense to create own context here but the but
        // problem is that currently we're using Qt widget as the window
        // and it creates a FBO into which all the rendering is done.
        // The FBOs are not shareable between contexts so then we'd need
        // to render to a texture shared between the widget context and
        // our context.

        GLint stencil_bits = 0;
        GLint red_bits   = 0;
        GLint green_bits = 0;
        GLint blue_bits  = 0;
        GLint alpha_bits = 0;
        GLint depth_bits = 0;
        glGetIntegerv(GL_STENCIL_BITS, &stencil_bits);
        glGetIntegerv(GL_RED_BITS, &red_bits);
        glGetIntegerv(GL_GREEN_BITS, &green_bits);
        glGetIntegerv(GL_BLUE_BITS, &blue_bits);
        glGetIntegerv(GL_ALPHA_BITS, &alpha_bits);
        glGetIntegerv(GL_DEPTH_BITS, &depth_bits);

        INFO("OpenGLESGraphicsDevice");
        INFO("Stencil bits: %1", stencil_bits);
        INFO("Red bits: %1", red_bits);
        INFO("Blue bits: %1", blue_bits);
        INFO("Green bits: %1", green_bits);
        INFO("Alpha bits: %1", alpha_bits);
        INFO("Depth bits: %1", depth_bits);
    }

    virtual void ClearColor(const Color4f& color) override
    {
        GL_CHECK(glClearColor(color.Red(), color.Green(), color.Blue(), color.Alpha()));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
    }
    virtual void ClearStencil(int value) override
    {
        GL_CHECK(glClearStencil(value));
        GL_CHECK(glClear(GL_STENCIL_BUFFER_BIT));
    }

    virtual Shader* FindShader(const std::string& name) override
    {
        auto it = mShaders.find(name);
        if (it == std::end(mShaders))
            return nullptr;
        return it->second.get();
    }

    virtual Shader* MakeShader(const std::string& name) override
    {
        auto shader = std::make_unique<ShaderImpl>();
        auto* ret   = shader.get();
        mShaders[name] = std::move(shader);
        return ret;
    }

    virtual Program* FindProgram(const std::string& name) override
    {
        auto it = mPrograms.find(name);
        if (it == std::end(mPrograms))
            return nullptr;
        return it->second.get();
    }

    virtual Program* MakeProgram(const std::string& name) override
    {
        auto program = std::make_unique<ProgImpl>();
        auto* ret    = program.get();
        mPrograms[name] = std::move(program);
        return ret;
    }

    virtual Geometry* FindGeometry(const std::string& name) override
    {
        auto it = mGeoms.find(name);
        if (it == std::end(mGeoms))
            return nullptr;
        return it->second.get();
    }

    virtual Geometry* MakeGeometry(const std::string& name) override
    {
        auto geometry = std::make_unique<GeomImpl>();
        auto* ret = geometry.get();
        mGeoms[name] = std::move(geometry);
        return ret;
    }

    virtual Texture* FindTexture(const std::string& name) override
    {
        auto it = mTextures.find(name);
        if (it == std::end(mTextures))
            return nullptr;
        return it->second.get();
    }

    virtual Texture* MakeTexture(const std::string& name) override
    {
        auto texture = std::make_unique<TextureImpl>();
        auto* ret = texture.get();
        mTextures[name] = std::move(texture);
        return ret;
    }

    virtual void DeleteShaders()
    {
        mShaders.clear();
    }
    virtual void DeletePrograms()
    {
        mPrograms.clear();
    }

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
        auto* fucking_qt_const_failure = const_cast<OpenGLES2GraphicsDevice*>(this);

        NativeState s;
        fucking_qt_const_failure->glGetIntegerv(GL_BLEND, &s.gl_blend_enabled);
        fucking_qt_const_failure->glGetIntegerv(GL_BLEND_SRC_RGB,   &s.gl_blend_src_rgb);
        fucking_qt_const_failure->glGetIntegerv(GL_BLEND_DST_RGB,   &s.gl_blend_dst_rgb);
        fucking_qt_const_failure->glGetIntegerv(GL_BLEND_SRC_ALPHA, &s.gl_blend_src_alpha);
        fucking_qt_const_failure->glGetIntegerv(GL_BLEND_DST_ALPHA, &s.gl_blend_dst_alpha);
        fucking_qt_const_failure->glGetIntegerv(GL_STENCIL_TEST, &s.gl_stencil_enabled);
        fucking_qt_const_failure->glGetIntegerv(GL_STENCIL_VALUE_MASK, &s.gl_stencil_mask);
        fucking_qt_const_failure->glGetIntegerv(GL_STENCIL_REF, &s.gl_stencil_ref);
        fucking_qt_const_failure->glGetIntegerv(GL_STENCIL_FUNC, &s.gl_stencil_func);
        fucking_qt_const_failure->glGetIntegerv(GL_STENCIL_FAIL, &s.gl_stencil_fail);
        fucking_qt_const_failure->glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &s.gl_stencil_dpass);
        fucking_qt_const_failure->glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &s.gl_stencil_dfail);
        fucking_qt_const_failure->glGetIntegerv(GL_COLOR_WRITEMASK, s.gl_color_mask);

        state->resize(sizeof(s));
        std::memcpy(&(*state)[0], &s, sizeof(s));
        //GL_CHECK((void)0);
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

        EnableIf(GL_BLEND, s.gl_blend_enabled);
        glBlendFunc(s.gl_blend_src_rgb,   s.gl_blend_dst_rgb);
        glBlendFunc(s.gl_blend_src_alpha, s.gl_blend_dst_alpha);

        EnableIf(GL_STENCIL_TEST, s.gl_stencil_enabled);
        glStencilFunc(s.gl_stencil_func, s.gl_stencil_ref, s.gl_stencil_mask);
        glStencilOp(s.gl_stencil_fail, s.gl_stencil_dfail, s.gl_stencil_dpass);

        glColorMask(s.gl_color_mask[0], s.gl_color_mask[1], s.gl_color_mask[2], s.gl_color_mask[3]);

        GL_CHECK((void)0);
    }
private:
    struct NativeState {
        int gl_blend_src_rgb   = 0;
        int gl_blend_dst_rgb   = 0;
        int gl_blend_src_alpha = 0;
        int gl_blend_dst_alpha = 0;
        int gl_blend_enabled   = 0;

        int gl_stencil_enabled = 0;
        int gl_stencil_func    = 0;
        int gl_stencil_ref     = 0;
        int gl_stencil_mask    = 0;
        int gl_stencil_fail    = 0;
        int gl_stencil_dfail   = 0;
        int gl_stencil_dpass   = 0;

        int gl_color_mask[4]   = {};
    };

    bool EnableIf(GLenum flag, bool on_off)
    {
        if (on_off)
        {
            GL_CHECK(glEnable(flag));
        }
        else
        {
            GL_CHECK(glDisable(flag));
        }
        return on_off;
    }
    static GLenum ToGLEnum(State::StencilFunc func)
    {
        using Func = State::StencilFunc;
        switch (func)
        {
            case Func::Disabled:         return GL_NONE;
            case Func::PassAlways:       return GL_ALWAYS;
            case Func::PassNever:        return GL_NEVER;
            case Func::RefIsLess:        return GL_LESS;
            case Func::RefIsLessOrEqual: return GL_LEQUAL;
            case Func::RefIsMore:        return GL_GREATER;
            case Func::RefIsMoreOrEqual: return GL_GEQUAL;
            case Func::RefIsEqual:       return GL_EQUAL;
            case Func::RefIsNotEqual:    return GL_NOTEQUAL;
        }
        ASSERT(!"???");
        return GL_NONE;
    }
    static GLenum ToGLEnum(State::StencilOp op)
    {
        using Op = State::StencilOp;
        switch (op)
        {
            case Op::DontModify: return GL_KEEP;
            case Op::WriteZero:  return GL_ZERO;
            case Op::WriteRef:   return GL_REPLACE;
            case Op::Increment:  return GL_INCR;
            case Op::Decrement:  return GL_DECR;
        }
        ASSERT("???");
        return GL_NONE;
    }

    void SetState(const State& state)
    {
        GL_CHECK(glViewport(state.viewport.x, state.viewport.y,
            state.viewport.width, state.viewport.height));

        if (EnableIf(GL_BLEND, state.bEnableBlend))
        {
            GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        }

        if (EnableIf(GL_STENCIL_TEST, state.stencil_func != State::StencilFunc::Disabled))
        {
            const auto stencil_func  = ToGLEnum(state.stencil_func);
            const auto stencil_fail  = ToGLEnum(state.stencil_fail);
            const auto stencil_dpass = ToGLEnum(state.stencil_dpass);
            const auto stencil_dfail = ToGLEnum(state.stencil_dfail);
            GL_CHECK(glStencilFunc(stencil_func, state.stencil_ref, state.stencil_mask));
            GL_CHECK(glStencilOp(stencil_fail, stencil_dfail, stencil_dpass));
        }
        if (state.bWriteColor)
        {
            GL_CHECK(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
        }
        else
        {
            GL_CHECK(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));
        }
    }

private:
    class TextureImpl : public Texture,  protected QOpenGLFunctions
    {
    public:
        TextureImpl()
        {
            initializeOpenGLFunctions();

            GL_CHECK(glGenTextures(1, &mName));
            DEBUG("New texture object %1 name = %2", (void*)this, mName);

            mMinFilter = MinFilter::Mipmap;
            mMagFilter = MagFilter::Linear;
            GL_CHECK(glActiveTexture(GL_TEXTURE0));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, mName));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        }
        ~TextureImpl()
        {
            GL_CHECK(glDeleteTextures(1, &mName));
            DEBUG("Deleted texture %1", mName);
        }

        virtual void Upload(const void* bytes, unsigned xres, unsigned yres, Format format) override
        {
            DEBUG("Loading texture %1 %2x%3 px", mName, xres, yres);

            GLenum sizeFormat = 0;
            GLenum baseFormat = 0;
            switch (format)
            {
                case Format::RGB:
                    sizeFormat = GL_RGB;
                    baseFormat = GL_RGB;
                    break;
                case Format::RGBA:
                    sizeFormat = GL_RGBA;
                    baseFormat = GL_RGBA;
                    break;
                default: assert(!"unknown texture format."); break;
            }

            GL_CHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
            GL_CHECK(glActiveTexture(GL_TEXTURE0));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, mName));
            GL_CHECK(glTexImage2D(GL_TEXTURE_2D,
                0, // mip level
                sizeFormat,
                xres,
                yres,
                0, // border must be 0
                baseFormat,
                GL_UNSIGNED_BYTE,
                bytes));
            GL_CHECK(glGenerateMipmap(GL_TEXTURE_2D));
            mWidth  = xres;
            mHeight = yres;
        }

        virtual void UploadFromFile(const std::string& filename) override
        {
            DEBUG("Loading texture %1", filename);

            std::ifstream in(filename, std::ios::in | std::ios::binary);
            if (!in)
                throw std::runtime_error("failed to open file: " + filename);

            in.seekg(0, std::ios::end);
            const auto size = (std::size_t)in.tellg();
            in.seekg(0, std::ios::beg);

            std::vector<char> data;
            data.resize(size);
            in.read(&data[0], size);
            if ((std::size_t)in.gcount() != size)
                throw std::runtime_error("failed to read all of: " + filename);

            int xres  = 0;
            int yres  = 0;
            int depth = 0;
            auto* bmp = stbi_load_from_memory((const stbi_uc*)data.data(),
                (int)data.size(), &xres, &yres, &depth, 0);
            if (bmp == nullptr)
                throw std::runtime_error("failed to decompress texture: " + filename);

            DEBUG("Decompressed texture %1x%2 px @ %3bits", xres, yres, depth * 8);

            Format format = Format::RGB;
            switch (depth)
            {
                //case 1: format = Format::Grayscale; break;
                case 3: format = Format::RGB; break;
                case 4: format = Format::RGBA; break;
                default:
                   stbi_image_free(bmp);
                   throw std::runtime_error("unknown texture format (depth): " + filename);
            }

            Upload(bmp, xres, yres, format);

            stbi_image_free(bmp);
        }

        virtual void SetFilter(MinFilter filter) override
        {
            mMinFilter = filter;
            GL_CHECK(glActiveTexture(GL_TEXTURE0));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, mName));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, getMinFilterAsGLEnum()));
        }

        virtual void SetFilter(MagFilter filter) override
        {
            mMagFilter = filter;
            GL_CHECK(glActiveTexture(GL_TEXTURE0));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, mName));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, getMagFilterAsGLEnum()));
        }

        Texture::MinFilter GetMinFilter() const override
        { return mMinFilter; }

        Texture::MagFilter GetMagFilter() const override
        { return mMagFilter; }

        virtual unsigned GetWidth() const override
        { return mWidth; }

        virtual unsigned GetHeight() const override
        { return mHeight; }

        void bind(GLuint unit)
        {
            //GL_CHECK(glBindTextureUnit(unit, m_name));
            //DEBUG("Texture %1 bound to unit %2", m_name, unit);
        }

        GLenum getMinFilterAsGLEnum() const
        {
            switch (mMinFilter)
            {
                case Texture::MinFilter::Nearest:
                return GL_NEAREST;
                case Texture::MinFilter::Linear:
                return GL_LINEAR;
                case Texture::MinFilter::Mipmap:
                return GL_LINEAR_MIPMAP_LINEAR;
            }
            assert(!"Incorrect texture minifying filter.");
            return GL_NONE;
        }

        GLenum getMagFilterAsGLEnum() const
        {
            switch (mMagFilter)
            {
                case Texture::MagFilter::Nearest:
                return GL_NEAREST;
                case Texture::MagFilter::Linear:
                return GL_LINEAR;
            }
            assert(!"Incorrect texture magnifying filter setting.");
            return GL_NONE;
        }

        GLuint GetName() const
        { return mName; }

    private:
        GLuint mName = 0;
    private:
        MinFilter mMinFilter = MinFilter::Nearest;
        MagFilter mMagFilter = MagFilter::Nearest;
    private:
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
    };

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
            GLint aPosition = glGetAttribLocation(program, "aPosition");
            GLint aTexCoord = glGetAttribLocation(program, "aTexCoord");

            uint8_t* base = reinterpret_cast<uint8_t*>(&mData[0]);
            if (aPosition != -1)
            {
                GL_CHECK(glVertexAttribPointer(aPosition, 2, GL_FLOAT, GL_FALSE,
                    sizeof(Vertex), (void*)(base + offsetof(Vertex, aPosition))));
                GL_CHECK(glEnableVertexAttribArray(aPosition));
            }
            if (aTexCoord != -1)
            {
                GL_CHECK(glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE,
                    sizeof(Vertex), (void*)(base + offsetof(Vertex, aTexCoord))));
                GL_CHECK(glEnableVertexAttribArray(aTexCoord));
            }
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
        virtual void SetUniform(const char* name, float x, float y, float z) override
        {
            glUseProgram(mProgram);

            auto ret = glGetUniformLocation(mProgram, name);
            if (ret == -1)
                return;
            GL_CHECK(glUniform3f(ret, x, y, z));
        }
        virtual void SetUniform(const char* name, float x, float y, float z, float w) override
        {
            glUseProgram(mProgram);

            auto ret = glGetUniformLocation(mProgram, name);
            if (ret == -1)
                return;
            GL_CHECK(glUniform4f(ret, x, y, z, w));
        }
        virtual void SetTexture(const char* sampler, unsigned unit, const Texture& texture) override
        {
            const auto& texture_impl = dynamic_cast<const TextureImpl&>(texture);
            const auto  texture_name = texture_impl.GetName();

            auto ret =  glGetUniformLocation(mProgram, sampler);
            if (ret == -1)
                return;
            GL_CHECK(glActiveTexture(GL_TEXTURE0 + unit));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture_name));
            GL_CHECK(glUniform1i(ret, unit));
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
        virtual bool CompileFile(const std::string& file) override
        {
            std::ifstream stream;
            stream.open("shaders/es2/" + file);
            if (!stream.is_open())
                throw std::runtime_error("failed to open file: " + file);

            const std::string source(std::istreambuf_iterator<char>(stream), {});

            return CompileSource(source);
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
    std::map<std::string, std::unique_ptr<Geometry>> mGeoms;
    std::map<std::string, std::unique_ptr<Shader>> mShaders;
    std::map<std::string, std::unique_ptr<Program>> mPrograms;
    std::map<std::string, std::unique_ptr<Texture>> mTextures;
};

// static
std::shared_ptr<GraphicsDevice> GraphicsDevice::Create(Type type)
{
    return std::make_shared<OpenGLES2GraphicsDevice>();
}


} // namespace
