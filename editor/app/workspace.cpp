// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
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
#include "editor/app/utility.h"
#include "graphics/resource.h"

namespace {

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

    using TexturePackingProgressCallback = std::function<void (std::string, int, int)>;

    void PackTextures(TexturePackingProgressCallback  progress)
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
        // -- composite the actual image files.
        // 4. copy the src image contents into the container image.
        // 5. write the container/packed image into the package folder
        // 6. update the textures whose source images were packaged (the file handle and the rectangle box)

        int cur_step = 0;
        int max_step = static_cast<int>(mTextureMap.size());

        for (auto it = mTextureMap.begin(); it != mTextureMap.end(); ++it)
        {
            progress("Copying textures...", cur_step++, max_step);

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

            dupemap.insert(tex.file);
        }

        unsigned atlas_number = 0;
        cur_step = 0;
        max_step = static_cast<int>(sources.size());

        while (!sources.empty())
        {
            progress("Packing textures...", cur_step++, max_step);

            app::RectanglePackSize packing_rect_result;
            app::PackRectangles({kMaxTextureWidth, kMaxTextureHeight}, sources, &packing_rect_result);
            // ok, some of the textures might have failed to pack on this pass.
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
        dst_io.setPermissions(src_io.permissions());
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

Workspace::Workspace()
{
    DEBUG("Create workspace");

    // initialize the primitive resources, i.e the materials
    // and drawables that are part of the workspace without any
    // user interaction.

    // Checkerboard is a special material that is always available.
    // It is used as the initial material when user hasn't selected
    // anything or when the material referenced by some object is deleted
    // the material reference can be updated to Checkerboard.
    auto checkerboard = gfx::TextureMap("app://textures/Checkerboard.png");
    checkerboard.SetId("_checkerboard");
    mResources.emplace_back(new MaterialResource(std::move(checkerboard), "Checkerboard"));

    // add some primitive colors.
    constexpr auto& values = magic_enum::enum_values<gfx::Color>();
    for (const auto& val : values)
    {
        const std::string color_name(magic_enum::enum_name(val));
        auto color = gfx::SolidColor(gfx::Color4f(val));
        color.SetId("_" + color_name);
        mResources.emplace_back(new MaterialResource(std::move(color), FromUtf8(color_name)));
    }

    mResources.emplace_back(new DrawableResource<gfx::CapsuleClass>("Capsule"));
    mResources.emplace_back(new DrawableResource<gfx::RectangleClass>("Rectangle"));
    mResources.emplace_back(new DrawableResource<gfx::IsocelesTriangleClass>("IsoscelesTriangle"));
    mResources.emplace_back(new DrawableResource<gfx::RightTriangleClass>("RightTriangle"));
    mResources.emplace_back(new DrawableResource<gfx::CircleClass>("Circle"));
    mResources.emplace_back(new DrawableResource<gfx::RoundRectangleClass>("RoundRect"));
    mResources.emplace_back(new DrawableResource<gfx::TrapezoidClass>("Trapezoid"));
    mResources.emplace_back(new DrawableResource<gfx::ParallelogramClass>("Parallelogram"));

    for (auto& resource : mResources)
    {
        resource->SetIsPrimitive(true);
    }
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

std::shared_ptr<gfx::Material> Workspace::MakeMaterialByName(const QString& name) const
{
    return gfx::CreateMaterialInstance(GetMaterialClassByName(name));

}
std::shared_ptr<gfx::Drawable> Workspace::MakeDrawableByName(const QString& name) const
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
    ERROR("Request for a material that doesn't exist: '%1'", name);
    return GetMaterialClassByName(QString("Checkerboard"));
}

std::shared_ptr<const gfx::MaterialClass> Workspace::GetMaterialClassByName(const char* name) const
{
    // convenience shim
    return GetMaterialClassByName(QString::fromUtf8(name));
}
std::shared_ptr<const gfx::DrawableClass> Workspace::GetDrawableClassByName(const QString& name) const
{
    for (const auto& resource : mResources)
    {
        if (!(resource->GetType() == Resource::Type::CustomShape ||
              resource->GetType() == Resource::Type::ParticleSystem ||
              resource->GetType() == Resource::Type::Drawable))
            continue;
        else if (resource->GetName() != name)
            continue;

        if (resource->GetType() == Resource::Type::Drawable)
            return ResourceCast<gfx::DrawableClass>(*resource).GetSharedResource();
        if (resource->GetType() == Resource::Type::ParticleSystem)
            return ResourceCast<gfx::KinematicsParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::CustomShape)
            return ResourceCast<gfx::PolygonClass>(*resource).GetSharedResource();
    }
    ERROR("Request for a drawable that doesn't exist: '%1'", name);
    return std::make_shared<gfx::RectangleClass>();
}
std::shared_ptr<const gfx::DrawableClass> Workspace::GetDrawableClassByName(const char* name) const
{
    // convenience shim
    return GetDrawableClassByName(QString::fromUtf8(name));
}

std::shared_ptr<const game::AnimationClass> Workspace::GetAnimationClassByName(const QString& name) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Animation)
            continue;
        else if (resource->GetName() != name)
            continue;
        return ResourceCast<game::AnimationClass>(*resource).GetSharedResource();
    }
    ERROR("Request for an animation that doesn't exist: '%1'", name);
    return nullptr;
}

