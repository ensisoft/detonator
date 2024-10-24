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

#include "config.h"

#if defined(GFX_ENABLE_DEVICE_TRACING)
#  define BASE_TRACING_ENABLE_TRACING
# else
#  undef BASE_TRACING_ENABLE_TRACING
#endif

#include <GLES3/gl3.h>

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
#include "base/bitflag.h"
#include "graphics/drawcmd.h"
#include "graphics/resource.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/device.h"
#include "graphics/geometry.h"
#include "graphics/texture.h"
#include "graphics/color4f.h"
#include "graphics/loader.h"
#include "graphics/framebuffer.h"
#include "device/device.h"

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


// GL_EXT_draw_buffers
#define MAX_COLOR_ATTACHMENTS_EXT             0x8CDF

#if !defined(GRAPHICS_CHECK_OPENGL)
#  pragma message "OpenGL calls are NOT checked!"
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

#pragma message "OpenGL calls are checked!"
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
    PFNGLDRAWARRAYSINSTANCEDPROC     glDrawArraysInstanced;
    PFNGLDRAWELEMENTSPROC            glDrawElements;
    PFNGLDRAWELEMENTSINSTANCEDPROC   glDrawElementsInstanced;
    PFNGLGETATTRIBLOCATIONPROC       glGetAttribLocation;
    PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
    PFNGLVERTEXATTRIBDIVISORPROC     glVertexAttribDivisor;
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
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample;
    PFNGLFRAMEBUFFERTEXTURE2DPROC    glFramebufferTexture2D;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC  glCheckFramebufferStatus;
    PFNGLBLITFRAMEBUFFERPROC         glBlitFramebuffer;
    PFNGLDRAWBUFFERSPROC             glDrawBuffers;
    PFNGLREADBUFFERPROC              glReadBuffer;
    PFNGLCLEARBUFFERFVPROC           glClearBufferfv;
    PFNGLCLEARBUFFERFIPROC           glClearBufferfi;
    PFNGLCLEARBUFFERIVPROC           glClearBufferiv;

    // KHR_debug
    PFNGLDEBUGMESSAGECALLBACKPROC    glDebugMessageCallback;
};

