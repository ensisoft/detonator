// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#define LOGTAG "gui"

#include "polygonwidget.h"

#include "warnpush.h"
#  include <QMessageBox>
#  include <QVector2D>
#  include <QAbstractTableModel>
#  include <base64/base64.h>
#include "warnpop.h"

#include <cmath>
#include <algorithm>

#include "base/math.h"
#include "base/utility.h"
#include "data/json.h"
#include "engine/camera.h"
#include "graphics/device_algo.h"
#include "graphics/framebuffer.h"
#include "graphics/material.h"
#include "graphics/material_class.h"
#include "graphics/material_instance.h"
#include "graphics/drawable.h"
#include "graphics/drawing.h"
#include "graphics/transform.h"
#include "graphics/painter.h"
#include "graphics/paint_context.h"
#include "graphics/utility.h"
#include "graphics/linebatch.h"
#include "graphics/simple_shape.h"
#include "graphics/shader_programs.h"
#include "graphics/polygon_mesh.h"
#include "graphics/guidegrid.h"
#include "graphics/texture.h"
#include "graphics/device.h"
#include "editor/app/eventlog.h"
#include "editor/app/format.h"
#include "editor/app/resource-uri.h"
#include "editor/gui/polygonwidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/settings.h"
#include "editor/gui/dlgtextedit.h"
#include "editor/gui/drawing.h"
#include "editor/gui/gfxmenu.h"
#include "graphics/vertex_algo.h"

namespace {

constexpr auto Margin = 20; // pixels

template<typename VertexType>
std::vector<VertexType> MakeVerts2D(const std::vector<QPoint>& points, float width, float height)
{
    std::vector<VertexType> vertices;
    for (const auto& p : points)
    {
        VertexType vertex;
        vertex.aPosition.x = p.x() / width;
        vertex.aPosition.y = p.y() / height * -1.0;
        vertex.aTexCoord.x = p.x() / width;
        vertex.aTexCoord.y = p.y() / height;
        vertices.push_back(vertex);
    }
    return vertices;
}

bool FindAxonometricTextureSize(const gfx::Material& material, gfx::Device& device,
    unsigned* texture_width, unsigned* texture_height)
{
    const auto* material_class = material.GetClass();
    if (!material_class)
        return false;

    if (material_class->GetNumTextureMaps() != 1)
        return false;

    const auto* texture_map = material_class->GetTextureMap(0);
    if (texture_map->GetNumTextures() != 1)
        return false;

    const auto* tile_texture_src = texture_map->GetTextureSource(0);
    const auto& tile_texture_gpu_id = tile_texture_src->GetGpuId();
    const auto* dimetric_tile_texture = device.FindTexture(tile_texture_gpu_id);
    if (!dimetric_tile_texture)
        return false;

    const auto whole_texture_width = dimetric_tile_texture->GetWidth();
    const auto whole_texture_height = dimetric_tile_texture->GetHeight();

    const auto& tile_rect = texture_map->GetTextureRect(0);
    *texture_width = tile_rect.GetWidth() * whole_texture_width;
    *texture_height = tile_rect.GetHeight() * whole_texture_height;
    return true;
}

gfx::Texture* RenderDimetricGuide(const std::string& gpu_id, gfx::Device& device,
    unsigned texture_width, unsigned texture_height)
{
    // expect the texture height to be an even multiple of the texture width.
    // the base case for isometric tiles is that tiles have a regular size
    // such as 64x64 px, but it's possible to have tiles that have tall objects
    // such as "walls", "trees" etc and the rendered image is then taller
    // for example 256x512 px.
    if (texture_height % texture_width)
        return nullptr;

    auto* texture = device.FindTexture(gpu_id);
    if (!texture)
    {
        texture = device.MakeTexture(gpu_id);
        texture->SetName("Dimetric Guide");
    }
    if (texture->GetWidth() != texture_width || texture->GetHeight() != texture_height)
        texture->Allocate(texture_width, texture_height, gfx::Texture::Format::sRGBA);

    auto* fbo = device.FindFramebuffer("PolygonWidgetFBO");
    if (!fbo)
    {
        fbo = device.MakeFramebuffer("PolygonWidgetFBO");
        gfx::Framebuffer::Config conf;
        conf.width  = 0; // irrelevant since using texture target
        conf.height = 0; // irrelevant since using texture target.
        conf.format = gfx::Framebuffer::Format::ColorRGBA8;
        conf.msaa   = gfx::Framebuffer::MSAA::Enabled;
        fbo->SetConfig(conf);
    }
    fbo->SetColorTarget(texture);
    device.ClearColor(gfx::Color::Transparent, fbo);

    gfx::Painter tile_painter;
    tile_painter.SetFramebuffer(fbo);
    tile_painter.SetDevice(&device);
    tile_painter.SetSurfaceSize(texture_width, texture_height);
    tile_painter.SetPixelRatio({1.0f, 1.0f});
    if (texture_height > texture_width)
    {
        tile_painter.SetViewport(0, static_cast<int>(texture_height) - static_cast<int>(texture_width),
                                 texture_width, texture_width);
        tile_painter.SetProjectionMatrix(engine::CreateProjectionMatrix(engine::Projection::Orthographic, texture_width, texture_width));
    }
    else
    {
        tile_painter.SetViewport(0, 0, texture_width, texture_height);
        tile_painter.SetProjectionMatrix(engine::CreateProjectionMatrix(engine::Projection::Orthographic, texture_width, texture_height));
    }
    tile_painter.SetViewMatrix(engine::CreateModelViewMatrix(engine::GameView::Dimetric, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f));

    // solve the size of the tile based on the Pythagoras theorem
    // a² + b² = c²
    // we know that the tile is a square so each side is equal length
    // thus we have
    //      2a² = c²
    //       a² = c² / 2
    // sqrt(a²) = sqrt(c²)/sqrt(2)
    //        a = c/sqrt(2.0)
    //
    // we can figure out the hypotenuse for this equation by projecting
    // a point from our scene space visible edge to the tile plane and
    // doubling that distance.

    const auto scene_edge_point = static_cast<float>(texture_width) / 2.0f;
    const auto tile_edge_point = engine::MapFromViewPlaneToGamePlane(glm::vec4 { scene_edge_point, 0.0f, 0.0f, 0.0f}, engine::GameView::Dimetric);
    const auto tile_hypotenuse = glm::length(tile_edge_point) * 2.0f;
    const auto tile_size= tile_hypotenuse / std::sqrt(2.0);

    gfx::Transform tile;
    tile.Scale(tile_size, tile_size);
    tile.MoveTo(0.0f, 0.0f);
    tile_painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Solid), tile, gfx::CreateMaterialFromImage(res::Checkerboard));

    gfx::Texture* result = nullptr;
    fbo->Resolve(&result);

    gfx::algo::FlipTexture("PolygonWidgetGpuId", result, &device, gfx::algo::FlipDirection::Horizontal);

    result->GenerateMips();
    return result;
}

// Map vertex to widget space.
template<typename VertexType>
QPoint MapVertexToWidget2D(const VertexType& vertex, float width, float height)
{
    return QPoint(vertex.aPosition.x * width, vertex.aPosition.y * height * -1.0f);
}

float PointDist(const QPoint& a, const QPoint& b)
{
    const auto& diff = a - b;
    return std::sqrt(diff.x()*diff.x() + diff.y()*diff.y());
}

} // namespace

namespace gui
{

std::string TranslateEnum(ShapeWidget::MeshType type)
{
    if (type == ShapeWidget::MeshType::Simple2DRenderMesh)
        return "2D Render Mesh";
    else if (type == ShapeWidget::MeshType::Simple2DShardEffectMesh)
        return "2D Shard Effect Mesh";
    else if (type == ShapeWidget::MeshType::Dimetric2DRenderMesh)
        return "2.5D Dimetric Render Mesh";
    else if (type == ShapeWidget::MeshType::Isometric2DRenderMesh)
        return "2.5D Isometric Render Mesh";
    else BUG("Unhandled translation");
    return "???";
}

class ShapeWidget::VertexDataTable : public QAbstractTableModel {
public:
    explicit VertexDataTable(gfx::tool::IPolygonBuilder& builder, gfx::PolygonMeshClass& polygon)
      : mBuilder(builder), mPolygon(polygon)
    {}
    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        const auto row = static_cast<unsigned>(index.row());
        const auto col = static_cast<unsigned>(index.column());

        const auto& field = headerData(col, Qt::Orientation::Horizontal, Qt::DisplayRole);
        if (field.toString() == "aShardIndex")
            return Qt::ItemIsSelectable | Qt::ItemIsEnabled;

        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
    }

    static bool SetVertexData(const gfx::VertexLayout& layout,
                       const gfx::VertexLayout::Attribute& attribute,
                       const QVariant& variant,
                       void* vertex, unsigned vector_index)
    {
        using DataType = gfx::VertexLayout::Attribute::DataType;

        if (attribute.num_vector_components == 1)
        {
            ASSERT(attribute.type == DataType::UnsignedInt);
            // todo:
        }
        else if (attribute.num_vector_components == 2)
        {
            ASSERT(attribute.type == DataType::Float);

            bool success = false;
            const float value = variant.toFloat(&success);
            if (!success)
                return false;

            auto* vec2 = gfx::VertexLayout::GetVertexAttributePtr<gfx::Vec2>(attribute, vertex);
            if (vector_index == 0)
                vec2->x = value;
            else if (vector_index == 1)
                vec2->y = value;
            else BUG("Vector index out of bounds.");
            return true;
        }
        else if (attribute.num_vector_components == 3)
        {
            ASSERT(attribute.type == DataType::Float);

            bool success = false;
            const float value = variant.toFloat(&success);
            if (!success)
                return false;

            auto* vec3 = gfx::VertexLayout::GetVertexAttributePtr<gfx::Vec3>(attribute, vertex);
            if (vector_index == 0)
                vec3->x = value;
            else if (vector_index == 1)
                vec3->y = value;
            else if (vector_index == 2)
            {
                if (attribute.name == "aLocalOffset" || attribute.name == "aWorldNormal")
                    vec3->z = value * -1.0f;
                else vec3->z = value;
            }
            else BUG("Vector index out of bounds.");
            return true;
        }
        else if (attribute.num_vector_components == 4)
        {
            ASSERT(attribute.type == DataType::Float);

            bool success = false;
            const float value = variant.toFloat(&success);
            if (!success)
                return false;

            auto* vec4 = gfx::VertexLayout::GetVertexAttributePtr<gfx::Vec4>(attribute, vertex);
            if (vector_index == 0)
                vec4->x = value;
            else if (vector_index == 1)
                vec4->y = value;
            else if (vector_index == 2)
                vec4->z = value;
            else if (vector_index == 3)
                vec4->w = value;
            else BUG("Vector index out of bounds.");
            return true;
        }
        else BUG("Unhandled number of vector components.");
        return false;
    }

    bool setData(const QModelIndex& index, const QVariant& variant, int role) override
    {
        const auto row = static_cast<unsigned>(index.row());
        const auto col = static_cast<unsigned>(index.column());

        const auto* layout = mPolygon.GetVertexLayout();
        if (!layout)
            return false;

        void* vertex = mBuilder.GetVertexPtr(row);

        unsigned count = 0;
        for (const auto& attr : layout->attributes)
        {
            for (unsigned i=0; i<attr.num_vector_components; ++i)
            {
                if (count == col)
                {
                    if (SetVertexData(*layout, attr, variant, vertex, i))
                    {
                        emit dataChanged(this->index(row, 0), this->index(row, 4));
                        return true;
                    }
                    return false;
                }
                ++count;
            }
        }
        return false;
    }

