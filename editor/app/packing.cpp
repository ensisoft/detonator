// Copyright (c) 2010-2019 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#define LOGTAG "workspace"

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#  include <QPainter>
#  include <QImage>
#  include <QImageWriter>
#  include <QPixmap>
#include "warnpop.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <set>

#include "editor/app/eventlog.h"
#include "editor/app/packing.h"
#include "editor/app/resource.h"
#include "editor/app/utility.h"
#include "editor/app/packing.h"
#include "graphics/resource.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "base/assert.h"

namespace {

class ResourcePacker : public gfx::ResourcePacker
{
public:
    using ObjectHandle = gfx::ResourcePacker::ObjectHandle;

    ResourcePacker(const QString& outdir, unsigned max_width, unsigned max_height, bool resize_large, bool pack_small)
        : kOutDir(outdir)
        , kMaxTextureWidth(max_width)
        , kMaxTextureHeight(max_height)
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
    virtual void SetTextureFlag(ObjectHandle instance, gfx::ResourcePacker::TextureFlags flags, bool on_off) override
    {
        ASSERT(flags == gfx::ResourcePacker::TextureFlags::CanCombine);
        mTextureMap[instance].can_be_combined = on_off;
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

    void PackTextures()
    {
        if (mTextureMap.empty())
            return;

        // OpenGL ES 2. defines the minimum required supported texture size to be
        // only 64x64 px which is not much. Anything bigger than that
        // is implementation specific. :p
        // for maximum portability we then just pretty much skip the whole packing.

        // the source list of rectangles (images to pack)
        std::vector<app::PackingRectangle> sources;

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
        // (combination of multiple textures) or a downsampled
        // (originally large) texture.
        std::unordered_map<std::string, GeneratedTextureEntry> relocation_map;

        // duplicate source file entries are discarded.
        std::set<std::string> dupemap;

        // 1. go over the list of textures, ignore duplicates
        // 2. select textures that seem like a good "fit" for packing. (size ?)
        // 3. combine the textures into atlas/atlasses.
        // -- composit the actual image files.
        // 4. copy the src image contents into the container image.
        // 5. write the container/packed image into the package folder
        // 6. update the textures whose source images were packaged (the file handle and the rectangle box)

        for (auto it = mTextureMap.begin(); it != mTextureMap.end(); ++it)
        {
            const TextureSource& tex = it->second;
            if (tex.file.empty())
                continue;
            if (auto it = dupemap.find(tex.file) != dupemap.end())
                continue;

            const QFileInfo info(app::FromUtf8(tex.file));
            const QString src = info.absoluteFilePath();
            const QPixmap pix(src);
            if (pix.isNull())
            {
                ERROR("Failed to open image: '%1'", src);
                mNumErrors++;
                continue;
            }

            const auto width  = pix.width();
            const auto height = pix.height();
            DEBUG("Image %1 %2%%3 px", src, width, height);
            if (width > kMaxTextureWidth || height > kMaxTextureHeight)
            {
                // todo: resample the image to fit within the maximum dimensions
                // for now just map it as is.
                GeneratedTextureEntry self;
                self.width  = 1.0f;
                self.height = 1.0f;
                self.xpos   = 0.0f;
                self.ypos   = 0.0f;
                self.texture_file   = app::FromUtf8(CopyFile(tex.file, "textures"));
                relocation_map[tex.file] = std::move(self);
            }
            else if (!kPackSmallTextures || !tex.can_be_combined)
            {
                // add as an identity texture relocation entry.
                GeneratedTextureEntry self;
                self.width  = 1.0f;
                self.height = 1.0f;
                self.xpos   = 0.0f;
                self.ypos   = 0.0f;
                self.texture_file   = app::FromUtf8(CopyFile(tex.file, "textures"));
                relocation_map[tex.file] = std::move(self);
            }
            else
            {
                // add as a source for texture packing
                app::PackingRectangle rc;
                rc.width  = pix.width();
                rc.height = pix.height();
                rc.cookie = tex.file;
                sources.push_back(rc);
            }
        }

        unsigned atlas_number = 0;

        while (!sources.empty())
        {
            app::RectanglePackSize packing_rect_result;
            app::PackRectangles({kMaxTextureWidth, kMaxTextureHeight}, sources, &packing_rect_result);
            // ok, some of the textures might have failed to pack on this pass.
            // separate the ones that were succesfully packed from the ones that
            // weren't. then composit the image for the success cases.
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
                GeneratedTextureEntry gen;
                gen.texture_file = app::FromUtf8(CopyFile(rc.cookie, "textures"));
                gen.width  = 1.0f;
                gen.height = 1.0f;
                gen.xpos   = 0.0f;
                gen.ypos   = 0.0f;
                relocation_map[rc.cookie] = gen;
                sources.erase(first_success);
                continue;
            }


            // composition buffer.
            QPixmap buffer(packing_rect_result.width, packing_rect_result.height);
            buffer.fill(QColor(0x00, 0x00, 0x00, 0x00));

            QPainter painter(&buffer);
            painter.setCompositionMode(QPainter::CompositionMode_Source); // copy src pixel as-is

            // do the composite pass.
            for (auto it = first_success; it != sources.end(); ++it)
            {
                const auto& rc = *it;
                ASSERT(rc.success);

                const QFileInfo info(app::FromUtf8(rc.cookie));
                const QString file(info.absoluteFilePath());
                const QRectF dst(rc.xpos, rc.ypos, rc.width, rc.height);
                const QRectF src(0, 0, rc.width, rc.height);
                const QPixmap pix(file);
                if (pix.isNull())
                {
                    ERROR("Failed to open image: '%1'", file);
                    mNumErrors++;
                }
                else
                {
                    painter.drawPixmap(dst, pix, src);

                }
            }

            const QString& name = QString("Generated_%1.png").arg(atlas_number);
            if (!app::MakePath(app::JoinPath(kOutDir, "textures")))
            {
                ERROR("Failed to create %1/%2", kOutDir, "textures");
                mNumErrors++;
            }
            const QString& file = app::JoinPath(app::JoinPath(kOutDir, "textures"), name);

            QImageWriter writer;
            writer.setFormat("PNG");
            writer.setQuality(100);
            writer.setFileName(file);
            if (!writer.write(buffer.toImage()))
            {
                ERROR("Failed to write image '%1'", file);
                mNumErrors++;
            }
            const float pack_width  = packing_rect_result.width;
            const float pack_height = packing_rect_result.height;

            // create mapping for each source texture to the generated
            // texture.
            for (auto it = first_success; it != sources.end(); ++it)
            {
                const auto& rc = *it;
                GeneratedTextureEntry gen;
                gen.texture_file   = QString("pck://textures/%1").arg(name);
                gen.width          = (float)rc.width / pack_width;
                gen.height         = (float)rc.height / pack_height;
                gen.xpos           = (float)rc.xpos / pack_width;
                gen.ypos           = (float)rc.ypos / pack_height;
                relocation_map[rc.cookie] = gen;
                DEBUG("Packed %1 into %2", rc.cookie, gen.texture_file);
            }

            // done with these.
            sources.erase(first_success, sources.end());

            atlas_number++;
        }

        // update texture object mappings, file handles and texture boxes.
        // for each texture object, look up where the original file handle
        // maps to. Then the original texture box is now a box within a box.
        for (auto& pair : mTextureMap)
        {
            TextureSource& tex = pair.second;
            const auto original_file = tex.file;
            const auto original_rect = tex.rect;

            auto it = relocation_map.find(original_file);
            if (it == relocation_map.end()) // font texture sources only have texture box.
                continue;
            //ASSERT(it != relocation_map.end());
            const auto& relocation = it->second;

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
private:
    std::string CopyFile(const std::string& file, const QString& where)
    {
        auto it = mResourceMap.find(file);
        if (it != std::end(mResourceMap))
        {
            DEBUG("Skipping duplicate copy of '%1'", file);
            return it->second;
        }

        if (!app::MakePath(app::JoinPath(kOutDir, where)))
        {
            ERROR("Failed to create '%1/%2'", kOutDir, where);
            mNumErrors++;
            // todo: what to return on error ?
            return "";
        }

        // this will actually resolve the file path.
        // using the resolution scheme in Workspace.
        const QFileInfo info(app::FromUtf8(file));
        if (!info.exists())
        {
            ERROR("File %1 could not be found!", file);
            mNumErrors++;
            // todo: what to return on error ?
            return "";
        }

        // todo: resolve a case where two different source files
        // would map to same file in the package folder.
        // one needs to be renamed.

        const QString& name = info.fileName();
        const QString& src  = info.absoluteFilePath(); // resolved path.
        const QString& dst  = app::JoinPath(kOutDir, app::JoinPath(where, name));
        CopyFileBuffer(src, dst);

        // generate the resource identifier
        const auto& pckid = app::ToUtf8(QString("pck://%1/%2").arg(where).arg(name));

        mResourceMap[file] = pckid;
        return pckid;
    }
private:
    void CopyFileBuffer(const QString& src, const QString& dst)
    {
        // if src equals dst then we can actually skip the copy, no?
        if (src == dst)
        {
            DEBUG("Skipping copy of '%1' to '%2'", src, dst);
            return;
        }

        // we're doing this silly copying here since Qt doesn't
        // have a copy operation that's without race condition,
        // i.e. QFile::copy won't overwrite.
        QFile src_io(src);
        QFile dst_io(dst);
        if (!src_io.open(QIODevice::ReadOnly))
        {
            ERROR("Failed to open '%1' for reading (%2).", src, src_io.error());
            mNumErrors++;
            return;
        }
        if (!dst_io.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            ERROR("Failed to open '%1 for writing (%2).", dst, dst_io.error());
            mNumErrors++;
            return;
        }
        const auto& buffer = src_io.readAll();
        if (dst_io.write(buffer) == -1)
        {
            ERROR("Failed to write file '%1' (%2)", dst, dst_io.error());
            mNumErrors++;
            return;
        }
        mNumFilesCopied++;
        DEBUG("Copied %1 bytes from %2 to %3", buffer.count(), src, dst);
    }
private:
    const QString kOutDir;
    const unsigned kMaxTextureHeight;
    const unsigned kMaxTextureWidth;
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
    };
    std::unordered_map<ObjectHandle, TextureSource> mTextureMap;

    // resource mapping from source to packed resource id.
    std::unordered_map<std::string, std::string> mResourceMap;
};

} // namespace



namespace app
{

class SpacePartition
{
public:
    SpacePartition(unsigned x, unsigned y, unsigned width, unsigned height)
      : mXPos(x)
      , mYPos(y)
      , mWidth(width)
      , mHeight(height)
    {}

