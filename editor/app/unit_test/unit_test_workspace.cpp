
#include "config.h"

#include "warnpush.h"
#  include <QCoreApplication>
#  include <QApplication>
#  include <QDir>
#include "warnpop.h"

#include <chrono>
#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/logging.h"
#include "base/utility.h"
#include "editor/app/resource.h"
#include "editor/app/workspace.h"
#include "editor/app/eventlog.h"
#include "gamelib/loader.h"
#include "graphics/types.h"

void DeleteDir(const QString& dir)
{
    QDir d(dir);
    d.removeRecursively();
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

bool operator==(const gfx::FRect& lhs, const gfx::FRect& rhs)
{
    return real::equals(lhs.GetX(), rhs.GetX()) &&
           real::equals(lhs.GetY(), rhs.GetY()) &&
           real::equals(lhs.GetWidth(), rhs.GetWidth()) &&
           real::equals(lhs.GetHeight(), rhs.GetHeight());
}

void unit_test_path_mapping()
{
    DeleteDir("TestWorkspace");

    const auto& cwd = QDir::currentPath();
    const auto& app = QCoreApplication::applicationDirPath();

    app::Workspace workspace;
    workspace.MakeWorkspace(app::JoinPath(cwd, "TestWorkspace"));

#if defined(WINDOWS_OS)
    TEST_REQUIRE(app::CleanPath("c:/foo/bar.png") == "c:\\foo\\bar.png");
    TEST_REQUIRE(app::CleanPath("c:\\foo\\bar.png") == "c:\\foo\\bar.png");
    TEST_REQUIRE(app::CleanPath("foo/bar/image.png") == "foo\\bar\\image.png");
#endif

    // test mapping of paths.
    // paths relative to the workspace are expressed using ws:// reference.
    // paths relative to the application are expressed using app:// reference
    // other paths are expressed using fs:// reference
    TEST_REQUIRE(workspace.AddFileToWorkspace(app::JoinPath(cwd, "TestWorkspace/relative/path/file.png")) == "ws://relative/path/file.png");
    TEST_REQUIRE(workspace.AddFileToWorkspace(app::JoinPath(app, "relative/path/file.png")) == "app://relative/path/file.png");

    TEST_REQUIRE(workspace.AddFileToWorkspace(app::JoinPath(cwd, "TestWorkspace\\relative\\path\\file.png")) == "ws://relative/path/file.png");
    TEST_REQUIRE(workspace.AddFileToWorkspace(app::JoinPath(app, "relative\\path\\file.png")) == "app://relative/path/file.png");
#if defined(POSIX_OS)
    TEST_REQUIRE(workspace.AddFileToWorkspace(QString("/tmp/file.png")) == "fs:///tmp/file.png");
    TEST_REQUIRE(workspace.MapFileToFilesystem("fs:///tmp/file.png") == "/tmp/file.png");
    TEST_REQUIRE(workspace.AddFileToWorkspace(QString("some/file/name.png")) == "fs://some/file/name.png");
    TEST_REQUIRE(workspace.MapFileToFilesystem("fs://some/file/name.png") == "some/file/name.png");
#elif defined(WINDOWS_OS)
    TEST_REQUIRE(workspace.AddFileToWorkspace(QString("c:\\tmp\\file.png")) == "fs://c:\\tmp\\file.png");
    TEST_REQUIRE(workspace.MapFileToFilesystem("fs://c:\\tmp\\file.png") == "c:\\tmp\\file.png");
    TEST_REQUIRE(workspace.AddFileToWorkspace(QString("some\\file\\name.png")) == "fs://some\\file\\name.png");
    TEST_REQUIRE(workspace.MapFileToFilesystem("fs://some\\file\\name.png") == "some\\file\\name.png");
#endif
    TEST_REQUIRE(workspace.MapFileToFilesystem("ws://relative/path/file.png") == app::JoinPath(cwd, "TestWorkspace/relative/path/file.png"));
    TEST_REQUIRE(workspace.MapFileToFilesystem("app://relative/path/file.png") == app::JoinPath(app, "relative/path/file.png"));

    // don't re-encode already encoded file names.
    TEST_REQUIRE(workspace.AddFileToWorkspace(QString("ws://relative/path/file.png")) == "ws://relative/path/file.png");
    TEST_REQUIRE(workspace.AddFileToWorkspace(QString("app://foo/bar/file.png")) == "app://foo/bar/file.png");
    TEST_REQUIRE(workspace.AddFileToWorkspace(QString("fs:///tmp/file.png")) == "fs:///tmp/file.png");
}

void unit_test_resource()
{
    DeleteDir("TestWorkspace");

    app::Workspace workspace;
    workspace.MakeWorkspace("TestWorkspace");
    TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 0);
    TEST_REQUIRE(workspace.GetNumResources() != 0);
    TEST_REQUIRE(workspace.GetNumPrimitiveResources() != 0);

    const auto primitives      = workspace.GetNumPrimitiveResources();
    const auto first_primitive = workspace.GetPrimitiveResource(0).Copy();
    const auto last_primitive  = workspace.GetPrimitiveResource(primitives-1).Copy();

    gfx::MaterialClass material;
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

    app::Workspace workspace;
    TEST_REQUIRE(workspace.LoadWorkspace("TestWorkspace") == false);
    TEST_REQUIRE(workspace.GetName().isEmpty());
    TEST_REQUIRE(workspace.GetDir().isEmpty());
    TEST_REQUIRE(workspace.IsOpen() == false);
    TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 0);

    TEST_REQUIRE(workspace.MakeWorkspace("TestWorkspace"));
    TEST_REQUIRE(workspace.IsOpen());
    TEST_REQUIRE(workspace.GetDir().contains("TestWorkspace"));
    TEST_REQUIRE(workspace.GetName().isEmpty() == false);

    // add some user defined content.
    gfx::MaterialClass material;
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
    settings.application_library = "library";
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

    workspace.CloseWorkspace();
    TEST_REQUIRE(workspace.GetName().isEmpty());
    TEST_REQUIRE(workspace.GetDir().isEmpty());
    TEST_REQUIRE(workspace.IsOpen() == false);
    TEST_REQUIRE(workspace.GetNumUserDefinedResources() == 0);
    TEST_REQUIRE(workspace.HasUserProperty("user-int") == false);
    TEST_REQUIRE(workspace.HasUserProperty("user-str") == false);
    TEST_REQUIRE(workspace.HasProperty("int") == false);
    TEST_REQUIRE(workspace.HasProperty("str") == false);

    TEST_REQUIRE(workspace.LoadWorkspace("TestWorkspace"));
    TEST_REQUIRE(workspace.IsOpen());
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
        const auto &res = workspace.GetUserDefinedResource(0);
        TEST_REQUIRE(res.GetName() == "TestMaterial");
        TEST_REQUIRE(res.GetIdUtf8() == material.GetId());
        TEST_REQUIRE(res.GetProperty("int", 0) == 123);
        TEST_REQUIRE(res.GetProperty("str", QString("")) == QString("hello"));
        TEST_REQUIRE(res.GetUserProperty("foo", 0) == 444);
        TEST_REQUIRE(res.GetUserProperty("bar", 0) == 777);
    }
    TEST_REQUIRE(workspace.GetProjectSettings().multisample_sample_count == 16);
    TEST_REQUIRE(workspace.GetProjectSettings().application_name == "foobar");
    TEST_REQUIRE(workspace.GetProjectSettings().application_version == "1.1.1");
    TEST_REQUIRE(workspace.GetProjectSettings().application_library == "library");
    TEST_REQUIRE(workspace.GetProjectSettings().default_min_filter == gfx::Device::MinFilter::Mipmap);
    TEST_REQUIRE(workspace.GetProjectSettings().default_mag_filter == gfx::Device::MagFilter::Linear);
    TEST_REQUIRE(workspace.GetProjectSettings().window_mode == app::Workspace::ProjectSettings::WindowMode::Fullscreen);
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