std::shared_ptr<const game::AnimationClass> Workspace::GetAnimationClassById(const QString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Animation)
            continue;
        else if (resource->GetId() != id)
            continue;
        return ResourceCast<game::AnimationClass>(*resource).GetSharedResource();
    }
    ERROR("Request for an animation that doesn't exist: '%1'", id);
    return nullptr;
}

std::shared_ptr<const gfx::MaterialClass> Workspace::GetMaterialClass(const std::string& klass) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Material)
            continue;
        else if (resource->GetIdUtf8() != klass)
            continue;
        return ResourceCast<gfx::MaterialClass>(*resource).GetSharedResource();
    }
    ERROR("Request for a material that doesn't exist: '%1'", klass);
    return GetMaterialClass(std::string("_checkerboard"));
}

std::shared_ptr<const gfx::DrawableClass> Workspace::GetDrawableClass(const std::string& klass) const
{
    //            == About resource loading ==
    // User defined resources have a combination of type and name
    // where type is the underlying class type and name identifies
    // the set of resources that the user edits that instances of that
    // said type then use.
    // Primitive / (non user defined resources) don't need a name
    // since the name is irrelevant since the objects are stateless
    // in this sense and don't have properties that would change between instances.
    // For example with drawable rectangles (gfx::Rectangle) all rectangles
    // that we might want to draw are basically the same. We don't need to
    // configure each rectangle object with properties that would distinguish
    // it from other rectangles.
    // In fact there's basically even no need to create more than 1 instance
    // of such resource and then share it between all users.
    //
    // User defined resources on the other hand *can be* unique.
    // For example particle engines, the underlying object type is same for
    // particle engine A and B, but their defining properties are completely
    // different. To distinguish the set of properties the system gives each
    // particle engine a unique "class id". The class is then used to identify
    // the class object instance.
    // Additionally the resources may or may not be
    // shared. For example when a fleet of alien spaceships are rendered
    // each spaceship might have their own particle engine (own simulation state)
    // thus producing a unique rendering for each spaceship. However the problem
    // is that this might be computationally heavy. For N ships we'd need to do
    // N particle engine simulations.
    // However it's also possible that the particle engines are shared and each
    // ship (of the same type) just refers to the same particle engine. Then
    // each ship will render the same particle stream.

    for (const auto& resource : mResources)
    {
        if (resource->GetIdUtf8() != klass)
            continue;

        if (resource->GetType() == Resource::Type::Drawable)
            return ResourceCast<gfx::DrawableClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::ParticleSystem)
            return ResourceCast<gfx::KinematicsParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::CustomShape)
            return ResourceCast<gfx::PolygonClass>(*resource).GetSharedResource();
    }
    ERROR("Request for a drawable that doesn't exist: '%1'", klass);
    return std::make_shared<gfx::RectangleClass>();
}

