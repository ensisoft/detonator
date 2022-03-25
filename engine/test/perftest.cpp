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

// engine subsystem performance tests

#include "config.h"

#include <iostream>
#include <string>
#include <any>

#include "base/cmdline.h"
#include "base/logging.h"
#include "base/test_help.h"
#include "base/trace.h"
#include "audio/format.h"
#include "audio/loader.h"
#include "audio/graph.h"
#include "graphics/device.h"
#include "graphics/painter.h"
#include "engine/audio.h"
#include "engine/renderer.h"

// We need this to create the rendering context.
#include "wdk/opengl/context.h"
#include "wdk/opengl/config.h"
#include "wdk/opengl/surface.h"

#include "test-shared.h"

namespace {
bool enable_pcm_caching=false;
bool enable_file_caching=false;

// setup context for headless rendering.
class TestContext : public gfx::Device::Context
{
public:
    TestContext(unsigned w, unsigned h)
    {
        wdk::Config::Attributes attrs;
        attrs.red_size         = 8;
        attrs.green_size       = 8;
        attrs.blue_size        = 8;
        attrs.alpha_size       = 8;
        attrs.stencil_size     = 8;
        attrs.surfaces.pbuffer = true;
        attrs.double_buffer    = false;
        attrs.sampling         = wdk::Config::Multisampling::MSAA4;
        attrs.srgb_buffer      = true;
        mConfig   = std::make_unique<wdk::Config>(attrs);
        mContext  = std::make_unique<wdk::Context>(*mConfig, 2, 0,  false, //debug
           wdk::Context::Type::OpenGL_ES);
        mSurface  = std::make_unique<wdk::Surface>(*mConfig, w, h);
        mContext->MakeCurrent(mSurface.get());
    }
    ~TestContext()
    {
        mContext->MakeCurrent(nullptr);
        mSurface->Dispose();
        mSurface.reset();
        mConfig.reset();
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

private:
    std::unique_ptr<wdk::Context> mContext;
    std::unique_ptr<wdk::Surface> mSurface;
    std::unique_ptr<wdk::Config>  mConfig;
};

struct Engine {
    base::TraceLog* trace_logger      = nullptr;
    base::TraceWriter* trace_writer   = nullptr;
    gfx::Device* graphics_device      = nullptr;
    gfx::Painter* graphics_painter    = nullptr;
    audio::Device* audio_device       = nullptr;
    audio::Loader* audio_loader       = nullptr;
    engine::AudioEngine* audio_engine = nullptr;
    engine::Renderer* renderer        = nullptr;
    engine::ClassLibrary* classlib    = nullptr;
};

class TestCase
{
public:
    virtual ~TestCase() = default;
    virtual void Prepare(Engine& engine) {}
    virtual void Execute(Engine& engine) = 0;
private:
};

class TestAudioFileDecode : public TestCase
{
public:
    TestAudioFileDecode(const std::string& file)
      : mFile(file)
    {}
    virtual void Execute(Engine& engine) override
    {
        auto laser = std::make_shared<audio::GraphClass>("laser", "21828282");
        audio::ElementCreateArgs elem;
        elem.type = "FileSource";
        elem.name = "file";
        elem.id = base::RandomString(10);
        elem.args["file"] = mFile;
        elem.args["type"] = audio::SampleType::Float32;
        elem.args["loops"] = 1u;
        elem.args["pcm_caching"] = enable_pcm_caching;
        elem.args["file_caching"] = enable_file_caching;
        const auto& e = laser->AddElement(std::move(elem));
        laser->SetGraphOutputElementId(e.id);
        laser->SetGraphOutputElementPort("out");

        std::vector<char> buffer;
        buffer.resize(1024);

        audio::AudioGraph::PrepareParams p;
        p.enable_pcm_caching = true;

        for (unsigned i = 0; i < 100; ++i)
        {
            audio::AudioGraph graph("graph", audio::Graph("graph", laser));
            graph.Prepare(*engine.audio_loader, p);
            std::uint64_t bytes = 0;
            while (graph.HasMore(bytes))
            {
                graph.FillBuffer(&buffer[0], buffer.size());
            }
        }
    }
private:
    const std::string mFile;
};

// typical assumed scenario. An audio graph that gets played over
// and over again in rapid succession. For example the sound of a
// player's weapon that gets fired when key is pressed/held pressed.
class TestAudioRapidFire : public TestCase
{
public:
    virtual void Execute(Engine& engine) override
    {
        auto laser = std::make_shared<audio::GraphClass>("laser", "21828282");
        audio::ElementCreateArgs elem;
        elem.type = "FileSource";
        elem.name = "file";
        elem.id = base::RandomString(10);
        elem.args["file"] = "assets/sounds/Laser_09.mp3";
        elem.args["type"] = audio::SampleType::Float32;
        elem.args["loops"] = 1u;
        elem.args["pcm_caching"] = enable_pcm_caching;
        elem.args["file_caching"] = enable_file_caching;
        const auto& e = laser->AddElement(std::move(elem));
        laser->SetGraphOutputElementId(e.id);
        laser->SetGraphOutputElementPort("out");

        for (int i = 0; i < 100; ++i)
        {
            engine.audio_engine->Update();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            engine.audio_engine->PlaySoundEffect(laser, 0u);
        }
    }
private:
};

// render a scene with multiple entities of the same type.
class TestRenderArmy : public TestCase
{
public:
    virtual void Prepare(Engine& engine) override
    {
        if (mScene)
            return;

        auto klass = std::make_shared<game::SceneClass>();

        for (int row=0; row<10; ++row)
        {
            for (int col=0; col<10; ++col)
            {
                game::SceneNodeClass node;
                node.SetEntityId(std::to_string(row * 10 + col));
                node.SetTranslation(col * 50.0f, row * 50.0f);
                node.SetName(std::to_string(row) + ":" + std::to_string(col));
                node.SetScale(1.0f, 1.0f);
                node.SetEntity(engine.classlib->FindEntityClassByName("M-6"));
                klass->LinkChild(nullptr, klass->AddNode(node));
            }
        }
        mScene = game::CreateSceneInstance(klass);
    }
    virtual void Execute(Engine& engine) override
    {
        TRACE_START();

        gfx::Transform transform;
        transform.Translate(50.0f, 50.0f);
        engine.graphics_device->BeginFrame();
        engine.graphics_device->ClearColor(gfx::Color4f(0.2f, 0.3f, 0.4f, 1.0f));
        engine.renderer->Draw(*mScene, *engine.graphics_painter, transform);
        engine.graphics_device->EndFrame();

        if (engine.trace_logger)
            engine.trace_logger->Write(*engine.trace_writer);

    }
private:
    std::unique_ptr<game::Scene> mScene;
};

} // namespace

int test_main(int argc, char* argv[])
{
    struct TestSpec {
        const char* name = nullptr;
        bool screenshot  = false;
        TestCase* test   = nullptr;
    } tests[] = {
        {"audio-rapid-fire", false, new TestAudioRapidFire()},
        {"audio-decode-mp3", false, new TestAudioFileDecode("assets/sounds/Laser_09.mp3")},
        {"audio-decode-ogg", false, new TestAudioFileDecode("assets/sounds/Laser_09.ogg")},
        {"audio-decode-wav", false, new TestAudioFileDecode("assets/sounds/Laser_09.wav")},
        { "render-army", true, new TestRenderArmy()}
    };

    base::LockedLogger<base::OStreamLogger> logger((base::OStreamLogger(std::cout)));
    base::SetGlobalLog(&logger);
    DEBUG("Hello!");

    base::CommandLineArgumentStack args(argc-1, (const char**)&argv[1]);
    base::CommandLineOptions opt;
    opt.Add("--debug-log", "Enable debug level log.");
    opt.Add("--loops", "Number of test loop iterations.", 1u);
    opt.Add("--help", "Print this help.");
    opt.Add("--timing", "Perform timing on tests.");
    opt.Add("--pcm-cache", "Enable audio PCM caching.");
    opt.Add("--enable-file-caching", "Enable file caching.");
    opt.Add("--screenshot", "Take screenshot of test cases with visual output.");
    opt.Add("--trace", "Trace file to write.", std::string());
    for (const auto& test : tests)
        opt.Add(test.name, "Test case");

    std::string cmdline_error;
    if (!opt.Parse(args, &cmdline_error, true))
    {
        std::fprintf(stdout, "Error parsing args. [err='%s']\n", cmdline_error.c_str());
        return 0;
    }
    if (opt.WasGiven("--help"))
    {
        opt.Print(std::cout);
        return 0;
    }
    enable_pcm_caching  = opt.WasGiven("--pcm-cache");
    enable_file_caching = opt.WasGiven("--file-cache");
    base::EnableDebugLog(opt.WasGiven("--debug-log"));

    std::unique_ptr<base::TraceWriter> trace_writer;
    std::unique_ptr<base::TraceLog> trace_logger;
    if (opt.WasGiven("--trace"))
    {
        const auto& trace_file = opt.GetValue<std::string>("--trace");
        if (base::EndsWith(trace_file, ".json"))
            trace_writer.reset(new base::ChromiumTraceJsonWriter(trace_file));
        else trace_writer.reset(new base::TextFileTraceWriter(trace_file));
        trace_logger.reset(new base::TraceLog(1000));
        base::SetThreadTrace(trace_logger.get());
    }

    TestClassLib classlib;

    constexpr auto SurfaceWidth  = 1024;
    constexpr auto SurfaceHeight = 768;
    const bool screenshot = opt.WasGiven("--screenshot");

    auto graphics_device = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
        std::make_shared<TestContext>(SurfaceWidth, SurfaceHeight));
    auto graphics_painter = gfx::Painter::Create(graphics_device);
    graphics_painter->SetSurfaceSize(SurfaceWidth, SurfaceHeight);
    graphics_painter->SetEditingMode(false);
    graphics_painter->SetOrthographicView(SurfaceWidth, SurfaceHeight);
    graphics_painter->SetViewport(0, 0, SurfaceWidth, SurfaceHeight);

