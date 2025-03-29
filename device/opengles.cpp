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
#include <unordered_map>
#include <type_traits>

#include "warnpush.h"
#  include <glm/gtc/type_ptr.hpp>
#include "warnpop.h"

#include "base/assert.h"
#include "base/logging.h"
#include "base/hash.h"
#include "base/utility.h"
#include "base/trace.h"
#include "base/bitflag.h"

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

static_assert(sizeof(GLint) == sizeof(int) && sizeof(GLuint) == sizeof(unsigned) && sizeof(GLfloat) == sizeof(float),
              "Basic OpenGL type sanity check");

static_assert(sizeof(int) == 4 && sizeof(unsigned) == 4 && sizeof(float) == 4 &&
              sizeof(glm::vec2) == 8 && sizeof(glm::vec3) == 12 && sizeof(glm::vec4) == 16 &&
              sizeof(glm::ivec2) == 8 && sizeof(glm::ivec3) == 12 && sizeof(glm::ivec4) == 16 &&
              sizeof(glm::uvec2) == 8 && sizeof(glm::uvec3) == 12 && sizeof(glm::uvec4) == 16,
              "Basic types sanity check");

// KHR_debug
typedef void (GL_APIENTRY *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id,GLenum severity, GLsizei length,
                                        const GLchar* message, const void* userParam);