    static QVariant GetVertexData(const gfx::VertexLayout& layout,
                                  const gfx::VertexLayout::Attribute& attribute, const void* vertex,
                                  unsigned vector_index)
    {
        using DataType = gfx::VertexLayout::Attribute::DataType;

        if (attribute.num_vector_components == 1)
        {
            ASSERT(attribute.type == DataType::UnsignedInt);
            const auto* value = gfx::VertexLayout::GetVertexAttributePtr<uint32_t>(attribute, vertex);
            return QString::number(*value);
        }
        else if (attribute.num_vector_components == 2)
        {
            ASSERT(attribute.type == DataType::Float);
            const auto* vec2 = gfx::VertexLayout::GetVertexAttributePtr<gfx::Vec2>(attribute, vertex);
            if (vector_index == 0)
                return QString::number(vec2->x, 'f', 2);
            else if (vector_index == 1)
                return QString::number(vec2->y, 'f', 2);
            else BUG("Vector index out of bounds.");
        }
        else if (attribute.num_vector_components == 3)
        {
            ASSERT(attribute.type == DataType::Float);
            const auto* vec3 = gfx::VertexLayout::GetVertexAttributePtr<gfx::Vec3>(attribute, vertex);
            if (vector_index == 0)
                return QString::number(vec3->x, 'f', 2);
            else if (vector_index == 1)
                return QString::number(vec3->y, 'f', 2);
            else if (vector_index == 2)
            {
                if (attribute.name == "aLocalOffset" || attribute.name == "aWorldNormal")
                    return QString::number(vec3->z * -1.0f, 'f', 2);
                return QString::number(vec3->z, 'f', 2);
            }
            else BUG("Vector index out of bounds.");
        }
        else if (attribute.num_vector_components == 4)
        {
            ASSERT(attribute.type == DataType::Float);
            const auto* vec4 = gfx::VertexLayout::GetVertexAttributePtr<gfx::Vec4>(attribute, vertex);
            if (vector_index == 0)
                return QString::number(vec4->x, 'f', 2);
            else if (vector_index == 1)
                return QString::number(vec4->y, 'f', 2);
            else if (vector_index == 2)
                return QString::number(vec4->z, 'f', 2);
            else if  (vector_index == 3)
                return QString::number(vec4->w, 'f', 2);
            else BUG("Vector index out of bounds.");
        }
        else BUG("Unhandled number of vector components.");
        return {};
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        const auto row = static_cast<unsigned>(index.row());
        const auto col = static_cast<unsigned>(index.column());
        if (role != Qt::DisplayRole)
            return {};

        const auto* layout = mPolygon.GetVertexLayout();
        if (layout == nullptr)
            return {};

        const void* vertex = mBuilder.GetVertexPtr(row);

        unsigned count = 0;
        for (const auto& attr : layout->attributes)
        {
            for (unsigned i=0; i<attr.num_vector_components; ++i)
            {
                if (count == col)
                    return GetVertexData(*layout, attr, vertex, i);
                ++count;
            }
        }
        return {};
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if ((role != Qt::DisplayRole) || (orientation != Qt::Horizontal))
            return {};

        const auto* layout = mPolygon.GetVertexLayout();
        if (layout == nullptr)
            return {};

        static const char* vector_suffix[] = {
            "X", "Y", "Z", "W"
        };
        unsigned index = 0;

        for (const auto& attr : layout->attributes)
        {
            if (attr.num_vector_components == 1)
            {
                if (index == section)
                    return app::FromLatin(attr.name);
                ++index;
            }
            else
            {
                for (unsigned i=0; i<attr.num_vector_components; ++i)
                {
                    if (index == section)
                        return app::FromLatin(attr.name + " " + vector_suffix[i]);
                    ++index;
                }
            }
        }
        return {};
    }
    int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mBuilder.GetVertexCount());
    }
    int columnCount(const QModelIndex&) const override
    {
        unsigned count = 0;

        const auto* layout = mPolygon.GetVertexLayout();
        if (!layout)
            return 0;

        for (const auto& attr : layout->attributes)
        {
            count += attr.num_vector_components;
        }
        return static_cast<int>(count);
    }

    void RefreshVertex(size_t vertex_index)
    {
        const auto row = static_cast<int>(vertex_index);
        emit dataChanged(this->index(row, 0), this->index(row, 4));
    }

    template<typename VertexType>
    void UpdateVertex(const VertexType& vertex, size_t vertex_index)
    {
        const auto row = static_cast<int>(vertex_index);

        const auto* layout = mPolygon.GetVertexLayout();
        ASSERT(*layout == gfx::GetVertexLayout<VertexType>());

        mBuilder.UpdateVertex(&vertex, vertex_index);

        emit dataChanged(this->index(row, 0), this->index(row, 4));
    }

    template<typename VertexType>
    void InsertVertex(const VertexType& vertex, size_t vertex_index, size_t cmd_index)
    {
        const auto first_index = static_cast<int>(vertex_index);
        const auto last_index = static_cast<int>(vertex_index);

        const auto* layout = mPolygon.GetVertexLayout();
        ASSERT(*layout == gfx::GetVertexLayout<VertexType>());

        beginInsertRows(QModelIndex(), first_index, last_index);
        mBuilder.InsertVertex(&vertex, cmd_index, vertex_index);
        endInsertRows();
    }

    template<typename VertexType>
    void AddVertices(const std::vector<VertexType>& vertices)
    {
        const auto first_index = mBuilder.GetVertexCount();
        const auto last_index = first_index + vertices.size() - 1;

        const auto* layout = mPolygon.GetVertexLayout();
        ASSERT(*layout == gfx::GetVertexLayout<VertexType>());

        beginInsertRows(QModelIndex(),
            static_cast<int>(first_index), static_cast<int>(last_index));

        for (const auto& vertex : vertices)
        {
            mBuilder.AppendVertex(&vertex);
        }

        endInsertRows();
    }
    void EraseVertex(size_t index)
    {
        beginRemoveRows(QModelIndex(), static_cast<int>(index), static_cast<int>(index));
        mBuilder.EraseVertex(index);
        endRemoveRows();
    }

    void Reset()
    {
        beginResetModel();
        endResetModel();
    }

    void ClearAndReset()
    {
        beginResetModel();
        mBuilder.ClearVertices();
        mBuilder.ClearDrawCommands();
        endResetModel();
    }
private:
    gfx::tool::IPolygonBuilder& mBuilder;
    gfx::PolygonMeshClass& mPolygon;
};

class ShapeWidget::MouseTool
{
public:
    struct ViewState {
        GridDensity grid;
        bool snap = false;
        float width  = 0;
        float height = 0;
    };

    virtual ~MouseTool() = default;
    virtual void MousePress(const QMouseEvent* mickey, const QPoint& pos, const ViewState& view)  {}
    virtual void MouseMove(const QMouseEvent* mickey, const QPoint& pos, const ViewState& view) {}
    virtual bool MouseRelease(const QMouseEvent* mickey, const QPoint& pos, const ViewState& view) = 0;
    virtual void CompleteTool(const ViewState& view) {};
    virtual void DrawTool(gfx::Painter& painter, const ViewState& view) const {}
    virtual void DrawHelp(gfx::Painter& painter) const {};
};

template<typename VertexType>
class ShapeWidget::MoveVertex2DTool : public MouseTool {
public:
    using BuilderType = gfx::tool::PolygonBuilder<VertexType>;

    explicit MoveVertex2DTool(State& state, size_t vertex_index)
      : mState(state)
      , mVertexIndex(vertex_index)
    {}
    void MousePress(const QMouseEvent* mickey, const QPoint& pos, const ViewState&) override
    {
        mCurrentPoint = pos;
    }
    void MouseMove(const QMouseEvent* mickey, const QPoint& pos, const ViewState& view) override
    {
        const auto ctrl = mickey->modifiers() & Qt::ControlModifier;

        bool snap = view.snap;
        if (ctrl)
            snap = !snap;

        auto* builder = dynamic_cast<BuilderType*>(mState.builder.get());

        auto vertex = builder->GetVertex(mVertexIndex);
        if (snap)
        {
            const auto num_cells = static_cast<float>(view.grid);
            const auto cell_width = view.width / num_cells;
            const auto cell_height = view.height / num_cells;
            const auto new_x = std::round(pos.x() / cell_width) * cell_width;
            const auto new_y = std::round(pos.y() / cell_height) * cell_height;
            const auto old_x = std::round((vertex.aPosition.x * view.width) / cell_width) * cell_width;
            const auto old_y = std::round((vertex.aPosition.y * -view.height) / cell_height) * cell_height;
            vertex.aPosition.x = new_x / view.width;
            vertex.aPosition.y = new_y / -view.height;
            if (new_x != old_x || new_y != old_y)
            {
                mCurrentPoint = QPoint(static_cast<int>(new_x),
                                       static_cast<int>(new_y));
            }
        }
        else
        {
            const auto dx = static_cast<float>(mCurrentPoint.x() - pos.x());
            const auto dy = static_cast<float>(mCurrentPoint.y() - pos.y());
            vertex.aPosition.x -= (dx / view.width);
            vertex.aPosition.y += (dy / view.height);
            mCurrentPoint = pos;
        }
        vertex.aTexCoord.x =  vertex.aPosition.x;
        vertex.aTexCoord.y = -vertex.aPosition.y;
        mState.table->UpdateVertex(vertex, mVertexIndex);
    }
    bool MouseRelease(const QMouseEvent *mickey, const QPoint &pos, const ViewState &view) override
    {
        return true;
    }
    void DrawHelp(gfx::Painter& painter) const override
    {
        ShowMessage("Move mouse to drag the vertex around.", gfx::FPoint(10.0f, 10.0f), painter);
    }
private:
    ShapeWidget::State& mState;
    QPoint mCurrentPoint;
    const size_t mVertexIndex;
};

template<typename VertexType>
class ShapeWidget::AddVertex2DTriangleFanTool : public MouseTool {
public:
    using BuilderType = gfx::tool::PolygonBuilder<VertexType>;

    explicit AddVertex2DTriangleFanTool(State& state)
        : mState(state)
    {}

    void CompleteTool(const ViewState& view) override
    {
        auto vertices = MakeVerts2D<VertexType>(mPoints, view.width, view.height);

        if constexpr (std::is_same<VertexType, gfx::ShardVertex2D>::value)
        {
            const auto* builder = dynamic_cast<const BuilderType*>(mState.builder.get());
            const auto vertex_count = builder->GetVertexCount();

            if (vertex_count)
            {
                uint32_t shard_index = 0;
                for (size_t i=0; i<builder->GetVertexCount(); ++i)
                {
                    const auto& shard_vertex = builder->GetVertex(i);
                    shard_index = std::max(shard_index, shard_vertex.aShardIndex);
                }
                for (auto& v : vertices)
                {
                    v.aShardIndex = shard_index + 1;
                }
            }
        }

        gfx::Geometry::DrawCommand cmd;
        cmd.type   = gfx::Geometry::DrawType::TriangleFan;
        cmd.count  = mPoints.size();
        cmd.offset = mState.builder->GetVertexCount();

        mState.table->AddVertices(std::move(vertices));
        mState.builder->AddDrawCommand(cmd);
        mPoints.clear();
    }

    void MouseMove(const QMouseEvent* mickey, const QPoint& pos, const ViewState&) override
    {
        mCurrentPoint = pos;
    }
    bool MouseRelease(const QMouseEvent *mickey, const QPoint &pos, const ViewState &view) override
    {
        const auto ctrl = mickey->modifiers() & Qt::ControlModifier;

        bool snap = view.snap;
        if (ctrl)
            snap = !snap;

        if (snap)
        {
            const auto num_cells = static_cast<float>(view.grid);

            const auto cell_width = view.width / num_cells;
            const auto cell_height = view.height / num_cells;
            const auto x = std::round(pos.x() / cell_width) * cell_width;
            const auto y = std::round(pos.y() / cell_height) * cell_height;
            mPoints.emplace_back(QPoint(x, y));
        }
        else
        {
            mPoints.push_back(pos);
        }

        return false;
    }
    void DrawTool(gfx::Painter& painter, const ViewState& view) const override
    {
        if (!mPoints.empty())
        {
            const QPoint& a = mPoints.back();
            const QPoint& b = mCurrentPoint;
            gfx::DebugDrawLine(painter, ToGfx(a), ToGfx(b), gfx::Color::HotPink, 2.0f);
        }

        std::vector<QPoint> points = mPoints;
        points.push_back(mCurrentPoint);

        gfx::Transform  transform;
        transform.Resize(view.width, view.height);

        gfx::Geometry::DrawCommand cmd;
        cmd.type   = gfx::Geometry::DrawType::TriangleFan;
        cmd.offset = 0;
        cmd.count  = points.size();

        BuilderType builder;
        builder.AddVertices(MakeVerts2D<VertexType>(points, view.width, view.height));
        builder.AddDrawCommand(cmd);

        gfx::PolygonMeshClass current(mState.polygon->GetId() + "_2");
        builder.BuildPoly(current);
        current.SetStatic(false);

        const auto alpha = 0.87f;
        static gfx::ColorClass color(gfx::MaterialClass::Type::Color);
        color.SetBaseColor(gfx::Color4f(gfx::Color::LightGray, alpha));
        color.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);

        painter.Draw(gfx::PolygonMeshInstance(current), transform,
            gfx::MaterialInstance(color));
    }
    void DrawHelp(gfx::Painter& painter) const  override
    {
        if (mPoints.empty())
            ShowMessage("Click to place first vertex", gfx::FPoint(10.0f, 10.0f), painter);
        else ShowMessage("Move mouse and click to place a vertex", gfx::FPoint(10.0f, 10.0f), painter);
    }
private:
    ShapeWidget::State& mState;
    std::vector<QPoint> mPoints;
    QPoint mCurrentPoint;
};

