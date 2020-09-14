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

#include <GLES2/gl2.h>

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
#include "base/utility.h"
#include "resource.h"
#include "shader.h"
#include "program.h"
#include "device.h"
#include "geometry.h"
#include "texture.h"
#include "color4f.h"

#define GL_CALL(x)                                      \
do {                                                    \
    mGL.x;                                              \
    const auto err = mGL.glGetError();                  \
    if (err != GL_NO_ERROR) {                           \
        std::printf("GL Error %s @ %s,%d\n",            \
            GLEnumToStr(err), __FILE__, __LINE__);      \
        std::fflush(stdout);                            \
        std::abort();                                   \
    }                                                   \
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
        CASE(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
        CASE(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
        CASE(GL_FRAMEBUFFER_UNSUPPORTED);

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

namespace gfx
{

// This struct holds the OpenGL ES 2.0 entry points that
// we need in this device implementation.
//
// Few notes about this particular implementation:
//
// 1. The pointers are members of an object instead of global
// function pointers because in fact it's possible that the
// pointers would change between one context to another context
// depending on the particular configuration used to create the
// context. (For example GDI pixel format on windows).
// So obviously one set of global function pointers would not then
// work for multiple devices should the function addresses change.
//
// 2. We're not using something like GLEW here because GLEW has a
// problem in that it doesn't use runtime "get proc" type of function
// resolution for *all* functions. I.e. it leaves the old functions
// (back from the OpenGL 1, fixed pipeline age) unresolved and expects
// them to be exported by the GL library and that the user also then
// links against -lGL.
// This however is incorrect in cases where the OpenGL context is
// provided by some "virtual context system", e.e libANGLE or Qt's
// QOpenGLWidget.We do not know the underlying OpenGL implementation
// and should not know any such details that GLEW would expect us to
// know. Rather a better way to deal with the problem is to resolve all
// functions in the same manner.
struct OpenGLFunctions
{
    PFNGLCREATEPROGRAMPROC           glCreateProgram;
    PFNGLCREATESHADERPROC            glCreateShader;
    PFNGLSHADERSOURCEPROC            glShaderSource;
    PFNGLGETERRORPROC                glGetError;
    PFNGLCOMPILESHADERPROC           glCompileShader;
    PFNGLDETACHSHADERPROC            glAttachShader;
    PFNGLDELETESHADERPROC            glDeleteShader;
    PFNGLLINKPROGRAMPROC             glLinkProgram;
    PFNGLUSEPROGRAMPROC              glUseProgram;
    PFNGLVALIDATEPROGRAMPROC         glValidateProgram;
    PFNGLDELETEPROGRAMPROC           glDeleteProgram;
    PFNGLCOLORMASKPROC               glColorMask;
    PFNGLSTENCILFUNCPROC             glStencilFunc;
    PFNGLSTENCILOPPROC               glStencilOp;
    PFNGLCLEARCOLORPROC              glClearColor;
    PFNGLCLEARPROC                   glClear;
    PFNGLCLEARSTENCILPROC            glClearStencil;
    PFNGLCLEARDEPTHFPROC             glClearDepthf;
    PFNGLBLENDFUNCPROC               glBlendFunc;
    PFNGLVIEWPORTPROC                glViewport;
    PFNGLDRAWARRAYSPROC              glDrawArrays;
    PFNGLGETATTRIBLOCATIONPROC       glGetAttribLocation;
    PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
    PFNGLGETSTRINGPROC               glGetString;
    PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation;
    PFNGLUNIFORM1IPROC               glUniform1i;
    PFNGLUNIFORM2IPROC               glUniform2i;
    PFNGLUNIFORM1FPROC               glUniform1f;
    PFNGLUNIFORM2FPROC               glUniform2f;
    PFNGLUNIFORM3FPROC               glUniform3f;
    PFNGLUNIFORM4FPROC               glUniform4f;
    PFNGLUNIFORM2FVPROC              glUniform2fv;
    PFNGLUNIFORM3FVPROC              glUniform3fv;
    PFNGLUNIFORM4FPROC               glUniform4fv;
    PFNGLUNIFORMMATRIX2FVPROC        glUniformMatrix2fv;
    PFNGLUNIFORMMATRIX3FVPROC        glUniformMatrix3fv;
    PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv;
    PFNGLGETPROGRAMIVPROC            glGetProgramiv;
    PFNGLGETSHADERIVPROC             glGetShaderiv;
    PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog;
    PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog;
    PFNGLDELETETEXTURESPROC          glDeleteTextures;
    PFNGLGENTEXTURESPROC             glGenTextures;
    PFNGLBINDTEXTUREPROC             glBindTexture;
    PFNGLACTIVETEXTUREPROC           glActiveTexture;
    PFNGLGENERATEMIPMAPPROC          glGenerateMipmap;
    PFNGLTEXIMAGE2DPROC              glTexImage2D;
    PFNGLTEXPARAMETERIPROC           glTexParameteri;
    PFNGLPIXELSTOREIPROC             glPixelStorei;
    PFNGLENABLEPROC                  glEnable;
    PFNGLDISABLEPROC                 glDisable;
    PFNGLGETINTEGERVPROC             glGetIntegerv;
    PFNGLREADPIXELSPROC              glReadPixels;
    PFNGLLINEWIDTHPROC               glLineWidth;
};

//
// OpenGL ES 2.0 based custom graphics device implementation
// try to keep this implementantation free of Qt in
// order to promote portability to possibly emscripten
// or Qt free implementation.
class OpenGLES2GraphicsDevice : public Device
{
public:
    OpenGLES2GraphicsDevice(Context* context)
        : mContext(context)
    {
    #define RESOLVE(x) mGL.x = reinterpret_cast<decltype(mGL.x)>(mContext->Resolve(#x));
        RESOLVE(glCreateProgram);
        RESOLVE(glCreateShader);
        RESOLVE(glShaderSource);
        RESOLVE(glGetError);
        RESOLVE(glCompileShader);
        RESOLVE(glAttachShader);
        RESOLVE(glDeleteShader);
        RESOLVE(glLinkProgram);
        RESOLVE(glUseProgram);
        RESOLVE(glValidateProgram);
        RESOLVE(glDeleteProgram);
        RESOLVE(glColorMask);
        RESOLVE(glStencilFunc);
        RESOLVE(glStencilOp);
        RESOLVE(glClearColor);
        RESOLVE(glClear);
        RESOLVE(glClearStencil);
        RESOLVE(glClearDepthf);
        RESOLVE(glBlendFunc);
        RESOLVE(glViewport);
        RESOLVE(glDrawArrays);
        RESOLVE(glGetAttribLocation);
        RESOLVE(glVertexAttribPointer);
        RESOLVE(glEnableVertexAttribArray);
        RESOLVE(glGetString);
        RESOLVE(glGetUniformLocation);
        RESOLVE(glUniform1i);
        RESOLVE(glUniform2i);
        RESOLVE(glUniform1f);
        RESOLVE(glUniform2f);
        RESOLVE(glUniform3f);
        RESOLVE(glUniform4f);
        RESOLVE(glUniform2fv);
        RESOLVE(glUniform3fv);
        RESOLVE(glUniform4fv);
        RESOLVE(glUniformMatrix2fv);
        RESOLVE(glUniformMatrix3fv);
        RESOLVE(glUniformMatrix4fv);
        RESOLVE(glGetProgramiv);
        RESOLVE(glGetShaderiv);
        RESOLVE(glGetProgramInfoLog);
        RESOLVE(glGetShaderInfoLog);
        RESOLVE(glDeleteTextures);
        RESOLVE(glGenTextures);
        RESOLVE(glBindTexture);
        RESOLVE(glActiveTexture);
        RESOLVE(glGenerateMipmap);
        RESOLVE(glTexImage2D);
        RESOLVE(glTexParameteri);
        RESOLVE(glPixelStorei);
        RESOLVE(glEnable);
        RESOLVE(glDisable);
        RESOLVE(glGetIntegerv);
        RESOLVE(glReadPixels);
        RESOLVE(glLineWidth);
    #undef RESOLVE

        GLint stencil_bits = 0;
        GLint red_bits   = 0;
        GLint green_bits = 0;
        GLint blue_bits  = 0;
        GLint alpha_bits = 0;
        GLint depth_bits = 0;
        GLint point_size[2];
        GLint max_texture_units = 0;
        GL_CALL(glGetIntegerv(GL_STENCIL_BITS, &stencil_bits));
        GL_CALL(glGetIntegerv(GL_RED_BITS, &red_bits));
        GL_CALL(glGetIntegerv(GL_GREEN_BITS, &green_bits));
        GL_CALL(glGetIntegerv(GL_BLUE_BITS, &blue_bits));
        GL_CALL(glGetIntegerv(GL_ALPHA_BITS, &alpha_bits));
        GL_CALL(glGetIntegerv(GL_DEPTH_BITS, &depth_bits));
        GL_CALL(glGetIntegerv(GL_ALIASED_POINT_SIZE_RANGE, point_size));
        GL_CALL(glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units));
        DEBUG("OpenGLESGraphicsDevice");
        DEBUG("GL %1 Vendor: %2, %3",
            mGL.glGetString(GL_VERSION),
            mGL.glGetString(GL_VENDOR),
            mGL.glGetString(GL_RENDERER));

        // a little hack to provide the INFO level graphics device
        // information only once.
        static bool have_printed_info = false;
        if (!have_printed_info)
        {
            INFO("GL %1 Vendor: %2, %3",
                mGL.glGetString(GL_VERSION),
                mGL.glGetString(GL_VENDOR),
                mGL.glGetString(GL_RENDERER));
            have_printed_info = true;
        }
        DEBUG("Stencil bits: %1", stencil_bits);
        DEBUG("Red bits: %1", red_bits);
        DEBUG("Blue bits: %1", blue_bits);
        DEBUG("Green bits: %1", green_bits);
        DEBUG("Alpha bits: %1", alpha_bits);
        DEBUG("Depth bits: %1", depth_bits);
        DEBUG("Point size: %1-%2", point_size[0], point_size[1]);
        DEBUG("Fragment shader texture units: %1", max_texture_units);
        mTextureUnits.resize(max_texture_units);
    }
    OpenGLES2GraphicsDevice(std::shared_ptr<Context> context)
        : OpenGLES2GraphicsDevice(context.get())
    {
        mContextImpl = context;
    }
   ~OpenGLES2GraphicsDevice()
    {
        DEBUG("~OpenGLES2GraphicsDevice");
       // make sure our cleanup order is specific so that the
       // resources are deleted before the context is deleted.
       mTextures.clear();
       mShaders.clear();
       mPrograms.clear();
       mGeoms.clear();
    }

    virtual void ClearColor(const Color4f& color) override
    {
        GL_CALL(glClearColor(color.Red(), color.Green(), color.Blue(), color.Alpha()));
        GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
    }
    virtual void ClearStencil(int value) override
    {
        GL_CALL(glClearStencil(value));
        GL_CALL(glClear(GL_STENCIL_BUFFER_BIT));
    }

    virtual void SetDefaultTextureFilter(MinFilter filter)
    {
        mDefaultMinTextureFilter = filter;
    }
    virtual void SetDefaultTextureFilter(MagFilter filter)
    {
        mDefaultMagTextureFilter = filter;
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
        auto shader = std::make_unique<ShaderImpl>(mGL);
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
        auto program = std::make_unique<ProgImpl>(mGL);
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
        auto geometry = std::make_unique<GeomImpl>(mGL);
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
        auto texture = std::make_unique<TextureImpl>(mGL);
        auto* ret = texture.get();
        mTextures[name] = std::move(texture);
        return ret;
    }

    virtual void DeleteShaders() override
    {
        mShaders.clear();
    }
    virtual void DeletePrograms() override
    {
        mPrograms.clear();
    }
    virtual void DeleteGeometries() override
    {
        mGeoms.clear();
    }
    virtual void DeleteTextures() override
    {
        mTextures.clear();
    }

    virtual void Draw(const Program& program, const Geometry& geometry, const State& state) override
    {
        SetState(state);

        GLenum default_texture_min_filter = GL_NONE;
        GLenum default_texture_mag_filter = GL_NONE;
        switch (mDefaultMinTextureFilter)
        {
            case Device::MinFilter::Nearest:   default_texture_min_filter = GL_NEAREST; break;
            case Device::MinFilter::Linear:    default_texture_min_filter = GL_LINEAR;  break;
            case Device::MinFilter::Mipmap:    default_texture_min_filter = GL_LINEAR_MIPMAP_LINEAR; break;
            case Device::MinFilter::Bilinear:  default_texture_min_filter = GL_LINEAR_MIPMAP_NEAREST; break;
            case Device::MinFilter::Trilinear: default_texture_min_filter = GL_LINEAR_MIPMAP_LINEAR; break;
        }
        switch (mDefaultMagTextureFilter)
        {
            case Device::MagFilter::Nearest: default_texture_mag_filter = GL_NEAREST; break;
            case Device::MagFilter::Linear:  default_texture_mag_filter = GL_LINEAR;  break;
        }

        auto* myprog = (ProgImpl*)(&program);
        auto* mygeom = (GeomImpl*)(&geometry);
        myprog->SetLastUseFrameNumber(mFrameNumber);
        myprog->SetState(mTextureUnits, default_texture_min_filter, default_texture_mag_filter);
        mygeom->SetLastUseFrameNumber(mFrameNumber);
        mygeom->Draw(myprog->GetName());
    }

    virtual Type GetDeviceType() const override
    { return Type::OpenGL_ES2; }

    virtual void CleanGarbage(size_t max_num_idle_frames) override
    {
        /* not needed atm.
        for (auto it = mPrograms.begin(); it != mPrograms.end();)
        {
            auto* impl = static_cast<ProgImpl*>(it->second.get());
            const auto last_used_frame_number = impl->GetLastUsedFrameNumber();
            if (mFrameNumber - last_used_frame_number >= max_num_idle_frames)
                it = mPrograms.erase(it);
            else ++it;
        }
        */

        for (auto it = mTextures.begin(); it != mTextures.end();)
        {
            auto* impl = static_cast<TextureImpl*>(it->second.get());
            const auto last_used_frame_number = impl->GetLastUsedFrameNumber();
            const auto is_eligible = impl->IsEligibleForGarbageCollection();
            const auto is_expired  = mFrameNumber - last_used_frame_number >= max_num_idle_frames;
            if (is_eligible && is_expired)
            {
                size_t unit = 0;
                for (unit=0; unit<mTextureUnits.size(); ++unit)
                {
                    if (mTextureUnits[unit].texture == impl)
                    {
                        mTextureUnits[unit].texture = nullptr;
                        break;
                    }
                }
                // delete the texture
                it = mTextures.erase(it);
            }
            else ++it;
        }
    }

    virtual void BeginFrame() override
    {
        for (auto& pair : mPrograms)
        {
            auto* impl = static_cast<ProgImpl*>(pair.second.get());
            impl->BeginFrame();
        }
    }
    virtual void EndFrame(bool display) override
    {
        mFrameNumber++;
        if (display)
            mContext->Display();
    }

    virtual Bitmap<RGBA> ReadColorBuffer(unsigned width, unsigned height) const override
    {
        Bitmap<RGBA> bmp(width, height);

        GL_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
        GL_CALL(glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
            (void*)bmp.GetDataPtr()));
        // by default the scan row order is reversed to what we expect.
        bmp.FlipHorizontally();
        return bmp;
    }

private:
    bool EnableIf(GLenum flag, bool on_off)
    {
        if (on_off)
        {
            GL_CALL(glEnable(flag));
        }
        else
        {
            GL_CALL(glDisable(flag));
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
        ASSERT(!"???");
        return GL_NONE;
    }

    void SetState(const State& state)
    {
        GL_CALL(glViewport(state.viewport.GetX(), state.viewport.GetY(),
            state.viewport.GetWidth(), state.viewport.GetHeight()));

        switch (state.blending)
        {
            case State::BlendOp::None:
                {
                    GL_CALL(glDisable(GL_BLEND));
                }
                break;
            case State::BlendOp::Transparent:
                {
                    GL_CALL(glEnable(GL_BLEND));
                    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
                }
                break;
            case State::BlendOp::Additive:
                {
                    GL_CALL(glEnable(GL_BLEND));
                    GL_CALL(glBlendFunc(GL_ONE, GL_ONE));
                }
                break;
        }

        if (EnableIf(GL_STENCIL_TEST, state.stencil_func != State::StencilFunc::Disabled))
        {
            const auto stencil_func  = ToGLEnum(state.stencil_func);
            const auto stencil_fail  = ToGLEnum(state.stencil_fail);
            const auto stencil_dpass = ToGLEnum(state.stencil_dpass);
            const auto stencil_dfail = ToGLEnum(state.stencil_dfail);
            GL_CALL(glStencilFunc(stencil_func, state.stencil_ref, state.stencil_mask));
            GL_CALL(glStencilOp(stencil_fail, stencil_dfail, stencil_dpass));
        }
        if (state.bWriteColor)
        {
            GL_CALL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
        }
        else
        {
            GL_CALL(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));
        }
    }

private:
    class TextureImpl;
    struct TextureUnit {
        const TextureImpl* texture = nullptr;
        Texture::MinFilter min_filter = Texture::MinFilter::Nearest;
        Texture::MagFilter mag_filter = Texture::MagFilter::Nearest;
        Texture::Wrapping  wrap_x     = Texture::Wrapping::Clamp;
        Texture::Wrapping  wrap_y     = Texture::Wrapping::Clamp;
    };

    using TextureUnits = std::vector<TextureUnit>;

    class TextureImpl : public Texture
    {
    public:
        TextureImpl(const OpenGLFunctions& funcs) : mGL(funcs)
        {
            GL_CALL(glGenTextures(1, &mName));
            DEBUG("New texture object %1 name = %2", (void*)this, mName);
        }
        ~TextureImpl()
        {
            GL_CALL(glDeleteTextures(1, &mName));
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
                case Format::Grayscale:
                    // when sampling R = G = B = 0.0 and A is the alpha value from here.
                    sizeFormat = GL_ALPHA;
                    baseFormat = GL_ALPHA;
                    break;
                default: assert(!"unknown texture format."); break;
            }

            GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
            GL_CALL(glActiveTexture(GL_TEXTURE0));

            // In order to upload the texture data we must bind the texture
            // to a texture unit. we must make sure not to overwrite any other
            // texture objects that might be bound to the unit. for example
            // if the rendering code is running a loop where it's first creating
            // texture objects if they don't exist and then setting them to a
            // sampler (which will do a bind). Then going another iteration of this
            // loop would overwrite the texture from before. Oooops!
            GLint current_texture = 0;
            GL_CALL(glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture));
            // bind our texture here.
            GL_CALL(glBindTexture(GL_TEXTURE_2D, mName));
            GL_CALL(glTexImage2D(GL_TEXTURE_2D,
                0, // mip level
                sizeFormat,
                xres,
                yres,
                0, // border must be 0
                baseFormat,
                GL_UNSIGNED_BYTE,
                bytes));
            GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
            mWidth  = xres;
            mHeight = yres;
            mFormat = format;

            // restore previous texture.
            GL_CALL(glBindTexture(GL_TEXTURE_2D, current_texture));
        }

        // refer actual state setting to the point when
        // when the texture is actually used in a program's
        // sampler
        virtual void SetFilter(MinFilter filter) override
        { mMinFilter = filter; }
        virtual void SetFilter(MagFilter filter) override
        { mMagFilter = filter; }
        virtual void SetWrapX(Wrapping w) override
        { mWrapX = w; }
        virtual void SetWrapY(Wrapping w) override
        { mWrapY = w; }

        virtual Texture::MinFilter GetMinFilter() const override
        { return mMinFilter; }
        virtual Texture::MagFilter GetMagFilter() const override
        { return mMagFilter; }
        virtual Texture::Wrapping GetWrapX() const override
        { return mWrapX; }
        virtual Texture::Wrapping GetWrapY() const override
        { return mWrapY; }
        virtual unsigned GetWidth() const override
        { return mWidth; }
        virtual unsigned GetHeight() const override
        { return mHeight; }
        virtual Texture::Format GetFormat() const override
        { return mFormat; }
        virtual void EnableGarbageCollection(bool gc) override
        { mEnableGC = gc; }

        // internal
        GLuint GetName() const
        { return mName; }

        void SetLastUseFrameNumber(size_t frame_number)
        { mFrameNumber = frame_number; }

        size_t GetLastUsedFrameNumber() const
        { return mFrameNumber; }

        bool IsEligibleForGarbageCollection() const
        { return mEnableGC; }

    private:
        const OpenGLFunctions& mGL;

        GLuint mName = 0;
    private:
        MinFilter mMinFilter = MinFilter::Default;
        MagFilter mMagFilter = MagFilter::Default;
        Wrapping mWrapX = Wrapping::Repeat;
        Wrapping mWrapY = Wrapping::Repeat;
    private:
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
        Format mFormat = Texture::Format::Grayscale;
        std::size_t mFrameNumber = 0;
        bool mEnableGC = false;
    };

    class GeomImpl : public Geometry
    {
    public:
        GeomImpl(const OpenGLFunctions& funcs) : mGL(funcs)
        {}
        virtual void ClearDraws() override
        {
            mDrawCommands.clear();
        }
        virtual void AddDrawCmd(DrawType type) override
        {
            DrawCommand cmd;
            cmd.type = type;
            cmd.offset = 0;
            cmd.count  = std::numeric_limits<size_t>::max();
            mDrawCommands.push_back(cmd);
        }

        virtual void AddDrawCmd(DrawType type, size_t offset, size_t count) override
        {
            DrawCommand cmd;
            cmd.type   = type;
            cmd.offset = offset;
            cmd.count  = count;
            mDrawCommands.push_back(cmd);
        }

        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }

        virtual void Update(const Vertex* verts, std::size_t count) override
        {
            mData.clear();
            mData.resize(count);
            for (size_t i=0; i<count; ++i)
            {
               mData[i] = verts[i];
            }
        }
        virtual void Update(const std::vector<Vertex>& verts) override
        {
            mData = verts;
        }
        virtual void Update(std::vector<Vertex>&& verts) override
        {
            mData = std::move(verts);
        }

        void Draw(GLuint program)
        {
            GLint aPosition = mGL.glGetAttribLocation(program, "aPosition");
            GLint aTexCoord = mGL.glGetAttribLocation(program, "aTexCoord");
            const uint8_t* base = reinterpret_cast<const uint8_t*>(&mData[0]);
            if (aPosition != -1)
            {
                GL_CALL(glVertexAttribPointer(aPosition, 2, GL_FLOAT, GL_FALSE,
                    sizeof(Vertex), (void*)(base + offsetof(Vertex, aPosition))));
                GL_CALL(glEnableVertexAttribArray(aPosition));
            }
            if (aTexCoord != -1)
            {
                GL_CALL(glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE,
                    sizeof(Vertex), (void*)(base + offsetof(Vertex, aTexCoord))));
                GL_CALL(glEnableVertexAttribArray(aTexCoord));
            }

            for (const auto& draw : mDrawCommands)
            {
                const auto count  = draw.count == std::numeric_limits<std::size_t>::max() ? mData.size() : draw.count;
                const auto type   = draw.type;
                const auto offset = (GLsizei)draw.offset;

                if (type == DrawType::Triangles)
                    GL_CALL(glDrawArrays(GL_TRIANGLES, offset, count));
                else if (type == DrawType::Points)
                    GL_CALL(glDrawArrays(GL_POINTS, offset, count));
                else if (type == DrawType::TriangleFan)
                    GL_CALL(glDrawArrays(GL_TRIANGLE_FAN, offset, count));
                else if (type == DrawType::Lines)
                {
                    GL_CALL(glLineWidth(mLineWidth));
                    GL_CALL(glDrawArrays(GL_LINES, offset, count));
                }
                else if (type == DrawType::LineLoop)
                {
                    GL_CALL(glLineWidth(mLineWidth));
                    GL_CALL(glDrawArrays(GL_LINE_LOOP, offset, count));
                }
            }
        }
        void SetLastUseFrameNumber(size_t frame_number)
        { mFrameNumber = frame_number; }

    private:
        struct DrawCommand {
            DrawType type = DrawType::Triangles;
            size_t count  = 0;
            size_t offset = 0;
        };
    private:
        const OpenGLFunctions& mGL;
        std::size_t mFrameNumber = 0;
        std::vector<DrawCommand> mDrawCommands;
        std::vector<Vertex> mData;
        float mLineWidth = 1.0f;
    };

    class ProgImpl : public Program
    {
    public:
        ProgImpl(const OpenGLFunctions& funcs) : mGL(funcs)
        {}

       ~ProgImpl()
        {
            if (mProgram)
            {
                GL_CALL(glDeleteProgram(mProgram));
            }
        }
        virtual bool Build(const std::vector<const Shader*>& shaders) override
        {
            GLuint prog = mGL.glCreateProgram();
            DEBUG("New program %1", prog);

            for (const auto* shader : shaders)
            {
                ASSERT(shader->IsValid());
                GL_CALL(glAttachShader(prog,
                    static_cast<const ShaderImpl*>(shader)->GetName()));
            }
            GL_CALL(glLinkProgram(prog));
            GL_CALL(glValidateProgram(prog));

            GLint link_status = 0;
            GLint valid_status = 0;
            GL_CALL(glGetProgramiv(prog, GL_LINK_STATUS, &link_status));
            GL_CALL(glGetProgramiv(prog, GL_VALIDATE_STATUS, &valid_status));

            GLint length = 0;
            GL_CALL(glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &length));

            std::string build_info;
            build_info.resize(length);
            GL_CALL(glGetProgramInfoLog(prog, length, nullptr, &build_info[0]));

            if ((link_status == 0) || (valid_status == 0))
            {
                ERROR("Program build error: %1", build_info);
                GL_CALL(glDeleteProgram(prog));
                return false;
            }

            DEBUG("Program was built succesfully!");
            DEBUG("Program info: %1", build_info);
            if (mProgram)
            {
                GL_CALL(glDeleteProgram(mProgram));
                GL_CALL(glUseProgram(0));
            }
            mProgram = prog;
            mVersion++;
            return true;
        }
        virtual bool IsValid() const override
        { return mProgram != 0; }

        virtual void SetUniform(const char* name, int x) override
        {
            auto& ret = GetUniform(name);
            if (ret.location == -1)
                return;
            uint32_t hash = 0;
            hash = base::hash_combine(hash, x);
            if (hash != ret.hash)
            {
                GL_CALL(glUseProgram(mProgram));
                GL_CALL(glUniform1i(ret.location, x));
                ret.hash = hash;
            }
        }
        virtual void SetUniform(const char* name, int x, int y) override
        {
            auto& ret = GetUniform(name);
            if (ret.location == -1)
                return;

            uint32_t hash = 0;
            hash = base::hash_combine(hash, x);
            hash = base::hash_combine(hash, y);
            if (hash != ret.hash)
            {
                GL_CALL(glUseProgram(mProgram));
                GL_CALL(glUniform2i(ret.location, x, y));
                ret.hash = hash;
            }
        }

        virtual void SetUniform(const char* name, float x) override
        {
            auto& ret = GetUniform(name);
            if (ret.location == -1)
                return;
            uint32_t hash = 0;
            hash = base::hash_combine(hash, x);
            if (ret.hash != hash)
            {
                GL_CALL(glUseProgram(mProgram));
                GL_CALL(glUniform1f(ret.location, x));
                ret.hash = hash;
            }
        }
        virtual void SetUniform(const char* name, float x, float y) override
        {
            auto& ret = GetUniform(name);
            if (ret.location == -1)
                return;

            uint32_t hash = 0;
            hash = base::hash_combine(hash, x);
            hash = base::hash_combine(hash, y);
            if (hash != ret.hash)
            {
                GL_CALL(glUseProgram(mProgram));
                GL_CALL(glUniform2f(ret.location, x, y));
                ret.hash = hash;
            }
        }
        virtual void SetUniform(const char* name, float x, float y, float z) override
        {
            auto& ret = GetUniform(name);
            if (ret.location == -1)
                return;

            uint32_t hash = 0;
            hash = base::hash_combine(hash, x);
            hash = base::hash_combine(hash, y);
            hash = base::hash_combine(hash, z);
            if (hash != ret.hash)
            {
                GL_CALL(glUseProgram(mProgram));
                GL_CALL(glUniform3f(ret.location, x, y, z));
                ret.hash = hash;
            }
        }
        virtual void SetUniform(const char* name, float x, float y, float z, float w) override
        {
            auto& ret = GetUniform(name);
            if (ret.location == -1)
                return;

            uint32_t hash = 0;
            hash = base::hash_combine(hash, x);
            hash = base::hash_combine(hash, y);
            hash = base::hash_combine(hash, z);
            hash = base::hash_combine(hash, w);
            if (hash != ret.hash)
            {
                GL_CALL(glUseProgram(mProgram));
                GL_CALL(glUniform4f(ret.location, x, y, z, w));
                ret.hash = hash;
            }
        }
        virtual void SetUniform(const char* name, const Color4f& color) override
        {
            auto& ret = GetUniform(name);
            if (ret.location == -1)
                return;
            const auto hash = color.GetHash();
            if (ret.hash != hash)
            {
                GL_CALL(glUseProgram(mProgram));
                GL_CALL(glUniform4f(ret.location, color.Red(), color.Green(), color.Blue(), color.Alpha()));
                ret.hash = hash;
            }
        }
        virtual void SetUniform(const char* name, const Matrix2x2& matrix) override
        {
            auto& ret = GetUniform(name);
            if (ret.location == -1)
                return;

            uint32_t hash = 0;
            const auto ptr = (const float*)&matrix;
            const auto len  = sizeof(matrix) / sizeof(float);
            for (size_t i=0; i<len; ++i)
                hash = base::hash_combine(hash, ptr[i]);
            if (ret.hash != hash)
            {
                GL_CALL(glUseProgram(mProgram));
                GL_CALL(glUniformMatrix2fv(ret.location, 1, GL_FALSE /* transpose */, (const float*)&matrix));
                ret.hash = hash;
            }
        }
        virtual void SetUniform(const char* name, const Matrix3x3& matrix) override
        {
            auto& ret = GetUniform(name);
            if (ret.location == -1)
                return;

            uint32_t hash = 0;
            const auto ptr = (const float*)&matrix;
            const auto len  = sizeof(matrix) / sizeof(float);
            for (size_t i=0; i<len; ++i)
                hash = base::hash_combine(hash, ptr[i]);

            if (ret.hash != hash)
            {
                GL_CALL(glUseProgram(mProgram));
                GL_CALL(glUniformMatrix3fv(ret.location, 1, GL_FALSE /*transpose*/, (const float*)&matrix));
                ret.hash = hash;
            }
        }
        virtual void SetUniform(const char* name, const Matrix4x4& matrix) override
        {
            auto& ret = GetUniform(name);
            if (ret.location == -1)
                return;

            uint32_t hash = 0;
            const auto ptr = (const float*)&matrix;
            const auto len  = sizeof(matrix) / sizeof(float);
            for (size_t i=0; i<len; ++i)
                hash = base::hash_combine(hash, ptr[i]);

            if (ret.hash != hash)
            {
                GL_CALL(glUseProgram(mProgram));
                GL_CALL(glUniformMatrix4fv(ret.location, 1, GL_FALSE /*transpose*/, (const float*)&matrix));
                ret.hash = hash;
            }
        }

        virtual void SetTexture(const char* sampler, unsigned unit, const Texture& texture) override
        {
            auto ret =  GetUniform(sampler);
            if (ret.location == -1)
                return;

            const auto* impl = static_cast<const TextureImpl*>(&texture);

            // in OpenGL the expected memory layout of texture data that
            // is given to glTexImage2D doesn't match the "typical" layout
            // that is used by many toolkits/libraries. The order of scan lines
            // is reversed and glTexImage expects the first scanline (in memory)
            // to be the bottom scanline of the image.
            // There are several ways to deal with this dilemma:
            // - flip all images on the horizontal axis before calling glTexImage2D.
            // - use a matrix to transform the texture coordinates to counter this.
            // - flip texture coordinates and assume that Y=0.0f means the top of
            //   the texture (first scan row) and Y=1.0f means the bottom of the
            //   the texture (last scan row).
            //
            //
            // this comment and the below kDeviceTextureMatrix is here only for posterity.
            // Currently we have flipped the texture coordinates like explained above.
            //
            // If the program being used to render stuff is using a texture
            // we helpfully setup here a "device texture matrix" that will be provided
            // for the program and should be used to transform the texture coordinates
            // before sampling textures.
            // This will avoid having to do any image flips which is especilly
            // handy when dealing with data that gets re-uploaded often
            // I.e. dynamically changing/procedural texture data.
            //
            // It should also be possible to use the device texture matrix for example
            // in cases where the device would bake some often used textures into an atlas
            // and implicitly alter the texture coordinates.
            //
            /* no longer used.
            static const float kDeviceTextureMatrix[3][3] = {
                {1.0f, 0.0f, 0.0f},
                {0.0f, -1.0f, 0.0f},
                {0.0f, 1.0f, 0.0f}
            };
            SetUniform("kDeviceTextureMatrix", kDeviceTextureMatrix);
            */

            if (unit >= mTextures.size())
                mTextures.resize(unit + 1);

            // keep track of textures being used so that if/when this
            // program is actually used to draw stuff we can realize
            // which textures will actually be used to draw and do the
            // texture binds.
            mTextures[unit].texture  = const_cast<TextureImpl*>(impl);
            mTextures[unit].location = ret.location;
        }

        void SetState(TextureUnits& units,
            GLenum default_texture_min_filter, GLenum default_texture_mag_filter) //const
        {
            GL_CALL(glUseProgram(mProgram));

            size_t num_textures = mTextures.size();
            if (num_textures > units.size())
            {
                WARN("Program uses more textures than there are units available.");
                num_textures = units.size();
            }
            // for all textures used by this draw, look if the texture
            // is already bound to some unit. if it is already bound
            // and texture parameters haven't changed then nothing needs to be
            // done. Otherwise see if there's a free texture slot and bind
            // if there or lastly "evict" some texture from some unit and overwrite
            // with new texture.
            size_t unit = 0;

            for (size_t i=0; i<num_textures; ++i)
            {
                const TextureImpl* texture = mTextures[i].texture;

                // see if there's already a unit that has this texture bound.
                // if so, we use the same unit.
                for (unit=0; unit < units.size(); ++unit)
                {
                    if (units[unit].texture == texture)
                        break;
                }
                if (unit == units.size())
                {
                    // look for a first free unit if any.
                    for (unit=0; unit<units.size(); ++unit)
                    {
                        if (units[unit].texture == nullptr)
                            break;
                    }
                }
                if (unit == units.size())
                {
                    size_t last_used_frame_number = units[0].texture->GetLastUsedFrameNumber();
                    unit = 0;
                    // look for a unit we can reuse
                    for (size_t i=0; i<units.size(); ++i)
                    {
                        const auto* bound_texture = units[i].texture;
                        if (bound_texture->GetLastUsedFrameNumber() < last_used_frame_number)
                        {
                            unit = i;
                        }
                    }
                }
                ASSERT(unit < units.size());

                // if nothing has changed then skip all of the work
                if (units[unit].texture    == texture &&
                    units[unit].min_filter == texture->GetMinFilter() &&
                    units[unit].mag_filter == texture->GetMagFilter() &&
                    units[unit].wrap_x     == texture->GetWrapX() &&
                    units[unit].wrap_y     == texture->GetWrapY())
                {
                    // set the teture unit to the sampler
                    GL_CALL(glUniform1i(mTextures[i].location, unit));
                    continue;
                }

                GLuint texture_name = texture->GetName();
                GLenum texture_min_filter = GL_NONE;
                GLenum texture_mag_filter = GL_NONE;
                switch (texture->GetMinFilter())
                {
                    case Texture::MinFilter::Default:
                        texture_min_filter = default_texture_min_filter;
                        break;
                    case Texture::MinFilter::Nearest:
                        texture_min_filter = GL_NEAREST;
                        break;
                    case Texture::MinFilter::Linear:
                        texture_min_filter = GL_LINEAR;
                        break;
                    case Texture::MinFilter::Mipmap:
                        texture_min_filter = GL_LINEAR_MIPMAP_LINEAR;
                        break;
                    case Texture::MinFilter::Bilinear:
                        texture_min_filter = GL_LINEAR_MIPMAP_NEAREST;
                        break;
                    case Texture::MinFilter::Trilinear:
                        texture_min_filter = GL_LINEAR_MIPMAP_LINEAR;
                        break;
                }
                switch (texture->GetMagFilter())
                {
                    case Texture::MagFilter::Default:
                        texture_mag_filter = default_texture_mag_filter;
                        break;
                    case Texture::MagFilter::Nearest:
                        texture_mag_filter = GL_NEAREST;
                        break;
                    case Texture::MagFilter::Linear:
                        texture_mag_filter = GL_LINEAR;
                        break;
                }

                // set all this fucking state here, so we can easily track/understand
                // which unit the texture is bound to.

                // // first select the desired texture unit.
                GL_CALL(glActiveTexture(GL_TEXTURE0 + unit));
                // bind the 2D texture.
                GL_CALL(glBindTexture(GL_TEXTURE_2D, texture_name));
                // set texture parameters, wrapping and min/mag filters.
                GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    texture->GetWrapX() == Texture::Wrapping::Clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT));
                GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    texture->GetWrapY() == Texture::Wrapping::Clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT));
                GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_mag_filter));
                GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_min_filter));
                // set the teture unit to the sampler
                GL_CALL(glUniform1i(mTextures[i].location, unit));

                // store current binding.
                units[unit].texture = texture;
                units[unit].min_filter = texture->GetMinFilter();
                units[unit].mag_filter = texture->GetMagFilter();
                units[unit].wrap_x = texture->GetWrapX();
                units[unit].wrap_y = texture->GetWrapY();
            }
        }
        GLuint GetName() const
        { return mProgram; }

        void SetLastUseFrameNumber(size_t frame_number)
        {
            mFrameNumber = frame_number;
            for (auto& it : mTextures)
            {
                it.texture->SetLastUseFrameNumber(frame_number);
            }
        }
        void BeginFrame()
        {
            // this clear has some unfortunate consequences.
            // If we don't clear then we're holding onto some texture object
            // and either a) it cannot be garbage collected or
            // b) it is garbage collected and we have a dandling pointer.
            // However doing this clear means that the program cannot be
            // used across frame's without having it's state reset.
            mTextures.clear();
        }
        size_t GetLastUsedFrameNumber() const
        { return mFrameNumber; }

    private:
        struct Uniform {
            int location = 0;
            uint32_t hash = 0;
        };
        std::unordered_map<std::string, Uniform> mUniforms;

        Uniform& GetUniform(const char* name)
        {
            auto it = mUniforms.find(name);
            if (it != std::end(mUniforms))
                return it->second;

            auto ret = mGL.glGetUniformLocation(mProgram, name);
            Uniform u;
            u.location = ret;
            mUniforms[name] = u;
            return mUniforms[name];
        }

    private:
        const OpenGLFunctions& mGL;
        GLuint mProgram = 0;
        GLuint mVersion = 0;
        struct Sampler {
            GLuint location = 0;
            TextureImpl* texture = nullptr;
        };
        std::vector<Sampler> mTextures;
        std::size_t mFrameNumber = 0;
    };

    class ShaderImpl : public Shader
    {
    public:
        ShaderImpl(const OpenGLFunctions& funcs) : mGL(funcs)
        {}

       ~ShaderImpl()
        {
            if (mShader)
            {
                GL_CALL(glDeleteShader(mShader));
            }
        }
        virtual bool CompileFile(const std::string& file) override
        {
            //                  == Notes about shaders ==
            // 1. Shaders are specific to a device within compatibility constraints
            // but a GLES 2 device context cannot use GL ES 3 shaders for example.
            //
            // 2. Logically shaders are part of the higher level graphics system, i.e.
            // they express some particular algorithm/technique in a device dependant
            // way, so logically they belong to the higher layer (i.e. roughly painter
            // material level of abstraction).
            //
            // Basically this means that the device abstraction breaks down because
            // we need this device specific (shader) code that belongs to the higher
            // level of the system.
            //
            // A fancy way of solving this would be to use a shader translator, i.e.
            // a device that can transform shader from one language version to another
            // language version or even from one shader language to another, for example
            // from GLSL to HLSL/MSL/SPIR-V.  In fact this is exactly what libANGLE does
            // when it implements GL ES2/3 on top of DX9/11/Vulkan/GL/Metal.
            // But this only works when the particular underlying implementations can
            // support similar feature sets. For example instanced rendering is not part
            // of ES 2 but >= ES 3 so therefore it's not possible to translate ES3 shaders
            // using instanced rendering into ES2 shaders directly but the whole rendering
            // path must be changed to not use instanced rendering.
            //
            // This then means that if a system was using a shader translation layer (such
            // as libANGLE) any decent system would still require several different endering
            // paths. For example a primary "high end path" and a "fallback path" for low end
            // devices. These different paths would use different feature sets (such as instanced
            // rendering) and also (in many cases) require different shaders to be written to
            // fully take advantage of the graphics API features.
            // Then these two different rendering paths would/could use a shader translation
            // layer in order to use some specific graphcis API. (through libANGLE)
            //
            //            <<Device>>
            //              |    |
            //     <ES2 Device> <ES3 Device>
            //             |      |
            //            <libANGLE>
            //                 |
            //      [GL/DX9/DX11/VULKAN/Metal]
            //
            // Where "ES2 Device" provides the low end graphics support and "ES3 Device"
            // device provides the "high end" graphics rendering path support.
            // Both device implementations would require their own shaders which would then need
            // to be translated to the device specific shaders matching the feature level.
            //
            // Finally, who should own the shaders and who should edit them ?
            //   a) The person who is using the graphics library ?
            //   b) The person who is writing the graphics library ?
            //
            // The answer seems to be mostly the latter (b), i.e. in most cases the
            // graphics functionality should work "out of the box" and the graphics
            // library should *just work* without the user having to write shaders.
            // However there's a need that user might want to write their own special
            // shaders because they're cool to do so and want some special customized
            // shader effect.
            //
            const auto& mapped_file = ResolveFile(ResourceLoader::ResourceType::Shader, file);

            std::ifstream stream;
            stream.open(mapped_file);
            if (!stream.is_open())
            {
                ERROR("Failed to open shader file: '%1'", mapped_file);
                return false;
            }
            const std::string source(std::istreambuf_iterator<char>(stream), {});

            if (!CompileSource(source))
            {
                ERROR("Failed to compile shader source file: '%1'", mapped_file);
                return false;
            }
            return true;
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
            if (type == GL_NONE)
            {
                ERROR("Failed to identify shader type.");
                return false;
            }

            GLint status = 0;
            GLint shader = mGL.glCreateShader(type);
            DEBUG("New shader %1 %2", shader, GLEnumToStr(type));

            const char* source_ptr = source.c_str();
            GL_CALL(glShaderSource(shader, 1, &source_ptr, nullptr));
            GL_CALL(glCompileShader(shader));
            GL_CALL(glGetShaderiv(shader, GL_COMPILE_STATUS, &status));

            GLint length = 0;
            GL_CALL(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length));

            std::string compile_info;
            compile_info.resize(length);
            GL_CALL(glGetShaderInfoLog(shader, length, nullptr, &compile_info[0]));

            if (status == 0)
            {
                GL_CALL(glDeleteShader(shader));
                ERROR("Shader compile error %1", compile_info);
                return false;
            }
            else
            {
                DEBUG("Shader was built succesfully!");
                if (!compile_info.empty())
                    INFO("Shader info: %1", compile_info);
            }

            if (mShader)
            {
                GL_CALL(glDeleteShader(mShader));
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
        const OpenGLFunctions& mGL;

    private:
        GLuint mShader  = 0;
        GLuint mVersion = 0;
    };
private:
    std::map<std::string, std::unique_ptr<Geometry>> mGeoms;
    std::map<std::string, std::unique_ptr<Shader>> mShaders;
    std::map<std::string, std::unique_ptr<Program>> mPrograms;
    std::map<std::string, std::unique_ptr<Texture>> mTextures;
    std::shared_ptr<Context> mContextImpl;
    Context* mContext = nullptr;
    std::size_t mFrameNumber = 0;
    OpenGLFunctions mGL;
    MinFilter mDefaultMinTextureFilter = MinFilter::Nearest;
    MagFilter mDefaultMagTextureFilter = MagFilter::Nearest;
    // texture units and their current settings.
    TextureUnits mTextureUnits;
};

// static
std::shared_ptr<Device> Device::Create(Type type,
    std::shared_ptr<Device::Context> context)
{
    return std::make_shared<OpenGLES2GraphicsDevice>(context);
}
// static
std::shared_ptr<Device> Device::Create(Type type, Device::Context* context)
{
    return std::make_shared<OpenGLES2GraphicsDevice>(context);
}

} // namespace
