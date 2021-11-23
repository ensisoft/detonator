
// For clang in Clion
#if !defined(__EMSCRIPTEN__)
#  define __EMSCRIPTEN__
#endif

#include "config.h"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <iostream>
#include <memory>
#include <chrono>
#include <unordered_map>

#include "base/logging.h"
#include "wdk/events.h"
#include "engine/main/interface.h"

// mkdir build
// cd build
// emcmake cmake ..
// make
// python -m http.server

namespace {

// returns number of seconds elapsed since the last call
// of this function.
double ElapsedSeconds()
{
    using clock = std::chrono::steady_clock;
    static auto start = clock::now();
    auto now  = clock::now();
    auto gone = now - start;
    start = now;
    return std::chrono::duration_cast<std::chrono::microseconds>(gone).count() /
           (1000.0 * 1000.0);
}
// returns number of seconds since the application started
// running.
double CurrentRuntime()
{
    using clock = std::chrono::steady_clock;
    static const auto start = clock::now();
    const auto now  = clock::now();
    const auto gone = now - start;
    return std::chrono::duration_cast<std::chrono::microseconds>(gone).count() /
           (1000.0 * 1000.0);
}

class WebGLContext : public gfx::Device::Context
{
public:
    WebGLContext()
    {
        EmscriptenWebGLContextAttributes attrs;
        emscripten_webgl_init_context_attributes(&attrs);
        attrs.alpha                        = false;
        attrs.depth                        = true;
        attrs.stencil                      = true;
        attrs.antialias                    = true;
        attrs.majorVersion                 = 1; // WebGL 1.0 is based on OpenGL ES 2.0
        attrs.minorVersion                 = 0;
        attrs.preserveDrawingBuffer        = false;
        attrs.failIfMajorPerformanceCaveat = true;
        /*
        attrs.premultipliedAlpha           = false;
        attrs.preserveDrawingBuffer        = false;
        attrs.failIfMajorPerformanceCaveat = false;
        attrs.enableExtensionsByDefault    = false;
        attrs.explicitSwapControl          = true; // does swap/display when OnAnimationFrame returns
        attrs.renderViaOffscreenBackBuffer = false;
        attrs.powerPreference              = EM_WEBGL_POWER_PREFERENCE_DEFAULT;
        attrs.proxyContextToMainThread     = EMSCRIPTEN_WEBGL_CONTEXT_PROXY_DISALLOW;
         */

        // todo: deal with failure?
        mContext = emscripten_webgl_create_context("canvas", &attrs);
        DEBUG("WebGL context = %1", mContext);
        emscripten_webgl_make_context_current(mContext);
    }
    ~WebGLContext()
    {
    }
    virtual void Display() override
    {
        // automatic on return from frame render
    }
    virtual void* Resolve(const char* name) override
    {
        void* ret = emscripten_webgl_get_proc_address(name);
        DEBUG("Resolving GL entry point. [name=%1, ret=%2]", name, ret);
        return ret;
    }
    virtual void MakeCurrent() override
    {
        emscripten_webgl_make_context_current(mContext);
    }
private:
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE mContext = 0;
};

class Application
{
public:
    Application()
    {
        base::SetThreadLog(&mLogger);
        base::EnableDebugLog(true);

        mContext.reset(new WebGLContext);
        mEngine.reset(Gamestudio_CreateEngine());

        int canvas_width  = 0;
        int canvas_height = 0;
        emscripten_set_canvas_element_size("canvas", 1024, 768);
        emscripten_get_canvas_element_size("canvas", &canvas_width, &canvas_height);

        // IMPORTANT: make sure that the order in which the engine is setup
        // is the same between all the launcher applications.
        // that is engine/main/main.cpp and editor/gui/playwindow.cpp

        // todo:
        engine::Engine::Environment env;
        mEngine->SetEnvironment(env);

        // todo:
        engine::Engine::InitParams init;
        init.editing_mode     = false;
        init.application_name = "test"; // TODO
        init.surface_width    = canvas_width;
        init.surface_height   = canvas_height;
        init.context          = mContext.get();
        init.game_script      = ""; // TODO:
        mEngine->Init(init);

        // todo:
        engine::Engine::EngineConfig config;
        mEngine->SetEngineConfig(config);

        mEngine->Load();
        mEngine->Start();

        mListener = mEngine->GetWindowListener();
    }
   ~Application()
    {
        base::SetThreadLog(nullptr);
    }
    EM_BOOL OnWindowResize(int emsc_type, const EmscriptenUiEvent* emsc_event)
    {
        if (emsc_type == EMSCRIPTEN_EVENT_RESIZE)
        {
            int canvas_with   = 0;
            int canvas_height = 0;
            emscripten_get_canvas_element_size("canvas", &canvas_with, &canvas_height);
            wdk::WindowEventResize resize;
            resize.width  = canvas_with;
            resize.height = canvas_height;
            mListener->OnResize(resize);
            mEngine->OnRenderingSurfaceResized(canvas_with, canvas_height);
        }
        return EM_TRUE;
    }