ShapeWidget::ShapeWidget(app::Workspace* workspace)
{
    DEBUG("Create PolygonWidget");

    mState.workspace = workspace;
    mState.polygon = std::make_shared<gfx::PolygonMeshClass>();

    mUI.setupUi(this);
    mUI.widget->onPaintScene   = std::bind(&ShapeWidget::PaintScene, this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onKeyPress     = std::bind(&ShapeWidget::OnKeyPressEvent, this, std::placeholders::_1);
    mUI.widget->onKeyRelease   = std::bind(&ShapeWidget::OnKeyReleaseEvent, this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&ShapeWidget::OnMousePress,this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&ShapeWidget::OnMouseRelease, this, std::placeholders::_1);
    mUI.widget->onMouseMove    = std::bind(&ShapeWidget::OnMouseMove, this, std::placeholders::_1);
    mUI.widget->onMouseWheel   = std::bind(&ShapeWidget::OnMouseWheel, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&ShapeWidget::OnMouseDoubleClick,this, std::placeholders::_1);

    mUI.widget->SetCursorColor(gfx::Color::HotPink, GfxWidget::CursorShape::CrossHair);
    mUI.widget->SetCursorColor(gfx::Color::HotPink, GfxWidget::CursorShape::ArrowCursor);

    auto* menu = new QMenu(this);
    menu->addAction(mUI.actionCustomizeShader);
    menu->addAction(mUI.actionShowShader);
    mUI.btnAddShader->setMenu(menu);

    mHamburger = new QMenu(this);
    mHamburger->setIcon(QIcon("icons:hamburger.png"));
    mHamburger->addAction(mUI.chkSnap);
    mHamburger->addAction(mUI.chkShowGrid);
    mHamburger->addAction(mUI.chkShowNormals);
    mHamburger->addAction(mUI.chkShowVertices);
    mHamburger->addAction(mUI.chkShowSurfaces);
    mHamburger->addAction(mUI.chkShowBlueprint);

    connect(mUI.btnHamburger, &QPushButton::clicked, this, [this]() {
        QPoint point;
        point.setX(0);
        point.setY(mUI.btnHamburger->width());
        mHamburger->popup(mUI.btnHamburger->mapToGlobal(point));
    });

    SetList(mUI.blueprints, workspace->ListUserDefinedMaterials());
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionStop, false);
    SetValue(mUI.chkShowNormals,   true);
    SetValue(mUI.chkShowBlueprint, true);
    SetValue(mUI.chkShowVertices,  true);
    SetValue(mUI.chkShowSurfaces,  true);
    SetValue(mUI.name, QString("My Shape"));
    SetValue(mUI.ID, mState.polygon->GetId());
    SetValue(mUI.chkStaticInstance, mState.polygon->IsStatic());
    SetValue(mUI.chkDoubleSided, mState.polygon->IsDoubleSided());
    SetValue(mUI.shaderFile, "Built-in Shader");
    setWindowTitle(GetValue(mUI.name));
    setFocusPolicy(Qt::StrongFocus);

    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    PopulateFromEnum<MeshType>(mUI.cmbMeshType);
    SetValue(mUI.cmbGrid, GridDensity::Grid20x20);
    SetValue(mUI.cmbMeshType, MeshType::Simple2DRenderMesh);

    mState.polygon->SetName(GetValue(mUI.name));
    mState.polygon->SetStatic(GetValue(mUI.chkStaticInstance));
    mState.polygon->SetDoubleSided(GetValue(mUI.chkDoubleSided));
    SetMeshType(MeshType::Simple2DRenderMesh);
    CreateMeshBuilder();
    mOriginalHash = mState.polygon->GetHash();

    SetEnabled(mUI.btnResetShader, mState.polygon->HasShaderSrc());
    SetEnabled(mUI.actionClear, mState.polygon->HasInlineData());
}

ShapeWidget::ShapeWidget(app::Workspace* workspace, const app::Resource& resource) : ShapeWidget(workspace)
{
    DEBUG("Editing shape '%1'", resource.GetName());

    *mState.polygon = *resource.GetContent<gfx::PolygonMeshClass>();
    mOriginalHash = mState.polygon->GetHash();

    CreateMeshBuilder();
    mState.builder->InitFrom(*mState.polygon);

    QString material;
    GetProperty(resource, "material", &material);
    GetUserProperty(resource, "grid",           mUI.cmbGrid);
    GetUserProperty(resource, "snap_to_grid",   mUI.chkSnap);
    GetUserProperty(resource, "show_grid",      mUI.chkShowGrid);
    GetUserProperty(resource, "show_normals",   mUI.chkShowNormals);
    GetUserProperty(resource, "show_vertices",  mUI.chkShowVertices);
    GetUserProperty(resource, "show_surfaces",  mUI.chkShowSurfaces);
    GetUserProperty(resource, "show_blueprint", mUI.chkShowBlueprint);
    GetUserProperty(resource, "widget",         mUI.widget);
    GetUserProperty(resource, "splitter",       mUI.splitter);
    GetUserProperty(resource, "kRandom",        mUI.kRandom);
    unsigned main_view_value = 0;
    GetUserProperty(resource, "main_view", &main_view_value);
    if (main_view_value == static_cast<unsigned>(ViewType::EditView))
        mMainView = ViewType::EditView;
    else if (main_view_value == static_cast<unsigned>(ViewType::LitView))
        mMainView = ViewType::LitView;

    SetValue(mUI.name, resource.GetName());
    SetValue(mUI.ID, mState.polygon->GetId());
    SetValue(mUI.chkStaticInstance, mState.polygon->IsStatic());
    SetValue(mUI.chkDoubleSided, mState.polygon->IsDoubleSided());
    SetValue(mUI.shaderFile, mState.polygon->HasShaderSrc() ? "Customized Shader" : "Built-in Shader");
    SetValue(mUI.blueprints, ListItemId(material));
    SetValue(mUI.cmbMeshType, GetMeshType());
    SetEnabled(mUI.btnResetShader, mState.polygon->HasShaderSrc());
    SetEnabled(mUI.actionClear, mState.polygon->HasInlineData());

    if (!material.isEmpty())
    {
        if (mState.workspace->IsValidMaterial(material))
        {
            auto klass = mState.workspace->FindMaterialClassById(app::ToUtf8(material));
            mBlueprint = gfx::CreateMaterialInstance(klass);
        }
        else
        {
            WARN("Material '%1' is no longer available.", material);
            SetValue(mUI.blueprints, -1);
        }
    }
}

ShapeWidget::~ShapeWidget()
{
    DEBUG("Destroy PolygonWidget");
}

QString ShapeWidget::GetId() const
{
    return GetValue(mUI.ID);
}

void ShapeWidget::InitializeSettings(const UISettings& settings)
{
    SetValue(mUI.cmbGrid,     settings.grid);
    SetValue(mUI.chkSnap,     settings.snap_to_grid);
    SetValue(mUI.chkShowGrid, settings.show_grid);
    SetValue(mUI.chkShowNormals,   true);
    SetValue(mUI.chkShowBlueprint, true);
    SetValue(mUI.chkShowVertices,  true);
    SetValue(mUI.chkShowSurfaces,  true);
}

void ShapeWidget::InitializeContent()
{
    QTimer::singleShot(10, this, [this]() {
        QList<int> sizes {98, 100};
        //mUI.splitter->setSizes(sizes);
    });
}

void ShapeWidget::SetViewerMode()
{
    SetVisible(mUI.baseProperties, false);
    SetVisible(mUI.lblHelp,        false);
    SetVisible(mUI.blueprints,     false);
    SetVisible(mUI.btnHamburger,   false);
    SetVisible(mUI.btnResetBlueprint, false);
    SetVisible(mUI.lblRandom, false);
    SetVisible(mUI.kRandom, false);
}

void ShapeWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionNewTriangleFan);
    bar.addSeparator();
    bar.addAction(mUI.actionClear);
}
void ShapeWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionNewTriangleFan);
    menu.addSeparator();
    menu.addAction(mUI.actionClear);
}

bool ShapeWidget::SaveState(Settings& settings) const
{
    data::JsonObject json;

    mState.builder->BuildPoly(*const_cast<gfx::PolygonMeshClass*>(mState.polygon.get()));
    mState.polygon->IntoJson(json);

    settings.SetValue("Polygon", "content", json);
    settings.SetValue("Polygon", "material", (QString)GetItemId(mUI.blueprints));
    settings.SetValue("Polygon", "hash", mOriginalHash);
    settings.SetValue("Polygon", "main_view", mMainView);
    settings.SaveWidget("Polygon", mUI.name);
    settings.SaveWidget("Polygon", mUI.chkShowGrid);
    settings.SaveWidget("Polygon", mUI.chkSnap);
    settings.SaveWidget("Polygon", mUI.chkShowNormals);
    settings.SaveWidget("Polygon", mUI.chkShowVertices);
    settings.SaveWidget("Polygon", mUI.chkShowSurfaces);
    settings.SaveWidget("Polygon", mUI.chkShowBlueprint);
    settings.SaveWidget("Polygon", mUI.cmbGrid);
    settings.SaveWidget("Polygon", mUI.widget);
    settings.SaveWidget("Polygon", mUI.splitter);
    settings.SaveWidget("Polygon", mUI.kRandom);
    return true;
}
bool ShapeWidget::LoadState(const Settings& settings)
{
    QString material;
    data::JsonObject json;
    settings.GetValue("Polygon", "content", &json);
    settings.GetValue("Polygon", "material", &material);
    settings.GetValue("Polygon", "hash", &mOriginalHash);
    settings.GetValue("Polygon", "main_view", &mMainView);
    settings.LoadWidget("Polygon", mUI.name);
    settings.LoadWidget("Polygon", mUI.chkShowGrid);
    settings.LoadWidget("Polygon", mUI.chkSnap);
    settings.LoadWidget("Polygon", mUI.chkShowNormals);
    settings.LoadWidget("Polygon", mUI.chkShowVertices);
    settings.LoadWidget("Polygon", mUI.chkShowSurfaces);
    settings.LoadWidget("Polygon", mUI.chkShowBlueprint);
    settings.LoadWidget("Polygon", mUI.cmbGrid);
    settings.LoadWidget("Polygon", mUI.widget);
    settings.LoadWidget("Polygon", mUI.splitter);
    settings.LoadWidget("Polygon", mUI.kRandom);

    if (!mState.polygon->FromJson(json))
        WARN("Failed to restore polygon shape state.");

    SetValue(mUI.ID, mState.polygon->GetId());
    SetValue(mUI.chkStaticInstance, mState.polygon->IsStatic());
    SetValue(mUI.chkDoubleSided, mState.polygon->IsDoubleSided());
    SetValue(mUI.blueprints, ListItemId(material));
    SetValue(mUI.shaderFile, mState.polygon->HasShaderSrc() ? "Customized Shader" : "Built-in Shader");
    SetValue(mUI.cmbMeshType, GetMeshType());
    SetEnabled(mUI.btnResetShader, mState.polygon->HasShaderSrc());
    SetEnabled(mUI.actionClear, mState.polygon->HasInlineData());

    CreateMeshBuilder();
    mState.builder->InitFrom(*mState.polygon);

    on_blueprints_currentIndexChanged(0);
    return true;
}

bool ShapeWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    switch (action)
    {
        case Actions::CanCut:
        case Actions::CanCopy:
        case Actions::CanPaste:
            return false;
        case Actions::CanReloadTextures:
        case Actions::CanReloadShaders:
            return true;
        case Actions::CanZoomIn:
        case Actions::CanZoomOut:
            return false;
        case Actions::CanUndo:
            return false;
    }
    return false;
}

void ShapeWidget::ReloadShaders()
{
    mUI.widget->ReloadShaders();
}
void ShapeWidget::ReloadTextures()
{
    mUI.widget->ReloadTextures();
}
void ShapeWidget::Shutdown()
{
    mUI.widget->Dispose();
}
void ShapeWidget::Render()
{
    mUI.widget->TriggerPaint();
}

void ShapeWidget::Update(double secs)
{
    if (mPaused || !mPlaying)
        return;

    mTime += secs;

    if (mBlueprint)
    {
        mBlueprint->SetRuntime(mTime);
    }
}

void ShapeWidget::Save()
{
    on_actionSave_triggered();
}

bool ShapeWidget::HasUnsavedChanges() const
{
    auto clone = mState.polygon->Copy();
    auto* poly = dynamic_cast<gfx::PolygonMeshClass*>(clone.get());

    mState.builder->BuildPoly(*poly);
    poly->SetStatic(GetValue(mUI.chkStaticInstance));
    poly->SetDoubleSided(GetValue(mUI.chkDoubleSided));
    poly->SetName(GetValue(mUI.name));

    if (mOriginalHash == poly->GetHash())
        return false;
    return true;
}

bool ShapeWidget::GetStats(Stats* stats) const
{
    stats->time  = mTime;
    stats->graphics.valid = true;
    stats->graphics.fps   = mUI.widget->GetCurrentFPS();
    const auto& dev_stats = mUI.widget->GetDeviceResourceStats();
    stats->device.static_vbo_mem_alloc    = dev_stats.static_vbo_mem_alloc;
    stats->device.static_vbo_mem_use      = dev_stats.static_vbo_mem_use;
    stats->device.dynamic_vbo_mem_alloc   = dev_stats.dynamic_vbo_mem_alloc;
    stats->device.dynamic_vbo_mem_use     = dev_stats.dynamic_vbo_mem_use;
    stats->device.streaming_vbo_mem_use   = dev_stats.streaming_vbo_mem_use;
    stats->device.streaming_vbo_mem_alloc = dev_stats.streaming_vbo_mem_alloc;
    return true;
}