std::shared_ptr<gfx::Material> Workspace::MakeMaterial(const std::string& name) const
{
    return gfx::CreateMaterialInstance(GetMaterialClass(name));
}

std::shared_ptr<gfx::Drawable> Workspace::MakeDrawable(const std::string& name) const
{
    return gfx::CreateDrawableInstance(GetDrawableClass(name));
}

std::string Workspace::ResolveFile(gfx::ResourceLoader::ResourceType type, const std::string& file) const
{
    // see comments in AddFile about resource path mapping.
    // this method is only called by the graphics/ subsystem

    QString ret = FromUtf8(file);
    if (ret.startsWith("ws://"))
        ret.replace("ws://", mWorkspaceDir);
    else if (ret.startsWith("app://"))
        ret.replace("app://", GetAppDir());
    else if (ret.startsWith("fs://"))
        ret.remove(0, 5);

    // special case for resources that are hard coded in the engine without
    // any resource location identifier. (such as shaders)
    // if it's just relative we expect it's relative to the application's
    // installation folder.
    const QFileInfo info(ret);
    if (info.isRelative())
    {
        // assuming it's a resource deployed as part of this application.
        // so we resolve the path based on the applications's
        // installation location. (for example a shader file)
        ret = JoinPath(GetAppDir(), ret);
    }

    DEBUG("Mapping gfx resource '%1' => '%2'", file, ret);

    return ToUtf8(ret);
}

bool Workspace::LoadWorkspace(const QString& dir)
{
    ASSERT(!mIsOpen);

    if (!LoadContent(JoinPath(dir, "content.json")) ||
        !LoadProperties(JoinPath(dir, "workspace.json")))
        return false;

    // we don't really care if this fails or not. nothing permanently
    // important should be stored in this file. I.e deleting it
    // will just make the application forget some data that isn't
    // crucial for the operation of the application or for the
    // integrity of the workspace and its content.
    LoadUserSettings(JoinPath(dir, ".workspace_private.json"));

    INFO("Loaded workspace '%1'", dir);
    mWorkspaceDir = CleanPath(dir);
#if defined(POSIX_OS)
    if (!mWorkspaceDir.endsWith("/"))
        mWorkspaceDir.append("/");
#elif defined(WINDOWS_OS)
    if (!mWorkspaceDir.endsWith("\\"))
        mWorkspaceDir.append("\\");
#endif
    mIsOpen = true;
    return true;
}

bool Workspace::MakeWorkspace(const QString& directory)
{
    ASSERT(!mIsOpen);

    QDir dir;
    if (!dir.mkpath(directory))
    {
        ERROR("Failed to create workspace directory. '%1'", directory);
        return false;
    }

    // this is where we could initialize the workspace with some resources
    // or whatnot.
    mWorkspaceDir = CleanPath(directory);
#if defined(POSIX_OS)
    if (!mWorkspaceDir.endsWith("/"))
        mWorkspaceDir.append("/");
#elif defined(WINDOWS_OS)
    if (!mWorkspaceDir.endsWith("\\"))
        mWorkspaceDir.append("\\");
#endif
    mIsOpen = true;
    return true;
}

bool Workspace::SaveWorkspace()
{
    ASSERT(!mWorkspaceDir.isEmpty());

    if (!SaveContent(JoinPath(mWorkspaceDir, "content.json")) ||
        !SaveProperties(JoinPath(mWorkspaceDir, "workspace.json")))
        return false;

    // should we notify the user if this fails or do we care?
    SaveUserSettings(JoinPath(mWorkspaceDir, ".workspace_private.json"));

    INFO("Saved workspace '%1'", mWorkspaceDir);
    return true;
}