//
// OpenGL ES 2.0 based custom graphics device implementation
// try to keep this implementation free of Qt in
// order to promote portability to possibly emscripten
// or Qt free implementation.
class OpenGLES2GraphicsDevice : public std::enable_shared_from_this<OpenGLES2GraphicsDevice>
                              , public gfx::Device
                              , public dev::Device
{
public:
    explicit OpenGLES2GraphicsDevice(dev::Context* context) noexcept
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
        RESOLVE(glDepthFunc);
        RESOLVE(glViewport);
        RESOLVE(glDrawArrays);
        RESOLVE(glDrawArraysInstanced);
        RESOLVE(glDrawElements);
        RESOLVE(glDrawElementsInstanced);
        RESOLVE(glGetAttribLocation);
        RESOLVE(glVertexAttribPointer);
        RESOLVE(glEnableVertexAttribArray);
        RESOLVE(glVertexAttribDivisor);
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
        RESOLVE(glRenderbufferStorageMultisample);
        RESOLVE(glBlitFramebuffer);
        RESOLVE(glFramebufferTexture2D);
        RESOLVE(glCheckFramebufferStatus);
        RESOLVE(glDrawBuffers)
        RESOLVE(glReadBuffer)
        RESOLVE(glClearBufferfv)
        RESOLVE(glClearBufferfi)
        RESOLVE(glClearBufferiv)
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
        GLint max_samples = 0;
        GL_CALL(glGetIntegerv(GL_MAX_SAMPLES, &max_samples));
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
        // a little hack to provide the INFO level graphics device
        // information only once.
        static bool have_printed_info = false;
        if (have_printed_info)
        {
            DEBUG("GL %1 Vendor: %2, %3",
                  mGL.glGetString(GL_VERSION),
                  mGL.glGetString(GL_VENDOR),
                  mGL.glGetString(GL_RENDERER));
            DEBUG("Stencil bits: %1", stencil_bits);
            DEBUG("Red bits: %1", red_bits);
            DEBUG("Blue bits: %1", blue_bits);
            DEBUG("Green bits: %1", green_bits);
            DEBUG("Alpha bits: %1", alpha_bits);
            DEBUG("Depth bits: %1", depth_bits);
            DEBUG("Point size: %1-%2", point_size[0], point_size[1]);
            DEBUG("Fragment shader texture units: %1", max_texture_units);
            DEBUG("Maximum render buffer size %1x%2", max_rbo_size, max_rbo_size);
            DEBUG("FBO MSAA samples: %1", max_samples);
        }
        else
        {
            INFO("GL %1 Vendor: %2, %3",
                 mGL.glGetString(GL_VERSION),
                 mGL.glGetString(GL_VENDOR),
                 mGL.glGetString(GL_RENDERER));
            INFO("Stencil bits: %1", stencil_bits);
            INFO("Red bits: %1", red_bits);
            INFO("Blue bits: %1", blue_bits);
            INFO("Green bits: %1", green_bits);
            INFO("Alpha bits: %1", alpha_bits);
            INFO("Depth bits: %1", depth_bits);
            INFO("Point size: %1-%2", point_size[0], point_size[1]);
            INFO("Fragment shader texture units: %1", max_texture_units);
            INFO("Maximum render buffer size %1x%2", max_rbo_size, max_rbo_size);
            INFO("FBO MSAA samples: %1", max_samples);
        }

        const char* extensions = (const char*)mGL.glGetString(GL_EXTENSIONS);
        std::stringstream ss(extensions);
        std::string extension;
        while (std::getline(ss, extension, ' '))
        {
            if (extension == "GL_EXT_sRGB")
                mExtensions.EXT_sRGB = true;
            else if (extension == "GL_OES_packed_depth_stencil")
                mExtensions.OES_packed_depth_stencil = true;
            else if (extension == "GL_EXT_draw_buffers")
                mExtensions.GL_EXT_draw_buffers;

            VERBOSE("Found extension '%1'", extension);
        }
        INFO("sRGB textures: %1", mExtensions.EXT_sRGB ? "YES" : "NO");
        INFO("FBO packed depth+stencil: %1", mExtensions.OES_packed_depth_stencil ? "YES" : "NO");
        INFO("EXT draw buffers: %1", mExtensions.GL_EXT_draw_buffers ? "YES" : "NO");

        if (context->IsDebug() && mGL.glDebugMessageCallback)
        {
            GL_CALL(glDebugMessageCallback(DebugCallback, nullptr));
            GL_CALL(glEnable(GL_DEBUG_OUTPUT_KHR));
            INFO("Debug output is enabled.");
        }

        const auto version = context->GetVersion();
        if (version == dev::Context::Version::OpenGL_ES3 ||
            version == dev::Context::Version::WebGL_2)
        {
            GLint max_color_attachments = 0;
            GL_CALL(glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_color_attachments));
            if (have_printed_info)
            {
                DEBUG("Maximum color attachments: %1", max_color_attachments);
            }
            else
            {
                INFO("Maximum color attachments: %1", max_color_attachments);
            }
        }
        else if (version == dev::Context::Version::OpenGL_ES2 ||
                    version == dev::Context::Version::WebGL_1)
        {
        }

        mTextureUnits.resize(max_texture_units);

        // set some initial state
        GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
        GL_CALL(glDisable(GL_DEPTH_TEST));
        GL_CALL(glEnable(GL_CULL_FACE));
        GL_CALL(glCullFace(GL_BACK));
        GL_CALL(glFrontFace(GL_CCW));

        have_printed_info = true;
    }
    explicit OpenGLES2GraphicsDevice(std::shared_ptr<dev::Context> context) noexcept
        : OpenGLES2GraphicsDevice(context.get())
    {
        mContextImpl = std::move(context);
    }
   ~OpenGLES2GraphicsDevice() override
    {
        DEBUG("~OpenGLES2GraphicsDevice");
        // make sure our cleanup order is specific so that the
        // resources are deleted before the context is deleted.
        mFBOs.clear();
        mTextures.clear();
        mShaders.clear();
        mPrograms.clear();
        mGeoms.clear();
        mInstances.clear();

        for (auto& buffer : mBuffers[0])
        {
            GL_CALL(glDeleteBuffers(1, &buffer.name));
        }
        for (auto& buffer : mBuffers[1])
        {
            GL_CALL(glDeleteBuffers(1, &buffer.name));
        }
    }

    void ClearColor(const gfx::Color4f& color, gfx::Framebuffer* fbo, ColorAttachment attachment) const override
    {
        if (!SetupFBO(fbo))
            return;

        if (fbo)
        {
            const auto color_buffer_index = static_cast<GLint>(attachment);
            const GLfloat value[] = {
                color.Red(), color.Green(), color.Blue(), color.Alpha()
            };
            GL_CALL(glClearBufferfv(GL_COLOR, color_buffer_index, value));
        }
        else
        {
            GL_CALL(glClearColor(color.Red(), color.Green(), color.Blue(), color.Alpha()));
            GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
        }
    }
    void ClearStencil(int value, gfx::Framebuffer* fbo) const override
    {
        if (!SetupFBO(fbo))
            return;

        if (fbo)
        {
            GL_CALL(glClearBufferiv(GL_STENCIL, 0, &value));
        }
        else
        {
            GL_CALL(glClearStencil(value));
            GL_CALL(glClear(GL_STENCIL_BUFFER_BIT));
        }
    }
    void ClearDepth(float value, gfx::Framebuffer* fbo) const override
    {
        if (!SetupFBO(fbo))
            return;

        if (fbo)
        {
            GL_CALL(glClearBufferfv(GL_DEPTH, 0, &value));
        }
        else
        {
            GL_CALL(glClearDepthf(value));
            GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));
        }
    }
    void ClearColorDepth(const gfx::Color4f& color, float depth, gfx::Framebuffer* fbo, ColorAttachment attachment) const override
    {
        if (!SetupFBO(fbo))
            return;

        if (fbo)
        {
            const auto color_buffer_index = static_cast<GLint>(attachment);
            const GLfloat value[] = {
                color.Red(), color.Green(), color.Blue(), color.Alpha()
            };
            GL_CALL(glClearBufferfv(GL_COLOR, color_buffer_index, value));
            GL_CALL(glClearBufferfv(GL_DEPTH, 0, &depth));
        }
        else
        {
            GL_CALL(glClearColor(color.Red(), color.Green(), color.Blue(), color.Alpha()));
            GL_CALL(glClearDepthf(depth));
            GL_CALL(glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT));
        }
    }
    void ClearColorDepthStencil(const gfx::Color4f&  color, float depth, int stencil, gfx::Framebuffer* fbo, ColorAttachment attachment) const override
    {
        if (!SetupFBO(fbo))
            return;

        if (fbo)
        {
            const auto color_buffer_index = static_cast<GLint>(attachment);
            const GLfloat value[] = {
                color.Red(), color.Green(), color.Blue(), color.Alpha()
            };
            GL_CALL(glClearBufferfv(GL_COLOR, color_buffer_index, value));
            GL_CALL(glClearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil));
        }
        else
        {
            GL_CALL(glClearColor(color.Red(), color.Green(), color.Blue(), color.Alpha()));
            GL_CALL(glClearDepthf(depth));
            GL_CALL(glClearStencil(stencil));
            GL_CALL(glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
        }
    }

    void SetDefaultTextureFilter(MinFilter filter) override
    {
        mDefaultMinTextureFilter = filter;
    }
    void SetDefaultTextureFilter(MagFilter filter) override
    {
        mDefaultMagTextureFilter = filter;
    }

    gfx::ShaderPtr FindShader(const std::string& name) override
    {
        auto it = mShaders.find(name);
        if (it == std::end(mShaders))
            return nullptr;
        return it->second;
    }

    gfx::ShaderPtr CreateShader(const std::string& id, const gfx::Shader::CreateArgs& args) override
    {
        auto shader = std::make_shared<ShaderImpl>(mGL);
        shader->SetName(args.name);
        shader->CompileSource(args.source);
        mShaders[id] = shader;
        return shader;
    }

    gfx::ProgramPtr FindProgram(const std::string& id) override
    {
        auto it = mPrograms.find(id);
        if (it == std::end(mPrograms))
            return nullptr;
        return it->second;
    }

    gfx::ProgramPtr CreateProgram(const std::string& id, const gfx::Program::CreateArgs& args) override
    {
        auto program = std::make_shared<ProgImpl>(mGL);

        std::vector<gfx::ShaderPtr> shaders;
        shaders.push_back(args.vertex_shader);
        shaders.push_back(args.fragment_shader);

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

    gfx::GeometryPtr FindGeometry(const std::string& id) override
    {
        auto it = mGeoms.find(id);
        if (it == std::end(mGeoms))
            return nullptr;
        return it->second;
    }

    gfx::GeometryPtr CreateGeometry(const std::string& id, gfx::Geometry::CreateArgs args) override
    {
        auto geometry = std::make_shared<GeomImpl>(this);
        geometry->SetFrameStamp(mFrameNumber);
        geometry->SetName(args.content_name);
        geometry->SetDataHash(args.content_hash);
        geometry->SetUsage(args.usage);
        geometry->SetBuffer(std::move(args.buffer));
        geometry->Upload();

        mGeoms[id] = geometry;
        return geometry;
    }

    gfx::GeometryInstancePtr FindGeometryInstance(const std::string& id) override
    {
        auto it = mInstances.find(id);
        if (it == std::end(mInstances))
            return nullptr;
        return it->second;
    }

    gfx::GeometryInstancePtr CreateGeometryInstance(const std::string& id, gfx::GeometryInstance::CreateArgs args) override
    {
        auto instance = std::make_shared<GeometryInstanceImpl>(this);
        instance->SetFrameStamp(mFrameNumber);
        instance->SetName(args.content_name);
        instance->SetDataHash(args.content_hash);
        instance->SetUsage(args.usage);
        instance->SetBuffer(std::move(args.buffer));
        instance->Upload();
        mInstances[id] = instance;
        return instance;
    }

    gfx::Texture* FindTexture(const std::string& name) override
    {
        auto it = mTextures.find(name);
        if (it == std::end(mTextures))
            return nullptr;
        return it->second.get();
    }

    gfx::Texture* MakeTexture(const std::string& name) override
    {
        auto texture = std::make_unique<TextureImpl>(name, mGL, *this);
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
    gfx::Framebuffer* FindFramebuffer(const std::string& name) override
    {
        auto it = mFBOs.find(name);
        if (it == std::end(mFBOs))
            return nullptr;
        return it->second.get();
    }
    gfx::Framebuffer* MakeFramebuffer(const std::string& name) override
    {
        auto fbo = std::make_unique<FramebufferImpl>(name, mGL, *this);
        auto* ret = fbo.get();
        mFBOs[name] = std::move(fbo);
        return ret;
    }

    void DeleteShaders() override
    {
        mShaders.clear();
    }
    void DeletePrograms() override
    {
        mPrograms.clear();
    }
    void DeleteGeometries() override
    {
        mGeoms.clear();
    }
    void DeleteTextures() override
    {
        mTextures.clear();

        for (auto& unit : mTextureUnits)
        {
            unit.texture = nullptr;
        }
    }
    void DeleteFramebuffers() override
    {
        mFBOs.clear();
    }
    void DeleteFramebuffer(const std::string& id) override
    {
        mFBOs.erase(id);
    }

    void Draw(const gfx::Program& program, const gfx::ProgramState& program_state,
              const gfx::GeometryDrawCommand& geometry, const State& state, gfx::Framebuffer* fbo) const override
    {
        if (!SetupFBO(fbo))
            return;

        const auto* myprog = static_cast<const ProgImpl*>(&program);
        const auto* mygeom = static_cast<const GeomImpl*>(geometry.GetGeometry());
        const auto* myinst = static_cast<const GeometryInstanceImpl*>(geometry.GetInstance());
        myprog->SetFrameStamp(mFrameNumber);
        mygeom->SetFrameStamp(mFrameNumber);
        if (myinst)
            myinst->SetFrameStamp(mFrameNumber);

        // start using this program
        //GL_CALL(glUseProgram(myprog->GetHandle()));

        // this will also call glUseProgram
        TRACE_CALL("SetUniforms", myprog->ApplyUniformState(program_state));

        // this check is fine for any draw case because even when drawing with
        // indices there should be vertex data. if there isn't that means the
        // geometry is dummy, i.e. contains not vertex data.
        // todo: is this a BUG actually??
        const auto vertex_buffer_byte_size = mygeom->GetVertexBufferByteSize();
        if (vertex_buffer_byte_size == 0)
            return;

        const auto index_buffer_type = mygeom->GetIndexBufferType();
        const auto index_byte_size = gfx::GetIndexByteSize(index_buffer_type);
        const auto index_buffer_byte_size = mygeom->GetIndexBufferByteSize();
        const auto buffer_index_count = index_buffer_byte_size / index_byte_size;

        const auto& vertex_layout = mygeom->GetVertexLayout();
        ASSERT(vertex_layout.vertex_struct_size && "Vertex layout has not been set.");

        const auto buffer_vertex_count = vertex_buffer_byte_size / vertex_layout.vertex_struct_size;

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
            if (state.depth_test == State::DepthTest::LessOrEQual)
                GL_CALL(glDepthFunc(GL_LEQUAL));
            else BUG("Unknown GL depth test mode.");
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
            case gfx::Device::MinFilter::Nearest:   default_texture_min_filter = GL_NEAREST; break;
            case gfx::Device::MinFilter::Linear:    default_texture_min_filter = GL_LINEAR;  break;
            case gfx::Device::MinFilter::Mipmap:    default_texture_min_filter = GL_NEAREST_MIPMAP_NEAREST; break;
            case gfx::Device::MinFilter::Bilinear:  default_texture_min_filter = GL_NEAREST_MIPMAP_LINEAR; break;
            case gfx::Device::MinFilter::Trilinear: default_texture_min_filter = GL_LINEAR_MIPMAP_LINEAR; break;
        }
        switch (mDefaultMagTextureFilter)
        {
            case gfx::Device::MagFilter::Nearest: default_texture_mag_filter = GL_NEAREST; break;
            case gfx::Device::MagFilter::Linear:  default_texture_mag_filter = GL_LINEAR;  break;
        }

        // set program texture bindings
        size_t num_textures = program_state.GetSamplerCount();
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

        // this is the set of units we're using already for this draw.
        // it might happen so that a texture that gets bound is an one
        // that was not used recently and then the next bind would
        // replace this texture! 
        std::unordered_set<size_t> units_for_this_draw;

        TRACE_ENTER(BindTextures);
        for (size_t i=0; i<num_textures; ++i)
        {
            const auto& setting = program_state.GetSamplerSetting(i);
            const auto& sampler = myprog->GetUniform(setting.name);

            auto* texture = (TextureImpl*)setting.texture;
            texture->SetFrameStamp(mFrameNumber);

            if (sampler.location == -1)
                continue;

            const auto num_units = mTextureUnits.size();
            size_t lru_unit        = num_units;
            size_t free_unit       = num_units;
            size_t current_unit    = num_units;
            size_t lru_frame_stamp = mFrameNumber;

            for (size_t i=0; i<mTextureUnits.size(); ++i)
            {
                if (mTextureUnits[i].texture == texture)
                {
                    current_unit = i;
                    break;
                }
                else if (mTextureUnits[i].texture == nullptr)
                {
                    free_unit = i;
                    break;
                }

                const auto frame_stamp = mTextureUnits[i].texture->GetFrameStamp();
                if (frame_stamp <= lru_frame_stamp)
                {
                    if (!base::Contains(units_for_this_draw, i))
                    {
                        lru_frame_stamp = frame_stamp;
                        lru_unit = i;
                    }
                }
            }

            size_t unit = 0;
            if (current_unit < num_units)
                unit = current_unit;
            else if (free_unit < num_units)
                unit = free_unit;
            else unit = lru_unit;

            ASSERT(unit < mTextureUnits.size());

            units_for_this_draw.insert(unit);

            // map the texture filter to a GL setting.
            GLenum texture_min_filter = GL_NONE;
            GLenum texture_mag_filter = GL_NONE;
            switch (texture->GetMinFilter())
            {
                case gfx::Texture::MinFilter::Default:
                    texture_min_filter = default_texture_min_filter;
                    break;
                case gfx::Texture::MinFilter::Nearest:
                    texture_min_filter = GL_NEAREST;
                    break;
                case gfx::Texture::MinFilter::Linear:
                    texture_min_filter = GL_LINEAR;
                    break;
                case gfx::Texture::MinFilter::Mipmap:
                    texture_min_filter = GL_NEAREST_MIPMAP_NEAREST;
                    break;
                case gfx::Texture::MinFilter::Bilinear:
                    texture_min_filter = GL_NEAREST_MIPMAP_LINEAR;
                    break;
                case gfx::Texture::MinFilter::Trilinear:
                    texture_min_filter = GL_LINEAR_MIPMAP_LINEAR;
                    break;
            }
            switch (texture->GetMagFilter())
            {
                case gfx::Texture::MagFilter::Default:
                    texture_mag_filter = default_texture_mag_filter;
                    break;
                case gfx::Texture::MagFilter::Nearest:
                    texture_mag_filter = GL_NEAREST;
                    break;
                case gfx::Texture::MagFilter::Linear:
                    texture_mag_filter = GL_LINEAR;
                    break;
            }
            ASSERT(texture_min_filter != GL_NONE);
            ASSERT(texture_mag_filter != GL_NONE);

            auto GLWrappingModeEnum = [](gfx::Texture::Wrapping wrapping) {
                if (wrapping == gfx::Texture::Wrapping::Clamp)
                    return GL_CLAMP_TO_EDGE;
                else if (wrapping == gfx::Texture::Wrapping::Repeat)
                    return GL_REPEAT;
                else if (wrapping == gfx::Texture::Wrapping::Mirror)
                    return GL_MIRRORED_REPEAT;
                else BUG("Bug on GL texture wrapping mode.");
                return GL_NONE;
            };

            GLenum texture_wrap_x = GLWrappingModeEnum(texture->GetWrapX());
            GLenum texture_wrap_y = GLWrappingModeEnum(texture->GetWrapY());

            const auto texture_handle = texture->GetHandle();
            const auto& texture_name  = texture->GetName();
            auto texture_state = texture->GetState();

            bool force_clamp_x = false;
            bool force_clamp_y = false;
            bool force_min_linear = false;

            // do some validation and warning logging if there's something that is wrong.
            if (texture_min_filter == GL_NEAREST_MIPMAP_NEAREST ||
                texture_min_filter == GL_NEAREST_MIPMAP_LINEAR ||
                texture_min_filter == GL_LINEAR_MIPMAP_LINEAR)
            {
                // this case handles both WebGL NPOT textures that don't support mips
                // and also cases such as render to a texture and using default filtering
                // when sampling and not having generated mips.
                if (!texture->HasMips())
                {
                    texture_min_filter = GL_LINEAR;
                    texture->SetFilter(gfx::Texture::MinFilter::Linear);
                    force_min_linear = true;
                }
            }

            if (mContext->GetVersion() == dev::Context::Version::WebGL_1)
            {
                // https://www.khronos.org/webgl/wiki/WebGL_and_OpenGL_Differences#Non-Power_of_Two_Texture_Support
                const auto width = texture->GetWidth();
                const auto height = texture->GetHeight();
                if (!base::IsPowerOfTwo(width) || !base::IsPowerOfTwo(height))
                {
                    if (texture_wrap_x == GL_REPEAT || texture_wrap_x == GL_MIRRORED_REPEAT)
                    {
                        texture_wrap_x = GL_CLAMP_TO_EDGE;
                        texture->SetWrapX(gfx::Texture::Wrapping::Clamp);
                        force_clamp_x = true;
                    }
                    if (texture_wrap_y == GL_REPEAT || texture_wrap_y == GL_MIRRORED_REPEAT)
                    {
                        texture_wrap_y = GL_CLAMP_TO_EDGE;
                        texture->SetWrapY(gfx::Texture::Wrapping::Clamp);
                        force_clamp_y = true;
                    }
                }
            }

            // if nothing has changed then skip all the work
            if (mTextureUnits[unit].texture == texture &&
                texture_state.min_filter == texture_min_filter &&
                texture_state.mag_filter == texture_mag_filter &&
                texture_state.wrap_x == texture_wrap_x &&
                texture_state.wrap_y == texture_wrap_y)
            {
                // set the texture unit to the sampler
                GL_CALL(glUniform1i(sampler.location, unit));
                continue;
            }

            if (!texture->IsTransient() && texture->WarnOnce())
            {
                if (force_min_linear)
                    WARN("Forcing GL_LINEAR on texture without mip maps. [texture='%1']", texture_name);
                if (force_clamp_x)
                    WARN("Forcing GL_CLAMP_TO_EDGE on NPOT texture. [texture='%1']", texture_name);
                if (force_clamp_y)
                    WARN("Forcing GL_CLAMP_TO_EDGE on NPOT texture. [texture='%1']", texture_name);
            }

            GL_CALL(glActiveTexture(GL_TEXTURE0 + unit));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, texture_handle));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture_wrap_x));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture_wrap_y));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_mag_filter));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_min_filter));

            // set the texture unit to the sampler
            GL_CALL(glUniform1i(sampler.location, unit));

            mTextureUnits[unit].texture  = texture;

            texture_state.wrap_x = texture_wrap_x;
            texture_state.wrap_y = texture_wrap_y;
            texture_state.mag_filter = texture_mag_filter;
            texture_state.min_filter = texture_min_filter;
            texture->SetState(texture_state);
        }
        TRACE_LEAVE(BindTextures);

        // start drawing geometry.

        const uint8_t* instance_base_ptr = myinst ? (const uint8_t*)myinst->GetVertexBufferByteOffset() : nullptr;

        // the brain-damaged API goes like this... when using DrawArrays with a client side
        // data pointer the glVertexAttribPointer 'pointer' argument is actually a pointer
        // to the vertex data. But when using a VBO the pointer is not a pointer but an offset
        // into the contents of the VBO.
        const uint8_t* vertex_base_ptr = (const uint8_t*)mygeom->GetVertexBufferByteOffset();
        // When an element array (i.e. an index buffer) is used the pointer argument in the
        // glDrawElements call changes from being pointer to the client side index data to
        // an offset into the element/index buffer.
        const uint8_t* index_buffer_offset = (const uint8_t*)mygeom->GetIndexBufferByteOffset();

        const auto* vertex_buffer = &mBuffers[0][mygeom->GetVertexBufferIndex()];
        const auto* index_buffer = mygeom->UsesIndexBuffer() ? &mBuffers[1][mygeom->GetIndexBufferIndex()] : nullptr;
        const auto* instance_buffer = myinst ? &mBuffers[0][myinst->GetVertexBufferIndex()] : nullptr;

        TRACE_ENTER(BindBuffers);

        if (index_buffer)
            GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer->name));

        // first enable the vertex attributes.
        GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer->name));
        for (const auto& attr : vertex_layout.attributes)
        {
            const GLint location = mGL.glGetAttribLocation(myprog->GetHandle(), attr.name.c_str());
            if (location == -1)
                continue;
            const auto size   = attr.num_vector_components;
            const auto stride = vertex_layout.vertex_struct_size;
            const auto attr_ptr = vertex_base_ptr + attr.offset;
            GL_CALL(glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, stride, attr_ptr));
            GL_CALL(glEnableVertexAttribArray(location));
        }

        if (instance_buffer)
        {
            const auto& per_instance_vertex_layout = myinst->GetVertexLayout();

            GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, instance_buffer->name));
            for (const auto& attr : per_instance_vertex_layout.attributes)
            {
                const GLint location = mGL.glGetAttribLocation(myprog->GetHandle(), attr.name.c_str());
                if (location == -1)
                    continue;
                const auto size = attr.num_vector_components;
                const auto stride = per_instance_vertex_layout.vertex_struct_size;
                const auto attr_ptr = instance_base_ptr + attr.offset;
                GL_CALL(glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, stride, attr_ptr));
                GL_CALL(glVertexAttribDivisor(location, 1)); // per instance attribute
                GL_CALL(glEnableVertexAttribArray(location));
            }
        }

        TRACE_LEAVE(BindBuffers);

        TRACE_ENTER(DrawGeometry);

        GLenum index_type;
        if (index_buffer_type == gfx::Geometry::IndexType::Index16)
            index_type = GL_UNSIGNED_SHORT;
        else if (index_buffer_type == gfx::Geometry::IndexType::Index32)
            index_type = GL_UNSIGNED_INT;
        else BUG("missing index type");

        const GLsizei instance_count = myinst ? myinst->GetInstanceCount() : 0;

        // go through the draw commands and submit the calls
        const auto& cmds = geometry.GetNumDrawCmds();
        for (size_t i=0; i<cmds; ++i)
        {
            // the number of buffer elements to draw by default if the draw doesn't specify
            // any actual number of elements. if we're using index buffer then consider the
            // number of index elements, otherwise consider the number of vertices.
            const auto buffer_element_count = index_buffer ? buffer_index_count : buffer_vertex_count;

            const auto& draw  = geometry.GetDrawCmd(i);
            const auto count  = draw.count == std::numeric_limits<uint32_t>::max() ? buffer_element_count : draw.count;
            const auto type   = draw.type;
            const auto offset = draw.offset;

            GLenum draw_mode;
            if (type == gfx::Geometry::DrawType::Triangles)
                draw_mode = GL_TRIANGLES;
            else if (type == gfx::Geometry::DrawType::Points)
                draw_mode = GL_POINTS;
            else if (type == gfx::Geometry::DrawType::TriangleFan)
                draw_mode = GL_TRIANGLE_FAN;
            else if (type == gfx::Geometry::DrawType::Lines)
                draw_mode = GL_LINES;
            else if (type == gfx::Geometry::DrawType::LineLoop)
                draw_mode = GL_LINE_LOOP;
            else BUG("Unknown draw primitive type.");

            // the byte offset from where to source the indices for the
            // draw is the base index buffer offset assigned for the geometry
            // plus the draw offset that is relative to the base offset.
            const uint8_t* index_buffer_draw_offset = index_buffer_offset + offset * index_byte_size;

            if (index_buffer && instance_buffer)
                GL_CALL(glDrawElementsInstanced(draw_mode, count, index_type, (const void*)index_buffer_draw_offset, instance_count));
            else if (index_buffer)
                GL_CALL(glDrawElements(draw_mode, count, index_type, (const void*)index_buffer_draw_offset));
            else if (instance_buffer)
                GL_CALL(glDrawArraysInstanced(draw_mode, offset, count, instance_count));
            else GL_CALL(glDrawArrays(draw_mode, offset, count));
        }
        TRACE_LEAVE(DrawGeometry);
    }

    void CleanGarbage(size_t max_num_idle_frames, unsigned flags) override
    {
        if (flags &  GCFlags::FBOs)
        {
            for (auto it = mFBOs.begin(); it != mFBOs.end(); )
            {
                auto* impl = static_cast<FramebufferImpl*>(it->second.get());
                const auto last_used_frame_number = impl->GetFrameStamp();
                if (mFrameNumber - last_used_frame_number >= max_num_idle_frames)
                    it = mFBOs.erase(it);
                else ++it;
            }
        }

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
            // yet all of them will be used and needed  to render the whole cycle,
            // and we should not clean away textures mid-cycle.
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

                if (is_expired && impl->GarbageCollect() && !IsTextureFBOTarget(impl))
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

            for (auto it = mInstances.begin(); it != mInstances.end(); )
            {
                auto* impl = static_cast<GeometryInstanceImpl*>(it->second.get());
                const auto last_used_frame_number = impl->GetFrameStamp();
                if (mFrameNumber - last_used_frame_number >= max_num_idle_frames)
                    it = mInstances.erase(it);
                else ++it;
            }
        }
    }

    void BeginFrame() override
    {
        for (auto& pair : mPrograms)
        {
            auto* impl = static_cast<ProgImpl*>(pair.second.get());
            impl->BeginFrame();
        }

        // trying to do so-called "buffer streaming" by "orphaning" the streaming
        // vertex buffers. this is achieved by re-specifying the contents of the
        // buffer by using nullptr data upload.
        // https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming

        // vertex buffers
        for (auto& buff : mBuffers[0])
        {
            if (buff.usage == gfx::Geometry::Usage::Stream)
            {
                GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, buff.name));
                GL_CALL(glBufferData(GL_ARRAY_BUFFER, buff.capacity, nullptr, GL_STREAM_DRAW));
                buff.offset = 0;
            }
        }
        // index buffers
        for (auto& buff : mBuffers[1])
        {
            if (buff.usage == gfx::Geometry::Usage::Stream)
            {
                GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buff.name));
                GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, buff.capacity, nullptr, GL_STREAM_DRAW));
                buff.offset = 0;
            }
        }
    }
    void EndFrame(bool display) override
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

            if (is_expired && impl->IsTransient() && !IsTextureFBOTarget(impl))
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

    gfx::Bitmap<gfx::Pixel_RGBA> ReadColorBuffer(unsigned width, unsigned height, gfx::Framebuffer* fbo) const override
    {
        gfx::Bitmap<gfx::Pixel_RGBA> bmp;

        if (!SetupFBO(fbo))
            return bmp;

        bmp.Resize(width, height);

        GL_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
        GL_CALL(glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (void*)bmp.GetDataPtr()));
        // by default the scan row order is reversed to what we expect.
        bmp.FlipHorizontally();
        return bmp;
    }

    gfx::Bitmap<gfx::Pixel_RGBA> ReadColorBuffer(unsigned x, unsigned y, unsigned width, unsigned height, gfx::Framebuffer* fbo) const override
    {
        gfx::Bitmap<gfx::Pixel_RGBA> bmp;
        if (!SetupFBO(fbo))
            return bmp;

        bmp.Resize(width, height);

        GL_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
        GL_CALL(glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (void*)bmp.GetDataPtr()));
        bmp.FlipHorizontally();
        return bmp;
    }
    void GetResourceStats(ResourceStats* stats) const override
    {
        std::memset(stats, 0, sizeof(*stats));
        for (const auto& buffer : mBuffers[0])
        {
            if (buffer.usage == gfx::Geometry::Usage::Static)
            {
                stats->static_vbo_mem_alloc += buffer.capacity;
                stats->static_vbo_mem_use   += buffer.offset;
            }
            else if (buffer.usage == gfx::Geometry::Usage::Dynamic)
            {
                stats->dynamic_vbo_mem_alloc += buffer.capacity;
                stats->dynamic_vbo_mem_use   += buffer.offset;
            }
            else if (buffer.usage == gfx::Geometry::Usage::Stream)
            {
                stats->streaming_vbo_mem_alloc += buffer.capacity;
                stats->streaming_vbo_mem_use   += buffer.offset;
            }
        }
        for (const auto& buffer : mBuffers[1])
        {
            if (buffer.usage == gfx::Geometry::Usage::Static)
            {
                stats->static_ibo_mem_alloc += buffer.capacity;
                stats->static_ibo_mem_use   += buffer.offset;
            }
            else if (buffer.usage == gfx::Geometry::Usage::Dynamic)
            {
                stats->dynamic_ibo_mem_alloc += buffer.capacity;
                stats->dynamic_ibo_mem_use   += buffer.offset;
            }
            else if (buffer.usage == gfx::Geometry::Usage::Stream)
            {
                stats->streaming_ibo_mem_alloc += buffer.capacity;
                stats->streaming_ibo_mem_use   += buffer.offset;
            }
        }
    }
    void GetDeviceCaps(DeviceCaps* caps) const override
    {
        std::memset(caps, 0, sizeof(*caps));
        GLint num_texture_units = 0;
        GLint max_fbo_size = 0;
        GL_CALL(glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &num_texture_units));
        GL_CALL(glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_fbo_size));

        caps->num_texture_units = num_texture_units;
        caps->max_fbo_height    = max_fbo_size;
        caps->max_fbo_width     = max_fbo_size;

        const auto version = mContext->GetVersion();
        if (version == dev::Context::Version::WebGL_2 ||
            version == dev::Context::Version::OpenGL_ES3)
        {
            caps->instanced_rendering = true;
            caps->multiple_color_attachments = true;
        }
        else if (version == dev::Context::Version::OpenGL_ES2 ||
                version == dev::Context::Version::WebGL_1)
        {

        }
    }

    gfx::Device* AsGraphicsDevice() override
    { return this; }
    std::shared_ptr<gfx::Device> GetSharedGraphicsDevice() override
    { return shared_from_this(); }

    enum class BufferType {
        VertexBuffer = GL_ARRAY_BUFFER,
        IndexBuffer  = GL_ELEMENT_ARRAY_BUFFER
    };

    std::tuple<size_t ,size_t> AllocateBuffer(size_t bytes, gfx::Geometry::Usage usage, BufferType type)
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

        if (usage == gfx::Geometry::Usage::Static)
        {
            flag = GL_STATIC_DRAW;
            capacity = std::max(size_t(1024 * 1024), bytes);
        }
        else if (usage == gfx::Geometry::Usage::Stream)
        {
            flag = GL_STREAM_DRAW;
            capacity = std::max(size_t(1024 * 1024), bytes);
        }
        else if (usage == gfx::Geometry::Usage::Dynamic)
        {
            flag = GL_DYNAMIC_DRAW;
            capacity = bytes;
        }
        else BUG("Unsupported vertex buffer type.");

        auto& buffers = type == BufferType::VertexBuffer ? mBuffers[0] : mBuffers[1];

        for (size_t i=0; i<buffers.size(); ++i)
        {
            auto& buffer = buffers[i];
            const auto available = buffer.capacity - buffer.offset;
            if ((available >= bytes) && (buffer.usage == usage))
            {
                const auto offset = buffer.offset;
                buffer.offset += bytes;
                buffer.refcount++;
                return {i, offset};
            }
        }

        BufferObject buffer;
        buffer.usage    = usage;
        buffer.offset   = bytes;
        buffer.capacity = capacity;
        buffer.refcount = 1;
        GL_CALL(glGenBuffers(1, &buffer.name));
        GL_CALL(glBindBuffer(static_cast<GLenum>(type), buffer.name));
        GL_CALL(glBufferData(static_cast<GLenum>(type), buffer.capacity, nullptr, flag));
        buffers.push_back(buffer);
        DEBUG("Allocated new buffer object. [bo=%1, size=%2, type=%3, type=%4]", buffer.name, buffer.capacity, usage,
              GLEnumToStr(static_cast<GLenum>(type)));
        return {buffers.size()-1, 0};
    }
    void FreeBuffer(size_t index, size_t offset, size_t bytes, gfx::Geometry::Usage usage, BufferType type)
    {
        auto& buffers = type == BufferType::VertexBuffer ? mBuffers[0] : mBuffers[1];

        ASSERT(index < buffers.size());
        auto& buffer = buffers[index];
        ASSERT(buffer.refcount > 0);
        buffer.refcount--;

        if (buffer.usage == gfx::Geometry::Usage::Static || buffer.usage == gfx::Geometry::Usage::Dynamic)
        {
            if (buffer.refcount == 0)
                buffer.offset = 0;
        }
        if (usage == gfx::Geometry::Usage::Static) {
            DEBUG("Free buffer data. [bo=%1, bytes=%2, offset=%3, type=%4, refs=%5, type=%6]",
                  buffer.name, bytes, offset, buffer.usage, buffer.refcount, GLEnumToStr(static_cast<GLenum>(type)));
        }
    }

    void UploadBuffer(size_t index, size_t offset, const void* data, size_t bytes, gfx::Geometry::Usage usage, BufferType type)
    {
        auto& buffers = type == BufferType::VertexBuffer ? mBuffers[0] : mBuffers[1];

        ASSERT(index < buffers.size());
        auto& buffer = buffers[index];
        ASSERT(offset + bytes <= buffer.capacity);
        GL_CALL(glBindBuffer(static_cast<GLenum>(type), buffer.name));
        GL_CALL(glBufferSubData(static_cast<GLenum>(type), offset, bytes, data));

        if (buffer.usage == gfx::Geometry::Usage::Static)
        {
            const int percent_full = 100 * (double)buffer.offset / (double)buffer.capacity;
            DEBUG("Uploaded buffer data. [bo=%1, bytes=%2, offset=%3, full=%4%, usage=%5, type=%6]",
                  buffer.name, bytes, offset, percent_full, buffer.usage, GLEnumToStr(static_cast<GLenum>(type)));
        }
    }
    bool SetupFBO(gfx::Framebuffer* fbo) const
    {
        if (fbo)
        {
            auto* impl = static_cast<FramebufferImpl*>(fbo);
            if (impl->IsReady())
            {
                if (!impl->Complete())
                    return false;
            }
            else
            {
                if (!impl->Create())
                    return false;
                if (!impl->Complete())
                    return false;
            }
            impl->SetFrameStamp(mFrameNumber);
        }
        else
        {
            GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        }
        return true;
    }