    bool Pack(PackingRectangle& img)
    {
        const auto w = img.width;
        const auto h = img.height;
        if (!mUsed)
        {
            const auto available_height = mBelow ? mHeight - mBelow->GetHeight() : mHeight;
            const auto available_width  = mRight ? mWidth - mRight->GetWidth() : mWidth;
            if (w <= available_width && h <= available_height)
            {
                if (!mRight)
                    mRight = std::make_unique<SpacePartition>(mXPos + w, mYPos, mWidth - w, h);
                if (!mBelow)
                    mBelow = std::make_unique<SpacePartition>(mXPos, mYPos + h, mWidth, mHeight - h);
                mImage  = &img;
                mUsed   = true;
                return true;
            }
        }
        else if (mRight && mRight->Pack(img))
            return true;
        else if (mBelow && mBelow->Pack(img))
            return true;
        return false;
    }
    unsigned GetHeight() const
    { return mHeight; }
    unsigned GetWidth() const
    { return mWidth; }

    // grow width and place old root to below
    void AccomodateBelow(std::unique_ptr<SpacePartition> old_root)
    {
        ASSERT(mWidth > old_root->mWidth);
        ASSERT(mHeight == old_root->mHeight);
        mUsed  = true;
        const auto width = mWidth - old_root->mWidth;
        mRight = std::make_unique<SpacePartition>(mXPos + old_root->mWidth, mYPos, width, mHeight);
        mBelow = std::move(old_root);
        mBelow->mXPos = mXPos;
        mBelow->mYPos = mYPos;
    }

