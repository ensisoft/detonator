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

#define LOGTAG "app"

#include "config.h"

#include "warnpush.h"
#  include <quazip/quazip.h>
#  include <quazip/quazipfile.h>
#  include <nlohmann/json.hpp>
#  include <neargye/magic_enum.hpp>
#  include <private/qfsfileengine_p.h> // private in Qt5
#  include <QCoreApplication>
#  include <QtAlgorithms>
#  include <QJsonDocument>
#  include <QJsonArray>
#  include <QByteArray>
#  include <QFile>
#  include <QFileInfo>
#  include <QIcon>
#  include <QPainter>
#  include <QImage>
#  include <QImageWriter>
#  include <QPixmap>
#  include <QDir>
#  include <QStringList>
#  include <QProgressDialog>
#include "warnpop.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <set>
#include <stack>
#include <functional>

#include "engine/ui.h"
#include "engine/data.h"
#include "data/json.h"
#include "base/json.h"
#include "graphics/resource.h"
#include "graphics/color4f.h"
#include "graphics/simple_shape.h"
#include "graphics/texture_file_source.h"
#include "graphics/image.h"
#include "graphics/packer.h"
#include "graphics/material_instance.h"
#include "editor/app/resource-uri.h"
#include "editor/app/resource_packer.h"
#include "editor/app/resource_tracker.h"
#include "editor/app/resource_util.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/utility.h"
#include "editor/app/format.h"
#include "editor/app/packing.h"
#include "editor/app/buffer.h"
#include "editor/app/process.h"
#include "editor/app/zip_archive.h"
#include "editor/app/zip_archive_exporter.h"
#include "editor/app/zip_archive_importer.h"
#include "editor/app/workspace_observer.h"
#include "editor/app/workspace_resource_packer.h"

// hack
#include "editor/app/resource_tracker.cpp"

namespace {

gfx::Color4f ToGfx(const QColor& color)
{
    const float a  = color.alphaF();
    const float r  = color.redF();
    const float g  = color.greenF();
    const float b  = color.blueF();
    return gfx::Color4f(r, g, b, a);
}


class GfxTexturePacker : public gfx::TexturePacker
{
public:
    using ObjectHandle = gfx::TexturePacker::ObjectHandle;

    GfxTexturePacker(const QString& outdir,
                     unsigned max_width, unsigned max_height,
                     unsigned pack_width, unsigned pack_height, unsigned padding,
                     bool resize_large, bool pack_small)
        : kOutDir(outdir)
        , kMaxTextureWidth(max_width)
        , kMaxTextureHeight(max_height)
        , kTexturePackWidth(pack_width)
        , kTexturePackHeight(pack_height)
        , kTexturePadding(padding)
        , kResizeLargeTextures(resize_large)
        , kPackSmallTextures(pack_small)
    {}
   ~GfxTexturePacker()
    {
        for (const auto& temp : mTempFiles)
        {
            QFile::remove(temp);
        }
    }
    virtual void PackTexture(ObjectHandle instance, const std::string& file) override
    {
        mTextureMap[instance].file = file;
    }
    virtual void SetTextureBox(ObjectHandle instance, const gfx::FRect& box) override
    {
        mTextureMap[instance].rect = box;
    }
    virtual void SetTextureFlag(ObjectHandle instance, gfx::TexturePacker::TextureFlags flags, bool on_off) override
    {
        if (flags == gfx::TexturePacker::TextureFlags::CanCombine)
            mTextureMap[instance].can_be_combined = on_off;
        else if (flags == gfx::TexturePacker::TextureFlags::AllowedToPack)
            mTextureMap[instance].allowed_to_combine = on_off;
        else if (flags == gfx::TexturePacker::TextureFlags::AllowedToResize)
            mTextureMap[instance].allowed_to_resize = on_off;
        else BUG("Unhandled texture packing flag.");
    }
    virtual std::string GetPackedTextureId(ObjectHandle instance) const override
    {
        auto it = mTextureMap.find(instance);
        ASSERT(it != std::end(mTextureMap));
        return it->second.file;
    }
    virtual gfx::FRect GetPackedTextureBox(ObjectHandle instance) const override
    {
        auto it = mTextureMap.find(instance);
        ASSERT(it != std::end(mTextureMap));
        return it->second.rect;
    }

    using TexturePackingProgressCallback = std::function<void (std::string, int, int)>;

    void PackTextures(TexturePackingProgressCallback  progress, app::WorkspaceResourcePacker& packer)
    {
        if (mTextureMap.empty())
            return;

        if (!app::MakePath(app::JoinPath(kOutDir, "textures")))
        {
            ERROR("Failed to create texture directory. [dir='%1/%2']", kOutDir, "textures");
            mNumErrors++;
            return;
        }

        // OpenGL ES 2. defines the minimum required supported texture size to be
        // only 64x64 px which is not much. Anything bigger than that
        // is implementation specific. :p
        // for maximum portability we then just pretty much skip the whole packing.

        using PackList = std::vector<app::PackingRectangle>;

        struct TextureCategory {
            QImage::Format format;
            PackList sources;
        };

        // the source list of rectangles (images to pack)

        TextureCategory rgba_textures;
        TextureCategory rgb_textures;
        TextureCategory grayscale_textures;
        rgba_textures.format = QImage::Format::Format_RGBA8888;
        rgb_textures.format  = QImage::Format::Format_RGB888;
        grayscale_textures.format = QImage::Format::Format_Grayscale8;

        TextureCategory* all_textures[] = {
            &rgba_textures, &rgb_textures, &grayscale_textures
        };

        struct GeneratedTextureEntry {
            QString uri;
            // box of the texture that was packed
            // now inside the texture_file
            float xpos   = 0;
            float ypos   = 0;
            float width  = 0;
            float height = 0;
        };

        // map original file handle to a new generated texture entry
        // which defines either a box inside a generated texture pack
        // (combination of multiple textures) or a downscaled (originally large) texture.
        std::unordered_map<std::string, GeneratedTextureEntry> relocation_map;
        // map image URIs to URIs. If the image has been resampled
        // the source URI maps to a file in the /tmp. Otherwise, it maps to itself.
        std::unordered_map<std::string, QString> image_map;

        // 1. go over the list of textures, ignore duplicates
        // 2. if the texture is larger than max texture size resize it
        // 3. if the texture can be combined pick it for combining otherwise
        //    generate a texture entry and copy into output
        // then:
        // 4. combine the textures that have been selected for combining
        //    into atlas/atlasses.
        // -- composite the actual image files.
        // 5. copy the src image contents into the container image.
        // 6. write the container/packed image into the package folder
        // 7. update the textures whose source images were packaged
        //    - the file handle/URI needs to be remapped
        //    - and the rectangle box needs to be remapped

        int cur_step = 0;
        int max_step = static_cast<int>(mTextureMap.size());

        for (auto it = mTextureMap.begin(); it != mTextureMap.end(); ++it)
        {
            progress("Copying textures...", cur_step++, max_step);

            const TextureSource& tex = it->second;
            if (tex.file.empty())
                continue;
            else if (auto it = image_map.find(tex.file) != image_map.end())
                continue;

            const QFileInfo info(app::FromUtf8(tex.file));
            const QString src_file = info.absoluteFilePath();
            //const QImage src_pix(src_file);
            // QImage seems to lie about something or then the test pngs are produced
            // somehow wrong but an image that should have 24 bits for depth gets
            // reported as 32bit when QImage loads it.
            std::vector<char> img_data;
            if (!app::ReadBinaryFile(src_file, img_data))
            {
                ERROR("Failed to open image file. [file='%1']", src_file);
                mNumErrors++;
                continue;
            }

            gfx::Image img;
            if (!img.Load(&img_data[0], img_data.size()))
            {
                ERROR("Failed to decompress image file. [file='%1']", src_file);
                mNumErrors++;
                continue;
            }
            const auto width  = img.GetWidth();
            const auto height = img.GetHeight();
            const auto* data = img.GetData();
            QImage src_pix;
            if (img.GetDepthBits() == 8)
                src_pix = QImage((const uchar*)data, width, height, width*1, QImage::Format_Grayscale8);
            else if (img.GetDepthBits() == 24)
                src_pix = QImage((const uchar*)data, width, height, width*3, QImage::Format_RGB888);
            else if (img.GetDepthBits() == 32)
                src_pix = QImage((const uchar*)data, width, height, width*4, QImage::Format_RGBA8888);

            if (src_pix.isNull())
            {
                ERROR("Failed to load image file. [file='%1']", src_file);
                mNumErrors++;
                continue;
            }
            QString img_file = src_file;
            QString img_name = info.fileName();
            auto img_width  = src_pix.width();
            auto img_height = src_pix.height();
            auto img_depth  = src_pix.depth();
            DEBUG("Loading image file. [file='%1', width=%2, height=%3, depth=%4]", src_file, img_width, img_height, img_depth);

            if (!(img_depth == 32 || img_depth == 24 || img_depth == 8))
            {
                ERROR("Unsupported image format and depth. [file='%1', depth=%2]", src_file, img_depth);
                mNumErrors++;
                continue;
            }

            const bool too_large = img_width > kMaxTextureWidth ||
                                   img_height > kMaxTextureHeight;
            const bool can_resize = kResizeLargeTextures && tex.allowed_to_resize;
            const bool needs_resampling = too_large && can_resize;

            // first check if the source image needs to be resampled. if so
            // resample in and output into /tmp
            if (needs_resampling)
            {
                const auto scale = std::min(kMaxTextureWidth / (float)img_width,
                                            kMaxTextureHeight / (float)img_height);
                const auto dst_width  = img_width * scale;
                const auto dst_height = img_height * scale;
                QImage::Format format = QImage::Format::Format_Invalid;
                if (img_depth == 32)
                    format = QImage::Format::Format_RGBA8888;
                else if (img_depth == 24)
                    format = QImage::Format::Format_RGB888;
                else if (img_depth == 8)
                    format = QImage::Format::Format_Grayscale8;
                else BUG("Missed image bit depth support check.");

                QImage buffer(dst_width, dst_height, format);
                buffer.fill(QColor(0x00, 0x00, 0x00, 0x00));
                QPainter painter(&buffer);
                painter.setCompositionMode(QPainter::CompositionMode_Source);
                painter.setRenderHint(QPainter::SmoothPixmapTransform, true); // bi-linear filtering
                const QRectF dst_rect(0, 0, dst_width, dst_height);
                const QRectF src_rect(0, 0, img_width, img_height);
                painter.drawImage(dst_rect, src_pix, src_rect);

                // create a scratch file into which write the re-sampled image file
                const QString& name = app::RandomString() + ".png";
                const QString temp  = app::JoinPath(QDir::tempPath(), name);
                QImageWriter writer;
                writer.setFormat("PNG");
                writer.setQuality(100);
                writer.setFileName(temp);
                if (!writer.write(buffer))
                {
                    ERROR("Failed to write temp image. [file='%1']", temp);
                    mNumErrors++;
                    continue;
                }
                DEBUG("Image was resampled and resized. [src='%1', dst='%2', width=%3, height=%4]", src_file, temp, img_width, img_height);
                img_width  = dst_width;
                img_height = dst_height;
                img_file   = temp;
                img_name   = info.baseName() + ".png";
                // map the input image to an image in /tmp/
                image_map[tex.file] = temp;
                mTempFiles.push_back(temp);
            }
            else
            {
                // the input image maps to itself since there's no
                // scratch image that is needed.
                image_map[tex.file] = app::FromUtf8(tex.file);
            }

            // check if the texture can be combined.
            if (kPackSmallTextures && tex.can_be_combined && tex.allowed_to_combine && img_width < kTexturePackWidth && img_height < kTexturePackHeight)
            {
                // add as a source for texture packing
                app::PackingRectangle rc;
                rc.width  = img_width + kTexturePadding * 2;
                rc.height = img_height + kTexturePadding * 2;
                rc.cookie = tex.file; // this is just used as an ID here.
                if (img_depth == 32)
                    rgba_textures.sources.push_back(rc);
                else if (img_depth == 24)
                    rgb_textures.sources.push_back(rc);
                else if (img_depth == 8)
                    grayscale_textures.sources.push_back(rc);
                else BUG("Missed image bit depth support check.");
            }
            else
            {
                // Generate a texture entry.
                GeneratedTextureEntry self;
                self.width  = 1.0f;
                self.height = 1.0f;
                self.xpos   = 0.0f;
                self.ypos   = 0.0f;
                self.uri    = packer.MapFileToPackage(packer.DoCopyFile(img_file, "textures", img_name));
                relocation_map[tex.file] = std::move(self);
            }
        } // for

        unsigned atlas_number = 0;
        cur_step = 0;
        max_step = static_cast<int>(grayscale_textures.sources.size() +
                                    rgb_textures.sources.size() +
                                    rgba_textures.sources.size());

        for (auto* texture_category : all_textures)
        {
            auto& sources = texture_category->sources;
            while (!sources.empty())
            {
                progress("Packing textures...", cur_step++, max_step);

                app::PackRectangles({kTexturePackWidth, kTexturePackHeight}, sources, nullptr);
                // ok, some textures might have failed to pack on this pass.
                // separate the ones that were successfully packed from the ones that
                // weren't. then composite the image for the success cases.
                auto first_success = std::partition(sources.begin(), sources.end(),
                    [](const auto& pack_rect) {
                        // put the failed cases first.
                        return pack_rect.success == false;
                });
                const auto num_to_pack = std::distance(first_success, sources.end());
                // we should have already dealt with too big images already.
                ASSERT(num_to_pack > 0);
                if (num_to_pack == 1)
                {
                    // if we can only fit 1 single image in the container
                    // then what's the point ?
                    // we'd just end up wasting space, so just leave it as is.
                    const auto& rc = *first_success;
                    ASSERT(image_map.find(rc.cookie) != image_map.end());
                    QString file = image_map[rc.cookie];

                    GeneratedTextureEntry gen;
                    gen.uri    = packer.MapFileToPackage(packer.DoCopyFile(file, "textures"));
                    gen.width  = 1.0f;
                    gen.height = 1.0f;
                    gen.xpos   = 0.0f;
                    gen.ypos   = 0.0f;
                    relocation_map[rc.cookie] = gen;
                    sources.erase(first_success);
                    continue;
                }

                // composition buffer.
                QImage buffer(kTexturePackWidth, kTexturePackHeight, texture_category->format);
                buffer.fill(QColor(0x00, 0x00, 0x00, 0x00));

                QPainter painter(&buffer);
                painter.setCompositionMode(QPainter::CompositionMode_Source); // copy src pixel as-is

                // do the composite pass.
                for (auto it = first_success; it != sources.end(); ++it)
                {
                    const auto& rc = *it;
                    ASSERT(rc.success);
                    const auto padded_width = rc.width;
                    const auto padded_height = rc.height;
                    const auto width = padded_width - kTexturePadding * 2;
                    const auto height = padded_height - kTexturePadding * 2;

                    ASSERT(image_map.find(rc.cookie) != image_map.end());
                    const QString img_file = image_map[rc.cookie];
                    const QFileInfo info(img_file);
                    const QString file(info.absoluteFilePath());
                    // compensate for possible texture sampling issues by padding the
                    // image with some extra pixels by growing it a few pixels on both
                    // axis.
                    const QRectF dst(rc.xpos, rc.ypos, padded_width, padded_height);
                    const QRectF src(0, 0, width, height);
                    const QPixmap img(file);
                    if (img.isNull())
                    {
                        ERROR("Failed to open texture packing image. [file='%1']", file);
                        mNumErrors++;
                    }
                    else
                    {
                        painter.drawPixmap(dst, img, src);
                    }
                }

                const QString& name = QString("Generated_%1.png").arg(atlas_number);
                const QString& file = app::JoinPath(app::JoinPath(kOutDir, "textures"), name);

                QImageWriter writer;
                writer.setFormat("PNG");
                writer.setQuality(100);
                writer.setFileName(file);
                if (!writer.write(buffer))
                {
                    ERROR("Failed to write image. [file='%1']", file);
                    mNumErrors++;
                }
                const float pack_width = kTexturePackWidth; //packing_rect_result.width;
                const float pack_height = kTexturePackHeight; //packing_rect_result.height;

                // create mapping for each source texture to the generated
                // texture.
                for (auto it = first_success; it != sources.end(); ++it)
                {
                    const auto& rc = *it;
                    const auto padded_width = rc.width;
                    const auto padded_height = rc.height;
                    const auto width = padded_width - kTexturePadding * 2;
                    const auto height = padded_height - kTexturePadding * 2;
                    const auto xpos = rc.xpos + kTexturePadding;
                    const auto ypos = rc.ypos + kTexturePadding;
                    GeneratedTextureEntry gen;
                    gen.uri    = app::toString("pck://textures/%1", name);
                    gen.width  = (float)width / pack_width;
                    gen.height = (float)height / pack_height;
                    gen.xpos   = (float)xpos / pack_width;
                    gen.ypos   = (float)ypos / pack_height;
                    relocation_map[rc.cookie] = gen;
                    DEBUG("New image packing entry. [id='%1', dst='%2']", rc.cookie, gen.uri);
                }

                // done with these.
                sources.erase(first_success, sources.end());

                atlas_number++;
            } // while (!sources.empty())
        }

        cur_step = 0;
        max_step = static_cast<int>(mTextureMap.size());
        // update texture object mappings, file handles and texture boxes.
        // for each texture object, look up where the original file handle
        // maps to. Then the original texture box is now a box within a box.
        for (auto& pair : mTextureMap)
        {
            progress("Remapping textures...", cur_step++, max_step);

            TextureSource& tex = pair.second;
            const auto original_file = tex.file;
            const auto original_rect = tex.rect;

            auto it = relocation_map.find(original_file);
            if (it == relocation_map.end()) // font texture sources only have texture box.
                continue;
            //ASSERT(it != relocation_map.end());
            const GeneratedTextureEntry& relocation = it->second;

            const auto original_rect_x = original_rect.GetX();
            const auto original_rect_y = original_rect.GetY();
            const auto original_rect_width  = original_rect.GetWidth();
            const auto original_rect_height = original_rect.GetHeight();

            tex.file = app::ToUtf8(relocation.uri);
            tex.rect = gfx::FRect(relocation.xpos + original_rect_x * relocation.width,
                                  relocation.ypos + original_rect_y * relocation.height,
                                  relocation.width * original_rect_width,
                                  relocation.height * original_rect_height);
        }
    }
    unsigned GetNumErrors() const
    { return mNumErrors; }
private:
    const QString kOutDir;
    const unsigned kMaxTextureHeight = 0;
    const unsigned kMaxTextureWidth = 0;
    const unsigned kTexturePackWidth = 0;
    const unsigned kTexturePackHeight = 0;
    const unsigned kTexturePadding = 0;
    const bool kResizeLargeTextures = true;
    const bool kPackSmallTextures = true;
    unsigned mNumErrors = 0;