void ShapeWidget::on_widgetColor_colorChanged(const QColor& color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void ShapeWidget::on_actionPlay_triggered()
{
    if (mPaused)
    {
        mPaused = false;
        mUI.actionPause->setEnabled(true);
    }
    else
    {
        mUI.actionPlay->setEnabled(false);
        mUI.actionPause->setEnabled(true);
        mUI.actionStop->setEnabled(true);
        mTime   = 0.0f;
        mPaused = false;
        mPlaying = true;
    }
}
void ShapeWidget::on_actionPause_triggered()
{
    mPaused = true;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
}
void ShapeWidget::on_actionStop_triggered()
{
    mUI.actionStop->setEnabled(false);
    mUI.actionPause->setEnabled(false);
    mUI.actionPlay->setEnabled(true);
    mPaused = false;
    mPlaying = false;
    mTime = 0.0;
    if (mBlueprint)
    {
        mBlueprint->SetRuntime(0.0);
    }
}

void ShapeWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.name))
        return;

    mState.builder->BuildPoly(*mState.polygon);
    mState.polygon->SetName(GetValue(mUI.name));
    mState.polygon->SetStatic(GetValue(mUI.chkStaticInstance));
    mState.polygon->SetDoubleSided(GetValue(mUI.chkDoubleSided));

    app::CustomShapeResource resource(mState.polygon, GetValue(mUI.name));
    SetProperty(resource, "material", (QString)GetItemId(mUI.blueprints));
    SetUserProperty(resource, "grid",           mUI.cmbGrid);
    SetUserProperty(resource, "snap_to_grid",   mUI.chkSnap);
    SetUserProperty(resource, "show_grid",      mUI.chkShowGrid);
    SetUserProperty(resource, "show_normals",   mUI.chkShowNormals);
    SetUserProperty(resource, "show_vertices",  mUI.chkShowVertices);
    SetUserProperty(resource, "show_surfaces",  mUI.chkShowSurfaces);
    SetUserProperty(resource, "show_blueprint", mUI.chkShowBlueprint);
    SetUserProperty(resource, "widget",         mUI.widget);
    SetUserProperty(resource, "splitter",       mUI.splitter);
    SetUserProperty(resource, "kRandom",        mUI.kRandom);
    SetUserProperty(resource, "main_view", static_cast<unsigned>(mMainView));

    mState.workspace->SaveResource(resource);
    mOriginalHash = mState.polygon->GetHash();
}

void ShapeWidget::on_actionNewTriangleFan_triggered()
{
    const auto mesh_type = GetMeshType();
    if (mesh_type == MeshType::Simple2DRenderMesh)
    {
        using ToolType = AddVertex2DTriangleFanTool<gfx::Vertex2D>;
        mMouseTool = std::make_unique<ToolType>(mState);
    }
    else if (mesh_type == MeshType::Simple2DShardEffectMesh)
    {
        using ToolType = AddVertex2DTriangleFanTool<gfx::ShardVertex2D>;
        mMouseTool = std::make_unique<ToolType>(mState);
    }
    else if (mesh_type == MeshType::Dimetric2DRenderMesh || mesh_type == MeshType::Isometric2DRenderMesh)
    {
        using ToolType = AddVertex2DTriangleFanTool<gfx::Perceptual3DVertex>;
        mMouseTool = std::make_unique<ToolType>(mState);
    } else BUG("Missing mesh type handling.");

    SetValue(mUI.actionNewTriangleFan, true);
}

void ShapeWidget::on_actionShowShader_triggered()
{
    auto* device = mUI.widget->GetDevice();

    gfx::Drawable::Environment environment;
    environment.editing_mode  = false; // we want to see the shader as it will be, so using false here
    environment.use_instancing = false;
    const auto& source = mState.polygon->GetShader(environment, *device);

    DlgTextEdit dlg(this);
    dlg.SetText(source.GetSource(), "GLSL");
    dlg.SetReadOnly(true);
    dlg.SetTitle("Shader Source");
    dlg.LoadGeometry(mState.workspace, "polygon-shader-source-dialog-geometry");
    dlg.execFU();
    dlg.SaveGeometry(mState.workspace, "polygon-shader-source-dialog-geometry");
}

void ShapeWidget::on_actionCustomizeShader_triggered()
{
    if (mShaderEditor)
    {
        mShaderEditor->activateWindow();
        return;
    }

    mCustomizedSource = mState.polygon->GetShaderSrc();
    if (!mState.polygon->HasShaderSrc())
    {
        mState.polygon->SetShaderSrc(R"(
// this is your custom vertex transform function.
// you can modify the incoming vertex data here as want.
void CustomVertexTransform(inout VertexData vs) {

   // your code here

})");
    }

    mShaderEditor = new DlgTextEdit(this);
    mShaderEditor->LoadGeometry(mState.workspace, "polygon-shader-editor-geometry");
    mShaderEditor->SetText(mState.polygon->GetShaderSrc(), "GLSL");
    mShaderEditor->SetTitle("Shader Source");
    mShaderEditor->EnableSaveApply();
    mShaderEditor->showFU();
    mShaderEditor->finished = [this](int ret) {
        if (ret == QDialog::Rejected)
            mState.polygon->SetShaderSrc(mCustomizedSource);
        else if (ret == QDialog::Accepted)
            mState.polygon->SetShaderSrc(mShaderEditor->GetText());
        mShaderEditor->SaveGeometry(mState.workspace, "polygon-shader-editor-geometry");
        mShaderEditor->deleteLater();
        mShaderEditor = nullptr;

        SetValue(mUI.shaderFile, mState.polygon->HasShaderSrc() ? "Customized Shader" : "Built-in Shader");
        SetEnabled(mUI.btnResetShader, mState.polygon->HasShaderSrc());
    };
    mShaderEditor->applyFunction = [this]() {
        mState.polygon->SetShaderSrc(mShaderEditor->GetText());
    };
}

void ShapeWidget::on_actionClear_triggered()
{
    if (mState.builder->GetVertexCount() == 0 &&
        mState.builder->GetCommandCount() == 0)
        return;

    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Icon::Question);
    msg.setText("Are you sure you want to clear all vertices?");
    msg.setStandardButtons(QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No);
    if (msg.exec() == QMessageBox::StandardButton::No)
        return;

    mState.table->ClearAndReset();
    SetEnabled(mUI.actionClear, false);
}

void ShapeWidget::on_blueprints_currentIndexChanged(int)
{
    mBlueprint.reset();
    if (mUI.blueprints->currentIndex() == -1)
        return;

    auto klass = mState.workspace->GetMaterialClassById(GetItemId(mUI.blueprints));
    mBlueprint = gfx::CreateMaterialInstance(klass);
}

void ShapeWidget::on_btnResetShader_clicked()
{
    mState.polygon->SetShaderSrc(std::string{});
    SetValue(mUI.shaderFile, "Built-In Shader");
    SetEnabled(mUI.btnResetShader, false);
}

void ShapeWidget::on_btnResetBlueprint_clicked()
{
    SetValue(mUI.blueprints, -1);
    mBlueprint.reset();
}

void ShapeWidget::on_chkStaticInstance_stateChanged(int)
{
    mState.builder->SetStatic(GetValue(mUI.chkStaticInstance));
}
void ShapeWidget::on_chkDoubleSided_stateChanged(int)
{
    mState.builder->SetDoubleSided(GetValue(mUI.chkDoubleSided));
}

void ShapeWidget::on_cmbMeshType_currentIndexChanged(int)
{
    const MeshType mesh_type = GetValue(mUI.cmbMeshType);
    if (mesh_type == GetMeshType())
        return;

    if (mState.polygon->GetVertexCount())
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Icon::Question);
        msg.setText("Changing the mesh type will reset all vertex data.\n"
            "Are you sure you want to continue?");
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        if (msg.exec() == QMessageBox::No)
        {
            SetValue(mUI.cmbMeshType, GetMeshType());
            return;
        }
    }
    SetMeshType(mesh_type);
    CreateMeshBuilder();
    mMainView = ViewType::EditView;
    mBlueprint.reset();
}

void ShapeWidget::on_tableView_customContextMenuRequested(const QPoint& point)
{
    QMenu menu(this);

    const auto mesh_type = GetMeshType();
    if (mesh_type == MeshType::Dimetric2DRenderMesh || mesh_type == MeshType::Isometric2DRenderMesh)
    {
        const bool have_selection = mSelectedVertex < mState.builder->GetVertexCount();
        auto* set_normal_positive_x = menu.addAction("Orient normal to +X");
        auto* set_normal_negative_x = menu.addAction("Orient normal to -X");
        auto* set_normal_positive_y = menu.addAction("Orient normal to +Y");
        auto* set_normal_negative_y = menu.addAction("Orient normal to -Y");
        auto* set_normal_positive_z = menu.addAction("Orient normal to +Z");
        auto* set_normal_negative_z = menu.addAction("Orient normal to -Z");
        SetEnabled(set_normal_positive_x, have_selection);
        SetEnabled(set_normal_negative_x, have_selection);
        SetEnabled(set_normal_positive_y, have_selection);
        SetEnabled(set_normal_negative_y, have_selection);
        SetEnabled(set_normal_positive_z, have_selection);
        SetEnabled(set_normal_negative_z, have_selection);

        connect(set_normal_positive_x, &QAction::triggered, this, [this]() {
            SetSelectedVertexNormal({1.0f, 0.0f, 0.0f});
        });
        connect(set_normal_negative_x, &QAction::triggered, this, [this]() {
            SetSelectedVertexNormal({-1.0f, 0.0f, 0.0f});
        });
        connect(set_normal_positive_y, &QAction::triggered, this, [this]() {
            SetSelectedVertexNormal({0.0f, 1.0f, 0.0f});
        });
        connect(set_normal_negative_y, &QAction::triggered, this, [this]() {
            SetSelectedVertexNormal({0.0f, -1.0f, 0.0f});
        });
        // z axel handedness is swapped between reality and what user sees.
        connect(set_normal_positive_z, &QAction::triggered, this, [this]() {
            SetSelectedVertexNormal({0.0f, 0.0f, -1.0f});
        });
        connect(set_normal_negative_z, &QAction::triggered, this, [this]() {
            SetSelectedVertexNormal({0.0f, 0.0f, 1.0f});
        });
    }

    menu.exec(QCursor::pos());
}

void ShapeWidget::OnAddResource(const app::Resource* resource)
{
    if (resource->GetType() != app::Resource::Type::Material)
        return;

    SetList(mUI.blueprints, mState.workspace->ListUserDefinedMaterials());
}

void ShapeWidget::OnRemoveResource(const app::Resource* resource)
{
    if (resource->GetType() != app::Resource::Type::Material)
        return;

    SetList(mUI.blueprints, mState.workspace->ListUserDefinedMaterials());
    if (mBlueprint && mBlueprint->GetClassId() == resource->GetIdUtf8())
        mBlueprint.reset();
}

bool ShapeWidget::OnEscape()
{
    if (mMouseTool)
    {
        const auto& rect = GetMainRenderRect();
        MouseTool::ViewState view;
        view.width = rect.width();
        view.height = rect.height();
        view.snap = GetValue(mUI.chkSnap);
        view.grid = GetValue(mUI.cmbGrid);
        mMouseTool->CompleteTool(view);
        mMouseTool.reset();

        mUI.actionNewTriangleFan->setChecked(false);
        mUI.actionClear->setEnabled(true);
    }
    else if (mSelectedVertex < mState.builder->GetVertexCount())
    {
        mSelectedVertex = 0xfffffff;
        ClearSelection(mUI.tableView);
    }
    else return false;

    return true;
}