void Workspace::CloseWorkspace()
{
    // remove all non-primitive resources.
    QAbstractTableModel::beginResetModel();
    mResources.erase(std::remove_if(mResources.begin(), mResources.end(),
                                    [](const auto& res) { return !res->IsPrimitive(); }),
                     mResources.end());
    QAbstractTableModel::endResetModel();

    mVisibleCount = 0;

    mProperties.clear();
    mUserProperties.clear();
    mWorkspaceDir.clear();
    mSettings = ProjectSettings {};
    mIsOpen   = false;
}

QString Workspace::GetName() const
{
    return mWorkspaceDir;
}

QString Workspace::AddFileToWorkspace(const QString& filepath)
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
std::string Workspace::AddFileToWorkspace(const std::string& file)
{
    return ToUtf8(AddFileToWorkspace(FromUtf8(file)));
}

QString Workspace::MapFileToFilesystem(const QString& file) const
{
    // see comments in AddFileToWorkspace.
    // this is basically the same as MapFilePath except this API
    // is internal to only this application whereas MapFilePath is part of the
    // API exposed to the graphics/ subsystem.

    QString ret = file;
    if (ret.startsWith("ws://"))
        ret = CleanPath(ret.replace("ws://", mWorkspaceDir));
    else if (file.startsWith("app://"))
        ret = CleanPath(ret.replace("app://", GetAppDir()));
    else if (file.startsWith("fs://"))
        ret.remove(0, 5);

    // return as is
    return ret;
}

template<typename ClassType>
void LoadResources(const std::string& array,
                   const nlohmann::json& json,
                   std::vector<std::unique_ptr<Resource>>& vector)
{
    DEBUG("Loading %1", array);

    if (!json.contains(array))
        return;
    for (const auto& object : json[array].items())
    {
        const auto& value = object.value();
        std::string name;
        std::string id;
        if (!base::JsonReadSafe(value, "resource_name", &name) ||
            !base::JsonReadSafe(value, "resource_id", &id))
        {
            ERROR("Unexpected JSON. Maybe old workspace version?");
            continue;
        }
        std::optional<ClassType> ret = ClassType::FromJson(value);
        if (!ret.has_value())
        {
            ERROR("Failed to load resource '%1'", name);
            continue;
        }
        vector.push_back(std::make_unique<GameResource<ClassType>>(std::move(ret.value()), FromUtf8(name)));
        DEBUG("Loaded resource '%1'", name);
    }
}

bool Workspace::LoadContent(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }
    const auto& buff = file.readAll(); // QByteArray
    if (buff.isEmpty())
    {
        // odd but not necessarily wrong. Could be an empty workspace
        // without any content.
        WARN("No workspace content found in file: '%1'", filename);
        return true;
    }

    // Warning about nlohmann::json
    // !! SEMANTICS CHANGE BETWEEN DEBUG AND RELEASE BUILD !!
    //
    // Trying to access an attribute using operator[] does not check
    // whether a given key exists. instead it uses a standard CRT assert
    // which then changes semantics depending whether NDEBUG is defined
    // or not.

    const auto* beg  = buff.data();
    const auto* end  = buff.data() + buff.size();
    auto [ok, json, error] = base::JsonParse(beg, end);
    if (!ok)
    {
        ERROR("Failed to parse JSON file: '%1'", filename);
        ERROR("JSON parse error: '%1'", error);
        return false;
    }

    LoadResources<gfx::MaterialClass>("materials", json, mResources);
    LoadResources<gfx::KinematicsParticleEngineClass>("particles", json, mResources);
    LoadResources<gfx::PolygonClass>("shapes", json, mResources);
    LoadResources<game::AnimationClass>("animations", json, mResources);

    // setup an invariant that states that the primitive materials
    // are in the list of resources after the user defined ones.
    // this way the the addressing scheme (when user clicks on an item
    // in the list of resources) doesn't need to change and it's possible
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
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }

    nlohmann::json json;
    for (const auto& resource : mResources)
    {
        // skip persisting primitive resources since they're always
        // created as part of the workspace creation and their resource
        // IDs are fixed.
        if (resource->IsPrimitive())
            continue;
        // serialize the user defined resource.
        resource->Serialize(json);
    }

    if (json.is_null())
    {
        WARN("Workspace contains no actual data. Skipped saving.");
        return true;
    }

    const auto& str = json.dump(2);
    if (file.write(&str[0], str.size()) == -1)
    {
        ERROR("File write failed. '%1'", filename);
        return false;
    }
    file.flush();
    file.close();

    INFO("Saved workspace content in '%1'", filename);
    return true;
}