    struct TextureSource {
        std::string file;
        gfx::FRect  rect;
        bool can_be_combined = true;
        bool allowed_to_resize = true;
        bool allowed_to_combine = true;
    };
    std::unordered_map<ObjectHandle, TextureSource> mTextureMap;
    std::vector<QString> mTempFiles;
};


} // namespace

namespace app
{


// static
bool Workspace::mEnableAppResourceCaching = true;
// static
Workspace::GraphicsBufferCache Workspace::mAppGraphicsBufferCache;

Workspace::Workspace(const QString& dir)
  : mWorkspaceDir(FixWorkspacePath(dir))
{
    DEBUG("Create workspace");

    // initialize the primitive resources, i.e the materials
    // and drawables that are part of the workspace without any
    // user interaction.

    // Checkerboard is a special material that is always available.
    // It is used as the initial material when user hasn't selected
    // anything or when the material referenced by some object is deleted
    // the material reference can be updated to Checkerboard.
    auto checkerboard = std::make_shared<gfx::MaterialClass>(gfx::MaterialClass::Type::Texture, std::string("_checkerboard"));
    checkerboard->SetTexture(gfx::LoadTextureFromFile(res::Checkerboard));
    checkerboard->SetName("Checkerboard");
    mResources.emplace_back(new MaterialResource(checkerboard, "Checkerboard"));

    // add some primitive colors.
    constexpr auto& values = magic_enum::enum_values<gfx::Color>();
    for (const auto& val : values)
    {
        const std::string color_name(magic_enum::enum_name(val));
        auto color = std::make_shared<gfx::ColorClass>(gfx::MaterialClass::Type::Color, "_" + color_name);
        color->SetBaseColor(val);
        color->SetName("_" + color_name);
        color->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        mResources.emplace_back(new MaterialResource(color, color_name));
    }

    // setup primitive drawables with known/fixed class IDs
    // these IDs are also hardcoded in the engine/loader.cpp which uses
    // these same IDs to create primitive resources.
    mResources.emplace_back(new DrawableResource<gfx::CapsuleClass>(gfx::CapsuleClass("_capsule"), "2D Capsule"));
    mResources.emplace_back(new DrawableResource<gfx::RectangleClass>(gfx::RectangleClass("_rect"), "2D Rectangle"));
    mResources.emplace_back(new DrawableResource<gfx::IsoscelesTriangleClass>(gfx::IsoscelesTriangleClass("_isosceles_triangle"), "2D Isosceles Triangle"));
    mResources.emplace_back(new DrawableResource<gfx::RightTriangleClass>(gfx::RightTriangleClass("_right_triangle"), "2D Right Triangle"));
    mResources.emplace_back(new DrawableResource<gfx::CircleClass>(gfx::CircleClass("_circle"), "2D Circle"));
    mResources.emplace_back(new DrawableResource<gfx::SemiCircleClass>(gfx::SemiCircleClass("_semi_circle"), "2D Semi Circle"));
    mResources.emplace_back(new DrawableResource<gfx::TrapezoidClass>(gfx::TrapezoidClass("_trapezoid"), "2D Trapezoid"));
    mResources.emplace_back(new DrawableResource<gfx::ParallelogramClass>(gfx::ParallelogramClass("_parallelogram"), "2D Parallelogram"));
    mResources.emplace_back(new DrawableResource<gfx::RoundRectangleClass>(gfx::RoundRectangleClass("_round_rect", "", 0.05f), "2D Round Rectangle"));
    mResources.emplace_back(new DrawableResource<gfx::ArrowCursorClass>(gfx::ArrowCursorClass("_arrow_cursor"), "2D Arrow Cursor"));
    mResources.emplace_back(new DrawableResource<gfx::BlockCursorClass>(gfx::BlockCursorClass("_block_cursor"), "2D Block Cursor"));

    mResources.emplace_back(new DrawableResource<gfx::ConeClass>(gfx::ConeClass("_cone", "", 100), "3D Cone"));
    mResources.emplace_back(new DrawableResource<gfx::CubeClass>(gfx::CubeClass("_cube"), "3D Cube"));
    mResources.emplace_back(new DrawableResource<gfx::CylinderClass>(gfx::CylinderClass("_cylinder", "", 100), "3D Cylinder"));
    mResources.emplace_back(new DrawableResource<gfx::PyramidClass>(gfx::PyramidClass("_pyramid"), "3D Pyramid"));
    mResources.emplace_back(new DrawableResource<gfx::SphereClass>(gfx::SphereClass("_sphere", "", 100), "3D Sphere"));

    for (auto& resource : mResources)
    {
        resource->SetIsPrimitive(true);
    }
    mSettings.application_identifier = app::RandomString();
}

Workspace::~Workspace()
{
    DEBUG("Destroy workspace");
}

QVariant Workspace::data(const QModelIndex& index, int role) const
{
    ASSERT(index.model() == this);
    ASSERT(index.row() < mResources.size());
    const auto& res = mResources[index.row()];

    if (role == Qt::SizeHintRole)
        return QSize(0, 16);
    else if (role == Qt::DisplayRole)
    {
        if (index.column() == 0)
            return toString(res->GetType());
        else if (index.column() == 1)
            return res->GetName();
    }
    else if (role == Qt::DecorationRole)
    {
        if (index.column() == 0)
            return res->GetIcon();
        else if (index.column() == 1)
        {
            if (!res->GetProperty("_is_valid_", true))
                return QIcon("icons:problem.png");
        }
    }
    else if (role == Qt::ToolTipRole)
    {
        std::string problem;
        if (res->GetProperty("_problem_", &problem))
            return app::toString(problem);
    }
    return QVariant();
}

QVariant Workspace::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        switch (section)
        {
            case 0:  return "Type";
            case 1:  return "Name";
        }
    }
    return QVariant();
}

