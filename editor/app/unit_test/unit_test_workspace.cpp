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

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#  include <QCoreApplication>
#  include <QApplication>
#  include <QDir>
#include "warnpop.h"

#include <chrono>
#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/test_help.h"
#include "base/logging.h"
#include "base/utility.h"
#include "base/json.h"
#include "editor/app/resource.h"
#include "editor/app/workspace.h"
#include "editor/app/eventlog.h"
#include "engine/loader.h"
#include "engine/ui.h"
#include "graphics/types.h"
#include "uikit/window.h"
#include "uikit/widget.h"

void DeleteDir(const QString& dir)
{
    QDir d(dir);
    d.removeRecursively();
}

void MakeDir(const QString& dir)
{
    QDir d(dir);
    d.mkpath(dir);
}

template<typename Pixel>
size_t CountPixels(const gfx::Bitmap<Pixel>& bmp, gfx::Color color)
{
    size_t ret = 0;
    for (unsigned y=0; y<bmp.GetHeight(); ++y) {
        for (unsigned x=0; x<bmp.GetWidth(); ++x)
            if (bmp.GetPixel(y, x) == color)
                ++ret;
    }
    return ret;
}
size_t CountPixels(const gfx::Bitmap<gfx::Grayscale>& bmp, int value)
{
    size_t ret = 0;
    for (unsigned  y=0; y<bmp.GetHeight(); ++y) {
        for (unsigned x=0; x<bmp.GetWidth(); ++x)
            if (bmp.GetPixel(y, x).r == value)
                ++ret;
    }
    return ret;
}

void unit_test_path_mapping()
{
    const auto& cwd = QDir::currentPath();
    const auto& app = QCoreApplication::applicationDirPath();

    DeleteDir("TestWorkspace");
    MakeDir(app::JoinPath(cwd, "TestWorkspace"));

    app::Workspace workspace(app::JoinPath(cwd, "TestWorkspace"));

#if defined(WINDOWS_OS)
    TEST_REQUIRE(app::CleanPath("c:/foo/bar.png") == "c:\\foo\\bar.png");
    TEST_REQUIRE(app::CleanPath("c:\\foo\\bar.png") == "c:\\foo\\bar.png");
    TEST_REQUIRE(app::CleanPath("foo/bar/image.png") == "foo\\bar\\image.png");
#endif

    // test mapping of paths.
    // paths relative to the workspace are expressed using ws:// reference.
    // paths relative to the application are expressed using app:// reference
    // other paths are expressed using fs:// reference
    TEST_REQUIRE(workspace.MapFileToWorkspace(app::JoinPath(cwd , "TestWorkspace/relative/path/file.png")) == "ws://relative/path/file.png");
    TEST_REQUIRE(workspace.MapFileToWorkspace(app::JoinPath(app , "relative/path/file.png")) == "app://relative/path/file.png");

    TEST_REQUIRE(workspace.MapFileToWorkspace(app::JoinPath(cwd , "TestWorkspace\\relative\\path\\file.png")) == "ws://relative/path/file.png");
    TEST_REQUIRE(workspace.MapFileToWorkspace(app::JoinPath(app , "relative\\path\\file.png")) == "app://relative/path/file.png");
    TEST_REQUIRE(workspace.MapFileToWorkspace(app::JoinPath(cwd , "TestWorkspace/some/folder")) == "ws://some/folder");
    TEST_REQUIRE(workspace.MapFileToWorkspace(app::JoinPath(cwd , "TestWorkspace\\some\\folder")) == "ws://some/folder");
#if defined(POSIX_OS)
    TEST_REQUIRE(workspace.MapFileToWorkspace(QString("/tmp/file.png")) == "fs:///tmp/file.png");
    TEST_REQUIRE(workspace.MapFileToFilesystem(QString("fs:///tmp/file.png")) == "/tmp/file.png");
    TEST_REQUIRE(workspace.MapFileToWorkspace(QString("some/file/name.png")) == "fs://some/file/name.png");
    TEST_REQUIRE(workspace.MapFileToFilesystem(QString("fs://some/file/name.png")) == "some/file/name.png");
    TEST_REQUIRE(workspace.MapFileToWorkspace(QString("/tmp/some/folder")) == "fs:///tmp/some/folder");
    TEST_REQUIRE(workspace.MapFileToFilesystem(QString("fs:///tmp/some/folder")) == "/tmp/some/folder");
#elif defined(WINDOWS_OS)
    TEST_REQUIRE(workspace.MapFileToWorkspace(QString("c:\\tmp\\file.png")) == "fs://c:\\tmp\\file.png");
    TEST_REQUIRE(workspace.MapFileToFilesystem(QString("fs://c:\\tmp\\file.png")) == "c:\\tmp\\file.png");
    TEST_REQUIRE(workspace.MapFileToWorkspace(QString("some\\file\\name.png")) == "fs://some\\file\\name.png");
    TEST_REQUIRE(workspace.MapFileToFilesystem(QString("fs://some\\file\\name.png")) == "some\\file\\name.png");
#endif
    TEST_REQUIRE(workspace.MapFileToFilesystem(QString("ws://relative/path/file.png")) == app::JoinPath(cwd, "TestWorkspace/relative/path/file.png"));
    TEST_REQUIRE(workspace.MapFileToFilesystem(QString("app://relative/path/file.png")) == app::JoinPath(app, "relative/path/file.png"));

    // don't re-encode already encoded file names.
    TEST_REQUIRE(workspace.MapFileToWorkspace(QString("ws://relative/path/file.png")) == "ws://relative/path/file.png");
    TEST_REQUIRE(workspace.MapFileToWorkspace(QString("app://foo/bar/file.png")) == "app://foo/bar/file.png");
    TEST_REQUIRE(workspace.MapFileToWorkspace(QString("fs:///tmp/file.png")) == "fs:///tmp/file.png");
}

void unit_test_resource()
{
    DeleteDir("TestWorkspace");
    MakeDir("TestWorkspace");

    app::Workspace workspace("TestWorkspace");
    TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 0);
    TEST_REQUIRE(workspace.GetNumResources() != 0);
    TEST_REQUIRE(workspace.GetNumPrimitiveResources() != 0);

    const auto primitives      = workspace.GetNumPrimitiveResources();
    const auto first_primitive = workspace.GetPrimitiveResource(0).Copy();
    const auto last_primitive  = workspace.GetPrimitiveResource(primitives-1).Copy();

    gfx::ColorClass material;
    app::MaterialResource material_resource(material, "material");
    workspace.SaveResource(material_resource);

    TEST_REQUIRE(workspace.GetNumPrimitiveResources() == primitives);
    TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 1);
    TEST_REQUIRE(workspace.GetPrimitiveResource(0).GetId() == first_primitive->GetId());
    TEST_REQUIRE(workspace.GetPrimitiveResource(primitives-1).GetId() == last_primitive->GetId());
    TEST_REQUIRE(workspace.GetUserDefinedResource(0).GetIdUtf8() == material.GetId());
    TEST_REQUIRE(workspace.GetUserDefinedResource(0).GetId() == material_resource.GetId());
    TEST_REQUIRE(workspace.GetNumResources() == primitives+1);

    workspace.DeleteResources(std::vector<size_t>{0});
    TEST_REQUIRE(workspace.GetNumPrimitiveResources() == primitives);
    TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 0);
    TEST_REQUIRE(workspace.GetPrimitiveResource(0).GetId() == first_primitive->GetId());
    TEST_REQUIRE(workspace.GetPrimitiveResource(primitives-1).GetId() == last_primitive->GetId());

    gfx::PolygonClass poly;
    app::CustomShapeResource shape_resource(poly, "poly");
    gfx::KinematicsParticleEngineClass particles;
    app::ParticleSystemResource  particle_resource(particles, "particles");

    workspace.SaveResource(shape_resource);
    workspace.SaveResource(material_resource);
    workspace.SaveResource(particle_resource);
    TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 3);
    workspace.DeleteResources(std::vector<size_t>{2, 0, 1});
    TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 0);
    TEST_REQUIRE(workspace.GetNumPrimitiveResources() == primitives);

    workspace.SaveResource(shape_resource);
    workspace.SaveResource(material_resource);
    workspace.SaveResource(particle_resource);
    workspace.DeleteResources(std::vector<size_t>{1});
    TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 2);
    TEST_REQUIRE(workspace.GetUserDefinedResource(0).GetId() == shape_resource.GetId());
    TEST_REQUIRE(workspace.GetUserDefinedResource(1).GetId() == particle_resource.GetId());

    workspace.DuplicateResources(std::vector<size_t>{0, 1});
    TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 4);
    TEST_REQUIRE(workspace.GetUserDefinedResource(0).GetId() != shape_resource.GetId());
    TEST_REQUIRE(workspace.GetUserDefinedResource(0).GetName() == "Copy of poly");
    TEST_REQUIRE(workspace.GetUserDefinedResource(1).GetId() == shape_resource.GetId());
    TEST_REQUIRE(workspace.GetUserDefinedResource(2).GetId() != particle_resource.GetId());
    TEST_REQUIRE(workspace.GetUserDefinedResource(2).GetName() == "Copy of particles");
    TEST_REQUIRE(workspace.GetUserDefinedResource(3).GetId() == particle_resource.GetId());

    workspace.DeleteResources(std::vector<size_t>{0, 2, 3, 1});
    TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 0);
    TEST_REQUIRE(workspace.GetNumPrimitiveResources() == primitives);
}