private:
    bool IsTextureFBOTarget(const gfx::Texture* texture) const
    {
        for (const auto& pair : mFBOs)
        {
            const auto* fbo = static_cast<const FramebufferImpl*>(pair.second.get());
            for (unsigned i=0; i<fbo->GetClientTextureCount(); ++i)
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


    bool EnableIf(GLenum flag, bool on_off) const
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
    };

    using TextureUnits = std::vector<TextureUnit>;

    class TextureImpl : public gfx::Texture
    {
    public:
        TextureImpl(std::string id, const OpenGLFunctions& funcs, OpenGLES2GraphicsDevice& device)
           : mGL(funcs)
           , mDevice(device)
           , mGpuId(std::move(id))
        {
            mFlags.set(Flags::Transient, false);
            mFlags.set(Flags::GarbageCollect, true);
        }
        ~TextureImpl() override
        {
            if (mHandle)
            {
                GL_CALL(glDeleteTextures(1, &mHandle));
                if (!IsTransient())
                    DEBUG("Deleted texture object. [name='%1', handle=%2]", mName, mHandle);
            }
        }

        void Upload(const void* bytes, unsigned xres, unsigned yres, Format format, bool mips) override
        {
            if (mHandle == 0)
            {
                GL_CALL(glGenTextures(1, &mHandle));
                if (!IsTransient())
                    DEBUG("Created new texture object. [name='%1', handle=%2]", mName, mHandle);
            }

            const auto version = mDevice.mContext->GetVersion();

            GLenum sizeFormat = 0;
            GLenum baseFormat = 0;
            switch (format)
            {
                case Format::sRGB:
                    if (version == dev::Context::Version::WebGL_1 ||
                        version == dev::Context::Version::OpenGL_ES2)
                    {
                        sizeFormat = GL_SRGB_EXT;
                        baseFormat = GL_RGB; //GL_SRGB_EXT;
                    }
                    else if (version == dev::Context::Version::WebGL_2 ||
                             version == dev::Context::Version::OpenGL_ES3)
                    {
                        sizeFormat = GL_SRGB8;
                        baseFormat = GL_RGB;
                    } else BUG("Unknown OpenGL ES version.");
                    break;
                case Format::sRGBA:
                    if (version == dev::Context::Version::WebGL_1 ||
                        version == dev::Context::Version::OpenGL_ES2)
                    {
                        sizeFormat = GL_SRGB_ALPHA_EXT;
                        baseFormat = GL_RGBA; //GL_SRGB_ALPHA_EXT;
                    }
                    else if (version == dev::Context::Version::WebGL_2 ||
                             version == dev::Context::Version::OpenGL_ES3)
                    {
                        sizeFormat = GL_SRGB8_ALPHA8;
                        baseFormat = GL_RGBA;
                    } else BUG("Unknown OpenGL ES version.");
                    break;
                case Format::RGB:
                    sizeFormat = GL_RGB;
                    baseFormat = GL_RGB;
                    break;
                case Format::RGBA:
                    sizeFormat = GL_RGBA;
                    baseFormat = GL_RGBA;
                    break;
                case Format::AlphaMask:
                    // when sampling R = G = B = 0.0 and A is the alpha value from here.
                    sizeFormat = GL_ALPHA;
                    baseFormat = GL_ALPHA;
                    break;
                default: BUG("Unknown texture format."); break;
            }

            if (version == dev::Context::Version::OpenGL_ES2 ||
                version == dev::Context::Version::WebGL_1)
            {
                if (format == Format::sRGB && !mDevice.mExtensions.EXT_sRGB)
                {
                    sizeFormat = GL_RGB;
                    baseFormat = GL_RGB;
                    WARN("Treating sRGB texture as RGB texture in the absence of EXT_sRGB. [name='%1']", mName);
                }
                else if (format == Format::sRGBA && !mDevice.mExtensions.EXT_sRGB)
                {
                    sizeFormat = GL_RGBA;
                    baseFormat = GL_RGBA;
                    WARN("Treating sRGBA texture as RGBA texture in the absence of EXT_sRGB. [name='%1']", mName);
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
                GenerateMips();
            }

            mWidth  = xres;
            mHeight = yres;
            mFormat = format;
            mDevice.mTextureUnits[last].texture = this;

            if (!IsTransient())
            {
                if (bytes)
                    DEBUG("Loaded texture data. [name='%1', size=%2x%3, format=%4, handle=%5]", mName, xres, yres, format, mHandle);
                else VERBOSE("Allocated texture storage. [name='%1', size=%2x%3, format=%4, handle=%5]", mName, xres, yres, format, mHandle);
            }
        }

        void SetFlag(Flags flag, bool on_off) override
        { mFlags.set(flag, on_off); }

        // refer actual state setting to the point when
        // the texture is actually used in a program's sampler
        void SetFilter(MinFilter filter) override
        { mMinFilter = filter; }
        void SetFilter(MagFilter filter) override
        { mMagFilter = filter; }
        void SetWrapX(Wrapping w) override
        { mWrapX = w; }
        void SetWrapY(Wrapping w) override
        { mWrapY = w; }

        Texture::MinFilter GetMinFilter() const override
        { return mMinFilter; }
        Texture::MagFilter GetMagFilter() const override
        { return mMagFilter; }
        Texture::Wrapping GetWrapX() const override
        { return mWrapX; }
        Texture::Wrapping GetWrapY() const override
        { return mWrapY; }
        unsigned GetWidth() const override
        { return mWidth; }
        unsigned GetHeight() const override
        { return mHeight; }
        Texture::Format GetFormat() const override
        { return mFormat; }
        void SetContentHash(size_t hash) override
        { mHash = hash; }
        void SetName(const std::string& name) override
        { mName = name; }
        void SetGroup(const std::string& group) override
        { mGroup = group; }
        size_t GetContentHash() const override
        { return mHash; }
        bool TestFlag(Flags flag) const override
        { return mFlags.test(flag); }
        std::string GetName() const override
        { return mName; }
        std::string GetGroup() const override
        { return mGroup; }
        std::string GetId() const override
        { return mGpuId; }

        bool GenerateMips() override
        {
            if (mHasMips)
                return true;

            if (mDevice.mContext->GetVersion() == dev::Context::Version::WebGL_1)
            {
                if (!base::IsPowerOfTwo(mWidth) || !base::IsPowerOfTwo(mHeight))
                {
                    WARN("WebGL1 doesn't support mips on NPOT texture. [name='%1', size=%2x%3]", mName, mWidth, mHeight);
                    return false;
                }
                if (mFormat == Format::sRGB || mFormat == Format::sRGBA)
                {
                    WARN("WebGL1 doesn't support mips on sRGB/sRGBA texture. [name='%1', format=%2]", mName, mFormat);
                    return false;
                }
            }
            else if (mDevice.mContext->GetVersion() == dev::Context::Version::WebGL_2)
            {
                if (mFormat == Format::sRGB || mFormat == Format::sRGBA)
                {
                    WARN("WebGL2 doesn't support mips on sRGB/sRGBA texture. [name='%1', format=%2]", mName, mFormat);
                    return false;
                }
            }
            else if (mDevice.mContext->GetVersion() == dev::Context::Version::OpenGL_ES2)
            {
                if (mFormat == Format::sRGB || mFormat == Format::sRGBA)
                {
                    WARN("GL ES2 doesn't support mips on sRGB/sRGBA texture. [name='%1', format=%2]", mName, mFormat);
                    return false;
                }
            }

            const auto last_unit_index = mDevice.mTextureUnits.size() - 1;
            const auto texture_unit    = GL_TEXTURE0 + last_unit_index;

            GL_CALL(glActiveTexture(texture_unit));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, mHandle));
            // seems that driver bugs are common regarding sRGB mipmap generation
            // so we're going to unwrap this GL call and assume any error 
            // is an error about failing to generate mips because of driver bugs
            mGL.glGenerateMipmap(GL_TEXTURE_2D);
            const auto error = mGL.glGetError();

            mDevice.mTextureUnits[last_unit_index].texture = this;
            mHasMips = error == GL_NO_ERROR;
            if (!IsTransient())
            {
                if (mHasMips)
                    DEBUG("Generated mip maps on texture. [name='%1']", mName);
                else WARN("Failed to generate mips on texture. [name='%1']", mName);
            }
            return mHasMips;
        }
        bool HasMips() const override
        { return mHasMips; }

        // internal
        bool WarnOnce() const
        {
            auto ret = mWarnOnce;
            mWarnOnce = false;
            return ret;
        }

        GLuint GetHandle() const
        { return mHandle; }
        void SetFrameStamp(size_t frame_number) const
        { mFrameNumber = frame_number; }
        std::size_t GetFrameStamp() const
        { return mFrameNumber; }

        struct GLState {
            GLenum wrap_x = GL_NONE;
            GLenum wrap_y = GL_NONE;
            GLenum min_filter = GL_NONE;
            GLenum mag_filter = GL_NONE;
        };
        GLState GetState() const
        { return mState; }
        void SetState(const GLState& state)
        { mState = state; }

    private:
        const OpenGLFunctions& mGL;
        OpenGLES2GraphicsDevice& mDevice;
        GLuint mHandle = 0;
        GLState mState;
    private:
        MinFilter mMinFilter = MinFilter::Default;
        MagFilter mMagFilter = MagFilter::Default;
        Wrapping mWrapX = Wrapping::Repeat;
        Wrapping mWrapY = Wrapping::Repeat;
        Format mFormat  = Texture::Format::AlphaMask;
    private:
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
        mutable std::size_t mFrameNumber = 0;
        std::size_t mHash = 0;
        std::string mName;
        std::string mGroup;
        std::string mGpuId;
        base::bitflag<Flags> mFlags;
        bool mHasMips   = false;
        mutable bool mWarnOnce  = true;
    };

    class GeometryInstanceImpl : public gfx::GeometryInstance
    {
    public:
        explicit GeometryInstanceImpl(OpenGLES2GraphicsDevice* device) noexcept
          : mDevice(device)
        {}
       ~GeometryInstanceImpl() override
        {
            if (mBufferSize)
            {
                mDevice->FreeBuffer(mBufferIndex, mBufferOffset, mBufferSize, mUsage, BufferType::VertexBuffer);
            }
        }
        void Upload() const
        {
            if (!mPendingUpload.has_value())
                return;

            auto upload = std::move(mPendingUpload.value());

            mPendingUpload.reset();

            const auto vertex_bytes = upload.GetVertexBytes();
            const auto vertex_ptr   = upload.GetVertexDataPtr();
            if (vertex_bytes == 0)
                return;

            std::tie(mBufferIndex, mBufferOffset) = mDevice->AllocateBuffer(vertex_bytes, mUsage, BufferType::VertexBuffer);
            mDevice->UploadBuffer(mBufferIndex,
                                  mBufferOffset,
                                  vertex_ptr, vertex_bytes,
                                  mUsage, BufferType::VertexBuffer);
            mBufferSize = vertex_bytes;
            mLayout = std::move(upload.GetLayout());
            if (mUsage == Usage::Static)
            {
                DEBUG("Uploaded geometry instance buffer data. [name='%1', bytes='%2', usage='%3']", mName, vertex_bytes, mUsage);
            }
        }

        inline void SetBuffer(gfx::GeometryInstanceBuffer&& buffer) noexcept
        { mPendingUpload = std::move(buffer); }
        inline void SetUsage(Usage usage) noexcept
        { mUsage = usage; }
        inline void SetDataHash(size_t hash) noexcept
        { mHash = hash; }
        inline void SetName(const std::string& name) noexcept
        { mName = name; }
        inline void SetFrameStamp(size_t frame_number) const noexcept
        { mFrameNumber = frame_number; }
        inline size_t GetFrameStamp() const noexcept
        { return mFrameNumber; }
        inline size_t GetVertexBufferByteOffset() const noexcept
        { return mBufferOffset; }
        inline size_t GetVertexBufferIndex() const noexcept
        { return mBufferIndex; }
        inline size_t GetInstanceCount() const noexcept
        { return mBufferSize / mLayout.vertex_struct_size; }
        inline const gfx::GeometryInstanceDataLayout& GetVertexLayout() const noexcept
        { return mLayout; }

    private:
        OpenGLES2GraphicsDevice* mDevice = nullptr;
        std::size_t mHash = 0;
        std::string mName;
        Usage mUsage = Usage::Static;
        mutable std::size_t mFrameNumber = 0;
        mutable std::size_t mBufferSize   = 0;
        mutable std::size_t mBufferOffset = 0;
        mutable std::size_t mBufferIndex  = 0;
        mutable std::optional<gfx::GeometryInstanceBuffer> mPendingUpload;
        mutable gfx::GeometryInstanceDataLayout mLayout;
    };

    class GeomImpl : public gfx::Geometry
    {
    public:
        using BufferType = OpenGLES2GraphicsDevice::BufferType;

        explicit GeomImpl(OpenGLES2GraphicsDevice* device) noexcept
          : mDevice(device)
        {}
       ~GeomImpl() override
        {
            if (mVertexBufferSize)
            {
                mDevice->FreeBuffer(mVertexBufferIndex, mVertexBufferOffset, mVertexBufferSize, mUsage, BufferType::VertexBuffer);
            }
            if (mIndexBufferSize)
            {
                mDevice->FreeBuffer(mIndexBufferIndex, mIndexBufferOffset, mIndexBufferSize, mUsage, BufferType::IndexBuffer);
            }
            if (mUsage == Usage::Static)
                DEBUG("Deleted geometry object. [name='%1']", mName);
        }

        size_t GetContentHash() const  override
        { return mHash; }
        size_t GetNumDrawCmds() const override
        { return mDrawCommands.size(); }
        DrawCommand GetDrawCmd(size_t index) const override
        { return mDrawCommands[index]; }
        Usage GetUsage() const override
        { return mUsage; }
        std::string GetName() const override
        { return mName; }

        void Upload() const
        {
            if (!mPendingUpload.has_value())
                return;

            auto upload = std::move(mPendingUpload.value());

            mPendingUpload.reset();

            const auto vertex_bytes = upload.GetVertexBytes();
            const auto index_bytes  = upload.GetIndexBytes();
            const auto vertex_ptr   = upload.GetVertexDataPtr();
            const auto index_ptr    = upload.GetIndexDataPtr();
            if (vertex_bytes == 0)
                return;

            std::tie(mVertexBufferIndex, mVertexBufferOffset) = mDevice->AllocateBuffer(vertex_bytes, mUsage, BufferType::VertexBuffer);
            mDevice->UploadBuffer(mVertexBufferIndex,
                                  mVertexBufferOffset,
                                  vertex_ptr, vertex_bytes,
                                  mUsage, BufferType::VertexBuffer);
            mVertexBufferSize = upload.GetVertexBytes();
            mVertexLayout     = std::move(upload.GetLayout());
            mDrawCommands     = std::move(upload.GetDrawCommands());

            if (mUsage == Usage::Static)
            {
                DEBUG("Uploaded geometry vertices. [name='%1', bytes='%2', usage='%3']", mName, vertex_bytes, mUsage);
            }
            if (index_bytes == 0)
                return;

            std::tie(mIndexBufferIndex, mIndexBufferOffset) = mDevice->AllocateBuffer(index_bytes, mUsage, BufferType::IndexBuffer);
            mDevice->UploadBuffer(mIndexBufferIndex,
                                  mIndexBufferOffset,
                                  index_ptr, index_bytes,
                                  mUsage, BufferType::IndexBuffer);
            mIndexBufferSize = index_bytes;
            mIndexBufferType = upload.GetIndexType();
            if (mUsage == Usage::Static)
            {
                DEBUG("Uploaded geometry indices. [name='%1', bytes='%2', usage='%3']", mName, index_bytes, mUsage);
            }
        }

        inline void SetBuffer(gfx::GeometryBuffer&& buffer) noexcept
        { mPendingUpload = std::move(buffer); }
        inline void SetUsage(Usage usage) noexcept
        { mUsage = usage; }
        inline void SetDataHash(size_t hash) noexcept
        { mHash = hash; }
        inline void SetName(const std::string& name) noexcept
        { mName = name; }
        inline void SetFrameStamp(size_t frame_number) const noexcept
        { mFrameNumber = frame_number; }
        inline size_t GetFrameStamp() const noexcept
        { return mFrameNumber; }
        inline size_t GetVertexBufferIndex() const noexcept
        { return mVertexBufferIndex; }
        inline size_t GetIndexBufferIndex() const noexcept
        { return mIndexBufferIndex; }
        inline size_t GetVertexBufferByteOffset() const noexcept
        { return mVertexBufferOffset; }
        inline size_t GetIndexBufferByteOffset() const noexcept
        { return mIndexBufferOffset; }
        inline size_t GetVertexBufferByteSize() const noexcept
        { return mVertexBufferSize; }
        inline size_t GetIndexBufferByteSize() const noexcept
        { return mIndexBufferSize; }
        inline IndexType GetIndexBufferType() const noexcept
        { return mIndexBufferType; }
        inline bool UsesIndexBuffer() const noexcept
        { return mIndexBufferSize != 0; }
        inline const gfx::VertexLayout& GetVertexLayout() const noexcept
        { return mVertexLayout; }
    private:
        OpenGLES2GraphicsDevice* mDevice = nullptr;

        std::size_t mHash = 0;
        std::string mName;
        Usage mUsage = Usage::Static;

        mutable std::size_t mFrameNumber = 0;
        mutable std::optional<gfx::GeometryBuffer> mPendingUpload;
        mutable std::vector<DrawCommand> mDrawCommands;
        mutable std::size_t mVertexBufferSize   = 0;
        mutable std::size_t mVertexBufferOffset = 0;
        mutable std::size_t mVertexBufferIndex  = 0;
        mutable std::size_t mIndexBufferSize   = 0;
        mutable std::size_t mIndexBufferOffset = 0;
        mutable std::size_t mIndexBufferIndex  = 0;
        mutable IndexType mIndexBufferType = IndexType::Index16;
        mutable gfx::VertexLayout mVertexLayout;
    };

    class ProgImpl : public gfx::Program
    {
    public:
        explicit ProgImpl(const OpenGLFunctions& funcs) noexcept
          : mGL(funcs)
        {}

       ~ProgImpl() override
        {
            if (mProgram)
            {
                GL_CALL(glDeleteProgram(mProgram));
                DEBUG("Deleted program object. [name='%1', handle='%2']", mName, mProgram);
            }
        }
        bool Build(const std::vector<gfx::ShaderPtr>& shaders)
        {
            GLuint prog = mGL.glCreateProgram();
            DEBUG("Created new GL program object. [name='%1', handle='%2']", mName, prog);

            for (const auto& shader : shaders)
            {
                ASSERT(shader && shader->IsValid());
                GL_CALL(glAttachShader(prog,
                    static_cast<const ShaderImpl*>(shader.get())->GetHandle()));
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

            if (link_status == 0)
            {
                ERROR("Program link error. [name='%1', info='%2']", mName, build_info);
                GL_CALL(glDeleteProgram(prog));
                return false;
            }
            else if (valid_status == 0)
            {
                ERROR("Program is not valid. [name='%1', info='%2']", mName, build_info);
                GL_CALL(glDeleteProgram(prog));
                return false;
            }

            DEBUG("Program was built successfully. [name='%1', info='%2']", mName, build_info);
            mProgram = prog;
            return true;
        }

        void ApplyUniformState(const gfx::ProgramState& state) const
        {
            GL_CALL(glUseProgram(mProgram));
            // flush pending uniforms onto the GPU program object
            for (size_t i=0; i<state.GetUniformCount(); ++i)
            {
                const auto& setting = state.GetUniformSetting(i);
                const auto& uniform = this->GetUniform(setting.name);
                if (uniform.location == -1)
                    continue;

                const auto& value = setting.value;
                const auto location = uniform.location;

                // if the glUnifromXYZ gives GL_INVALID_OPERATION a possible
                // cause is using wrong API for the uniform. for example
                // calling glUniform1f when the uniform is an int.

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
                else if (const auto* ptr = std::get_if<gfx::Color4f>(&value)) {
                    // Assume sRGB encoded color values now. so this is a simple
                    // place to convert to linear and will catch all uses without
                    // breaking the higher level APIs
                    // the cost of the sRGB conversion should be mitigated due to
                    // the hash check to compare to the previous value.
                    const auto& linear = sRGB_Decode(*ptr);
                    GL_CALL(glUniform4f(location, linear.Red(), linear.Green(), linear.Blue(), linear.Alpha()));
                }
                else if (const auto* ptr = std::get_if<glm::mat2>(&value))
                    GL_CALL(glUniformMatrix2fv(location, 1, GL_FALSE /* transpose */, glm::value_ptr(*ptr)));
                else if (const auto* ptr = std::get_if<glm::mat3>(&value))
                    GL_CALL(glUniformMatrix3fv(location, 1, GL_FALSE /* transpose */, glm::value_ptr(*ptr)));
                else if (const auto* ptr = std::get_if<glm::mat4>(&value))
                    GL_CALL(glUniformMatrix4fv(location, 1, GL_FALSE /*transpose*/, glm::value_ptr(*ptr)));
                else BUG("Unhandled shader program uniform type.");
            }
        }


        bool IsValid() const override
        { return mProgram != 0; }

        void SetName(const std::string& name)
        {
            mName = name;
        }


        void BeginFrame()
        {
        }

        GLuint GetHandle() const
        { return mProgram; }
        void SetFrameStamp(size_t frame_number) const
        { mFrameNumber = frame_number; }
        size_t GetFrameStamp() const
        { return mFrameNumber; }


        struct CachedUniform {
            GLuint location = 0;
            uint32_t hash   = 0;
        };

        CachedUniform& GetUniform(const std::string& name) const
        {
            auto it = mUniformCache.find(name);
            if (it != std::end(mUniformCache))
                return it->second;

            auto ret = mGL.glGetUniformLocation(mProgram, name.c_str());
            CachedUniform uniform;
            uniform.location = ret;
            uniform.hash     = 0;
            mUniformCache[name] = uniform;
            return mUniformCache[name];
        }

    private:
        const OpenGLFunctions& mGL;
        GLuint mProgram = 0;
        GLuint mVersion = 0;
        std::string mName;
        mutable std::unordered_map<std::string, CachedUniform> mUniformCache;
        mutable std::size_t mFrameNumber = 0;
    };

    class ShaderImpl : public gfx::Shader
    {
    public:
        explicit ShaderImpl(const OpenGLFunctions& funcs) noexcept
          : mGL(funcs)
        {}

       ~ShaderImpl() override
        {
            if (mShader)
            {
                GL_CALL(glDeleteShader(mShader));
                DEBUG("Deleted shader object. [name='%1', handle=[%2]", mName, mShader);
            }
        }
        void CompileSource(const std::string& source)
        {
            GLenum type = GL_NONE;
            std::stringstream ss(source);
            std::string line;
            while (std::getline(ss, line) && type == GL_NONE)
            {
                if (line.find("gl_Position") != std::string::npos)
                    type = GL_VERTEX_SHADER;
                else if (line.find("gl_FragColor") != std::string::npos ||
                         line.find("fragOutColor") != std::string::npos)
                    type = GL_FRAGMENT_SHADER;
            }
            if (type == GL_NONE)
            {
                ERROR("Failed to identify shader type. [name='%1']", mName);
                DEBUG("In order for the automatic shader type identification to work your shader must have one of the following:");
                DEBUG("GLSL 100 (ES2) gl_Position => vertex shader");
                DEBUG("GLSL 300 (ES3) gl_Position => vertex shader");
                DEBUG("GLSL 100 (ES2) gl_FragColor => fragment shader");
                DEBUG("GLSL 300 (ES3) fragOutColor => fragment shader");
                return;
            }

            GLint status = 0;
            GLint shader = mGL.glCreateShader(type);
            DEBUG("Created new GL shader object. [name='%1', type='%2']", mName, GLEnumToStr(type));

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
                ERROR("Shader compile error. [name='%1', info='%2']", mName, compile_info);

                std::stringstream ss(source);
                std::string line;
                while (std::getline(ss, line))
                {
                    DEBUG("%1", line);
                }
                mError = compile_info;
                return;
            }
            else
            {
                DEBUG("Shader was built successfully. [name='%1', info='%2']", mName, compile_info);
            }
            mShader = shader;
        }


        bool IsValid() const override
        { return mShader != 0; }
        std::string GetName() const override
        { return mName; }
        std::string GetError() const override
        { return mError; }

        inline void SetName(std::string name) noexcept
        { mName = std::move(name); }

        inline GLuint GetHandle() const noexcept
        { return mShader; }

    private:
        const OpenGLFunctions& mGL;

    private:
        GLuint mShader  = 0;
        GLuint mVersion = 0;
        std::string mName;
        std::string mError;
    };
    struct Extensions;

    class FramebufferImpl : public gfx::Framebuffer
    {
    public:
        FramebufferImpl(std::string name, const OpenGLFunctions& funcs, OpenGLES2GraphicsDevice& device)
          : mName(std::move(name))
          , mGL(funcs)
          , mDevice(device)
        {}
       ~FramebufferImpl() override
        {
            for (auto& texture : mTextures)
            {
                if (!texture)
                    continue;

                for (size_t unit = 0; unit < mDevice.mTextureUnits.size(); ++unit)
                {
                    if (mDevice.mTextureUnits[unit].texture == texture.get())
                    {
                        mDevice.mTextureUnits[unit].texture = nullptr;
                        break;
                    }
                }
                mTextures.clear();
            }

            if (mDepthBuffer)
            {
                GL_CALL(glDeleteRenderbuffers(1, &mDepthBuffer));
            }
            for (auto buffer : mMultisampleColorBuffers)
            {
                GL_CALL(glDeleteRenderbuffers(1, &buffer.handle));
            }

            if (mHandle)
            {
                GL_CALL(glDeleteFramebuffers(1, &mHandle));
                DEBUG("Deleted frame buffer object. [name='%1', handle=%2]", mName, mHandle);
            }
        }
        void SetConfig(const Config& conf) override
        {
            ASSERT(conf.color_target_count >= 1);

            // we don't allow the config to be changed after it has been created.
            // todo: delete the SetConfig API and take the FBO config in the
            // device level API to create the FBO.
            if (mHandle)
            {
                ASSERT(mConfig.format == conf.format);
                ASSERT(mConfig.msaa   == conf.msaa);
                ASSERT(mConfig.color_target_count == conf.color_target_count);

                // the size can change after the FBO has been created
                // but only when the format is ColorRGBA8.
                ASSERT(mConfig.format == Format::ColorRGBA8);
            }

            mConfig = conf;
            mClientTextures.resize(mConfig.color_target_count);
            mTextures.resize(mConfig.color_target_count);
        }

        void SetColorTarget(gfx::Texture* texture, ColorAttachment attachment) override
        {
            const auto index = static_cast<uint8_t>(attachment);

            ASSERT(index < mConfig.color_target_count);

            if (texture == mClientTextures[index])
                return;

            mClientTextures[index] = static_cast<TextureImpl*>(texture);

            // if we have a client texture the client texture drives the FBO size.
            // Otherwise the FBO size is based on the size set in the FBO config.
            //
            // The render target (and the resolve target) textures are allowed
            // to change during the lifetime of the FBO but when the texture is
            // changed after the FBO has been created the texture size must match
            // the size used to create the other attachments (if any)

            if (mClientTextures[index])
            {
                const auto width  = mClientTextures[index]->GetWidth();
                const auto height = mClientTextures[index]->GetHeight();

                // don't allow zero size texture.
                ASSERT(width && height);

                // if the FBO has been created and the format is such that there
                // are other attachments then the client texture size must
                // match the size of the other attachments. otherwise the FBO
                // is in invalid state.
                if (mHandle && (mConfig.format != Format::ColorRGBA8))
                {
                    ASSERT(width == mConfig.width);
                    ASSERT(height == mConfig.height);
                }
            }

            // check that every client provided texture has the same size.
            unsigned width  = 0;
            unsigned height = 0;
            for (size_t i=0; i<mClientTextures.size(); ++i)
            {
                if (!mClientTextures[i])
                    continue;

                if (width == 0 && height == 0)
                {
                    width = mClientTextures[i]->GetWidth();
                    height = mClientTextures[i]->GetHeight();
                }
                else
                {
                    ASSERT(mClientTextures[i]->GetWidth() == width);
                    ASSERT(mClientTextures[i]->GetHeight() == height);
                }
            }
        }

        void Resolve(gfx::Texture** color, ColorAttachment attachment) const override
        {
            const auto index = static_cast<uint8_t>(attachment);

            // resolve the MSAA render buffer into a texture target with glBlitFramebuffer
            // the insane part here is that we need a *another* frame buffer for resolving
            // the multisampled color attachment into a texture.
            if (const auto samples = GetSamples())
            {
                auto* resolve_target = GetColorBufferTexture(index);

                FramebufferImpl* fbo = static_cast<FramebufferImpl*>(mDevice.FindFramebuffer("ResolveFBO"));
                if (fbo == nullptr)
                {
                    fbo = static_cast<FramebufferImpl*>(mDevice.MakeFramebuffer("ResolveFBO"));
                    gfx::Framebuffer::Config config;
                    config.width  = 0; // irrelevant since using a texture target.
                    config.height = 0;
                    config.format = gfx::Framebuffer::Format::ColorRGBA8;
                    config.msaa   = gfx::Framebuffer::MSAA::Disabled;
                    config.color_target_count = 1;
                    fbo->SetConfig(config);
                    fbo->SetColorTarget(resolve_target, gfx::Framebuffer::ColorAttachment::Attachment0);
                    fbo->Create();
                    fbo->Complete();
                }
                else
                {
                    fbo->SetColorTarget(resolve_target, gfx::Framebuffer::ColorAttachment::Attachment0);
                    fbo->Complete();
                }

                // See the comments in Complete() regarding the glDrawBuffers which is
                // the other half required to deal with multiple color attachments
                // in multisampled FBO
                ASSERT(mGL.glReadBuffer);

                const auto width  = resolve_target->GetWidth();
                const auto height = resolve_target->GetHeight();

                GL_CALL(glBindFramebuffer(GL_READ_FRAMEBUFFER, mHandle));
                GL_CALL(glReadBuffer(GL_COLOR_ATTACHMENT0 + index));

                const GLenum draw_buffers = GL_COLOR_ATTACHMENT0;
                GL_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo->mHandle));
                GL_CALL(glDrawBuffers(1, &draw_buffers));
                GL_CALL(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));

                fbo->SetFrameStamp(mFrameNumber);
                fbo->Resolve(nullptr, gfx::Framebuffer::ColorAttachment::Attachment0);
                fbo->SetColorTarget(nullptr, gfx::Framebuffer::ColorAttachment::Attachment0);

                if (color)
                    *color = resolve_target;
            }
            else
            {
                auto* texture = GetColorBufferTexture(index);
                if (color)
                    *color = texture;
            }
        }

        unsigned GetWidth() const override
        {
            if (mClientTextures.empty())
                return mConfig.width;

            return mClientTextures[0]->GetWidth();
        }
        unsigned GetHeight() const override
        {
            if (mClientTextures.empty())
                return mConfig.height;

            return mClientTextures[0]->GetHeight();
        }
        Format GetFormat() const override
        {
            return mConfig.format;
        }

        bool Complete()
        {
            // in case of a multisampled FBO the color attachment is a multisampled render buffer
            // and the resolve client texture will be the *resolve* target in the blit framebuffer operation.
            if (const auto samples = GetSamples())
            {
                CreateColorBufferTextures();
                const auto* resolve_target = GetColorBufferTexture(0);
                const unsigned width  = resolve_target->GetWidth();
                const unsigned height = resolve_target->GetHeight();

                // this should not leak anything since we only allow the
                // number of color targets to be set once in the SetConfig
                // thus this vector is only ever resized once.
                mMultisampleColorBuffers.resize(mConfig.color_target_count);

                for (unsigned i=0; i<mConfig.color_target_count; ++i)
                {
                    auto& buff = mMultisampleColorBuffers[i];
                    if (!buff.handle)
                        GL_CALL(glGenRenderbuffers(1, &buff.handle));

                    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, buff.handle));

                    // GL ES3 reference pages under glRenderBufferStorageMultiple lists the table of formats but
                    // this table doesn't include the information about which formats are "color renderable".
                    // Check the ES3 spec under "3.3 TEXTURES" (p.180) or the ES3 reference pages under glTexStorage2D.
                    // https://registry.khronos.org/OpenGL-Refpages/es3.0/

                    if ((buff.width != width) || (buff.height != height))
                    {
                        GL_CALL(glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_SRGB8_ALPHA8, width, height));
                        buff.width  = width;
                        buff.height = height;
                        DEBUG("Allocated multi-sampled render buffer storage. [vbo='%1', size=%2x%3]", mName, width, height);
                    }
                    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mHandle));
                    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER, buff.handle));
                }
            }
            else
            {
                CreateColorBufferTextures();
                for (unsigned i=0; i<mConfig.color_target_count; ++i)
                {
                    auto* color_target = GetColorBufferTexture(i);
                    // in case of a single sampled FBO the resolve target can be used directly
                    // as the color attachment in the FBO.
                    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mHandle));
                    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, color_target->GetHandle(), 0));
                }
            }


            // trying to render to multiple color attachments without platform
            // support is a BUG. The device client is responsible for taking
            // an alternative rendering path when there's no support
            // for multiple color attachments.
            // This API is only available starting from GL ES3 or WebGL2.
            //
            // It's also available with GL_EXT_draw_buffers extension but
            // the problem is that there's no support for *glReadBuffer*
            //
            // We could choose to have single sampled FBO with ES2 / WebGL1
            // with the GL_EXT_draw_buffers extension however.  But this
            // isn't done now since the idea is to move forward to ES3 and
            // ES3 shaders too (where not yet done)
            ASSERT(mGL.glDrawBuffers);

            std::vector<GLenum> draw_buffers;

            for (unsigned i = 0; i < mConfig.color_target_count; ++i)
            {
                draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + i);
            }
            GL_CALL(glDrawBuffers(draw_buffers.size(), &draw_buffers[0]));

            // possible FBO *error* statuses are: INCOMPLETE_ATTACHMENT, INCOMPLETE_DIMENSIONS and INCOMPLETE_MISSING_ATTACHMENT
            // we're treating these status codes as BUGS in the engine code that is trying to create the
            // frame buffer object and has violated the frame buffer completeness requirement or other
            // creation parameter constraints.
            const auto ret = mGL.glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (ret == GL_FRAMEBUFFER_COMPLETE)
                return true;
            else if (ret == GL_FRAMEBUFFER_UNSUPPORTED)
                ERROR("Unsupported FBO configuration. [name='%1']", mName);
            else if (ret == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT) BUG("Incomplete FBO attachment.");
            else if (ret == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS) BUG("Incomplete FBO dimensions.");
            else if (ret == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT) BUG("Incomplete FBO, missing attachment.");
            else if (ret == GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE) BUG("Incomplete FBO, wrong sample counts.");
            return false;
        }

        bool Create()
        {
            ASSERT(mHandle == 0);

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

            CreateColorBufferTextures();

            auto* texture = GetColorBufferTexture(0);

            const auto version = mDevice.mContext->GetVersion();
            const auto xres = texture->GetWidth();
            const auto yres = texture->GetHeight();
            const auto samples = GetSamples();

            GL_CALL(glGenFramebuffers(1, &mHandle));
            GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mHandle));

            // all the calls to bind the texture target to the framebuffer have been
            // removed to Complete() function and are here for reference only.
            // The split between Create() and Complete() allows the same FBO object
            // to be reused with a different target texture.
            if (mConfig.format == Format::ColorRGBA8)
            {
                // for posterity
                //GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColor->GetHandle(), 0));
            }
            else if (mConfig.format == Format::ColorRGBA8_Depth16)
            {
                GL_CALL(glGenRenderbuffers(1, &mDepthBuffer));
                GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, mDepthBuffer));
                if (samples)
                    GL_CALL(glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT16, xres, yres));
                else GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, xres, yres));
                GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer));
            }
            else if (mConfig.format == Format::ColorRGBA8_Depth24_Stencil8)
            {
                if (version == dev::Context::Version::OpenGL_ES2)
                {
                    ASSERT(samples == 0);

                    if (!mDevice.mExtensions.OES_packed_depth_stencil)
                    {
                        ERROR("Failed to create FBO. OES_packed_depth_stencil extension was not found. [name='%1']", mName);
                        return false;
                    }
                    GL_CALL(glGenRenderbuffers(1, &mDepthBuffer));
                    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, mDepthBuffer));
                    GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, xres, yres));
                    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer));
                    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer));
                }
                else if (version == dev::Context::Version::WebGL_1)
                {
                    ASSERT(samples == 0);

                    // the WebGL spec doesn't actually mention the bit depths for the packed
                    // depth+stencil render buffer and the API exposed GLenum is GL_DEPTH_STENCIL 0x84F9
                    // which however is the same as GL_DEPTH_STENCIL_OES from OES_packed_depth_stencil
                    // So, I'm assuming here that the format is in fact 24bit depth with 8bit stencil.
                    GL_CALL(glGenRenderbuffers(1, &mDepthBuffer));
                    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, mDepthBuffer));
                    GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, WEBGL_DEPTH_STENCIL, xres, yres));
                    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, WEBGL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer));
                }
                else if (version == dev::Context::Version::OpenGL_ES3 || version == dev::Context::Version::WebGL_2)
                {
                    GL_CALL(glGenRenderbuffers(1, &mDepthBuffer));
                    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, mDepthBuffer));
                    if (samples)
                        GL_CALL(glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, xres, yres));
                    else GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, xres, yres));
                    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer));
                    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer));
                }
            }

            // commit the size
            mConfig.width  = xres;
            mConfig.height = yres;

            DEBUG("Created new frame buffer object. [name='%1', width=%2, height=%3, format=%4, samples=%5]", mName, xres, yres, mConfig.format, samples);
            return true;
        }
        void SetFrameStamp(size_t stamp)
        {
            for (auto& texture : mTextures)
            {
                if (texture)
                    texture->SetFrameStamp(stamp);
            }

            for (auto* texture : mClientTextures)
            {
                if (texture)
                    texture->SetFrameStamp(stamp);
            }

            mFrameNumber = stamp;
        }
        inline size_t GetFrameStamp() const noexcept
        {
            return mFrameNumber;
        }
        inline GLuint GetHandle() const noexcept
        {
            return mHandle;
        }
        inline bool IsReady() const noexcept
        {
            return mHandle != 0;
        }

        unsigned GetSamples() const noexcept
        {
            if (mConfig.msaa == MSAA::Disabled)
                return 0;

            const auto version = mDevice.mContext->GetVersion();
            if (version == dev::Context::Version::OpenGL_ES2 ||
                version == dev::Context::Version::WebGL_1)
                return 0;
            else if (version == dev::Context::Version::OpenGL_ES3 ||
                     version == dev::Context::Version::WebGL_2)
                return 4;
            else BUG("Missing OpenGL ES version.");
            return 0;
        }

        void CreateColorBufferTextures()
        {
            mClientTextures.resize(mConfig.color_target_count);
            mTextures.resize(mConfig.color_target_count);

            for (unsigned i=0; i<mConfig.color_target_count; ++i)
            {
                if (mClientTextures[i])
                    continue;

                // we must have FBO width and height for creating
                // the color buffer texture.
                ASSERT(mConfig.width && mConfig.height);

                if (!mTextures[i])
                {
                    const auto& name = "FBO/" + mName + "/Color" + std::to_string(i);
                    mTextures[i] = std::make_unique<TextureImpl>(name, mGL, mDevice);
                    mTextures[i]->SetName(name);
                    mTextures[i]->Allocate(mConfig.width, mConfig.height, gfx::Texture::Format::sRGBA);
                    mTextures[i]->SetFilter(gfx::Texture::MinFilter::Linear);
                    mTextures[i]->SetFilter(gfx::Texture::MagFilter::Linear);
                    mTextures[i]->SetWrapX(gfx::Texture::Wrapping::Clamp);
                    mTextures[i]->SetWrapY(gfx::Texture::Wrapping::Clamp);
                    DEBUG("Allocated new FBO color buffer (texture) target. [name='%1', width=%2, height=%3]]", mName,
                          mConfig.width, mConfig.height);
                }
                else
                {
                    const auto width  = mTextures[i]->GetWidth();
                    const auto height = mTextures[i]->GetHeight();
                    if (width != mConfig.width || height != mConfig.height)
                        mTextures[i]->Allocate(mConfig.width, mConfig.height, gfx::Texture::Format::sRGBA);
                }

            }
        }

        TextureImpl* GetColorBufferTexture(unsigned index) const
        {
            if (mClientTextures[index])
                return mClientTextures[index];

            return mTextures[index].get();
        }

        const gfx::Texture* GetClientTexture(unsigned index) const
        {
            return mClientTextures[index];
        }
        unsigned GetClientTextureCount() const
        {
            return mConfig.color_target_count;
        }

    private:
        const std::string mName;
        const OpenGLFunctions& mGL;
        OpenGLES2GraphicsDevice& mDevice;

        // Texture target that we allocate when the use hasn't
        // provided a client texture. In case of a single sampled
        // FBO this is used directly as the color attachment
        // in case of multiple sampled this will be used as the
        // resolve target.
        std::vector<std::unique_ptr<TextureImpl>> mTextures;

        // Client provided texture(s) that will ultimately contain
        // the rendered result.
        std::vector<TextureImpl*> mClientTextures;

        GLuint mHandle = 0;
        // this is either only depth or packed depth+stencil
        GLuint mDepthBuffer = 0;
        // In case of multisampled FBO the color buffer is a MSAA
        // render buffer which then gets resolved (blitted) into the
        // associated color target texture.
        struct MSAARenderBuffer {
            GLuint handle = 0;
            GLuint width  = 0;
            GLuint height = 0;
        };
        std::vector<MSAARenderBuffer> mMultisampleColorBuffers;

        Config mConfig;
        std::size_t mFrameNumber = 0;
    };
