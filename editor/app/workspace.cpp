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

#define LOGTAG "workspace"

#include "config.h"

#include "warnpush.h"
#  include <boost/algorithm/string/erase.hpp>
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
#include "warnpop.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <set>
#include <functional>

#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/utility.h"
#include "editor/app/format.h"
#include "editor/app/packing.h"
#include "editor/app/buffer.h"
#include "editor/app/process.h"
#include "graphics/resource.h"
#include "graphics/color4f.h"
#include "engine/ui.h"
#include "engine/data.h"
#include "data/json.h"
#include "base/json.h"

namespace {

gfx::Color4f ToGfx(const QColor& color)
{
    const float a  = color.alphaF();
    const float r  = color.redF();
    const float g  = color.greenF();
    const float b  = color.blueF();
    return gfx::Color4f(r, g, b, a);
}

QString GetAppDir()
{
    static const auto& dir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
#if defined(POSIX_OS)
    return dir + "/";
#elif defined(WINDOWS_OS)
    return dir + "\\";
#else
# error unimplemented function
#endif
    return dir;
}

QString FixWorkspacePath(QString path)
{
    path = app::CleanPath(path);
#if defined(POSIX_OS)
    if (!path.endsWith("/"))
        path.append("/");
#elif defined(WINDOWS_OS)
    if (!path.endsWith("\\"))
        path.append("\\");
#endif
    return path;
}

class ResourcePacker : public gfx::Packer
{
public:
    using ObjectHandle = gfx::Packer::ObjectHandle;

    ResourcePacker(const QString& outdir,
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

    virtual void PackShader(ObjectHandle instance, const std::string& file) override
    {
        // todo: the shader type/path (es2 or 3)
        mShaderMap[instance] = CopyFile(file, "shaders/es2");
    }
    virtual void PackTexture(ObjectHandle instance, const std::string& file) override
    {
        mTextureMap[instance].file = file;
    }
    virtual void SetTextureBox(ObjectHandle instance, const gfx::FRect& box) override
    {
        mTextureMap[instance].rect = box;
    }
    virtual void SetTextureFlag(ObjectHandle instance, gfx::Packer::TextureFlags flags, bool on_off) override
    {
        if (flags == gfx::Packer::TextureFlags::CanCombine)
            mTextureMap[instance].can_be_combined = on_off;
        else if (flags == gfx::Packer::TextureFlags::AllowedToPack)
            mTextureMap[instance].allowed_to_combine = on_off;
        else if (flags == gfx::Packer::TextureFlags::AllowedToResize)
            mTextureMap[instance].allowed_to_resize = on_off;
        else BUG("Unhandled texture packing flag.");
    }
    virtual void PackFont(ObjectHandle instance, const std::string& file) override
    {
        mFontMap[instance] = CopyFile(file, "fonts");
    }

    virtual std::string GetPackedShaderId(ObjectHandle instance) const override
    {
        auto it = mShaderMap.find(instance);
        ASSERT(it != std::end(mShaderMap));
        return it->second;
    }
    virtual std::string GetPackedTextureId(ObjectHandle instance) const override
    {
        auto it = mTextureMap.find(instance);
        ASSERT(it != std::end(mTextureMap));
        return it->second.file;
    }
    virtual std::string GetPackedFontId(ObjectHandle instance) const override
    {
        auto it = mFontMap.find(instance);
        ASSERT(it != std::end(mFontMap));
        return it->second;
    }
    virtual gfx::FRect GetPackedTextureBox(ObjectHandle instance) const override
    {
        auto it = mTextureMap.find(instance);
        ASSERT(it != std::end(mTextureMap));
        return it->second.rect;
    }

    using TexturePackingProgressCallback = std::function<void (std::string, int, int)>;

    void PackTextures(TexturePackingProgressCallback  progress)
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
            QString  texture_file;
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
                self.texture_file = app::FromUtf8(CopyFile(app::ToUtf8(img_file), "textures", img_name));
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
                    gen.texture_file = app::FromUtf8(CopyFile(app::ToUtf8(file), "textures"));
                    gen.width = 1.0f;
                    gen.height = 1.0f;
                    gen.xpos = 0.0f;
                    gen.ypos = 0.0f;
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
                    } else
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
                    gen.texture_file = QString("pck://textures/%1").arg(name);
                    gen.width = (float) width / pack_width;
                    gen.height = (float) height / pack_height;
                    gen.xpos = (float) xpos / pack_width;
                    gen.ypos = (float) ypos / pack_height;
                    relocation_map[rc.cookie] = gen;
                    DEBUG("New image packing entry. [id='%1', dst='%2']", rc.cookie, gen.texture_file);
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

            tex.file = app::ToUtf8(relocation.texture_file);
            tex.rect = gfx::FRect(relocation.xpos + original_rect_x * relocation.width,
                                  relocation.ypos + original_rect_y * relocation.height,
                                  relocation.width * original_rect_width,
                                  relocation.height * original_rect_height);
        }
    }


    size_t GetNumErrors() const
    { return mNumErrors; }
    size_t GetNumFilesCopied() const
    { return mNumFilesCopied; }

    std::string CopyFile(const std::string& file, const QString& where, const QString& name = "")
    {
        auto it = mResourceMap.find(file);
        if (it != std::end(mResourceMap))
        {
            DEBUG("Skipping duplicate file copy. [file='%1']", file);
            return it->second;
        }

        if (!app::MakePath(app::JoinPath(kOutDir, where)))
        {
            ERROR("Failed to create directory. [dir='%1/%2']", kOutDir, where);
            mNumErrors++;
            // todo: what to return on error ?
            return "";
        }

        // this will actually resolve the file path.
        // using the resolution scheme in Workspace.
        const QFileInfo src_info(app::FromUtf8(file));
        if (!src_info.exists())
        {
            ERROR("Could not find file. [file='%1']", file);
            mNumErrors++;
            // todo: what to return on error ?
            return "";
        }

        const QString& src_file = src_info.absoluteFilePath(); // resolved path.
        const QString& src_name = src_info.fileName();
        QString dst_name = name;
        if (dst_name.isEmpty())
            dst_name = src_name;
        QString dst_file = app::JoinPath(kOutDir, app::JoinPath(where, dst_name));
        // try to generate a different name for the file when a file
        // by the same name already exists.
        unsigned attempt = 0;
        while (true)
        {
            const QFileInfo dst_info(dst_file);
            // if there's no file by this name we're good to go
            if (!dst_info.exists())
                break;
            // if the destination file exists *from before* we're
            // going to overwrite it. The user should have been confirmed
            // for this and it should be fine at this point.
            // So only try to resolve a name collision if we're trying to
            // write an output file by the same name multiple times
            if (mFileNames.find(dst_file) == mFileNames.end())
                break;
            // generate a new name.
            dst_name = QString("%1_%2").arg(attempt).arg(src_name);
            dst_file = app::JoinPath(kOutDir, app::JoinPath(where, dst_name));
            attempt++;
        }
        CopyFileBuffer(src_file, dst_file);
        // keep track of which files we wrote.
        mFileNames.insert(dst_file);

        // generate the resource identifier
        const auto& pckid = app::ToUtf8(QString("pck://%1/%2").arg(where).arg(dst_name));

        mResourceMap[file] = pckid;
        return pckid;
    }