bool Workspace::SaveProperties(const QString& filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }

    // our JSON root object
    QJsonObject json;

    QJsonObject project;
    project["multisample_sample_count"] = (int)mSettings.multisample_sample_count;
    project["application_name"]         = mSettings.application_name;
    project["application_version"]      = mSettings.application_version;
    project["application_library"]      = mSettings.application_library;
    project["default_min_filter"]       = toString(mSettings.default_min_filter);
    project["default_mag_filter"]       = toString(mSettings.default_mag_filter);
    project["window_mode"]              = toString(mSettings.window_mode);
    project["window_width"]             = (int)mSettings.window_width;
    project["window_height"]            = (int)mSettings.window_height;
    project["window_can_resize"]        = mSettings.window_can_resize;
    project["window_has_border"]        = mSettings.window_has_border;
    project["window_vsync"]             = mSettings.window_vsync;
    project["ticks_per_second"]         = (int)mSettings.ticks_per_second;
    project["updates_per_second"]       = (int)mSettings.updates_per_second;
    project["working_folder"]           = mSettings.working_folder;
    project["command_line_arguments"]   = mSettings.command_line_arguments;
    project["use_gamehost_process"]     = mSettings.use_gamehost_process;

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
    mSettings.multisample_sample_count = project["multisample_sample_count"].toInt();
    mSettings.application_name       = project["application_name"].toString();
    mSettings.application_version    = project["application_version"].toString();
    mSettings.application_library    = project["application_library"].toString();
    mSettings.default_min_filter     = EnumFromString(project["default_min_filter"].toString(),
        gfx::Device::MinFilter::Nearest);
    mSettings.default_mag_filter     = EnumFromString(project["default_mag_filter"].toString(),
        gfx::Device::MagFilter::Nearest);
    mSettings.window_mode            = EnumFromString(project["window_mode"].toString(),
        ProjectSettings::WindowMode::Windowed);
    mSettings.window_width           = project["window_width"].toInt();
    mSettings.window_height          = project["window_height"].toInt();
    mSettings.window_can_resize      = project["window_can_resize"].toBool();
    mSettings.window_has_border      = project["window_has_border"].toBool();
    mSettings.window_vsync           = project["window_vsync"].toBool();
    mSettings.ticks_per_second       = project["ticks_per_second"].toInt();
    mSettings.updates_per_second     = project["updates_per_second"].toInt();
    mSettings.working_folder         = project["working_folder"].toString();
    mSettings.command_line_arguments = project["command_line_arguments"].toString();
    mSettings.use_gamehost_process   = project["use_gamehost_process"].toBool();

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

QStringList Workspace::ListAllMaterials() const
{
    QStringList list;
    list << ListPrimitiveMaterials();
    list << ListUserDefinedMaterials();
    return list;
}

QStringList Workspace::ListPrimitiveMaterials() const
{
    QStringList list;
    for (const auto& resource : mResources)
    {
        if (resource->IsPrimitive() &&
            resource->GetType() == Resource::Type::Material)
            list.append(resource->GetName());
    }
    list.sort();
    return list;
}

QStringList Workspace::ListUserDefinedMaterials() const
{
    QStringList list;
    for (const auto& resource : mResources)
    {
        if (!resource->IsPrimitive() &&
             resource->GetType() == Resource::Type::Material)
            list.append(resource->GetName());
    }
    return list;
}

QStringList Workspace::ListAllDrawables() const
{
    QStringList list;
    list << ListPrimitiveDrawables();
    list << ListUserDefinedDrawables();
    return list;
}