QAbstractFileEngine* Workspace::create(const QString& file) const
{
    // CAREFUL ABOUT RECURSION HERE.
    // DO NOT CALL QFile, QFileInfo or QDir !

    // only handle our special cases.
    QString ret = file;
    if (ret.startsWith("ws://"))
        ret.replace("ws://", mWorkspaceDir);
    else if (file.startsWith("app://"))
        ret.replace("app://", GetAppDir());
    else if (file.startsWith("fs://"))
        ret.remove(0, 5);
    else return nullptr;

    DEBUG("Mapping Qt file '%1' => '%2'", file, ret);

    return new QFSFileEngine(ret);
}

std::unique_ptr<gfx::Material> Workspace::MakeMaterialByName(const AnyString& name) const
{
    return gfx::CreateMaterialInstance(GetMaterialClassByName(name));

}
std::unique_ptr<gfx::Drawable> Workspace::MakeDrawableByName(const AnyString& name) const
{
    return gfx::CreateDrawableInstance(GetDrawableClassByName(name));
}
std::unique_ptr<gfx::Drawable> Workspace::MakeDrawableById(const AnyString& id) const
{
    return gfx::CreateDrawableInstance(GetDrawableClassById(id));
}

std::shared_ptr<const gfx::MaterialClass> Workspace::GetMaterialClassByName(const AnyString& name) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Material)
            continue;
        else if (resource->GetName() != name)
            continue;
        return ResourceCast<gfx::MaterialClass>(*resource).GetSharedResource();
    }
    BUG("No such material class.");
    return nullptr;
}

std::shared_ptr<const gfx::MaterialClass> Workspace::GetMaterialClassById(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Material)
            continue;
        else if (resource->GetId() != id)
            continue;
        return ResourceCast<gfx::MaterialClass>(*resource).GetSharedResource();
    }
    BUG("No such material class.");
    return nullptr;
}

std::shared_ptr<const gfx::DrawableClass> Workspace::GetDrawableClassByName(const AnyString& name) const
{
    for (const auto& resource : mResources)
    {
        if (!(resource->GetType() == Resource::Type::Shape ||
              resource->GetType() == Resource::Type::ParticleSystem ||
              resource->GetType() == Resource::Type::Drawable))
            continue;
        else if (resource->GetName() != name)
            continue;

        if (resource->GetType() == Resource::Type::Drawable)
            return ResourceCast<gfx::DrawableClass>(*resource).GetSharedResource();
        if (resource->GetType() == Resource::Type::ParticleSystem)
            return ResourceCast<gfx::ParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::Shape)
            return ResourceCast<gfx::PolygonMeshClass>(*resource).GetSharedResource();
    }
    BUG("No such drawable class.");
    return nullptr;
}

std::shared_ptr<const gfx::DrawableClass> Workspace::GetDrawableClassById(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (!(resource->GetType() == Resource::Type::Shape ||
              resource->GetType() == Resource::Type::ParticleSystem ||
              resource->GetType() == Resource::Type::Drawable))
            continue;
        else if (resource->GetId() != id)
            continue;

        if (resource->GetType() == Resource::Type::Drawable)
            return ResourceCast<gfx::DrawableClass>(*resource).GetSharedResource();
        if (resource->GetType() == Resource::Type::ParticleSystem)
            return ResourceCast<gfx::ParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::Shape)
            return ResourceCast<gfx::PolygonMeshClass>(*resource).GetSharedResource();
    }
    BUG("No such drawable class.");
    return nullptr;
}


std::shared_ptr<const game::EntityClass> Workspace::GetEntityClassByName(const AnyString& name) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Entity)
            continue;
        else if (resource->GetName() != name)
            continue;
        return ResourceCast<game::EntityClass>(*resource).GetSharedResource();
    }
    BUG("No such entity class.");
    return nullptr;
}
std::shared_ptr<const game::EntityClass> Workspace::GetEntityClassById(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Entity)
            continue;
        else if (resource->GetId() != id)
            continue;
        return ResourceCast<game::EntityClass>(*resource).GetSharedResource();
    }
    BUG("No such entity class.");
    return nullptr;
}

std::shared_ptr<const game::TilemapClass> Workspace::GetTilemapClassById(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Tilemap)
            continue;
        else if (resource->GetId() != id)
            continue;
        return ResourceCast<game::TilemapClass>(*resource).GetSharedResource();
    }
    BUG("No such tilemap class.");
    return nullptr;
}


engine::ClassHandle<const audio::GraphClass> Workspace::FindAudioGraphClassById(const std::string& id) const
{
    return FindClassHandleById<audio::GraphClass>(id, Resource::Type::AudioGraph);
}
engine::ClassHandle<const audio::GraphClass> Workspace::FindAudioGraphClassByName(const std::string& name) const
{
    return FindClassHandleByName<audio::GraphClass>(name, Resource::Type::AudioGraph);
}
engine::ClassHandle<const uik::Window> Workspace::FindUIByName(const std::string& name) const
{
    return FindClassHandleByName<uik::Window>(name, Resource::Type::UI);
}
engine::ClassHandle<const uik::Window> Workspace::FindUIById(const std::string& id) const
{
    return FindClassHandleById<uik::Window>(id, Resource::Type::UI);
}
engine::ClassHandle<const gfx::MaterialClass> Workspace::FindMaterialClassByName(const std::string& name) const
{
    return FindClassHandleByName<gfx::MaterialClass>(name, Resource::Type::Material);
}
engine::ClassHandle<const gfx::MaterialClass> Workspace::FindMaterialClassById(const std::string& klass) const
{
    return FindClassHandleById<gfx::MaterialClass>(klass, Resource::Type::Material);
}
engine::ClassHandle<const gfx::DrawableClass> Workspace::FindDrawableClassById(const std::string& klass) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetIdUtf8() != klass)
            continue;

        if (resource->GetType() == Resource::Type::Drawable)
            return ResourceCast<gfx::DrawableClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::ParticleSystem)
            return ResourceCast<gfx::ParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::Shape)
            return ResourceCast<gfx::PolygonMeshClass>(*resource).GetSharedResource();
    }
    return nullptr;
}
engine::ClassHandle<const game::EntityClass> Workspace::FindEntityClassByName(const std::string& name) const
{
    return FindClassHandleByName<game::EntityClass>(name, Resource::Type::Entity);
}
engine::ClassHandle<const game::EntityClass> Workspace::FindEntityClassById(const std::string& id) const
{
    return FindClassHandleById<game::EntityClass>(id, Resource::Type::Entity);
}

engine::ClassHandle<const game::SceneClass> Workspace::FindSceneClassByName(const std::string& name) const
{
    std::shared_ptr<game::SceneClass> ret;
    for (auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Scene)
            continue;
        else if (resource->GetNameUtf8() != name)
            continue;
        ret = ResourceCast<game::SceneClass>(*resource).GetSharedResource();
        break;
    }
    if (!ret)
        return nullptr;

    // resolve entity references.
    for (size_t i=0; i<ret->GetNumNodes(); ++i)
    {
        auto& placement = ret->GetPlacement(i);
        auto klass = FindEntityClassById(placement.GetEntityId());
        if (!klass)
        {
            WARN("Scene entity placement entity class not found. [scene='%1', placement='%2']", ret->GetName(), placement.GetName());
            placement.ResetEntity();
            placement.ResetEntityParams();
        }
        else
        {
            placement.SetEntity(klass);
        }
    }
    return ret;
}
engine::ClassHandle<const game::SceneClass> Workspace::FindSceneClassById(const std::string& id) const
{
    std::shared_ptr<game::SceneClass> ret;
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Scene)
            continue;
        else if (resource->GetIdUtf8() != id)
            continue;
        ret = ResourceCast<game::SceneClass>(*resource).GetSharedResource();
    }
    if (!ret)
        return nullptr;

    // resolve entity references.
    for (size_t i=0; i<ret->GetNumNodes(); ++i)
    {
        auto& placement = ret->GetPlacement(i);
        auto klass = FindEntityClassById(placement.GetEntityId());
        if (!klass)
        {
            WARN("Scene entity placement entity class not found. [scene='%1', placement='%2']", ret->GetName(), placement.GetName());
            placement.ResetEntity();
            placement.ResetEntityParams();
        }
        else
        {
            placement.SetEntity(klass);
        }
    }
    return ret;
}

engine::ClassHandle<const game::TilemapClass> Workspace::FindTilemapClassById(const std::string& id) const
{
    return FindClassHandleById<game::TilemapClass>(id, Resource::Type::Tilemap);
}

engine::EngineDataHandle Workspace::LoadEngineDataUri(const std::string& URI) const
{
    const auto& file = MapFileToFilesystem(URI);
    DEBUG("URI '%1' => '%2'", URI, file);
    return EngineBuffer::LoadFromFile(file);
}

engine::EngineDataHandle Workspace::LoadEngineDataFile(const std::string& filename) const
{
    return EngineBuffer::LoadFromFile(app::FromUtf8(filename));
}
engine::EngineDataHandle Workspace::LoadEngineDataId(const std::string& id) const
{
    for (size_t i=0; i < mUserResourceCount; ++i)
    {
        const auto& resource = mResources[i];
        if (resource->GetIdUtf8() != id)
            continue;

        std::string uri;
        if (resource->IsDataFile())
        {
            const DataFile* data = nullptr;
            resource->GetContent(&data);
            uri = data->GetFileURI();
        }
        else if (resource->IsScript())
        {
            const Script* script = nullptr;
            resource->GetContent(&script);
            uri = script->GetFileURI();
        }
        else BUG("Unknown ID in engine data loading.");
        const auto& file = MapFileToFilesystem(uri);
        DEBUG("URI '%1' => '%2'", uri, file);
        return EngineBuffer::LoadFromFile(file, resource->GetName());
    }
    return nullptr;
}

gfx::ResourceHandle Workspace::LoadResource(const gfx::Loader::ResourceDesc& desc)
{
    const auto& URI = desc.uri;

    if (base::StartsWith(URI, "app://"))
        return LoadAppResource(URI);

    const auto& file = MapFileToFilesystem(URI);
    DEBUG("URI '%1' => '%2'", URI, file);
    auto ret = GraphicsBuffer::LoadFromFile(file);
    return ret;
}

audio::SourceStreamHandle Workspace::OpenAudioStream(const std::string& URI,
    AudioIOStrategy strategy, bool enable_file_caching) const
{
    const auto& file = MapFileToFilesystem(URI);
    DEBUG("URI '%1' => '%2'", URI, file);
    return audio::OpenFileStream(app::ToUtf8(file), strategy, enable_file_caching);
}

game::TilemapDataHandle Workspace::LoadTilemapData(const game::Loader::TilemapDataDesc& desc) const
{
    const auto& file = MapFileToFilesystem(desc.uri);
    DEBUG("URI '%1' => '%2'", desc.uri, file);
    if (desc.read_only)
        return TilemapMemoryMap::OpenFilemap(file);

    return TilemapBuffer::LoadFromFile(file);
}

// static
void Workspace::ClearAppGraphicsCache()
{
    mAppGraphicsBufferCache.clear();
    DEBUG("Cleared app graphics buffer cache.");
}

// static
gfx::ResourceHandle Workspace::LoadAppResource(const std::string& URI)
{
    // static map of resources that are part of the application, i.e.
    // app://something. They're not expected to change.
    if (mEnableAppResourceCaching)
    {
        auto it = mAppGraphicsBufferCache.find(URI);
        if (it != mAppGraphicsBufferCache.end())
            return it->second;
    }

    QString file = app::FromUtf8(URI);
    file = CleanPath(file.replace("app://", GetAppDir()));

    auto ret = GraphicsBuffer::LoadFromFile(file);
    if (mEnableAppResourceCaching)
    {
        mAppGraphicsBufferCache[URI] = ret;
    }
    return ret;
}