private:
    void CopyFileBuffer(const QString& src, const QString& dst)
    {
        // if src equals dst then we can actually skip the copy, no?
        if (src == dst)
        {
            DEBUG("Skipping copy of file onto itself. [src='%1', dst='%2']", src, dst);
            return;
        }
        auto [success, error] = app::CopyFile(src, dst);
        if (!success)
        {
            ERROR("Failed to copy file. [src='%1', dst='%2' error=%3]", src, dst, error);
            mNumErrors++;
            return;
        }
        mNumFilesCopied++;
        DEBUG("File copy done. [src='%1', dst='%2']", src, dst);
    }
private:
    const QString kOutDir;
    const unsigned kMaxTextureHeight = 0;
    const unsigned kMaxTextureWidth = 0;
    const unsigned kTexturePackWidth = 0;
    const unsigned kTexturePackHeight = 0;
    const unsigned kTexturePadding = 0;
    const bool kResizeLargeTextures = true;
    const bool kPackSmallTextures = true;
    std::size_t mNumErrors = 0;
    std::size_t mNumFilesCopied = 0;

    // maps from object to shader.
    std::unordered_map<ObjectHandle, std::string> mShaderMap;
    // maps from object to font.
    std::unordered_map<ObjectHandle, std::string> mFontMap;

    struct TextureSource {
        std::string file;
        gfx::FRect  rect;
        bool can_be_combined = true;
        bool allowed_to_resize = true;
        bool allowed_to_combine = true;
    };
    std::unordered_map<ObjectHandle, TextureSource> mTextureMap;

    // resource mapping from source to packed resource id.
    std::unordered_map<std::string, std::string> mResourceMap;
    // filenames of files we've written.
    std::unordered_set<QString> mFileNames;
};

} // namespace

namespace app
{

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
    auto checkerboard = gfx::CreateMaterialClassFromTexture("app://textures/Checkerboard.png");
    checkerboard.SetId("_checkerboard");
    mResources.emplace_back(new MaterialResource(std::move(checkerboard), "Checkerboard"));

    // add some primitive colors.
    constexpr auto& values = magic_enum::enum_values<gfx::Color>();
    for (const auto& val : values)
    {
        const std::string color_name(magic_enum::enum_name(val));
        auto color = gfx::CreateMaterialClassFromColor(gfx::Color4f(val));
        color.SetId("_" + color_name);
        color.SetName("_" + color_name);
        mResources.emplace_back(new MaterialResource(std::move(color), FromUtf8(color_name)));
    }

    // setup primitive drawables with known/fixed class IDs
    // these IDs are also hardcoded in the engine/loader.cpp which uses
    // these same IDs to create primitive resources.
    mResources.emplace_back(new DrawableResource<gfx::CapsuleClass>(gfx::CapsuleClass("_capsule"), "Capsule"));
    mResources.emplace_back(new DrawableResource<gfx::RectangleClass>(gfx::RectangleClass("_rect"), "Rectangle"));
    mResources.emplace_back(new DrawableResource<gfx::IsoscelesTriangleClass>(gfx::IsoscelesTriangleClass("_isosceles_triangle"), "IsoscelesTriangle"));
    mResources.emplace_back(new DrawableResource<gfx::RightTriangleClass>(gfx::RightTriangleClass("_right_triangle"), "RightTriangle"));
    mResources.emplace_back(new DrawableResource<gfx::CircleClass>(gfx::CircleClass("_circle"), "Circle"));
    mResources.emplace_back(new DrawableResource<gfx::SemiCircleClass>(gfx::SemiCircleClass("_semi_circle"), "SemiCircle"));
    mResources.emplace_back(new DrawableResource<gfx::TrapezoidClass>(gfx::TrapezoidClass("_trapezoid"), "Trapezoid"));
    mResources.emplace_back(new DrawableResource<gfx::ParallelogramClass>(gfx::ParallelogramClass("_parallelogram"), "Parallelogram"));
    mResources.emplace_back(new DrawableResource<gfx::RoundRectangleClass>(gfx::RoundRectangleClass("_round_rect", 0.05f), "RoundRect"));
    mResources.emplace_back(new DrawableResource<gfx::CursorClass>(gfx::CursorClass("_arrow_cursor",
                                                                          gfx::CursorClass::Shape::Arrow),"Arrow Cursor"));
    mResources.emplace_back(new DrawableResource<gfx::CursorClass>(gfx::CursorClass("_block_cursor",
                                                                                    gfx::CursorClass::Shape::Block),"Block Cursor"));
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
    const auto& res = mResources[index.row()];

    if (role == Qt::SizeHintRole)
        return QSize(0, 16);
    else if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case 0: return toString(res->GetType());
            case 1: return res->GetName();
        }
    }
    else if (role == Qt::DecorationRole && index.column() == 0)
    {
        return res->GetIcon();
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

std::unique_ptr<gfx::Material> Workspace::MakeMaterialByName(const QString& name) const
{
    return gfx::CreateMaterialInstance(GetMaterialClassByName(name));

}
std::unique_ptr<gfx::Drawable> Workspace::MakeDrawableByName(const QString& name) const
{
    return gfx::CreateDrawableInstance(GetDrawableClassByName(name));
}

std::shared_ptr<const gfx::MaterialClass> Workspace::GetMaterialClassByName(const QString& name) const
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

std::shared_ptr<const gfx::MaterialClass> Workspace::GetMaterialClassByName(const char* name) const
{
    // convenience shim
    return GetMaterialClassByName(QString::fromUtf8(name));
}

std::shared_ptr<const gfx::MaterialClass> Workspace::GetMaterialClassById(const QString& id) const
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

std::shared_ptr<const gfx::DrawableClass> Workspace::GetDrawableClassByName(const QString& name) const
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
            return ResourceCast<gfx::KinematicsParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::Shape)
            return ResourceCast<gfx::PolygonClass>(*resource).GetSharedResource();
    }
    BUG("No such drawable class.");
    return nullptr;
}
std::shared_ptr<const gfx::DrawableClass> Workspace::GetDrawableClassByName(const char* name) const
{
    // convenience shim
    return GetDrawableClassByName(QString::fromUtf8(name));
}
std::shared_ptr<const gfx::DrawableClass> Workspace::GetDrawableClassById(const QString& id) const
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
            return ResourceCast<gfx::KinematicsParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::Shape)
            return ResourceCast<gfx::PolygonClass>(*resource).GetSharedResource();
    }
    BUG("No such drawable class.");
    return nullptr;
}


std::shared_ptr<const game::EntityClass> Workspace::GetEntityClassByName(const QString& name) const
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
std::shared_ptr<const game::EntityClass> Workspace::GetEntityClassById(const QString& id) const
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

