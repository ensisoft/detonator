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

#if defined(GFX_ENABLE_DEVICE_TRACING)
#  define BASE_TRACING_ENABLE_TRACING
# else
#  undef BASE_TRACING_ENABLE_TRACING
#endif

#include <GLES2/gl2.h>

#include <cstdio>
#include <cassert>
#include <cstring> // for memcpy
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <unordered_map>

#include "base/assert.h"
#include "base/logging.h"
#include "base/hash.h"
#include "base/utility.h"
#include "base/trace.h"
#include "graphics/resource.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/device.h"
#include "graphics/geometry.h"
#include "graphics/texture.h"
#include "graphics/color4f.h"
#include "graphics/loader.h"
#include "graphics/framebuffer.h"

// WebGL
#define WEBGL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define WEBGL_DEPTH_STENCIL            0x84F9

// EXT_sRGB
// https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_sRGB.txt
// Accepted by the <format> and <internalformat> parameter of TexImage2D, and
// TexImage3DOES.  These are also accepted by <format> parameter of
// TexSubImage2D and TexSubImage3DOES:
#define GL_SRGB_EXT                                       0x8C40
#define GL_SRGB_ALPHA_EXT                                 0x8C42
// Accepted by the <internalformat> parameter of RenderbufferStorage:
#define GL_SRGB8_ALPHA8_EXT                               0x8C43
// Accepted by the <pname> parameter of GetFramebufferAttachmentParameteriv:
#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT      0x8210

// OES_packed_depth_stencil
// https://registry.khronos.org/OpenGL/extensions/OES/OES_packed_depth_stencil.txt
// Accepted by the <format> parameter of TexImage2D and TexSubImage2D and by the
// <internalformat> parameter of TexImage2D:
#define GL_DEPTH_STENCIL_OES                              0x84F9
// Accepted by the <type> parameter of TexImage2D and TexSubImage2D:
#define GL_UNSIGNED_INT_24_8_OES                          0x84FA
// Accepted by the <internalformat> parameter of RenderbufferStorage, and
// returned in the <params> parameter of GetRenderbufferParameteriv when
// <pname> is RENDERBUFFER_INTERNAL_FORMAT:
#define GL_DEPTH24_STENCIL8_OES                           0x88F0

