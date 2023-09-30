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

#define LOGTAG "main"

#include "config.h"

#include "warnpush.h"
#  include <QCoreApplication>
#  include <QApplication>
#  include <QSurfaceFormat>
#  include <QDir>
#include "warnpop.h"

#include <chrono>
#include <iostream>

#if defined(LINUX_OS)
#  include <fenv.h>
#  include <dlfcn.h>
#elif defined(WINDOWS_OS)
#  include <Windows.h>
#endif

#include "base/logging.h"
#include "base/cmdline.h"
#include "graphics/resource.h"
#include "editor/app/workspace.h"
#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/app/ipc.h"
#include "editor/gui/playwindow.h"

class ForwardingLogger : public base::Logger
{
public:
    ForwardingLogger() : mLogger(std::cout)
    {}
    virtual void Write(base::LogEvent type, const char* file, int line, const char* msg) override
    {
        // 1. strip the file/line information. Events written into
        // gamehosts's app::EventLog don't have this.
        // 2. encode the type of the message in the message itself
        // and write to stdout for the editor process to read it.
        std::string prefix;
        if (type == base::LogEvent::Error)
            prefix = "E: ";
        else if (type == base::LogEvent::Warning)
            prefix = "W: ";
        else if (type == base::LogEvent::Info)
            prefix = "I: ";
        else if (type == base::LogEvent::Debug)
            prefix = "D: ";

        std::string message;
        message.append(prefix);
        message.append(msg);
        message.append("\n");
        mLogger.Write(type, message.c_str());
    }

    virtual void Write(base::LogEvent type, const char* msg) override
    {
        mLogger.Write(type, msg);
    }
    virtual void Flush() override
    {
        mLogger.Flush();
    }
    virtual base::bitflag<base::Logger::WriteType> GetWriteMask() const override
    {
        base::bitflag<base::Logger::WriteType> ret;
        ret.set(base::Logger::WriteType::WriteRaw, !mWriteFormatted);
        ret.set(base::Logger::WriteType::WriteFormatted, mWriteFormatted);
        return ret;
    }

    void EnableTerminalColors(bool on_off)
    {
        mLogger.EnableTerminalColors(on_off);
    }
    void WriteFormatted(bool on_off)
    { mWriteFormatted = on_off; }

private:
    base::OStreamLogger mLogger;
    bool mWriteFormatted = false;
};