    audio::Loader audio_loader;
    audio::Format audio_format;
    audio_format.channel_count = 2;
    audio_format.sample_rate   = 44100;
    audio_format.sample_type   = audio::SampleType::Float32;

    engine::AudioEngine audio_engine("test");
    audio_engine.SetBufferSize(20); // milliseconds
    audio_engine.SetLoader(&audio_loader);
    audio_engine.SetFormat(audio_format);
    audio_engine.Start();

    engine::Renderer renderer;
    renderer.SetEditingMode(false);
    renderer.SetClassLibrary(&classlib);

    Engine engine;
    engine.trace_logger     = trace_logger.get();
    engine.trace_writer     = trace_writer.get();
    engine.audio_loader     = &audio_loader;
    engine.audio_engine     = &audio_engine;
    engine.graphics_device  = graphics_device.get();
    engine.graphics_painter = graphics_painter.get();
    engine.renderer = &renderer;
    engine.classlib = &classlib;

    const auto iterations = opt.GetValue<unsigned>("--loops");
    for (const auto& spec : tests)
    {
        if (!opt.WasGiven(spec.name))
            continue;
        INFO("Running test case '%1'. [loops=%2]", spec.name, iterations);
        spec.test->Prepare(engine);
        if (opt.WasGiven("--timing"))
        {
            const auto& times = base::TimedTest(iterations, [&spec, &engine]() {
                spec.test->Execute(engine);
            } );
            base::PrintTestTimes(spec.name, times);
        }
        else
        {
            for (unsigned i=0; i<iterations; ++i)
                spec.test->Execute(engine);
        }
        if (spec.screenshot && screenshot)
        {
            const auto& rgba = graphics_device->ReadColorBuffer(SurfaceWidth, SurfaceHeight);
            const auto& name = std::string(spec.name) + ".png";
            gfx::WritePNG(rgba, name);
            INFO("Wrote screen capture '%1'", name);
        }
    }
    for (const auto& spec: tests)
        delete spec.test;

    return 0;
}