void ShapeWidget::PaintScene(gfx::Painter& painter, double secs)
{
    mMessages.clear();

    auto* device = painter.GetDevice();

    // hack hack if we have the main window preview window displaying this
    // same custom shape then we have a competition of hash values used
    // to compare the polygon data content against the content in the GPU
    // buffer. And the competition is between the class object stored in
    // the workspace that has the same Class ID but different content hash
    // and *this* polygon class instance here that is a copy but maps to
    // the same class ID but with different hash (because it has different
    // content when it's being edited).
    // so hack around this problem by adding a suffix here.
    auto poly = std::make_shared<gfx::PolygonMeshClass>(mState.polygon->GetId() + "_1");
    poly->SetMeshType(mState.polygon->GetMeshType());
    poly->SetShaderSrc(mState.polygon->GetShaderSrc());
    poly->SetName(mState.polygon->GetName());
    mState.builder->BuildPoly(*poly);

    // set to true since we're constructing this polygon on every frame
    // without this we'll eat all the static vertex/index buffers. argh!
    poly->SetDynamic(true);

    poly->SetDoubleSided(GetValue(mUI.chkDoubleSided));

    const auto mesh_type = GetMeshType();
    if (mesh_type == MeshType::Dimetric2DRenderMesh || mesh_type == MeshType::Isometric2DRenderMesh)
    {
        mAxonometricTextureWidth  = 1024;
        mAxonometricTextureHeight = 1024;
        if (mBlueprint)
        {
            unsigned texture_width  = 0;
            unsigned texture_height = 0;
            if (FindAxonometricTextureSize(*mBlueprint, *device, &texture_width, &texture_height))
            {
                const auto aspect = static_cast<float>(texture_width)/static_cast<float>(texture_height);
                mAxonometricTextureWidth  = mAxonometricTextureWidth * aspect;
            }
        }

        // solve the size of the tile based on the Pythagoras theorem
        // a² + b² = c²
        // we know that the tile is a square so each side is equal length
        // thus we have
        //      2a² = c²
        //       a² = c² / 2
        // sqrt(a²) = sqrt(c²)/sqrt(2)
        //        a = c/sqrt(2.0)
        //
        // we can figure out the hypotenuse for this equation by projecting
        // a point from our scene space visible edge to the tile plane and
        // doubling that distance.

        const auto game_view = mesh_type == MeshType::Dimetric2DRenderMesh
                                   ? engine::GameView::EnumValue::Dimetric
                                   : engine::GameView::EnumValue::Isometric;
        const auto scene_edge_point = static_cast<float>(mAxonometricTextureWidth) / 2.0f;
        const auto tile_edge_point = engine::MapFromViewPlaneToGamePlane(glm::vec4 { scene_edge_point, 0.0f, 0.0f, 0.0f}, game_view);
        const auto tile_hypotenuse = glm::length(tile_edge_point) * 2.0f;
        const auto tile_size = static_cast<float>(tile_hypotenuse / std::sqrt(2.0));
        mAxonometricTileBaseSize = glm::vec3 { tile_size, tile_size, tile_size };

        PaintLitAxonometricScene(GetViewRect(ViewType::LitView), poly, device);
        Paint3DAxonometricScene(GetViewRect(ViewType::Axo3DView), poly, device);
    }

    poly->SetMeshType(gfx::PolygonMeshClass::MeshType::Simple2DRenderMesh);

    PaintEditScene(GetViewRect(ViewType::EditView), poly, device);

    if (mMessages.empty() && mMouseTool)
    {
        mMouseTool->DrawHelp(painter);
    }
    else
    {
        gfx::FPoint point;
        point.Translate(Margin, Margin);
        for (const auto& msg : mMessages)
        {
            ShowMessage(msg, point, painter);
            point.Translate(0.0f, 25.0f);
        }
    }
}

void ShapeWidget::PaintEditScene(const QRect& rect, const PolygonClassHandle& polygon, gfx::Device* device)
{
    const float width   = rect.width();
    const float height  = rect.height();
    const float xoffset = rect.x();
    const float yoffset = rect.y();
    const auto mesh_type = GetMeshType();
    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    gfx::Painter painter;
    painter.SetDevice(device);
    painter.SetSurfaceSize(mUI.widget->width(), mUI.widget->height());
    painter.SetViewport(xoffset, yoffset, width, height);
    painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(width, height));

    mPixelDistance2Dand3D.reset();

    if (mesh_type == MeshType::Dimetric2DRenderMesh || mesh_type == MeshType::Isometric2DRenderMesh)
    {
        gfx::Painter tile_painter;
        tile_painter.SetDevice(device);
        tile_painter.SetSurfaceSize(mUI.widget->width(), mUI.widget->height());
        tile_painter.SetViewport(xoffset, yoffset, width, height);
        ConfigureTilePainter(tile_painter);

        auto checkerboard = gfx::CreateMaterialFromImage(res::Checkerboard);

        const auto tile_base_size = mAxonometricTileBaseSize;
        const auto tile_size = tile_base_size.x;

        // ground reference
        {
            gfx::Transform tile;
            tile.Resize(tile_base_size);
            tile.MoveTo(-tile_size, -tile_size);
            tile_painter.Draw(gfx::Rectangle(), tile, checkerboard);
        }

        // right back reference
        {
            gfx::Transform tile;
            tile.Resize(tile_base_size);
            tile.MoveTo(-tile_size, -tile_size);
            tile.Push();
                tile.RotateAroundX(gfx::FDegrees(-90.0f));

            checkerboard.SetUniform("kBaseColor", gfx::Color::Green);
            tile_painter.Draw(gfx::Rectangle(), tile, checkerboard, gfx::Painter::Culling::None);
        }

        // left back reference
        {
            gfx::Transform tile;
            tile.Resize(tile_base_size);
            tile.MoveTo(-tile_size, -tile_size);
            tile.Push();
                tile.RotateAroundY(gfx::FDegrees(90.0f));

            checkerboard.SetUniform("kBaseColor", gfx::Color::Red);
            tile_painter.Draw(gfx::Rectangle(), tile, checkerboard, gfx::Painter::Culling::None);
        }

        // unit cube that should completely obscure the tile floor and the back walls.
        if (false)
        {
            gfx::Transform tile;
            tile.Resize(tile_base_size);
            tile.MoveTo(-tile_size, -tile_size);

            checkerboard.SetUniform("kBaseColor", gfx::Color::White);
            tile_painter.Draw(gfx::Cube(), tile, checkerboard);
        }

        // putting this computation here since conveniently we have the matrices
        // available here for doing the mathy stuff.
        if (mSelectedVertex < mState.builder->GetVertexCount())
        {
            using BuilderType = gfx::tool::PolygonBuilder<gfx::Perceptual3DVertex>;
            const auto* builder = dynamic_cast<const BuilderType*>(mState.builder.get());
            const auto& vertex = builder->GetVertex(mSelectedVertex);

            const auto& projection_matrix_2d = painter.GetProjMatrix();
            const auto& projection_matrix_3d = tile_painter.GetProjMatrix();
            const auto& view_matrix_3d = tile_painter.GetViewMatrix();

            const auto vertex_2d_clip_space = projection_matrix_2d * glm::vec4 {
                width * vertex.aPosition.x, height * -vertex.aPosition.y, 0.0f, 1.0f };

            const auto vertex_3d_clip_space = projection_matrix_3d * view_matrix_3d * glm::vec4 {
                -tile_base_size.x + vertex.aLocalOffset.x * tile_base_size.x,
                -tile_base_size.y + vertex.aLocalOffset.y * tile_base_size.y,
                                    vertex.aLocalOffset.z * tile_base_size.z, 1.0f };
            //DEBUG("%1 vs %2", vertex_2d_clip_space, vertex_3d_clip_space);
            // now both vertices are in clip space, map to pixel distances.
            const auto horizontal_distance_pixels = std::abs(vertex_2d_clip_space.x - vertex_3d_clip_space.x) * width * 0.5;
            const auto vertical_distance_pixels = std::abs(vertex_2d_clip_space.y - vertex_3d_clip_space.y) * width * 0.5;
            const auto pixel_distance = std::sqrt(horizontal_distance_pixels*horizontal_distance_pixels +
                                                  vertical_distance_pixels*vertical_distance_pixels);
            mPixelDistance2Dand3D = pixel_distance;
            //DEBUG("Pixel distance between points = %1", static_cast<int>(pixel_distance));
        }
    }

    // if we have some blueprint then use it on the background.
    if (GetValue(mUI.chkShowBlueprint) && mBlueprint)
    {
        gfx::Transform model;
        model.Resize(width, height);
        painter.Draw(gfx::Rectangle(), model, *mBlueprint);
    }

    if (GetValue(mUI.chkShowGrid))
    {
        const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
        const auto num_cell_lines = static_cast<unsigned>(grid) - 1;

        gfx::Transform view;
        view.Resize(width-2.0f, height-2.0f);
        view.Translate(1.0f, 1.0f);
        painter.Draw(gfx::Grid(num_cell_lines, num_cell_lines), view,
                     gfx::CreateMaterialFromColor(gfx::Color::LightGray));
    }
    else
    {
        gfx::Transform view;
        view.Resize(width, height);
        painter.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Outline), view,
                     gfx::CreateMaterialFromColor(gfx::Color::LightGray));
    }
    if (mMainView != ViewType::EditView)
    {
        const auto mouse_pos_widget = mUI.widget->mapFromGlobal(QCursor::pos());
        if (rect.contains(mouse_pos_widget))
        {
            gfx::Transform view;
            view.Resize(width, height);
            painter.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Outline), view,
                         gfx::CreateMaterialFromColor(gfx::Color::Green));
        }
    }

    // draw the polygon we're working on
    if (GetValue(mUI.chkShowSurfaces))
    {
        const auto alpha = 0.87f;
        static gfx::ColorClass color(gfx::MaterialClass::Type::Color);
        color.SetBaseColor(gfx::Color4f(gfx::Color::LightGray, alpha));
        color.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);

        gfx::PolygonMeshInstance mesh(polygon);
        mesh.SetTime(mTime);
        mesh.SetRandomValue(GetValue(mUI.kRandom));

        gfx::Transform view;
        view.Resize(width, height);

        gfx::Painter::LegacyDrawState state;
        state.line_width = 1.0f;
        state.culling = gfx::Painter::Culling::Back;
        if (mesh.IsDoubleSided())
            state.culling = gfx::Painter::Culling::None;
        painter.Draw(mesh, view, gfx::MaterialInstance(color), state);
    }

    // visualize the vertices.
    if (GetValue(mUI.chkShowVertices))
    {
        if (mesh_type == MeshType::Simple2DRenderMesh)
            PaintVertices2D<gfx::Vertex2D>(painter);
        else if (mesh_type == MeshType::Simple2DShardEffectMesh)
            PaintVertices2D<gfx::ShardVertex2D>(painter);
        else if (mesh_type == MeshType::Dimetric2DRenderMesh || mesh_type == MeshType::Isometric2DRenderMesh)
            PaintVertices25D<gfx::Perceptual3DVertex>(painter);
        else BUG("Missing mesh type handling.");
    }

    // visualize the current triangle
    if (mMouseTool)
    {
        MouseTool::ViewState view;
        view.width = width;
        view.height = height;
        view.snap = GetValue(mUI.chkSnap);
        view.grid = GetValue(mUI.cmbGrid);
        mMouseTool->DrawTool(painter, view);
    }

    if (mPixelDistance2Dand3D)
    {
        const auto px_diff = static_cast<int>(mPixelDistance2Dand3D.value());
        mMessages.push_back("Use mouse wheel + keys 'x', 'y' and 'z' to adjust 3D point");
        mMessages.push_back(base::FormatString("Point difference %1 px %2", px_diff, px_diff == 0 ? "POINT SET" : ""));
    }
    if (mShaderEditor)
        mShaderEditor->ClearError();

}

void ShapeWidget::PaintLitAxonometricScene(const QRect& rect, const PolygonClassHandle& poly, gfx::Device* device) const
{
    const auto widget_width  = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    const auto view_width = static_cast<float>(rect.width());
    const auto view_height = static_cast<float>(rect.height());

    gfx::Painter painter;
    painter.SetDevice(device);
    painter.SetViewport(rect.x(), rect.y(), rect.width(), rect.height());
    painter.SetSurfaceSize(widget_width, widget_height);
    painter.SetPixelRatio({1.0f, 1.0f});
    painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, view_width, 0.0f, view_height, -1000.0f, 1000.0f));

    const auto tile_size = mAxonometricTileBaseSize;
    const auto light_position  = glm::vec3 { tile_size.x * 0.5f, tile_size.y * 0.5f, -200.0f } + mAxonometricLightPosition;
    const auto light_direction = glm::vec3 { 0.0f, 0.0f, 1.0f };

    gfx::Transform axonometric_model_to_view;
    axonometric_model_to_view.Resize(tile_size);

    gfx::PolygonMeshInstance instance(poly);
    gfx::PolygonMeshInstance::Perceptual3DGeometry geometry;
    geometry.axonometric_model_view = axonometric_model_to_view;
    instance.SetPerceptualGeometry(geometry);

    gfx::BasicLightProgram lit_program;
    if (mLightType != LightType::None)
    {
        gfx::BasicLightProgram::Light light;
        if (mLightType == LightType::Point)
            light.type = gfx::BasicLightProgram::LightType::Point;
        else if (mLightType == LightType::Spot)
            light.type = gfx::BasicLightProgram::LightType::Spot;
        else if (mLightType == LightType::Directional)
            light.type = gfx::BasicLightProgram::LightType::Directional;
        else BUG("Missing light type.");

        light.view_position = light_position;
        light.view_direction = light_direction;
        light.ambient_color = gfx::Color4f(gfx::Color::White) * 0.5f;
        light.diffuse_color = gfx::Color4f(gfx::Color::White) * 1.0f;
        light.specular_color = gfx::Color4f(gfx::Color::White) * 1.0f;
        light.quadratic_attenuation = 0.00005;
        light.spot_half_angle = gfx::FDegrees(35.0f);
        lit_program.AddLight(light);
    }
    // todo: this isn't correct. we should find the camera
    // position here such that the camera looks at the object
    // at the correct axonometric angle.
    lit_program.SetCameraCenter(200.0f, 200.0f, -200.0f);

    gfx::Painter::DrawState state;
    state.depth_test = gfx::Painter::DepthTest::Disabled;
    state.culling    = gfx::Painter::Culling::Back;
    state.line_width = 4.0f;

    gfx::Transform render_model_to_view;
    render_model_to_view.Resize(view_width, view_height);
    if (mBlueprint)
        painter.Draw(instance, render_model_to_view, *mBlueprint, state, lit_program);
    else painter.Draw(instance, render_model_to_view, gfx::CreateMaterialFromColor(gfx::Color::DarkGray), state, lit_program);

    if (mMainView == ViewType::LitView)
    {
        gfx::Painter tile_painter;
        tile_painter.SetDevice(painter.GetDevice());
        tile_painter.SetViewport(painter.GetViewport());
        tile_painter.SetSurfaceSize(painter.GetSurfaceSize());
        ConfigureTilePainter(tile_painter);

        if (mLightType == LightType::Point || mLightType == LightType::Spot)
        {
            gfx::FlatShadedColorProgram flat_program;
            gfx::Transform transform;
            transform.Resize(40.0f, 40.0f, 40.0f);
            transform.Translate(-tile_size.x, -tile_size.y);
            transform.Translate(light_position);
            tile_painter.Draw(gfx::Sphere(), transform, gfx::CreateMaterialFromColor(gfx::Color::White), state, flat_program);
        }
        mMessages.push_back("Use mouse wheel + keys 'x', 'y' and 'z' to adjust light position.");
        mMessages.push_back("Press 'p' for point light, 's' for spot light and 'd' for directional light.");
    }
    PaintViewRect(rect, "Axonometric Lit", device);
}