void unit_test_save_load()
{
    DeleteDir("TestWorkspace");
    MakeDir("TestWorkspace"); // initially empty workspace folder.

    // save workspace and some content
    {
        app::Workspace workspace("TestWorkspace");
        TEST_REQUIRE(workspace.LoadWorkspace() == false);
        TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 0);

        // add some user defined content.
        gfx::ColorClass material;
        material.SetId("foo123");
        app::MaterialResource resource(material, "TestMaterial");
        resource.SetProperty("int", 123);
        resource.SetProperty("str", QString("hello"));
        resource.SetUserProperty("foo", 444);
        resource.SetUserProperty("bar", 777);
        workspace.SaveResource(resource);
        // workspace properties are specific to the workspace and
        // are saved in the workspace files.
        // user properties are private and important only to the particular user
        // and stored in the dot (.filename) file
        workspace.SetProperty("int", 123);
        workspace.SetProperty("str", QString("hello"));
        workspace.SetUserProperty("user-int", 321);
        workspace.SetUserProperty("user-str", QString("hullo"));

        // set project settings.
        app::Workspace::ProjectSettings settings;
        settings.multisample_sample_count = 16;
        settings.application_name = "foobar";
        settings.application_version = "1.1.1";
        settings.application_library_win = "library.dll";
        settings.application_library_lin = "liblibrary.so";
        settings.default_min_filter = gfx::Device::MinFilter::Mipmap;
        settings.default_mag_filter = gfx::Device::MagFilter::Linear;
        settings.window_mode = app::Workspace::ProjectSettings::WindowMode::Fullscreen;
        settings.window_width = 600;
        settings.window_height = 400;
        settings.window_has_border = false;
        settings.window_can_resize = false;
        settings.window_vsync = true;
        settings.ticks_per_second = 100;
        settings.updates_per_second = 100;
        settings.working_folder = "blah";
        settings.command_line_arguments = "args";
        settings.use_gamehost_process = false;
        workspace.SetProjectSettings(settings);
        TEST_REQUIRE(workspace.SaveWorkspace());
    }

    // load workspace
    {
        app::Workspace workspace("TestWorkspace");
        TEST_REQUIRE(workspace.LoadWorkspace());
        TEST_REQUIRE(workspace.GetDir().contains("TestWorkspace"));
        TEST_REQUIRE(workspace.GetName().isEmpty() == false);
        TEST_REQUIRE(workspace.HasUserProperty("user-int"));
        TEST_REQUIRE(workspace.HasUserProperty("user-str"));
        TEST_REQUIRE(workspace.HasProperty("int"));
        TEST_REQUIRE(workspace.HasProperty("str"));
        TEST_REQUIRE(workspace.GetProperty("int", 0) == 123);
        TEST_REQUIRE(workspace.GetProperty("str", QString("")) == "hello");
        TEST_REQUIRE(workspace.HasUserProperty("user-int"));
        TEST_REQUIRE(workspace.HasUserProperty("user-str"));
        TEST_REQUIRE(workspace.GetUserProperty("user-int", 0) == 321);
        TEST_REQUIRE(workspace.GetUserProperty("user-str", QString("")) == "hullo");
        TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 1);
        {
            const auto& res = workspace.GetUserDefinedResource(0);
            TEST_REQUIRE(res.GetName() == "TestMaterial");
            TEST_REQUIRE(res.GetIdUtf8() == "foo123");
            TEST_REQUIRE(res.GetProperty("int", 0) == 123);
            TEST_REQUIRE(res.GetProperty("str", QString("")) == QString("hello"));
            TEST_REQUIRE(res.GetUserProperty("foo", 0) == 444);
            TEST_REQUIRE(res.GetUserProperty("bar", 0) == 777);
        }
        TEST_REQUIRE(workspace.GetProjectSettings().multisample_sample_count == 16);
        TEST_REQUIRE(workspace.GetProjectSettings().application_name == "foobar");
        TEST_REQUIRE(workspace.GetProjectSettings().application_version == "1.1.1");
        TEST_REQUIRE(workspace.GetProjectSettings().application_library_win == "library.dll");
        TEST_REQUIRE(workspace.GetProjectSettings().application_library_lin == "liblibrary.so");
        TEST_REQUIRE(workspace.GetProjectSettings().default_min_filter == gfx::Device::MinFilter::Mipmap);
        TEST_REQUIRE(workspace.GetProjectSettings().default_mag_filter == gfx::Device::MagFilter::Linear);
        TEST_REQUIRE(
                workspace.GetProjectSettings().window_mode == app::Workspace::ProjectSettings::WindowMode::Fullscreen);
        TEST_REQUIRE(workspace.GetProjectSettings().window_width == 600);
        TEST_REQUIRE(workspace.GetProjectSettings().window_height == 400);
        TEST_REQUIRE(workspace.GetProjectSettings().window_has_border == false);
        TEST_REQUIRE(workspace.GetProjectSettings().window_can_resize == false);
        TEST_REQUIRE(workspace.GetProjectSettings().window_vsync == true);
        TEST_REQUIRE(workspace.GetProjectSettings().ticks_per_second == 100);
        TEST_REQUIRE(workspace.GetProjectSettings().updates_per_second == 100);
        TEST_REQUIRE(workspace.GetProjectSettings().working_folder == "blah");
        TEST_REQUIRE(workspace.GetProjectSettings().command_line_arguments == "args");
        TEST_REQUIRE(workspace.GetProjectSettings().use_gamehost_process == false);
    }

}

void unit_test_packing_basic()
{
    DeleteDir("TestWorkspace");
    DeleteDir("TestPackage");

    QDir d;
    // setup dummy shaders and data.
    TEST_REQUIRE(d.mkpath("shaders/es2"));
    TEST_REQUIRE(d.mkpath("lua"));
    TEST_REQUIRE(d.mkpath("audio"));
    TEST_REQUIRE(d.mkpath("data"));
    TEST_REQUIRE(d.mkpath("fonts"));
    TEST_REQUIRE(d.mkpath("ui"));
    TEST_REQUIRE(app::WriteTextFile("shaders/es2/my_material.glsl", "my_material.glsl"));
    // setup dummy scripts this one is global (outside the workspace tree)
    TEST_REQUIRE(app::WriteTextFile("lua/game_script.lua", "game_script.lua"));
    TEST_REQUIRE(app::WriteTextFile("audio/music.mp3", "music.mp3"));
    TEST_REQUIRE(app::WriteTextFile("data/levels.txt", "levels.txt"));
    // setup dummy font file
    TEST_REQUIRE(app::WriteTextFile("fonts/font.otf", "font.otf"));
    // setup dummy UI style file
    QString style(
R"(
{
  "properties": [
     {
       "key": "widget/border-width",
       "value": 1.0
     }
   ],

  "materials": [
     {
       "key": "widget/background",
       "type": "Null"
     }
  ]
}
)");
    TEST_REQUIRE(app::WriteTextFile("ui/style.json", style));

    MakeDir("TestWorkspace");
    app::Workspace workspace("TestWorkspace");

    // set project settings.
    app::Workspace::ProjectSettings settings;
    settings.multisample_sample_count = 16;
    settings.application_name = "foobar";
    settings.application_version = "1.1.1";
    settings.application_library_lin = "libgame.so";
    settings.application_library_win = "game.dll";
    settings.default_min_filter = gfx::Device::MinFilter::Mipmap;
    settings.default_mag_filter = gfx::Device::MagFilter::Linear;
    settings.window_mode = app::Workspace::ProjectSettings::WindowMode::Fullscreen;
    settings.window_width = 600;
    settings.window_height = 400;
    settings.window_has_border = false;
    settings.window_can_resize = false;
    settings.window_vsync = true;
    settings.ticks_per_second = 100;
    settings.updates_per_second = 50;
    settings.working_folder = "blah";
    settings.command_line_arguments = "args";
    settings.use_gamehost_process = false;
    workspace.SetProjectSettings(settings);

    // setup some content.
    gfx::CustomMaterialClass material;
    material.SetShaderUri(workspace.MapFileToWorkspace(std::string("shaders/es2/my_material.glsl")));
    app::MaterialResource material_resource(material, "material");
    gfx::PolygonClass poly;
    app::CustomShapeResource shape_resource(poly, "poly");
    gfx::KinematicsParticleEngineClass particles;
    app::ParticleSystemResource  particle_resource(particles, "particles");

    app::Script script;
    script.SetFileURI(workspace.MapFileToWorkspace(std::string("lua/game_script.lua")));
    app::ScriptResource script_resource(script, "GameScript");

    audio::GraphClass audio_graph("music_graph");
    audio::GraphClass::Element music_src;
    music_src.id   = base::RandomString(10);
    music_src.name = "music";
    music_src.type = "FileSource";
    music_src.args["file"] = workspace.MapFileToWorkspace(std::string("audio/music.mp3"));
    audio_graph.AddElement(std::move(music_src));
    app::AudioResource audio_resource(audio_graph, "music.mp3");

    app::DataFile data;
    data.SetFileURI(workspace.MapFileToWorkspace(std::string("data/levels.txt")));
    app::DataResource data_resource(data, "levels.txt");

    uik::Window window;
    window.SetStyleName(workspace.MapFileToWorkspace(std::string("ui/style.json")));
    app::UIResource ui_resource(window, "UI");

    workspace.SaveResource(material_resource);
    workspace.SaveResource(shape_resource);
    workspace.SaveResource(particle_resource);
    workspace.SaveResource(script_resource);
    workspace.SaveResource(audio_resource);
    workspace.SaveResource(data_resource);
    workspace.SaveResource(ui_resource);

    // setup entity resource that uses a font resource.
    {
        game::TextItemClass text;
        text.SetFontName(workspace.MapFileToWorkspace(std::string("fonts/font.otf")));
        text.SetText("hello");

        game::EntityNodeClass node;
        node.SetName("node");
        node.SetTextItem(text);

        game::EntityClass entity;
        entity.SetName("entity");
        entity.AddNode(node);

        app::EntityResource resource(entity, "entity");
        workspace.SaveResource(resource);
    }

    app::Workspace::ContentPackingOptions options;
    options.directory    = "TestPackage";
    options.package_name = "test";
    options.write_content_file = true;
    options.write_config_file  = true;
    options.combine_textures   = false;
    options.resize_textures    = false;

    // select the resources.
    std::vector<const app::Resource*> resources;
    resources.push_back(&workspace.GetUserDefinedResource(0));
    resources.push_back(&workspace.GetUserDefinedResource(1));
    resources.push_back(&workspace.GetUserDefinedResource(2));
    resources.push_back(&workspace.GetUserDefinedResource(3));
    resources.push_back(&workspace.GetUserDefinedResource(4));
    resources.push_back(&workspace.GetUserDefinedResource(5));
    resources.push_back(&workspace.GetUserDefinedResource(6));
    resources.push_back(&workspace.GetUserDefinedResource(7));
    TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

    // in the output folder we should have content.json, config.json
    // and the shaders copied into shaders/es2/
    TEST_REQUIRE(app::ReadTextFile("TestPackage/test/shaders/es2/my_material.glsl") == "my_material.glsl");
    // Lua scripts should be copied into lua/
    TEST_REQUIRE(app::ReadTextFile("TestPackage/test/lua/game_script.lua") == "game_script.lua");
    // Audio files should be copied into audio/
    TEST_REQUIRE(app::ReadTextFile("TestPackage/test/audio/music.mp3") == "music.mp3");
    // Data files should be copied into data/
    TEST_REQUIRE(app::ReadTextFile("TestPackage/test/data/levels.txt") == "levels.txt");
    // Font files should be copied into fonts/
    TEST_REQUIRE(app::ReadTextFile("TestPackage/test/fonts/font.otf") == "font.otf");
    // UI style files should be copied into ui/
    // the UI style is rewritten when the resource references are re-mapped and thus
    // may not be the exact same string as what was originally written.
    const auto& style_string = app::ReadTextFile("TestPackage/test/ui/style.json");
    TEST_REQUIRE(style_string.contains("materials"));
    TEST_REQUIRE(style_string.contains("widget/background"));
    TEST_REQUIRE(style_string.contains("widget/border-width"));

    auto loader = engine::JsonFileClassLoader::Create();
    loader->LoadClassesFromFile("TestPackage/test/content.json");
    TEST_REQUIRE(loader->FindMaterialClassById(material.GetId()));
    TEST_REQUIRE(loader->FindDrawableClassById(poly.GetId()));
    TEST_REQUIRE(loader->FindDrawableClassById(particles.GetId()));

    auto [ok, json, error] = base::JsonParseFile("TestPackage/config.json");
    TEST_REQUIRE(ok);
    TEST_REQUIRE(json["config"]["sampling"] == "MSAA16");
    TEST_REQUIRE(json["config"]["srgb"] == true);
    TEST_REQUIRE(json["window"]["can_resize"] == false);
    TEST_REQUIRE(json["window"]["has_border"] == false);
    TEST_REQUIRE(json["window"]["width"] == 600);
    TEST_REQUIRE(json["window"]["height"] == 400);
    TEST_REQUIRE(json["window"]["set_fullscreen"] == true);
    TEST_REQUIRE(json["window"]["vsync"] == true);
    TEST_REQUIRE(json["window"]["cursor"] == true);
    TEST_REQUIRE(json["application"]["title"] == "foobar");
    TEST_REQUIRE(json["application"]["version"] == "1.1.1");
    TEST_REQUIRE(json["application"]["library"] == "game");
    TEST_REQUIRE(json["engine"]["ticks_per_second"] == 100.0);
    TEST_REQUIRE(json["engine"]["updates_per_second"] == 50.0);

    DeleteDir("TestPackage");
    options.write_config_file = false;
    options.write_content_file = false;
    workspace.BuildReleasePackage(resources, options);
    TEST_REQUIRE(!base::FileExists("TestPackage/test/content.json"));
    TEST_REQUIRE(!base::FileExists("TestPackage/config.json"));
}