QStringList Workspace::ListPrimitiveDrawables() const
{
    QStringList list;
    for (const auto& resource : mResources)
    {
        if (resource->IsPrimitive() &&
            (resource->GetType() == Resource::Type::ParticleSystem ||
             resource->GetType() == Resource::Type::CustomShape ||
             resource->GetType() == Resource::Type::Drawable))
            list.append(resource->GetName());
    }
    list.sort();
    return list;
}

QStringList Workspace::ListUserDefinedDrawables() const
{
    QStringList list;
    for (const auto& resource : mResources)
    {
        if (!resource->IsPrimitive() &&
                (resource->GetType() == Resource::Type::ParticleSystem ||
                 resource->GetType() == Resource::Type::CustomShape ||
                 resource->GetType() == Resource::Type::Drawable))
            list.append(resource->GetName());
    }
    list.sort();
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
        res->UpdateFrom(resource);
        emit ResourceUpdated(mResources[i].get());
        emit dataChanged(index(i, 0), index(i, 2));
        return;
    }
    // if we're here no such resource exists yet.
    // Create a new resource and add it to the list of resources.
    beginInsertRows(QModelIndex(), mVisibleCount, mVisibleCount);
    // insert at the end of the visible range which is from [0, mVisibleCount)
    mResources.insert(mResources.begin() + mVisibleCount, resource.Copy());
    endInsertRows();

    auto& back = mResources[mVisibleCount];
    emit NewResourceAvailable(back.get());
    mVisibleCount++;
}

QString Workspace::MapDrawableName(const QString &id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id)
            return resource->GetName();
    }
    ERROR("No such drawable class '%1'", id);
    return "Rectangle";
}

QString Workspace::MapMaterialName(const QString &id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id)
            return resource->GetName();
    }
    ERROR("No such material class '%1'", id);
    return "Checkerboard";
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
             resource->GetType() == Resource::Type::CustomShape ||
             resource->GetType() == Resource::Type::Drawable))
            return true;
    }
    return false;
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

    std::sort(indices.begin(), indices.end(), std::less<size_t>());

    // because the high probability of unwanted recursion
    // messing this iteration up (for example by something
    // calling back to this workspace from Resource
    // deletion signal handler and adding a new resource) we
    // must take some special care here.
    // So therefore first put the resources to be deleted into
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
}