    // grow height and place the old root to the right
    void AccomodateRight(std::unique_ptr<SpacePartition> old_root)
    {
        ASSERT(mWidth  == old_root->mWidth);
        ASSERT(mHeight > old_root->mHeight);
        mUsed  = true;
        const auto height = mHeight - old_root->mHeight;
        mBelow = std::make_unique<SpacePartition>(mXPos, mYPos + old_root->mHeight, mWidth, height);
        mRight = std::move(old_root);
        mRight->mXPos = mXPos;
        mRight->mYPos = mYPos;
    }
    void Finalize()
    {
        if (mImage)
        {
            mImage->xpos = mXPos;
            mImage->ypos = mYPos;
            mImage->success = true;
        }
        if (mRight)
            mRight->Finalize();
        if (mBelow)
            mBelow->Finalize();
    }

private:
    unsigned mXPos = 0;
    unsigned mYPos = 0;
    unsigned mWidth  = 0;
    unsigned mHeight = 0;
    PackingRectangle* mImage = nullptr;
    bool mUsed = false;

    std::unique_ptr<SpacePartition> mRight;
    std::unique_ptr<SpacePartition> mBelow;
};

// Binary tree bin packing algorithm
// https://codeincomplete.com/posts/bin-packing/

RectanglePackSize PackRectangles(std::vector<PackingRectangle>& images)
{
    RectanglePackSize ret;
    if (images.size() == 0)
        return ret;
    else if (images.size() == 1)
    {
        ret.height = images[0].height;
        ret.width  = images[0].width;
        images[0].xpos = 0;
        images[0].ypos = 0;
        return ret;
    }

    std::sort(std::begin(images), std::end(images),
        [](const PackingRectangle& first, const PackingRectangle& second) {
            const auto max0 = std::max(first.width, first.height);
            const auto max1 = std::max(second.width, second.height);
            return max0 < max1;
        });
    std::reverse(std::begin(images), std::end(images));

    auto root = std::make_unique<SpacePartition>(0, 0, images[0].width, images[0].height);
    for (auto& img : images)
    {
        if (root->Pack(img))
            continue;

        const auto root_width  = root->GetWidth();
        const auto root_height = root->GetHeight();

        // do we have enough width in order to grow height ?
        const auto can_grow_height = root_width >= img.width;
        // do we have enough height in order to grow width ?
        const auto can_grow_width  = root_height >= img.height;

        // preferred grow dimension
        const auto should_grow_width  = can_grow_width && root_height >= (root_width + img.width);
        const auto should_grow_height = can_grow_height && root_width >= (root_height + img.height);

        std::unique_ptr<SpacePartition> new_root;

        if (should_grow_width)
        {
            const auto new_width  = root_width + img.width;
            const auto new_height = root_height;
            new_root = std::make_unique<SpacePartition>(0, 0, new_width, new_height);
            new_root->AccomodateBelow(std::move(root));
        }
        else if (should_grow_height)
        {
            const auto new_width  = root_width;
            const auto new_height = root_height + img.height;
            new_root = std::make_unique<SpacePartition>(0, 0, new_width, new_height);
            new_root->AccomodateRight(std::move(root));
        }
        else if (can_grow_height)
        {
            const auto new_width  = root_width;
            const auto new_height = root_height + img.height;
            new_root = std::make_unique<SpacePartition>(0, 0, new_width, new_height);
            new_root->AccomodateRight(std::move(root));
        }
        else if (can_grow_width)
        {
            const auto new_width  = root_width + img.width;
            const auto new_height = root_height;
            new_root = std::make_unique<SpacePartition>(0, 0, new_width, new_height);
            new_root->AccomodateBelow(std::move(root));
        }

        root = std::move(new_root);
        root->Pack(img);
    }
    root->Finalize();
    ret.height = root->GetHeight();
    ret.width  = root->GetWidth();
    return ret;
}

bool PackRectangles(const RectanglePackSize& max, std::vector<PackingRectangle>& list,  RectanglePackSize* ret_pack_size)
{
    if (list.empty())
        return true;

    // sort into descending order (biggest ones first)
    std::sort(std::begin(list), std::end(list),
        [](const auto& lhs, const auto& rhs) {
            const auto max0 = std::max(lhs.width, lhs.height);
            const auto max1 = std::max(rhs.width, rhs.height);
            return max0 > max1;
        });
    bool all_ok = true;
    auto root = std::make_unique<SpacePartition>(0, 0, max.width, max.height);
    for (auto& img : list)
    {
        if (!root->Pack(img))
            all_ok = false;
    }
    root->Finalize();
    if (ret_pack_size)
    {
        // compute the actual minimum box based on the content
        unsigned width  = 0;
        unsigned height = 0;
        for (const auto& item : list)
        {
            const auto right_edge = item.xpos + item.width;
            const auto bottom_edge = item.ypos + item.height;
            width = std::max(right_edge, width);
            height = std::max(bottom_edge, height);
        }
        ret_pack_size->height = height;
        ret_pack_size->width  = width;
    }
    return all_ok;
}

bool PackContent(const std::vector<const Resource*>& resources, const ContentPackingOptions& options)
{
    const QString& outdir = JoinPath(options.directory, options.package_name);
    if (!MakePath(outdir))
    {
        ERROR("Failed to create %1", outdir);
        return false;
    }
    // filename of the JSON based descriptor that contains all the
    // resource definitions.
    const auto& json_filename = JoinPath(outdir, "content.json");

    QFile json_file(json_filename);
    if (!json_file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open file: '%1' for writing (%2)", json_filename, json_file.error());
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
        mutable_copies.push_back(resource->Clone());
    }

    ResourcePacker packer(outdir, options.max_texture_width, options.max_texture_height,
        options.resize_textures, options.combine_textures);

    // collect the resources in the packer.
    for (const auto& resource : mutable_copies)
    {
        if (resource->IsMaterial())
        {
            // todo: maybe move to Resource interface ?
            const gfx::Material* material = nullptr;
            resource->GetContent(&material);
            material->BeginPacking(&packer);
        }
        else if (resource->IsParticleEngine())
        {
            const gfx::KinematicsParticleEngine* engine = nullptr;
            resource->GetContent(&engine);
            engine->Pack(&packer);
        }
    }

    packer.PackTextures();

    for (const auto& resource : mutable_copies)
    {
        if (resource->IsMaterial())
        {
            // todo: maybe move to resource interface ?
            gfx::Material* material = nullptr;
            resource->GetContent(&material);
            material->FinishPacking(&packer);
        }
    }

    // finally serialize
    nlohmann::json json;
    json["json_version"]  = 1;
    json["made_with_app"] = APP_TITLE;
    json["made_with_ver"] = APP_VERSION;
    for (const auto& resource : mutable_copies)
    {
        resource->Serialize(json);
    }

    const auto& str = json.dump(2);
    if (json_file.write(&str[0], str.size()) == -1)
    {
        ERROR("Failed to write JSON file: '%1' %2", json_filename, json_file.error());
        return false;
    }
    json_file.flush();
    json_file.close();
    if (const auto errors = packer.GetNumErrors())
    {
        WARN("Resource packing completed with errors (%1).", errors);
        WARN("Please see the log file for details about errors.");
    }
    else
    {
        INFO("Packed %1 resource(s) into %2 succesfully.", resources.size(), outdir);
        //INFO("%1 files copied.", packer.GetNumFilesCopied());
    }
    return packer.GetNumErrors() == 0;
}


} // namespace