void unit_test_packing_texture_composition(unsigned padding)
{
    // generate some test textures.
    gfx::RgbBitmap bitmap[4];
    bitmap[0].Resize(64, 64);
    bitmap[0].Fill(gfx::Color::Blue);
    bitmap[1].Resize(64, 64);
    bitmap[1].Fill(gfx::Color::Red);
    bitmap[2].Resize(512, 512);
    bitmap[2].Fill(gfx::Color::Green);
    bitmap[3].Resize(1024, 1024);
    bitmap[3].Fill(gfx::Color::Yellow);

    gfx::WritePNG(bitmap[0], "test_bitmap0.png");
    gfx::WritePNG(bitmap[1], "test_bitmap1.png");
    gfx::WritePNG(bitmap[2], "test_bitmap2.png");
    gfx::WritePNG(bitmap[3], "test_bitmap3.png");

    // start with 1 texture. nothing will be combined since
    // there's just 1 texture.
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");

        gfx::TextureMap2DClass material;
        material.SetTexture(gfx::LoadTextureFromFile("test_bitmap0.png"));
        app::MaterialResource resource(material, "material");

        MakeDir("TestWorkspace");
        app::Workspace workspace("TestWorkspace");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory          = "TestPackage";
        options.package_name       = "";
        options.write_content_file = true;
        options.write_config_file  = true;
        options.combine_textures   = true;
        options.resize_textures    = false;
        options.texture_pack_width = 1024;
        options.texture_pack_width = 1024;
        options.max_texture_width  = 1024;
        options.max_texture_height = 1024;
        options.texture_padding    = padding;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

        gfx::Image generated;
        TEST_REQUIRE(generated.Load("TestPackage/textures/test_bitmap0.png"));
        const auto& bmp = generated.AsBitmap<gfx::RGB>();
        TEST_REQUIRE(bmp.GetWidth() == bitmap[0].GetWidth());
        TEST_REQUIRE(bmp.GetHeight() == bitmap[0].GetHeight());
        TEST_REQUIRE(gfx::Compare(bitmap[0], bmp));
    }

    // use 2 small textures. packing should be done.
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");

        gfx::SpriteClass material;
        material.AddTexture(gfx::LoadTextureFromFile("test_bitmap0.png"));
        material.AddTexture(gfx::LoadTextureFromFile("test_bitmap1.png"));
        app::MaterialResource resource(material, "material");

        MakeDir("TestWorkspace");
        app::Workspace workspace("TestWorkspace");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory           = "TestPackage";
        options.package_name        = "";
        options.write_content_file  = true;
        options.write_config_file   = true;
        options.combine_textures    = true;
        options.resize_textures     = false;
        options.max_texture_width   = 1024;
        options.max_texture_height  = 1024;
        options.texture_pack_width  = 1024;
        options.texture_pack_height = 1024;
        options.texture_padding     = padding;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

        gfx::Image generated;
        TEST_REQUIRE(generated.Load("TestPackage/textures/Generated_0.png"));
        const auto& bmp = generated.AsBitmap<gfx::RGB>();
        TEST_REQUIRE(bmp.GetWidth() == 1024 && bmp.GetWidth() == 1024);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Blue) == (64+2*padding)*(64+2*padding));
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Red) == (64+2*padding)*(64+2*padding));
    }

    // disable packing, should get 2 textures
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");

        gfx::SpriteClass material;
        material.AddTexture(gfx::LoadTextureFromFile("test_bitmap0.png"));
        material.AddTexture(gfx::LoadTextureFromFile("test_bitmap1.png"));
        app::MaterialResource resource(material, "material");

        MakeDir("TestWorkspace");
        app::Workspace workspace("TestWorkspace");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory           = "TestPackage";
        options.package_name        = "";
        options.write_content_file  = true;
        options.write_config_file   = true;
        options.combine_textures    = false;
        options.resize_textures     = false;
        options.max_texture_width   = 1024;
        options.max_texture_height  = 1024;
        options.texture_pack_width  = 1024;
        options.texture_pack_height = 1024;
        options.texture_padding     = padding;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

        gfx::Image img;
        TEST_REQUIRE(img.Load("TestPackage/textures/test_bitmap0.png"));
        TEST_REQUIRE(gfx::Compare(bitmap[0], img.AsBitmap<gfx::RGB>()));

        TEST_REQUIRE(img.Load("TestPackage/textures/test_bitmap1.png"));
        TEST_REQUIRE(gfx::Compare(bitmap[1], img.AsBitmap<gfx::RGB>()));
    }

    // texture size that exceeds the max texture sizes and no resizing and no packing
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");
        MakeDir("TestWorkspace");

        gfx::TextureMap2DClass material;
        material.SetTexture(gfx::LoadTextureFromFile("test_bitmap3.png"));
        app::MaterialResource resource(material, "material");

        app::Workspace workspace("TestWorkspace");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory          = "TestPackage";
        options.package_name       = "";
        options.write_content_file = true;
        options.write_config_file  = true;
        options.combine_textures   = false;
        options.resize_textures    = false;
        options.max_texture_width  = 512;
        options.max_texture_height = 512;
        options.texture_padding    = padding;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

        gfx::Image img;
        TEST_REQUIRE(img.Load("TestPackage/textures/test_bitmap3.png"));
        TEST_REQUIRE(gfx::Compare(bitmap[3], img.AsBitmap<gfx::RGB>()));
    }

    // texture size that exceeds the max texture size gets resized.
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");
        MakeDir("TestWorkspace");

        gfx::TextureMap2DClass material;
        material.SetTexture(gfx::LoadTextureFromFile("test_bitmap3.png"));
        app::MaterialResource resource(material, "material");

        app::Workspace workspace("TestWorkspace");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory          = "TestPackage";
        options.package_name       = "";
        options.write_content_file = true;
        options.write_config_file  = true;
        options.combine_textures   = false;
        options.resize_textures    = true;
        options.max_texture_width  = 512;
        options.max_texture_height = 512;
        options.texture_padding    = padding;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

        gfx::Image img;
        TEST_REQUIRE(img.Load("TestPackage/textures/test_bitmap3.png"));
        const auto& bmp = img.AsBitmap<gfx::RGB>();
        TEST_REQUIRE(bmp.GetHeight() == 512);
        TEST_REQUIRE(bmp.GetWidth() == 512);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Yellow) == 512*512);
    }

    // test discarding multiple copies of textures while combining
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");
        MakeDir("TestWorkspace");

        app::Workspace workspace("TestWorkspace");

        // first material
        {
            gfx::SpriteClass material;
            material.AddTexture(gfx::LoadTextureFromFile("test_bitmap0.png"));
            material.AddTexture(gfx::LoadTextureFromFile("test_bitmap1.png"));
            app::MaterialResource resource(material, "material 1");
            workspace.SaveResource(resource);
        }
        // second material
        {
            gfx::SpriteClass material;
            material.AddTexture(gfx::LoadTextureFromFile("test_bitmap0.png"));
            material.AddTexture(gfx::LoadTextureFromFile("test_bitmap1.png"));
            material.AddTexture(gfx::LoadTextureFromFile("test_bitmap2.png"));
            material.AddTexture(gfx::LoadTextureFromFile("test_bitmap3.png"));
            app::MaterialResource resource(material, "material 2");
            workspace.SaveResource(resource);
        }

        app::Workspace::ContentPackingOptions options;
        options.directory = "TestPackage";
        options.package_name = "";
        options.write_content_file = true;
        options.write_config_file  = true;
        options.combine_textures   = true;
        options.resize_textures    = false;
        options.max_texture_width  = 1024;
        options.max_texture_height = 1024;
        options.texture_padding    = padding;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        resources.push_back(&workspace.GetUserDefinedResource(1));
        TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

        // bitmap0 and bitmap1 should only be copied once and combined
        // with bitmap2. Bitmap3 is too large to pack.
        gfx::Image generated;
        TEST_REQUIRE(generated.Load("TestPackage/textures/Generated_0.png"));
        const auto& bmp = generated.AsBitmap<gfx::RGB>();
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Blue) == (64+2*padding)*(64+2*padding));
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Red) == (64+2*padding)*(64+2*padding));
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Green) == (512+2*padding)*(512+2*padding));

        gfx::Image img;
        TEST_REQUIRE(img.Load("TestPackage/textures/test_bitmap3.png"));
        TEST_REQUIRE(gfx::Compare(bitmap[3], img.AsBitmap<gfx::RGB>()));
    }

    // test discarding multiple copies of textures
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");
        MakeDir("TestWorkspace");

        app::Workspace workspace("TestWorkspace");

        // first material
        {
            gfx::SpriteClass material;
            material.AddTexture(gfx::LoadTextureFromFile("test_bitmap0.png"));
            material.AddTexture(gfx::LoadTextureFromFile("test_bitmap1.png"));
            app::MaterialResource resource(material, "material 1");
            workspace.SaveResource(resource);
        }
        // second material
        {
            gfx::SpriteClass material;
            material.AddTexture(gfx::LoadTextureFromFile("test_bitmap0.png"));
            material.AddTexture(gfx::LoadTextureFromFile("test_bitmap1.png"));
            material.AddTexture(gfx::LoadTextureFromFile("test_bitmap2.png"));
            material.AddTexture(gfx::LoadTextureFromFile("test_bitmap3.png"));
            app::MaterialResource resource(material, "material 2");
            workspace.SaveResource(resource);
        }

        app::Workspace::ContentPackingOptions options;
        options.directory = "TestPackage";
        options.package_name = "";
        options.write_content_file = true;
        options.write_config_file  = true;
        options.combine_textures   = false; // !
        options.resize_textures    = false;
        options.max_texture_width  = 1024;
        options.max_texture_height = 1024;
        options.texture_padding    = padding;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        resources.push_back(&workspace.GetUserDefinedResource(1));
        TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

        gfx::Image img;
        TEST_REQUIRE(img.Load("TestPackage/textures/test_bitmap0.png"));
        TEST_REQUIRE(gfx::Compare(bitmap[0], img.AsBitmap<gfx::RGB>()));
        TEST_REQUIRE(img.Load("TestPackage/textures/test_bitmap1.png"));
        TEST_REQUIRE(gfx::Compare(bitmap[1], img.AsBitmap<gfx::RGB>()));
        TEST_REQUIRE(img.Load("TestPackage/textures/test_bitmap2.png"));
        TEST_REQUIRE(gfx::Compare(bitmap[2], img.AsBitmap<gfx::RGB>()));
        TEST_REQUIRE(img.Load("TestPackage/textures/test_bitmap3.png"));
        TEST_REQUIRE(gfx::Compare(bitmap[3], img.AsBitmap<gfx::RGB>()));
    }

    // todo: test cases where texture packing cannot be done (see material.cpp)
}