std::shared_ptr<const game::TilemapClass> Workspace::GetTilemapClassById(const QString& id) const
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

std::shared_ptr<const game::TilemapClass> Workspace::GetTilemapClassById(const std::string& id) const
{
    return GetTilemapClassById(FromUtf8(id));
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
            return ResourceCast<gfx::KinematicsParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::Shape)
            return ResourceCast<gfx::PolygonClass>(*resource).GetSharedResource();
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
        auto& node = ret->GetNode(i);
        auto klass = FindEntityClassById(node.GetEntityId());
        if (!klass)
        {
            const auto& node_id    = node.GetId();
            const auto& node_name  = node.GetName();
            const auto& node_entity_id = node.GetEntityId();
            WARN("Scene '%1' node '%2' ('%2') refers to entity '%3' that is not found.",
                 name, node_id, node_name, node_entity_id);
        }
        else
        {
            node.SetEntity(klass);
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
        auto& node = ret->GetNode(i);
        auto klass = FindEntityClassById(node.GetEntityId());
        if (!klass)
        {
            const auto& node_id    = node.GetId();
            const auto& node_name  = node.GetName();
            const auto& node_entity_id = node.GetEntityId();
            WARN("Scene '%1' node '%2' ('%3') refers to entity '%3' that is not found.",
                 id, node_id, node_name, node_entity_id);
        }
        else
        {
            node.SetEntity(klass);
        }
    }
    return ret;
}

engine::ClassHandle<const game::TilemapClass> Workspace::FindTilemapClassById(const std::string& id) const
{
    return FindClassHandleById<game::TilemapClass>(id, Resource::Type::Tilemap);
}

engine::EngineDataHandle Workspace::LoadEngineData(const std::string& URI) const
{
    const auto& file = MapFileToFilesystem(app::FromUtf8(URI));
    DEBUG("URI '%1' => '%2'", URI, file);
    return EngineBuffer::LoadFromFile(file);
}

engine::EngineDataHandle Workspace::LoadEngineDataFromFile(const std::string& filename) const
{
    return EngineBuffer::LoadFromFile(app::FromUtf8(filename));
}

gfx::ResourceHandle Workspace::LoadResource(const std::string& URI)
{
    // static map of resources that are part of the application, i.e.
    // app://something. They're not expected to change.
    static std::unordered_map<std::string,
            std::shared_ptr<const GraphicsBuffer>> application_resources;
    if (base::StartsWith(URI, "app://")) {
        auto it = application_resources.find(URI);
        if (it != application_resources.end())
            return it->second;
    }
    const auto& file = MapFileToFilesystem(app::FromUtf8(URI));
    DEBUG("URI '%1' => '%2'", URI, file);
    auto ret = GraphicsBuffer::LoadFromFile(file);
    if (base::StartsWith(URI, "app://")) {
        application_resources[URI] = ret;
    }
    return ret;
}