// KHR_debug
typedef void (GL_APIENTRY *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
typedef  void (GL_APIENTRY *PFNGLDEBUGMESSAGECALLBACKPROC)(GLDEBUGPROC proc, const void* user);
// https://registry.khronos.org/OpenGL/extensions/KHR/KHR_debug.txt
// Tokens accepted by the <target> parameters of Enable, Disable, and IsEnabled:
#define GL_DEBUG_OUTPUT_KHR                                     0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR                         0x8242
// Returned by GetIntegerv when <pname> is CONTEXT_FLAGS:
#define GL_CONTEXT_FLAG_DEBUG_BIT_KHR                           0x00000002
// Tokens accepted by the <value> parameters of GetBooleanv, GetIntegerv,
// GetFloatv, GetDoublev and GetInteger64v:
#define GL_MAX_DEBUG_MESSAGE_LENGTH_KHR                         0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES_KHR                        0x9144
#define GL_DEBUG_LOGGED_MESSAGES_KHR                            0x9145
#define GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH_KHR                 0x8243
#define GL_MAX_DEBUG_GROUP_STACK_DEPTH_KHR                      0x826C
#define GL_DEBUG_GROUP_STACK_DEPTH_KHR                          0x826D
#define GL_MAX_LABEL_LENGTH_KHR                                 0x82E8
// Tokens accepted by the <pname> parameter of GetPointerv:
#define GL_DEBUG_CALLBACK_FUNCTION_KHR                          0x8244
#define GL_DEBUG_CALLBACK_USER_PARAM_KHR                        0x8245
// Tokens accepted or provided by the <source> parameters of
// DebugMessageControl, DebugMessageInsert and DEBUGPROC, and the <sources>
// parameter of GetDebugMessageLog (some commands restrict <source> to a
// subset of these parameters; see the specification body for details):
#define GL_DEBUG_SOURCE_API_KHR                                 0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM_KHR                       0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER_KHR                     0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY_KHR                         0x8249
#define GL_DEBUG_SOURCE_APPLICATION_KHR                         0x824A
#define GL_DEBUG_SOURCE_OTHER_KHR                               0x824B
// Tokens accepted or provided by the <type> parameters of
// DebugMessageControl, DebugMessageInsert and DEBUGPROC, and the <types>
// parameter of GetDebugMessageLog:
#define GL_DEBUG_TYPE_ERROR_KHR                                 0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR                   0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR                    0x824E
#define GL_DEBUG_TYPE_PORTABILITY_KHR                           0x824F
#define GL_DEBUG_TYPE_PERFORMANCE_KHR                           0x8250
#define GL_DEBUG_TYPE_OTHER_KHR                                 0x8251
#define GL_DEBUG_TYPE_MARKER_KHR                                0x8268
// Tokens accepted or provided by the <type> parameters of
// DebugMessageControl and DEBUGPROC, and the <types> parameter of
// GetDebugMessageLog:
#define GL_DEBUG_TYPE_PUSH_GROUP_KHR                            0x8269
#define GL_DEBUG_TYPE_POP_GROUP_KHR                             0x826A
// Tokens accepted or provided by the <severity> parameters of
// DebugMessageControl, DebugMessageInsert and DEBUGPROC callback functions,
// and the <severities> parameter of GetDebugMessageLog:
#define GL_DEBUG_SEVERITY_HIGH_KHR                              0x9146
#define GL_DEBUG_SEVERITY_MEDIUM_KHR                            0x9147
#define GL_DEBUG_SEVERITY_LOW_KHR                               0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION_KHR                      0x826B
// Returned by GetError:
#define GL_STACK_UNDERFLOW_KHR                                  0x0504
#define GL_STACK_OVERFLOW_KHR                                   0x0503
// Tokens accepted or provided by the <identifier> parameters of
// ObjectLabel and GetObjectLabel:
#define GL_BUFFER_KHR                                           0x82E0
#define GL_SHADER_KHR                                           0x82E1
#define GL_PROGRAM_KHR                                          0x82E2
// #define VERTEX_ARRAY_KHR                                  ????
#define GL_QUERY_KHR                                            0x82E3
#define GL_PROGRAM_PIPELINE_KHR                                 0x82E4
// #define TRANSFORM_FEEDBACK_KHR                            ????
#define GL_SAMPLER_KHR                                          0x82E6
// #define TEXTURE_KHR                                       ????
// #define RENDERBUFFER_KHR                                  ????
// #define FRAMEBUFFER_KHR                                   ????

#if defined(WEBGL)
#  define GL_CALL(x) mGL.x
#else
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
#endif

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
        CASE(GL_FRAMEBUFFER_COMPLETE);
        CASE(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
        CASE(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
        CASE(GL_FRAMEBUFFER_UNSUPPORTED);
        CASE(GL_FRAGMENT_SHADER);
        CASE(GL_VERTEX_SHADER);
        CASE(GL_DEPTH_COMPONENT16);
        CASE(GL_RGBA4);
        CASE(GL_STENCIL_INDEX8);
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

void GL_APIENTRY DebugCallback(GLenum source,
                               GLenum type,
                               GLuint id,
                               GLenum severity,
                               GLsizei length, const GLchar* message,
                               const void* user)
{
    if (type == GL_DEBUG_TYPE_PERFORMANCE_KHR)
        WARN("GL perf warning. %1", message);
    else if (type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR)
        WARN("GL deprecated behaviour detected. %1", message);
    else if (type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR)
        WARN("GL undefined behavior detected. %1", message);
    else if (type == GL_DEBUG_TYPE_ERROR_KHR)
        ERROR("GL error detected. %1", message);
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
    PFNGLCLEARDEPTHFPROC             glClearDepthf;
    PFNGLCLEARPROC                   glClear;
    PFNGLCLEARSTENCILPROC            glClearStencil;
    PFNGLBLENDFUNCPROC               glBlendFunc;
    PFNGLDEPTHFUNCPROC               glDepthFunc;
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
    PFNGLSCISSORPROC                 glScissor;
    PFNGLCULLFACEPROC                glCullFace;
    PFNGLFRONTFACEPROC               glFrontFace;
    PFNGLGENBUFFERSPROC              glGenBuffers;
    PFNGLDELETEBUFFERSPROC           glDeleteBuffers;
    PFNGLBINDBUFFERPROC              glBindBuffer;
    PFNGLBUFFERDATAPROC              glBufferData;
    PFNGLBUFFERSUBDATAPROC           glBufferSubData;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
    PFNGLGENFRAMEBUFFERSPROC         glGenFramebuffers;
    PFNGLDELETEFRAMEBUFFERSPROC      glDeleteFramebuffers;
    PFNGLBINDFRAMEBUFFERPROC         glBindFramebuffer;
    PFNGLGENRENDERBUFFERSPROC        glGenRenderbuffers;
    PFNGLDELETERENDERBUFFERSPROC     glDeleteRenderbuffers;
    PFNGLBINDRENDERBUFFERPROC        glBindRenderbuffer;
    PFNGLRENDERBUFFERSTORAGEPROC     glRenderbufferStorage;
    PFNGLFRAMEBUFFERTEXTURE2DPROC    glFramebufferTexture2D;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC  glCheckFramebufferStatus;

    // KHR_debug
    PFNGLDEBUGMESSAGECALLBACKPROC    glDebugMessageCallback;
};

//
// OpenGL ES 2.0 based custom graphics device implementation
// try to keep this implementation free of Qt in
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
        RESOLVE(glClearDepthf);
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
        RESOLVE(glScissor);
        RESOLVE(glCullFace);
        RESOLVE(glFrontFace);
        RESOLVE(glGenBuffers);
        RESOLVE(glDeleteBuffers);
        RESOLVE(glBindBuffer);
        RESOLVE(glBufferData);
        RESOLVE(glBufferSubData);
        RESOLVE(glFramebufferRenderbuffer);
        RESOLVE(glGenFramebuffers);
        RESOLVE(glDeleteFramebuffers);
        RESOLVE(glBindFramebuffer);
        RESOLVE(glGenRenderbuffers);
        RESOLVE(glDeleteRenderbuffers);
        RESOLVE(glBindRenderbuffer);
        RESOLVE(glRenderbufferStorage);
        RESOLVE(glFramebufferTexture2D);
        RESOLVE(glCheckFramebufferStatus);
        // KHR_debug
        RESOLVE(glDebugMessageCallback);
    #undef RESOLVE

        GLint stencil_bits = 0;
        GLint red_bits   = 0;
        GLint green_bits = 0;
        GLint blue_bits  = 0;
        GLint alpha_bits = 0;
        GLint depth_bits = 0;
        GLint point_size[2];
        GLint max_texture_units = 0;
        GLint max_rbo_size = 0;
        GL_CALL(glGetIntegerv(GL_STENCIL_BITS, &stencil_bits));
        GL_CALL(glGetIntegerv(GL_RED_BITS, &red_bits));
        GL_CALL(glGetIntegerv(GL_GREEN_BITS, &green_bits));
        GL_CALL(glGetIntegerv(GL_BLUE_BITS, &blue_bits));
        GL_CALL(glGetIntegerv(GL_ALPHA_BITS, &alpha_bits));
        GL_CALL(glGetIntegerv(GL_DEPTH_BITS, &depth_bits));
        GL_CALL(glGetIntegerv(GL_ALIASED_POINT_SIZE_RANGE, point_size));
        GL_CALL(glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units));
        GL_CALL(glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_rbo_size));
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
        DEBUG("Maximum render buffer size %1x%2", max_rbo_size, max_rbo_size);
        mTextureUnits.resize(max_texture_units);

        // set some initial state
        GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
        GL_CALL(glDisable(GL_DEPTH_TEST));
        GL_CALL(glEnable(GL_CULL_FACE));
        GL_CALL(glCullFace(GL_BACK));
        GL_CALL(glFrontFace(GL_CCW));

        const char* extensions = (const char*)mGL.glGetString(GL_EXTENSIONS);
        std::stringstream ss(extensions);
        std::string extension;
        while (std::getline(ss, extension, ' '))
        {
            if (extension == "GL_EXT_sRGB")
                mExtensions.EXT_sRGB = true;
            else if (extension == "GL_OES_packed_depth_stencil")
                mExtensions.OES_packed_depth_stencil = true;
            //DEBUG("Found extension '%1'", extension);
        }
        INFO("sRGB textures: %1", mExtensions.EXT_sRGB ? "YES" : "NO");
        INFO("FBO packed depth+stencil: %1", mExtensions.OES_packed_depth_stencil ? "YES" : "NO");

        if (context->IsDebug() && mGL.glDebugMessageCallback)
        {
            GL_CALL(glDebugMessageCallback(DebugCallback, nullptr));
            GL_CALL(glEnable(GL_DEBUG_OUTPUT_KHR));
            INFO("Debug output is enabled.");
        }
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
       mFBOs.clear();
       mTextures.clear();
       mShaders.clear();
       mPrograms.clear();
       mGeoms.clear();

       for (auto& buffer : mBuffers)
       {
           GL_CALL(glDeleteBuffers(1, &buffer.name));
       }
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
    virtual void ClearDepth(float value) override
    {
        GL_CALL(glClearDepthf(value));
        GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));
    }
    virtual void ClearColorDepth(const Color4f& color, float depth) override
    {
        GL_CALL(glClearColor(color.Red(), color.Green(), color.Blue(), color.Alpha()));
        GL_CALL(glClearDepthf(depth));
        GL_CALL(glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT));
    }
    virtual void ClearColorDepthStencil(const Color4f&  color, float depth, int stencil) override
    {
        GL_CALL(glClearColor(color.Red(), color.Green(), color.Blue(), color.Alpha()));
        GL_CALL(glClearDepthf(depth));
        GL_CALL(glClearStencil(stencil));
        GL_CALL(glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    }

    virtual void SetDefaultTextureFilter(MinFilter filter) override
    {
        mDefaultMinTextureFilter = filter;
    }
    virtual void SetDefaultTextureFilter(MagFilter filter) override
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
        ret->SetFrameStamp(mFrameNumber);
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
        auto geometry = std::make_unique<GeomImpl>(this);
        auto* ret = geometry.get();
        mGeoms[name] = std::move(geometry);
        ret->SetFrameStamp(mFrameNumber);
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
        auto texture = std::make_unique<TextureImpl>(mGL, *this);
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
    virtual Framebuffer* FindFramebuffer(const std::string& name) override
    {
        auto it = mFBOs.find(name);
        if (it == std::end(mFBOs))
            return nullptr;
        return it->second.get();
    }
    virtual Framebuffer* MakeFramebuffer(const std::string& name) override
    {
        auto fbo = std::make_unique<FramebufferImpl>(name, mGL, *this);
        auto* ret = fbo.get();
        mFBOs[name] = std::move(fbo);
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
    virtual void DeleteFramebuffers() override
    {
        mFBOs.clear();
    }

    virtual void SetFramebuffer(const Framebuffer* fbo) override
    {
        if (fbo)
        {
            const auto* impl = static_cast<const FramebufferImpl*>(fbo);
            GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, impl->GetHandle()));
        }
        else
        {
            GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        }
    }

    virtual void Draw(const Program& program, const Geometry& geometry, const State& state) override
    {
        const auto* myprog = static_cast<const ProgImpl*>(&program);
        const auto* mygeom = static_cast<const GeomImpl*>(&geometry);
        myprog->SetFrameStamp(mFrameNumber);
        mygeom->SetFrameStamp(mFrameNumber);

        // start using this program
        GL_CALL(glUseProgram(myprog->GetName()));

        // IMPORTANT !
        // the program doesn't set the uniforms directly but instead of compares the uniform
        // value against a cached hash in order to elide redundant uniform sets.
        // However, this means that if a uniform value is set but the draw that uses the
        // program doesn't actually draw anything doing an early return here would mean
        // that the uniform state setting would be skipped. This would later lead to
        // incorrect assumption about the state of the program. I.e. using the program
        // later would not set the uniform value since the cached hash would incorrectly
        // indicate that the actual value has already been set in the program!
        // Easiest fix for this right now is to simply do the uniform setting first (and always)
        // even if the geometry is "empty", (i.e. no vertex data/draw commands).

        TRACE_ENTER(SetUniforms);

        // set program uniforms
        size_t num_uniforms = myprog->GetNumUniformsSet();
        for (size_t i=0; i<num_uniforms; ++i)
        {
            const auto& uniform = myprog->GetUniformSetting(i);
            const auto& value   = uniform.value;
            const auto location = uniform.location;
            if (const auto* ptr = std::get_if<int>(&value))
                GL_CALL(glUniform1i(location, *ptr));
            else if (const auto* ptr = std::get_if<float>(&value))
                GL_CALL(glUniform1f(location, *ptr));
            else if (const auto* ptr = std::get_if<glm::ivec2>(&value))
                GL_CALL(glUniform2i(location, ptr->x, ptr->y));
            else if (const auto* ptr = std::get_if<glm::vec2>(&value))
                GL_CALL(glUniform2f(location, ptr->x, ptr->y));
            else if (const auto* ptr = std::get_if<glm::vec3>(&value))
                GL_CALL(glUniform3f(location, ptr->x, ptr->y, ptr->z));
            else if (const auto* ptr = std::get_if<glm::vec4>(&value))
                GL_CALL(glUniform4f(location, ptr->x, ptr->y, ptr->z, ptr->w));
            else if (const auto* ptr = std::get_if<Color4f>(&value))
                GL_CALL(glUniform4f(location, ptr->Red(), ptr->Green(), ptr->Blue(), ptr->Alpha()));
            else if (const auto* ptr = std::get_if<ProgImpl::Uniform::Matrix2>(&value))
                GL_CALL(glUniformMatrix2fv(location, 1, GL_FALSE /* transpose */, (const float*)ptr->s));
            else if (const auto* ptr = std::get_if<ProgImpl::Uniform::Matrix3>(&value))
                GL_CALL(glUniformMatrix3fv(location, 1, GL_FALSE /* transpose */, (const float*)ptr->s));
            else if (const auto* ptr = std::get_if<ProgImpl::Uniform::Matrix4>(&value))
                GL_CALL(glUniformMatrix4fv(location, 1, GL_FALSE /*transpose*/, (const float*)&ptr->s));
            else BUG("Unhandled shader program uniform type.");
        }

        TRACE_LEAVE(SetUniforms);

        const auto buffer_byte_size = mygeom->GetByteSize();
        if (buffer_byte_size == 0)
            return;

        const auto& vertex_layout = mygeom->GetVertexLayout();
        ASSERT(vertex_layout.vertex_struct_size && "Vertex layout has not been set.");

        const auto buffer_vertex_count = buffer_byte_size / vertex_layout.vertex_struct_size;

        TRACE_ENTER(SetState);
        GL_CALL(glLineWidth(state.line_width));
        GL_CALL(glViewport(state.viewport.GetX(), state.viewport.GetY(),
                           state.viewport.GetWidth(), state.viewport.GetHeight()));
        switch (state.culling)
        {
            case State::Culling::None:
                GL_CALL(glDisable(GL_CULL_FACE));
                break;
            case State::Culling::Back:
                GL_CALL(glEnable(GL_CULL_FACE));
                GL_CALL(glCullFace(GL_BACK));
                break;
            case State::Culling::Front:
                GL_CALL(glEnable(GL_CULL_FACE));
                GL_CALL(glCullFace(GL_FRONT));
                break;
            case State::Culling::FrontAndBack:
                GL_CALL(glEnable(GL_CULL_FACE));
                GL_CALL(glCullFace(GL_FRONT_AND_BACK));
                break;
        }
        switch (state.blending)
        {
            case State::BlendOp::None:
                GL_CALL(glDisable(GL_BLEND));
                break;
            case State::BlendOp::Transparent:
                GL_CALL(glEnable(GL_BLEND));
                if (state.premulalpha)
                    GL_CALL(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
                else GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
                break;
            case State::BlendOp::Additive:
                GL_CALL(glEnable(GL_BLEND));
                GL_CALL(glBlendFunc(GL_ONE, GL_ONE));
                break;
        }

        // enable scissor if needed.
        if (EnableIf(GL_SCISSOR_TEST, !state.scissor.IsEmpty()))
        {
            GL_CALL(glScissor(state.scissor.GetX(), state.scissor.GetY(),
                              state.scissor.GetWidth(), state.scissor.GetHeight()));
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
        if (EnableIf(GL_DEPTH_TEST, state.depth_test != State::DepthTest::Disabled))
        {
            GLenum depth_test;
            if (state.depth_test == State::DepthTest::LessOrEQual)
                depth_test = GL_LEQUAL;
            else BUG("Unknown GL depth test mode.");
            GL_CALL(glDepthFunc(depth_test));
        }

        if (state.bWriteColor) {
            GL_CALL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
        } else {
            GL_CALL(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));
        }
        TRACE_LEAVE(SetState);

        GLenum default_texture_min_filter = GL_NONE;
        GLenum default_texture_mag_filter = GL_NONE;
        switch (mDefaultMinTextureFilter)
        {
            case Device::MinFilter::Nearest:   default_texture_min_filter = GL_NEAREST; break;
            case Device::MinFilter::Linear:    default_texture_min_filter = GL_LINEAR;  break;
            case Device::MinFilter::Mipmap:    default_texture_min_filter = GL_NEAREST_MIPMAP_NEAREST; break;
            case Device::MinFilter::Bilinear:  default_texture_min_filter = GL_NEAREST_MIPMAP_LINEAR; break;
            case Device::MinFilter::Trilinear: default_texture_min_filter = GL_LINEAR_MIPMAP_LINEAR; break;
        }
        switch (mDefaultMagTextureFilter)
        {
            case Device::MagFilter::Nearest: default_texture_mag_filter = GL_NEAREST; break;
            case Device::MagFilter::Linear:  default_texture_mag_filter = GL_LINEAR;  break;
        }

        // set program texture bindings
        size_t num_textures = myprog->GetNumSamplersSet();
        if (num_textures > mTextureUnits.size())
        {
            WARN("Program uses more textures than there are units available.");
            num_textures = mTextureUnits.size();
        }
        // for all textures used by this draw, look if the texture
        // is already bound to some unit. if it is already bound
        // and texture parameters haven't changed then nothing needs to be
        // done. Otherwise, see if there's a free texture unit slot or lastly
        // "evict" some texture from some unit and overwrite the binding with
        // this texture.
        size_t unit = 0;

        TRACE_ENTER(BindTextures);
        for (size_t i=0; i<num_textures; ++i)
        {
            const auto& sampler = myprog->GetSamplerSetting(i);
            const auto* texture = sampler.texture;
            texture->SetFrameStamp(mFrameNumber);
            // if the shader compiler has removed the uniform the location is -1
            // in which case we can skip rest of the work.
            if (sampler.location == -1)
                continue;

            // see if there's already a unit that has this texture bound.
            // if so, we use the same unit.
            for (unit=0; unit < mTextureUnits.size(); ++unit)
            {
                if (mTextureUnits[unit].texture == texture)
                    break;
            }
            // if the texture isn't bound yet we need to find a texture
            // unit that can be used. first look for a free unit if any.
            if (unit == mTextureUnits.size())
            {
                for (unit=0; unit<mTextureUnits.size(); ++unit)
                {
                    if (mTextureUnits[unit].texture == nullptr)
                        break;
                }
            }
            // if the texture isn't bound yet we're using all texture units.
            // this means a texture must be evicted and to let this texture be
            // bound to a unit. so we must look for a texture candidate for
            // replacement.
            if (unit == mTextureUnits.size())
            {
                size_t lru_frame_stamp = mTextureUnits[0].texture->GetFrameStamp();
                unit = 0;
                // look for a unit we can reuse, find the unit with the
                // LRU (least recently used) texture for replacement.
                for (size_t i=1; i<mTextureUnits.size(); ++i)
                {
                    const auto* bound_texture = mTextureUnits[i].texture;
                    const auto frame_stamp = bound_texture->GetFrameStamp();
                    if (frame_stamp < lru_frame_stamp)
                    {
                        unit = i;
                        lru_frame_stamp = frame_stamp;
                    }
                }
            }
            ASSERT(unit < mTextureUnits.size());

            // map the texture filter to a GL setting.
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
                    texture_min_filter = GL_NEAREST_MIPMAP_NEAREST;
                    break;
                case Texture::MinFilter::Bilinear:
                    texture_min_filter = GL_NEAREST_MIPMAP_LINEAR;
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
            ASSERT(texture_min_filter != GL_NONE);
            ASSERT(texture_mag_filter != GL_NONE);

            GLenum texture_wrap_x = texture->GetWrapX() == Texture::Wrapping::Clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT;
            GLenum texture_wrap_y = texture->GetWrapY() == Texture::Wrapping::Clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT;

#if defined(WEBGL)
            bool force_webgl_linear = false;
            bool force_webgl_wrap_x = false;
            bool force_webgl_wrap_y = false;

            // https://www.khronos.org/webgl/wiki/WebGL_and_OpenGL_Differences#Non-Power_of_Two_Texture_Support
            const auto width  = texture->GetWidth();
            const auto height = texture->GetHeight();
            const auto name   = texture->GetName();
            if (!base::IsPowerOfTwo(width) || !base::IsPowerOfTwo(height))
            {
                if (!(texture_min_filter == GL_NEAREST || texture_min_filter == GL_LINEAR))
                {
                    texture_min_filter = GL_LINEAR;
                    force_webgl_linear = true;
                }
                if (texture_wrap_x == GL_REPEAT)
                {
                    texture_wrap_x = GL_CLAMP_TO_EDGE;
                    force_webgl_wrap_x = true;
                }
                if (texture_wrap_y == GL_REPEAT)
                {
                    texture_wrap_y = GL_CLAMP_TO_EDGE;
                    force_webgl_wrap_y = true;
                }
            }
#endif
            // if nothing has changed then skip all the work
            if (mTextureUnits[unit].texture    == texture &&
                mTextureUnits[unit].min_filter == texture_min_filter &&
                mTextureUnits[unit].mag_filter == texture_mag_filter &&
                mTextureUnits[unit].wrap_x     == texture_wrap_x &&
                mTextureUnits[unit].wrap_y     == texture_wrap_y)
            {
                // set the texture unit to the sampler
                GL_CALL(glUniform1i(sampler.location, unit));
                continue;
            }

            // set all this dang state here, so we can easily track/understand
            // which unit the texture is bound to.
            const auto texture_handle = texture->GetHandle();
            const auto& texture_name  = texture->GetName();

#if defined(WEBGL)
            if (force_webgl_linear)
                WARN("Forcing GL_LINEAR on NPOT texture without mips. [texture='%1']", texture_name);
            if (force_webgl_wrap_x)
                WARN("Forcing GL_CLAMP_TO_EDGE on NPOT texture. [texture='%1']", texture_name);
            if (force_webgl_wrap_y)
                WARN("Forcing GL_CLAMP_TO_EDGE on NPOT texture. [texture='%1']", texture_name);
#endif
            if (texture_mag_filter == GL_NEAREST_MIPMAP_NEAREST ||
                texture_mag_filter == GL_NEAREST_MIPMAP_LINEAR ||
                texture_mag_filter == GL_LINEAR_MIPMAP_LINEAR)
            {
                if (!texture->HasMips())
                    WARN("Texture filter requires mips but texture doesn't have any! [texture='%1']", texture_name);
            }

            // // first select the desired texture unit.
            GL_CALL(glActiveTexture(GL_TEXTURE0 + unit));

            // bind the 2D texture.
            if (mTextureUnits[unit].texture != texture)
                GL_CALL(glBindTexture(GL_TEXTURE_2D, texture_handle));
            // set texture parameters, wrapping and min/mag filters.
            if (mTextureUnits[unit].wrap_x != texture_wrap_x)
                GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture_wrap_x));
            if (mTextureUnits[unit].wrap_y != texture_wrap_y)
                GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture_wrap_y));
            if (mTextureUnits[unit].mag_filter != texture_mag_filter)
                GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_mag_filter));
            if (mTextureUnits[unit].min_filter != texture_min_filter)
                GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_min_filter));

            // set the texture unit to the sampler
            GL_CALL(glUniform1i(sampler.location, unit));

            // store current binding and the sampler state.
            mTextureUnits[unit].texture    = texture;
            mTextureUnits[unit].min_filter = texture_min_filter;
            mTextureUnits[unit].mag_filter = texture_mag_filter;
            mTextureUnits[unit].wrap_x     = texture_wrap_x;
            mTextureUnits[unit].wrap_y     = texture_wrap_y;
        }
        TRACE_LEAVE(BindTextures);

        // start drawing geometry.

        // the brain-damaged API goes like this... when using DrawArrays with a client side
        // data pointer the argument is actually a pointer to the data.
        // When using a VBO the pointer is not a pointer but an offset to the VBO.
        const uint8_t* base_ptr =  (const uint8_t*)mygeom->GetByteOffset();

        const auto& buffer = mBuffers[mygeom->GetBufferIndex()];

        TRACE_ENTER(BindBuffers);
        GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, buffer.name));

        // first enable the vertex attributes.
        for (const auto& attr : vertex_layout.attributes)
        {
            const GLint location = mGL.glGetAttribLocation(myprog->GetName(), attr.name.c_str());
            if (location == -1)
                continue;
            const auto size   = attr.num_vector_components;
            const auto stride = vertex_layout.vertex_struct_size;
            const auto attr_ptr = base_ptr + attr.offset;
            GL_CALL(glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, stride, attr_ptr));
            GL_CALL(glEnableVertexAttribArray(location));
        }
        TRACE_LEAVE(BindBuffers);

        TRACE_ENTER(DrawGeometry);
        // go through the draw commands and submit the calls
        const auto& cmds = mygeom->GetNumDrawCmds();
        for (size_t i=0; i<cmds; ++i)
        {
            const auto& draw  = mygeom->GetDrawCommand(i);
            const auto count  = draw.count == std::numeric_limits<std::size_t>::max() ? buffer_vertex_count : draw.count;
            const auto type   = draw.type;
            const auto offset = draw.offset;

            if (type == Geometry::DrawType::Triangles)
                GL_CALL(glDrawArrays(GL_TRIANGLES, offset, count));
            else if (type == Geometry::DrawType::Points)
                GL_CALL(glDrawArrays(GL_POINTS, offset, count));
            else if (type == Geometry::DrawType::TriangleFan)
                GL_CALL(glDrawArrays(GL_TRIANGLE_FAN, offset, count));
            else if (type == Geometry::DrawType::Lines)
                GL_CALL(glDrawArrays(GL_LINES, offset, count));
            else if (type == Geometry::DrawType::LineLoop)
                GL_CALL(glDrawArrays(GL_LINE_LOOP, offset, count));
            else BUG("Unknown draw primitive type.");
        }
        TRACE_LEAVE(DrawGeometry);
    }

    virtual void CleanGarbage(size_t max_num_idle_frames, unsigned flags) override
    {
        if (flags & GCFlags::Programs)
        {
            for (auto it = mPrograms.begin(); it != mPrograms.end();)
            {
                auto* impl = static_cast<ProgImpl*>(it->second.get());
                const auto last_used_frame_number = impl->GetFrameStamp();
                if (mFrameNumber - last_used_frame_number >= max_num_idle_frames)
                    it = mPrograms.erase(it);
                else ++it;
            }
        }

        if (flags & GCFlags::Textures)
        {
            // use texture grouping to clean up (or not) groups of
            // textures and not just individual textures.
            // this is done because a sprite cycle could have any number
            // of textures and not all of them are used all the time.
            // yet all of them will be used and needed  to render the whole cycle
            // and we should not clean away textures mid cycle.
            std::unordered_map<std::string, size_t> group_last_use;
            for (auto& pair : mTextures)
            {
                const auto& texture = pair.second;
                const auto* impl  = static_cast<TextureImpl*>(texture.get());
                const auto& group = impl->GetGroup();
                if (group.empty())
                    continue;
                const auto last_used  = impl->GetFrameStamp();
                group_last_use[group] = std::max(group_last_use[group], last_used);
            }

            for (auto it = mTextures.begin(); it != mTextures.end();)
            {
                const auto* impl = static_cast<TextureImpl*>(it->second.get());
                const auto& group = impl->GetGroup();
                const auto group_last_used = group_last_use[group];
                const auto this_last_used  = impl->GetFrameStamp();
                const auto last_used = std::max(group_last_used, this_last_used);
                const auto is_expired = mFrameNumber - last_used >= max_num_idle_frames;
                if (is_expired)
                {
                    size_t unit = 0;
                    for (unit = 0; unit < mTextureUnits.size(); ++unit)
                    {
                        if (mTextureUnits[unit].texture == impl)
                        {
                            mTextureUnits[unit].texture = nullptr;
                            break;
                        }
                    }
                    // delete the texture
                    it = mTextures.erase(it);
                } else ++it;
            }
        }

        if (flags & GCFlags::Geometries)
        {
            for (auto it = mGeoms.begin(); it != mGeoms.end();)
            {
                auto* impl = static_cast<GeomImpl*>(it->second.get());
                const auto last_used_frame_number = impl->GetFrameStamp();
                if (mFrameNumber - last_used_frame_number >= max_num_idle_frames)
                    it = mGeoms.erase(it);
                else ++it;
            }
        }
    }

    virtual void BeginFrame() override
    {
        for (auto& pair : mPrograms)
        {
            auto* impl = static_cast<ProgImpl*>(pair.second.get());
            impl->BeginFrame();
        }

        // trying to do so called "buffer streaming" by "orphaning" the streaming
        // vertex buffers. this is achieved by re-specifying the contents of the
        // buffer by using nullptr data upload.
        // https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming
        for (auto& buff : mBuffers)
        {
            if (buff.usage == Geometry::Usage::Stream)
            {
                GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, buff.name));
                GL_CALL(glBufferData(GL_ARRAY_BUFFER, buff.capacity, nullptr, GL_STREAM_DRAW));
                buff.offset = 0;
            }
        }
    }
    virtual void EndFrame(bool display) override
    {
        mFrameNumber++;
        if (display)
            mContext->Display();

        const auto max_num_idle_frames = 120;

        // clean up expired transient textures.
        for (auto it = mTextures.begin(); it != mTextures.end();)
        {
            auto* impl = static_cast<TextureImpl*>(it->second.get());
            const auto last_used_frame_number = impl->GetFrameStamp();
            const auto is_expired = mFrameNumber - last_used_frame_number >= max_num_idle_frames;
            if (is_expired && impl->IsTransient())
            {
                size_t unit = 0;
                for (unit = 0; unit < mTextureUnits.size(); ++unit)
                {
                    if (mTextureUnits[unit].texture == impl)
                    {
                        mTextureUnits[unit].texture = nullptr;
                        break;
                    }
                }
                // delete the texture
                it = mTextures.erase(it);
            } else ++it;
        }
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

    virtual Bitmap<RGBA> ReadColorBuffer(unsigned x, unsigned y,
                                         unsigned width, unsigned height) const override
    {
        Bitmap<RGBA> bmp(width, height);
        GL_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
        GL_CALL(glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                (void*)bmp.GetDataPtr()));
        bmp.FlipHorizontally();
        return bmp;
    }
    virtual void GetResourceStats(ResourceStats* stats) const override
    {
        std::memset(stats, 0, sizeof(*stats));
        for (const auto& buffer : mBuffers)
        {
            if (buffer.usage == Geometry::Usage::Static)
            {
                stats->static_vbo_mem_alloc += buffer.capacity;
                stats->static_vbo_mem_use   += buffer.offset;
            }
            else if (buffer.usage == Geometry::Usage::Dynamic)
            {
                stats->dynamic_vbo_mem_alloc += buffer.capacity;
                stats->dynamic_vbo_mem_use   += buffer.offset;
            }
            else if (buffer.usage == Geometry::Usage::Stream)
            {
                stats->streaming_vbo_mem_alloc += buffer.capacity;
                stats->streaming_vbo_mem_use   += buffer.offset;
            }
        }
    }
    virtual void GetDeviceCaps(DeviceCaps* caps) const override
    {
        std::memset(caps, 0, sizeof(*caps));
        GLint num_texture_units = 0;
        GLint max_fbo_size = 0;
        GL_CALL(glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &num_texture_units));
        GL_CALL(glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_fbo_size));

        caps->num_texture_units     = num_texture_units;
        caps->max_fbo_height        = max_fbo_size;
        caps->max_fbo_width         = max_fbo_size;
    }

    std::tuple<size_t ,size_t> AllocateBuffer(size_t bytes, Geometry::Usage usage)
    {
        // there are 3 different types of buffers and each have their
        // own allocation strategy:

        // 1. Static buffers
        // Static buffers are allocated by static geometry objects
        // that are typically created once and never updated.
        // For static buffers we're using so called bump allocation.
        // That means that for each geometry allocation we take the
        // first chunk that can be found and has enough space. These
        // individual chunks can then never be "freed" but only when
        // the whole VBO is no longer referred to can the data be re-used.
        // This should be the optimal allocation strategy for static
        // game data that is created when the application begins
        // running and then never gets modified.
        //
        // 2. Dynamic buffers
        // Dynamic buffers can be allocated and used by geometry objects
        // that have had their geometry data updated. The usage can thus
        // grow or shrink during application run. This type of buffering
        // needs an allocation strategy that can handle fragmentation.
        // Since that doesn't currently exist (but is a TODO) we're just
        // going to use a VBO *per* geometry and let the driver handle
        // the fragmentation.
        //
        // 3. Streaming buffers.
        // Streaming buffers are used for streaming geometry that gets
        // updated on every frame, for example particle engines. The
        // allocation strategy is also to use a bump allocation but
        // reset the contents of each buffer on every new frame. This
        // allows the total buffer allocation to grow to a "high water
        // mark" and then keep re-using those buffers frame after frame.

        GLenum flag = GL_NONE;
        size_t capacity = 0;

        if (usage == Geometry::Usage::Static)
        {
            flag = GL_STATIC_DRAW;
            capacity = std::max(size_t(1024 * 1024), bytes);
        }
        else if (usage == Geometry::Usage::Stream)
        {
            flag = GL_STREAM_DRAW;
            capacity = std::max(size_t(1024 * 1024), bytes);
        }
        else if (usage == Geometry::Usage::Dynamic)
        {
            flag = GL_DYNAMIC_DRAW;
            capacity = bytes;
        }
        else BUG("Unsupported vertex buffer type.");

        for (size_t i=0; i<mBuffers.size(); ++i)
        {
            auto& buffer = mBuffers[i];
            const auto available = buffer.capacity - buffer.offset;
            if ((available >= bytes) && (buffer.usage == usage))
            {
                const auto offset = buffer.offset;
                buffer.offset += bytes;
                buffer.refcount++;
                return {i, offset};
            }
        }

        VertexBuffer buffer;
        buffer.usage    = usage;
        buffer.offset   = bytes;
        buffer.capacity = capacity;
        buffer.refcount = 1;
        GL_CALL(glGenBuffers(1, &buffer.name));
        GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, buffer.name));
        GL_CALL(glBufferData(GL_ARRAY_BUFFER, buffer.capacity, nullptr, flag));
        mBuffers.push_back(buffer);
        DEBUG("Allocated new vertex buffer. [vbo=%1, size=%2, type=%3]", buffer.name, buffer.capacity, usage);
        return {mBuffers.size()-1, 0};
    }
    void FreeBuffer(size_t index, size_t offset, size_t bytes, Geometry::Usage usage)
    {
        ASSERT(index < mBuffers.size());
        auto& buffer = mBuffers[index];
        ASSERT(buffer.refcount > 0);
        buffer.refcount--;

        if (buffer.usage == Geometry::Usage::Static || buffer.usage == Geometry::Usage::Dynamic)
        {
            if (buffer.refcount == 0)
                buffer.offset = 0;
        }
        if (usage == Geometry::Usage::Static)
            DEBUG("Free vertex data. [vbo=%1, bytes=%2, offset=%3, type=%4, refs=%5]", buffer.name,
                  bytes, offset, buffer.usage, buffer.refcount);
    }

    void UploadBuffer(size_t index, size_t offset, const void* data, size_t bytes, Geometry::Usage usage)
    {
        ASSERT(index < mBuffers.size());
        auto& buffer = mBuffers[index];
        ASSERT(offset + bytes <= buffer.capacity);
        GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, buffer.name));
        GL_CALL(glBufferSubData(GL_ARRAY_BUFFER, offset, bytes, data));

        if (buffer.usage == Geometry::Usage::Static)
        {
            const int percent_full = 100 * (double)buffer.offset / (double)buffer.capacity;
            DEBUG("Uploaded vertex data. [vbo=%1, bytes=%2, offset=%3, full=%4%, type=%5]", buffer.name,
                  bytes, offset, percent_full, buffer.usage);
        }
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
        BUG("Unknown GL stencil function");
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
        BUG("Unknown GL stencil op");
        return GL_NONE;
    }