void unit_test_packing_texture_composition_format()
{
    // source textures with different formats should not be combined
    // but rather only textures with the same format should be combined.
    // in other words, rgba textures can go into a rgba atlas, rgb textures
    // can be combined into rbg atlas and 8bit grayscale textures into
    // 8bit grayscale atlas.

    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");
        MakeDir("TestWorkspace");

        gfx::AlphaMask masks[2];
        gfx::RgbBitmap rgb_textures[2];
        gfx::RgbaBitmap rgba_textures[2];

        masks[0].Resize(64, 64);
        masks[0].Fill(gfx::Grayscale(0x20));
        masks[1].Resize(50, 180);
        masks[1].Fill(gfx::Grayscale(0x45));

        rgb_textures[0].Resize(80, 166);
        rgb_textures[0].Fill(gfx::Color::Red);
        rgb_textures[1].Resize(64, 64);
        rgb_textures[1].Fill(gfx::Color::Yellow);

        rgba_textures[0].Resize(100, 100);
        rgba_textures[0].Fill(gfx::Color::Blue);
        rgba_textures[1].Resize(200, 100);
        rgba_textures[1].Fill(gfx::Color::Red);

        gfx::WritePNG(masks[0], "test_8bit_bitmap0.png");
        gfx::WritePNG(masks[1], "test_8bit_bitmap1.png");
        gfx::WritePNG(rgb_textures[0], "test_24bit_bitmap0.png");
        gfx::WritePNG(rgb_textures[1], "test_24bit_bitmap1.png");
        gfx::WritePNG(rgba_textures[0], "test_32bit_bitmap0.png");
        gfx::WritePNG(rgba_textures[1], "test_32bit_bitmap1.png");

        gfx::SpriteClass material;
        material.AddTexture(gfx::LoadTextureFromFile("test_8bit_bitmap0.png"));
        material.AddTexture(gfx::LoadTextureFromFile("test_8bit_bitmap1.png"));
        material.AddTexture(gfx::LoadTextureFromFile("test_24bit_bitmap0.png"));
        material.AddTexture(gfx::LoadTextureFromFile("test_24bit_bitmap1.png"));
        material.AddTexture(gfx::LoadTextureFromFile("test_32bit_bitmap0.png"));
        material.AddTexture(gfx::LoadTextureFromFile("test_32bit_bitmap1.png"));
        app::MaterialResource resource(material, "material");

        MakeDir("TestWorkspace");
        app::Workspace workspace("TestWorkspace");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory           = "TestPackage";
        options.package_name        = "";
        options.write_content_file  = true;
        options.write_config_file   = true;
        options.combine_textures    = true;
        options.resize_textures     = false;
        options.max_texture_width   = 1024;
        options.max_texture_height  = 1024;
        options.texture_pack_width  = 1024;
        options.texture_pack_height = 1024;
        options.texture_padding     = 0;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

        // todo: assuming a specific order in which the textures are generated. this needs to be fixed.

        // RGBA
        {
            gfx::Image generated;
            TEST_REQUIRE(generated.Load("TestPackage/textures/Generated_0.png"));
            TEST_REQUIRE(generated.GetWidth() == 1024);
            TEST_REQUIRE(generated.GetHeight() == 1024);
            TEST_REQUIRE(generated.GetDepthBits() == 32);
            const auto& bmp = generated.AsBitmap<gfx::RGBA>();
            TEST_REQUIRE(CountPixels(bmp, gfx::Color::Blue) == 100*100);
            TEST_REQUIRE(CountPixels(bmp, gfx::Color::Red) == 200*100);
        }

        // RGB
        {
            gfx::Image generated;
            TEST_REQUIRE(generated.Load("TestPackage/textures/Generated_1.png"));
            TEST_REQUIRE(generated.GetWidth() == 1024);
            TEST_REQUIRE(generated.GetHeight() == 1024);
            TEST_REQUIRE(generated.GetDepthBits() == 24);
            const auto& bmp = generated.AsBitmap<gfx::RGB>();
            TEST_REQUIRE(CountPixels(bmp, gfx::Color::Yellow) == 64*64);
            TEST_REQUIRE(CountPixels(bmp, gfx::Color::Red) == 80*166);
        }

        // Alpha
        {
            gfx::Image generated;
            TEST_REQUIRE(generated.Load("TestPackage/textures/Generated_2.png"));
            TEST_REQUIRE(generated.GetWidth() == 1024);
            TEST_REQUIRE(generated.GetHeight() == 1024);
            TEST_REQUIRE(generated.GetDepthBits() == 8);
            const auto& bmp = generated.AsBitmap<gfx::Grayscale>();
            TEST_REQUIRE(CountPixels(bmp, 0x20) == 64*64);
            TEST_REQUIRE(CountPixels(bmp, 0x45) == 50*180);
        }
    }
}