private:
    std::map<std::string, std::shared_ptr<gfx::GeometryInstance>> mInstances;
    std::map<std::string, std::shared_ptr<gfx::Geometry>> mGeoms;
    std::map<std::string, std::shared_ptr<gfx::Shader>> mShaders;
    std::map<std::string, std::shared_ptr<gfx::Program>> mPrograms;
    std::map<std::string, std::unique_ptr<gfx::Texture>> mTextures;
    std::map<std::string, std::unique_ptr<gfx::Framebuffer>> mFBOs;
    std::shared_ptr<dev::Context> mContextImpl;
    dev::Context* mContext = nullptr;
    std::size_t mFrameNumber = 0;
    OpenGLFunctions mGL;
    MinFilter mDefaultMinTextureFilter = MinFilter::Nearest;
    MagFilter mDefaultMagTextureFilter = MagFilter::Nearest;
    // texture units and their current settings.
    mutable TextureUnits mTextureUnits;

    struct BufferObject {
        gfx::Geometry::Usage usage = gfx::Geometry::Usage::Static;
        GLuint name     = 0;
        size_t capacity = 0;
        size_t offset   = 0;
        size_t refcount = 0;
    };
    // vertex buffers at index 0 and index buffers at index 1
    std::vector<BufferObject> mBuffers[2];
    struct Extensions {
        bool EXT_sRGB = false;
        bool OES_packed_depth_stencil = false;
        // support multiple color attachments in GL ES2.
        bool GL_EXT_draw_buffers = false;
    } mExtensions;
};

} // namespace

namespace dev {

std::shared_ptr<Device> CreateDevice(std::shared_ptr<dev::Context> context)
{
    if (context->GetVersion() == Context::Version::OpenGL_ES2 ||
        context->GetVersion() == Context::Version::OpenGL_ES3)
        return std::make_shared<OpenGLES2GraphicsDevice>(context);
    else if (context->GetVersion() == Context::Version::WebGL_1 ||
             context->GetVersion() == Context::Version::WebGL_2)
        return std::make_shared<OpenGLES2GraphicsDevice>(context);
    else BUG("No such device implemented.");
    return nullptr;
}
std::shared_ptr<Device> CreateDevice(dev::Context* context)
{
    if (context->GetVersion() == Context::Version::OpenGL_ES2 ||
        context->GetVersion() == Context::Version::OpenGL_ES3)
        return std::make_shared<OpenGLES2GraphicsDevice>(context);
    else if (context->GetVersion() == Context::Version::WebGL_1 ||
             context->GetVersion() == Context::Version::WebGL_2)
        return std::make_shared<OpenGLES2GraphicsDevice>(context);
    else BUG("No such device implemented.");
    return nullptr;
}

} // namespace