void unit_test_packing_basic()
{
    DeleteDir("TestWorkspace");
    DeleteDir("TestPackage");

    QDir d;
    // setup dummy shaders.
    TEST_REQUIRE(d.mkpath("shaders/es2"));
    TEST_REQUIRE(app::WriteTextFile("shaders/es2/solid_color.glsl", "solid_color.glsl"));
    TEST_REQUIRE(app::WriteTextFile("shaders/es2/vertex_array.glsl", "vertex_array.glsl"));
    TEST_REQUIRE(app::WriteTextFile("shaders/es2/particles.glsl", "particles.glsl"));

    app::Workspace workspace;
    workspace.MakeWorkspace("TestWorkspace");
    // set project settings.
    app::Workspace::ProjectSettings settings;
    settings.multisample_sample_count = 16;
    settings.application_name = "foobar";
    settings.application_version = "1.1.1";
    settings.application_library = "library";
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
    gfx::MaterialClass material;
    app::MaterialResource material_resource(material, "material");
    gfx::PolygonClass poly;
    app::CustomShapeResource shape_resource(poly, "poly");
    gfx::KinematicsParticleEngineClass particles;
    app::ParticleSystemResource  particle_resource(particles, "particles");

    workspace.SaveResource(material_resource);
    workspace.SaveResource(shape_resource);
    workspace.SaveResource(particle_resource);

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
    TEST_REQUIRE(workspace.PackContent(resources, options));

    // in the output folder we should have content.json, config.json
    // and the shaders copied into shaders/es2/
    TEST_REQUIRE(app::ReadTextFile("TestPackage/test/shaders/es2/solid_color.glsl") == "solid_color.glsl");
    TEST_REQUIRE(app::ReadTextFile("TestPackage/test/shaders/es2/vertex_array.glsl") == "vertex_array.glsl");
    TEST_REQUIRE(app::ReadTextFile("TestPackage/test/shaders/es2/particles.glsl") == "particles.glsl");

    game::ContentLoader loader;
    loader.LoadFromFile("TestPackage/test", "TestPackage/test/content.json");
    TEST_REQUIRE(loader.GetMaterialClass(material.GetId()));
    TEST_REQUIRE(loader.GetDrawableClass(poly.GetId()));
    TEST_REQUIRE(loader.GetDrawableClass(particles.GetId()));

    auto [ok, json, error] = base::JsonParseFile("TestPackage/test/config.json");
    TEST_REQUIRE(ok);
    TEST_REQUIRE(json["config"]["sampling"] == "MSAA16");
    TEST_REQUIRE(json["window"]["can_resize"] == false);
    TEST_REQUIRE(json["window"]["has_border"] == false);
    TEST_REQUIRE(json["window"]["width"] == 600);
    TEST_REQUIRE(json["window"]["height"] == 400);
    TEST_REQUIRE(json["window"]["set_fullscreen"] == true);
    TEST_REQUIRE(json["window"]["vsync"] == true);
    TEST_REQUIRE(json["application"]["title"] == "foobar");
    TEST_REQUIRE(json["application"]["version"] == "1.1.1");
    TEST_REQUIRE(json["application"]["library"] == "library");
    TEST_REQUIRE(json["application"]["ticks_per_second"] == 100.0);
    TEST_REQUIRE(json["application"]["updates_per_second"] == 50.0);

    DeleteDir("TestPackage");
    options.write_config_file = false;
    options.write_content_file = false;
    workspace.PackContent(resources, options);
    TEST_REQUIRE(!base::FileExists("TestPackage/test/content.json"));
    TEST_REQUIRE(!base::FileExists("TestPackage/test/config.json"));
}

