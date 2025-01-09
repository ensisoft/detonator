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
#include "device/graphics.h"
#include "device/vertex.h"

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
    PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
    PFNGLVERTEXATTRIBDIVISORPROC     glVertexAttribDivisor;
    PFNGLGETSTRINGPROC               glGetString;
    PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation;
    PFNGLGETUNIFORMBLOCKINDEXPROC    glGetUniformBlockIndex;
    PFNGLUNIFORMBLOCKBINDINGPROC     glUniformBlockBinding;
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
    PFNGLBINDBUFFERRANGEPROC         glBindBufferRange;
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
                              , public dev::GraphicsDevice
{

private:
    struct CachedUniform {
        GLuint location = 0;
        uint32_t hash   = 0;
    };
    struct BufferObject {
        dev::BufferUsage usage = dev::BufferUsage::Static;
        GLuint name     = 0;
        size_t capacity = 0;
        size_t offset   = 0;
        size_t refcount = 0;
    };
    struct TextureState {
        GLenum wrap_x = GL_NONE;
        GLenum wrap_y = GL_NONE;
        GLenum min_filter = GL_NONE;
        GLenum mag_filter = GL_NONE;
        bool has_mips = false;
    };
    struct FramebufferState {
        // this is either only depth or packed depth+stencil
        GLuint depth_buffer = 0;
        // In case of multisampled FBO the color buffer is a MSAA
        // render buffer which then gets resolved (blitted) into the
        // associated color target texture.
        struct MSAARenderBuffer {
            GLuint handle = 0;
            GLuint width  = 0;
            GLuint height = 0;
        };
        std::vector<MSAARenderBuffer> multisample_color_buffers;
    };
    using UniformCache = std::unordered_map<std::string, CachedUniform>;

    std::shared_ptr<dev::Context> mContextImpl;
    dev::Context* mContext = nullptr;

    mutable std::unordered_map<unsigned, UniformCache> mUniformCache;
    mutable std::unordered_map<unsigned, UniformCache> mUniformBlockCache;
    mutable std::unordered_map<unsigned, TextureState> mTextureState;
    mutable std::unordered_map<unsigned, FramebufferState> mFramebufferState;

    // vertex buffers at index 0 and index buffers at index 1, uniform buffers at index 2
    std::vector<BufferObject> mBuffers[3];

    unsigned mResolveFbo = 0;
    unsigned mTempTextureUnitIndex = 0;
    unsigned mTextureUnitCount = 0;
    unsigned mUniformBufferOffsetAlignment = 0;

    OpenGLFunctions mGL;

    struct Extensions {
        bool EXT_sRGB = false;
        bool OES_packed_depth_stencil = false;
        // support multiple color attachments in GL ES2.
        bool GL_EXT_draw_buffers = false;
    } mExtensions;
private:

    CachedUniform& GetUniformFromCache(const dev::GraphicsProgram& program, const std::string& name) const
    {
        // todo: replace with a better faster key
        auto& cache = mUniformCache[program.handle];

        auto it = cache.find(name);
        if (it != std::end(cache))
            return it->second;

        auto ret = mGL.glGetUniformLocation(program.handle, name.c_str());
        CachedUniform uniform;
        uniform.location = ret;
        uniform.hash     = 0;
        cache[name] = uniform;
        return cache[name];
    }
    CachedUniform& GetUniformBlockFromCache(const dev::GraphicsProgram& program, const std::string& name) const
    {
        auto& cache = mUniformBlockCache[program.handle];

        auto it = cache.find(name);
        if (it != std::end(cache))
            return it->second;

        auto ret = mGL.glGetUniformBlockIndex(program.handle, name.c_str());
        CachedUniform block;
        block.location = ret;
        block.hash = 0;
        cache[name] = block;
        return cache[name];
    }

public:
    explicit OpenGLES2GraphicsDevice(dev::Context* context) noexcept
        : mContext(context)
        , mDevice(this)
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
        RESOLVE(glDisableVertexAttribArray);
        RESOLVE(glVertexAttribDivisor);
        RESOLVE(glGetString);
        RESOLVE(glGetUniformLocation);
        RESOLVE(glGetUniformBlockIndex);
        RESOLVE(glUniformBlockBinding);
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
        RESOLVE(glBindBufferRange);
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
        GLint uniform_buffer_offset_alignment = 0;
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
        GL_CALL(glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uniform_buffer_offset_alignment));
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
            DEBUG("UBO offset alignment: %1", uniform_buffer_offset_alignment);
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
            INFO("UBO offset alignment: %1", uniform_buffer_offset_alignment);
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

        mTextureUnits.resize(max_texture_units-1);
        // we use this for uploading textures and generating mips
        // so we don't need to play stupid games with the "actual"
        // texture units.
        mTempTextureUnitIndex = max_texture_units-1;
        mTextureUnitCount = max_texture_units-1; // see the temp

        // set some initial state
        GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
        GL_CALL(glDisable(GL_DEPTH_TEST));
        GL_CALL(glEnable(GL_CULL_FACE));
        GL_CALL(glCullFace(GL_BACK));
        GL_CALL(glFrontFace(GL_CCW));

        have_printed_info = true;
        mUniformBufferOffsetAlignment = (unsigned)uniform_buffer_offset_alignment;
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

        if (mResolveFbo)
        {
            GL_CALL(glDeleteFramebuffers(1, &mResolveFbo));
        }

        for (auto& buffer : mBuffers[0])
        {
            GL_CALL(glDeleteBuffers(1, &buffer.name));
        }
        for (auto& buffer : mBuffers[1])
        {
            GL_CALL(glDeleteBuffers(1, &buffer.name));
        }
        for (auto& buffer : mBuffers[2])
        {
            GL_CALL(glDeleteBuffers(1, &buffer.name));
        }
    }

    static size_t BufferIndex(dev::BufferType type)
    {
        if (type == dev::BufferType::VertexBuffer)
            return 0;
        else if (type == dev::BufferType::IndexBuffer)
            return 1;
        else if (type == dev::BufferType::UniformBuffer)
            return 2;
        BUG("Bug on buffer index.");
        return 0;
    }

    static GLenum GetBufferEnum(dev::BufferType type)
    {
        if (type == dev::BufferType::VertexBuffer)
            return GL_ARRAY_BUFFER;
        else if (type == dev::BufferType::IndexBuffer)
            return GL_ELEMENT_ARRAY_BUFFER;
        else if (type == dev::BufferType::UniformBuffer)
            return GL_UNIFORM_BUFFER;
        BUG("Bug on buffer type.");
        return 0;
    }
    static GLenum GetWrappingMode(dev::TextureWrapping wrapping)
    {
        if (wrapping == dev::TextureWrapping::Clamp)
            return GL_CLAMP_TO_EDGE;
        else if (wrapping == dev::TextureWrapping::Repeat)
            return GL_REPEAT;
        else if (wrapping == dev::TextureWrapping::Mirror)
            return GL_MIRRORED_REPEAT;
        else BUG("Bug on GL texture wrapping mode.");
        return GL_NONE;
    }
    static GLenum GetTextureMinFilter(dev::TextureMinFilter filter)
    {
        if (filter == dev::TextureMinFilter::Nearest)
            return GL_NEAREST;
        else if (filter == dev::TextureMinFilter::Linear)
            return GL_LINEAR;
        else if (filter == dev::TextureMinFilter::Mipmap)
            return GL_NEAREST_MIPMAP_NEAREST;
        else if (filter == dev::TextureMinFilter::Bilinear)
            return GL_NEAREST_MIPMAP_LINEAR;
        else if (filter == dev::TextureMinFilter::Trilinear)
            return GL_LINEAR_MIPMAP_LINEAR;
        else BUG("Bug on min texture filter");
        return GL_NONE;
    }
    static GLenum GetTextureMagFilter(dev::TextureMagFilter filter)
    {
        if (filter == dev::TextureMagFilter::Nearest)
            return GL_NEAREST;
        else if (filter == dev::TextureMagFilter::Linear)
            return GL_LINEAR;
        else BUG("Bug on mag texture filter.");
        return GL_NONE;
    }
    static GLenum GetDrawMode(dev::DrawType draw)
    {
        if (draw == dev::DrawType::Triangles)
            return GL_TRIANGLES;
        else if (draw == dev::DrawType::Points)
            return GL_POINTS;
        else if (draw == dev::DrawType::TriangleFan)
            return GL_TRIANGLE_FAN;
        else if (draw == dev::DrawType::Lines)
            return GL_LINES;
        else if (draw == dev::DrawType::LineLoop)
            return GL_LINE_LOOP;
        else BUG("Bug on draw primitive.");
        return GL_NONE;
    }
    static GLenum GetIndexType(dev::IndexType type)
    {
        if (type == dev::IndexType::Index16)
            return GL_UNSIGNED_SHORT;
        else if (type == dev::IndexType::Index32)
            return GL_UNSIGNED_INT;
        else BUG("Bug on index type.");
        return GL_NONE;
    }
    static GLenum GetEnum(dev::StencilFunc func)
    {
        using Func = State::StencilFunc;

        if (func == Func::Disabled)
            return GL_NONE;
        else if (func == Func::PassAlways)
            return GL_ALWAYS;
        else if (func == Func::PassNever)
            return GL_NEVER;
        else if (func == Func::RefIsLess)
            return GL_LESS;
        else if (func == Func::RefIsLessOrEqual)
            return GL_LEQUAL;
        else if (func == Func::RefIsMore)
            return GL_GREATER;
        else if (func == Func::RefIsMoreOrEqual)
            return GL_GEQUAL;
        else if (func == Func::RefIsEqual)
            return GL_EQUAL;
        else if (func == Func::RefIsNotEqual)
            return GL_NOTEQUAL;
        else BUG("Bug on stencil func");
        return GL_NONE;
    }
    static GLenum GetEnum(dev::StencilOp op)
    {
        using Op = State::StencilOp;

        if (op == Op::DontModify)
            return GL_KEEP;
        else if (op == Op::WriteZero)
            return GL_ZERO;
        else if (op == Op::WriteRef)
            return GL_REPLACE;
        else if (op == Op::Increment)
            return GL_INCR;
        else if (op == Op::Decrement)
            return GL_DECR;
        else BUG("Bug on stencil op");
        return GL_NONE;
    }

    struct TextureFormat {
        GLenum sizeFormat;
        GLenum baseFormat;
    };

    TextureFormat GetTextureFormat(dev::TextureFormat format) const
    {
        const auto version = mContext->GetVersion();

        GLenum sizeFormat = 0;
        GLenum baseFormat = 0;
        switch (format)
        {
            case dev::TextureFormat::sRGB:
                if (version == dev::Context::Version::WebGL_1 || version == dev::Context::Version::OpenGL_ES2)
                {
                    sizeFormat = GL_SRGB_EXT;
                    baseFormat = GL_RGB; //GL_SRGB_EXT;
                }
                else if (version == dev::Context::Version::WebGL_2 || version == dev::Context::Version::OpenGL_ES3)
                {
                    sizeFormat = GL_SRGB8;
                    baseFormat = GL_RGB;
                }
                else BUG("Unknown OpenGL ES version.");
                break;
            case dev::TextureFormat::sRGBA:
                if (version == dev::Context::Version::WebGL_1 || version == dev::Context::Version::OpenGL_ES2)
                {
                    sizeFormat = GL_SRGB_ALPHA_EXT;
                    baseFormat = GL_RGBA; //GL_SRGB_ALPHA_EXT;
                }
                else if (version == dev::Context::Version::WebGL_2 || version == dev::Context::Version::OpenGL_ES3)
                {
                    sizeFormat = GL_SRGB8_ALPHA8;
                    baseFormat = GL_RGBA;
                }
                else BUG("Unknown OpenGL ES version.");
                break;
            case dev::TextureFormat::RGB:
                sizeFormat = GL_RGB;
                baseFormat = GL_RGB;
                break;
            case dev::TextureFormat::RGBA:
                sizeFormat = GL_RGBA;
                baseFormat = GL_RGBA;
                break;
            case dev::TextureFormat::AlphaMask:
                // when sampling R = G = B = 0.0 and A is the alpha value from here.
                sizeFormat = GL_ALPHA;
                baseFormat = GL_ALPHA;
                break;
            default:
                BUG("Unknown texture format.");
                break;
        }

        if (version == dev::Context::Version::OpenGL_ES2 || version == dev::Context::Version::WebGL_1)
        {
            if (format == dev::TextureFormat::sRGB && !mExtensions.EXT_sRGB)
            {
                sizeFormat = GL_RGB;
                baseFormat = GL_RGB;
                WARN("Treating sRGB texture as RGB texture in the absence of EXT_sRGB.");
            }
            else if (format == dev::TextureFormat::sRGBA && !mExtensions.EXT_sRGB)
            {
                sizeFormat = GL_RGBA;
                baseFormat = GL_RGBA;
                WARN("Treating sRGBA texture as RGBA texture in the absence of EXT_sRGB.");
            }
        }
        TextureFormat ret;
        ret.baseFormat = baseFormat;
        ret.sizeFormat = sizeFormat;
        return ret;
    }


    void DeleteShader(const dev::GraphicsShader& shader) override
    {
        GL_CALL(glDeleteShader(shader.handle));
    }
    void DeleteProgram(const dev::GraphicsProgram& program) override
    {
        GL_CALL(glDeleteProgram(program.handle));

        mUniformCache.erase(program.handle);
        mUniformBlockCache.erase(program.handle);
    }

    dev::Framebuffer GetDefaultFramebuffer() const override
    {
        static dev::Framebuffer framebuffer;
        framebuffer.handle = 0;
        // todo: width, height, format
        return framebuffer;
    }

    dev::Framebuffer CreateFramebuffer(const dev::FramebufferConfig& config) override
    {
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

        const auto version = mContext->GetVersion();

        GLuint handle;
        GL_CALL(glGenFramebuffers(1, &handle));
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, handle));

        unsigned samples = 0;
        if (config.msaa)
        {
            const auto version = mContext->GetVersion();
            if (version == dev::Context::Version::OpenGL_ES2 ||
                version == dev::Context::Version::WebGL_1)
                samples = 0;
            else if (version == dev::Context::Version::OpenGL_ES3 ||
                     version == dev::Context::Version::WebGL_2)
                samples = 4;
            else BUG("Missing OpenGL ES version.");
        }

        auto& framebuffer_state = mFramebufferState[handle];

        // all the calls to bind the texture target to the framebuffer have been
        // removed to Complete() function and are here for reference only.
        // The split between Create() and Complete() allows the same FBO object
        // to be reused with a different target texture.
        if (config.format == dev::FramebufferFormat::ColorRGBA8)
        {
            // for posterity
            //GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColor->GetHandle(), 0));
        }
        else if (config.format == dev::FramebufferFormat::ColorRGBA8_Depth16)
        {
            GL_CALL(glGenRenderbuffers(1, &framebuffer_state.depth_buffer));
            GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, framebuffer_state.depth_buffer));
            if (samples)
                GL_CALL(glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT16, config.width, config.height));
            else GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, config.width, config.height));
            GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, framebuffer_state.depth_buffer));
        }
        else if (config.format == dev::FramebufferFormat::ColorRGBA8_Depth24_Stencil8)
        {
            if (version == dev::Context::Version::OpenGL_ES2)
            {
                ASSERT(samples == 0);

                if (!mExtensions.OES_packed_depth_stencil)
                {
                    ERROR("Failed to create FBO. OES_packed_depth_stencil extension was not found.");
                    dev::Framebuffer fbo;
                    fbo.handle = 0;
                    fbo.width  = 0;
                    fbo.height = 0;
                    fbo.format = dev::FramebufferFormat::Invalid;
                    return fbo;
                }
                GL_CALL(glGenRenderbuffers(1, &framebuffer_state.depth_buffer));
                GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, framebuffer_state.depth_buffer));
                GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, config.width, config.height));
                GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, framebuffer_state.depth_buffer));
                GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, framebuffer_state.depth_buffer));
            }
            else if (version == dev::Context::Version::WebGL_1)
            {
                ASSERT(samples == 0);

                // the WebGL spec doesn't actually mention the bit depths for the packed
                // depth+stencil render buffer and the API exposed GLenum is GL_DEPTH_STENCIL 0x84F9
                // which however is the same as GL_DEPTH_STENCIL_OES from OES_packed_depth_stencil
                // So, I'm assuming here that the format is in fact 24bit depth with 8bit stencil.
                GL_CALL(glGenRenderbuffers(1, &framebuffer_state.depth_buffer));
                GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, framebuffer_state.depth_buffer));
                GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, WEBGL_DEPTH_STENCIL, config.width, config.height));
                GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, WEBGL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, framebuffer_state.depth_buffer));
            }
            else if (version == dev::Context::Version::OpenGL_ES3 || version == dev::Context::Version::WebGL_2)
            {
                GL_CALL(glGenRenderbuffers(1, &framebuffer_state.depth_buffer));
                GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, framebuffer_state.depth_buffer));
                if (samples)
                    GL_CALL(glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, config.width, config.height));
                else GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, config.width, config.height));
                GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, framebuffer_state.depth_buffer));
                GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, framebuffer_state.depth_buffer));
            }
        }
        dev::Framebuffer fbo;
        fbo.handle  = handle;
        fbo.height  = config.height;
        fbo.width   = config.width;
        fbo.format  = config.format;
        fbo.samples = samples;
        return fbo;
    }
    void AllocateRenderTarget(const dev::Framebuffer& framebuffer, unsigned color_attachment,
                              unsigned width, unsigned height) override
    {
        ASSERT(framebuffer.IsValid());
        ASSERT(framebuffer.IsCustom());

        auto& framebuffer_state = mFramebufferState[framebuffer.handle];

        // this should not leak anything since we only allow the
        // number of color targets to be set once in the SetConfig
        // thus this vector is only ever resized once.
        if (framebuffer_state.multisample_color_buffers.size() <= color_attachment)
            framebuffer_state.multisample_color_buffers.resize(color_attachment+1);

        const auto samples = framebuffer.samples;

        auto& buff = framebuffer_state.multisample_color_buffers[color_attachment];
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
            DEBUG("Allocated multi-sampled render buffer storage. [size=%1x%2]", width, height);
        }

        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.handle));
        GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + color_attachment, GL_RENDERBUFFER, buff.handle));
    }
    void BindRenderTargetTexture2D(const dev::Framebuffer& framebuffer, const dev::TextureObject& texture,
                                   unsigned color_attachment) override
    {
        ASSERT(framebuffer.IsValid());
        ASSERT(framebuffer.IsCustom());
        ASSERT(texture.IsValid());
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.handle));
        GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + color_attachment, GL_TEXTURE_2D, texture.handle, 0));
    }
    bool CompleteFramebuffer(const dev::Framebuffer& framebuffer,
                             const std::vector<unsigned>& color_attachments) override
    {
        ASSERT(framebuffer.IsValid());
        ASSERT(framebuffer.IsCustom());

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

        for (unsigned i = 0; i < color_attachments.size(); ++i)
        {
            draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + color_attachments[i]);
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
            return false;
        else if (ret == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT) BUG("Incomplete FBO attachment.");
        else if (ret == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS) BUG("Incomplete FBO dimensions.");
        else if (ret == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT) BUG("Incomplete FBO, missing attachment.");
        else if (ret == GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE) BUG("Incomplete FBO, wrong sample counts.");
        return false;
    }

    void ResolveFramebuffer(const dev::Framebuffer& multisampled_framebuffer,
                            const dev::TextureObject& resolve_target,
                            unsigned color_attachment) override
    {
        ASSERT(multisampled_framebuffer.IsValid());
        ASSERT(multisampled_framebuffer.IsCustom());
        ASSERT(multisampled_framebuffer.IsMultisampled());
        ASSERT(resolve_target.IsValid());
        // See the comments in Complete() regarding the glDrawBuffers which is
        // the other half required to deal with multiple color attachments
        // in multisampled FBO
        ASSERT(mGL.glReadBuffer);

        if (!mResolveFbo)
            GL_CALL(glGenFramebuffers(1, &mResolveFbo));

        const auto draw_buffers = GLenum(GL_COLOR_ATTACHMENT0);
        const auto resolve_width = resolve_target.texture_width;
        const auto resolve_height = resolve_target.texture_height;

        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mResolveFbo));
        GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolve_target.handle, 0));
        GL_CALL(glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampled_framebuffer.handle));
        GL_CALL(glReadBuffer(GL_COLOR_ATTACHMENT0 + color_attachment));
        GL_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mResolveFbo));
        GL_CALL(glDrawBuffers(1, &draw_buffers));
        GL_CALL(glBlitFramebuffer(0, 0, resolve_width, resolve_height, 0, 0, resolve_width, resolve_height,
                                  GL_COLOR_BUFFER_BIT, GL_NEAREST));
    }

    void BindFramebuffer(const dev::Framebuffer& framebuffer) const override
    {
        ASSERT(framebuffer.IsValid());
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.handle));
    }

    void DeleteFramebuffer(const dev::Framebuffer& framebuffer) override
    {
        ASSERT(framebuffer.IsValid());
        ASSERT(framebuffer.IsCustom());

        auto& framebuffer_state = mFramebufferState[framebuffer.handle];
        if (framebuffer_state.depth_buffer)
            GL_CALL(glDeleteRenderbuffers(1, &framebuffer_state.depth_buffer));

        for (auto buffer : framebuffer_state.multisample_color_buffers)
        {
            GL_CALL(glDeleteRenderbuffers(1, &buffer.handle));
        }

        GL_CALL(glDeleteFramebuffers(1, &framebuffer.handle));

        mFramebufferState.erase(framebuffer.handle);
    }

    dev::GraphicsShader CompileShader(const std::string& source, dev::ShaderType type, std::string* compile_info) override
    {
        GLenum gl_type = GL_NONE;
        if (type == dev::ShaderType::VertexShader)
            gl_type = GL_VERTEX_SHADER;
        else if (type == dev::ShaderType::FragmentShader)
            gl_type = GL_FRAGMENT_SHADER;
        else BUG("Bug on graphics shader type.");

        GLint status = 0;
        GLint shader = mGL.glCreateShader(gl_type);

        const char* source_ptr = source.c_str();
        GL_CALL(glShaderSource(shader, 1, &source_ptr, nullptr));
        GL_CALL(glCompileShader(shader));
        GL_CALL(glGetShaderiv(shader, GL_COMPILE_STATUS, &status));

        GLint length = 0;
        GL_CALL(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length));

        compile_info->resize(length);
        GL_CALL(glGetShaderInfoLog(shader, length, nullptr, &(*compile_info)[0]));

        if (status == 0)
        {
            GL_CALL(glDeleteShader(shader));
            return { 0, dev::ShaderType::Invalid };
        }
        return { static_cast<unsigned>(shader), type };
    }

    dev::GraphicsProgram BuildProgram(const std::vector<dev::GraphicsShader>& shaders, std::string* build_info) override
    {
        GLuint program = mGL.glCreateProgram();
        for (const auto& shader : shaders)
        {
            ASSERT(shader.IsValid());
            GL_CALL(glAttachShader(program, shader.GetHandle()));
        }
        GL_CALL(glLinkProgram(program));
        GL_CALL(glValidateProgram(program));

        GLint link_status = 0;
        GLint valid_status = 0;
        GL_CALL(glGetProgramiv(program, GL_LINK_STATUS, &link_status));
        GL_CALL(glGetProgramiv(program, GL_VALIDATE_STATUS, &valid_status));

        GLint length = 0;
        GL_CALL(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length));

        build_info->resize(length);
        GL_CALL(glGetProgramInfoLog(program, length, nullptr, &(*build_info)[0]));

        if (link_status == 0 || valid_status == 0)
        {
            GL_CALL(glDeleteProgram(program));
            return { 0 };
        }
        return { program };
    }

    dev::TextureObject AllocateTexture2D(unsigned texture_width,
                                         unsigned texture_height, dev::TextureFormat format) override
    {
        const auto& internal_format = GetTextureFormat(format);
        const auto texture_border = 0;
        const auto texture_level = 0; // mip level

        GLuint handle = 0;
        GL_CALL(glGenTextures(1, &handle));
        GL_CALL(glActiveTexture(GL_TEXTURE0 + mTempTextureUnitIndex));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, handle));
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, texture_level, internal_format.sizeFormat,
                             texture_width, texture_height, texture_border,
                             internal_format.baseFormat, GL_UNSIGNED_BYTE, nullptr));

        auto& texture_state = mTextureState[handle];
        texture_state.min_filter = GL_NONE;
        texture_state.mag_filter = GL_NONE;
        texture_state.wrap_x     = GL_NONE;
        texture_state.wrap_y     = GL_NONE;
        texture_state.has_mips   = false;

        dev::TextureObject ret;
        ret.handle = handle;
        ret.type   = dev::TextureType::Texture2D;
        ret.texture_width = texture_width;
        ret.texture_height = texture_height;
        return ret;
    }
    dev::TextureObject UploadTexture2D(const void* bytes,
                                       unsigned texture_width,
                                       unsigned texture_height, dev::TextureFormat format) override
    {
        const auto& internal_format = GetTextureFormat(format);
        const auto texture_border = 0;
        const auto texture_level = 0; // mip level

        GLuint handle = 0;
        GL_CALL(glGenTextures(1, &handle));
        GL_CALL(glActiveTexture(GL_TEXTURE0 + mTempTextureUnitIndex));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, handle));
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, texture_level, internal_format.sizeFormat,
                             texture_width, texture_height, texture_border,
                             internal_format.baseFormat, GL_UNSIGNED_BYTE, bytes));

        auto& texture_state = mTextureState[handle];
        texture_state.min_filter = GL_NONE;
        texture_state.mag_filter = GL_NONE;
        texture_state.wrap_x     = GL_NONE;
        texture_state.wrap_y     = GL_NONE;
        texture_state.has_mips   = false;

        dev::TextureObject ret;
        ret.handle = handle;
        ret.type   = dev::TextureType::Texture2D;
        ret.texture_width = texture_width;
        ret.texture_height = texture_height;
        return ret;
    }

    GraphicsDevice::MipStatus GenerateMipmaps(const dev::TextureObject& texture) override
    {
        ASSERT(texture.IsValid());
        ASSERT(texture.texture_width);
        ASSERT(texture.texture_height);

        if (mContext->GetVersion() == dev::Context::Version::WebGL_1)
        {
            if (!base::IsPowerOfTwo(texture.texture_width) || !base::IsPowerOfTwo(texture.texture_height))
                return GraphicsDevice::MipStatus::UnsupportedSize;

            if (texture.format == dev::TextureFormat::sRGB ||
                texture.format == dev::TextureFormat::sRGBA)
                return GraphicsDevice::MipStatus::UnsupportedFormat;
        }
        else if (mContext->GetVersion() == dev::Context::Version::WebGL_2)
        {
            if (texture.format == dev::TextureFormat::sRGB ||
                texture.format == dev::TextureFormat::sRGBA)
                return GraphicsDevice::MipStatus::UnsupportedFormat;
        }
        else if (mContext->GetVersion() == dev::Context::Version::OpenGL_ES2)
        {
            if (texture.format == dev::TextureFormat::sRGB ||
                texture.format == dev::TextureFormat::sRGBA)
                return GraphicsDevice::MipStatus::UnsupportedFormat;
        }
        GL_CALL(glActiveTexture(GL_TEXTURE0 + mTempTextureUnitIndex));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, texture.handle));
        // seems that driver bugs are common regarding sRGB mipmap generation
        // so we're going to unwrap this GL call and assume any error
        // is an error about failing to generate mips because of driver bugs
        mGL.glGenerateMipmap(GL_TEXTURE_2D);
        const auto error = mGL.glGetError();
        if (error != GL_NO_ERROR)
            return GraphicsDevice::MipStatus::Error;

        auto& texture_state = mTextureState[texture.handle];
        texture_state.has_mips = true;

        return GraphicsDevice::MipStatus::Success;
    }

    bool BindTexture2D(const dev::TextureObject& texture, unsigned texture_sampler, unsigned texture_unit,
                       dev::TextureWrapping texture_x_wrap, dev::TextureWrapping texture_y_wrap,
                       dev::TextureMinFilter texture_min_filter, dev::TextureMagFilter texture_mag_filter, BindWarnings* warnings) const override
    {
        ASSERT(texture.IsValid());
        ASSERT(texture.texture_width && texture.texture_height);

        if (texture_unit >= mTextureUnitCount)
        {
            ERROR("Texture unit index exceeds the maximum available texture units. [unit=%1, available=¤2]", texture_unit,
                  mTextureUnitCount);
            return false;
        }

        bool force_clamp_x = false;
        bool force_clamp_y = false;
        bool force_min_linear = false;

        auto internal_texture_min_filter = GetTextureMinFilter(texture_min_filter);
        auto internal_texture_mag_filter = GetTextureMagFilter(texture_mag_filter);
        auto internal_texture_x_wrap = GetWrappingMode(texture_x_wrap);
        auto internal_texture_y_wrap = GetWrappingMode(texture_y_wrap);

        auto& texture_state = mTextureState[texture.handle];

        // do some validation and warning logging if there's something that is wrong.
        if (internal_texture_min_filter == GL_NEAREST_MIPMAP_NEAREST ||
            internal_texture_min_filter == GL_NEAREST_MIPMAP_LINEAR ||
            internal_texture_min_filter == GL_LINEAR_MIPMAP_LINEAR)
        {
            // this case handles both WebGL NPOT textures that don't support mips
            // and also cases such as render to a texture and using default filtering
            // when sampling and not having generated mips.
            if (!texture_state.has_mips)
            {
                internal_texture_min_filter = GL_LINEAR;
                force_min_linear = true;
            }
        }

        if (mContext->GetVersion() == dev::Context::Version::WebGL_1)
        {
            // https://www.khronos.org/webgl/wiki/WebGL_and_OpenGL_Differences#Non-Power_of_Two_Texture_Support
            const auto width = texture.texture_width;
            const auto height = texture.texture_height;
            if (!base::IsPowerOfTwo(width) || !base::IsPowerOfTwo(height))
            {
                if (internal_texture_x_wrap == GL_REPEAT || internal_texture_x_wrap == GL_MIRRORED_REPEAT)
                {
                    internal_texture_x_wrap = GL_CLAMP_TO_EDGE;
                    force_clamp_x = true;
                }
                if (internal_texture_y_wrap == GL_REPEAT || internal_texture_y_wrap == GL_MIRRORED_REPEAT)
                {
                    internal_texture_y_wrap = GL_CLAMP_TO_EDGE;
                    force_clamp_y = true;
                }
            }
        }

        if (warnings)
        {
            warnings->force_clamp_x = force_clamp_x;
            warnings->force_clamp_y = force_clamp_y;
            warnings->force_min_linear = force_min_linear;
        }

        if (texture_state.min_filter != internal_texture_min_filter ||
            texture_state.mag_filter != internal_texture_mag_filter ||
            texture_state.wrap_x != internal_texture_x_wrap ||
            texture_state.wrap_y != internal_texture_y_wrap)
        {
            GL_CALL(glActiveTexture(GL_TEXTURE0 + texture_unit));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, texture.handle));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, internal_texture_x_wrap));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, internal_texture_y_wrap));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, internal_texture_mag_filter));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, internal_texture_min_filter));
            GL_CALL(glUniform1i(texture_sampler, texture_unit));
            texture_state.mag_filter = internal_texture_mag_filter;
            texture_state.min_filter = internal_texture_min_filter;
            texture_state.wrap_x = internal_texture_x_wrap;
            texture_state.wrap_y = internal_texture_y_wrap;
        }
        else
        {
            // set the texture unit to the sampler
            GL_CALL(glActiveTexture(GL_TEXTURE0 + texture_unit));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, texture.handle));
            GL_CALL(glUniform1i(texture_sampler, texture_unit));
        }
        return true;
    }

    void DeleteTexture(const dev::TextureObject& texture) override
    {
        GL_CALL(glDeleteTextures(1, &texture.handle));

        mTextureState.erase(texture.handle);
    }

    dev::GraphicsBuffer AllocateBuffer(size_t bytes, dev::BufferUsage usage, dev::BufferType type) override
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

        if (type == dev::BufferType::UniformBuffer)
        {
            const auto reminder = bytes % mUniformBufferOffsetAlignment;
            const auto padding  = mUniformBufferOffsetAlignment - reminder;
            VERBOSE("Grow uniform buffer size from %1 to %2 bytes for offset alignment to %3.",
                    bytes, bytes+padding, mUniformBufferOffsetAlignment);
            bytes += padding;
        }

        if (usage == dev::BufferUsage::Static)
        {
            flag = GL_STATIC_DRAW;
            capacity = std::max(size_t(1024 * 1024), bytes);
        }
        else if (usage == dev::BufferUsage::Stream)
        {
            flag = GL_STREAM_DRAW;
            capacity = std::max(size_t(1024 * 1024), bytes);
        }
        else if (usage == dev::BufferUsage::Dynamic)
        {
            flag = GL_DYNAMIC_DRAW;
            capacity = bytes;
        }
        else BUG("Unsupported vertex buffer type.");

        auto& buffers = mBuffers[BufferIndex(type)];
        for (size_t buffer_index=0; buffer_index<buffers.size(); ++buffer_index)
        {
            auto& buffer = buffers[buffer_index];
            const auto available = buffer.capacity - buffer.offset;
            if ((available >= bytes) && (buffer.usage == usage))
            {
                const auto offset = buffer.offset;
                buffer.offset += bytes;
                buffer.refcount++;
                return dev::GraphicsBuffer {buffer.name, type, buffer_index, offset, bytes};
            }
        }

        BufferObject buffer;
        buffer.usage    = usage;
        buffer.offset   = bytes;
        buffer.capacity = capacity;
        buffer.refcount = 1;

        GL_CALL(glGenBuffers(1, &buffer.name));
        GL_CALL(glBindBuffer(GetBufferEnum(type), buffer.name));
        GL_CALL(glBufferData(GetBufferEnum(type), buffer.capacity, nullptr, flag));

        buffers.push_back(buffer);

        DEBUG("Allocated new buffer object. [bo=%1, size=%2, type=%3, type=%4]",
              buffer.name, buffer.capacity, usage, type);

        const auto buffer_index = buffers.size() - 1;
        const auto buffer_offset = 0;
        return dev::GraphicsBuffer {buffer.name, type, buffer_index, buffer_offset, bytes};
    }
    void FreeBuffer(const dev::GraphicsBuffer& buffer) override
    {
        auto& buffers = mBuffers[BufferIndex(buffer.type)];

        ASSERT(buffer.buffer_index < buffers.size());
        auto& buffer_object = buffers[buffer.buffer_index];
        ASSERT(buffer_object.refcount > 0);
        buffer_object.refcount--;

        if (buffer_object.usage == dev::BufferUsage::Static || buffer_object.usage == dev::BufferUsage::Dynamic)
        {
            if (buffer_object.refcount == 0)
                buffer_object.offset = 0;
        }
        if (buffer_object.usage == dev::BufferUsage::Static) {
            DEBUG("Free buffer data. [bo=%1, bytes=%2, offset=%3, type=%4, refs=%5, type=%6]",
                  buffer_object.name, buffer.buffer_bytes, buffer.buffer_offset, buffer_object.usage, buffer_object.refcount, buffer.type);
        }
    }
    void UploadBuffer(const dev::GraphicsBuffer& buffer, const void* data, size_t bytes) override
    {
        auto& buffers = mBuffers[BufferIndex(buffer.type)];
        ASSERT(buffer.buffer_index < buffers.size());

        auto& buffer_object = buffers[buffer.buffer_index];
        ASSERT(bytes <= buffer.buffer_bytes);
        ASSERT(buffer.buffer_offset + bytes <= buffer_object.capacity);

        GL_CALL(glBindBuffer(GetBufferEnum(buffer.type), buffer_object.name));
        GL_CALL(glBufferSubData(GetBufferEnum(buffer.type), buffer.buffer_offset, bytes, data));

        if (buffer_object.usage == dev::BufferUsage::Static)
        {
            const int percent_full = 100 * (double) buffer_object.offset / (double) buffer_object.capacity;
            DEBUG("Uploaded buffer data. [bo=%1, bytes=%2, offset=%3, full=%4%, usage=%5, type=%6]",
                  buffer_object.name, bytes, buffer.buffer_offset, percent_full, buffer_object.usage, buffer.type);
        }
    }

    void BindVertexBuffer(const dev::GraphicsBuffer& buffer,
                          const dev::GraphicsProgram& program,
                          const dev::VertexLayout& vertex_layout) const override
    {
        ASSERT(buffer.IsValid());
        ASSERT(program.IsValid());
        ASSERT(buffer.type == dev::BufferType::VertexBuffer);

        // the brain-damaged API goes like this... when using DrawArrays with a client side
        // data pointer the glVertexAttribPointer 'pointer' argument is actually a pointer
        // to the vertex data. But when using a VBO the pointer is not a pointer but an offset
        // into the contents of the VBO.
        const uint8_t* base_ptr = reinterpret_cast<const uint8_t*>(buffer.buffer_offset);

        GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, buffer.handle));
        for (const auto& attr : vertex_layout.attributes)
        {
            const GLint location = mGL.glGetAttribLocation(program.handle, attr.name.c_str());
            if (location == -1)
                continue;
            const auto size   = attr.num_vector_components;
            const auto stride = vertex_layout.vertex_struct_size;
            const auto attr_ptr = base_ptr + attr.offset;
            GL_CALL(glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, stride, attr_ptr));
            GL_CALL(glVertexAttribDivisor(location, attr.divisor));
            GL_CALL(glEnableVertexAttribArray(location));
        }

    }
    void BindIndexBuffer(const dev::GraphicsBuffer& buffer) const override
    {
        ASSERT(buffer.IsValid());
        ASSERT(buffer.type == dev::BufferType::IndexBuffer);
        GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.handle));
    }

    void SetProgramState(const dev::GraphicsProgram& program, const dev::ProgramState& state) const override
    {
        GL_CALL(glUseProgram(program.handle));
        // flush pending uniforms onto the GPU program object
        for (size_t i=0; i<state.uniforms.size(); ++i)
        {
            const auto* uniform_setting = state.uniforms[i];
            const auto& uniform_binding = GetUniformFromCache(program, uniform_setting->name);
            if (uniform_binding.location == -1)
                continue;

            const auto& value = uniform_setting->value;
            const auto location = uniform_binding.location;

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

    void SetPipelineState(const dev::GraphicsPipelineState& state) const override
    {
        GL_CALL(glLineWidth(state.line_width));
        GL_CALL(glViewport(state.viewport.GetX(), state.viewport.GetY(),
                           state.viewport.GetWidth(), state.viewport.GetHeight()));

        // scissor
        if (state.scissor.IsEmpty()) {
            GL_CALL(glDisable(GL_SCISSOR_TEST));
        } else {
            GL_CALL(glEnable(GL_SCISSOR_TEST));
            GL_CALL(glScissor(state.scissor.GetX(), state.scissor.GetY(),
                              state.scissor.GetWidth(), state.scissor.GetHeight()));
        }

        // polygon winding order
        if (state.winding_order == dev::PolygonWindingOrder::CounterClockWise) {
            GL_CALL(glFrontFace(GL_CCW));
        } else if (state.winding_order == dev::PolygonWindingOrder::ClockWise) {
            GL_CALL(glFrontFace(GL_CW));
        } else BUG("Bug on polygon winding order.");

        // blending
        if (state.blending == dev::BlendOp::None) {
            GL_CALL(glDisable(GL_BLEND));
        } else if (state.blending == dev::BlendOp::Transparent && state.premulalpha) {
            GL_CALL(glEnable(GL_BLEND));
            GL_CALL(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
        } else if (state.blending == dev::BlendOp::Transparent) {
            GL_CALL(glEnable(GL_BLEND));
            GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        } else if (state.blending == dev::BlendOp::Additive) {
            GL_CALL(glEnable(GL_BLEND));
            GL_CALL(glBlendFunc(GL_ONE, GL_ONE));
        } else BUG("Bug on blend state.");

        // culling
        if (state.culling == dev::Culling::None) {
            GL_CALL(glDisable(GL_CULL_FACE));
        } else if (state.culling == dev::Culling::Front) {
            GL_CALL(glEnable(GL_CULL_FACE));
            GL_CALL(glCullFace(GL_FRONT));
        } else if (state.culling == dev::Culling::Back) {
            GL_CALL(glEnable(GL_CULL_FACE));
            GL_CALL(glCullFace(GL_BACK));
        } else if (state.culling == dev::Culling::FrontAndBack) {
            GL_CALL(glEnable(GL_CULL_FACE));
            GL_CALL(glCullFace(GL_FRONT_AND_BACK));
        } else BUG("Bug on cull face state.");

        // depth testing
        if (state.depth_test == dev::DepthTest::Disabled) {
            GL_CALL(glDisable(GL_DEPTH_TEST));
        } else if (state.depth_test == dev::DepthTest::LessOrEQual) {
            GL_CALL(glEnable(GL_DEPTH_TEST));
            GL_CALL(glDepthFunc(GL_LEQUAL));
        } else BUG("Bug on depth test state.");

        // stencil test and stencil func
        if (state.stencil_func == dev::StencilFunc::Disabled) {
            GL_CALL(glDisable(GL_STENCIL_TEST));
        } else {
            const auto stencil_func  = GetEnum(state.stencil_func);
            const auto stencil_fail  = GetEnum(state.stencil_fail);
            const auto stencil_dpass = GetEnum(state.stencil_dpass);
            const auto stencil_dfail = GetEnum(state.stencil_dfail);
            GL_CALL(glEnable(GL_STENCIL_TEST));
            GL_CALL(glStencilFunc(stencil_func, state.stencil_ref, state.stencil_mask));
            GL_CALL(glStencilOp(stencil_fail, stencil_dfail, stencil_dpass));
        }

        // color mask
        if (state.bWriteColor) {
            GL_CALL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
        } else {
            GL_CALL(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));
        }
    }

    void BindProgramBuffer(const dev::GraphicsProgram& program, const dev::GraphicsBuffer& buffer,
                           const std::string& interface_block_name,
                           unsigned binding_index) override
    {
        ASSERT(buffer.IsValid());
        ASSERT(buffer.type == dev::BufferType::UniformBuffer);

        const auto& uniform_block = GetUniformBlockFromCache(program, interface_block_name);
        if (uniform_block.location == -1)
            return;

        GL_CALL(glUseProgram(program.handle));
        GL_CALL(glBindBuffer(GL_UNIFORM_BUFFER, buffer.handle));
        GL_CALL(glUniformBlockBinding(program.handle, uniform_block.location, binding_index));
        GL_CALL(glBindBufferRange(GL_UNIFORM_BUFFER, binding_index, buffer.handle, buffer.buffer_offset, buffer.buffer_bytes));
    }

    void Draw(dev::DrawType draw_primitive, dev::IndexType index_type,
              unsigned primitive_count, unsigned index_buffer_byte_offset, unsigned instance_count) const override
    {
        const auto primitive_draw_mode = GetDrawMode(draw_primitive);
        const auto primitive_index_type = GetIndexType(index_type);

        GL_CALL(glDrawElementsInstanced(primitive_draw_mode,
                                        primitive_count,
                                        primitive_index_type,
                                        (const void*)ptrdiff_t(index_buffer_byte_offset), instance_count));
    }
    void Draw(dev::DrawType draw_primitive, dev::IndexType index_type,
              unsigned primitive_count, unsigned index_buffer_byte_offset) const override
    {
        const auto primitive_draw_mode = GetDrawMode(draw_primitive);
        const auto primitive_index_type = GetIndexType(index_type);

        GL_CALL(glDrawElements(primitive_draw_mode,
                               primitive_count,
                               primitive_index_type, (const void*)ptrdiff_t(index_buffer_byte_offset)));
    }

    void Draw(dev::DrawType draw_primitive, unsigned vertex_start_index,
              unsigned vertex_draw_count, unsigned instance_count) const override
    {
        const auto primitive_draw_mode = GetDrawMode(draw_primitive);

        GL_CALL(glDrawArraysInstanced(primitive_draw_mode,
                                      vertex_start_index, vertex_draw_count, instance_count));
    }
    void Draw(dev::DrawType draw_primitive, unsigned vertex_start_index, unsigned vertex_draw_count) const override
    {
        const auto primitive_draw_mode = GetDrawMode(draw_primitive);

        GL_CALL(glDrawArrays(primitive_draw_mode,
                             vertex_start_index, vertex_draw_count));
    }

    void ClearColor(const base::Color4f& color, const dev::Framebuffer& fbo, ColorAttachment attachment) const override
    {
        BindFramebuffer(fbo);

        if (fbo.IsCustom())
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
    void ClearStencil(int value, const dev::Framebuffer& fbo) const override
    {
        BindFramebuffer(fbo);

        if (fbo.IsCustom())
        {
            GL_CALL(glClearBufferiv(GL_STENCIL, 0, &value));
        }
        else
        {
            GL_CALL(glClearStencil(value));
            GL_CALL(glClear(GL_STENCIL_BUFFER_BIT));
        }
    }
    void ClearDepth(float value, const dev::Framebuffer& fbo) const override
    {
        BindFramebuffer(fbo);

        if (fbo.IsCustom())
        {
            GL_CALL(glClearBufferfv(GL_DEPTH, 0, &value));
        }
        else
        {
            GL_CALL(glClearDepthf(value));
            GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));
        }
    }
    void ClearColorDepth(const base::Color4f& color, float depth, const dev::Framebuffer& fbo, ColorAttachment attachment) const override
    {
        BindFramebuffer(fbo);

        if (fbo.IsCustom())
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
    void ClearColorDepthStencil(const base::Color4f& color, float depth, int stencil, const dev::Framebuffer& fbo, ColorAttachment attachment) const override
    {
        BindFramebuffer(fbo);

        if (fbo.IsCustom())
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

    void ReadColor(unsigned width, unsigned height, const dev::Framebuffer& fbo, void* color_data) const override
    {
        BindFramebuffer(fbo);
        GL_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
        GL_CALL(glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, color_data));
    }
    void ReadColor(unsigned x, unsigned y, unsigned width, unsigned  height,
                   const dev::Framebuffer& fbo, void* color_data) const override
    {
        BindFramebuffer(fbo);
        GL_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
        GL_CALL(glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, color_data));
    }

    void GetResourceStats_tmp(dev::GraphicsDeviceResourceStats* stats) const override
    {
        std::memset(stats, 0, sizeof(*stats));
        for (const auto& buffer : mBuffers[0])
        {
            if (buffer.usage == dev::BufferUsage::Static)
            {
                stats->static_vbo_mem_alloc += buffer.capacity;
                stats->static_vbo_mem_use   += buffer.offset;
            }
            else if (buffer.usage == dev::BufferUsage::Dynamic)
            {
                stats->dynamic_vbo_mem_alloc += buffer.capacity;
                stats->dynamic_vbo_mem_use   += buffer.offset;
            }
            else if (buffer.usage == dev::BufferUsage::Stream)
            {
                stats->streaming_vbo_mem_alloc += buffer.capacity;
                stats->streaming_vbo_mem_use   += buffer.offset;
            }
        }
        for (const auto& buffer : mBuffers[1])
        {
            if (buffer.usage == dev::BufferUsage::Static)
            {
                stats->static_ibo_mem_alloc += buffer.capacity;
                stats->static_ibo_mem_use   += buffer.offset;
            }
            else if (buffer.usage == dev::BufferUsage::Dynamic)
            {
                stats->dynamic_ibo_mem_alloc += buffer.capacity;
                stats->dynamic_ibo_mem_use   += buffer.offset;
            }
            else if (buffer.usage == dev::BufferUsage::Stream)
            {
                stats->streaming_ibo_mem_alloc += buffer.capacity;
                stats->streaming_ibo_mem_use   += buffer.offset;
            }
        }
        for (const auto& buffer : mBuffers[2])
        {
            if (buffer.usage == dev::BufferUsage::Static)
            {
                stats->static_ubo_mem_alloc += buffer.capacity;
                stats->static_ubo_mem_use   += buffer.offset;
            }
            else if (buffer.usage == dev::BufferUsage::Dynamic)
            {
                stats->dynamic_ubo_mem_alloc += buffer.capacity;
                stats->dynamic_ubo_mem_use   += buffer.offset;
            }
            else if (buffer.usage == dev::BufferUsage::Stream)
            {
                stats->streaming_ubo_mem_alloc += buffer.capacity;
                stats->streaming_ubo_mem_use   += buffer.offset;
            }
        }
    }
    void GetDeviceCaps_tmp(dev::GraphicsDeviceCaps* caps) const override
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
            caps->instanced_rendering = false;
            caps->multiple_color_attachments = false;
        }
    }

    void BeginFrame_tmp() override
    {
        // trying to do so-called "buffer streaming" by "orphaning" the streaming
        // vertex buffers. this is achieved by re-specifying the contents of the
        // buffer by using nullptr data upload.
        // https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming

        // vertex buffers
        for (auto& buff : mBuffers[0])
        {
            if (buff.usage == dev::BufferUsage::Stream)
            {
                GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, buff.name));
                GL_CALL(glBufferData(GL_ARRAY_BUFFER, buff.capacity, nullptr, GL_STREAM_DRAW));
                buff.offset = 0;
            }
        }
        // index buffers
        for (auto& buff : mBuffers[1])
        {
            if (buff.usage == dev::BufferUsage::Stream)
            {
                GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buff.name));
                GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, buff.capacity, nullptr, GL_STREAM_DRAW));
                buff.offset = 0;
            }
        }
        // uniform buffers
        for (auto& buff : mBuffers[2])
        {
            if (buff.usage == dev::BufferUsage::Stream)
            {
                GL_CALL(glBindBuffer(GL_UNIFORM_BUFFER, buff.name));
                GL_CALL(glBufferData(GL_UNIFORM_BUFFER, buff.capacity, nullptr, GL_STREAM_DRAW));
                buff.offset = 0;
            }
        }
    }
    void EndFrame_tmp(bool display) override
    {
        if (display)
            mContext->Display();
    }


    void ClearColor(const gfx::Color4f& color, gfx::Framebuffer* fbo, ColorAttachment attachment) const override
    {
        const auto& framebuffer = SetupFBO(fbo);
        if (!framebuffer.IsValid())
            return;

        mDevice->ClearColor(color, framebuffer, attachment);
    }
    void ClearStencil(int value, gfx::Framebuffer* fbo) const override
    {
        const auto& framebuffer = SetupFBO(fbo);
        if (!framebuffer.IsValid())
            return;

        mDevice->ClearStencil(value, framebuffer);

    }
    void ClearDepth(float value, gfx::Framebuffer* fbo) const override
    {
        const auto& framebuffer = SetupFBO(fbo);
        if (!framebuffer.IsValid())
            return;

        mDevice->ClearDepth(value, framebuffer);

    }
    void ClearColorDepth(const gfx::Color4f& color, float depth, gfx::Framebuffer* fbo, ColorAttachment attachment) const override
    {
        const auto& framebuffer = SetupFBO(fbo);
        if (!framebuffer.IsValid())
            return;

        mDevice->ClearColorDepth(color, depth, framebuffer, attachment);

    }
    void ClearColorDepthStencil(const gfx::Color4f&  color, float depth, int stencil, gfx::Framebuffer* fbo, ColorAttachment attachment) const override
    {
        const auto& framebuffer = SetupFBO(fbo);
        if (!framebuffer.IsValid())
            return;

        mDevice->ClearColorDepthStencil(color, depth, stencil, framebuffer, attachment);
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
        auto shader = std::make_shared<ShaderImpl>(this);
        shader->SetName(args.name);
        shader->CompileSource(args.source, args.debug);
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
        auto program = std::make_shared<ProgImpl>(this);

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

    gfx::InstancedDrawPtr FindInstancedDraw(const std::string& id) override
    {
        auto it = mInstances.find(id);
        if (it == std::end(mInstances))
            return nullptr;
        return it->second;
    }

    gfx::InstancedDrawPtr CreateInstancedDraw(const std::string& id, gfx::InstancedDraw::CreateArgs args) override
    {
        auto instance = std::make_shared<InstancedDrawImpl>(this);
        instance->SetFrameStamp(mFrameNumber);
        instance->SetContentName(args.content_name);
        instance->SetContentHash(args.content_hash);
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
        auto texture = std::make_unique<TextureImpl>(this, name);
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
        auto fbo = std::make_unique<FramebufferImpl>(this, name);
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
        dev::Framebuffer framebuffer = SetupFBO(fbo);
        if (!framebuffer.IsValid())
            return;

        const auto* myprog = static_cast<const ProgImpl*>(&program);
        const auto* mygeom = static_cast<const GeomImpl*>(geometry.GetGeometry());
        const auto* myinst = static_cast<const InstancedDrawImpl*>(geometry.GetInstance());
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
        if (mygeom->IsEmpty())
            return;

        TRACE_CALL("SetPipelineState", mDevice->SetPipelineState(state));

        // set program texture bindings
        const auto num_textures = program_state.GetSamplerCount();

        TRACE_ENTER(BindTextures);
        for (size_t i=0; i<num_textures; ++i)
        {
            const auto& setting = program_state.GetSamplerSetting(i);

            auto* texture = (TextureImpl*)setting.texture;
            // if the program sampler/texture setting is using a discontinuous set of
            // texture units we might end up with "holes" in the program texture state
            // and then the texture object is actually a nullptr.
            if (texture == nullptr)
                continue;

            texture->SetFrameStamp(mFrameNumber);

            const auto& sampler = GetUniformFromCache(myprog->GetProgram(), setting.name);
            if (sampler.location == -1)
                continue;

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

            BindTexture2D(texture->GetTexture(), sampler.location,
                          texture_unit,
                          texture_wrap_x, texture_wrap_y,
                          texture_min_filter, texture_mag_filter,
                          &warnings);

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

                    // draw elements instanced
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

#if 0

        if (instance_buffer)
        {
            GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, instance_buffer->name));
            const auto& per_instance_vertex_layout = myinst->GetVertexLayout();
            for (const auto& attr : per_instance_vertex_layout.attributes)
            {
                const GLint location = mGL.glGetAttribLocation(myprog->GetHandle(), attr.name.c_str());
                if (location == -1)
                    continue;
                GL_CALL(glDisableVertexAttribArray(location));
            }
        }

        GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer->name));
        for (const auto& attr : vertex_layout.attributes)
        {
            const GLint location = mGL.glGetAttribLocation(myprog->GetHandle(), attr.name.c_str());
            if (location == -1)
                continue;
            GL_CALL(glDisableVertexAttribArray(location));
        }

        GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif
    }

    void CleanGarbage(size_t max_num_idle_frames, unsigned flags) override
    {
        if (flags &  GCFlags::FBOs)
        {
            const auto did_have_fbos = mFBOs.empty();

            for (auto it = mFBOs.begin(); it != mFBOs.end(); )
            {
                auto* impl = static_cast<FramebufferImpl*>(it->second.get());
                const auto last_used_frame_number = impl->GetFrameStamp();
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
                auto* impl = static_cast<ProgImpl*>(it->second.get());
                const auto last_used_frame_number = impl->GetFrameStamp();
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
                const auto* impl  = static_cast<TextureImpl*>(texture.get());
                const auto& group = impl->GetGroup();
                if (group.empty())
                    continue;
                const auto last_used  = impl->GetFrameStamp();
                group_last_use[group] = std::max(group_last_use[group], last_used);
            }

            const bool did_have_textures = !mTextures.empty();

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

            if (did_have_textures && mTextures.empty())
                INFO("All GPU texture objects were deleted.");
        }

        if (flags & GCFlags::Geometries)
        {
            const auto did_have_geometries = !mGeoms.empty();
            const auto did_have_instances = !mInstances.empty();

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
                auto* impl = static_cast<InstancedDrawImpl*>(it->second.get());
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

    void BeginFrame() override
    {
        for (auto& pair : mPrograms)
        {
            auto* impl = static_cast<ProgImpl*>(pair.second.get());
            impl->BeginFrame();
        }

        mDevice->BeginFrame_tmp();


    }
    void EndFrame(bool display) override
    {
        mDevice->EndFrame_tmp(display);

        mFrameNumber++;

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
        dev::Framebuffer framebuffer = SetupFBO(fbo);
        if (!framebuffer.IsValid())
            return bmp;

        bmp.Resize(width, height);
        mDevice->ReadColor(width, height, framebuffer, bmp.GetDataPtr());

        // by default the scan row order is reversed to what we expect.
        bmp.FlipHorizontally();
        return bmp;
    }

    gfx::Bitmap<gfx::Pixel_RGBA> ReadColorBuffer(unsigned x, unsigned y, unsigned width, unsigned height, gfx::Framebuffer* fbo) const override
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
    void GetResourceStats(ResourceStats* stats) const override
    {
        mDevice->GetResourceStats_tmp(stats);
    }
    void GetDeviceCaps(DeviceCaps* caps) const override
    {
        mDevice->GetDeviceCaps_tmp(caps);
    }

    gfx::Device* AsGraphicsDevice() override
    { return this; }
    std::shared_ptr<gfx::Device> GetSharedGraphicsDevice() override
    { return shared_from_this(); }

    dev::Framebuffer SetupFBO(gfx::Framebuffer* fbo) const
    {
        dev::Framebuffer invalid;
        if (fbo)
        {
            auto* impl = static_cast<FramebufferImpl*>(fbo);
            if (impl->IsReady())
            {
                if (!impl->Complete())
                    return invalid;
            }
            else
            {
                if (!impl->Create())
                    return invalid;
                if (!impl->Complete())
                    return invalid;
            }
            impl->SetFrameStamp(mFrameNumber);
            return impl->GetFramebuffer();
        }
        return mDevice->GetDefaultFramebuffer();
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

private:
    class TextureImpl : public gfx::Texture
    {
    public:
        TextureImpl(dev::GraphicsDevice* device, std::string id)
           : mDevice(device)
           , mGpuId(std::move(id))
        {
            mFlags.set(Flags::Transient, false);
            mFlags.set(Flags::GarbageCollect, true);
        }
        ~TextureImpl() override
        {
            if (mTexture.IsValid())
            {
                mDevice->DeleteTexture(mTexture);
                if (!IsTransient())
                    DEBUG("Deleted texture object. [name='%1']", mName);
            }
        }

        void Upload(const void* bytes, unsigned xres, unsigned yres, Format format, bool mips) override
        {
            if (mTexture.IsValid())
            {
                mDevice->DeleteTexture(mTexture);
            }

            if (bytes)
            {
                mTexture = mDevice->UploadTexture2D(bytes, xres, yres, format);
                if (!IsTransient())
                {
                    DEBUG("Uploaded new texture object. [name='%1', size=%2x%3]", mName, xres, yres);
                }

                if (mips)
                {
                    const auto ret = mDevice->GenerateMipmaps(mTexture);
                    if (ret == dev::GraphicsDevice::MipStatus::UnsupportedSize)
                        WARN("Unsupported texture size for mipmap generation. [name='%1]", mName);
                    else if (ret == dev::GraphicsDevice::MipStatus::UnsupportedFormat)
                        WARN("Unsupported texture format for mipmap generation. [name='%1']", mName);
                    else if (ret == dev::GraphicsDevice::MipStatus::Error)
                        WARN("Failed to generate mips on texture. [name='%1']", mName);
                    else if (ret == dev::GraphicsDevice::MipStatus::Success)
                    {
                        if (!IsTransient())
                            DEBUG("Successfully generated texture mips. [name='%1']", mName);
                    }
                    else BUG("Bug on mipmap status.");

                    mHasMips = ret == dev::GraphicsDevice::MipStatus::Success;
                }
            }
            else
            {
                mTexture = mDevice->AllocateTexture2D(xres, yres, format);
                if (!IsTransient())
                {
                    DEBUG("Allocated new texture object. [name='%1', size=%2x%3]", mName, xres, yres);
                }
            }
            mWidth  = xres;
            mHeight = yres;
            mFormat = format;
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
            ASSERT(mTexture.IsValid());
            ASSERT(mTexture.texture_width && mTexture.texture_height);

            if (mHasMips)
                return true;

            const auto ret = mDevice->GenerateMipmaps(mTexture);
            if (ret == dev::GraphicsDevice::MipStatus::UnsupportedSize)
                WARN("Unsupported texture size for mipmap generation. [name='%1]", mName);
            else if (ret == dev::GraphicsDevice::MipStatus::UnsupportedFormat)
                WARN("Unsupported texture format for mipmap generation. [name='%1']", mName);
            else if (ret == dev::GraphicsDevice::MipStatus::Error)
                WARN("Failed to generate mips on texture. [name='%1']", mName);
            else if (ret == dev::GraphicsDevice::MipStatus::Success)
            {
                if (!IsTransient())
                    DEBUG("Successfully generated texture mips. [name='%1']", mName);
            }
            else BUG("Bug on mipmap status.");

            mHasMips = ret == dev::GraphicsDevice::MipStatus::Success;
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

        dev::TextureObject GetTexture() const
        {
            return mTexture;
        }
        GLuint GetHandle() const
        {
            return mTexture.handle;
        }

        void SetFrameStamp(size_t frame_number) const
        { mFrameNumber = frame_number; }
        std::size_t GetFrameStamp() const
        { return mFrameNumber; }

    private:
        dev::GraphicsDevice* mDevice = nullptr;
        dev::TextureObject mTexture;

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

    class InstancedDrawImpl : public gfx::InstancedDraw
    {
    public:
        explicit InstancedDrawImpl(dev::GraphicsDevice* device) noexcept
          : mDevice(device)
        {}
       ~InstancedDrawImpl() override
        {
            if (mBuffer.IsValid())
            {
                mDevice->FreeBuffer(mBuffer);
            }
            if (mUsage == Usage::Static)
                DEBUG("Deleted instanced draw object. [name='%1']", mContentName);
        }
        std::size_t GetContentHash() const override
        {
            return mContentHash;
        }
        std::string GetContentName() const override
        {
            return mContentName;
        }
        void SetContentHash(size_t hash) override
        {
            mContentHash = hash;
        }
        void SetContentName(std::string name) override
        {
            mContentName = std::move(name);
        }

        void Upload() const
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

        inline void SetBuffer(gfx::InstancedDrawBuffer&& buffer) noexcept
        { mPendingUpload = std::move(buffer); }
        inline void SetUsage(Usage usage) noexcept
        { mUsage = usage; }
        inline void SetFrameStamp(size_t frame_number) const noexcept
        { mFrameNumber = frame_number; }
        inline size_t GetFrameStamp() const noexcept
        { return mFrameNumber; }
        inline size_t GetVertexBufferByteOffset() const noexcept
        { return mBuffer.buffer_offset; }
        inline size_t GetVertexBufferIndex() const noexcept
        { return mBuffer.buffer_index; }
        inline size_t GetInstanceCount() const noexcept
        { return mBuffer.buffer_bytes / mLayout.vertex_struct_size; }
        inline const gfx::InstanceDataLayout& GetVertexLayout() const noexcept
        { return mLayout; }
        inline const dev::GraphicsBuffer& GetVertexBuffer() const noexcept
        { return mBuffer; }

    private:
        dev::GraphicsDevice* mDevice = nullptr;
        std::size_t mContentHash = 0;
        std::string mContentName;
        gfx::BufferUsage mUsage = gfx::BufferUsage::Static;
        mutable std::size_t mFrameNumber = 0;
        mutable std::optional<gfx::InstancedDrawBuffer> mPendingUpload;
        mutable gfx::InstanceDataLayout mLayout;
        mutable dev::GraphicsBuffer  mBuffer;
    };

    class GeomImpl : public gfx::Geometry
    {
    public:
        explicit GeomImpl(dev::GraphicsDevice* device) noexcept
          : mDevice(device)
        {}
       ~GeomImpl() override
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

            mVertexBuffer = mDevice->AllocateBuffer(vertex_bytes, mUsage, dev::BufferType::VertexBuffer);
            mDevice->UploadBuffer(mVertexBuffer, vertex_ptr, vertex_bytes);

            mVertexLayout     = std::move(upload.GetLayout());
            mDrawCommands     = std::move(upload.GetDrawCommands());

            if (mUsage == gfx::BufferUsage::Static)
            {
                DEBUG("Uploaded geometry vertices. [name='%1', bytes='%2', usage='%3']", mName, vertex_bytes, mUsage);
            }
            if (index_bytes == 0)
                return;

            mIndexBuffer = mDevice->AllocateBuffer(index_bytes, mUsage, dev::BufferType::IndexBuffer);
            mDevice->UploadBuffer(mIndexBuffer, index_ptr, index_bytes);

            mIndexBufferType = upload.GetIndexType();

            if (mUsage == gfx::BufferUsage::Static)
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

        inline bool IsEmpty() const noexcept
        { return mVertexBuffer.buffer_bytes == 0; }

        inline size_t GetVertexBufferByteOffset() const noexcept
        { return mVertexBuffer.buffer_offset; }
        inline size_t GetVertexBufferByteSize() const noexcept
        { return mVertexBuffer.buffer_bytes; }

        inline size_t GetIndexBufferByteOffset() const noexcept
        { return mIndexBuffer.buffer_offset; }
        inline size_t GetIndexBufferByteSize() const noexcept
        { return mIndexBuffer.buffer_bytes; }

        inline IndexType GetIndexBufferType() const noexcept
        { return mIndexBufferType; }

        inline bool UsesIndexBuffer() const noexcept
        { return mIndexBuffer.IsValid(); }

        inline const gfx::VertexLayout& GetVertexLayout() const noexcept
        { return mVertexLayout; }

        inline const dev::GraphicsBuffer& GetVertexBuffer() const noexcept
        { return mVertexBuffer; }

        inline const dev::GraphicsBuffer& GetIndexBuffer() const noexcept
        { return mIndexBuffer; }

        inline size_t GetVertexCount() const noexcept
        { return mVertexBuffer.buffer_bytes / mVertexLayout.vertex_struct_size; }

        inline size_t GetIndexCount() const noexcept
        { return mIndexBuffer.buffer_bytes / dev::GetIndexByteSize(mIndexBufferType); }

        inline size_t GetIndexByteSize() const noexcept
        { return dev::GetIndexByteSize(mIndexBufferType); }
    private:
        dev::GraphicsDevice* mDevice = nullptr;

        std::size_t mHash = 0;
        std::string mName;
        gfx::BufferUsage mUsage = gfx::BufferUsage::Static;

        mutable std::size_t mFrameNumber = 0;
        mutable std::optional<gfx::GeometryBuffer> mPendingUpload;
        mutable std::vector<DrawCommand> mDrawCommands;
        mutable dev::GraphicsBuffer mVertexBuffer;
        mutable dev::GraphicsBuffer mIndexBuffer;
        mutable IndexType mIndexBufferType = IndexType::Index16;
        mutable gfx::VertexLayout mVertexLayout;
    };

    class ProgImpl : public gfx::Program
    {
    public:
        explicit ProgImpl(dev::GraphicsDevice* device) noexcept
          : mDevice(device)
        {}

       ~ProgImpl() override
        {
            if (mProgram.IsValid())
            {
                mDevice->DeleteProgram(mProgram);
                DEBUG("Deleted program object. [name='%1']", mName);
            }

            for (auto pair : mUniformBlockBuffers)
            {
                const auto& buffer = pair.second;
                if (buffer.IsValid())
                {
                    mDevice->FreeBuffer(buffer);
                }
            }
        }
        bool Build(const std::vector<gfx::ShaderPtr>& shaders)
        {
            std::vector<dev::GraphicsShader> shader_handles;
            for (const auto& shader : shaders)
            {
                const auto* ptr = static_cast<const ShaderImpl*>(shader.get());
                shader_handles.push_back(ptr->GetShader());
            }

            std::string build_info;
            auto program = mDevice->BuildProgram(shader_handles, &build_info);
            if (!program.IsValid())
            {
                ERROR("Program build error. [error='%1']", build_info);
                return false;
            }

            DEBUG("Program was built successfully. [name='%1', info='%2']", mName, build_info);
            for (auto& shader : shaders)
            {
                const auto* ptr = static_cast<const ShaderImpl*>(shader.get());
                ptr->ClearSource();
            }
            mProgram = program;
            return true;
        }

        void ApplyUniformState(const gfx::ProgramState& state) const
        {
            dev::ProgramState ps;
            for (size_t i=0; i<state.GetUniformCount(); ++i)
            {
                const auto& uniform = state.GetUniformSetting(i);
                ps.uniforms.push_back(&uniform);
            }
            mDevice->SetProgramState(mProgram, ps);

            for (size_t i=0; i<state.GetUniformBlockCount(); ++i)
            {
                const auto& block = state.GetUniformBlock(i);

                auto gpu_buffer = mUniformBlockBuffers[block.GetName()];

                const auto& cpu_block_buffer = block.GetBuffer();
                const auto block_buffer_size = cpu_block_buffer.size();

                if (gpu_buffer.buffer_bytes < block_buffer_size)
                {
                    gpu_buffer = mDevice->AllocateBuffer(block_buffer_size, dev::BufferUsage::Stream, dev::BufferType::UniformBuffer);
                }
                mDevice->UploadBuffer(gpu_buffer, cpu_block_buffer.data(), block_buffer_size);
                mDevice->BindProgramBuffer(mProgram, gpu_buffer, block.GetName(), i); // todo: binding index ??
                mUniformBlockBuffers[block.GetName()] = gpu_buffer;
            }
        }

        bool IsValid() const override
        {
            return mProgram.IsValid();
        }

        std::string GetName() const override
        {
            return mName;
        }

        std::string GetId() const override
        {
            return mGpuId;
        }

        void SetName(const std::string& name)
        {
            mName = name;
        }
        void SetId(const std::string& id)
        {
            mGpuId = id;
        }


        void BeginFrame()
        {
        }

        dev::GraphicsProgram GetProgram() const
        {
            return mProgram;
        }

        GLuint GetHandle() const
        { return mProgram.handle; }
        void SetFrameStamp(size_t frame_number) const
        { mFrameNumber = frame_number; }
        size_t GetFrameStamp() const
        { return mFrameNumber; }

    private:
        dev::GraphicsDevice* mDevice = nullptr;
        dev::GraphicsProgram mProgram;
        std::string mName;
        std::string mGpuId;

        mutable std::unordered_map<std::string, dev::GraphicsBuffer> mUniformBlockBuffers;
        mutable std::size_t mFrameNumber = 0;
    };

    class ShaderImpl : public gfx::Shader
    {
    public:
        explicit ShaderImpl(dev::GraphicsDevice* device) noexcept
          : mDevice(device)
        {}

       ~ShaderImpl() override
        {
            if (mShader.IsValid())
            {
                mDevice->DeleteShader(mShader);
                DEBUG("Deleted graphics shader. [name='%1']", mName);
            }
        }
        void CompileSource(const std::string& source, bool debug)
        {
            dev::ShaderType type = dev::ShaderType::Invalid;
            std::stringstream ss(source);
            std::string line;
            while (std::getline(ss, line) && type == dev::ShaderType::Invalid)
            {
                if (line.find("gl_Position") != std::string::npos)
                    type = dev::ShaderType::VertexShader;
                else if (line.find("gl_FragColor") != std::string::npos ||
                         line.find("fragOutColor") != std::string::npos)
                    type = dev::ShaderType::FragmentShader;
            }
            if (type == dev::ShaderType::Invalid)
            {
                ERROR("Failed to identify shader type. [name='%1']", mName);
                DEBUG("In order for the automatic shader type identification to work your shader must have one of the following:");
                DEBUG("GLSL 100 (ES2) gl_Position => vertex shader");
                DEBUG("GLSL 300 (ES3) gl_Position => vertex shader");
                DEBUG("GLSL 100 (ES2) gl_FragColor => fragment shader");
                DEBUG("GLSL 300 (ES3) fragOutColor => fragment shader");
                return;
            }

            auto shader = mDevice->CompileShader(source, type, &mCompileInfo);
            if (!shader.IsValid())
            {
                ERROR("Shader object compile error. [name='%1', info='%2']", mName, mCompileInfo);
                DumpSource(source);
                return;
            }
            else
            {
                if (debug)
                    DumpSource(source);

                DEBUG("Shader was built successfully. [name='%1', info='%2']", mName, mCompileInfo);
            }
            mShader = shader;
            mSource = source;
        }
        void DumpSource() const
        {
            DumpSource(mSource);
        }
        void DumpSource(const std::string& source) const
        {
            DEBUG("Shader source: [name='%1']", mName);
            std::stringstream ss(source);
            std::string line;
            std::size_t counter = 1;
            while (std::getline(ss, line))
            {
                DEBUG("L:%1 %2", counter, line);
                ++counter;
            }
        }
        void ClearSource() const
        {
            mSource.clear();
        }

        bool IsValid() const override
        { return mShader.IsValid(); }
        std::string GetName() const override
        { return mName; }
        std::string GetError() const override
        { return mCompileInfo; }

        inline void SetName(std::string name) noexcept
        { mName = std::move(name); }

        inline dev::GraphicsShader GetShader() const noexcept
        { return mShader; }

    private:
        dev::GraphicsDevice* mDevice = nullptr;
        dev::GraphicsShader mShader;
        std::string mName;
        std::string mCompileInfo;
        mutable std::string mSource;
    };
    struct Extensions;

    class FramebufferImpl : public gfx::Framebuffer
    {
    public:
        FramebufferImpl(dev::GraphicsDevice* device, std::string name)
          : mName(std::move(name))
          , mDevice(device)
        {}
       ~FramebufferImpl() override
        {
            mTextures.clear();

            if (mFramebuffer.IsValid())
            {
                mDevice->DeleteFramebuffer(mFramebuffer);
                DEBUG("Deleted frame buffer object. [name='%1']", mName);
            }
        }
        void SetConfig(const Config& conf) override
        {
            ASSERT(conf.color_target_count >= 1);

            // we don't allow the config to be changed after it has been created.
            // todo: delete the SetConfig API and take the FBO config in the
            // device level API to create the FBO.
            if (mFramebuffer.IsValid())
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
                if (mFramebuffer.IsValid() && (mConfig.format != Format::ColorRGBA8))
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
            if (const auto samples = mFramebuffer.samples)
            {
                auto* resolve_target = GetColorBufferTexture(index);
                mDevice->ResolveFramebuffer(mFramebuffer, resolve_target->GetTexture(), index);
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
            if (const auto samples = mFramebuffer.samples)
            {
                CreateColorBufferTextures();
                const auto* resolve_target = GetColorBufferTexture(0);
                const unsigned width  = resolve_target->GetWidth();
                const unsigned height = resolve_target->GetHeight();

                for (unsigned i=0; i<mConfig.color_target_count; ++i)
                {
                    mDevice->AllocateRenderTarget(mFramebuffer, i, width, height);
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
                    mDevice->BindRenderTargetTexture2D(mFramebuffer, color_target->GetTexture(), i);
                }
            }
            std::vector<unsigned> color_attachments;
            for (unsigned i=0; i<mConfig.color_target_count; ++i)
                color_attachments.push_back(i);

            if (!mDevice->CompleteFramebuffer(mFramebuffer, color_attachments))
            {
                ERROR("Unsupported framebuffer configuration. [name='%1']", mName);
                return false;
            }

            return true;
        }

        bool Create()
        {
            ASSERT(!mFramebuffer.IsValid());

            CreateColorBufferTextures();

            auto* texture = GetColorBufferTexture(0);
            const auto width  = texture->GetWidth();
            const auto height = texture->GetHeight();

            dev::FramebufferConfig config;
            config.width  = width;
            config.height = height;
            config.msaa   = IsMultisampled();
            config.format = mConfig.format;
            mFramebuffer = mDevice->CreateFramebuffer(config);
            DEBUG("Created new frame buffer object. [name='%1', width=%2, height=%3, format=%4, samples=%5]",
                  mName, width, height, mConfig.format, config.msaa);

            // commit the size
            mConfig.width  = width;
            mConfig.height = height;
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
            return mFramebuffer.handle;
        }
        inline bool IsReady() const noexcept
        {
            return mFramebuffer.IsValid();
        }

        bool IsMultisampled() const noexcept
        {
            return mConfig.msaa == MSAA::Enabled;
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
                    mTextures[i] = std::make_unique<TextureImpl>(mDevice, name);
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

        dev::Framebuffer GetFramebuffer() const
        { return mFramebuffer; }
    private:
        const std::string mName;

        dev::GraphicsDevice* mDevice = nullptr;
        dev::Framebuffer mFramebuffer;

        // Texture target that we allocate when the use hasn't
        // provided a client texture. In case of a single sampled
        // FBO this is used directly as the color attachment
        // in case of multiple sampled this will be used as the
        // resolve target.
        std::vector<std::unique_ptr<TextureImpl>> mTextures;

        // Client provided texture(s) that will ultimately contain
        // the rendered result.
        std::vector<TextureImpl*> mClientTextures;
        std::size_t mFrameNumber = 0;

        Config mConfig;
    };
private:
    std::map<std::string, std::shared_ptr<gfx::InstancedDraw>> mInstances;
    std::map<std::string, std::shared_ptr<gfx::Geometry>> mGeoms;
    std::map<std::string, std::shared_ptr<gfx::Shader>> mShaders;
    std::map<std::string, std::shared_ptr<gfx::Program>> mPrograms;
    std::map<std::string, std::unique_ptr<gfx::Texture>> mTextures;
    std::map<std::string, std::unique_ptr<gfx::Framebuffer>> mFBOs;
    std::size_t mFrameNumber = 0;

    dev::GraphicsDevice* mDevice = nullptr;

    MinFilter mDefaultMinTextureFilter = MinFilter::Nearest;
    MagFilter mDefaultMagTextureFilter = MagFilter::Nearest;

    // cached texture unit state. used to omit texture unit
    // state changes when not needed.
    struct TextureUnit {
        // the texture currently bound to the unit.
        TextureImpl* texture = nullptr;
    };
    // texture units and their current settings.
    mutable std::vector<TextureUnit> mTextureUnits;
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