void ShapeWidget::Paint3DAxonometricScene(const QRect& rect, const PolygonClassHandle& polygon, gfx::Device* device) const
{
    const auto widget_width  = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    const auto view_width = static_cast<float>(rect.width());
    const auto view_height = static_cast<float>(rect.height());

    gfx::Painter painter;
    painter.SetDevice(device);
    painter.SetViewport(rect.x(), rect.y(), rect.width(), rect.height());
    painter.SetSurfaceSize(widget_width, widget_height);
    painter.SetPixelRatio({1.0f, 1.0f});
    painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(-500.0f, 500.0f, 500.0f, -500.0f, -1000.0f, 1000.0f));

    const auto cam_world_pos = glm::vec3(100.0f, 100.0f, 100.0f);
    const auto cam_look_at = glm::vec3(0.0f, 0.0f, 0.0f);
    painter.SetViewMatrix(glm::lookAt(cam_world_pos, cam_look_at, glm::vec3(0.0f, 1.0f, 0.0f)));

    gfx::Transform axonometric_model_to_view;
    gfx::PolygonMeshInstance instance(polygon);
    gfx::PolygonMeshInstance::Perceptual3DGeometry geometry;
    geometry.axonometric_model_view = axonometric_model_to_view;
    geometry.enable_perceptual_3D   = true;
    instance.SetPerceptualGeometry(geometry);

    gfx::Painter::DrawState state;
    state.depth_test = gfx::Painter::DepthTest::LessOrEQual;
    state.culling    = gfx::Painter::Culling::Back;

    const auto size = glm::vec3 {300.0f, 300.0f, 300.0f };
    const auto time = base::GetTime();
    auto checkerboard = gfx::CreateMaterialFromImage(res::Checkerboard);

    double angle = time * 30.0f;
    if (angle > 360.0f)
        angle -= 360.0f;

    gfx::FlatShadedColorProgram flat_program;
    gfx::Transform transform;
    transform.RotateAroundY(gfx::FDegrees(-angle));

    transform.Push();
        transform.Resize(size);
        transform.RotateAroundX(gfx::FDegrees(90.0f));
        transform.Translate(-size.x*0.5f, 0.0f, -size.y*0.5f);
        transform.Translate(0.0f, -100.0f,  0.0f);

    painter.Draw(instance, transform, gfx::CreateMaterialFromColor(gfx::Color::DarkGray), state, flat_program);
    painter.Draw(gfx::Rectangle(), transform, checkerboard, state, flat_program);

    PaintViewRect(rect, "Axonometric 3D", device);
}

void ShapeWidget::PaintViewRect(const QRect& rect, const app::AnyString& name, gfx::Device* device) const
{
    const auto widget_width  = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    const auto view_width = static_cast<float>(rect.width());
    const auto view_height = static_cast<float>(rect.height());

    gfx::Painter painter;
    painter.SetDevice(device);
    painter.SetViewport(rect.x(), rect.y(), rect.width(), rect.height());
    painter.SetSurfaceSize(widget_width, widget_height);
    painter.SetPixelRatio({1.0f, 1.0f});
    painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, view_width, 0.0f, view_height, -1000.0f, 1000.0f));

    gfx::Transform model;
    // a little tweak for OpenGL on windows that would not otherwise rasterize
    // the bottom and right edges
    model.Resize(view_width-2.0f, view_height-2.0f);
    model.Translate(1.0f, 1.0f);
    const auto mouse_pos_widget = mUI.widget->mapFromGlobal(QCursor::pos());
    const auto outline_color = rect.contains(mouse_pos_widget) ? gfx::Color::Green : gfx::Color::DarkGray;
    painter.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Outline), model,
                 gfx::CreateMaterialFromColor(outline_color));

    gfx::FRect text;
    text.Move(5.0f, 5.0f);
    text.Resize(rect.width() - 5.0f, 10.0f);
    gfx::DrawTextRect(painter, name, "app://fonts/orbitron-medium.otf", 12, text,
        gfx::Color::DarkGray, gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
}

void ShapeWidget::OnMousePress(QMouseEvent* mickey)
{
    const auto& rect = GetMainRenderRect();

    const bool shift = mickey->modifiers() & Qt::ShiftModifier;
    const bool ctrl = mickey->modifiers() & Qt::ControlModifier;
    const auto btn = mickey->button();

    if (rect.contains(mickey->pos()))
    {
        const float width  = rect.width();
        const float height = rect.height();
        const float xoffset = rect.x();
        const float yoffset = rect.y();
        const auto& point = mickey->pos() - QPoint(xoffset, yoffset);

        if (mMainView == ViewType::EditView)
        {
            if (btn == Qt::MouseButton::LeftButton && !mMouseTool)
            {
                auto pick_mode = PickMode::Sticky;
                if (shift)
                    pick_mode = PickMode::Cycling;

                const auto mesh_type = GetMeshType();
                if (mesh_type == MeshType::Simple2DRenderMesh)
                    PickVertex2D<gfx::Vertex2D>(point, width, height, pick_mode);
                else if (mesh_type == MeshType::Simple2DShardEffectMesh)
                    PickVertex2D<gfx::ShardVertex2D>(point, width, height, pick_mode);
                else if (mesh_type == MeshType::Dimetric2DRenderMesh || mesh_type == MeshType::Isometric2DRenderMesh)
                    PickVertex2D<gfx::Perceptual3DVertex>(point, width, height, pick_mode);
                else BUG("Unhandled mesh type.");
            }
        }
        if (mMouseTool)
        {
            MouseTool::ViewState view;
            view.grid   = GetValue(mUI.cmbGrid);
            view.snap   = GetValue(mUI.chkSnap);
            view.width  = width;
            view.height = height;
            mMouseTool->MousePress(mickey, point, view);
        }
    }
    else
    {
        const auto mesh_type = GetMeshType();
        if (mesh_type == MeshType::Dimetric2DRenderMesh || mesh_type == MeshType::Isometric2DRenderMesh)
        {
            const auto& lit = GetLitRenderRect();
            const auto& v3d = GetAxo3DRenderRect();
            if (lit.contains(mickey->pos()))
            {
                if (mMainView == ViewType::LitView)
                    mMainView = ViewType::EditView;
                else mMainView = ViewType::LitView;
            }
            else if (v3d.contains(mickey->pos()))
            {
                if (mMainView == ViewType::Axo3DView)
                    mMainView = ViewType::EditView;
                else mMainView = ViewType::Axo3DView;
            }
        }
    }
}

void ShapeWidget::OnMouseRelease(QMouseEvent* mickey)
{
    const auto& rect = GetMainRenderRect();
    const float xoffset = rect.x();
    const float yoffset = rect.y();
    const float width  = rect.width();
    const float height = rect.height();
    const auto& point = mickey->pos() - QPoint(xoffset, yoffset);
    const auto btn = mickey->button();

    const auto mesh_type = GetMeshType();

    if (!mMouseTool && btn == Qt::MouseButton::RightButton)
    {
        const auto have_selection = mSelectedVertex < mState.builder->GetVertexCount();

        if (mMainView == ViewType::EditView)
        {
            GfxMenu menu;
            menu.AddAction("Start New Triangle Fan", QIcon("icons32:triangle_fan.png"), [this]() {
                QTimer::singleShot(0, this, &ShapeWidget::on_actionNewTriangleFan_triggered);
            });
            menu.AddSeparator();
            if (mesh_type == MeshType::Dimetric2DRenderMesh || mesh_type == MeshType::Isometric2DRenderMesh)
            {
                GfxMenu orient_menu;
                orient_menu.SetText("Orient Normal");
                orient_menu.AddAction("Orient normal to +X", [this]() {
                    SetSelectedVertexNormal({1.0f, 0.0f, 0.0f});
                })->setEnabled(have_selection);
                orient_menu.AddAction("Orient normal to -X", [this]() {
                    SetSelectedVertexNormal({-1.0f, 0.0f, 0.0f});
                })->setEnabled(have_selection);
                orient_menu.AddAction("Orient normal to +Y", [this]() {
                    SetSelectedVertexNormal({0.0f, 1.0f, 0.0f});
                })->setEnabled(have_selection);
                orient_menu.AddAction("Orient normal to -Y", [this]() {
                    SetSelectedVertexNormal({0.0f, -1.0f, 0.0f});
                })->setEnabled(have_selection);
                orient_menu.AddAction("Orient normal to +Z", [this]() {
                    SetSelectedVertexNormal({0.0f, 0.0f, -1.0f});
                })->setEnabled(have_selection);
                orient_menu.AddAction("Orient normal to -Z", [this]() {
                    SetSelectedVertexNormal({0.0f, 0.0f, 1.0f});
                })->setEnabled(have_selection);
                menu.AddSubMenu(std::move(orient_menu));
            }

            menu.AddAction("Insert Vertex Before", [this]() {
                InsertVertex(InsertionPoint::Before);
            })->setEnabled(have_selection);
            menu.AddAction("Insert Vertex After", [this]() {
                InsertVertex(InsertionPoint::After);
            })->setEnabled(have_selection);
            menu.AddAction("Delete Vertex", QIcon("icons:delete.png"), [this]() {
                mState.table->EraseVertex(mSelectedVertex);
                mSelectedVertex = InvalidIndex;
                ClearSelection(mUI.tableView);
                if (mState.builder->GetVertexCount() == 0)
                    SetEnabled(mUI.actionClear, false);
            })->setEnabled(have_selection);

            menu.AddSeparator();
            menu.AddAction(mUI.actionClear);
            menu.AddSeparator();

            GfxMenu view_menu;
            view_menu.SetText("View");
            view_menu.SetIcon(mHamburger->icon());
            view_menu.AddActions(mHamburger->actions());

            GfxMenu grid_menu;
            grid_menu.SetText("Grid");
            grid_menu.SetIcon(QIcon("icons32:guidegrid.png"));
            grid_menu.AddAction("10x10",   [this]() { SetValue(mUI.cmbGrid, GridDensity::Grid10x10); });
            grid_menu.AddAction("20x20",   [this]() { SetValue(mUI.cmbGrid, GridDensity::Grid20x20); });
            grid_menu.AddAction("50x50",   [this]() { SetValue(mUI.cmbGrid, GridDensity::Grid50x50); });
            grid_menu.AddAction("100x100", [this]() { SetValue(mUI.cmbGrid, GridDensity::Grid100x100); });

            menu.AddSubMenu(std::move(view_menu));
            menu.AddSeparator();
            menu.AddSubMenu(std::move(grid_menu));
            mUI.widget->OpenContextMenu(mickey->pos(), std::move(menu));
        }
    }

    if (mMouseTool)
    {
        MouseTool::ViewState view;
        view.grid   = GetValue(mUI.cmbGrid);
        view.snap   = GetValue(mUI.chkSnap);
        view.width  = width;
        view.height = height;
        if (mMouseTool->MouseRelease(mickey, point, view))
            mMouseTool.reset();
    }
}