void unit_test_packing_texture_composition()
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

        gfx::MaterialClass material;
        material.AddTexture("test_bitmap0.png");
        app::MaterialResource resource(material, "material");

        app::Workspace workspace;
        workspace.MakeWorkspace("TestWorkspace");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory = "TestPackage";
        options.package_name = "";
        options.write_content_file = true;
        options.write_config_file = true;
        options.combine_textures = true;
        options.resize_textures = false;
        options.max_texture_width = 1024;
        options.max_texture_height = 1024;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.PackContent(resources, options));

        gfx::Image generated;
        TEST_REQUIRE(generated.Load("TestPackage/textures/test_bitmap0.png"));
        TEST_REQUIRE(gfx::Compare(bitmap[0], generated.AsBitmap<gfx::RGB>()));
    }

    // use 2 small textures. packing should be done.
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");

        gfx::MaterialClass material;
        material.AddTexture("test_bitmap0.png");
        material.AddTexture("test_bitmap1.png");
        app::MaterialResource resource(material, "material");

        app::Workspace workspace;
        workspace.MakeWorkspace("TestWorkspace");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory = "TestPackage";
        options.package_name = "";
        options.write_content_file = true;
        options.write_config_file = true;
        options.combine_textures = true;
        options.resize_textures = false;
        options.max_texture_width = 1024;
        options.max_texture_height = 1024;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.PackContent(resources, options));

        gfx::Image generated;
        TEST_REQUIRE(generated.Load("TestPackage/textures/Generated_0.png"));
        const auto& bmp = generated.AsBitmap<gfx::RGB>();
        TEST_REQUIRE((bmp.GetWidth() == 64 && bmp.GetHeight() == 128) ||
                          (bmp.GetWidth() == 128 && bmp.GetHeight() == 64));
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Blue) == 64*64);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Red) == 64*64);
    }

    // disable packing, should get 2 textures
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");

        gfx::MaterialClass material;
        material.AddTexture("test_bitmap0.png");
        material.AddTexture("test_bitmap1.png");
        app::MaterialResource resource(material, "material");

        app::Workspace workspace;
        workspace.MakeWorkspace("TestWorkspace");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory = "TestPackage";
        options.package_name = "";
        options.write_content_file = true;
        options.write_config_file  = true;
        options.combine_textures   = false;
        options.resize_textures    = false;
        options.max_texture_width  = 1024;
        options.max_texture_height = 1024;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.PackContent(resources, options));

        gfx::Image img;
        TEST_REQUIRE(img.Load("TestPackage/textures/test_bitmap0.png"));
        TEST_REQUIRE(gfx::Compare(bitmap[0], img.AsBitmap<gfx::RGB>()));

        TEST_REQUIRE(img.Load("TestPackage/textures/test_bitmap1.png"));
        TEST_REQUIRE(gfx::Compare(bitmap[1], img.AsBitmap<gfx::RGB>()));
    }

    // texture size that exceeds the max texture sizes and no resizing
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");

        gfx::MaterialClass material;
        material.AddTexture("test_bitmap3.png");
        app::MaterialResource resource(material, "material");

        app::Workspace workspace;
        workspace.MakeWorkspace("TestWorkspace");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory = "TestPackage";
        options.package_name = "";
        options.write_content_file = true;
        options.write_config_file  = true;
        options.combine_textures   = true;
        options.resize_textures    = false;
        options.max_texture_width  = 512;
        options.max_texture_height = 512;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.PackContent(resources, options));

        gfx::Image img;
        TEST_REQUIRE(img.Load("TestPackage/textures/test_bitmap3.png"));
        TEST_REQUIRE(gfx::Compare(bitmap[3], img.AsBitmap<gfx::RGB>()));
    }

    // texture size that exceeds the max texture size gets resized.
    {
        // todo: implement resizing of textures
    }

    // test discarding multiple copies of textures while combining
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");

        app::Workspace workspace;
        workspace.MakeWorkspace("TestWorkspace");
        // first material
        {
            gfx::MaterialClass material;
            material.AddTexture("test_bitmap0.png");
            material.AddTexture("test_bitmap1.png");
            app::MaterialResource resource(material, "material 1");
            workspace.SaveResource(resource);
        }
        // second material
        {
            gfx::MaterialClass material;
            material.AddTexture("test_bitmap0.png");
            material.AddTexture("test_bitmap1.png");
            material.AddTexture("test_bitmap2.png");
            material.AddTexture("test_bitmap3.png");
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
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        resources.push_back(&workspace.GetUserDefinedResource(1));
        TEST_REQUIRE(workspace.PackContent(resources, options));

        // bitmap0 and bitmap1 should only be copied once and combined
        // with bitmap2. Bitmap3 is too large to pack.
        gfx::Image generated;
        TEST_REQUIRE(generated.Load("TestPackage/textures/Generated_0.png"));
        const auto& bmp = generated.AsBitmap<gfx::RGB>();
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Blue) == 64*64);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Red) == 64*64);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Green) == 512*512);

        gfx::Image img;
        TEST_REQUIRE(img.Load("TestPackage/textures/test_bitmap3.png"));
        TEST_REQUIRE(gfx::Compare(bitmap[3], img.AsBitmap<gfx::RGB>()));
    }

    // test discarding multiple copies of textures
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");

        app::Workspace workspace;
        workspace.MakeWorkspace("TestWorkspace");
        // first material
        {
            gfx::MaterialClass material;
            material.AddTexture("test_bitmap0.png");
            material.AddTexture("test_bitmap1.png");
            app::MaterialResource resource(material, "material 1");
            workspace.SaveResource(resource);
        }
        // second material
        {
            gfx::MaterialClass material;
            material.AddTexture("test_bitmap0.png");
            material.AddTexture("test_bitmap1.png");
            material.AddTexture("test_bitmap2.png");
            material.AddTexture("test_bitmap3.png");
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
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        resources.push_back(&workspace.GetUserDefinedResource(1));
        TEST_REQUIRE(workspace.PackContent(resources, options));

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

void unit_test_packing_texture_rects()
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

        app::Workspace workspace;
        workspace.MakeWorkspace("TestWorkspace");

        gfx::MaterialClass material;
        material.AddTexture("test_bitmap0.png");
        material.SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
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
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.PackContent(resources, options));

        game::ContentLoader loader;
        loader.LoadFromFile("TestPackage", "TestPackage/content.json");
        auto mat = loader.GetMaterialClass(material.GetId());
        const auto& rect = mat->GetTextureRect(0);
        TEST_REQUIRE(rect == gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    }

    // sub rectangle, no texture combination
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");

        app::Workspace workspace;
        workspace.MakeWorkspace("TestWorkspace");

        gfx::MaterialClass material;
        material.AddTexture("test_bitmap0.png");
        material.SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 0.5f, 0.5f));
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
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.PackContent(resources, options));

        game::ContentLoader loader;
        loader.LoadFromFile("TestPackage", "TestPackage/content.json");
        auto mat = loader.GetMaterialClass(material.GetId());
        const auto& rect = mat->GetTextureRect(0);
        TEST_REQUIRE(rect == gfx::FRect(0.0f, 0.0f, 0.5f, 0.5f));
    }

    // texture rectangles with texture packing.
    {
        DeleteDir("TestWorkspace");
        DeleteDir("TestPackage");

        app::Workspace workspace;
        workspace.MakeWorkspace("TestWorkspace");

        gfx::MaterialClass material;
        material.AddTexture("test_bitmap0.png");
        material.AddTexture("test_bitmap1.png");
        const auto src_rect0 = gfx::FRect(0.5f, 0.5f, 0.5f, 0.5f);
        const auto src_rect1 = gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f);
        material.SetTextureRect(0, src_rect0);
        material.SetTextureRect(1, src_rect1);
        app::MaterialResource resource(material, "material");
        workspace.SaveResource(resource);

        app::Workspace::ContentPackingOptions options;
        options.directory = "TestPackage";
        options.package_name = "";
        options.write_content_file = true;
        options.write_config_file = true;
        options.combine_textures = true;
        options.resize_textures = false;
        options.max_texture_width = 1024;
        options.max_texture_height = 1024;
        // select the resources.
        std::vector<const app::Resource *> resources;
        resources.push_back(&workspace.GetUserDefinedResource(0));
        TEST_REQUIRE(workspace.PackContent(resources, options));

        gfx::Image img;
        TEST_REQUIRE(img.Load("TestPackage/textures/Generated_0.png"));
        const auto& bmp = img.AsBitmap<gfx::RGB>();
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::HotPink) == 32*32);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Green) == 32*32);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Red) == 32*32);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Blue) == 32*32);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::Yellow) == 32*32);

        game::ContentLoader loader;
        loader.LoadFromFile("TestPackage", "TestPackage/content.json");
        const auto& mat = loader.GetMaterialClass(material.GetId());
        const auto& rect0 = mat->GetTextureRect(0);
        const auto& rect1 = mat->GetTextureRect(1);
        const auto src_fixed_rect0 = src_rect0.Expand(bitmap[0].GetSize());
        const auto src_fixed_rect1 = src_rect1.Expand(bitmap[1].GetSize());
        const auto dst_fixed_rect0 = rect0.Expand(bmp.GetSize());
        const auto dst_fixed_rect1 = rect1.Expand(bmp.GetSize());
        TEST_REQUIRE(gfx::Compare(bitmap[0].Copy(src_fixed_rect0), bmp.Copy(dst_fixed_rect0)));
        TEST_REQUIRE(gfx::Compare(bitmap[1].Copy(src_fixed_rect1), bmp.Copy(dst_fixed_rect1)));
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
    unit_test_packing_texture_composition();
    unit_test_packing_texture_rects();
    return 0;
}