void Main(int argc, char* argv[])
{
    base::CommandLineOptions options;
    options.Add("--app-style", "Name of the style to apply.", std::string(""));
    options.Add("--no-term-colors", "Turn off terminal colors.");
    options.Add("--standalone", "Run as a standalone executable.");
    options.Add("--workspace", "Path to workspace content dir.", std::string(""));
    options.Add("--socket-name", "Name of the local socket to connect to.", std::string(""));
    options.Add("--help", "Print this help.");

    std::string arg_error;
    if (!options.Parse(base::CreateStandardArgs(argc, argv), &arg_error))
    {
        std::cout << arg_error;
        std::cout << std::endl;
        return;
    }
    if (options.WasGiven("--help"))
    {
        options.Print(std::cout);
        return;
    }
    const bool term_colors = !options.WasGiven("--no-term-colors");
    const bool standalone  =  options.WasGiven("--standalone");
    std::string wsdir;
    std::string socket;
    std::string style;
    options.GetValue("--app-style", &style);
    options.GetValue("--workspace", &wsdir);
    if (!options.GetValue("--socket-name", &socket))
        socket = "gamestudio-local-socket";
    if (wsdir.empty())
    {
        std::cout << "No workspace directory given.\n";
        return;
    }

    // set the logger object for the subsystem to use, we'll
    // direct all this to the terminal for now.
    ForwardingLogger logger;
    logger.EnableTerminalColors(term_colors);
    logger.WriteFormatted(standalone);
    base::SetGlobalLog(&logger);
    base::EnableDebugLog(true);
    DEBUG("It's alive!");

    // capture log events written into app::EventLog.
    // The game host process doesn't have an event log for the host
    // application itself (the eventlog shows the events coming from the
    // *game*) so these log events are encoded and written into base logger
    // which writes them to stdout so that Editor application can read
    // them from the stdout of the child process (game host).
    app::EventLog::get().OnNewEvent = [](const app::Event& event) {
        base::LogEvent type;
        if (event.type == app::Event::Type::Info)
            type = base::LogEvent::Info;
        else if (event.type == app::Event::Type::Warning)
            type = base::LogEvent::Warning;
        else if (event.type == app::Event::Type::Error)
            type = base::LogEvent::Error;
        else if (event.type == app::Event::Type::Debug)
            type = base::LogEvent::Debug;
        else return;
        auto msg = app::ToUtf8(event.message);
        // currently the app::Event information doesn't have file/line
        // information.
        auto* logger = base::GetGlobalLog();
        if (logger->TestWriteMask(base::Logger::WriteType::WriteRaw))
            logger->Write(type, __FILE__, __LINE__, msg.c_str());

        if (logger->TestWriteMask(base::Logger::WriteType::WriteFormatted)) {
            msg.append("\n");
            logger->Write(type, msg.c_str());
        }
    };
/*
#if defined(LINUX_OS)
    // SIGFPE on floating point exception
    // todo: investigate the floating point exceptions that
    // started happening after integrating box2D. Is bo2D causing
    // them or just exposing some bugs in the other parts of the code?
    feenableexcept(FE_INVALID  |
                   FE_DIVBYZERO |
                   FE_OVERFLOW|
                   FE_UNDERFLOW
    );
    DEBUG("Enabled floating point exceptions");
#endif
 */

    // turn on Qt logging: QT_LOGGING_RULES = qt.qpa.gl
    // turns out this attribute is needed in order to make Qt
    // create a GLES2 context.
    // https://lists.qt-project.org/pipermail/interest/2015-February/015404.html
    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
    // set the aliases for icon search paths
    QDir::setSearchPaths("icons", QStringList(":/16x16_ico_png"));
    QDir::setSearchPaths("level", QStringList(":/32x32_ico_png"));

    // Set default surface format.
    // note that the alpha channel is not used on purpose.
    // using an alpha channel will cause artifacts with alpha
    // compositing window compositor such as picom. i.e. the
    // background surfaces in the compositor's window stack will
    // show through. in terms of alpha blending the game content    
    // whether the destination color buffer has alpha channel or 
    // not should be irrelevant. 
    QSurfaceFormat format;
    format.setVersion(3, 0);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setDepthBufferSize(24);
    format.setAlphaBufferSize(0); // no alpha channel
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    format.setSwapInterval(0);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setColorSpace(QSurfaceFormat::ColorSpace::sRGBColorSpace);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    // Add a path for Qt to look for the plugins at runtime
    // note that this needs to be called *after* the QApplication object has been created.
    QCoreApplication::addLibraryPath(app::JoinPath(QCoreApplication::applicationDirPath(), "plugins"));

    if (!style.empty())
    {
        app::SetStyle(app::FromUtf8(style));
    }

    app::Workspace workspace(app::FromUtf8(wsdir));
    if (!workspace.LoadWorkspace())
        return;
    gfx::SetResourceLoader(&workspace);

    app::IPCClient ipc;
    if (!standalone)
    {
        DEBUG("Connecting to local socket '%1'", socket);
        if (!ipc.Open(app::FromUtf8(socket)))
            return;

        DEBUG("IPC socket open!");
        // connect IPC signal for updating a resource we have loaded
        // in memory
        QObject::connect(&ipc, &app::IPCClient::ResourceUpdated,
                         &workspace, &app::Workspace::UpdateResource);
        // connect IPC signal for transmitting back user property changes.
        QObject::connect(&workspace, &app::Workspace::UserPropertyUpdated,
                         &ipc, &app::IPCClient::UserPropertyUpdated);
    }
    constexpr auto is_separate_process = true;
    gui::PlayWindow window(workspace, is_separate_process);
    window.ShowWithWAR();
    window.LoadState("play_window");
    if (!window.LoadGame())
        return;

    if (!style.empty())
    {
        app::SetTheme(app::FromUtf8(style));
    }

    // main game loop.
    unsigned frame = 0;
    while (!window.IsClosed())
    {
        app.processEvents();
        if (window.IsClosed())
            break;

        window.RunGameLoopOnce();

        if ((frame % 10) == 0)
        {
            window.NonGameTick();
            base::FlushGlobalLog();
        }

        ++frame;
    }
    window.NonGameTick();
    base::FlushGlobalLog();

    window.SaveState("play_window");
    window.Shutdown();
    gfx::SetResourceLoader(nullptr);
    DEBUG("Exiting...");
}

int main(int argc, char* argv[])
{
    try
    {
        Main(argc, argv);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Oops there was a problem:\n";
        std::cerr << e.what() << std::endl;
        return 1;
    }
    std::cout << "Have a good day.\n";
    std::cout << std::endl;
    return 0;
}