void ShapeWidget::OnMouseMove(QMouseEvent* mickey)
{
    const auto& rect = GetMainRenderRect();
    const float xoffset = rect.x();
    const float yoffset = rect.y();
    const float width  = rect.width();
    const float height = rect.height();
    const auto& point = mickey->pos() - QPoint(xoffset, yoffset);

    if (mMouseTool)
    {
        MouseTool::ViewState view;
        view.grid   = GetValue(mUI.cmbGrid);
        view.snap   = GetValue(mUI.chkSnap);
        view.width  = width;
        view.height = height;
        mMouseTool->MouseMove(mickey, point, view);
    }
    else
    {
        const auto mesh_type = GetMeshType();
        if (mesh_type == MeshType::Simple2DRenderMesh)
            HoverVertex2D<gfx::Vertex2D>(point, width, height);
        else if (mesh_type == MeshType::Simple2DShardEffectMesh)
            HoverVertex2D<gfx::ShardVertex2D>(point, width, height);
        else if (mesh_type == MeshType::Dimetric2DRenderMesh || mesh_type == MeshType::Isometric2DRenderMesh)
            HoverVertex2D<gfx::Perceptual3DVertex>(point, width, height);
        else BUG("Unhandled mesh type.");
    }
}

void ShapeWidget::OnMouseDoubleClick(QMouseEvent* mickey)
{

}

void ShapeWidget::OnMouseWheel(QWheelEvent* wheel)
{
    const auto mesh_type = GetMeshType();
    if (mesh_type == MeshType::Isometric2DRenderMesh || mesh_type == MeshType::Dimetric2DRenderMesh)
    {
        if (mMainView == ViewType::EditView)
            return ScrollAxonometricVertex<gfx::Perceptual3DVertex>(wheel);
        else if (mMainView == ViewType::LitView)
            return ScrollAxonometricLight(wheel);
    }
}

bool ShapeWidget::OnKeyPressEvent(QKeyEvent* key)
{
    auto SetLight = [this](LightType light) {
        if (mLightType == light)
            mLightType = LightType::None;
        else mLightType = light;
    };

    if (key->key() == Qt::Key_Escape)
    {
        OnEscape();
    }
    else if (key->key() == Qt::Key_Delete)
    {
        if (mSelectedVertex < mState.builder->GetVertexCount())
        {
            mState.table->EraseVertex(mSelectedVertex);
            mSelectedVertex = InvalidIndex;
            ClearSelection(mUI.tableView);
        }

        if (mState.builder->GetVertexCount() == 0)
            mUI.actionClear->setEnabled(false);
    }
    else if (key->key() == Qt::Key_E)
    {

    }
    else if (key->key() == Qt::Key_X)
        mHotkeysPressed.insert(Hotkey::KeyX);
    else if (key->key() == Qt::Key_Y)
        mHotkeysPressed.insert(Hotkey::KeyY);
    else if (key->key() == Qt::Key_Z)
        mHotkeysPressed.insert(Hotkey::KeyZ);
    else if (key->key() == Qt::Key_P)
        SetLight(LightType::Point);
    else if (key->key() == Qt::Key_S)
        SetLight(LightType::Spot);
    else if (key->key() == Qt::Key_D)
        SetLight(LightType::Directional);
    else  return false;

    return true;
}

bool ShapeWidget::OnKeyReleaseEvent(QKeyEvent* key)
{
    if (key->key() == Qt::Key_X)
        mHotkeysPressed.erase(Hotkey::KeyX);
    else if (key->key() == Qt::Key_Y)
        mHotkeysPressed.erase(Hotkey::KeyY);
    else if (key->key() == Qt::Key_Z)
        mHotkeysPressed.erase(Hotkey::KeyZ);
    else return false;

    return true;
}

template<typename VertexType>
void ShapeWidget::PaintVertices2D(gfx::Painter& painter) const
{
    using BuilderType = gfx::tool::PolygonBuilder<VertexType>;

    const auto& rect = GetViewRect(ViewType::EditView);
    const float width  = rect.width();
    const float height = rect.height();

    const auto* builder = dynamic_cast<const BuilderType*>(mState.builder.get());

    for (size_t i = 0; i < mState.builder->GetVertexCount(); ++i)
    {
        const auto& vert = builder->GetVertex(i);
        const auto x = width * vert.aPosition.x;
        const auto y = height * -vert.aPosition.y;

        gfx::Transform model;
        model.Resize(15, 15);
        model.MoveTo(x, y);
        model.Translate(-7.5, -7.5);

        if (IsSelected(i))
        {
            painter.Draw(gfx::Circle(gfx::Circle::Style::Outline), model, gfx::CreateMaterialFromColor(gfx::Color::Green));
            if (!mMouseTool)
            {
                const auto time = fmod(base::GetTime(), 1.5) / 1.5;
                const auto size = 15.0f + time * 50.0f;
                gfx::Transform model;
                model.Resize(size, size);
                model.MoveTo(x, y);
                model.Translate(-size*0.5f, -size*0.5f);
                painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Outline), model, gfx::CreateMaterialFromColor(gfx::Color::Green));
            }
        }
        else if (IsHovered(i))
        {
            painter.Draw(gfx::Circle(gfx::Circle::Style::Outline), model, gfx::CreateMaterialFromColor(gfx::Color::Yellow));
        }
        else
        {
            painter.Draw(gfx::Circle(gfx::Circle::Style::Outline), model, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
        }
    }
}

template<typename VertexType>
void ShapeWidget::PaintVertices25D(gfx::Painter& painter) const
{
    using BuilderType = gfx::tool::PolygonBuilder<VertexType>;

    const auto& rect = GetViewRect(ViewType::EditView);
    const float width  = rect.width();
    const float height = rect.height();
    const bool show_normals = GetValue(mUI.chkShowNormals);

    const auto* builder = dynamic_cast<const BuilderType*>(mState.builder.get());

    const auto tile_base_size = mAxonometricTileBaseSize;

    gfx::Painter tile_painter;
    tile_painter.SetDevice(painter.GetDevice());
    tile_painter.SetViewport(painter.GetViewport());
    tile_painter.SetSurfaceSize(painter.GetSurfaceSize());
    ConfigureTilePainter(tile_painter);

    for (size_t i = 0; i < mState.builder->GetVertexCount(); ++i)
    {
        const auto& vertex = builder->GetVertex(i);
        const auto x = width * vertex.aPosition.x;
        const auto y = height * -vertex.aPosition.y;

        gfx::Transform model;
        model.Resize(15, 15);
        model.MoveTo(x, y);
        model.Translate(-7.5, -7.5);

        if (IsSelected(i))
        {
            painter.Draw(gfx::Circle(gfx::Circle::Style::Outline), model, gfx::CreateMaterialFromColor(gfx::Color::Green));

            // local offset is normalized and expressed in multiples of the tile size.
            // so always remember to multiple the local offset by the desired tile size
            // to express the coordinate in the current tile space units.
            const auto& local_offset = gfx::ToVec(vertex.aLocalOffset);

            gfx::Transform tile_vertex;
            tile_vertex.Resize(10.0f, 10.0f, 10.0f);
            tile_vertex.MoveTo(-tile_base_size.x, -tile_base_size.y, 0.0f);
            tile_vertex.Translate(tile_base_size * local_offset);
            //tile_painter.Draw(gfx::Sphere(), tile_vertex, gfx::CreateMaterialFromColor(gfx::Color::White));

            gfx::Transform line;
            line.MoveTo(-tile_base_size.x, -tile_base_size.y, 0.0f);
            tile_painter.Draw(gfx::LineBatch3D(
                { local_offset.x * tile_base_size.x, 0.0f, local_offset.z * tile_base_size.z },
                { local_offset.x * tile_base_size.x, tile_base_size.y, local_offset.z * tile_base_size.z }), line,
                gfx::CreateMaterialFromColor(gfx::Color::Green), 2.0f);

            tile_painter.Draw(gfx::LineBatch3D(
                { 0.0f, local_offset.y * tile_base_size.y, local_offset.z * tile_base_size.z },
                { tile_base_size.x, local_offset.y * tile_base_size.y, local_offset.z * tile_base_size.z }), line,
                gfx::CreateMaterialFromColor(gfx::Color::Red), 2.0f);


            if (!mMouseTool)
            {
                const auto time = fmod(base::GetTime(), 1.5) / 1.5;
                const auto size = 15.0f + time * 50.0f;
                gfx::Transform model;
                model.Resize(size, size);
                model.MoveTo(x, y);
                model.Translate(-size*0.5f, -size*0.5f);
                painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Outline), model, gfx::CreateMaterialFromColor(gfx::Color::Green));
            }
        }
        else if (IsHovered(i))
        {
            painter.Draw(gfx::Circle(gfx::Circle::Style::Outline), model, gfx::CreateMaterialFromColor(gfx::Color::Yellow));
        }
        else
        {
            painter.Draw(gfx::Circle(gfx::Circle::Style::Outline), model, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
        }

        if (show_normals)
        {
            const auto normal = glm::normalize(gfx::ToVec(vertex.aWorldNormal));
            const auto offset = gfx::ToVec(vertex.aLocalOffset);
            const auto line_point_a = glm::vec3 { -tile_base_size.x, -tile_base_size.y, 0.0f } +
                                      tile_base_size * offset;
            const auto line_point_b = line_point_a + normal * 50.0f;
            gfx::Transform model;
            tile_painter.Draw(gfx::LineBatch3D(line_point_a, line_point_b), model,
                gfx::CreateMaterialFromColor(gfx::Color::HotPink), 1.0f);
        }
    }
}


template<typename VertexType>
void ShapeWidget::PickVertex2D(const QPoint& pick_point, float width, float height, PickMode mode)
{
    DEBUG("Pick vertex with mode '%1'", mode);

    using BuilderType = gfx::tool::PolygonBuilder<VertexType>;
    using ToolType = MoveVertex2DTool<VertexType>;

    const auto* builder = dynamic_cast<const BuilderType*>(mState.builder.get());

    const auto previous_selection = mSelectedVertex;

    mSelectedVertex = InvalidIndex;
    if (mPickingCandidates.empty())
        return;

    if (mode == PickMode::Normal || mode == PickMode::Sticky)
    {
        if (mode == PickMode::Sticky)
        {
            for (const auto& p : mPickingCandidates)
            {
                if (p.index == previous_selection)
                {
                    mSelectedVertex = p.index;
                    break;
                }
            }
        }
        if (mSelectedVertex == InvalidIndex)
        {
            auto smallest_distance = mPickingCandidates[0].distance;
            mSelectedVertex = mPickingCandidates[0].index;
            for (size_t i=1; i<mPickingCandidates.size(); ++i)
            {
                if (mPickingCandidates[i].distance < smallest_distance)
                {
                    smallest_distance = mPickingCandidates[i].distance;
                    mSelectedVertex = mPickingCandidates[i].index;
                }
            }
        }
    }
    else if (mode == PickMode::Cycling)
    {
        const auto index = (mPickingIndex++) % mPickingCandidates.size();
        mSelectedVertex = mPickingCandidates[index].index;
    }
    ASSERT(mSelectedVertex != InvalidIndex);
    ASSERT(mSelectedVertex < builder->GetVertexCount());

    mMouseTool = std::make_unique<ToolType>(mState, mSelectedVertex);
    SelectRow(mUI.tableView, mSelectedVertex);
}

template<typename VertexType>
void ShapeWidget::HoverVertex2D(const QPoint& pick_point, float width, float height)
{
    using BuilderType = gfx::tool::PolygonBuilder<VertexType>;

    const auto* builder = dynamic_cast<const BuilderType*>(mState.builder.get());

    mPickingCandidates.clear();
    mPickingIndex = 0;

    // select a vertex based on proximity to the click point.
    for (size_t i=0; i<mState.builder->GetVertexCount(); ++i)
    {
        const auto& vertex = builder->GetVertex(i);
        const auto x = width * vertex.aPosition.x;
        const auto y = height * -vertex.aPosition.y;
        const auto r = QPoint(x, y) - pick_point;
        const auto distance = std::sqrt(r.x() * r.x() + r.y() * r.y());
        if (distance <= 10.0f)
        {
            PickCandidate pc;
            pc.distance = distance;
            pc.index = i;
            pc.tangent = atan2(r.y(), r.x());
            mPickingCandidates.push_back(pc);
        }
    }

    std::sort(mPickingCandidates.begin(), mPickingCandidates.end(), [](const auto& a, const auto& b) {
        return a.tangent < b.tangent;
    });
}

void ShapeWidget::InsertVertex(InsertionPoint where)
{
    const auto mesh_type = GetMeshType();
    if (mesh_type == MeshType::Simple2DRenderMesh)
        InsertVertex2D<gfx::Vertex2D>(where);
    else if (mesh_type == MeshType::Simple2DShardEffectMesh)
        InsertVertex2D<gfx::ShardVertex2D>(where);
    else if (mesh_type == MeshType::Dimetric2DRenderMesh || mesh_type == MeshType::Isometric2DRenderMesh)
        InsertVertex2D<gfx::Perceptual3DVertex>(where);
    else BUG("Missing mesh type handling.");
}