void unit_test_packing_texture_composition_rects(unsigned padding)
{
    // generate a test texture.
    gfx::RgbBitmap bitmap[2];
    bitmap[0].Resize(64, 64);
    bitmap[0].Fill(gfx::URect(0, 0, 32, 32), gfx::Color::Green);
    bitmap[0].Fill(gfx::URect(32, 0, 32, 32), gfx::Color::Red);
    bitmap[0].Fill(gfx::URect(0, 32, 32, 32), gfx::Color::Blue);
    bitmap[0].Fill(gfx::URect(32, 32, 32, 32), gfx::Color::Yellow);

    bitmap[1].Resize(32, 32);
    bitmap[1].Fill(gfx::Color::HotPink);
    gfx::WritePNG(bitmap[0], "test_bitmap0.png");
    gfx::WritePNG(bitmap[1], "test_bitmap1.png");

    // texture rect covers the whole texture, no texture combination.
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");
        MakeDir("TestWorkspace");

        app::Workspace workspace("TestWorkspace");

        gfx::TextureMap2DClass material;
        material.SetTexture(gfx::LoadTextureFromFile("test_bitmap0.png"));
        material.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
        app::MaterialResource resource(material, "material");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory = "TestPackage";
        options.package_name = "";
        options.write_content_file = true;
        options.write_config_file = true;
        options.combine_textures = false;
        options.resize_textures = false;
        options.max_texture_width = 1024;
        options.max_texture_height = 1024;
        options.texture_padding = padding;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

        auto loader = engine::JsonFileClassLoader::Create();
        loader->LoadClassesFromFile("TestPackage/content.json");
        auto mat = loader->FindMaterialClassById(material.GetId());
        const auto& rect = mat->AsTexture()->GetTextureRect();
        TEST_REQUIRE(rect == gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    }

    // sub rectangle, no texture combination
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");
        MakeDir("TestWorkspace");

        app::Workspace workspace("TestWorkspace");

        gfx::TextureMap2DClass material;
        material.SetTexture(gfx::LoadTextureFromFile("test_bitmap0.png"));
        material.SetTextureRect(gfx::FRect(0.0f, 0.0f, 0.5f, 0.5f));
        app::MaterialResource resource(material, "material");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory = "TestPackage";
        options.package_name = "";
        options.write_content_file = true;
        options.write_config_file = true;
        options.combine_textures = false;
        options.resize_textures = false;
        options.max_texture_width = 1024;
        options.max_texture_height = 1024;
        options.texture_padding = padding;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

        auto loader = engine::JsonFileClassLoader::Create();
        loader->LoadClassesFromFile("TestPackage/content.json");
        auto mat = loader->FindMaterialClassById(material.GetId());
        const auto& rect = mat->AsTexture()->GetTextureRect();
        TEST_REQUIRE(rect == gfx::FRect(0.0f, 0.0f, 0.5f, 0.5f));
    }

    // texture rectangles with texture packing.
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");
        MakeDir("TestWorkspace");

        app::Workspace workspace("TestWorkspace");

        gfx::SpriteClass material;
        material.AddTexture(gfx::LoadTextureFromFile("test_bitmap0.png"));
        material.AddTexture(gfx::LoadTextureFromFile("test_bitmap1.png"));
        const auto src_rect0 = gfx::FRect(0.5f, 0.5f, 0.5f, 0.5f);
        const auto src_rect1 = gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f);
        material.SetTextureRect(0, src_rect0);
        material.SetTextureRect(1, src_rect1);
        app::MaterialResource resource(material, "material");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory          = "TestPackage";
        options.package_name       = "";
        options.write_content_file = true;
        options.write_config_file  = true;
        options.combine_textures   = true;
        options.resize_textures    = false;
        options.max_texture_width  = 1024;
        options.max_texture_height = 1024;
        options.texture_padding    = padding;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

        gfx::Image img;
        TEST_REQUIRE(img.Load("TestPackage/textures/Generated_0.png"));
        const auto& bmp = img.AsBitmap<gfx::RGB>();
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::HotPink) == (32+2*padding) * (32+2*padding));
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Green)   >= 32*32);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Red)     >= 32*32);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Blue)    >= 32*32);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Yellow)  >= 32*32);

        auto loader = engine::JsonFileClassLoader::Create();
        loader->LoadClassesFromFile("TestPackage/content.json");
        const auto& mat = loader->FindMaterialClassById(material.GetId());
        const auto& rect0 = mat->AsSprite()->GetTextureRect(0);
        const auto& rect1 = mat->AsSprite()->GetTextureRect(1);
        const auto src_fixed_rect0 = src_rect0.Expand(bitmap[0].GetSize());
        const auto src_fixed_rect1 = src_rect1.Expand(bitmap[1].GetSize());
        const auto dst_fixed_rect0 = rect0.Expand(bmp.GetSize());
        const auto dst_fixed_rect1 = rect1.Expand(bmp.GetSize());
        TEST_REQUIRE(gfx::Compare(bitmap[0].Copy(src_fixed_rect0), bmp.Copy(dst_fixed_rect0)));
        TEST_REQUIRE(gfx::Compare(bitmap[1].Copy(src_fixed_rect1), bmp.Copy(dst_fixed_rect1)));
    }
}


void unit_test_packing_texture_name_collision()
{
    DeleteDir("TestWorkspace");
    DeleteDir("TestPackage");
    MakeDir("TestWorkspace");

    app::Workspace workspace("TestWorkspace");

    // when there are multiple source textures with the same name
    // such as ws://textures/foo/1.png and ws://textures/bar/1.png
    // they output names must be resolved to different names.
    gfx::RgbBitmap bitmap[2];
    bitmap[0].Resize(64, 64);
    bitmap[0].Fill(gfx::Color::Green);
    bitmap[1].Resize(32, 32);
    bitmap[1].Fill(gfx::Color::HotPink);

    MakeDir("TestWorkspace/textures/foo");
    MakeDir("TestWorkspace/textures/bar");
    gfx::WritePNG(bitmap[0], "TestWorkspace/textures/foo/bitmap.png");
    gfx::WritePNG(bitmap[1], "TestWorkspace/textures/bar/bitmap.png");

    // setup 2 materials
    gfx::TextureMap2DClass materials[2];
    materials[0].SetTexture(gfx::LoadTextureFromFile("ws://textures/foo/bitmap.png"));
    materials[1].SetTexture(gfx::LoadTextureFromFile("ws://textures/bar/bitmap.png"));
    workspace.SaveResource(app::MaterialResource(materials[0], "material0"));
    workspace.SaveResource(app::MaterialResource(materials[1], "material0"));

    app::Workspace::ContentPackingOptions options;
    options.directory = "TestPackage";
    options.package_name = "";
    options.write_content_file = true;
    options.write_config_file = true;
    options.combine_textures = false;
    options.resize_textures = false;
    options.max_texture_width = 1024;
    options.max_texture_height = 1024;
    options.texture_padding = 0;
    // select the resources.
    std::vector<const app::Resource *> resources;
    resources.push_back(&workspace.GetUserDefinedResource(0));
    resources.push_back(&workspace.GetUserDefinedResource(1));
    TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

    // verify output
    auto cloader = engine::JsonFileClassLoader::Create();
    auto floader = engine::FileResourceLoader::Create();
    cloader->LoadClassesFromFile("TestPackage/content.json");
    floader->SetContentPath("TestPackage");
    gfx::SetResourceLoader(floader.get());

    {
        const auto& mat = cloader->FindMaterialClassById(materials[0].GetId());
        const auto& source = mat->AsTexture()->GetTextureSource();
        const auto* file = static_cast<const gfx::detail::TextureFileSource*>(source);
        gfx::Image img;
        TEST_REQUIRE(img.Load(file->GetFilename()));
        const auto& bmp = img.AsBitmap<gfx::RGB>();
        TEST_REQUIRE(bmp == bitmap[0]);
    }

    {
        const auto& mat = cloader->FindMaterialClassById(materials[1].GetId());
        const auto& source = mat->AsTexture()->GetTextureSource();
        const auto* file = static_cast<const gfx::detail::TextureFileSource*>(source);
        gfx::Image img;
        TEST_REQUIRE(img.Load(file->GetFilename()));
        const auto& bmp = img.AsBitmap<gfx::RGB>();
        TEST_REQUIRE(bmp == bitmap[1]);
    }
    gfx::SetResourceLoader(nullptr);
}

void unit_test_packing_ui_style_resources()
{
    DeleteDir("TestWorkspace");
    DeleteDir("TestPackage");
    QDir d;

    // setup dummy UI style file
    QString style(
            R"(
{
  "properties": [
     {
       "key": "widget/border-width",
       "value": 1.0
     },
     {
       "key": "widget/text-font",
       "value": "ws://fonts/style_font.otf"
     }
   ],

  "materials": [
     {
       "key": "widget/background",
       "type": "Null"
     }
  ]
}
)");
    TEST_REQUIRE(d.mkpath("ui"));
    TEST_REQUIRE(d.mkpath("fonts"));
    TEST_REQUIRE(app::WriteTextFile("fonts/widget_font.otf", "widget_font.otf"));
    TEST_REQUIRE(app::WriteTextFile("fonts/window_font.otf", "window_font.otf"));
    TEST_REQUIRE(app::WriteTextFile("ui/style.json", style));

    MakeDir("TestWorkspace");
    app::Workspace workspace("TestWorkspace");

    // setup dummy font files
    TEST_REQUIRE(d.mkpath("TestWorkspace/fonts"));
    TEST_REQUIRE(app::WriteTextFile("TestWorkspace/fonts/style_font.otf", "style_font.otf"));
    // set project settings.
    app::Workspace::ProjectSettings settings;
    workspace.SetProjectSettings(settings);

    // setup a UI window with widget(s)
    {
        uik::Label label;

        engine::UIStyle style;
        style.SetProperty(label.GetId() + "/text-font", workspace.MapFileToWorkspace(std::string("fonts/widget_font.otf")));
        style.SetProperty("window/radiobutton/text-font", workspace.MapFileToWorkspace(std::string("fonts/window_font.otf")));
        label.SetStyleString(style.MakeStyleString(label.GetId()));

        uik::Window window;
        window.SetStyleName(workspace.MapFileToWorkspace(std::string("ui/style.json")));
        window.SetStyleString(style.MakeStyleString("window"));
        window.AddWidget(std::move(label));
        app::UIResource ui_resource(window, "UI");
        workspace.SaveResource(ui_resource);
    }

    app::Workspace::ContentPackingOptions options;
    options.directory    = "TestPackage";
    options.package_name = "test";
    options.write_content_file = true;
    options.write_config_file  = true;
    options.combine_textures   = false;
    options.resize_textures    = false;

    // select the resources.
    std::vector<const app::Resource*> resources;
    resources.push_back(&workspace.GetUserDefinedResource(0));
    TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

    // UI style files should be copied into ui/
    // the UI style is rewritten when the resource references are re-mapped and thus
    // may not be the exact same string as what was originally written.
    const auto& style_string = app::ReadTextFile("TestPackage/test/ui/style.json");
    TEST_REQUIRE(style_string.contains("properties"));
    TEST_REQUIRE(style_string.contains("materials"));
    TEST_REQUIRE(style_string.contains("pck://fonts/style_font.otf"));
    TEST_REQUIRE(!style_string.contains("ws://fonts/style_font.otf"));
    // UI Font files should be copied into fonts/
    TEST_REQUIRE(app::ReadTextFile("TestPackage/test/fonts/widget_font.otf") == "widget_font.otf");
    TEST_REQUIRE(app::ReadTextFile("TestPackage/test/fonts/window_font.otf") == "window_font.otf");
    TEST_REQUIRE(app::ReadTextFile("TestPackage/test/fonts/style_font.otf") == "style_font.otf");
}