typedef void (GL_APIENTRY *PFNGLDEBUGMESSAGECALLBACKPROC)(GLDEBUGPROC proc, const void* user);

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
    PFNGLUNIFORM1UIPROC              glUniform1ui;
    PFNGLUNIFORM2UIPROC              glUniform2ui;
    PFNGLUNIFORM3UIPROC              glUniform3ui;
    PFNGLUNIFORM4UIPROC              glUniform4ui;
    PFNGLUNIFORM1IPROC               glUniform1i;
    PFNGLUNIFORM2IPROC               glUniform2i;
    PFNGLUNIFORM3IPROC               glUniform3i;
    PFNGLUNIFORM4IPROC               glUniform4i;
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
                              , public dev::Device
                              , public dev::GraphicsDevice {

private:
    struct CachedUniform {
        GLuint location = 0;
        uint32_t hash = 0;
    };
    struct BufferObject {
        dev::BufferUsage usage = dev::BufferUsage::Static;
        GLuint name = 0;
        size_t capacity = 0;
        size_t offset = 0;
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
            GLuint width = 0;
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

    unsigned mTempResolveFbo = 0;
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
        uniform.hash = 0;
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
        RESOLVE(glUniform1ui);
        RESOLVE(glUniform2ui);
        RESOLVE(glUniform3ui);
        RESOLVE(glUniform4ui);
        RESOLVE(glUniform1i);
        RESOLVE(glUniform2i);
        RESOLVE(glUniform3i);
        RESOLVE(glUniform4i);
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
        GLint red_bits = 0;
        GLint green_bits = 0;
        GLint blue_bits = 0;
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
        } else
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

        const char* extensions = (const char*) mGL.glGetString(GL_EXTENSIONS);
        std::stringstream ss(extensions);
        std::string extension;
        while (std::getline(ss, extension, ' '))
        {
            if (extension == "GL_EXT_sRGB")
                mExtensions.EXT_sRGB = true;
            else if (extension == "GL_OES_packed_depth_stencil")
                mExtensions.OES_packed_depth_stencil = true;
            else if (extension == "GL_EXT_draw_buffers")
                mExtensions.GL_EXT_draw_buffers = true;

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
            } else
            {
                INFO("Maximum color attachments: %1", max_color_attachments);
            }
        } else if (version == dev::Context::Version::OpenGL_ES2 ||
                   version == dev::Context::Version::WebGL_1)
        {
        }
        // we use this for uploading textures and generating mips
        // so we don't need to play stupid games with the "actual"
        // texture units.
        mTempTextureUnitIndex = max_texture_units - 1;
        mTextureUnitCount = max_texture_units - 1; // see the temp
        mUniformBufferOffsetAlignment = (unsigned) uniform_buffer_offset_alignment;

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
        DEBUG("Destroy dev::GraphicsDevice");

        if (mTempResolveFbo)
        {
            GL_CALL(glDeleteFramebuffers(1, &mTempResolveFbo));
        }
        for (auto& buffer: mBuffers[0])
        {
            GL_CALL(glDeleteBuffers(1, &buffer.name));
        }
        for (auto& buffer: mBuffers[1])
        {
            GL_CALL(glDeleteBuffers(1, &buffer.name));
        }
        for (auto& buffer: mBuffers[2])
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

    static GLenum GetEnum(dev::BufferType type)
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

    static GLenum GetEnum(dev::TextureWrapping wrapping)
    {
        if (wrapping == dev::TextureWrapping::Clamp)
            return GL_CLAMP_TO_EDGE;
        else if (wrapping == dev::TextureWrapping::Repeat)
            return GL_REPEAT;
        else if (wrapping == dev::TextureWrapping::Mirror)
            return GL_MIRRORED_REPEAT;

        BUG("Bug on GL texture wrapping mode.");
        return GL_NONE;
    }

    static GLenum GetEnum(dev::TextureMinFilter filter)
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

        BUG("Bug on min texture filter");
        return GL_NONE;
    }

    static GLenum GetEnum(dev::TextureMagFilter filter)
    {
        if (filter == dev::TextureMagFilter::Nearest)
            return GL_NEAREST;
        else if (filter == dev::TextureMagFilter::Linear)
            return GL_LINEAR;

        BUG("Bug on mag texture filter.");
        return GL_NONE;
    }

    static GLenum GetEnum(dev::DrawType draw)
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

        BUG("Bug on draw primitive.");
        return GL_NONE;
    }

    static GLenum GetEnum(dev::IndexType type)
    {
        if (type == dev::IndexType::Index16)
            return GL_UNSIGNED_SHORT;
        else if (type == dev::IndexType::Index32)
            return GL_UNSIGNED_INT;

        BUG("Bug on index type.");
        return GL_NONE;
    }

    static GLenum GetEnum(dev::StencilFunc func)
    {
        using Func = dev::StencilFunc;

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

        BUG("Bug on stencil func");
        return GL_NONE;
    }

    static GLenum GetEnum(dev::StencilOp op)
    {
        using Op = dev::StencilOp;

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

        BUG("Bug on stencil op");
        return GL_NONE;
    }

    static GLenum GetEnum(dev::ShaderType shader)
    {
        if (shader == dev::ShaderType::VertexShader)
            return GL_VERTEX_SHADER;
        else if (shader == dev::ShaderType::FragmentShader)
            return GL_FRAGMENT_SHADER;

        BUG("Bug on shader type.");
        return GL_NONE;
    }

    static GLenum GetEnum(dev::BufferUsage usage)
    {
        if (usage == dev::BufferUsage::Static)
            return GL_STATIC_DRAW;
        else if (usage == dev::BufferUsage::Stream)
            return GL_STREAM_DRAW;
        else if (usage == dev::BufferUsage::Dynamic)
            return GL_DYNAMIC_DRAW;

        BUG("Bug on buffer usage.");
        return GL_NONE;
    }

    struct TextureFormat {
        GLenum sizeFormat = GL_NONE;
        GLenum baseFormat = GL_NONE;
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
                } else BUG("Unknown OpenGL ES version.");
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
                } else BUG("Unknown OpenGL ES version.");
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
            if (version == dev::Context::Version::OpenGL_ES2 ||
                version == dev::Context::Version::WebGL_1)
                samples = 0;
            else if (version == dev::Context::Version::OpenGL_ES3 ||
                     version == dev::Context::Version::WebGL_2)
                samples = 4;
            else
                BUG("Missing OpenGL ES version.");
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
                GL_CALL(glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT16, config.width,
                                                         config.height));
            else
                GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, config.width, config.height));
            GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                              framebuffer_state.depth_buffer));
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
                    fbo.width = 0;
                    fbo.height = 0;
                    fbo.format = dev::FramebufferFormat::Invalid;
                    return fbo;
                }
                GL_CALL(glGenRenderbuffers(1, &framebuffer_state.depth_buffer));
                GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, framebuffer_state.depth_buffer));
                GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, config.width, config.height));
                GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                                  framebuffer_state.depth_buffer));
                GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                                  framebuffer_state.depth_buffer));
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
                GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, WEBGL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                                  framebuffer_state.depth_buffer));
            }
            else if (version == dev::Context::Version::OpenGL_ES3 || version == dev::Context::Version::WebGL_2)
            {
                GL_CALL(glGenRenderbuffers(1, &framebuffer_state.depth_buffer));
                GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, framebuffer_state.depth_buffer));
                if (samples)
                    GL_CALL(glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8,
                                                             config.width, config.height));
                else
                    GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, config.width, config.height));
                GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                                  framebuffer_state.depth_buffer));
                GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                                  framebuffer_state.depth_buffer));
            }
        }
        dev::Framebuffer fbo;
        fbo.handle = handle;
        fbo.height = config.height;
        fbo.width = config.width;
        fbo.format = config.format;
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
            framebuffer_state.multisample_color_buffers.resize(color_attachment + 1);

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
            buff.width = width;
            buff.height = height;
            DEBUG("Allocated multi-sampled render buffer storage. [size=%1x%2]", width, height);
        }

        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.handle));
        GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + color_attachment, GL_RENDERBUFFER,
                                          buff.handle));
    }

    void BindRenderTargetTexture2D(const dev::Framebuffer& framebuffer, const dev::TextureObject& texture,
                                   unsigned color_attachment) override
    {
        ASSERT(framebuffer.IsValid());
        ASSERT(framebuffer.IsCustom());
        ASSERT(texture.IsValid());
        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.handle));
        GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + color_attachment, GL_TEXTURE_2D,
                                       texture.handle, 0));
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

        if (!mTempResolveFbo)
            GL_CALL(glGenFramebuffers(1, &mTempResolveFbo));

        const auto draw_buffers = GLenum(GL_COLOR_ATTACHMENT0);
        const auto resolve_width = resolve_target.texture_width;
        const auto resolve_height = resolve_target.texture_height;

        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, mTempResolveFbo));
        GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolve_target.handle, 0));
        GL_CALL(glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampled_framebuffer.handle));
        GL_CALL(glReadBuffer(GL_COLOR_ATTACHMENT0 + color_attachment));
        GL_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mTempResolveFbo));
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

        for (auto buffer: framebuffer_state.multisample_color_buffers)
        {
            GL_CALL(glDeleteRenderbuffers(1, &buffer.handle));
        }

        GL_CALL(glDeleteFramebuffers(1, &framebuffer.handle));

        mFramebufferState.erase(framebuffer.handle);
    }

    dev::GraphicsShader CompileShader(const std::string& source, dev::ShaderType type, std::string* compile_info) override
    {
        GLint status = 0;
        GLuint shader = mGL.glCreateShader(GetEnum(type));

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
            return {0, dev::ShaderType::Invalid};
        }
        return {static_cast<unsigned>(shader), type};
    }

    dev::GraphicsProgram BuildProgram(const std::vector<dev::GraphicsShader>& shaders, std::string* build_info) override
    {
        GLuint program = mGL.glCreateProgram();
        for (const auto& shader: shaders)
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
            return {0};
        }
        return {program};
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
        texture_state.wrap_x = GL_NONE;
        texture_state.wrap_y = GL_NONE;
        texture_state.has_mips = false;

        dev::TextureObject ret;
        ret.handle = handle;
        ret.type = dev::TextureType::Texture2D;
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
        texture_state.wrap_x = GL_NONE;
        texture_state.wrap_y = GL_NONE;
        texture_state.has_mips = false;

        dev::TextureObject ret;
        ret.handle = handle;
        ret.type = dev::TextureType::Texture2D;
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

    bool BindTexture2D(const dev::TextureObject& texture, const dev::GraphicsProgram& program, const std::string& sampler_name, unsigned texture_unit,
                       dev::TextureWrapping texture_x_wrap, dev::TextureWrapping texture_y_wrap,
                       dev::TextureMinFilter texture_min_filter, dev::TextureMagFilter texture_mag_filter,
                       BindWarnings* warnings) const override
    {
        ASSERT(texture.IsValid());
        ASSERT(texture.texture_width && texture.texture_height);

        if (texture_unit >= mTextureUnitCount)
        {
            ERROR("Texture unit index exceeds the maximum available texture units. [unit=%1, available=¤2]",
                  texture_unit, mTextureUnitCount);
            return false;
        }

        const auto& sampler = GetUniformFromCache(program, sampler_name);
        if (sampler.location == -1)
            return true;

        bool force_clamp_x = false;
        bool force_clamp_y = false;
        bool force_min_linear = false;

        auto internal_texture_min_filter = GetEnum(texture_min_filter);
        auto internal_texture_mag_filter = GetEnum(texture_mag_filter);
        auto internal_texture_x_wrap = GetEnum(texture_x_wrap);
        auto internal_texture_y_wrap = GetEnum(texture_y_wrap);

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
            GL_CALL(glUniform1i(sampler.location, texture_unit));
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
            GL_CALL(glUniform1i(sampler.location, texture_unit));
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

        if (type == dev::BufferType::UniformBuffer)
        {
            const auto reminder = bytes % mUniformBufferOffsetAlignment;
            const auto padding = mUniformBufferOffsetAlignment - reminder;
            VERBOSE("Grow uniform buffer size from %1 to %2 bytes for offset alignment to %3.",
                    bytes, bytes + padding, mUniformBufferOffsetAlignment);
            bytes += padding;
        }

        size_t capacity = 0;
        if (usage == dev::BufferUsage::Static)
            capacity = std::max(size_t(1024 * 1024), bytes);
        else if (usage == dev::BufferUsage::Stream)
            capacity = std::max(size_t(1024 * 1024), bytes);
        else if (usage == dev::BufferUsage::Dynamic)
            capacity = bytes;
        else BUG("Unsupported vertex buffer type.");

        auto& buffers = mBuffers[BufferIndex(type)];
        for (size_t buffer_index = 0; buffer_index < buffers.size(); ++buffer_index)
        {
            auto& buffer = buffers[buffer_index];
            const auto available = buffer.capacity - buffer.offset;
            if ((available >= bytes) && (buffer.usage == usage))
            {
                const auto offset = buffer.offset;
                buffer.offset += bytes;
                buffer.refcount++;
                return dev::GraphicsBuffer{buffer.name, type, buffer_index, offset, bytes};
            }
        }

        BufferObject buffer;
        buffer.usage = usage;
        buffer.offset = bytes;
        buffer.capacity = capacity;
        buffer.refcount = 1;

        GL_CALL(glGenBuffers(1, &buffer.name));
        GL_CALL(glBindBuffer(GetEnum(type), buffer.name));
        GL_CALL(glBufferData(GetEnum(type), buffer.capacity, nullptr, GetEnum(usage)));

        buffers.push_back(buffer);

        DEBUG("Allocated new buffer object. [bo=%1, size=%2, type=%3, type=%4]",
              buffer.name, buffer.capacity, usage, type);

        const auto buffer_index = buffers.size() - 1;
        const auto buffer_offset = 0;
        return dev::GraphicsBuffer{buffer.name, type, buffer_index, buffer_offset, bytes};
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
        if (buffer_object.usage == dev::BufferUsage::Static)
        {
            DEBUG("Free buffer data. [bo=%1, bytes=%2, offset=%3, type=%4, refs=%5, type=%6]",
                  buffer_object.name, buffer.buffer_bytes, buffer.buffer_offset, buffer_object.usage,
                  buffer_object.refcount, buffer.type);
        }
    }

    void UploadBuffer(const dev::GraphicsBuffer& buffer, const void* data, size_t bytes) override
    {
        auto& buffers = mBuffers[BufferIndex(buffer.type)];
        ASSERT(buffer.buffer_index < buffers.size());

        auto& buffer_object = buffers[buffer.buffer_index];
        ASSERT(bytes <= buffer.buffer_bytes);
        ASSERT(buffer.buffer_offset + bytes <= buffer_object.capacity);

        GL_CALL(glBindBuffer(GetEnum(buffer.type), buffer_object.name));
        GL_CALL(glBufferSubData(GetEnum(buffer.type), buffer.buffer_offset, bytes, data));

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
        const auto* base_ptr = reinterpret_cast<const uint8_t*>(buffer.buffer_offset);

        GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, buffer.handle));
        for (const auto& attr: vertex_layout.attributes)
        {
            const GLint location = mGL.glGetAttribLocation(program.handle, attr.name.c_str());
            if (location == -1)
                continue;
            const auto size = attr.num_vector_components;
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
        for (size_t i = 0; i < state.uniforms.size(); ++i)
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
            else if (const auto* ptr = std::get_if<unsigned>(&value))
                GL_CALL(glUniform1ui(location, *ptr));
            else if (const auto* ptr = std::get_if<float>(&value))
                GL_CALL(glUniform1f(location, *ptr));
            else if (const auto* ptr = std::get_if<glm::uvec2>(&value))
                GL_CALL(glUniform2ui(location, ptr->x, ptr->y));
            else if (const auto* ptr = std::get_if<glm::uvec3>(&value))
                GL_CALL(glUniform3ui(location, ptr->x, ptr->y, ptr->z));
            else if (const auto* ptr = std::get_if<glm::uvec4>(&value))
                GL_CALL(glUniform4ui(location, ptr->x, ptr->y, ptr->z, ptr->w));
            else if (const auto* ptr = std::get_if<glm::ivec2>(&value))
                GL_CALL(glUniform2i(location, ptr->x, ptr->y));
            else if (const auto* ptr = std::get_if<glm::ivec3>(&value))
                GL_CALL(glUniform3i(location, ptr->x, ptr->y, ptr->z));
            else if (const auto* ptr = std::get_if<glm::ivec4>(&value))
                GL_CALL(glUniform4i(location, ptr->x, ptr->y, ptr->z, ptr->w));
            else if (const auto* ptr = std::get_if<glm::vec2>(&value))
                GL_CALL(glUniform2f(location, ptr->x, ptr->y));
            else if (const auto* ptr = std::get_if<glm::vec3>(&value))
                GL_CALL(glUniform3f(location, ptr->x, ptr->y, ptr->z));
            else if (const auto* ptr = std::get_if<glm::vec4>(&value))
                GL_CALL(glUniform4f(location, ptr->x, ptr->y, ptr->z, ptr->w));
            else if (const auto* ptr = std::get_if<base::Color4f>(&value))
            {
                // Assume sRGB encoded color values now. so this is a simple
                // place to convert to linear and will catch all uses without
                // breaking the higher level APIs
                // the cost of the sRGB conversion should be mitigated due to
                // the hash check to compare to the previous value.
                const auto& linear = sRGB_Decode(*ptr);
                GL_CALL(glUniform4f(location, linear.Red(), linear.Green(), linear.Blue(), linear.Alpha()));
            } else if (const auto* ptr = std::get_if<glm::mat2>(&value))
                GL_CALL(glUniformMatrix2fv(location, 1, GL_FALSE /* transpose */, glm::value_ptr(*ptr)));
            else if (const auto* ptr = std::get_if<glm::mat3>(&value))
                GL_CALL(glUniformMatrix3fv(location, 1, GL_FALSE /* transpose */, glm::value_ptr(*ptr)));
            else if (const auto* ptr = std::get_if<glm::mat4>(&value))
                GL_CALL(glUniformMatrix4fv(location, 1, GL_FALSE /*transpose*/, glm::value_ptr(*ptr)));
            else
                BUG("Unhandled shader program uniform type.");
        }
    }

    void SetPipelineState(const dev::GraphicsPipelineState& state) const override
    {
        GL_CALL(glLineWidth(state.line_width));

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
        GL_CALL(glBindBufferRange(GL_UNIFORM_BUFFER, binding_index, buffer.handle, buffer.buffer_offset,
                                  buffer.buffer_bytes));
    }

    void SetViewportState(const dev::ViewportState& state) const override
    {
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
    }

    void SetColorDepthStencilState(const dev::ColorDepthStencilState& state) const override
    {
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
            const auto stencil_func = GetEnum(state.stencil_func);
            const auto stencil_fail = GetEnum(state.stencil_fail);
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

    void Draw(dev::DrawType draw_primitive, dev::IndexType index_type,
              unsigned primitive_count, unsigned index_buffer_byte_offset, unsigned instance_count) const override
    {
        const auto primitive_draw_mode = GetEnum(draw_primitive);
        const auto primitive_index_type = GetEnum(index_type);

        GL_CALL(glDrawElementsInstanced(primitive_draw_mode,
                                        primitive_count,
                                        primitive_index_type,
                                        (const void*) ptrdiff_t(index_buffer_byte_offset), instance_count));
    }

    void Draw(dev::DrawType draw_primitive, dev::IndexType index_type,
              unsigned primitive_count, unsigned index_buffer_byte_offset) const override
    {
        const auto primitive_draw_mode = GetEnum(draw_primitive);
        const auto primitive_index_type = GetEnum(index_type);

        GL_CALL(glDrawElements(primitive_draw_mode,
                               primitive_count,
                               primitive_index_type, (const void*) ptrdiff_t(index_buffer_byte_offset)));
    }

    void Draw(dev::DrawType draw_primitive, unsigned vertex_start_index,
              unsigned vertex_draw_count, unsigned instance_count) const override
    {
        const auto primitive_draw_mode = GetEnum(draw_primitive);

        GL_CALL(glDrawArraysInstanced(primitive_draw_mode,
                                      vertex_start_index, vertex_draw_count, instance_count));
    }

    void Draw(dev::DrawType draw_primitive, unsigned vertex_start_index, unsigned vertex_draw_count) const override
    {
        const auto primitive_draw_mode = GetEnum(draw_primitive);

        GL_CALL(glDrawArrays(primitive_draw_mode,
                             vertex_start_index, vertex_draw_count));
    }

    void ClearColor(const base::Color4f& color, const dev::Framebuffer& fbo, dev::ColorAttachment attachment) const override
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

    void ClearColorDepth(const base::Color4f& color, float depth, const dev::Framebuffer& fbo,
                         dev::ColorAttachment attachment) const override
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

    void ClearColorDepthStencil(const base::Color4f& color, float depth, int stencil, const dev::Framebuffer& fbo,
                                dev::ColorAttachment attachment) const override
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

    void ReadColor(unsigned x, unsigned y, unsigned width, unsigned height,
                   const dev::Framebuffer& fbo, void* color_data) const override
    {
        BindFramebuffer(fbo);
        GL_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
        GL_CALL(glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, color_data));
    }

    void GetResourceStats(dev::GraphicsDeviceResourceStats* stats) const override
    {
        std::memset(stats, 0, sizeof(*stats));
        for (const auto& buffer: mBuffers[0])
        {
            if (buffer.usage == dev::BufferUsage::Static)
            {
                stats->static_vbo_mem_alloc += buffer.capacity;
                stats->static_vbo_mem_use += buffer.offset;
            }
            else if (buffer.usage == dev::BufferUsage::Dynamic)
            {
                stats->dynamic_vbo_mem_alloc += buffer.capacity;
                stats->dynamic_vbo_mem_use += buffer.offset;
            }
            else if (buffer.usage == dev::BufferUsage::Stream)
            {
                stats->streaming_vbo_mem_alloc += buffer.capacity;
                stats->streaming_vbo_mem_use += buffer.offset;
            }
        }
        for (const auto& buffer: mBuffers[1])
        {
            if (buffer.usage == dev::BufferUsage::Static)
            {
                stats->static_ibo_mem_alloc += buffer.capacity;
                stats->static_ibo_mem_use += buffer.offset;
            }
            else if (buffer.usage == dev::BufferUsage::Dynamic)
            {
                stats->dynamic_ibo_mem_alloc += buffer.capacity;
                stats->dynamic_ibo_mem_use += buffer.offset;
            }
            else if (buffer.usage == dev::BufferUsage::Stream)
            {
                stats->streaming_ibo_mem_alloc += buffer.capacity;
                stats->streaming_ibo_mem_use += buffer.offset;
            }
        }
        for (const auto& buffer: mBuffers[2])
        {
            if (buffer.usage == dev::BufferUsage::Static)
            {
                stats->static_ubo_mem_alloc += buffer.capacity;
                stats->static_ubo_mem_use += buffer.offset;
            }
            else if (buffer.usage == dev::BufferUsage::Dynamic)
            {
                stats->dynamic_ubo_mem_alloc += buffer.capacity;
                stats->dynamic_ubo_mem_use += buffer.offset;
            }
            else if (buffer.usage == dev::BufferUsage::Stream)
            {
                stats->streaming_ubo_mem_alloc += buffer.capacity;
                stats->streaming_ubo_mem_use += buffer.offset;
            }
        }
    }

    void GetDeviceCaps(dev::GraphicsDeviceCaps* caps) const override
    {
        std::memset(caps, 0, sizeof(*caps));
        GLint num_texture_units = 0;
        GLint max_fbo_size = 0;
        GL_CALL(glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &num_texture_units));
        GL_CALL(glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_fbo_size));

        caps->num_texture_units = num_texture_units;
        caps->max_fbo_height = max_fbo_size;
        caps->max_fbo_width = max_fbo_size;

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

    void BeginFrame() override
    {
        // trying to do so-called "buffer streaming" by "orphaning" the streaming
        // vertex buffers. this is achieved by re-specifying the contents of the
        // buffer by using nullptr data upload.
        // https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming

        // vertex buffers
        for (auto& buff: mBuffers[0])
        {
            if (buff.usage == dev::BufferUsage::Stream)
            {
                GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, buff.name));
                GL_CALL(glBufferData(GL_ARRAY_BUFFER, buff.capacity, nullptr, GL_STREAM_DRAW));
                buff.offset = 0;
            }
        }
        // index buffers
        for (auto& buff: mBuffers[1])
        {
            if (buff.usage == dev::BufferUsage::Stream)
            {
                GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buff.name));
                GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, buff.capacity, nullptr, GL_STREAM_DRAW));
                buff.offset = 0;
            }
        }
        // uniform buffers
        for (auto& buff: mBuffers[2])
        {
            if (buff.usage == dev::BufferUsage::Stream)
            {
                GL_CALL(glBindBuffer(GL_UNIFORM_BUFFER, buff.name));
                GL_CALL(glBufferData(GL_UNIFORM_BUFFER, buff.capacity, nullptr, GL_STREAM_DRAW));
                buff.offset = 0;
            }
        }
    }

    void EndFrame(bool display) override
    {
        if (display)
            mContext->Display();
    }

    GraphicsDevice* AsGraphicsDevice() override
    {
        return this;
    }

    std::shared_ptr<GraphicsDevice> GetSharedGraphicsDevice() override
    {
        return shared_from_this();
    }
private:
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