void Workspace::DuplicateResources(const QModelIndexList& list)
{
    std::vector<size_t> indices;
    for (const auto& i : list)
        indices.push_back(i.row());
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

void Workspace::Tick()
{

}

bool Workspace::PackContent(const std::vector<const Resource*>& resources, const ContentPackingOptions& options)
{
    const QString& outdir = JoinPath(options.directory, options.package_name);
    if (!MakePath(outdir))
    {
        ERROR("Failed to create %1", outdir);
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

    ResourcePacker packer(outdir, options.max_texture_width, options.max_texture_height,
        options.resize_textures, options.combine_textures);

    // not all shaders are referenced by user defined resources
    // so for now just copy all the shaders that we discover over to
    // the output folder.
    QStringList filters;
    filters << "*.glsl";
    const auto& appdir  = QCoreApplication::applicationDirPath();
    const auto& glsldir = app::JoinPath(appdir, "shaders/es2");
    QDir dir;
    dir.setPath(glsldir);
    dir.setNameFilters(filters);
    const QStringList& files = dir.entryList();
    for (int i=0; i<files.size(); ++i)
    {
        emit ResourcePackingUpdate("Copying shaders...", i, files.size());
        const QFile file(files[i]);
        const QFileInfo info(file);
        const QString& name = "shaders/es2/" + info.fileName();
        packer.CopyFile(app::ToUtf8(name), "shaders/es2");
    }

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

    packer.PackTextures([this](const std::string& action, int step, int max) {
        emit ResourcePackingUpdate(FromLatin(action), step, max);
    });

    for (int i=0; i<mutable_copies.size(); ++i)
    {
        const auto& resource = mutable_copies[i];
        emit ResourcePackingUpdate("Updating resources...", i, mutable_copies.size());
        if (resource->IsMaterial())
        {
            // todo: maybe move to resource interface ?
            gfx::MaterialClass* material = nullptr;
            resource->GetContent(&material);
            material->FinishPacking(&packer);
        }
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
            ERROR("Failed to open file: '%1' for writing (%2)", json_filename, json_file.error());
            return false;
        }

        // finally serialize
        nlohmann::json json;
        json["json_version"] = 1;
        json["made_with_app"] = APP_TITLE;
        json["made_with_ver"] = APP_VERSION;
        for (const auto &resource : mutable_copies)
        {
            resource->Serialize(json);
        }

        const auto &str = json.dump(2);
        if (json_file.write(&str[0], str.size()) == -1)
        {
            ERROR("Failed to write JSON file: '%1' %2", json_filename, json_file.error());
            return false;
        }
        json_file.flush();
        json_file.close();
    }

    // write config file?
    if (options.write_config_file)
    {
        emit ResourcePackingUpdate("Writing config JSON file...", 0, 0);

        const auto& json_filename = JoinPath(outdir, "config.json");
        QFile json_file;
        json_file.setFileName(json_filename);
        json_file.open(QIODevice::WriteOnly);
        if (!json_file.isOpen())
        {
            ERROR("Failed to open file: '%1' for writing (%2)", json_filename, json_file.error());
            return false;
        }
        nlohmann:: json json;
        json["json_version"]  = 1;
        json["made_with_app"] = APP_TITLE;
        json["made_with_ver"] = APP_VERSION;
        json["config"]["red_size"]     = 8;
        json["config"]["green_size"]   = 8;
        json["config"]["blue_size"]    = 8;
        json["config"]["alpha_size"]   = 8;
        json["config"]["stencil_size"] = 8;
        json["config"]["depth_size"]   = 0;
        if (mSettings.multisample_sample_count == 0)
            json["config"]["sampling"] = "None";
        else if (mSettings.multisample_sample_count == 4)
            json["config"]["sampling"] = "MSAA4";
        else if (mSettings.multisample_sample_count == 8)
            json["config"]["sampling"] = "MSAA8";
        else if (mSettings.multisample_sample_count == 16)
            json["config"]["sampling"] = "MSAA16";
        json["window"]["width"]  = mSettings.window_width;
        json["window"]["height"] = mSettings.window_height;
        json["window"]["can_resize"] = mSettings.window_can_resize;
        json["window"]["has_border"] = mSettings.window_has_border;
        json["window"]["vsync"]      = mSettings.window_vsync;
        if (mSettings.window_mode == ProjectSettings::WindowMode::Windowed)
            json["window"]["set_fullscreen"] = false;
        else if (mSettings.window_mode == ProjectSettings::WindowMode::Fullscreen)
            json["window"]["set_fullscreen"] = true;
        json["application"]["title"]   = ToUtf8(mSettings.application_name);
        json["application"]["version"] = ToUtf8(mSettings.application_version);
        json["application"]["library"] = ToUtf8(mSettings.application_library);
        json["application"]["ticks_per_second"]   = (float)mSettings.ticks_per_second;
        json["application"]["updates_per_second"] = (float)mSettings.updates_per_second;
        json["application"]["content"] = "content.json";

        const auto& str = json.dump(2);
        if (json_file.write(&str[0], str.size()) == -1)
        {
            ERROR("Failed to write JSON file: '%1' '%2'", json_filename, json_file.error());
            return false;
        }
        json_file.flush();
        json_file.close();
    }

    if (const auto errors = packer.GetNumErrors())
    {
        WARN("Resource packing completed with errors (%1).", errors);
        WARN("Please see the log file for details about errors.");
        return false;
    }
    // Copy game main executable.
    std::string runner = "GameMain";
#if defined(WINDOWS_OS)
    runner.append(".exe");
#endif
    packer.CopyFile(runner, "");

    INFO("Packed %1 resource(s) into %2 successfully.", resources.size(), outdir);
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