bool Workspace::LoadWorkspace(ResourceMigrationLog* log, WorkspaceAsyncWorkObserver* observer)
{
    if (!LoadContent(JoinPath(mWorkspaceDir, "content.json"), log, observer) ||
        !LoadProperties(JoinPath(mWorkspaceDir, "workspace.json"), observer))
        return false;

    // we don't really care if this fails or not. nothing permanently
    // important should be stored in this file. I.e deleting it
    // will just make the application forget some data that isn't
    // crucial for the operation of the application or for the
    // integrity of the workspace and its content.
    LoadUserSettings(JoinPath(mWorkspaceDir, ".workspace_private.json"));

    // Invoke resource migration hook that allows us to perform one-off
    // activities when the underlying data has changed between different
    // data versions.
    for (auto& res : mResources)
    {
        if (!res->IsPrimitive())
            res->Migrate(log);
    }

    INFO("Loaded workspace '%1'", mWorkspaceDir);
    return true;
}

bool Workspace::SaveWorkspace()
{
    if (!SaveContent(JoinPath(mWorkspaceDir, "content.json")) ||
        !SaveProperties(JoinPath(mWorkspaceDir, "workspace.json")))
        return false;

    // should we notify the user if this fails or do we care?
    SaveUserSettings(JoinPath(mWorkspaceDir, ".workspace_private.json"));

    INFO("Saved workspace '%1'", mWorkspaceDir);
    NOTE("Workspace was saved.");
    return true;
}

QString Workspace::GetName() const
{
    return mWorkspaceDir;
}

QString Workspace::GetDir() const
{
    return mWorkspaceDir;
}

QString Workspace::GetSubDir(const QString& dir, bool create) const
{
    const auto& path = JoinPath(mWorkspaceDir, dir);

    if (create)
    {
        QDir d;
        d.setPath(path);
        if (d.exists())
            return path;
        if (!d.mkpath(path))
        {
            ERROR("Failed to create workspace sub directory. [path='%1']", path);
        }
    }
    return path;
}

AnyString Workspace::MapFileToWorkspace(const AnyString& name) const
{
    return MapFileToUri(name, mWorkspaceDir);
}

AnyString Workspace::MapFileToFilesystem(const AnyString& uri) const
{
    return MapUriToFile(uri, mWorkspaceDir);
}

bool Workspace::LoadContent(const QString& filename, ResourceMigrationLog* log, WorkspaceAsyncWorkObserver* observer)
{
    data::JsonFile file;
    const auto [json_ok, error] = file.Load(app::ToUtf8(filename));
    if (!json_ok)
    {
        ERROR("Failed to load workspace JSON content file. [file='%1', error='%2']", filename, error);
        return false;
    }
    data::JsonObject root = file.GetRootObject();

    if (observer)
    {
        const auto resource_count = root.GetNumChunks("materials") +
                                    root.GetNumChunks("particles") +
                                    root.GetNumChunks("shapes") +
                                    root.GetNumChunks("entities") +
                                    root.GetNumChunks("scenes") +
                                    root.GetNumChunks("tilemaps") +
                                    root.GetNumChunks("scripts") +
                                    root.GetNumChunks("data_files") +
                                    root.GetNumChunks("audio_graphs") +
                                    root.GetNumChunks("uis");
        observer->EnqueueStepReset(resource_count);

        std::atomic<bool> load_thread_done = false;

        std::thread loader([this, &root, log, observer, &load_thread_done] {
            observer->EnqueueUpdateMessage("LOADING RESOURCES");

            LoadMaterials<gfx::MaterialClass>("materials", root, mResources, log, observer);
            LoadResources<gfx::ParticleEngineClass>("particles", root, mResources, log, observer);
            LoadResources<gfx::PolygonMeshClass>("shapes", root, mResources, log, observer);
            LoadResources<game::EntityClass>("entities", root, mResources, log, observer);
            LoadResources<game::SceneClass>("scenes", root, mResources, log, observer);
            LoadResources<game::TilemapClass>("tilemaps", root, mResources, log, observer);
            LoadResources<Script>("scripts", root, mResources, log, observer);
            LoadResources<DataFile>("data_files", root, mResources, log, observer);
            LoadResources<audio::GraphClass>("audio_graphs", root, mResources, log, observer);
            LoadResources<uik::Window>("uis", root, mResources, log, observer);

            load_thread_done.store(true, std::memory_order_release);
        });

        base::ElapsedTimer timer;
        timer.Start();

        // intentional slowdown to make the loading process a bit smoother
        // if actually happened really fast.
        while (!load_thread_done.load(std::memory_order_acquire) || timer.SinceStart() < .5)
        {
            observer->ApplyPendingUpdates();
        }
        loader.join();

        DEBUG("Resource load done in %1s", timer.SinceStart());
    }
    else
    {
        LoadMaterials<gfx::MaterialClass>("materials", root, mResources, log, nullptr);
        LoadResources<gfx::ParticleEngineClass>("particles", root, mResources, log, nullptr);
        LoadResources<gfx::PolygonMeshClass>("shapes", root, mResources, log, nullptr);
        LoadResources<game::EntityClass>("entities", root, mResources, log, nullptr);
        LoadResources<game::SceneClass>("scenes", root, mResources, log, nullptr);
        LoadResources<game::TilemapClass>("tilemaps", root, mResources, log, nullptr);
        LoadResources<Script>("scripts", root, mResources, log, nullptr);
        LoadResources<DataFile>("data_files", root, mResources, log, nullptr);
        LoadResources<audio::GraphClass>("audio_graphs", root, mResources, log, nullptr);
        LoadResources<uik::Window>("uis", root, mResources, log, nullptr);
    }

    // create an invariant that states that the primitive materials
    // are in the list of resources after the user defined ones.
    // this way the addressing scheme (when user clicks on an item
    // in the list of resources) doesn't need to change, and it's possible
    // to easily limit the items to be displayed only to those that are
    // user defined.
    auto primitives_start = std::stable_partition(mResources.begin(), mResources.end(),
        [](const auto& resource)  {
            return resource->IsPrimitive() == false;
        });
    mUserResourceCount = std::distance(mResources.begin(), primitives_start);

    for (size_t i=0; i<mUserResourceCount; ++i)
    {
        emit ResourceLoaded(mResources[i].get());
    }

    INFO("Loaded content file '%1'", filename);
    return true;
}

bool Workspace::SaveContent(const QString& filename) const
{
    data::JsonObject root;
    for (const auto& resource : mResources)
    {
        // skip persisting primitive resources since they're always
        // created as part of the workspace creation and their resource
        // IDs are fixed.
        if (resource->IsPrimitive())
            continue;
        // serialize the user defined resource.
        resource->Serialize(root);
    }
    data::JsonFile file;
    file.SetRootObject(root);
    const auto [ok, error] = file.Save(app::ToUtf8(filename));
    if (!ok)
    {
        ERROR("Failed to save JSON content file. [file='%1']", filename);
        return false;
    }
    INFO("Saved workspace content in '%1'", filename);
    return true;
}

bool Workspace::SaveProperties(const QString& filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open properties file for writing. [file='%1']", filename);
        return false;
    }

    // our JSON root object
    QJsonObject json;

    QJsonObject project;
    IntoJson(project, mSettings);

    // serialize the workspace properties into JSON
    json["workspace"] = QJsonObject::fromVariantMap(mProperties);
    json["project"]   = project;

    // serialize the properties stored in each and every
    // resource object.
    for (const auto& resource : mResources)
    {
        if (resource->IsPrimitive())
            continue;
        resource->SaveProperties(json);
    }
    // set the root object to the json document then serialize
    QJsonDocument docu(json);
    file.write(docu.toJson());
    file.close();

    INFO("Saved workspace data in '%1'", filename);
    return true;
}

void Workspace::SaveUserSettings(const QString& filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open file: '%1' for writing. (%2)", filename, file.error());
        return;
    }
    QJsonObject json;
    json["user"] = QJsonObject::fromVariantMap(mUserProperties);
    for (const auto& resource : mResources)
    {
        if (resource->IsPrimitive())
            continue;
        resource->SaveUserProperties(json);
    }

    QJsonDocument docu(json);
    file.write(docu.toJson());
    file.close();
    INFO("Saved private workspace data in '%1'", filename);
}

bool Workspace::LoadProperties(const QString& filename, WorkspaceAsyncWorkObserver* observer)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }

    const auto& buff = file.readAll(); // QByteArray

    QJsonDocument docu(QJsonDocument::fromJson(buff));

    const QJsonObject& project = docu["project"].toObject();
    FromJson(project, mSettings);

    // load the workspace properties.
    mProperties = docu["workspace"].toObject().toVariantMap();

    if (observer)
    {
        observer->EnqueueUpdateMessage("LOADING PROPERTIES");
        observer->EnqueueStepReset(mResources.size());
        observer->ApplyPendingUpdates();
    }

    // so we expect that the content has been loaded first.
    // and then ask each resource object to load its additional
    // properties from the workspace file.
    for (unsigned i=0; i<mResources.size(); ++i)
    {
        auto& resource = mResources[i];

        if (resource->IsPrimitive())
            continue;

        unsigned version = 0;
        ASSERT(resource->GetProperty("__version", &version));
        resource->LoadProperties(docu.object());
        resource->SetProperty("__version", version);

        if (observer)
        {
            observer->EnqueueStepIncrement();
            observer->ApplyPendingUpdates();
        }
    }

    INFO("Loaded workspace file '%1'", filename);
    return true;
}

void Workspace::LoadUserSettings(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        WARN("Failed to open: '%1' (%2)", filename, file.error());
        return;
    }
    const auto& buff = file.readAll(); // QByteArray
    const QJsonDocument docu(QJsonDocument::fromJson(buff));
    mUserProperties = docu["user"].toObject().toVariantMap();

    for (auto& resource : mResources)
    {
        resource->LoadUserProperties(docu.object());
    }

    INFO("Loaded private workspace data: '%1'", filename);
}

Workspace::ResourceList Workspace::ListAllMaterials() const
{
    ResourceList list;
    base::AppendVector(list, ListPrimitiveMaterials());
    base::AppendVector(list, ListUserDefinedMaterials());
    return list;
}

Workspace::ResourceList Workspace::ListPrimitiveMaterials() const
{
    return ListResources(Resource::Type::Material, true, true);
}

Workspace::ResourceList  Workspace::ListUserDefinedUIs() const
{
    return ListResources(Resource::Type::UI, false, true);
}

Workspace::ResourceList Workspace::ListUserDefinedMaps() const
{
    return ListResources(Resource::Type::Tilemap, false, true);
}

Workspace::ResourceList Workspace::ListUserDefinedScripts() const
{
    return ListResources(Resource::Type::Script, false, true);
}

Workspace::ResourceList Workspace::ListUserDefinedMaterials() const
{
    return ListResources(Resource::Type::Material, false, true);
}

Workspace::ResourceList Workspace::ListAllDrawables() const
{
    ResourceList list;
    base::AppendVector(list, ListPrimitiveDrawables());
    base::AppendVector(list, ListUserDefinedDrawables());
    return list;
}

Workspace::ResourceList Workspace::ListPrimitiveDrawables() const
{
    ResourceList list;
    base::AppendVector(list, ListResources(Resource::Type::Drawable, true, false));
    base::AppendVector(list, ListResources(Resource::Type::ParticleSystem, true, false));
    base::AppendVector(list, ListResources(Resource::Type::Shape, true, false));

    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    return list;
}

Workspace::ResourceList Workspace::ListUserDefinedDrawables() const
{
    ResourceList list;
    base::AppendVector(list, ListResources(Resource::Type::Drawable, false, false));
    base::AppendVector(list, ListResources(Resource::Type::ParticleSystem, false, false));
    base::AppendVector(list, ListResources(Resource::Type::Shape, false, false));

    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    return list;
}

Workspace::ResourceList Workspace::ListUserDefinedEntities() const
{
    return ListResources(Resource::Type::Entity, false, true);
}