private:
    class TextureImpl;
    // cached texture unit state. used to omit texture unit
    // state changes when not needed.
    struct TextureUnit {
        // the texture currently bound to the unit.
        const TextureImpl* texture = nullptr;
        // the unit's texture filtering setting.
        // initialized to GL_NONE just to make sure that the first
        // time the unit is used the settings are applied.
        GLenum min_filter = GL_NONE;
        GLenum mag_filter = GL_NONE;
        // the unit's texture coordinate wrapping settings.
        // initialized to GL_NONE just to make sure that the first
        // time the unit is used the settings are applied.
        GLenum wrap_x = GL_NONE;
        GLenum wrap_y = GL_NONE;
    };

    using TextureUnits = std::vector<TextureUnit>;

    class TextureImpl : public Texture
    {
    public:
        TextureImpl(const OpenGLFunctions& funcs, OpenGLES2GraphicsDevice& device)
           : mGL(funcs)
           , mDevice(device)
        {}
        ~TextureImpl()
        {
            if (mHandle)
            {
                GL_CALL(glDeleteTextures(1, &mHandle));
                if (!mTransient)
                    DEBUG("Deleted texture object. [name='%1', handle=%2]", mName, mHandle);
            }
        }

        virtual void Upload(const void* bytes, unsigned xres, unsigned yres, Format format, bool mips) override
        {
            if (mHandle == 0)
            {
                GL_CALL(glGenTextures(1, &mHandle));
                if (!mTransient)
                    DEBUG("New texture object. [name='%1', handle=%2]", mName, mHandle);
            }
            if (!mTransient)
                DEBUG("Loading texture. [name='%1', size=%2x%3, format=%4, handle=%5]", mName, xres, yres, format, mHandle);

            GLenum sizeFormat = 0;
            GLenum baseFormat = 0;
            switch (format)
            {
                case Format::sRGB:
                    sizeFormat = GL_SRGB_EXT;
                    baseFormat = GL_SRGB_EXT;
                    break;
                case Format::sRGBA:
                    sizeFormat = GL_SRGB_ALPHA_EXT;
                    baseFormat = GL_SRGB_ALPHA_EXT;
                    break;
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
                default: BUG("Unknown texture format."); break;
            }

            // if the texture is sRGB it can be used as-is as long as sRGB
            // extension is present. if no sRGB extension is available then
            // the texture needs to be converted into linear color space.
            // todo: convert to normalized linear floats ? converting to
            // linear u8 loses perceptible precision
            std::unique_ptr<IBitmap> linear;
            if (bytes)
            {
                if (format == Format::sRGB && !mDevice.mExtensions.EXT_sRGB)
                {
                    BitmapReadView<RGB> view((const RGB*) bytes, xres, yres);
                    linear = ConvertToLinear(view);
                    bytes = linear->GetDataPtr();
                    sizeFormat = GL_RGB;
                    baseFormat = GL_RGB;
                }
                else if (format == Format::sRGBA && !mDevice.mExtensions.EXT_sRGB)
                {
                    BitmapReadView<RGBA> view((const RGBA*) bytes, xres, yres);
                    linear = ConvertToLinear(view);
                    bytes = linear->GetDataPtr();
                    sizeFormat = GL_RGBA;
                    baseFormat = GL_RGBA;
                }
            }

            GL_CALL(glActiveTexture(GL_TEXTURE0));

            // trash the last texture unit in the hopes that it would not
            // cause a rebind later.
            const auto last = mDevice.mTextureUnits.size() - 1;
            const auto unit = GL_TEXTURE0 + last;

            // bind our texture here.
            GL_CALL(glActiveTexture(unit));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, mHandle));
            GL_CALL(glTexImage2D(GL_TEXTURE_2D,
                0, // mip level
                sizeFormat,
                xres,
                yres,
                0, // border must be 0
                baseFormat,
                GL_UNSIGNED_BYTE,
                bytes));

            mHasMips = false;

            if (bytes && mips)
            {
            #if defined(WEBGL)
                // WebGL only supports mips with POT textures.
                // This also means that with NPOT textures only linear or nearest
                // sampling can be used since mips are not available.
                // https://www.khronos.org/webgl/wiki/WebGL_and_OpenGL_Differences#Non-Power_of_Two_Texture_Support
                if (base::IsPowerOfTwo(xres) && base::IsPowerOfTwo(yres))
                {
                    GenerateMips(bytes, xres, yres, format);
                    mHasMips = true;
                }
                else WARN("WebGL doesn't support mips on NPOT textures. [texture='%1', width=%2, height=%3]", mName, xres, yres);
            #else
                GenerateMips(bytes, xres, yres, format);
                mHasMips = true;
            #endif
            }

            mWidth  = xres;
            mHeight = yres;
            mFormat = format;
            // we trashed this texture unit's texture binding.
            mDevice.mTextureUnits[last].texture = this;
            mDevice.mTextureUnits[last].wrap_x  = GL_NONE;
            mDevice.mTextureUnits[last].wrap_y  = GL_NONE;
            mDevice.mTextureUnits[last].min_filter = GL_NONE;
            mDevice.mTextureUnits[last].mag_filter = GL_NONE;
        }

        // refer actual state setting to the point when
        // the texture is actually used in a program's sampler
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
        virtual void SetContentHash(size_t hash) override
        { mHash = hash; }
        virtual void SetName(const std::string& name) override
        { mName = name; }
        virtual void SetGroup(const std::string& group) override
        { mGroup = group; }
        virtual size_t GetContentHash() const override
        { return mHash; }
        virtual void SetTransient(bool on_off) override
        { mTransient = on_off; }

        // internal
        bool IsTransient() const
        { return mTransient; }
        bool HasMips() const
        { return mHasMips; }
        GLuint GetHandle() const
        { return mHandle; }
        void SetFrameStamp(size_t frame_number) const
        { mFrameNumber = frame_number; }
        std::size_t GetFrameStamp() const
        { return mFrameNumber; }
        const std::string& GetName() const
        { return mName; }
        const std::string& GetGroup() const
        { return mGroup; }
    private:
        void GenerateMips(const void* bytes, unsigned xres, unsigned yres, Format format)
        {
            if (format == Format::sRGB)
                GenerateMipsFrom_sRGB<RGB>(bytes, xres, yres, GL_SRGB_EXT);
            else if (format == Format::sRGBA)
                GenerateMipsFrom_sRGB<RGBA>(bytes, xres, yres, GL_SRGB_ALPHA_EXT);
            else GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
        }
        template<typename T_rgb>
        void GenerateMipsFrom_sRGB(const void* bytes, unsigned xres, unsigned yres, GLenum type)
        {
            // if the texture has sRGB format then according to GL_EXT_sRGB
            // no mipmap generation is possible but glGenerateMipmap will return INVALID_OPERATION.
            // thus with sRGB formats we need to generate the mipmaps manually ourselves.
            // in this function we're expecting that the base level (level 0) has already
            // been loaded, so the next level is level 1

            // level 0 view
            BitmapReadView<T_rgb> view((const T_rgb*)bytes, xres, yres);

            auto level  = 1u;
            auto mipmap = GenerateNextMipmap(view, true);
            while (mipmap)
            {
                GL_CALL(glTexImage2D(GL_TEXTURE_2D,
                    level,
                    type,
                    mipmap->GetWidth(),
                    mipmap->GetHeight(),
                    0, // border must be 0
                    type,
                    GL_UNSIGNED_BYTE,
                    mipmap->GetDataPtr()));
                auto view = mipmap->GetReadView();
                mipmap = GenerateNextMipmap(*view, true);
                level++;
            }
        }
    private:
        const OpenGLFunctions& mGL;
        OpenGLES2GraphicsDevice& mDevice;
        GLuint mHandle = 0;
    private:
        MinFilter mMinFilter = MinFilter::Default;
        MagFilter mMagFilter = MagFilter::Default;
        Wrapping mWrapX = Wrapping::Repeat;
        Wrapping mWrapY = Wrapping::Repeat;
    private:
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
        Format mFormat = Texture::Format::Grayscale;
        mutable std::size_t mFrameNumber = 0;
        std::size_t mHash = 0;
        std::string mName;
        std::string mGroup;
        bool mTransient = false;
        bool mHasMips   = false;
    };

    class GeomImpl : public Geometry
    {
    public:
        GeomImpl(OpenGLES2GraphicsDevice* device) : mDevice(device)
        {}
       ~GeomImpl()
        {
            if (mBufferSize)
            {
                mDevice->FreeBuffer(mBufferIndex, mBufferOffset, mBufferSize, mBufferUsage);
            }
        }
        virtual void ClearDraws() override
        {
            mDrawCommands.clear();
        }
        virtual void AddDrawCmd(DrawType type) override
        {
            DrawCommand cmd;
            cmd.type   = type;
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
        virtual void SetVertexLayout(const VertexLayout& layout) override
        { mLayout = layout; }

        virtual void Upload(const void* data, size_t bytes, Usage usage) override
        {
            if (data == nullptr || bytes == 0)
            {
                if (mBufferSize)
                    mDevice->FreeBuffer(mBufferIndex, mBufferOffset, mBufferSize, mBufferUsage);

                mBufferSize  = 0;
                mBufferUsage = usage;
                return;
            }

            if ((usage != mBufferUsage) || (bytes > mBufferSize))
            {
                if (mBufferSize)
                    mDevice->FreeBuffer(mBufferIndex, mBufferOffset, mBufferSize, mBufferUsage);

                std::tie(mBufferIndex, mBufferOffset) = mDevice->AllocateBuffer(bytes, usage);
            }
            mDevice->UploadBuffer(mBufferIndex, mBufferOffset, data, bytes, usage);
            mBufferSize  = bytes;
            mBufferUsage = usage;
        }

        virtual void SetDataHash(size_t hash) override
        { mHash = hash; }
        virtual size_t GetDataHash() const  override
        { return mHash; }

        void SetFrameStamp(size_t frame_number) const
        { mFrameNumber = frame_number; }

        struct DrawCommand {
            DrawType type = DrawType::Triangles;
            size_t count  = 0;
            size_t offset = 0;
        };
        size_t GetFrameStamp() const
        { return mFrameNumber; }
        size_t GetBufferIndex() const
        { return mBufferIndex; }
        size_t GetByteOffset() const
        { return mBufferOffset; }
        size_t GetByteSize() const
        { return mBufferSize; }
        size_t GetNumDrawCmds() const
        { return mDrawCommands.size(); }
        const DrawCommand& GetDrawCommand(size_t index) const
        { return mDrawCommands[index]; }
        const VertexLayout& GetVertexLayout() const
        { return mLayout; }
    private:
        OpenGLES2GraphicsDevice* mDevice = nullptr;

        mutable std::size_t mFrameNumber = 0;
        std::vector<DrawCommand> mDrawCommands;
        std::size_t mBufferSize   = 0;
        std::size_t mBufferOffset = 0;
        std::size_t mBufferIndex  = 0;
        std::size_t mHash = 0;
        Usage mBufferUsage = Usage::Static;
        VertexLayout mLayout;
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
                DEBUG("Delete program %1", mProgram);
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

            DEBUG("Program was built successfully!");
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
                mUniforms.push_back({ret.location, x});
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
                mUniforms.push_back({ret.location, glm::ivec2(x, y)});
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
                mUniforms.push_back({ret.location, x});
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
                mUniforms.push_back({ret.location, glm::vec2(x, y)});
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
                mUniforms.push_back({ret.location, glm::vec3(x, y, z)});
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
                mUniforms.push_back({ret.location, glm::vec4(x, y, z, w)});
                ret.hash = hash;
            }
        }
        virtual void SetUniform(const char* name, const Color4f& color) override
        {
            auto& ret = GetUniform(name);
            if (ret.location == -1)
                return;
            const auto hash = base::hash_combine(0u, color);
            if (ret.hash != hash)
            {
                mUniforms.push_back({ret.location, color});
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
                mUniforms.push_back({ret.location, Uniform::Matrix2(matrix)});
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
                mUniforms.push_back({ret.location, Uniform::Matrix3(matrix) });
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
                mUniforms.push_back({ret.location, Uniform::Matrix4(matrix) });
                ret.hash = hash;
            }
        }

        virtual void SetTexture(const char* sampler, unsigned unit, const Texture& texture) override
        {
            auto ret = GetUniform(sampler);

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
            //   texture (last scan row).
            //
            //
            // this comment and the below kDeviceTextureMatrix is here only for posterity.
            // Currently, we have flipped the texture coordinates like explained above.
            //
            // If the program being used to render stuff is using a texture
            // we helpfully set up here a "device texture matrix" that will be provided
            // for the program and should be used to transform the texture coordinates
            // before sampling textures.
            // This will avoid having to do any image flips which is especially
            // handy when dealing with data that gets often (re)uploaded, e.g.
            // dynamically changing/procedural texture data.
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

            if (unit >= mSamplers.size())
                mSamplers.resize(unit + 1);

            // keep track of textures being used so that if/when this
            // program is actually used to draw stuff we can realize
            // which textures will actually be used to draw and do the
            // texture binds.
            mSamplers[unit].texture  = const_cast<TextureImpl*>(impl);
            mSamplers[unit].location = ret.location;
        }
        virtual void SetTextureCount(unsigned count) override
        {
            mSamplers.resize(count);
        }

        void BeginFrame()
        {
            // this clear has some unfortunate consequences.
            // If we don't clear then we're holding onto some texture objects
            // which means that:
            //   a) the texture(s) cannot be garbage collected
            //   b) the texture(s) are garbage collected and the pointers are garbage
            // Right now the solution is to clear the texture sampler settings
            // on every frame and require that the material system sets the
            // textures again before every draw.
            mSamplers.clear();
            mUniforms.clear();
        }

        struct Sampler {
            GLuint location = 0;
            TextureImpl* texture = nullptr;
        };
        struct Uniform {
            GLuint location = 0;
            struct Matrix2 {
                float s[2*2];
                Matrix2(const Program::Matrix2x2& matrix)
                { std::memcpy(&s, matrix, sizeof(matrix)); }
            };
            struct Matrix3 {
                float s[3*3];
                Matrix3(const Program::Matrix3x3& matrix)
                { std::memcpy(&s, matrix, sizeof(matrix)); }
            };
            struct Matrix4 {
                float s[4*4];
                Matrix4(const Program::Matrix4x4& matrix)
                { std::memcpy(&s, matrix, sizeof(matrix)); }
            };
            std::variant<int, float,
                glm::ivec2,
                glm::vec2,
                glm::vec3,
                glm::vec4,
                gfx::Color4f,
                Matrix2, Matrix3, Matrix4> value;
        };
        size_t GetNumSamplersSet() const
        { return mSamplers.size(); }
        size_t GetNumUniformsSet() const
        { return mUniforms.size(); }
        const Sampler& GetSamplerSetting(size_t index) const
        { return mSamplers[index]; }
        const Uniform& GetUniformSetting(size_t index) const
        { return mUniforms[index]; }
        GLuint GetName() const
        { return mProgram; }
        void SetFrameStamp(size_t frame_number) const
        { mFrameNumber = frame_number; }
        size_t GetFrameStamp() const
        { return mFrameNumber; }
    private:
        struct CachedUniform {
            GLuint location = 0;
            uint32_t hash   = 0;
        };
        CachedUniform& GetUniform(const char* name)
        {
            auto it = mUniformCache.find(name);
            if (it != std::end(mUniformCache))
                return it->second;

            auto ret = mGL.glGetUniformLocation(mProgram, name);
            CachedUniform uniform;
            uniform.location = ret;
            uniform.hash     = 0;
            mUniformCache[name] = uniform;
            return mUniformCache[name];
        }
        std::unordered_map<std::string, CachedUniform> mUniformCache;
    private:
        const OpenGLFunctions& mGL;
        GLuint mProgram = 0;
        GLuint mVersion = 0;
        std::vector<Sampler> mSamplers;
        std::vector<Uniform> mUniforms;
        mutable std::size_t mFrameNumber = 0;
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
        virtual bool CompileFile(const std::string& URI) override
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
            // as libANGLE) any decent system would still require several different rendering
            // paths. For example a primary "high end path" and a "fallback path" for low end
            // devices. These different paths would use different feature sets (such as instanced
            // rendering) and also (in many cases) require different shaders to be written to
            // fully take advantage of the graphics API features.
            // Then these two different rendering paths would/could use a shader translation
            // layer in order to use some specific graphics API. (through libANGLE)
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
            const auto& buffer = gfx::LoadResource(URI);
            if (!buffer)
            {
                ERROR("Failed to load shader source: '%1'", URI);
                return false;
            }

            const char* beg = (const char*)buffer->GetData();
            const char* end = beg + buffer->GetSize();
            if (!CompileSource(std::string(beg, end)))
            {
                ERROR("Failed to compile shader source file: '%1'", URI);
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
                DEBUG("Shader was built successfully!");
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
    struct Extensions;

    class FramebufferImpl : public Framebuffer
    {
    public:
        FramebufferImpl(const std::string& name, const OpenGLFunctions& funcs, OpenGLES2GraphicsDevice& device)
          : mName(name)
          , mGL(funcs)
          , mDevice(device)
        {}
       ~FramebufferImpl()
        {
            if (mColor)
            {
                for (size_t unit=0; unit<mDevice.mTextureUnits.size(); ++unit)
                {
                    if (mDevice.mTextureUnits[unit].texture == mColor.get())
                    {
                        mDevice.mTextureUnits[unit].texture = nullptr;
                        break;
                    }
                }
                mColor.reset();
            }

            if (mDepthBuffer)
            {
                GL_CALL(glDeleteRenderbuffers(1, &mDepthBuffer));
            }
            if (mHandle)
            {
                GL_CALL(glDeleteFramebuffers(1, &mHandle));
                DEBUG("Deleted frame buffer object. [handle=%1]", mHandle);
            }
        }
        virtual bool Create(const Config& conf) override
        {
            ASSERT(mHandle == 0);
            ASSERT(conf.width && conf.height);

            // WebGL spec see.
            // https://registry.khronos.org/webgl/specs/latest/1.0/

            // The OpenGL ES Framebuffer has multiple different sometimes exclusive properties/features that can be
            // parametrized when creating an FBO.
            // - Logical buffers attached to the FBO possible combinations of
            //   logical buffers including not having some buffer.
            //    * Color buffer
            //    * Depth buffer
            //    * Stencil buffer
            // - The bit representation for some logical buffer that dictates the number of bits used for the data.
            //   For example 8bit RGBA/32bit float color buffer or 16bit depth buffer or 8bit stencil buffer.
            // - The storage object that provides the data for the bitwise representation of the buffer's contents.
            //    * Texture object
            //    * Render buffer
            //
            // The OpenGL API essentially allows for a lot of possible FBO configurations to be created while in
            // practice only few are supported (and make sense). Unfortunately the ES2 spec does not require any
            // particular configurations to be supported by the implementation. Additionally, only 16bit color buffer
            // configurations are available for render buffer. In practice, it seems that implementations prefer
            // to support configurations that use a combined storage for depth + stencil and this requires and
            // extension 'OES_packed_depth_stencil'.
            // WebGL however makes an explicit requirement for implementations to support at least the following
            // configurations:
            // COLOR0                            DEPTH                           STENCIL
            // RGBA/UNSIGNED_BYTE texture        N/A                             N/A
            // RGBA/UNSIGNED_BYTE texture        DEPTH_COMPONENT16 renderbuffer  N/A
            // RGBA/UNSIGNED_BYTE texture        DEPTH_STENCIL     renderbuffer  DEPTH_STENCIL renderbuffer
            //
            // Small caveat the WebGL spec doesn't specify the bitwise representation for DEPTH_STENCIL, i.e.
            // how many bits of depth and how many bits of stencil.

            // Some relevant extensions that provide more FBO format support.(All tested on Linux + NVIDIA)
            // NAME                       Firefox   Chrome   Desktop   Desc
            // ---------------------------------------------------------------------------------------------------------
            // OES_rgb8_rgba8             No        No       Yes       RGB8 and RGBA8 color render buffer support (written against ES1)
            // OES_depth32                No        No       Yes       32bit depth render buffer (written against ES1)
            // OES_depth24                No        No       Yes       24bit depth render buffer (written against ES1)
            // OES_depth_texture          No        No       Yes
            // OES_packed_depth_stencil   No        No       Yes       24bit depth combined with 8bit stencil render buffer and texture format
            // WEBGL_color_buffer_float   Yes       Yes      No        32bit float color render buffer and float texture attachment
            // WEBGL_depth_texture        ?         ?        No        Limited form of OES_packed_depth_stencil + OES_depth_texture (ANGLE_depth_texture)
            //

            // Additional issues:
            // * WebGL supports only POT textures with mips
            // * Multisampled FBO creation and resolve. -> EXT_multisampled_render_to_texture but only for desktop
            // * MIP generation
            // * sRGB encoding

            const auto xres = conf.width;
            const auto yres = conf.height;

            struct Binder {
                Binder(const OpenGLFunctions& gl) : mGL(gl)
                {
                    GL_CALL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mCurrentFBO));
                }
               ~Binder()
                {
                    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mCurrentFBO));
                }
            private:
                const OpenGLFunctions& mGL;
                GLint mCurrentFBO = 0;
            } binder(mGL);

            mColor = std::make_unique<TextureImpl>(mGL, mDevice);
            mColor->Upload(nullptr, xres, yres, Texture::Format::RGBA, false /*mips*/);
            mColor->SetName("FBO/" + mName + "/color0");

            GL_CALL(glGenFramebuffers(1, &mHandle));
            GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mHandle));

            if (conf.format == Format::ColorRGBA8)
            {
                GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColor->GetHandle(), 0));
            }
            else if (conf.format == Format::ColorRGBA8_Depth16)
            {
                GL_CALL(glGenRenderbuffers(1, &mDepthBuffer));
                GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, mDepthBuffer));
                GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, xres, yres));
                GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer));
                GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColor->GetHandle(), 0));
            }
            else if (conf.format == Format::ColorRGBA8_Depth24_Stencil8)
            {
                if (mDevice.mContext->GetVersion() == Device::Context::Version::OpenGL_ES2)
                {
                    if (!mDevice.mExtensions.OES_packed_depth_stencil)
                    {
                        ERROR("Failed to create FBO. OES_packed_depth_stencil extension was not found.");
                        return false;
                    }
                    GL_CALL(glGenRenderbuffers(1, &mDepthBuffer));
                    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, mDepthBuffer));
                    GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, xres, yres));
                    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer));
                    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer));
                    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColor->GetHandle(), 0));
                }
                else
                {
                    // the WebGL spec doesn't actually mention the bit depths for the packed
                    // depth+stencil render buffer and the API exposed GLenum is GL_DEPTH_STENCIL 0x84F9
                    // which however is the same as GL_DEPTH_STENCIL_OES from OES_packed_depth_stencil
                    // So, I'm assuming here that the format is in fact 24bit depth with 8bit stencil.
                    GL_CALL(glGenRenderbuffers(1, &mDepthBuffer));
                    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, mDepthBuffer));
                    GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, WEBGL_DEPTH_STENCIL, xres, yres));
                    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, WEBGL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer));
                    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColor->GetHandle(), 0));
                }
            }
            // possible FBO *error* statuses are: INCOMPLETE_ATTACHMENT, INCOMPLETE_DIMENSIONS and INCOMPLETE_MISSING_ATTACHMENT
            // we're treating these status codes as BUGS in the engine code that is trying to create the
            // frame buffer object and has violated the frame buffer completeness requirement or other
            // creation parameter constraints.
            const auto ret = mGL.glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (ret == GL_FRAMEBUFFER_COMPLETE)
                DEBUG("Created new FBO. [width=%1, height=%2, format=%3, handle=%4]", xres, yres, conf.format, mHandle);
            else if (ret == GL_FRAMEBUFFER_UNSUPPORTED)
                ERROR("Unsupported FBO configuration. [width=%1, height=%2, format=%3]", xres, yres, conf.format);
            else BUG("Incorrect FBO setup.");
            mConfig = conf;
            return ret == GL_FRAMEBUFFER_COMPLETE;
        }
        virtual void Resolve(Texture** color) const override
        {
            ASSERT(mColor);
            *color = mColor.get();
        }
        virtual unsigned GetWidth() const override
        { return mConfig.width; }
        virtual unsigned GetHeight() const override
        { return mConfig.height; }
        virtual Format GetFormat() const override
        { return mConfig.format; }

        GLuint GetHandle() const
        { return mHandle; }
    private:
        const std::string mName;
        const OpenGLFunctions& mGL;
        OpenGLES2GraphicsDevice& mDevice;
        std::unique_ptr<TextureImpl> mColor;
        GLuint mHandle = 0;
        // this is either only depth or packed depth+stencil
        GLuint mDepthBuffer = 0;
        Config mConfig;
    };