audio::SourceStreamHandle Workspace::OpenAudioStream(const std::string& URI,
    AudioIOStrategy strategy, bool enable_file_caching) const
{
    const auto& file = MapFileToFilesystem(app::FromUtf8(URI));
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

bool Workspace::LoadWorkspace()
{
    if (!LoadContent(JoinPath(mWorkspaceDir, "content.json")) ||
        !LoadProperties(JoinPath(mWorkspaceDir, "workspace.json")))
        return false;

    // we don't really care if this fails or not. nothing permanently
    // important should be stored in this file. I.e deleting it
    // will just make the application forget some data that isn't
    // crucial for the operation of the application or for the
    // integrity of the workspace and its content.
    LoadUserSettings(JoinPath(mWorkspaceDir, ".workspace_private.json"));

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
    const auto& path = app::JoinPath(mWorkspaceDir, dir);

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

QString Workspace::MapFileToWorkspace(const QString& filepath) const
{
    // don't remap already mapped files.
    if (filepath.startsWith("app://") ||
        filepath.startsWith("fs://") ||
        filepath.startsWith("ws://"))
        return filepath;

    // when the user is adding resource files to this project (workspace) there's the problem
    // of making the workspace "portable".
    // Portable here means two things:
    // * portable from one operating system to another (from Windows to Linux or vice versa)
    // * portable from one user's environment to another user's environment even when on
    //   the same operating system (i.e. from Windows to Windows or Linux to Linux)
    //
    // Using relative file paths (as opposed to absolute file paths) solves the problem
    // but then the problem is that the relative paths need to be resolved at runtime
    // and also some kind of "landmark" is needed in order to make the files
    // relative something to.
    //
    // The expectation is that during content creation most of the game resources
    // would be placed in a place relative to the workspace. In this case the
    // runtime path resolution would use paths relative to the current workspace.
    // However there's also some content (such as the pre-built shaders) that
    // is bundled with the Editor application and might be used from that location
    // as a game resource.

    // We could always copy the files into some location under the workspace to
    // solve this problem but currently this is not yet done.

    // if the file is in the current workspace path or in the path of the current executable
    // we can then express this path as a relative path.
    const auto& appdir = GetAppDir();
    const auto& file = CleanPath(filepath);

    if (file.startsWith(mWorkspaceDir))
    {
        QString ret = file;
        ret.remove(0, mWorkspaceDir.count());
        if (ret.startsWith("/") || ret.startsWith("\\"))
            ret.remove(0, 1);
        ret = ret.replace("\\", "/");
        return QString("ws://%1").arg(ret);
    }
    else if (file.startsWith(appdir))
    {
        QString ret = file;
        ret.remove(0, appdir.count());
        if (ret.startsWith("/") || ret.startsWith("\\"))
            ret.remove(0, 1);
        ret = ret.replace("\\", "/");
        return QString("app://%1").arg(ret);
    }
    // mapping other paths to identity. will not be portable to another
    // user's computer to another system, unless it's accessible on every
    // machine using the same path (for example a shared file system mount)
    return QString("fs://%1").arg(file);
}

// convenience wrapper
std::string Workspace::MapFileToWorkspace(const std::string& file) const
{
    return ToUtf8(MapFileToWorkspace(FromUtf8(file)));
}

QString Workspace::MapFileToFilesystem(const QString& uri) const
{
    // see comments in AddFileToWorkspace.
    // this is basically the same as MapFilePath except this API
    // is internal to only this application whereas MapFilePath is part of the
    // API exposed to the graphics/ subsystem.

    QString ret = uri;
    if (ret.startsWith("ws://"))
        ret = CleanPath(ret.replace("ws://", mWorkspaceDir));
    else if (uri.startsWith("app://"))
        ret = CleanPath(ret.replace("app://", GetAppDir()));
    else if (uri.startsWith("fs://"))
        ret.remove(0, 5);

    // return as is
    return ret;
}

QString Workspace::MapFileToFilesystem(const std::string& uri) const
{
    return MapFileToFilesystem(FromUtf8(uri));
}

template<typename ClassType>
bool LoadResources(const char* type,
                   const data::Reader& data,
                   std::vector<std::unique_ptr<Resource>>& vector)
{
    DEBUG("Loading %1", type);
    bool success = true;
    for (unsigned i=0; i<data.GetNumChunks(type); ++i)
    {
        const auto& chunk = data.GetReadChunk(type, i);
        std::string name;
        std::string id;
        if (!chunk->Read("resource_name", &name) ||
            !chunk->Read("resource_id", &id))
        {
            ERROR("Unexpected JSON. Maybe old workspace version?");
            success = false;
            continue;
        }
        std::optional<ClassType> ret = ClassType::FromJson(*chunk);
        if (!ret.has_value())
        {
            ERROR("Failed to load resource '%1'", name);
            success = false;
            continue;
        }
        vector.push_back(std::make_unique<GameResource<ClassType>>(std::move(ret.value()), FromUtf8(name)));
        DEBUG("Loaded resource '%1'", name);
    }
    return success;
}

template<typename ClassType>
bool LoadMaterials(const char* type,
                   const data::Reader& data,
                   std::vector<std::unique_ptr<Resource>>& vector)
{
    DEBUG("Loading %1", type);
    bool success = true;
    for (unsigned i=0; i<data.GetNumChunks(type); ++i)
    {
        const auto& chunk = data.GetReadChunk(type, i);
        std::string name;
        std::string id;
        if (!chunk->Read("resource_name", &name) ||
            !chunk->Read("resource_id", &id))
        {
            ERROR("Unexpected JSON. Maybe old workspace version?");
            success = false;
            continue;
        }
        auto ret = ClassType::FromJson(*chunk);
        if (!ret)
        {
            ERROR("Failed to load resource '%1'", name);
            success = false;
            continue;
        }
        vector.push_back(std::make_unique<MaterialResource>(std::move(ret), FromUtf8(name)));
        DEBUG("Loaded resource '%1'", name);
    }
    return success;
}

bool Workspace::LoadContent(const QString& filename)
{
    data::JsonFile file;
    const auto [ok, error] = file.Load(app::ToUtf8(filename));
    if (!ok)
    {
        ERROR("Failed to load JSON content file '%1'", filename);
        ERROR("JSON file load error '%1'", error);
        return false;
    }
    data::JsonObject root = file.GetRootObject();

    LoadMaterials<gfx::MaterialClass>("materials", root, mResources);
    LoadResources<gfx::KinematicsParticleEngineClass>("particles", root, mResources);
    LoadResources<gfx::PolygonClass>("shapes", root, mResources);
    LoadResources<game::EntityClass>("entities", root, mResources);
    LoadResources<game::SceneClass>("scenes", root, mResources);
    LoadResources<game::TilemapClass>("tilemaps", root, mResources);
    LoadResources<Script>("scripts", root, mResources);
    LoadResources<DataFile>("data_files", root, mResources);
    LoadResources<audio::GraphClass>("audio_graphs", root, mResources);
    LoadResources<uik::Window>("uis", root, mResources);

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
    mVisibleCount = std::distance(mResources.begin(), primitives_start);

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
    JsonWrite(project, "multisample_sample_count", mSettings.multisample_sample_count);
    JsonWrite(project, "application_identifier"  , mSettings.application_identifier);
    JsonWrite(project, "application_name"        , mSettings.application_name);
    JsonWrite(project, "application_version"     , mSettings.application_version);
    JsonWrite(project, "application_library_win" , mSettings.application_library_win);
    JsonWrite(project, "application_library_lin" , mSettings.application_library_lin);
    JsonWrite(project, "debug_font"              , mSettings.debug_font);
    JsonWrite(project, "debug_show_fps"          , mSettings.debug_show_fps);
    JsonWrite(project, "debug_show_msg"          , mSettings.debug_show_msg);
    JsonWrite(project, "debug_draw"              , mSettings.debug_draw);
    JsonWrite(project, "debug_print_fps"         , mSettings.debug_print_fps);
    JsonWrite(project, "logging_debug"           , mSettings.log_debug);
    JsonWrite(project, "logging_warn"            , mSettings.log_debug);
    JsonWrite(project, "logging_info"            , mSettings.log_debug);
    JsonWrite(project, "logging_error"           , mSettings.log_debug);
    JsonWrite(project, "default_min_filter"      , mSettings.default_min_filter);
    JsonWrite(project, "default_mag_filter"      , mSettings.default_mag_filter);
    JsonWrite(project, "webgl_power_preference"  , mSettings.webgl_power_preference);
    JsonWrite(project, "webgl_antialias"         , mSettings.webgl_antialias);
    JsonWrite(project, "html5_developer_ui"      , mSettings.html5_developer_ui);
    JsonWrite(project, "canvas_mode"             , mSettings.canvas_mode);
    JsonWrite(project, "canvas_width"            , mSettings.canvas_width);
    JsonWrite(project, "canvas_height"           , mSettings.canvas_height);
    JsonWrite(project, "window_mode"             , mSettings.window_mode);
    JsonWrite(project, "window_width"            , mSettings.window_width);
    JsonWrite(project, "window_height"           , mSettings.window_height);
    JsonWrite(project, "window_can_resize"       , mSettings.window_can_resize);
    JsonWrite(project, "window_has_border"       , mSettings.window_has_border);
    JsonWrite(project, "window_vsync"            , mSettings.window_vsync);
    JsonWrite(project, "window_cursor"           , mSettings.window_cursor);
    JsonWrite(project, "config_srgb"             , mSettings.config_srgb);
    JsonWrite(project, "grab_mouse"              , mSettings.grab_mouse);
    JsonWrite(project, "save_window_geometry"    , mSettings.save_window_geometry);
    JsonWrite(project, "ticks_per_second"        , mSettings.ticks_per_second);
    JsonWrite(project, "updates_per_second"      , mSettings.updates_per_second);
    JsonWrite(project, "working_folder"          , mSettings.working_folder);
    JsonWrite(project, "command_line_arguments"  , mSettings.command_line_arguments);
    JsonWrite(project, "use_gamehost_process"    , mSettings.use_gamehost_process);
    JsonWrite(project, "enable_physics"          , mSettings.enable_physics);
    JsonWrite(project, "num_velocity_iterations" , mSettings.num_velocity_iterations);
    JsonWrite(project, "num_position_iterations" , mSettings.num_position_iterations);
    JsonWrite(project, "phys_gravity_x"          , mSettings.physics_gravity.x);
    JsonWrite(project, "phys_gravity_y"          , mSettings.physics_gravity.y);
    JsonWrite(project, "phys_scale_x"            , mSettings.physics_scale.x);
    JsonWrite(project, "phys_scale_y"            , mSettings.physics_scale.y);
    JsonWrite(project, "game_viewport_width"     , mSettings.viewport_width);
    JsonWrite(project, "game_viewport_height"    , mSettings.viewport_height);
    JsonWrite(project, "clear_color"             , mSettings.clear_color);
    JsonWrite(project, "mouse_pointer_material"  , mSettings.mouse_pointer_material);
    JsonWrite(project, "mouse_pointer_drawable"  , mSettings.mouse_pointer_drawable);
    JsonWrite(project, "mouse_pointer_visible"   , mSettings.mouse_pointer_visible);
    JsonWrite(project, "mouse_pointer_hotspot"   , mSettings.mouse_pointer_hotspot);
    JsonWrite(project, "mouse_pointer_size"      , mSettings.mouse_pointer_size);
    JsonWrite(project, "mouse_pointer_units"     , mSettings.mouse_pointer_units);
    JsonWrite(project, "game_script"             , mSettings.game_script);
    JsonWrite(project, "audio_channels"          , mSettings.audio_channels);
    JsonWrite(project, "audio_sample_rate"       , mSettings.audio_sample_rate);
    JsonWrite(project, "audio_sample_type"       , mSettings.audio_sample_type);
    JsonWrite(project, "audio_buffer_size"       , mSettings.audio_buffer_size);
    JsonWrite(project, "enable_audio_pcm_caching", mSettings.enable_audio_pcm_caching);
    JsonWrite(project, "desktop_audio_io_strategy", mSettings.desktop_audio_io_strategy);
    JsonWrite(project, "wasm_audio_io_strategy"  , mSettings.wasm_audio_io_strategy);

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

bool Workspace::LoadProperties(const QString& filename)
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
    JsonReadSafe(project, "multisample_sample_count", &mSettings.multisample_sample_count);
    JsonReadSafe(project, "application_identifier",   &mSettings.application_identifier);
    JsonReadSafe(project, "application_name",         &mSettings.application_name);
    JsonReadSafe(project, "application_version",      &mSettings.application_version);
    JsonReadSafe(project, "application_library_win",  &mSettings.application_library_win);
    JsonReadSafe(project, "application_library_lin",  &mSettings.application_library_lin);
    JsonReadSafe(project, "debug_font"              , &mSettings.debug_font);
    JsonReadSafe(project, "debug_show_fps"          , &mSettings.debug_show_fps);
    JsonReadSafe(project, "debug_show_msg"          , &mSettings.debug_show_msg);
    JsonReadSafe(project, "debug_draw"              , &mSettings.debug_draw);
    JsonReadSafe(project, "debug_print_fps"         , &mSettings.debug_print_fps);
    JsonReadSafe(project, "logging_debug"           , &mSettings.log_debug);
    JsonReadSafe(project, "logging_warn"            , &mSettings.log_debug);
    JsonReadSafe(project, "logging_info"            , &mSettings.log_debug);
    JsonReadSafe(project, "logging_error"           , &mSettings.log_debug);
    JsonReadSafe(project, "default_min_filter",       &mSettings.default_min_filter);
    JsonReadSafe(project, "default_mag_filter",       &mSettings.default_mag_filter);
    JsonReadSafe(project, "webgl_power_preference"  , &mSettings.webgl_power_preference);
    JsonReadSafe(project, "webgl_antialias"         , &mSettings.webgl_antialias);
    JsonReadSafe(project, "html5_developer_ui"      , &mSettings.html5_developer_ui);
    JsonReadSafe(project, "canvas_mode"             , &mSettings.canvas_mode);
    JsonReadSafe(project, "canvas_width"            , &mSettings.canvas_width);
    JsonReadSafe(project, "canvas_height"           , &mSettings.canvas_height);
    JsonReadSafe(project, "window_mode",              &mSettings.window_mode);
    JsonReadSafe(project, "window_width",             &mSettings.window_width);
    JsonReadSafe(project, "window_height",            &mSettings.window_height);
    JsonReadSafe(project, "window_can_resize",        &mSettings.window_can_resize);
    JsonReadSafe(project, "window_has_border",        &mSettings.window_has_border);
    JsonReadSafe(project, "window_vsync",             &mSettings.window_vsync);
    JsonReadSafe(project, "window_cursor",            &mSettings.window_cursor);
    JsonReadSafe(project, "config_srgb",              &mSettings.config_srgb);
    JsonReadSafe(project, "grab_mouse",               &mSettings.grab_mouse);
    JsonReadSafe(project, "save_window_geometry",     &mSettings.save_window_geometry);
    JsonReadSafe(project, "ticks_per_second",         &mSettings.ticks_per_second);
    JsonReadSafe(project, "updates_per_second",       &mSettings.updates_per_second);
    JsonReadSafe(project, "working_folder",           &mSettings.working_folder);
    JsonReadSafe(project, "command_line_arguments",   &mSettings.command_line_arguments);
    JsonReadSafe(project, "use_gamehost_process",     &mSettings.use_gamehost_process);
    JsonReadSafe(project, "enable_physics",           &mSettings.enable_physics);
    JsonReadSafe(project, "num_position_iterations",  &mSettings.num_position_iterations);
    JsonReadSafe(project, "num_velocity_iterations",  &mSettings.num_velocity_iterations);
    JsonReadSafe(project, "phys_gravity_x",           &mSettings.physics_gravity.x);
    JsonReadSafe(project, "phys_gravity_y",           &mSettings.physics_gravity.y);
    JsonReadSafe(project, "phys_scale_x",             &mSettings.physics_scale.x);
    JsonReadSafe(project, "phys_scale_y",             &mSettings.physics_scale.y);
    JsonReadSafe(project, "game_viewport_width",      &mSettings.viewport_width);
    JsonReadSafe(project, "game_viewport_height",     &mSettings.viewport_height);
    JsonReadSafe(project, "clear_color",              &mSettings.clear_color);
    JsonReadSafe(project, "mouse_pointer_material",   &mSettings.mouse_pointer_material);
    JsonReadSafe(project, "mouse_pointer_drawable",   &mSettings.mouse_pointer_drawable);
    JsonReadSafe(project, "mouse_pointer_visible",    &mSettings.mouse_pointer_visible);
    JsonReadSafe(project, "mouse_pointer_hotspot",    &mSettings.mouse_pointer_hotspot);
    JsonReadSafe(project, "mouse_pointer_size",       &mSettings.mouse_pointer_size);
    JsonReadSafe(project, "mouse_pointer_units",      &mSettings.mouse_pointer_units);
    JsonReadSafe(project, "game_script",              &mSettings.game_script);
    JsonReadSafe(project, "audio_channels",           &mSettings.audio_channels);
    JsonReadSafe(project, "audio_sample_rate",        &mSettings.audio_sample_rate);
    JsonReadSafe(project, "audio_sample_type",        &mSettings.audio_sample_type);
    JsonReadSafe(project, "audio_buffer_size",        &mSettings.audio_buffer_size);
    JsonReadSafe(project, "enable_audio_pcm_caching", &mSettings.enable_audio_pcm_caching);
    JsonReadSafe(project, "desktop_audio_io_strategy", &mSettings.desktop_audio_io_strategy);
    JsonReadSafe(project, "wasm_audio_io_strategy"  , &mSettings.wasm_audio_io_strategy);

    // load the workspace properties.
    mProperties = docu["workspace"].toObject().toVariantMap();

    // so we expect that the content has been loaded first.
    // and then ask each resource object to load its additional
    // properties from the workspace file.
    for (auto& resource : mResources)
    {
        resource->LoadProperties(docu.object());
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

void Workspace::SaveResource(const Resource& resource)
{
    RECURSION_GUARD(this, "ResourceList");

    const auto& id = resource.GetId();
    for (size_t i=0; i<mResources.size(); ++i)
    {
        auto& res = mResources[i];
        if (res->GetId() != id)
            continue;
        res->UpdateFrom(resource);
        emit ResourceUpdated(mResources[i].get());
        emit dataChanged(index(i, 0), index(i, 1));
        INFO("Saved resource '%1'", resource.GetName());
        NOTE("Saved resource '%1'", resource.GetName());
        return;
    }
    // if we're here no such resource exists yet.
    // Create a new resource and add it to the list of resources.
    beginInsertRows(QModelIndex(), mVisibleCount, mVisibleCount);
    // insert at the end of the visible range which is from [0, mVisibleCount)
    mResources.insert(mResources.begin() + mVisibleCount, resource.Copy());

    // careful! endInsertRows will trigger the view proxy to re-fetch the contents.
    // make sure to update this property before endInsertRows otherwise
    // we'll hit ASSERT incorrectly in GetUserDefinedProperty.
    mVisibleCount++;

    endInsertRows();

    auto& back = mResources[mVisibleCount-1];
    ASSERT(back->GetId() == resource.GetId());
    ASSERT(back->GetName() == resource.GetName());
    emit NewResourceAvailable(back.get());

    INFO("Saved new resource '%1'", resource.GetName());
    NOTE("Saved new resource '%1'", resource.GetName());
}

QString Workspace::MapDrawableIdToName(const QString& id) const
{
    return MapResourceIdToName(id);
}

QString Workspace::MapMaterialIdToName(const QString& id) const
{
    return MapResourceIdToName(id);
}

QString Workspace::MapEntityIdToName(const QString &id) const
{
    return MapResourceIdToName(id);
}

QString Workspace::MapResourceIdToName(const QString &id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id)
            return resource->GetName();
    }
    return "";
}

bool Workspace::IsValidMaterial(const QString& klass) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() == Resource::Type::Material &&
            resource->GetId() == klass)
            return true;
    }
    return false;
}