QStringList Workspace::ListUserDefinedEntityIds() const
{
    QStringList list;
    for (const auto& resource : mResources)
    {
        if (!resource->IsEntity())
            continue;
        list.append(resource->GetId());
    }
    return list;
}

Workspace::ResourceList Workspace::ListResources(Resource::Type type, bool primitive, bool sort) const
{
    ResourceList list;
    for (const auto& resource : mResources)
    {
        if (resource->IsPrimitive() == primitive &&
            resource->GetType() == type)
        {
            ResourceListItem item;
            item.name     = resource->GetName();
            item.id       = resource->GetId();
            item.resource = resource.get();
            list.push_back(item);
        }
    }
    if (sort)
    {
        std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
            return a.name < b.name;
        });
    }
    return list;
}

Workspace::ResourceList Workspace::ListUserDefinedResources() const
{
    ResourceList ret;

    for (size_t i=0; i<mUserResourceCount; ++i)
    {
        const auto& resource = mResources[i];

        ResourceListItem item;
        item.name = resource->GetName();
        item.id   = resource->GetId();
        item.icon = resource->GetIcon();
        item.resource = resource.get();
        ret.push_back(item);
    }
    return ret;
}

Workspace::ResourceList Workspace::ListCursors() const
{
    ResourceList list;
    ResourceListItem arrow;
    arrow.name = "Arrow Cursor";
    arrow.id   = "_arrow_cursor";
    arrow.resource = FindResourceById("_arrow_cursor");
    list.push_back(arrow);
    ResourceListItem block;
    block.name = "Block Cursor";
    block.id   = "_block_cursor";
    block.resource = FindResourceById("_block_cursor");
    list.push_back(block);
    return list;
}

Workspace::ResourceList Workspace::ListDataFiles() const
{
    return ListResources(Resource::Type::DataFile, false, false);
}

ResourceList Workspace::ListDependencies(const ModelIndexList& indices) const
{
    std::unordered_map<QString, Resource*> resource_map;
    for (size_t i=0; i < mUserResourceCount; ++i)
    {
        auto& res = mResources[i];
        resource_map[res->GetId()] = res.get();
    }

    std::unordered_set<QString> unique_ids;

    for (size_t index : indices)
    {
        const auto& res = mResources[index];
        QStringList deps = res->ListDependencies();

        std::stack<QString> stack;
        for (auto id : deps)
            stack.push(id);

        while (!stack.empty())
        {
            auto top_id = stack.top();
            auto* resource = resource_map[top_id];
            // if it's a primitive resource then we're not going to find it here
            // and there's no need to explore it
            if (resource == nullptr)
            {
                stack.pop();
                continue;
            }
            // if we've already seen this resource we can skip
            // exploring from here.
            if (base::Contains(unique_ids, top_id))
            {
                stack.pop();
                continue;
            }

            unique_ids.insert(top_id);

            deps = resource->ListDependencies();
            for (auto id : deps)
                stack.push(id);
        }
    }

    ResourceList ret;
    for (const auto& id : unique_ids)
    {
        auto* res = resource_map[id];

        ResourceListItem item;
        item.name = res->GetName();
        item.id   = res->GetId();
        item.icon = res->GetIcon();
        item.resource = res;
        ret.push_back(item);
    }
    return ret;
}

ResourceList Workspace::ListResourceUsers(const ModelIndexList& list) const
{
    ResourceList users;

    // The dependency graph goes only one way from user -> dependant.
    // this means that right now to go the other way.
    // in order to make this operation run faster we'd need to track the
    // relationship the other way too.
    // This could be done either when the resource is saved or in the background
    // in the Workspace tick (or something)

    for (size_t i=0; i<mUserResourceCount; ++i)
    {
        // take a resource and find its deps
        const auto& current_res = mResources[i];
        const auto& current_deps = ListDependencies(i); // << warning this is the slow/heavy OP !

        // if the deps include any of the resources listed as args then this
        // current resource is a user.
        for (const auto& dep : current_deps)
        {
            bool found_match = false;

            for (auto index : list)
            {
                if (mResources[index]->GetId() == dep.id)
                {
                    found_match = true;
                    break;
                }
            }
            if (found_match)
            {
                ResourceListItem ret;
                ret.id = current_res->GetId();
                ret.name = current_res->GetName();
                ret.icon = current_res->GetIcon();
                ret.resource = current_res.get();
                users.push_back(ret);
            }
        }
    }
    return users;
}


QStringList Workspace::ListFileResources(const ModelIndexList& indices) const
{
    std::unordered_set<AnyString> uris;

    ResourceTracker tracker(mWorkspaceDir, &uris);

    for (size_t index : indices)
    {
        // this is mutable, but we know the contents do not change.
        // the API isn't exactly a perfect match since it was designed
        // for packing which mutates the resources at one go. It could
        // be refactored into 2 steps, first iterate and transact on
        // the resources and then update the resources.
        const auto& resource  = GetUserDefinedResource(index);
        const_cast<Resource&>(resource).Pack(tracker);
    }

    QStringList list;
    for (auto uri : uris)
        list.append(uri);

    return list;
}

void Workspace::SaveResource(const Resource& resource)
{
    RECURSION_GUARD(this, "ResourceList");

    const auto& id = resource.GetId();
    for (size_t i=0; i<mResources.size(); ++i)
    {
        auto& res = mResources[i];
        if (res->GetId() != id)
            continue;

        res->SetUserProperties(resource.GetUserProperties());
        res->SetProperties(resource.GetProperties());
        res->CopyContent(resource);

        emit ResourceUpdated(mResources[i].get());
        emit dataChanged(index(i, 0), index(i, 1));
        INFO("Saved resource '%1'", resource.GetName());
        NOTE("Saved resource '%1'", resource.GetName());
        return;
    }
    // if we're here no such resource exists yet.
    // Create a new resource and add it to the list of resources.
    beginInsertRows(QModelIndex(), mUserResourceCount, mUserResourceCount);
    // insert at the end of the visible range which is from [0, mUserResourceCount)
    mResources.insert(mResources.begin() + mUserResourceCount, resource.Copy());

    // careful! endInsertRows will trigger the view proxy to re-fetch the contents.
    // make sure to update this property before endInsertRows otherwise
    // we'll hit ASSERT incorrectly in GetUserDefinedMaterial
    mUserResourceCount++;

    endInsertRows();

    auto& back = mResources[mUserResourceCount - 1];
    ASSERT(back->GetId() == resource.GetId());
    ASSERT(back->GetName() == resource.GetName());
    emit ResourceAdded(back.get());

    INFO("Saved new resource '%1'", resource.GetName());
    NOTE("Saved new resource '%1'", resource.GetName());
}

QString Workspace::MapDrawableIdToName(const AnyString& id) const
{
    return MapResourceIdToName(id);
}

QString Workspace::MapMaterialIdToName(const AnyString& id) const
{
    return MapResourceIdToName(id);
}

QString Workspace::MapEntityIdToName(const AnyString &id) const
{
    return MapResourceIdToName(id);
}

QString Workspace::MapResourceIdToName(const AnyString &id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id)
            return resource->GetName();
    }
    return "";
}

bool Workspace::IsValidMaterial(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id &&
            resource->GetType() == Resource::Type::Material)
            return true;
    }
    return false;
}

bool Workspace::IsValidDrawable(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id &&
            (resource->GetType() == Resource::Type::ParticleSystem ||
             resource->GetType() == Resource::Type::Shape ||
             resource->GetType() == Resource::Type::Drawable))
            return true;
    }
    return false;
}

bool Workspace::IsValidTilemap(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id &&  resource->IsTilemap())
            return true;
    }
    return false;
}

bool Workspace::IsValidScript(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id && resource->IsScript())
            return true;
    }
    return false;
}

bool Workspace::IsUserDefinedResource(const AnyString& id) const
{
    for (const auto& res : mResources)
    {
        if (res->GetId() == id)
        {
            return !res->IsPrimitive();
        }
    }
    BUG("No such material was found.");
    return false;
}

bool Workspace::IsValidUI(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id && resource->IsUI())
            return true;
    }
    return false;
}

Resource& Workspace::GetResource(const ModelIndex& index)
{
    ASSERT(index < mResources.size());
    return *mResources[index];
}
Resource& Workspace::GetResource(size_t index)
{
    ASSERT(index < mResources.size());
    return *mResources[index];
}

Resource& Workspace::GetPrimitiveResource(size_t index)
{
    const auto num_primitives = mResources.size() - mUserResourceCount;

    ASSERT(index < num_primitives);
    return *mResources[mUserResourceCount + index];
}

Resource& Workspace::GetUserDefinedResource(size_t index)
{
    ASSERT(index < mUserResourceCount);

    return *mResources[index];
}

Resource* Workspace::FindResourceById(const QString &id)
{
    for (auto& res : mResources)
    {
        if (res->GetId() == id)
            return res.get();
    }
    return nullptr;
}

Resource* Workspace::FindResourceByName(const QString &name, Resource::Type type)
{
    for (auto& res : mResources)
    {
        if (res->GetName() == name && res->GetType() == type)
            return res.get();
    }
    return nullptr;
}

Resource& Workspace::GetResourceByName(const QString& name, Resource::Type type)
{
    for (auto& res : mResources)
    {
        if (res->GetType() == type && res->GetName() == name)
            return *res;
    }
    BUG("No such resource");
}
Resource& Workspace::GetResourceById(const QString& id)
{
    for (auto& res : mResources)
    {
        if (res->GetId() == id)
            return *res;
    }
    BUG("No such resource.");
}


const Resource& Workspace::GetResourceByName(const QString& name, Resource::Type type) const
{
    for (const auto& res : mResources)
    {
        if (res->GetType() == type && res->GetName() == name)
            return *res;
    }
    BUG("No such resource");
}

const Resource* Workspace::FindResourceById(const QString &id) const
{
    for (const auto& res : mResources)
    {
        if (res->GetId() == id)
            return res.get();
    }
    return nullptr;
}
const Resource* Workspace::FindResourceByName(const QString &name, Resource::Type type) const
{
    for (const auto& res : mResources)
    {
        if (res->GetName() == name && res->GetType() == type)
            return res.get();
    }
    return nullptr;
}

const Resource& Workspace::GetResource(const ModelIndex& index) const
{
    ASSERT(index < mResources.size());
    return *mResources[index];
}

const Resource& Workspace::GetUserDefinedResource(size_t index) const
{
    ASSERT(index < mUserResourceCount);
    return *mResources[index];
}
const Resource& Workspace::GetPrimitiveResource(size_t index) const
{
    const auto num_primitives = mResources.size() - mUserResourceCount;

    ASSERT(index < num_primitives);
    return *mResources[mUserResourceCount + index];
}