// bug that happens when a texture is resampled and written out
// but the output name collides with another texture name.
void unit_test_packing_texture_name_collision_resample_bug()
{
    DeleteDir("TestWorkspace");
    DeleteDir("TestPackage");
    MakeDir("TestWorkspace");

    app::Workspace workspace("TestWorkspace");

    // when there are multiple source textures with the same name
    // such as ws://textures/foo/1.png and ws://textures/bar/1.png
    // they output names must be resolved to different names.
    gfx::RgbBitmap bitmap[2];
    bitmap[0].Resize(128, 128);
    bitmap[0].Fill(gfx::Color::Green);
    bitmap[1].Resize(32, 32);
    bitmap[1].Fill(gfx::Color::HotPink);

    MakeDir("TestWorkspace/textures/foo");
    MakeDir("TestWorkspace/textures/bar");
    gfx::WritePNG(bitmap[0], "TestWorkspace/textures/foo/bitmap.png");
    gfx::WritePNG(bitmap[1], "TestWorkspace/textures/bar/bitmap.png");

    // setup 2 materials
    gfx::TextureMap2DClass materials[2];
    materials[0].SetTexture(gfx::LoadTextureFromFile("ws://textures/foo/bitmap.png"));
    materials[1].SetTexture(gfx::LoadTextureFromFile("ws://textures/bar/bitmap.png"));
    workspace.SaveResource(app::MaterialResource(materials[0], "material0"));
    workspace.SaveResource(app::MaterialResource(materials[1], "material0"));

    app::Workspace::ContentPackingOptions options;
    options.directory          = "TestPackage";
    options.package_name       = "";
    options.write_content_file = true;
    options.write_config_file  = true;
    options.combine_textures   = false;
    options.resize_textures    = true; // must be on for the bug to happen
    options.max_texture_width  = 64;
    options.max_texture_height = 64;
    options.texture_padding    = 0;
    // select the resources.
    std::vector<const app::Resource *> resources;
    resources.push_back(&workspace.GetUserDefinedResource(0));
    resources.push_back(&workspace.GetUserDefinedResource(1));
    TEST_REQUIRE(workspace.BuildReleasePackage(resources, options));

    // verify output
    auto cloader = engine::JsonFileClassLoader::Create();
    auto floader = engine::FileResourceLoader::Create();
    cloader->LoadClassesFromFile("TestPackage/content.json");
    floader->SetContentPath("TestPackage");
    gfx::SetResourceLoader(floader.get());

    {
        const auto& mat = cloader->FindMaterialClassById(materials[0].GetId());
        const auto& source = mat->AsTexture()->GetTextureSource();
        const auto* file = static_cast<const gfx::detail::TextureFileSource*>(source);
        gfx::Image img;
        TEST_REQUIRE(img.Load(file->GetFilename()));
        const auto& bmp = img.AsBitmap<gfx::RGB>();

        // the output texture has been resampled.
        gfx::RgbBitmap expected;
        expected.Resize(64, 64);
        expected.Fill(gfx::Color::Green);
        TEST_REQUIRE(bmp == expected);
    }

    {
        const auto& mat = cloader->FindMaterialClassById(materials[1].GetId());
        const auto& source = mat->AsTexture()->GetTextureSource();
        const auto* file = static_cast<const gfx::detail::TextureFileSource*>(source);
        gfx::Image img;
        TEST_REQUIRE(img.Load(file->GetFilename()));
        const auto& bmp = img.AsBitmap<gfx::RGB>();
        TEST_REQUIRE(bmp == bitmap[1]);
    }
    gfx::SetResourceLoader(nullptr);
}

void unit_test_json_export_import()
{
    DeleteDir("TestWorkspace");
    MakeDir("TestWorkspace"); // initially empty workspace folder.

    {
        app::Workspace workspace("TestWorkspace");
        // add some user defined content.
        gfx::ColorClass material;
        material.SetId("foo123");
        app::MaterialResource resource(material, "TestMaterial");
        resource.SetProperty("int", 123);
        resource.SetProperty("str", QString("hello"));
        workspace.SaveResource(resource);
        workspace.ExportResourceJson(std::vector<size_t>{0}, "test_export_import_content.json");
    }

    {
        std::vector<std::unique_ptr<app::Resource>> resources;
        TEST_REQUIRE(app::Workspace::ImportResourcesFromJson("test_export_import_content.json", resources));
        TEST_REQUIRE(resources.size() == 1);
        TEST_REQUIRE(resources[0]->GetId() == "foo123");
        TEST_REQUIRE(resources[0]->GetProperty("int", 0) == 123);
        TEST_REQUIRE(resources[0]->GetProperty("str", QString()) == "hello");
    }
}

app::ResourceListItem* FindResourceItem(const QString& name, app::ResourceList& list)
{
    for (auto& item : list)
    {
        if (item.name == name)
            return &item;
    }
    return nullptr;
}

void unit_test_list_deps()
{
    // material depends on nothing
    // polygon depends on material (for display only)
    // particle engine depends on material (for display only)
    // entity depends on script, drawable (polygon/particle), material
    // scene depends on script, entity
    // tilemap depends on material, data
    // audio depends on nothing
    // script depends on nothing

    app::Workspace workspace("TestWorkspace");

    {
        gfx::ColorClass material;
        app::MaterialResource material_resource(material, "mat1");
        workspace.SaveResource(material_resource);
    }

    // this is a red-herring and not actually used!
    {
        gfx::ColorClass material;
        app::MaterialResource material_resource(material, "mat2");
        workspace.SaveResource(material_resource);
    }

    {
        gfx::PolygonClass poly;
        app::CustomShapeResource shape_resource(poly, "poly");
        workspace.SaveResource(shape_resource);
    }

    {
        gfx::KinematicsParticleEngineClass particles;
        app::ParticleSystemResource  particle_resource(particles, "particles");
        workspace.SaveResource(particle_resource);
    }

    {
        app::Script script;
        app::ScriptResource script_resource(script, "EntityScript");
        workspace.SaveResource(script_resource);

        game::EntityClass entity;
        entity.SetName("entity");
        entity.SetSriptFileId(script_resource.GetIdUtf8());

        {
            const auto* material = workspace.FindResourceByName("mat1", app::Resource::Type::Material);
            const auto* drawable = workspace.FindResourceByName("poly", app::Resource::Type::Shape);

            game::DrawableItemClass draw;
            draw.SetMaterialId(material->GetIdUtf8());
            draw.SetDrawableId(drawable->GetIdUtf8());
            game::EntityNodeClass node;
            node.SetName("node1");
            node.SetDrawable(draw);
            entity.AddNode(node);
        }
        {
            const auto* material = workspace.FindResourceByName("Red", app::Resource::Type::Material);
            const auto* drawable = workspace.FindResourceByName("particles", app::Resource::Type::ParticleSystem);
            game::DrawableItemClass draw;
            draw.SetMaterialId(material->GetIdUtf8());
            draw.SetDrawableId(drawable->GetIdUtf8());
            game::EntityNodeClass node;
            node.SetName("node2");
            node.SetDrawable(draw);
            entity.AddNode(node);
        }

        app::EntityResource resource(entity, "entity");
        workspace.SaveResource(resource);
    }

    {
        game::TilemapClass map;
        map.SetName("map");

        const auto* material = workspace.FindResourceByName("mat1", app::Resource::Type::Material);

        game::TilemapLayerClass layer;
        layer.SetName("layer");
        layer.SetType(game::TilemapLayerClass::Type::Render);
        layer.SetPaletteMaterialId(material->GetIdUtf8(), 0);
        map.AddLayer(layer);

        app::TilemapResource resource(map, "map");
        workspace.SaveResource(resource);
    }

    {
        const auto* map = workspace.FindResourceByName("map", app::Resource::Type::Tilemap);

        app::Script script;
        app::ScriptResource script_resource(script, "SceneScript");
        workspace.SaveResource(script_resource);

        game::SceneClass scene;
        scene.SetName("scene");
        scene.SetScriptFileId(script_resource.GetIdUtf8());
        scene.SetTilemapId(map->GetIdUtf8());

        const auto* entity = workspace.FindResourceByName("entity", app::Resource::Type::Entity);

        {
            game::SceneNodeClass node;
            node.SetName("node");
            node.SetEntityId(entity->GetIdUtf8());
            scene.AddNode(node);
        }
        app::SceneResource  resource(scene, "scene");
        workspace.SaveResource(resource);
    }

    {
        TEST_REQUIRE(workspace.GetUserDefinedResource(0).GetName() == "mat1");
        TEST_REQUIRE(workspace.GetUserDefinedResource(1).GetName() == "mat2");
        TEST_REQUIRE(workspace.GetUserDefinedResource(2).GetName() == "poly");
        TEST_REQUIRE(workspace.GetUserDefinedResource(3).GetName() == "particles");
        TEST_REQUIRE(workspace.GetUserDefinedResource(4).GetName() == "EntityScript");
        TEST_REQUIRE(workspace.GetUserDefinedResource(5).GetName() == "entity");
        TEST_REQUIRE(workspace.GetUserDefinedResource(6).GetName() == "map");
        TEST_REQUIRE(workspace.GetUserDefinedResource(7).GetName() == "SceneScript");
        TEST_REQUIRE(workspace.GetUserDefinedResource(8).GetName() == "scene");

        // material
        {
            auto list = workspace.ListDependencies(0);
            TEST_REQUIRE(list.empty());
        }
        // entity
        {
            auto list = workspace.ListDependencies(5);
            TEST_REQUIRE(list.size() == 4);
            TEST_REQUIRE(FindResourceItem("mat1", list));
            TEST_REQUIRE(FindResourceItem("particles", list));
            TEST_REQUIRE(FindResourceItem("poly", list));
            TEST_REQUIRE(FindResourceItem("EntityScript", list));
            // red herring
            TEST_REQUIRE(FindResourceItem("mat2", list) == nullptr);
        }
        // tilemap
        {
            auto list = workspace.ListDependencies(6);
            TEST_REQUIRE(list.size() == 1);
            TEST_REQUIRE(FindResourceItem("mat1", list));
        }
        // scene
        {
            auto list = workspace.ListDependencies(8);
            TEST_REQUIRE(list.size() == 7);
            TEST_REQUIRE(FindResourceItem("mat1", list));
            TEST_REQUIRE(FindResourceItem("particles", list));
            TEST_REQUIRE(FindResourceItem("poly", list));
            TEST_REQUIRE(FindResourceItem("EntityScript", list));
            TEST_REQUIRE(FindResourceItem("SceneScript", list));
            TEST_REQUIRE(FindResourceItem("entity", list));
            TEST_REQUIRE(FindResourceItem("map", list));
        }

        // scene + entity
        {
            auto list = workspace.ListDependencies(std::vector<size_t>{5, 8});
            TEST_REQUIRE(list.size() == 7);
            TEST_REQUIRE(FindResourceItem("mat1", list));
            TEST_REQUIRE(FindResourceItem("particles", list));
            TEST_REQUIRE(FindResourceItem("poly", list));
            TEST_REQUIRE(FindResourceItem("EntityScript", list));
            TEST_REQUIRE(FindResourceItem("SceneScript", list));
            TEST_REQUIRE(FindResourceItem("entity", list));
            TEST_REQUIRE(FindResourceItem("map", list));
        }

        // tilemap + entity
        {
            auto list = workspace.ListDependencies(std::vector<size_t>{5, 6});
            TEST_REQUIRE(list.size() == 4);
            TEST_REQUIRE(FindResourceItem("mat1", list));
            TEST_REQUIRE(FindResourceItem("particles", list));
            TEST_REQUIRE(FindResourceItem("poly", list));
            TEST_REQUIRE(FindResourceItem("EntityScript", list));
        }
    }
}