bool Workspace::IsValidDrawable(const QString& klass) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == klass &&
            (resource->GetType() == Resource::Type::ParticleSystem ||
             resource->GetType() == Resource::Type::Shape ||
             resource->GetType() == Resource::Type::Drawable))
            return true;
    }
    return false;
}

bool Workspace::IsValidTilemap(const QString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id &&  resource->IsTilemap())
            return true;
    }
    return false;
}

bool Workspace::IsValidScript(const QString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id && resource->IsScript())
            return true;
    }
    return false;
}

bool Workspace::IsUserDefinedResource(const QString& id) const
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
bool Workspace::IsUserDefinedResource(const std::string& id) const
{
    return IsUserDefinedResource(FromUtf8(id));
}

Resource& Workspace::GetResource(size_t index)
{
    ASSERT(index < mResources.size());
    return *mResources[index];
}
Resource& Workspace::GetPrimitiveResource(size_t index)
{
    const auto num_primitives = mResources.size() - mVisibleCount;

    ASSERT(index < num_primitives);
    return *mResources[mVisibleCount + index];
}

Resource& Workspace::GetUserDefinedResource(size_t index)
{
    ASSERT(index < mVisibleCount);

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

const Resource& Workspace::GetResource(size_t index) const
{
    ASSERT(index < mResources.size());
    return *mResources[index];
}

const Resource& Workspace::GetUserDefinedResource(size_t index) const
{
    ASSERT(index < mVisibleCount);
    return *mResources[index];
}
const Resource& Workspace::GetPrimitiveResource(size_t index) const
{
    const auto num_primitives = mResources.size() - mVisibleCount;

    ASSERT(index < num_primitives);
    return *mResources[mVisibleCount + index];
}

void Workspace::DeleteResources(const QModelIndexList& list)
{
    std::vector<size_t> indices;
    for (const auto& i : list)
        indices.push_back(i.row());

    DeleteResources(indices);
}
void Workspace::DeleteResource(size_t index)
{
    DeleteResources(std::vector<size_t>{index});
}

void Workspace::DeleteResources(std::vector<size_t> indices)
{
    RECURSION_GUARD(this, "ResourceList");

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
                for (size_t j = 0; j < mVisibleCount; ++j)
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
    // combine the original indicies together with the associated
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
        mVisibleCount--;

        endRemoveRows();
    }
    // invoke a resource deletion signal for each resource now
    // by iterating over the separate container. (avoids broken iteration)
    for (const auto& carcass : graveyard)
    {
        emit ResourceToBeDeleted(carcass.get());
    }

    // script resources are special in the sense that they're the only
    // resources where the underlying file system content file is actually
    // created by this editor. for everything else, shaders, image files
    // and font files the resources are created by other tools/applications
    // and we only keep references to those files.
    // So for scripts when the script resource is deleted we're actually
    // going to delete the underlying filesystem file as well.
    for (const auto& carcass : graveyard)
    {
        QString dead_file;
        if (carcass->IsScript())
        {
            Script* script = nullptr;
            carcass->GetContent(&script);
            dead_file = MapFileToFilesystem(script->GetFileURI());
        }
        else if (carcass->IsDataFile())
        {
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

void Workspace::DeleteResource(const std::string& id)
{
    for (size_t i=0; i<GetNumUserDefinedResources(); ++i)
    {
        const auto& res = GetUserDefinedResource(i);
        if (res.GetIdUtf8() == id)
        {
            DeleteResource(i);
            return;
        }
    }
}
void Workspace::DeleteResource(const QString& id)
{
    for (size_t i=0; i<GetNumUserDefinedResources(); ++i)
    {
        const auto& res = GetUserDefinedResource(i);
        if (res.GetId() == id)
        {
            DeleteResource(i);
            return;
        }
    }
}

void Workspace::DuplicateResources(const QModelIndexList& list)
{
    std::vector<size_t> indices;
    for (const auto& i : list)
        indices.push_back(i.row());
    DuplicateResources(indices);
}

void Workspace::DuplicateResources(std::vector<size_t> indices)
{
    RECURSION_GUARD(this, "ResourceList");

    std::sort(indices.begin(), indices.end(), std::less<size_t>());

    std::vector<std::unique_ptr<Resource>> dupes;
    for (int i=0; i<indices.size(); ++i)
    {
        const auto row = indices[i];
        const auto& resource = GetResource(row);
        auto clone = resource.Clone();
        clone->SetName(QString("Copy of %1").arg(resource.GetName()));
        dupes.push_back(std::move(clone));
    }

    for (int i=0; i<(int)dupes.size(); ++i)
    {
        const auto pos = indices[i]+ i;
        beginInsertRows(QModelIndex(), pos, pos);
        auto it = mResources.begin();
        it += pos;
        auto* dupe = dupes[i].get();
        mResources.insert(it, std::move(dupes[i]));
        mVisibleCount++;
        endInsertRows();

        emit NewResourceAvailable(dupe);
    }
}

void Workspace::DuplicateResource(size_t index)
{
    DuplicateResources(std::vector<size_t>{index});
}

bool Workspace::ExportResources(const QModelIndexList& list, const QString& filename) const
{
    std::vector<size_t> indices;
    for (const auto& index : list)
        indices.push_back(index.row());
    return ExportResources(indices, filename);
}

bool Workspace::ExportResources(const std::vector<size_t>& indices, const QString& filename) const
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
bool Workspace::ImportResources(const QString& filename, std::vector<std::unique_ptr<Resource>>& resources)
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
    success = success && LoadResources<gfx::KinematicsParticleEngineClass>("particles", root, resources);
    success = success && LoadResources<gfx::PolygonClass>("shapes", root, resources);
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
        if (suffix == "LUA")
        {
            const auto& uri = MapFileToWorkspace(file);
            Script script;
            script.SetFileURI(ToUtf8(uri));
            ScriptResource res(script, name);
            SaveResource(res);
            INFO("Imported new script file '%1' based on file '%2'", name, info.filePath());
        }
        else if (suffix == "JPEG" || suffix == "JPG" || suffix == "PNG" || suffix == "TGA" || suffix == "BMP")
        {
            const auto& uri = MapFileToWorkspace(file);
            gfx::detail::TextureFileSource texture;
            texture.SetFileName(ToUtf8(uri));
            texture.SetName(ToUtf8(name));

            gfx::TextureMap2DClass klass;
            klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            klass.SetTexture(texture.Copy());
            klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Default);
            klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter ::Default);
            MaterialResource res(klass, name);
            SaveResource(res);
            INFO("Imported new material '%1' based on image file '%2'", name, info.filePath());
        }
        else if (suffix == "MP3" || suffix == "WAV" || suffix == "FLAC" || suffix == "OGG")
        {
            const auto& uri = MapFileToWorkspace(file);
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
            const auto& uri = MapFileToWorkspace(file);
            DataFile data;
            data.SetFileURI(uri);
            data.SetTypeTag(DataFile::TypeTag::External);
            DataResource res(data, name);
            SaveResource(res);
            INFO("Imported new data file '%1' based on file '%2'", name, info.filePath());
        }
    }
}