void Workspace::DeleteResources(const ModelIndexList& list, std::vector<QString>* dead_files)
{
    RECURSION_GUARD(this, "ResourceList");

    std::vector<size_t> indices = list.GetData();

    std::vector<size_t> relatives;
    // scan the list of indices for associated data resources. 
    for (auto i : indices)
    { 
        // for each tilemap resource
        // look for the data resources associated with the map layers.
        // Add any data object IDs to the list of new indices of resources
        // to be deleted additionally.
        const auto& res = mResources[i];
        if (res->IsTilemap())
        {
            const game::TilemapClass* map = nullptr;
            res->GetContent(&map);
            for (unsigned i = 0; i < map->GetNumLayers(); ++i)
            {
                const auto& layer = map->GetLayer(i);
                for (size_t j = 0; j < mUserResourceCount; ++j)
                {
                    const auto& res = mResources[j];
                    if (!res->IsDataFile())
                        continue;

                    const app::DataFile* data = nullptr;
                    res->GetContent(&data);
                    if (data->GetTypeTag() == DataFile::TypeTag::TilemapData &&
                        data->GetOwnerId() == layer.GetId())
                    {
                        relatives.push_back(j);
                        break;
                    }
                }
            }
        }
    }
    // combine the original indices together with the associated
    // resource indices.
    base::AppendVector(indices, relatives);

    std::sort(indices.begin(), indices.end(), std::less<size_t>());
    // remove dupes. dupes could happen if the resource was already
    // in the original indices list and then was added for the second
    // time when scanning resources mentioned in the indices list for
    // associated resources that need to be deleted.
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    // because the high probability of unwanted recursion
    // messing this iteration up (for example by something
    // calling back to this workspace from Resource
    // deletion signal handler and adding a new resource) we
    // must take some special care here.
    // So, therefore first put the resources to be deleted into
    // a separate container while iterating and removing from the
    // removing from the primary list and only then invoke the signal
    // for each resource.
    std::vector<std::unique_ptr<Resource>> graveyard;

    for (int i=0; i<indices.size(); ++i)
    {
        const auto row = indices[i] - i;
        beginRemoveRows(QModelIndex(), row, row);

        auto it = std::begin(mResources);
        std::advance(it, row);
        graveyard.push_back(std::move(*it));
        mResources.erase(it);
        mUserResourceCount--;

        endRemoveRows();
    }
    // invoke a resource deletion signal for each resource now
    // by iterating over the separate container. (avoids broken iteration)
    for (const auto& carcass : graveyard)
    {
        emit ResourceRemoved(carcass.get());
    }

    // script and tilemap layer data resources are special in the sense that
    // they're the only resources where the underlying filesystem data file
    // is actually created by this editor. for everything else, shaders,
    // image files and font files the resources are created by other tools,
    // and we only keep references to those files.
    for (const auto& carcass : graveyard)
    {
        QString dead_file;
        if (carcass->IsScript())
        {
            // for scripts when the script resource is deleted we're actually
            // going to delete the underlying filesystem file as well.
            Script* script = nullptr;
            carcass->GetContent(&script);
            if (dead_files)
                dead_files->push_back(MapFileToFilesystem(script->GetFileURI()));
            else dead_file = MapFileToFilesystem(script->GetFileURI());
        }
        else if (carcass->IsDataFile())
        {
            // data files that link to a tilemap layer are also going to be
            // deleted when the map is deleted. These files would be completely
            // useless without any way to actually use them for anything.
            DataFile* data = nullptr;
            carcass->GetContent(&data);
            if (data->GetTypeTag() == DataFile::TypeTag::TilemapData)
                dead_file = MapFileToFilesystem(data->GetFileURI());
        }
        if (dead_file.isEmpty())
            continue;

        if (!QFile::remove(dead_file))
        {
            ERROR("Failed to delete file. [file='%1']", dead_file);
        }
        else
        {
            INFO("Deleted file '%1'.", dead_file);
        }
    }
}

void Workspace::DeleteResource(const AnyString& id, std::vector<QString>* dead_files)
{
    for (size_t i=0; i<GetNumUserDefinedResources(); ++i)
    {
        const auto& res = GetUserDefinedResource(i);
        if (res.GetId() == id)
        {
            DeleteResources(i, dead_files);
            return;
        }
    }
}

void Workspace::DuplicateResources(const ModelIndexList& list, QModelIndexList* result)
{
    RECURSION_GUARD(this, "ResourceList");

    auto indices = list.GetData();

    std::sort(indices.begin(), indices.end(), std::less<size_t>());

    std::map<const Resource*, size_t> insert_index;
    std::vector<std::unique_ptr<Resource>> dupes;

    for (int i=0; i<indices.size(); ++i)
    {
        const auto row_index = indices[i];
        const auto& src_resource = GetResource(row_index);

        auto cpy_resource = src_resource.Clone();
        cpy_resource->SetName(QString("Copy of %1").arg(src_resource.GetName()));

        if (src_resource.IsTilemap())
        {
            const game::TilemapClass* src_map = nullptr;
            game::TilemapClass* cpy_map = nullptr;
            src_resource.GetContent(&src_map);
            cpy_resource->GetContent(&cpy_map);
            ASSERT(src_map->GetNumLayers() == cpy_map->GetNumLayers());
            for (size_t i=0; i<src_map->GetNumLayers(); ++i)
            {
                const auto& src_layer = src_map->GetLayer(i);
                const auto& src_uri   = src_layer.GetDataUri();
                if (src_uri.empty())
                    continue;
                auto& cpy_layer = cpy_map->GetLayer(i);

                const auto& dst_uri = base::FormatString("ws://data/%1.bin", cpy_layer.GetId());
                const auto& src_file = MapFileToFilesystem(src_uri);
                const auto& dst_file = MapFileToFilesystem(dst_uri);
                const auto [success, error] = CopyFile(src_file, dst_file);
                if (!success)
                {
                    WARN("Failed to duplicate tilemap layer data file. [layer='%1', file='%2', error='%3']",
                         cpy_layer.GetName(), dst_file, error);
                    cpy_layer.ResetDataId();
                    cpy_layer.ResetDataUri();
                }
                else
                {
                    app::DataFile cpy_data;
                    cpy_data.SetFileURI(dst_uri);
                    cpy_data.SetOwnerId(cpy_layer.GetId());
                    cpy_data.SetTypeTag(app::DataFile::TypeTag::TilemapData);
                    const auto& cpy_data_resource_name = toString("%1 Layer Data", cpy_resource->GetName());
                    auto cpy_data_resource = std::make_unique<app::DataResource>(cpy_data, cpy_data_resource_name);
                    insert_index[cpy_data_resource.get()] = row_index;
                    dupes.push_back(std::move(cpy_data_resource));
                    cpy_layer.SetDataId(cpy_data.GetId());
                    cpy_layer.SetDataUri(dst_uri);
                    DEBUG("Duplicated tilemap layer data. [layer='%1', src='%2', dst='%3']",
                          cpy_layer.GetName(), src_file, dst_file);
                }
            }
        }
        insert_index[cpy_resource.get()] = row_index;
        dupes.push_back(std::move(cpy_resource));
    }

    for (size_t i=0; i<dupes.size(); ++i)
    {
        const auto row = insert_index[dupes[i].get()] + i;

        beginInsertRows(QModelIndex(), row, row);

        auto* dupe_ptr = dupes[i].get();
        mResources.insert(mResources.begin() + row, std::move(dupes[i]));
        mUserResourceCount++;
        endInsertRows();

        emit ResourceAdded(dupe_ptr);

        if (result)
            result->push_back(this->index(row, 0));
    }
}

bool Workspace::ExportResourceJson(const ModelIndexList& indices, const QString& filename) const
{
    data::JsonObject json;
    for (size_t index : indices)
    {
        const auto& resource = GetResource(index);
        resource.Serialize(json);

        QJsonObject props;
        resource.SaveProperties(props);
        QJsonDocument docu(props);
        QByteArray bytes = docu.toJson().toBase64();

        const auto& prop_key = resource.GetIdUtf8();
        const auto& prop_val = std::string(bytes.constData(), bytes.size());
        json.Write(prop_key.c_str(), prop_val);
    }

    data::JsonFile file;
    file.SetRootObject(json);
    const auto [success, error] = file.Save(app::ToUtf8(filename));
    if (!success)
        ERROR("Export resource as JSON error '%1'.", error);
    else INFO("Exported %1 resource(s) into '%2'", indices.size(), filename);
    return success;
}

// static
bool Workspace::ImportResourcesFromJson(const QString& filename, std::vector<std::unique_ptr<Resource>>& resources)
{
    data::JsonFile file;
    auto [success, error] = file.Load(ToUtf8(filename));
    if (!success)
    {
        ERROR("Import resource as JSON error '%1'.", error);
        return false;
    }
    data::JsonObject root = file.GetRootObject();

    success = true;
    success = success && LoadMaterials<gfx::MaterialClass>("materials", root, resources);
    success = success && LoadResources<gfx::ParticleEngineClass>("particles", root, resources);
    success = success && LoadResources<gfx::PolygonMeshClass>("shapes", root, resources);
    success = success && LoadResources<game::EntityClass>("entities", root, resources);
    success = success && LoadResources<game::SceneClass>("scenes", root, resources);
    success = success && LoadResources<game::TilemapClass>("tilemaps", root, resources);
    success = success && LoadResources<Script>("scripts", root, resources);
    success = success && LoadResources<DataFile>("data_files", root, resources);
    success = success && LoadResources<audio::GraphClass>("audio_graphs", root, resources);
    success = success && LoadResources<uik::Window>("uis", root, resources);
    DEBUG("Loaded %1 resources from '%2'.", resources.size(), filename);

    // restore the properties.
    for (auto& resource : resources)
    {
        const auto& prop_key = resource->GetIdUtf8();
        std::string prop_val;
        if (!root.Read(prop_key.c_str(), &prop_val)) {
            WARN("No properties found for resource '%1'.", prop_key);
            continue;
        }
        if (prop_val.empty())
            continue;

        const auto& bytes = QByteArray::fromBase64(QByteArray(prop_val.c_str(), prop_val.size()));
        QJsonParseError error;
        const auto& docu = QJsonDocument::fromJson(bytes, &error);
        if (docu.isNull()) {
            WARN("Json parse error when parsing resource '%1' properties.", prop_key);
            continue;
        }
        resource->LoadProperties(docu.object());
    }
    return success;
}

void Workspace::ImportFilesAsResource(const QStringList& files)
{
    // todo: given a collection of file names some of the files
    // could belong together in a sprite/texture animation sequence.
    // for example if we have "bird_0.png", "bird_1.png", ... "bird_N.png" 
    // we could assume that these are all material animation frames
    // and should go together into one material.
    // On the other hand there are also cases like with tile sets that have
    // tiles named tile1.png, tile2.png ... and these should be separated.
    // not sure how to deal with this smartly. 

    for (const QString& file : files)
    {
        const QFileInfo info(file);
        if (!info.isFile())
        {
            WARN("File is not actually a file. [file='%1']", file);
            continue;
        }
        const auto& name   = info.baseName();
        const auto& suffix = info.completeSuffix().toUpper();
        const auto& uri = MapFileToWorkspace(file);
        if (suffix == "LUA")
        {
            Script script;
            script.SetFileURI(ToUtf8(uri));
            script.SetTypeTag(Script::TypeTag::ScriptData);
            ScriptResource res(script, name);
            SaveResource(res);
            INFO("Imported new script file '%1' based on file '%2'", name, info.filePath());
        }
        else if (suffix == "JPEG" || suffix == "JPG" || suffix == "PNG" || suffix == "TGA" || suffix == "BMP")
        {
            gfx::TextureFileSource texture;
            texture.SetFileName(ToUtf8(uri));
            texture.SetName(ToUtf8(name));

            gfx::MaterialClass klass(gfx::MaterialClass::Type::Texture, base::RandomString(10));
            klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            klass.SetTexture(texture.Copy());
            klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Default);
            klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter ::Default);
            klass.SetActiveTextureMap(klass.GetTextureMap(0)->GetId());

            MaterialResource res(klass, name);
            SaveResource(res);
            INFO("Imported new material '%1' based on image file '%2'", name, info.filePath());
        }
        else if (suffix == "MP3" || suffix == "WAV" || suffix == "FLAC" || suffix == "OGG")
        {
            audio::GraphClass klass(ToUtf8(name));
            audio::GraphClass::Element element;
            element.name = ToUtf8(name);
            element.id   = base::RandomString(10);
            element.type = "FileSource";
            element.args["file"] = ToUtf8(uri);
            klass.SetGraphOutputElementId(element.id);
            klass.SetGraphOutputElementPort("out");
            klass.AddElement(std::move(element));
            AudioResource res(klass, name);
            SaveResource(res);
            INFO("Imported new audio graph '%1' based on file '%2'", name, info.filePath());
        }
        else
        {
            DataFile data;
            data.SetFileURI(uri);
            data.SetTypeTag(DataFile::TypeTag::External);
            DataResource res(data, name);
            SaveResource(res);
            INFO("Imported new data file '%1' based on file '%2'", name, info.filePath());
        }
        DEBUG("Mapping imported file '%1' => '%2'", file, uri);
    }
}

void Workspace::Tick()
{

}