private:
    std::map<std::string, std::unique_ptr<Geometry>> mGeoms;
    std::map<std::string, std::unique_ptr<Shader>> mShaders;
    std::map<std::string, std::unique_ptr<Program>> mPrograms;
    std::map<std::string, std::unique_ptr<Texture>> mTextures;
    std::map<std::string, std::unique_ptr<Framebuffer>> mFBOs;
    std::shared_ptr<Context> mContextImpl;
    Context* mContext = nullptr;
    std::size_t mFrameNumber = 0;
    OpenGLFunctions mGL;
    MinFilter mDefaultMinTextureFilter = MinFilter::Nearest;
    MagFilter mDefaultMagTextureFilter = MagFilter::Nearest;
    // texture units and their current settings.
    TextureUnits mTextureUnits;

    struct VertexBuffer {
        Geometry::Usage usage = Geometry::Usage::Static;
        GLuint name     = 0;
        size_t capacity = 0;
        size_t offset   = 0;
        size_t refcount = 0;
    };
    std::vector<VertexBuffer> mBuffers;
    struct Extensions {
        bool EXT_sRGB = false;
        bool OES_packed_depth_stencil = false;
    } mExtensions;
};

namespace detail {
std::shared_ptr<Device> CreateOpenGLES2Device(std::shared_ptr<gfx::Device::Context> context)
{
    return std::make_shared<OpenGLES2GraphicsDevice>(context);
}
std::shared_ptr<Device> CreateOpenGLES2Device(gfx::Device::Context* context)
{
    return std::make_shared<OpenGLES2GraphicsDevice>(context);
}
} // namespace

} // namespace