void unit_test_export_import_basic()
{
    {
        DeleteDir("TestWorkspace");

        QDir d;
        // setup dummy shaders and data.
        TEST_REQUIRE(d.mkpath("TestWorkspace"));
        TEST_REQUIRE(d.mkpath("TestWorkspace/shaders/es2"));
        TEST_REQUIRE(d.mkpath("TestWorkspace/lua"));
        TEST_REQUIRE(d.mkpath("TestWorkspace/audio"));
        TEST_REQUIRE(d.mkpath("TestWorkspace/data"));
        TEST_REQUIRE(d.mkpath("TestWorkspace/fonts"));
        TEST_REQUIRE(d.mkpath("TestWorkspace/ui"));
        TEST_REQUIRE(d.mkpath("TestWorkspace/textures"));
        TEST_REQUIRE(d.mkpath("TestWorkspace/textures/foobar"));
        TEST_REQUIRE(app::WriteTextFile("TestWorkspace/shaders/es2/my_material.glsl", "my_material.glsl"));
        TEST_REQUIRE(app::WriteTextFile("TestWorkspace/lua/game_script.lua", "game_script.lua"));
        TEST_REQUIRE(app::WriteTextFile("TestWorkspace/audio/music.mp3", "music.mp3"));
        TEST_REQUIRE(app::WriteTextFile("TestWorkspace/data/levels.txt", "levels.txt"));
        TEST_REQUIRE(app::WriteTextFile("TestWorkspace/fonts/font.otf", "font.otf"));
        // setup dummy UI style file
        QString style(
                R"(
{
  "properties": [
     {
       "key": "widget/border-width",
       "value": 1.0
     },
     {
       "key": "widget/text-font",
       "value": "ws://fonts/font.otf"
     }
   ],

  "materials": [
     {
       "key": "widget/background",
       "type": "Null"
     }
  ]
}
)");
        TEST_REQUIRE(app::WriteTextFile("TestWorkspace/ui/style.json", style));

        app::Workspace workspace("TestWorkspace");

        gfx::RgbBitmap bitmap;
        bitmap.Resize(128, 100);
        bitmap.Fill(gfx::Color::Yellow);
        gfx::WritePNG(bitmap, "TestWorkspace/textures/foobar/test_bitmap.png");

        // setup some content.
        gfx::detail::TextureFileSource texture_source;
        texture_source.SetFileName(workspace.MapFileToWorkspace(std::string("TestWorkspace/textures/foobar/test_bitmap.png")));
        texture_source.SetName("test-texture");

        gfx::TextureMap2D texture;
        texture.SetSamplerName("kTexture");
        texture.SetRectUniformName("kTextureRect");
        texture.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
        texture.SetTexture(texture_source.Copy());

        gfx::CustomMaterialClass material;
        material.SetTextureMap("texture", std::move(texture));
        material.SetShaderUri(workspace.MapFileToWorkspace(std::string("TestWorkspace/shaders/es2/my_material.glsl")));
        app::MaterialResource material_resource(material, "material");

        gfx::PolygonClass poly;
        app::CustomShapeResource shape_resource(poly, "poly");
        gfx::KinematicsParticleEngineClass particles;
        app::ParticleSystemResource particle_resource(particles, "particles");

        app::Script script;
        script.SetFileURI(workspace.MapFileToWorkspace(std::string("TestWorkspace/lua/game_script.lua")));
        app::ScriptResource script_resource(script, "GameScript");

        audio::GraphClass audio_graph("music_graph");
        audio::GraphClass::Element music_src;
        music_src.id = base::RandomString(10);
        music_src.name = "music";
        music_src.type = "FileSource";
        music_src.args["file"] = workspace.MapFileToWorkspace(std::string("TestWorkspace/audio/music.mp3"));
        audio_graph.AddElement(std::move(music_src));
        app::AudioResource audio_resource(audio_graph, "music.mp3");

        app::DataFile data;
        data.SetFileURI(workspace.MapFileToWorkspace(std::string("TestWorkspace/data/levels.txt")));
        app::DataResource data_resource(data, "levels.txt");

        uik::Window window;
        window.SetStyleName(workspace.MapFileToWorkspace(std::string("TestWorkspace/ui/style.json")));
        app::UIResource ui_resource(window, "UI");

        workspace.SaveResource(material_resource);
        workspace.SaveResource(shape_resource);
        workspace.SaveResource(particle_resource);
        workspace.SaveResource(script_resource);
        workspace.SaveResource(audio_resource);
        workspace.SaveResource(data_resource);
        workspace.SaveResource(ui_resource);

        app::Workspace::ExportOptions options;
        options.zip_file = "test-export.zip";

        // select the resources.
        std::vector<const app::Resource*> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        resources.push_back(&workspace.GetUserDefinedResource(1));
        resources.push_back(&workspace.GetUserDefinedResource(2));
        resources.push_back(&workspace.GetUserDefinedResource(3));
        resources.push_back(&workspace.GetUserDefinedResource(4));
        resources.push_back(&workspace.GetUserDefinedResource(5));
        resources.push_back(&workspace.GetUserDefinedResource(6));
        TEST_REQUIRE(workspace.ExportResourceArchive(resources, options));
    }

    {
        DeleteDir("TestWorkspace");
        MakeDir("TestWorkspace");

        app::Workspace workspace("TestWorkspace");

        app::ResourceArchive zip;
        zip.SetImportSubFolderName("test-export");
        TEST_REQUIRE(zip.Open("test-export.zip"));
        TEST_REQUIRE(workspace.ImportResourceArchive(zip));
        TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 7);
        TEST_REQUIRE(app::ReadTextFile("TestWorkspace/test-export/shaders/es2/my_material.glsl") == "my_material.glsl");
        TEST_REQUIRE(app::ReadTextFile("TestWorkspace/test-export/lua/game_script.lua") == "game_script.lua");
        TEST_REQUIRE(app::ReadTextFile("TestWorkspace/test-export/audio/music.mp3") == "music.mp3");
        TEST_REQUIRE(app::ReadTextFile("TestWorkspace/test-export/data/levels.txt") == "levels.txt");
        TEST_REQUIRE(app::ReadTextFile("TestWorkspace/test-export/fonts/font.otf") == "font.otf");
        const auto& style_string = app::ReadTextFile("TestWorkspace/test-export/ui/style.json");
        TEST_REQUIRE(!style_string.isEmpty());

        gfx::Image texture;
        TEST_REQUIRE(texture.Load("TestWorkspace/test-export/textures/test_bitmap.png"));
        const auto& bmp = texture.AsBitmap<gfx::RGB>();
        TEST_REQUIRE(bmp.GetWidth() == 128);
        TEST_REQUIRE(bmp.GetHeight() == 100);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Yellow) == 128 * 100);
    }
}