bool Workspace::ExportResourceArchive(const std::vector<const Resource*>& resources, const ExportOptions& options)
{
    ZipArchiveExporter zip(options.zip_file, mWorkspaceDir);
    if (!zip.Open())
        return false;

    // unfortunately we need to make copies of the resources
    // since packaging might modify the resources yet the
    // original resources should not be changed.
    // todo: perhaps rethink this.. what other ways would there be ?
    // constraints:
    //  - don't wan to duplicate the serialization/deserialization/JSON writing
    //  - should not know details of resources (materials, drawables etc)
    //  - material depends on resource packer, resource packer should not then
    //    know about material
    std::vector<std::unique_ptr<Resource>> mutable_copies;
    for (const auto* resource : resources)
    {
        ASSERT(!resource->IsPrimitive());
        mutable_copies.push_back(resource->Copy());
    }

    // Partition the resources such that the data objects come in first.
    // This is done because some resources such as tilemaps refer to the
    // data resources by URI and in order for the URI remapping to work
    // the packer must have packed the data object before packing the
    // tilemap object.
    std::partition(mutable_copies.begin(), mutable_copies.end(),
        [](const auto& resource) {
            return resource->IsDataFile();
        });

    QJsonObject properties;
    data::JsonObject content;
    for (auto& resource: mutable_copies)
    {
        if (!resource->Pack(zip))
        {
            ERROR("Resource packing failed. [name='%1']", resource->GetName());
            return false;
        }
        resource->Serialize(content);
        resource->SaveProperties(properties);
    }
    QJsonDocument doc(properties);
    zip.WriteText(content.ToString(), "content.json");
    zip.WriteBytes(doc.toJson(), "properties.json");
    zip.Close();
    return true;
}

bool Workspace::ImportResourceArchive(ZipArchive& zip)
{
    const auto& sub_folder = zip.GetImportSubFolderName();
    const auto& name_prefix = zip.GetResourceNamePrefix();
    ZipArchiveImporter importer(zip.mZipFile, sub_folder, mWorkspaceDir, zip.mZip);

    // it seems a bit funny here to be calling "pack" when actually we're
    // unpacking but the implementation of zip based resource packer is
    // such that data is copied (packed) from the zip and into the workspace
    for (size_t i=0; i<zip.mResources.size(); ++i)
    {
        if (zip.IsIndexIgnored(i))
            continue;
        auto& resource = zip.mResources[i];
        if (!resource->Pack(importer))
        {
            ERROR("Resource import failed. [resource='%1']", resource->GetName());
            return false;
        }
        const auto& name = resource->GetName();
        resource->SetName(name_prefix + name);
    }

    for (size_t i=0; i<zip.mResources.size(); ++i)
    {
        if (zip.IsIndexIgnored(i))
            continue;
        auto& resource = zip.mResources[i];
        SaveResource(*resource);
    }
    return true;
}

bool Workspace::BuildReleasePackage(const std::vector<const Resource*>& resources,
                                    const ContentPackingOptions& options, WorkspaceAsyncWorkObserver* observer)
{
    const QString& outdir = JoinPath(options.directory, options.package_name);
    if (!MakePath(outdir))
    {
        ERROR("Failed to create output directory. [dir='%1']", outdir);
        return false;
    }

    // unfortunately we need to make copies of the resources
    // since packaging might modify the resources yet the
    // original resources should not be changed.
    // todo: perhaps rethink this.. what other ways would there be ?
    // constraints:
    //  - don't wan to duplicate the serialization/deserialization/JSON writing
    //  - should not know details of resources (materials, drawables etc)
    //  - material depends on resource packer, resource packer should not then
    //    know about material
    std::vector<std::unique_ptr<Resource>> mutable_copies;
    for (const auto* resource : resources)
    {
        ASSERT(!resource->IsPrimitive());
        mutable_copies.push_back(resource->Copy());
    }

    // Partition the resources such that the data objects come in first.
    // This is done because some resources such as tilemaps refer to the
    // data resources by URI and in order for the URI remapping to work
    // the packer must have packed the data object before packing the
    // tilemap object.
    std::partition(mutable_copies.begin(), mutable_copies.end(),
        [](const auto& resource) {
            return resource->IsDataFile();
        });

    DEBUG("Max texture size. [width=%1, height=%2]", options.max_texture_width, options.max_texture_height);
    DEBUG("Pack size. [width=%1, height=%2]", options.texture_pack_width, options.texture_pack_height);
    DEBUG("Pack flags. [resize=%1, combine=%2]", options.resize_textures, options.combine_textures);

    GfxTexturePacker texture_packer(outdir,
        options.max_texture_width,
        options.max_texture_height,
        options.texture_pack_width,
        options.texture_pack_height,
        options.texture_padding,
        options.resize_textures,
        options.combine_textures);

    // collect the resources in the packer.
    for (int i=0; i<mutable_copies.size(); ++i)
    {
        const auto& resource = mutable_copies[i];
        if (resource->IsMaterial())
        {
            // todo: maybe move to Resource interface ?
            const gfx::MaterialClass* material = nullptr;
            resource->GetContent(&material);
            material->BeginPacking(&texture_packer);
        }

        if (observer)
        {
            observer->EnqueueUpdate("Collecting resources...", mutable_copies.size(), i);
            observer->ApplyPendingUpdates();
        }
    }

    WorkspaceResourcePacker file_packer(outdir, mWorkspaceDir);

    unsigned errors = 0;

    // copy some file based content around.
    // todo: this would also need some kind of file name collision
    // resolution and mapping functionality.
    for (int i=0; i<mutable_copies.size(); ++i)
    {
        if (!mutable_copies[i]->Pack(file_packer))
        {
            ERROR("Resource packing failed. [name='%1']", mutable_copies[i]->GetName());
            ++errors;
        }
    }

    texture_packer.PackTextures([observer](const std::string& action, int step, int max) {
        if (observer)
        {
            observer->EnqueueUpdate(action, max, step);
            observer->ApplyPendingUpdates();
        }
    }, file_packer);

    for (int i=0; i<mutable_copies.size(); ++i)
    {
        const auto& resource = mutable_copies[i];
        if (resource->IsMaterial())
        {
            // todo: maybe move to resource interface ?
            gfx::MaterialClass* material = nullptr;
            resource->GetContent(&material);
            material->FinishPacking(&texture_packer);
        }

        if (observer)
        {
            observer->EnqueueUpdate("Updating resource references...", mutable_copies.size(), i);
            observer->ApplyPendingUpdates();
        }
    }

    if (!mSettings.debug_font.isEmpty())
    {
        // todo: should change the font URI.
        // but right now this still also works since there's a hack for this
        // in the loader in engine/ (Also same app:// thing applies to the UI style files)
        file_packer.CopyFile(app::ToUtf8(mSettings.debug_font), "fonts/");
    }

    if (!mSettings.loading_font.isEmpty())
    {
        file_packer.CopyFile(app::ToUtf8(mSettings.loading_font), "fonts/");
    }

    // write content file ?
    if (options.write_content_file)
    {
        if (observer)
        {
            observer->EnqueueUpdate("Writing content JSON file...", 0, 0);
            observer->ApplyPendingUpdates();
        }

        // filename of the JSON based descriptor that contains all the
        // resource definitions.
        const auto &json_filename = JoinPath(outdir, "content.json");

        QFile json_file;
        json_file.setFileName(json_filename);
        json_file.open(QIODevice::WriteOnly);
        if (!json_file.isOpen())
        {
            ERROR("Failed to open content JSON file. [file='%1', error='%2']", json_filename, json_file.error());
            ++errors;
        }

        // finally serialize
        data::JsonObject json;
        json.Write("json_version", 1);
        json.Write("made_with_app", APP_TITLE);
        json.Write("made_with_ver", APP_VERSION);
        for (const auto &resource : mutable_copies)
        {
            resource->Serialize(json);
        }

        const auto &str = json.ToString();
        if (json_file.write(&str[0], str.size()) == -1)
        {
            ERROR("Failed to write content JSON file. [file='%1', error='%2']", json_filename, json_file.error());
            ++errors;
        }
        json_file.flush();
        json_file.close();
    }

    // resolves the path.
    const QFileInfo engine_dll(mSettings.GetApplicationLibrary());
    QString engine_name = engine_dll.fileName();
    if (engine_name.startsWith("lib"))
        engine_name.remove(0, 3);
    if (engine_name.endsWith(".so"))
        engine_name.chop(3);
    else if (engine_name.endsWith(".dll"))
        engine_name.chop(4);

    // write config file?
    if (options.write_config_file)
    {
        if (observer)
        {
            observer->EnqueueUpdate("Writing config JSON file...", 0, 0);
            observer->ApplyPendingUpdates();
        }

        nlohmann:: json json;
        base::JsonWrite(json, "json_version",   1);
        base::JsonWrite(json, "made_with_app",  APP_TITLE);
        base::JsonWrite(json, "made_with_ver",  APP_VERSION);
        base::JsonWrite(json["config"], "red_size",     8);
        base::JsonWrite(json["config"], "green_size",   8);
        base::JsonWrite(json["config"], "blue_size",    8);
        base::JsonWrite(json["config"], "alpha_size",   8);
        base::JsonWrite(json["config"], "stencil_size", 8);
        base::JsonWrite(json["config"], "depth_size",   24);
        base::JsonWrite(json["config"], "srgb", mSettings.config_srgb);
        if (mSettings.multisample_sample_count == 0)
            base::JsonWrite(json["config"], "sampling", "None");
        else if (mSettings.multisample_sample_count == 4)
            base::JsonWrite(json["config"], "sampling", "MSAA4");
        else if (mSettings.multisample_sample_count == 8)
            base::JsonWrite(json["config"], "sampling", "MSAA8");
        else if (mSettings.multisample_sample_count == 16)
            base::JsonWrite(json["config"], "sampling", "MSAA16");
        base::JsonWrite(json["window"], "width",      mSettings.window_width);
        base::JsonWrite(json["window"], "height",     mSettings.window_height);
        base::JsonWrite(json["window"], "can_resize", mSettings.window_can_resize);
        base::JsonWrite(json["window"], "has_border", mSettings.window_has_border);
        base::JsonWrite(json["window"], "vsync",      mSettings.window_vsync);
        base::JsonWrite(json["window"], "cursor",     mSettings.window_cursor);
        base::JsonWrite(json["window"], "grab_mouse", mSettings.grab_mouse);
        base::JsonWrite(json["window"], "save_geometry", mSettings.save_window_geometry);
        if (mSettings.window_mode == ProjectSettings::WindowMode::Windowed)
            base::JsonWrite(json["window"], "set_fullscreen", false);
        else if (mSettings.window_mode == ProjectSettings::WindowMode::Fullscreen)
            base::JsonWrite(json["window"], "set_fullscreen", true);

        base::JsonWrite(json["application"], "library", ToUtf8(engine_name));
        base::JsonWrite(json["application"], "identifier", ToUtf8(mSettings.application_identifier));
        base::JsonWrite(json["application"], "title",    ToUtf8(mSettings.application_name));
        base::JsonWrite(json["application"], "version",  ToUtf8(mSettings.application_version));
        base::JsonWrite(json["application"], "content", ToUtf8(options.package_name));
        base::JsonWrite(json["application"], "game_script", ToUtf8(mSettings.game_script));
        base::JsonWrite(json["desktop"], "audio_io_strategy", mSettings.desktop_audio_io_strategy);
        base::JsonWrite(json["loading_screen"], "font", ToUtf8(mSettings.loading_font));
        base::JsonWrite(json["debug"], "font", ToUtf8(mSettings.debug_font));
        base::JsonWrite(json["debug"], "show_msg", mSettings.debug_show_msg);
        base::JsonWrite(json["debug"], "show_fps", mSettings.debug_show_fps);
        base::JsonWrite(json["debug"], "draw", mSettings.debug_draw);
        base::JsonWrite(json["debug"], "print_fps", mSettings.debug_print_fps);
        base::JsonWrite(json["logging"], "debug", mSettings.log_debug);
        base::JsonWrite(json["logging"], "warn", mSettings.log_warn);
        base::JsonWrite(json["logging"], "info", mSettings.log_info);
        base::JsonWrite(json["logging"], "error", mSettings.log_error);
        base::JsonWrite(json["html5"], "canvas_width", mSettings.canvas_width);
        base::JsonWrite(json["html5"], "canvas_height", mSettings.canvas_height);
        base::JsonWrite(json["html5"], "canvas_mode", mSettings.canvas_mode);
        base::JsonWrite(json["html5"], "canvas_fs_strategy", mSettings.canvas_fs_strategy);
        base::JsonWrite(json["html5"], "webgl_power_pref", mSettings.webgl_power_preference);
        base::JsonWrite(json["html5"], "webgl_antialias", mSettings.webgl_antialias);
        base::JsonWrite(json["html5"], "developer_ui", mSettings.html5_developer_ui);
        base::JsonWrite(json["html5"], "pointer_lock", mSettings.html5_pointer_lock);
        base::JsonWrite(json["wasm"], "audio_io_strategy", mSettings.wasm_audio_io_strategy);
        base::JsonWrite(json["engine"], "default_min_filter", mSettings.default_min_filter);
        base::JsonWrite(json["engine"], "default_mag_filter", mSettings.default_mag_filter);
        base::JsonWrite(json["engine"], "ticks_per_second",   (float)mSettings.ticks_per_second);
        base::JsonWrite(json["engine"], "updates_per_second", (float)mSettings.updates_per_second);
        base::JsonWrite(json["engine"], "clear_color", ToGfx(mSettings.clear_color));
        base::JsonWrite(json["physics"], "enabled", mSettings.enable_physics);
        base::JsonWrite(json["physics"], "num_velocity_iterations", mSettings.num_velocity_iterations);
        base::JsonWrite(json["physics"], "num_position_iterations", mSettings.num_position_iterations);
        base::JsonWrite(json["physics"], "gravity", mSettings.physics_gravity);
        base::JsonWrite(json["physics"], "scale",   mSettings.physics_scale);
        base::JsonWrite(json["mouse_cursor"], "material", ToUtf8(mSettings.mouse_pointer_material));
        base::JsonWrite(json["mouse_cursor"], "drawable", ToUtf8(mSettings.mouse_pointer_drawable));
        base::JsonWrite(json["mouse_cursor"], "show", mSettings.mouse_pointer_visible);
        base::JsonWrite(json["mouse_cursor"], "hotspot", mSettings.mouse_pointer_hotspot);
        base::JsonWrite(json["mouse_cursor"], "size", mSettings.mouse_pointer_size);
        base::JsonWrite(json["mouse_cursor"], "units", mSettings.mouse_pointer_units);
        base::JsonWrite(json["audio"], "channels", mSettings.audio_channels);
        base::JsonWrite(json["audio"], "sample_rate", mSettings.audio_sample_rate);
        base::JsonWrite(json["audio"], "sample_type", mSettings.audio_sample_type);
        base::JsonWrite(json["audio"], "buffer_size", mSettings.audio_buffer_size);
        base::JsonWrite(json["audio"], "pcm_caching", mSettings.enable_audio_pcm_caching);

        // This is a lazy workaround for the fact that the unit tests don't set up the
        // game script properly as a script object in the workspace. This means there's
        // no proper script copying/URI mapping taking place for the game script.
        // So we check here for the real workspace to see if there's a mapping and if so
        // then replace the original game script value with the mapped script URI.
        if (file_packer.HasMapping(mSettings.game_script))
        {
            base::JsonWrite(json["application"], "game_script", file_packer.MapUri(mSettings.game_script));
        }

        const auto& json_filename = JoinPath(options.directory, "config.json");
        QFile json_file;
        json_file.setFileName(json_filename);
        json_file.open(QIODevice::WriteOnly);
        if (!json_file.isOpen())
        {
            ERROR("Failed to open config JSON file. [file='%1', error='%2']", json_filename, json_file.error());
            ++errors;
        }
        else
        {
            const auto& str = json.dump(2);
            if (json_file.write(&str[0], str.size()) == -1)
            {
                ERROR("Failed to write config JSON file. [file='%1', error='%2']", json_filename, json_file.error());
                ++errors;
            }
            json_file.flush();
            json_file.close();
        }
    }

    if (options.write_html5_content_fs_image)
    {
        if (observer)
        {
            observer->EnqueueUpdate("Generating HTML5 filesystem image...", 0, 0);
            observer->ApplyPendingUpdates();
        }

        QString package_script = app::JoinPath(options.emsdk_path, "/upstream/emscripten/tools/file_packager.py");

        if (!FileExists(options.python_executable))
        {
            ERROR("Python executable was not found. [python='%1']", options.python_executable);
            ++errors;
        }
        else if (!FileExists(package_script))
        {
            ERROR("Emscripten filesystem package script was not found. [script='%1']", package_script);
            ++errors;
        }
        else
        {
            const char* filesystem_image_name = "FILESYSTEM";

            QStringList args;
            args << package_script;
            args << filesystem_image_name;
            args << "--preload";
            args << options.package_name;
            args << "config.json";
            args << QString("--js-output=%1.js").arg(filesystem_image_name);

            DEBUG("Generating HTML5 filesystem image. [emsdk='%1', python='%2']", options.emsdk_path,
                  options.python_executable);
            DEBUG("%1", args.join(" "));

            Process::Error error_code = Process::Error::None;
            QStringList stdout_buffer;
            QStringList stderr_buffer;
            if (!Process::RunAndCapture(options.python_executable,
                                        options.directory, args,
                                        &stdout_buffer,
                                        &stderr_buffer,
                                        &error_code))
            {
                ERROR("Building HTML5/WASM filesystem image failed. [error='%1', python='%2', script='%2']",
                      error_code, options.python_executable, package_script);
                ++errors;
            }
        }
    }

    if (options.copy_html5_files)
    {
        // these should be in the dist/ folder and are the
        // built by the emscripten build in emscripten/
        struct HTML5_Engine_File {
            QString name;
            bool mandatory;
        };

        const HTML5_Engine_File files[] = {
            {"GameEngine.js", true},
            {"GameEngine.wasm", true},
            // the JS Web worker glue code. this file is only produced by Emscripten
            // if the threaded WASM build is being used.
            {"GameEngine.worker.js", false},
            // this is just a helper file for convenience
            {"http-server.py", false},
            // this is needed for the trace file save
            {"FileSaver.js", false},
            // This is just for version information. The file is
            // produced by the Emscripten build using CMake-git-version-tracking
            {"GameEngineVersion.txt", false}
        };
        for (const auto& file : files)
        {
            const auto& src = app::GetAppInstFilePath("html5/" + file.name);
            const auto& dst = app::JoinPath(options.directory, file.name);
            auto[success, error] = app::CopyFile(src, dst);
            if (!success)
            {
                if (file.mandatory)
                {
                    ERROR("Failed to copy game engine file. [src='%1', dst='%2', error='%3']", src, dst, error);
                    ++errors;
                }
                else
                {
                    WARN("Failed to copy game engine file. [src='%1', dst='%2', error='%3']", src, dst, error);
                    WARN("This file is not absolutely essential so you may proceed,");
                    WARN("But there might be limited functionality.");
                }
            }
        }
    }
    if (options.write_html5_game_file)
    {
        const QString files[] = {
           "game.html"
        };
        for (int i=0; i<1; ++i)
        {
            const auto& src = app::GetAppInstFilePath(files[i]);
            const auto& dst = app::JoinPath(options.directory, files[i]);
            auto[success, error] = app::CopyFile(src, dst);
            if (!success)
            {
                ERROR("Failed to copy game html file. [src='1%', dst='%2', error='%3']", src, dst, error);
                ++errors;
            }
        }
    }

    // Copy game main executable/engine library
    if (options.copy_native_files)
    {
        // TODO: fix this name stuff here, only take it from the options.
        // the name stuff is duplicated in the package complete dialog
        // when trying to launch the native game.

        QString src_exec = "GameMain";
        QString dst_exec = mSettings.application_name;
        if (dst_exec.isEmpty())
            dst_exec = "GameMain";
#if defined(WINDOWS_OS)
        src_exec.append(".exe");
        dst_exec.append(".exe");
        engine_name.append(".dll");
#elif defined(LINUX_OS)
        engine_name.prepend("lib");
        engine_name.append(".so");
#endif
        dst_exec = app::JoinPath(options.directory, dst_exec);
        auto [success, error ] = app::CopyFile(src_exec, dst_exec);
        if (!success)
        {
            ERROR("Failed to copy game executable. [src='%1', dst='%2', error='%3']", src_exec, dst_exec, error);
            ++errors;
        }
        const auto& src_lib = MapFileToFilesystem(mSettings.GetApplicationLibrary());
        const auto& dst_lib = app::JoinPath(options.directory, engine_name);
        std::tie(success, error) = app::CopyFile(src_lib, dst_lib);
        if (!success)
        {
            ERROR("Failed to copy game engine library. [src='%1', dst='%2', error='%3']", src_lib, dst_lib, error);
            ++errors;
        }
    }

    if (const auto total_errors = errors + texture_packer.GetNumErrors() + file_packer.GetNumErrors())
    {
        WARN("Resource packing completed with errors (%1).", total_errors);
        WARN("Please see the log file for details.");
        return false;
    }

    INFO("Packed %1 resource(s) into '%2' successfully.", resources.size(), options.directory);
    return true;
}