void Workspace::Tick()
{

}

bool Workspace::PackContent(const std::vector<const Resource*>& resources, const ContentPackingOptions& options)
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
        mutable_copies.push_back(resource->Copy());
    }

    DEBUG("Max texture size. [width=%1, height=%2]", options.max_texture_width, options.max_texture_height);
    DEBUG("Pack size. [width=%1, height=%2]", options.texture_pack_width, options.texture_pack_height);
    DEBUG("Pack flags. [resize=%1, combine=%2]", options.resize_textures, options.combine_textures);

    ResourcePacker packer(outdir,
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
        emit ResourcePackingUpdate("Collecting resources...", i, mutable_copies.size());
        if (resource->IsMaterial())
        {
            // todo: maybe move to Resource interface ?
            const gfx::MaterialClass* material = nullptr;
            resource->GetContent(&material);
            material->BeginPacking(&packer);
        }
        else if (resource->IsParticleEngine())
        {
            const gfx::KinematicsParticleEngineClass* engine = nullptr;
            resource->GetContent(&engine);
            engine->Pack(&packer);
        }
        else if (resource->IsCustomShape())
        {
            const gfx::PolygonClass* polygon = nullptr;
            resource->GetContent(&polygon);
            polygon->Pack(&packer);
        }
    }

    unsigned errors = 0;

    // copy some file based content around.
    // todo: this would also need some kind of file name collision
    // resolution and mapping functionality.
    for (int i=0; i<mutable_copies.size(); ++i)
    {
        const auto& resource = mutable_copies[i];
        if (resource->IsScript())
        {
            const Script* script = nullptr;
            resource->GetContent(&script);
            packer.CopyFile(script->GetFileURI(), "lua/");
        }
        else if (resource->IsDataFile())
        {
            const DataFile* datafile = nullptr;
            resource->GetContent(&datafile);
            packer.CopyFile(datafile->GetFileURI(), "data/");
        }
        else if (resource->IsAudioGraph())
        {
            // todo: this audio packing sucks a little bit since it needs
            // to know about the details of elements now. maybe this
            // should be refactored into the audio/ subsystem.. ?
            audio::GraphClass* audio = nullptr;
            resource->GetContent(&audio);
            for (size_t i=0; i<audio->GetNumElements(); ++i)
            {
                auto& elem = audio->GetElement(i);
                for (auto& p : elem.args)
                {
                    const auto& name = p.first;
                    if (name != "file") continue;

                    auto* file_uri = std::get_if<std::string>(&p.second);
                    ASSERT(file_uri && "Missing audio element 'file' parameter.");
                    if (file_uri->empty())
                    {
                        WARN("Audio element doesn't have input file set. [graph='%1', elem='%2']", audio->GetName(), name);
                        continue;
                    }
                    *file_uri = packer.CopyFile(*file_uri, "audio");
                }
            }
        }
        else if (resource->IsUI())
        {
            uik::Window* window = nullptr;
            resource->GetContent(&window);
            // package the style resources. currently this is only the font files.
            auto style_data = LoadEngineData(window->GetStyleName());
            if (!style_data)
            {
                ERROR("Failed to open UI style file. [UI='%1', style='%2']", window->GetName(), window->GetStyleName());
                errors++;
                continue;
            }
            engine::UIStyle style;
            if (!style.LoadStyle(*style_data))
            {
                ERROR("Failed to load UI style. [UI='%1', style='%2']", window->GetName(), window->GetStyleName());
                errors++;
                continue;
            }
            std::vector<engine::UIStyle::PropertyKeyValue> props;
            style.GatherProperties("-font", &props);
            for (auto& p : props)
            {
                std::string src_font_uri;
                std::string dst_font_uri;
                p.prop.GetValue(&src_font_uri);
                dst_font_uri = packer.CopyFile(src_font_uri, "fonts");
                p.prop.SetValue(dst_font_uri);
                style.SetProperty(p.key, p.prop);
            }
            window->SetStyleName(packer.CopyFile(window->GetStyleName(), "ui"));
            // for each widget, parse the style string and see if there are more font-name props.
            window->ForEachWidget([&style, &packer](uik::Widget* widget) {
                auto style_string = widget->GetStyleString();
                if (style_string.empty())
                    return;
                DEBUG("Original widget style string. [widget='%1', style='%2']", widget->GetId(), style_string);
                style.ClearProperties();
                style.ClearMaterials();
                style.ParseStyleString(widget->GetId(), style_string);
                std::vector<engine::UIStyle::PropertyKeyValue> props;
                style.GatherProperties("-font", &props);
                for (auto& p : props)
                {
                    std::string src_font_uri;
                    std::string dst_font_uri;
                    p.prop.GetValue(&src_font_uri);
                    dst_font_uri = packer.CopyFile(src_font_uri, "fonts");
                    p.prop.SetValue(dst_font_uri);
                    style.SetProperty(p.key, p.prop);
                }
                style_string = style.MakeStyleString(widget->GetId());
                // this is a bit of a hack but we know that the style string
                // contains the widget id for each property. removing the
                // widget id from the style properties:
                // a) saves some space
                // b) makes the style string copyable from one widget to another as-s
                boost::erase_all(style_string, widget->GetId() + "/");
                DEBUG("Updated widget style string. [widget='%1', style='%2']", widget->GetId(), style_string);
                widget->SetStyleString(std::move(style_string));
            });
            auto window_style_string = window->GetStyleString();
            if (!window_style_string.empty())
            {
                DEBUG("Original window style string. [window='%1', style='%2']", window->GetName(), window_style_string);
                style.ClearProperties();
                style.ClearMaterials();
                style.ParseStyleString("window", window_style_string);
                std::vector<engine::UIStyle::PropertyKeyValue> props;
                style.GatherProperties("-font", &props);
                for (auto& p : props)
                {
                    std::string src_font_uri;
                    std::string dst_font_uri;
                    p.prop.GetValue(&src_font_uri);
                    dst_font_uri = packer.CopyFile(src_font_uri, "fonts");
                    p.prop.SetValue(dst_font_uri);
                    style.SetProperty(p.key, p.prop);
                }
                window_style_string = style.MakeStyleString("window");
                // this is a bit of a hack but we know that the style string
                // contains the prefix "window" for each property. removing the
                // prefix from the style properties:
                // a) saves some space
                // b) makes the style string copyable from one widget to another as-s
                boost::erase_all(window_style_string, "window/");
                // set the actual style string.
                DEBUG("Updated window style string. [window='%1', style='%2']", window->GetName(), window_style_string);
                window->SetStyleString(std::move(window_style_string));
            }
        }
        else if (resource->IsEntity())
        {
            game::EntityClass* entity = nullptr;
            resource->GetContent(&entity);
            for (size_t i=0; i<entity->GetNumNodes(); ++i)
            {
                auto& node = entity->GetNode(i);
                if (!node.HasTextItem())
                    continue;
                auto* text = node.GetTextItem();
                text->SetFontName(packer.CopyFile(text->GetFontName(), "fonts/"));
            }
        }
        else if (resource->IsTilemap())
        {
            game::TilemapClass* map = nullptr;
            resource->GetContent(&map);
            for (size_t i=0; i<map->GetNumLayers(); ++i)
            {
                auto& layer = map->GetLayer(i);
                layer.SetDataUri(base::FormatString("pck://data/%1.bin", layer.GetId()));
            }
        }
    }

    packer.PackTextures([this](const std::string& action, int step, int max) {
        emit ResourcePackingUpdate(FromLatin(action), step, max);
    });

    for (int i=0; i<mutable_copies.size(); ++i)
    {
        const auto& resource = mutable_copies[i];
        emit ResourcePackingUpdate("Updating resources references...", i, mutable_copies.size());
        if (resource->IsMaterial())
        {
            // todo: maybe move to resource interface ?
            gfx::MaterialClass* material = nullptr;
            resource->GetContent(&material);
            material->FinishPacking(&packer);
        }
    }

    if (!mSettings.debug_font.isEmpty())
    {
        // todo: should change the font URI.
        // but right now this still also works since there's a hack for this
        // in the loader in engine/ (Also same app:// thing applies to the UI style files)
        packer.CopyFile(app::ToUtf8(mSettings.debug_font), "fonts/");
    }

    // write content file ?
    if (options.write_content_file)
    {
        emit ResourcePackingUpdate("Writing content JSON file...", 0, 0);
        // filename of the JSON based descriptor that contains all the
        // resource definitions.
        const auto &json_filename = JoinPath(outdir, "content.json");

        QFile json_file;
        json_file.setFileName(json_filename);
        json_file.open(QIODevice::WriteOnly);
        if (!json_file.isOpen())
        {
            ERROR("Failed to open content JSON file. [file='%1', error='%2']", json_filename, json_file.error());
            return false;
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
            return false;
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
        emit ResourcePackingUpdate("Writing config JSON file...", 0, 0);

        const auto& json_filename = JoinPath(options.directory, "config.json");
        QFile json_file;
        json_file.setFileName(json_filename);
        json_file.open(QIODevice::WriteOnly);
        if (!json_file.isOpen())
        {
            ERROR("Failed to open config JSON file. [file='%1', error='%2']", json_filename, json_file.error());
            return false;
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
        base::JsonWrite(json["application"], "save_window_geometry", mSettings.save_window_geometry);
        base::JsonWrite(json["desktop"], "audio_io_strategy", mSettings.desktop_audio_io_strategy);
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
        base::JsonWrite(json["html5"], "webgl_power_pref", mSettings.webgl_power_preference);
        base::JsonWrite(json["html5"], "webgl_antialias", mSettings.webgl_antialias);
        base::JsonWrite(json["html5"], "developer_ui", mSettings.html5_developer_ui);
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
        const auto& str = json.dump(2);
        if (json_file.write(&str[0], str.size()) == -1)
        {
            ERROR("Failed to write config JSON file. [file='%1', error='%2']", json_filename, json_file.error());
            return false;
        }
        json_file.flush();
        json_file.close();
    }

    if (options.write_html5_content_fs_image)
    {
        emit ResourcePackingUpdate("Generating HTML5 filesystem image...", 0, 0);

        DEBUG("Generating HTML5 filesystem image. [emsdk='%1', python='%2']", options.emsdk_path, options.python_executable);

        QString filesystem_image_name = "FILESYSTEM";
        QStringList args;
        args << app::JoinPath(options.emsdk_path, "/upstream/emscripten/tools/file_packager.py");
        args << filesystem_image_name;
        args << "--preload";
        args << options.package_name;
        args << "config.json";
        args << QString("--js-output=%1.js").arg(filesystem_image_name);
        QStringList stdout_buffer;
        QStringList stderr_buffer;
        Process::RunAndCapture(options.python_executable,
                               options.directory, args, &stdout_buffer, &stderr_buffer);
    }

    if (options.copy_html5_files)
    {
        // these should be in the dist/ folder and are the
        // built by the emscripten build in emscripten/
        const QString files[] = {
           "GameEngine.js", "GameEngine.wasm"
        };
        for (int i=0; i<2; ++i)
        {
            const auto& src = app::GetAppInstFilePath(files[i]);
            const auto& dst = app::JoinPath(options.directory, files[i]);
            auto[success, error] = app::CopyFile(src, dst);
            if (!success)
            {
                ERROR("Failed to copy game engine file. [src='%1', dst='%2', error='%3']", src, dst, error);
                ++errors;
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

    const auto total_errors = errors + packer.GetNumErrors();
    if (total_errors)
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
    mUserProperties[name] = data;
}

} // namespace