void unit_test_export_name_dupe()
{
    gfx::RgbBitmap bitmap[2];
    bitmap[0].Resize(64, 64);
    bitmap[0].Fill(gfx::Color::Green);
    bitmap[1].Resize(32, 32);
    bitmap[1].Fill(gfx::Color::HotPink);

    {
        DeleteDir("TestWorkspace");
        MakeDir("TestWorkspace");
        MakeDir("TestWorkspace/textures/foo");
        MakeDir("TestWorkspace/textures/bar");

        gfx::WritePNG(bitmap[0], "TestWorkspace/textures/foo/bitmap.png");
        gfx::WritePNG(bitmap[1], "TestWorkspace/textures/bar/bitmap.png");

        app::Workspace workspace("TestWorkspace");
        gfx::TextureMap2DClass materials[2];
        materials[0].SetTexture(gfx::LoadTextureFromFile("ws://textures/foo/bitmap.png"));
        materials[1].SetTexture(gfx::LoadTextureFromFile("ws://textures/bar/bitmap.png"));
        workspace.SaveResource(app::MaterialResource(materials[0], "material0"));
        workspace.SaveResource(app::MaterialResource(materials[1], "material1"));

        app::Workspace::ExportOptions options;
        options.zip_file = "test-export2.zip";
        // select the resources.
        std::vector<const app::Resource*> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        resources.push_back(&workspace.GetUserDefinedResource(1));
        TEST_REQUIRE(workspace.ExportResourceArchive(resources, options));
    }

    {
        DeleteDir("TestWorkspace");
        MakeDir("TestWorkspace");

        app::Workspace workspace("TestWorkspace");

        app::ResourceArchive zip;
        zip.SetImportSubFolderName("test-export");
        TEST_REQUIRE(zip.Open("test-export2.zip"));
        TEST_REQUIRE(workspace.ImportResourceArchive(zip));
        TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 2);

        {
            const auto* resource0 = workspace.FindResourceByName("material0", app::Resource::Type::Material);
            const auto* resource1 = workspace.FindResourceByName("material1", app::Resource::Type::Material);
            TEST_REQUIRE(resource0);
            TEST_REQUIRE(resource1);
            const gfx::MaterialClass* material0;
            const gfx::MaterialClass* material1;
            resource0->GetContent(&material0);
            resource1->GetContent(&material1);
            const auto* texture_map0 = material0->AsTexture();
            const auto* texture_map1 = material1->AsTexture();
            const auto* texture_map_source0 = texture_map0->GetTextureSource();
            const auto* texture_map_source1 = texture_map1->GetTextureSource();
            const auto* texture_map_file_source0 = dynamic_cast<const gfx::detail::TextureFileSource*>(texture_map_source0);
            const auto* texture_map_file_source1 = dynamic_cast<const gfx::detail::TextureFileSource*>(texture_map_source1);
            TEST_REQUIRE(texture_map_file_source0->GetFilename() != texture_map_file_source1->GetFilename());
            const auto& texture_file0 = workspace.MapFileToFilesystem(texture_map_file_source0->GetFilename());
            const auto& texture_file1 = workspace.MapFileToFilesystem(texture_map_file_source1->GetFilename());
            gfx::Image img0;
            gfx::Image img1;
            TEST_REQUIRE(img0.Load(app::ToUtf8(texture_file0)));
            TEST_REQUIRE(img1.Load(app::ToUtf8(texture_file1)));
            const auto& bmp0 = img0.AsBitmap<gfx::RGB>();
            const auto& bmp1 = img1.AsBitmap<gfx::RGB>();
            TEST_REQUIRE(bmp0 == bitmap[0]);
            TEST_REQUIRE(bmp1 == bitmap[1]);
        }
    }
}

void unit_test_duplicate_with_data()
{
    // check duplication of tilemap layer data.
    {
        DeleteDir("TestWorkspace");

        QDir d;
        TEST_REQUIRE(d.mkpath("TestWorkspace"));
        TEST_REQUIRE(d.mkpath("TestWorkspace/data"));

        app::Workspace workspace("TestWorkspace");

        game::TilemapLayerClass layer;
        layer.SetName("layer");
        layer.SetType(game::TilemapLayerClass::Type::DataUInt8);

        const auto& data_uri  = app::toString("ws://data/%1.bin", layer.GetId());
        const auto& data_file = workspace.MapFileToFilesystem(data_uri);
        TEST_REQUIRE(app::WriteTextFile(data_file, "dummy layer data"));

        app::DataFile datafile;
        datafile.SetFileURI(data_uri);
        datafile.SetTypeTag(app::DataFile::TypeTag::TilemapData);
        datafile.SetOwnerId(layer.GetId());

        layer.SetDataUri(app::ToUtf8(data_uri));
        layer.SetDataId(datafile.GetId());

        game::TilemapClass map;
        map.SetName("map");
        map.SetMapWidth(10);
        map.SetMapHeight(10);
        map.AddLayer(layer);

        app::DataResource data_resource(datafile, "layer data");
        workspace.SaveResource(data_resource);

        app::TilemapResource map_resource(map, "tilemap");
        workspace.SaveResource(map_resource);

        TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 2);
        TEST_REQUIRE(workspace.GetUserDefinedResource(0).GetName() == "layer data");
        TEST_REQUIRE(workspace.GetUserDefinedResource(1).GetName() == "tilemap");

        workspace.DuplicateResource(1);
        TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 4);

        {
            const auto& cpy_data_res = workspace.GetResourceByName("Copy of tilemap Layer Data", app::Resource::Type::DataFile);
            const auto& cpy_map_res  = workspace.GetResourceByName("Copy of tilemap", app::Resource::Type::Tilemap);

            const game::TilemapClass* cpy_map = nullptr;
            cpy_map_res.GetContent(&cpy_map);
            TEST_REQUIRE(cpy_map->GetNumLayers() == 1);
            const auto& cpy_layer = cpy_map->GetLayer(0);

            const app::DataFile* data = nullptr;
            cpy_data_res.GetContent(&data);
            TEST_REQUIRE(data->GetTypeTag() == app::DataFile::TypeTag::TilemapData);
            TEST_REQUIRE(data->GetFileURI() == base::FormatString("ws://data/%1.bin", cpy_layer.GetId()));
            TEST_REQUIRE(data->GetOwnerId() == cpy_layer.GetId());
            TEST_REQUIRE(data->GetFileURI() == cpy_layer.GetDataUri());
            TEST_REQUIRE(data->GetId()      == cpy_layer.GetDataId());
            const auto& cpy_file = workspace.MapFileToFilesystem(data->GetFileURI());
            TEST_REQUIRE(app::ReadTextFile(cpy_file) == "dummy layer data");
        }
    }
}

void unit_test_delete_with_data()
{
    // check deletion of tilemap layer data.
    {
        DeleteDir("TestWorkspace");

        QDir d;
        TEST_REQUIRE(d.mkpath("TestWorkspace"));
        TEST_REQUIRE(d.mkpath("TestWorkspace/data"));

        app::Workspace workspace("TestWorkspace");

        game::TilemapLayerClass layer;
        layer.SetName("layer");
        layer.SetType(game::TilemapLayerClass::Type::DataUInt8);

        const auto& data_uri = app::toString("ws://data/%1.bin", layer.GetId());
        const auto& data_file = workspace.MapFileToFilesystem(data_uri);
        TEST_REQUIRE(app::WriteTextFile(data_file, "dummy layer data"));
        TEST_REQUIRE(QFileInfo(data_file).exists() == true);

        app::DataFile datafile;
        datafile.SetFileURI(data_uri);
        datafile.SetTypeTag(app::DataFile::TypeTag::TilemapData);
        datafile.SetOwnerId(layer.GetId());

        layer.SetDataUri(app::ToUtf8(data_uri));
        layer.SetDataId(datafile.GetId());

        game::TilemapClass map;
        map.SetName("map");
        map.SetMapWidth(10);
        map.SetMapHeight(10);
        map.AddLayer(layer);

        app::DataResource data_resource(datafile, "layer data");
        workspace.SaveResource(data_resource);

        app::TilemapResource map_resource(map, "tilemap");
        workspace.SaveResource(map_resource);

        TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 2);
        TEST_REQUIRE(workspace.GetUserDefinedResource(0).GetName() == "layer data");
        TEST_REQUIRE(workspace.GetUserDefinedResource(1).GetName() == "tilemap");

        workspace.DeleteResource(1);
        TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 0);
        TEST_REQUIRE(QFileInfo(data_file).exists() == false);
    }

    // todo: script deletion
    {

    }
}

int test_main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    base::OStreamLogger logger(std::cout);
    base::SetGlobalLog(&logger);
    base::EnableDebugLog(true);
    app::EventLog::get().OnNewEvent = [](const app::Event& event) {
        std::cout << app::ToUtf8(event.message);
        std::cout << std::endl;
    };

    unit_test_path_mapping();
    unit_test_resource();
    unit_test_save_load();
    unit_test_packing_basic();
    unit_test_packing_texture_composition(0);
    unit_test_packing_texture_composition(3);
    unit_test_packing_texture_composition_format();
    unit_test_packing_texture_composition_rects(0);
    unit_test_packing_texture_composition_rects(5);
    unit_test_packing_texture_name_collision();
    unit_test_packing_ui_style_resources();
    unit_test_packing_texture_name_collision_resample_bug();
    unit_test_json_export_import();
    unit_test_list_deps();
    unit_test_export_import_basic();
    unit_test_export_name_dupe();

    unit_test_duplicate_with_data();
    unit_test_delete_with_data();
    return 0;
}