void Workspace::UpdateResource(const Resource* resource)
{
    SaveResource(*resource);
}

void Workspace::UpdateUserProperty(const QString& name, const QVariant& data)
{
    if (!data.isValid())
    {
        ERROR("User property is not valid!. [key='%1']", name);
        return;
    }
    const auto& prev = GetUserVariantProperty(name);
    if (prev.isValid())
    {
        if (data.type() != prev.type())
        {
            DEBUG("User property has changed type on property update! [key='%1', prev='%2', next='%3']",
                 name, prev.type(), data.type());
            // ok, let it pass
            //return;
        }
    }

    mUserProperties[name] = data;
    DEBUG("Updated user property. [key='%1', type=%2]", name, data.type());

}

void WorkspaceProxy::DebugPrint() const
{
    DEBUG("Sorted resource order:");

    for (int i=0; i<rowCount(); ++i)
    {
        const auto foo_index = this->index(i, 0);
        const auto src_index = this->mapToSource(foo_index);
        const auto& res = mWorkspace->GetResource(src_index);
        DEBUG("%1 %2", res.GetType(), res.GetName());
    }

    DEBUG("");
}

bool WorkspaceProxy::filterAcceptsRow(int row, const QModelIndex& parent) const
{
    if (!mWorkspace)
        return false;
    const auto& resource = mWorkspace->GetUserDefinedResource(row);
    if (!mBits.test(resource.GetType()))
        return false;
    if (mFilterString.isEmpty())
        return true;

    bool accepted = false;

    QStringList filter_tokens = mFilterString.split(" " , Qt::SplitBehaviorFlags::SkipEmptyParts);
    for (auto filter_token : filter_tokens)
    {
        if (filter_token.startsWith("#"))
        {
            filter_token = filter_token.remove(0, 1);
            if (resource.HasTag(filter_token))
                return true;
        }
        else
        {
            const auto& name = resource.GetName();
            if (name.contains(filter_token, Qt::CaseInsensitive))
                return true;
        }
    }
    return false;
}

void WorkspaceProxy::sort(int column, Qt::SortOrder order)
{
    DEBUG("Sort workspace resources. [column=%1, order=%2]", column, order);
    QSortFilterProxyModel::sort(column, order);
}

bool WorkspaceProxy::lessThan(const QModelIndex& lhs, const QModelIndex& rhs) const
{
    const auto& lhs_res = mWorkspace->GetResource(lhs);
    const auto& rhs_res = mWorkspace->GetResource(rhs);

    const auto& lhs_type_val = toString(lhs_res.GetType());
    const auto& rhs_type_val = toString(rhs_res.GetType());
    const auto& lhs_name = lhs_res.GetName();
    const auto& rhs_name = rhs_res.GetName();

    if (lhs.column() == 0 && rhs.column() == 0)
    {
        if (lhs_type_val < rhs_type_val)
            return true;
        else if (lhs_type_val == rhs_type_val)
            return lhs_name < rhs_name;
        return false;
    }
    else if (lhs.column() == 1 && rhs.column() == 1)
    {
        if (lhs_name < rhs_name)
            return true;
        else if (lhs_name == rhs_name)
            return lhs_type_val < rhs_type_val;
        return false;
    }
    else BUG("Unknown sorting column combination!");
    return false;
}

} // namespace