template<typename VertexType>
void ShapeWidget::InsertVertex2D(InsertionPoint where)
{
    if (mSelectedVertex >= mState.builder->GetVertexCount())
        return;

    using BuilderType = gfx::tool::PolygonBuilder<VertexType>;
    const auto* builder = dynamic_cast<const BuilderType*>(mState.builder.get());
    const auto vertex_index = mSelectedVertex;
    const auto cmd_index = mState.builder->FindDrawCommand(vertex_index);

    // junk vertex that doesn't belong anywhere
    if (cmd_index == std::numeric_limits<size_t>::max())
        return;

    // degenerate left over triangle ?
    const auto& cmd = mState.builder->GetDrawCommand(cmd_index);
    if (cmd.count < 3)
        return;

    const auto cmd_vertex_count = cmd.count;
    const auto cmd_vertex_offset = cmd.offset;

    // we're going to interpolate between one and two
    // and index_new will be the insertion index
    auto cmd_vertex_index_one = vertex_index - cmd.offset;
    auto cmd_vertex_index_two = 0;
    auto cmd_vertex_index_new = 0;
    if (where == InsertionPoint::After)
    {
        cmd_vertex_index_two = (cmd_vertex_index_one + 1) % cmd_vertex_count;
        cmd_vertex_index_new = cmd_vertex_index_one + 1;
    }
    else if (where == InsertionPoint::Before)
    {
        if (cmd_vertex_index_one == 0)
        {
            cmd_vertex_index_two = cmd_vertex_count - 1;
            cmd_vertex_index_new = cmd_vertex_count;
        }
        else
        {
            cmd_vertex_index_two = cmd_vertex_index_one - 1;
            cmd_vertex_index_new = cmd_vertex_index_one;
        }
    }

    const auto& vertex_one = builder->GetVertex(cmd_vertex_offset + cmd_vertex_index_one);
    const auto& vertex_two = builder->GetVertex(cmd_vertex_offset + cmd_vertex_index_two);
    const auto& vertex_new = gfx::InterpolateVertex(vertex_one, vertex_two, 0.5f);
    mState.table->InsertVertex(vertex_new, cmd_vertex_index_new, cmd_index);
    if (where == InsertionPoint::After)
    {
        mSelectedVertex++;
        SelectRow(mUI.tableView, mSelectedVertex);
    }
}

template<typename VertexType>
void ShapeWidget::ScrollAxonometricVertex(const QWheelEvent* wheel)
{
    if (mSelectedVertex >= mState.builder->GetVertexCount())
        return;

    using BuilderType = gfx::tool::PolygonBuilder<VertexType>;

    auto* builder = dynamic_cast<BuilderType*>(mState.builder.get());

    const QPoint& num_degrees = wheel->angleDelta() / 8;
    const QPoint& num_steps = num_degrees / 15;

    const auto mods = wheel->modifiers();

    float step = 0.1f;
    if (mPixelDistance2Dand3D.has_value())
    {
        const auto px_diff = static_cast<int>(mPixelDistance2Dand3D.value());
        if (px_diff < 50)
            step = 0.05f;
        if (px_diff < 20)
            step = 0.01f;
        if (px_diff < 5)
            step = 0.005f;
    }
    if (mods & Qt::ShiftModifier)
        step /= 2.0f;

    const auto d = -step * num_steps.y();

    auto& vertex = builder->GetVertex(mSelectedVertex);

    if (base::Contains(mHotkeysPressed, Hotkey::KeyX))
        vertex.aLocalOffset.x += d;
    else if (base::Contains(mHotkeysPressed, Hotkey::KeyY))
        vertex.aLocalOffset.y += d;
    else if (base::Contains(mHotkeysPressed, Hotkey::KeyZ))
        vertex.aLocalOffset.z += d;

    vertex.aLocalOffset.x = math::clamp(0.0f, 1.0f, vertex.aLocalOffset.x);
    vertex.aLocalOffset.y = math::clamp(0.0f, 1.0f, vertex.aLocalOffset.y);
    if (vertex.aLocalOffset.z > 0.0f)
        vertex.aLocalOffset.z = 0.0f;

    mState.table->RefreshVertex(mSelectedVertex);
}

void ShapeWidget::ScrollAxonometricLight(const QWheelEvent* wheel)
{
    const QPoint& num_degrees = wheel->angleDelta() / 8;
    const QPoint& num_steps = num_degrees / 15;
    const float step = 20.0f;
    const float steps = step * num_steps.y();

    if (base::Contains(mHotkeysPressed, Hotkey::KeyX))
        mAxonometricLightPosition.x += steps;
    else if (base::Contains(mHotkeysPressed, Hotkey::KeyY))
        mAxonometricLightPosition.y += steps;
    else if (base::Contains(mHotkeysPressed, Hotkey::KeyZ))
        mAxonometricLightPosition.z += steps;
}

ShapeWidget::MeshType ShapeWidget::GetMeshType() const
{
    const auto type = mState.polygon->GetMeshType();
    if (type == gfx::PolygonMeshClass::MeshType::Simple2DRenderMesh)
        return MeshType::Simple2DRenderMesh;
    else if (type == gfx::PolygonMeshClass::MeshType::Simple2DShardEffectMesh)
        return MeshType::Simple2DShardEffectMesh;
    else if (type == gfx::PolygonMeshClass::MeshType::Dimetric2DRenderMesh)
        return MeshType::Dimetric2DRenderMesh;
    else if (type == gfx::PolygonMeshClass::MeshType::Isometric2DRenderMesh)
        return MeshType::Isometric2DRenderMesh;
    else BUG("Unhandled mesh type.");
    return MeshType::Simple2DRenderMesh;
}

void ShapeWidget::SetMeshType(MeshType mesh_type)
{
    mState.polygon->ClearContent();

    if (mesh_type == MeshType::Simple2DRenderMesh)
    {
        mState.polygon->SetMeshType(gfx::PolygonMeshClass::MeshType::Simple2DRenderMesh);
        mState.polygon->SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    }
    else if (mesh_type == MeshType::Simple2DShardEffectMesh)
    {
        mState.polygon->SetMeshType(gfx::PolygonMeshClass::MeshType::Simple2DShardEffectMesh);
        mState.polygon->SetVertexLayout(gfx::GetVertexLayout<gfx::ShardVertex2D>());

    }
    else if (mesh_type == MeshType::Dimetric2DRenderMesh)
    {
        mState.polygon->SetMeshType(gfx::PolygonMeshClass::MeshType::Dimetric2DRenderMesh);
        mState.polygon->SetVertexLayout(gfx::GetVertexLayout<gfx::Perceptual3DVertex>());
    }
    else if (mesh_type == MeshType::Isometric2DRenderMesh)
    {
        mState.polygon->SetMeshType(gfx::PolygonMeshClass::MeshType::Isometric2DRenderMesh);
        mState.polygon->SetVertexLayout(gfx::GetVertexLayout<gfx::Perceptual3DVertex>());
    }
    else BUG("Unhandled mesh type.");
}

void ShapeWidget::CreateMeshBuilder()
{
    const auto type = GetMeshType();

    if (type == MeshType::Simple2DRenderMesh)
    {
        mState.builder = std::make_unique<gfx::tool::PolygonBuilder2D>();
        mState.table   = std::make_unique<VertexDataTable>(*mState.builder, *mState.polygon);
    }
    else if (type == MeshType::Simple2DShardEffectMesh)
    {
        mState.builder = std::make_unique<gfx::tool::PolygonBuilderShard2D>();
        mState.table   = std::make_unique<VertexDataTable>(*mState.builder, *mState.polygon);

    }
    else if (type == MeshType::Dimetric2DRenderMesh || type == MeshType::Isometric2DRenderMesh)
    {
        mState.builder = std::make_unique<gfx::tool::PolygonBuilderPerceptual3D>();
        mState.table   = std::make_unique<VertexDataTable>(*mState.builder, *mState.polygon);
    }
    else BUG("Unhandled mesh type.");

    mUI.tableView->setModel(mState.table.get());
    // Adwaita theme bugs out and doesn't show the data without this
    // little hack here.
    mState.table->Reset();

    QObject::connect(mUI.tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
        [this](const QItemSelection&, const QItemSelection&) {
            const auto row = GetSelectedRow(mUI.tableView);
            mSelectedVertex = row;
        });
}

void ShapeWidget::SetSelectedVertexNormal(const glm::vec3& normal)
{
    if (mSelectedVertex >= mState.builder->GetVertexCount())
        return;

    const auto mesh_type = GetMeshType();
    if (mesh_type == MeshType::Dimetric2DRenderMesh || mesh_type == MeshType::Isometric2DRenderMesh)
    {
        using BuilderType = gfx::tool::PolygonBuilder<gfx::Perceptual3DVertex>;
        auto* builder = dynamic_cast<BuilderType*>(mState.builder.get());
        auto& vertex = builder->GetVertex(mSelectedVertex);
        vertex.aWorldNormal = gfx::ToVec(normal);

        mState.table->RefreshVertex(mSelectedVertex);
    }
}

void ShapeWidget::ConfigureTilePainter(gfx::Painter& tile_painter) const
{
    const auto mesh_type = GetMeshType();
    const auto game_view = mesh_type == MeshType::Dimetric2DRenderMesh
                               ? engine::GameView::EnumValue::Dimetric
                               : engine::GameView::EnumValue::Isometric;

    const auto ortho_left = static_cast<float>(mAxonometricTextureWidth) / -2.0f;
    const auto ortho_right = static_cast<float>(mAxonometricTextureWidth) / 2.0f;
    const auto ortho_bottom = 0.0f;
    const auto ortho_top = static_cast<float>(mAxonometricTextureHeight); // / 2.0f;

    tile_painter.SetPixelRatio({1.0f, 1.0f});
    tile_painter.SetProjectionMatrix(glm::ortho(ortho_left, ortho_right, ortho_bottom, ortho_top, -10000.0f, 10000.0f));
    tile_painter.SetViewMatrix(engine::CreateModelViewMatrix(game_view, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f));
}

QRect ShapeWidget::GetMainRenderRect() const
{
    const auto widget_width  = mUI.widget->width() - Margin * 2;
    const auto widget_height = mUI.widget->height() - Margin * 2;
    const auto size = std::min(widget_width, widget_height);
    const auto width = static_cast<float>(size);
    const auto height = static_cast<float>(size);

    const auto small_rect_height = (height - 3 * Margin) / 4;
    const auto small_rect_width  = small_rect_height;
    const auto total_width = small_rect_width + width + Margin;

    auto xoffset = Margin; // + (widget_width - size) / 2;
    auto yoffset = Margin; // + (widget_height - size) / 2;
    if (widget_width > total_width)
        xoffset = (widget_width - total_width) / 2;

    return QRect(xoffset, yoffset, width, height);
}

QRect ShapeWidget::GetLitRenderRect() const
{
    const auto widget_width  = mUI.widget->width() - Margin * 2;
    const auto widget_height = mUI.widget->height() - Margin * 2;
    const auto main_rect = GetMainRenderRect();
    const auto width = widget_width - main_rect.width() - Margin;
    const auto height = (main_rect.height() - 3 * Margin) / 4; // space for 4 previews
    const auto size = std::min(width, height);

    const auto xoffset = Margin + main_rect.x() + main_rect.width();
    const auto yoffset = Margin;
    return QRect(xoffset, yoffset, size, size);
}

QRect ShapeWidget::GetAxo3DRenderRect() const
{
    QRect rect = GetLitRenderRect();
    rect.translate(0, rect.height());
    rect.translate(0, Margin);
    return rect;
}

QRect ShapeWidget::GetViewRect(ViewType view) const
{
    if (mMainView == ViewType::EditView && view == ViewType::EditView)
        return GetMainRenderRect();
    else if (mMainView == ViewType::EditView && view == ViewType::LitView)
        return GetLitRenderRect();
    else if (mMainView == ViewType::EditView && view == ViewType::Axo3DView)
        return GetAxo3DRenderRect();
    else if (mMainView == ViewType::LitView && view == ViewType::EditView)
        return GetLitRenderRect();
    else if (mMainView == ViewType::LitView && view == ViewType::LitView)
        return GetMainRenderRect();
    else if (mMainView == ViewType::LitView && view == ViewType::Axo3DView)
        return GetAxo3DRenderRect();
    else if (mMainView == ViewType::Axo3DView && view == ViewType::EditView)
        return GetAxo3DRenderRect();
    else if (mMainView == ViewType::Axo3DView && view == ViewType::LitView)
        return GetLitRenderRect();
    else if (mMainView == ViewType::Axo3DView && view == ViewType::Axo3DView)
        return GetMainRenderRect();
    else BUG("Missing view rect mapping handling");
    return QRect();
}

bool ShapeWidget::IsHovered(size_t index) const
{
    for (const auto& candidate : mPickingCandidates)
    {
        if (candidate.index == index)
            return true;
    }
    return false;
}

bool ShapeWidget::IsSelected(size_t index) const
{
    return mSelectedVertex == index;
}

} // namespace