    EM_BOOL OnAnimationFrame()
    {
        const auto time_step = ElapsedSeconds();

        mEngine->Update(time_step);
        mEngine->Draw();

        ++mFrames;
        ++mCounter;
        mSeconds += time_step;
        if (mSeconds > 1.0)
        {
            engine::Engine::HostStats stats;
            stats.current_fps         = mCounter / mSeconds;
            stats.num_frames_rendered = mFrames;
            stats.total_wall_time     = CurrentRuntime();
            mEngine->SetHostStats(stats);
            mCounter = 0;
            mSeconds = 0;
        }
        return EM_TRUE;
    }
    EM_BOOL OnMouseEvent(int emsc_type, const EmscriptenMouseEvent* emsc_event)
    {
        wdk::bitflag<wdk::Keymod> mods;
        if (emsc_event->shiftKey)
            mods.set(wdk::Keymod::Shift, true);
        if (emsc_event->altKey)
            mods.set(wdk::Keymod::Alt, true);
        if (emsc_event->ctrlKey)
            mods.set(wdk::Keymod::Control, true);

        wdk::MouseButton btn = wdk::MouseButton::None;
        if (emsc_event->button == 0)
            btn = wdk::MouseButton::Left;
        else if (emsc_event->button == 1)
            btn = wdk::MouseButton::Wheel;
        else if (emsc_event->button == 2)
            btn = wdk::MouseButton::Right;
        else WARN("Unmapped mouse button. [value=%1]", emsc_event->button);

        if (emsc_type == EMSCRIPTEN_EVENT_MOUSEDOWN)
        {
            wdk::WindowEventMousePress event;
            event.window_x  = emsc_event->targetX;
            event.window_y  = emsc_event->targetY;
            event.global_y  = emsc_event->screenX;
            event.global_y  = emsc_event->screenY;
            event.modifiers = mods;
            event.btn       = btn;
            mListener->OnMousePress(event);
        }
        else if (emsc_type == EMSCRIPTEN_EVENT_MOUSEUP)
        {
            wdk::WindowEventMouseRelease event;
            event.window_x  = emsc_event->targetX;
            event.window_y  = emsc_event->targetY;
            event.global_y  = emsc_event->screenX;
            event.global_y  = emsc_event->screenY;
            event.modifiers = mods;
            event.btn       = btn;
            mListener->OnMouseRelease(event);
        }
        else if (emsc_type == EMSCRIPTEN_EVENT_MOUSEMOVE)
        {
            wdk::WindowEventMouseMove event;
            event.window_x  = emsc_event->targetX;
            event.window_y  = emsc_event->targetY;
            event.global_y  = emsc_event->screenX;
            event.global_y  = emsc_event->screenY;
            event.modifiers = mods;
            event.btn       = btn;
            mListener->OnMouseMove(event);
        } else WARN("Unhandled mouse event.[emsc_type=%1]", emsc_type);
        return EM_TRUE;
    }
    EM_BOOL OnKeyboardEvent(int emsc_type, const EmscriptenKeyboardEvent* emsc_event)
    {
        wdk::bitflag<wdk::Keymod> mods;
        if (emsc_event->ctrlKey)
            mods.set(wdk::Keymod::Control, true);
        if (emsc_event->shiftKey)
            mods.set(wdk::Keymod::Shift, true);
        if (emsc_event->altKey)
            mods.set(wdk::Keymod::Alt, true);

        static std::unordered_map<std::string, wdk::Keysym> keymap;
        if (keymap.empty())
        {
            // unmapped. (no key in wdk)
            // "Pause", "PrintScreen", "AltRight", "MetaLeft", "MetaRight"
            // NumPad keys (multiple)
            keymap["Backspace"]    = wdk::Keysym::Backspace;
            keymap["Tab"]          = wdk::Keysym::Tab;
            keymap["Enter"]        = wdk::Keysym::Enter;
            keymap["ShiftLeft"]    = wdk::Keysym::ShiftL;
            keymap["ShiftRight"]   = wdk::Keysym::ShiftR;
            keymap["ControlLeft"]  = wdk::Keysym::ControlL;
            keymap["ControlRight"] = wdk::Keysym::ControlR;
            keymap["AltLeft"]      = wdk::Keysym::AltL;
            keymap["CapsLock"]     = wdk::Keysym::CapsLock;
            keymap["Escape"]       = wdk::Keysym::Escape;
            keymap["Space"]        = wdk::Keysym::Space;
            keymap["PageUp"]       = wdk::Keysym::PageUp;
            keymap["PageDown"]     = wdk::Keysym::PageDown;
            keymap["End"]          = wdk::Keysym::End;
            keymap["Home"]         = wdk::Keysym::Home;
            keymap["ArrowLeft"]    = wdk::Keysym::ArrowLeft;
            keymap["ArrowUp"]      = wdk::Keysym::ArrowUp;
            keymap["ArrowRight"]   = wdk::Keysym::ArrowRight;
            keymap["ArrowDown"]    = wdk::Keysym::ArrowDown;
            keymap["Insert"]       = wdk::Keysym::Insert;
            keymap["Delete"]       = wdk::Keysym::Del;
            keymap["Digit0"]       = wdk::Keysym::Key0;
            keymap["Digit1"]       = wdk::Keysym::Key1;
            keymap["Digit2"]       = wdk::Keysym::Key2;
            keymap["Digit3"]       = wdk::Keysym::Key3;
            keymap["Digit4"]       = wdk::Keysym::Key4;
            keymap["Digit5"]       = wdk::Keysym::Key5;
            keymap["Digit6"]       = wdk::Keysym::Key6;
            keymap["Digit7"]       = wdk::Keysym::Key7;
            keymap["Digit8"]       = wdk::Keysym::Key8;
            keymap["Digit9"]       = wdk::Keysym::Key9;
            keymap["KeyA"]         = wdk::Keysym::KeyA;
            keymap["KeyB"]         = wdk::Keysym::KeyB;
            keymap["KeyC"]         = wdk::Keysym::KeyC;
            keymap["KeyD"]         = wdk::Keysym::KeyD;
            keymap["KeyE"]         = wdk::Keysym::KeyE;
            keymap["KeyF"]         = wdk::Keysym::KeyF;
            keymap["KeyG"]         = wdk::Keysym::KeyG;
            keymap["KeyH"]         = wdk::Keysym::KeyH;
            keymap["KeyI"]         = wdk::Keysym::KeyI;
            keymap["KeyJ"]         = wdk::Keysym::KeyJ;
            keymap["KeyK"]         = wdk::Keysym::KeyK;
            keymap["KeyL"]         = wdk::Keysym::KeyL;
            keymap["KeyM"]         = wdk::Keysym::KeyM;
            keymap["KeyN"]         = wdk::Keysym::KeyN;
            keymap["KeyO"]         = wdk::Keysym::KeyO;
            keymap["KeyP"]         = wdk::Keysym::KeyP;
            keymap["KeyQ"]         = wdk::Keysym::KeyQ;
            keymap["KeyR"]         = wdk::Keysym::KeyR;
            keymap["KeyS"]         = wdk::Keysym::KeyS;
            keymap["KeyT"]         = wdk::Keysym::KeyT;
            keymap["KeyU"]         = wdk::Keysym::KeyU;
            keymap["KeyV"]         = wdk::Keysym::KeyV;
            keymap["KeyW"]         = wdk::Keysym::KeyW;
            keymap["KeyX"]         = wdk::Keysym::KeyX;
            keymap["KeyY"]         = wdk::Keysym::KeyY;
            keymap["KeyZ"]         = wdk::Keysym::KeyZ;
            keymap["F1"]           = wdk::Keysym::F1;
            keymap["F2"]           = wdk::Keysym::F2;
            keymap["F3"]           = wdk::Keysym::F3;
            keymap["F4"]           = wdk::Keysym::F4;
            keymap["F5"]           = wdk::Keysym::F5;
            keymap["F6"]           = wdk::Keysym::F6;
            keymap["F7"]           = wdk::Keysym::F7;
            keymap["F8"]           = wdk::Keysym::F8;
            keymap["F9"]           = wdk::Keysym::F9;
            keymap["F10"]          = wdk::Keysym::F10;
            keymap["F11"]          = wdk::Keysym::F11;
            keymap["F12"]          = wdk::Keysym::F12;
            keymap["Minus"]        = wdk::Keysym::Minus;
            keymap["Plus"]         = wdk::Keysym::Plus;
        }
        wdk::Keysym symbol = wdk::Keysym::None;
        auto it = keymap.find(emsc_event->code);
        if (it != keymap.end())
            symbol = it->second;

        if (emsc_type == EMSCRIPTEN_EVENT_KEYPRESS)
        {
            wdk::WindowEventChar character;
            std::memcpy(character.utf8, emsc_event->key, sizeof(character.utf8));
            mListener->OnChar(character);
        }
        else if (emsc_type == EMSCRIPTEN_EVENT_KEYDOWN)
        {
            wdk::WindowEventKeyDown key;
            key.modifiers = mods;
            key.symbol    = symbol;
            mListener->OnKeyDown(key);
        }
        else if (emsc_type == EMSCRIPTEN_EVENT_KEYUP)
        {
            wdk::WindowEventKeyUp key;
            key.modifiers = mods;
            key.symbol    = symbol;
            mListener->OnKeyUp(key);
        }
        return EM_TRUE;
    }
private:
    base::EmscriptenLogger mLogger;
    std::unique_ptr<WebGLContext> mContext;
    std::unique_ptr<engine::Engine> mEngine;
    wdk::WindowListener* mListener = nullptr;
    double mSeconds = 0;
    unsigned mCounter = 0;
    unsigned mFrames  = 0;

};

EM_BOOL OnWindowSizeChanged(int emsc_type, const EmscriptenUiEvent* emsc_event, void* user_data)
{
    return static_cast<Application*>(user_data)->OnWindowResize(emsc_type, emsc_event);
}
EM_BOOL OnMouseEvent(int emsc_type, const EmscriptenMouseEvent* emsc_event, void* user_data)
{
    return static_cast<Application*>(user_data)->OnMouseEvent(emsc_type, emsc_event);
}

EM_BOOL OnWheelEvent(int emsc_type, const EmscriptenWheelEvent* emsc_event, void* user_data)
{
    return EM_TRUE;
}

EM_BOOL OnKeyboardEvent(int emsc_type, const EmscriptenKeyboardEvent* emsc_event, void* user_data)
{
    return static_cast<Application*>(user_data)->OnKeyboardEvent(emsc_type, emsc_event);
}

EM_BOOL OnTouchEvent(int emsc_type, const EmscriptenTouchEvent* emsc_event, void* user_data)
{
    return EM_TRUE;
}

EM_BOOL OnFocusEvent(int emsc_type, const EmscriptenFocusEvent* emsc_event, void* user_data)
{
    return EM_TRUE;
}
EM_BOOL OnBlurEvent(int emsc_type, const EmscriptenFocusEvent* emsc_event, void* user_data)
{
    return EM_TRUE;
}
EM_BOOL OnContextEvent(int emsc_type, const void* reserved, void* user_data)
{
    return EM_TRUE;
}
EM_BOOL OnAnimationFrame(double time, void* user_data)
{
    // the time value is the time from performance.now()
    // which has 1ms resolution on ff.
    // https://developer.mozilla.org/en-US/docs/Web/API/Performance/now
    return static_cast<Application*>(user_data)->OnAnimationFrame();
}
} // namespace

int main()
{
    // todo: how to exit cleanly? is there even such thing?
    Application* app = new Application;

    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, app, EM_FALSE /* capture*/, OnWindowSizeChanged);

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, app, true, OnKeyboardEvent);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, app, true, OnKeyboardEvent);
    emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, app, true, OnKeyboardEvent);

    emscripten_set_mousedown_callback("canvas", app, true, OnMouseEvent);
    emscripten_set_mouseup_callback("canvas", app, true, OnMouseEvent);
    emscripten_set_mousemove_callback("canvas", app, true, OnMouseEvent);
    emscripten_set_mouseenter_callback("canvas", app, true, OnMouseEvent);
    emscripten_set_mouseleave_callback("canvas", app, true, OnMouseEvent);
    // note that this thread will return after calling this.
    // and after exiting the main function will go call OnAnimationFrame
    // when the browser sees fit.
    emscripten_request_animation_frame_loop(OnAnimationFrame, app);
    return 0;
}

