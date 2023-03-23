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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QFileDialog>
#  include <QMessageBox>
#  include <base64/base64.h>
#include "warnpop.h"

#include <map>

#include "audio/element.h"
#include "audio/graph.h"
#include "audio/format.h"
#include "audio/player.h"
#include "audio/device.h"
#include "data/json.h"

#include "editor/app/workspace.h"
#include "editor/app/eventlog.h"
#include "editor/app/format.h"
#include "editor/gui/audiowidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/settings.h"
#include "editor/gui/clipboard.h"

namespace {
struct PortDesc {
    std::string name;
    QRectF rect;
    QPointF link_pos;
};
struct ArgDesc {
    std::string name;
    audio::GraphClass::ElementArg arg;
};

struct ElementDesc {
    std::string type;
    std::vector<ArgDesc> args;
    std::vector<PortDesc> input_ports;
    std::vector<PortDesc> output_ports;
};

using ElementMap = std::map<std::string, ElementDesc>;

bool FindAudioFileInfo(const std::string& file, audio::FileSource::FileInfo* info)
{
    static std::unordered_map<std::string, audio::FileSource::FileInfo> cache;
    auto it = cache.find(file);
    if (it != cache.end()) {
        *info = it->second;
        return true;
    }
    if (!audio::FileSource::ProbeFile(file, info))
        return false;
    cache[file] = *info;
    return true;
}

ElementMap GetElementMap()
{
    static ElementMap map;
    if (!map.empty())
        return map;

    const auto& elements = audio::ListAudioElements();
    for (const auto& name : elements)
    {
        const auto* desc = audio::FindElementDesc(name);
        ElementDesc elem;
        elem.type = name;
        for (const auto& pair : desc->args)
        {
            ArgDesc arg;
            arg.name = pair.first;
            arg.arg  = pair.second;
            elem.args.push_back(std::move(arg));
        }
        for (const auto& p : desc->input_ports)
        {
            PortDesc port;
            port.name = p.name;
            elem.input_ports.push_back(std::move(port));
        }
        for (const auto& p : desc->output_ports)
        {
            PortDesc port;
            port.name = p.name;
            elem.output_ports.push_back(std::move(port));
        }
        map[name] = elem;
    }
    return map;
}

ElementDesc FindElementDescription(const std::string& type)
{ return GetElementMap()[type]; }


class AudioLink : public QGraphicsItem
{
public:
    AudioLink(const AudioLink&) = delete;
    AudioLink()
      : mId(base::RandomString(10))
    {
        setFlag(QGraphicsItem::ItemIsMovable, false);
        setFlag(QGraphicsItem::ItemIsSelectable, false);
    }
    AudioLink(const audio::GraphClass::Link& link)
      : mId(link.id)
    {
        mSrcElem = link.src_element;
        mDstElem = link.dst_element;
        mSrcPort = link.src_port;
        mDstPort = link.dst_port;
        setFlag(QGraphicsItem::ItemIsMovable, false);
        setFlag(QGraphicsItem::ItemIsSelectable, false);
    }
    void SetCurve(const QPointF& src, const QPointF& dst)
    {
        mSrc = src;
        mDst = dst;
        this->update();
    }
    void SetSrc(const std::string& src_elem, const std::string& src_port)
    {
        mSrcElem = src_elem;
        mSrcPort = src_port;
    }
    void SetDst(const std::string& dst_elem, const std::string& dst_port)
    {
        mDstElem = dst_elem;
        mDstPort = dst_port;
    }
    void SetId(const std::string& id)
    { mId = id; }

    void ApplyState(audio::GraphClass& klass) const
    {
        audio::GraphClass::Link link;
        link.id = mId;
        link.src_element = mSrcElem;
        link.dst_element = mDstElem;
        link.src_port = mSrcPort;
        link.dst_port = mDstPort;
        klass.AddLink(std::move(link));
    }

    bool Validate() const
    {
        return true;
    }
    virtual QRectF boundingRect() const override
    {
        const auto& src = mapFromScene(mSrc);
        const auto& dst = mapFromScene(mDst);
        const auto top    = std::min(src.y(), dst.y());
        const auto left   = std::min(src.x(), dst.x());
        const auto right  = std::max(src.x(), dst.x());
        const auto bottom = std::max(src.y(), dst.y());
        return QRectF(left, top, right-left, bottom-top);
    }
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        const auto& src = mapFromScene(mSrc);
        const auto& dst = mapFromScene(mDst);
        const auto& rect = boundingRect();

        QPainterPath path;
        path.moveTo(src);
        path.cubicTo(QPointF(dst.x(), src.y()),
                     QPointF(src.x(), dst.y()),
                     dst);
        QPen pen;
        pen.setColor(QColor(0, 128, 0));
        pen.setWidth(10);
        painter->setPen(pen);
        painter->drawPath(path);
    }
    QPointF GetSrcPoint() const
    { return mSrc; }
    QPointF GetDstPoint() const
    { return mDst; }
    const std::string& GetSrcElem() const
    { return mSrcElem; }
    const std::string& GetDstElem() const
    { return mDstElem; }
    const std::string& GetSrcPort() const
    { return mSrcPort; }
    const std::string& GetDstPort() const
    { return mDstPort; }
    const std::string& GetLinkId() const
    { return mId; }

    void LoadState(const app::Resource& resource)
    {
        float src_x, src_y;
        float dst_x, dst_y;
        resource.GetProperty(app::FromUtf8("link_" + mId + "_src_x"), &src_x);
        resource.GetProperty(app::FromUtf8("link_" + mId + "_src_y"), &src_y);
        resource.GetProperty(app::FromUtf8("link_" + mId + "_dst_x"), &dst_x);
        resource.GetProperty(app::FromUtf8("link_" + mId + "_dst_y"), &dst_y);
        mSrc = QPointF(src_x, src_y);
        mDst = QPointF(dst_x, dst_y);
    }

    void SaveState(app::Resource& resource) const
    {
        resource.SetProperty(app::FromUtf8("link_" + mId + "_src_x"), mSrc.x());
        resource.SetProperty(app::FromUtf8("link_" + mId + "_src_y"), mSrc.y());
        resource.SetProperty(app::FromUtf8("link_" + mId + "_dst_x"), mDst.x());
        resource.SetProperty(app::FromUtf8("link_" + mId + "_dst_y"), mDst.y());
    }

    void IntoJson(data::Writer& writer) const
    {
        writer.Write("id", mId);
        writer.Write("src_point", gui::ToVec2(mSrc));
        writer.Write("dst_point", gui::ToVec2(mDst));
        writer.Write("src_elem", mSrcElem);
        writer.Write("src_port", mSrcPort);
        writer.Write("dst_elem", mDstElem);
        writer.Write("dst_port", mDstPort);
    }
    void FromJson(const data::Reader& reader)
    {
        glm::vec2 src_point;
        glm::vec2 dst_point;
        reader.Read("id", &mId);
        reader.Read("src_point", &src_point);
        reader.Read("dst_point", &dst_point);
        reader.Read("src_elem", &mSrcElem);
        reader.Read("src_port", &mSrcPort);
        reader.Read("dst_elem", &mDstElem);
        reader.Read("dst_port", &mDstPort);
        mSrc = QPointF(src_point.x, src_point.y);
        mDst = QPointF(dst_point.x, dst_point.y);
    }
    AudioLink& operator=(const AudioLink&) = delete;
private:
    std::string mId;
    QPointF mSrc;
    QPointF mDst;
    std::string mSrcElem;
    std::string mSrcPort;
    std::string mDstElem;
    std::string mDstPort;
};

class AudioElement : public QGraphicsItem
{
public:
    AudioElement(const audio::GraphClass::Element& elem)
      : mId(elem.id)
      , mType(elem.type)
      , mName(elem.name)
    {
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);

        const auto& desc = FindElementDescription(mType);
        mIPorts = desc.input_ports;
        mOPorts = desc.output_ports;
        mArgs   = desc.args;
        for (auto& arg : mArgs)
        {
            auto it = elem.args.find(arg.name);
            if (it == elem.args.end())
                continue;
            arg.arg = it->second;
        }

        if (!elem.input_ports.empty())
        {
            std::vector<PortDesc> ports;
            for (const auto& port : elem.input_ports)
            {
                PortDesc desc;
                desc.name = port.name;
                ports.push_back(std::move(desc));
            }
            mIPorts = std::move(ports);
        }
        if (!elem.output_ports.empty())
        {
            std::vector<PortDesc> ports;
            for (const auto& port : elem.output_ports)
            {
                PortDesc desc;
                desc.name = port.name;
                ports.push_back(std::move(desc));
            }
            mOPorts = std::move(ports);
        }
        ComputePorts();
    }

    AudioElement(const ElementDesc& desc)
      : mId(base::RandomString(10))
      , mType(desc.type)
      , mIPorts(desc.input_ports)
      , mOPorts(desc.output_ports)
      , mArgs(desc.args)
    {
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);
        ComputePorts();
    }

    bool IsFileSource() const
    { return mType == "FileSource"; }
    unsigned GetNumOutputPorts() const
    { return mOPorts.size(); }
    unsigned GetNumInputPorts() const
    { return mIPorts.size(); }
    const PortDesc& GetOutputPort(unsigned index) const
    { return mOPorts[index]; }
    const PortDesc& GetInputPort(unsigned index) const
    { return mIPorts[index]; }

    std::string GetName() const
    { return mName; }
    std::string GetId() const
    { return mId; }
    void SetName(const std::string& name)
    { mName = name; }
    void SetId(const std::string& id)
    { mId = id; }

    bool HasArgument(const std::string& name) const
    {
        for (const auto& a : mArgs)
            if (a.name == name) return true;
        return false;
    }

    template<typename T>
    T* GetArgValue(const std::string& name)
    {
        if (auto* desc = FindArg(name))
        {
            ASSERT(std::holds_alternative<T>(desc->arg));
            return &std::get<T>(desc->arg);
        }
        return nullptr;
    }

    template<typename T>
    const T* GetArgValue(const std::string& name) const
    {
        if (const auto* desc = FindArg(name))
        {
            ASSERT(std::holds_alternative<T>(desc->arg));
            return &std::get<T>(desc->arg);
        }
        return nullptr;
    }

    void ApplyState(audio::GraphClass& klass) const
    {
        audio::GraphClass::Element element;
        element.id   = mId;
        element.name = mName;
        element.type = mType;
        for (const auto& arg : mArgs)
        {
            element.args[arg.name] = arg.arg;
        }
        if (mType == "Mixer" || mType == "Playlist")
        {
            std::vector<audio::PortDesc> ports;
            for (const auto& port : mIPorts)
            {
                audio::PortDesc desc;
                desc.name = port.name;
                ports.push_back(std::move(desc));
            }
            element.input_ports = std::move(ports);
        }
        if (mType == "Splitter")
        {
            std::vector<audio::PortDesc> ports;
            for (const auto& port : mOPorts)
            {
                audio::PortDesc desc;
                desc.name = port.name;
                ports.push_back(std::move(desc));
            }
            element.output_ports = std::move(ports);
        }

        klass.AddElement(std::move(element));
    }

    bool Validate() const
    {
        for (const auto& arg : mArgs)
        {
            if (arg.name == "file")
            {
                if (const auto* ptr = GetArgValue<std::string>("file"))
                    if (ptr->empty()) return SetValid("Invalid source file (none).", false);
            }
            else if (arg.name == "format")
            {
                if (const auto* ptr = GetArgValue<audio::Format>("format"))
                {
                    if (ptr->channel_count == 0) return SetValid("Invalid channel count.", false);
                    else if (ptr->sample_rate == 0) return SetValid("Invalid sample rate.", false);
                }
            }
            else if (arg.name == "sample_rate")
            {
                if (const auto* ptr = GetArgValue<unsigned>("sample_rate"))
                    if (*ptr == 0) return SetValid("Invalid sample rate.", false);
            }
        }
        return SetValid("", true);
    }
    bool SetValid(const QString& msg, bool valid) const
    {
        mMessage = msg;
        mIsValid = valid;
        return valid;
    }

    virtual QRectF boundingRect() const override
    {
        qreal penWidth = 1;
        return QRectF(0.0f, 0.0f, mWidth, mHeight);
    }

    const PortDesc* FindOutputPort(const std::string& name) const
    {
        return base::SafeFind(mOPorts, [&name](const auto& p) {
            return p.name == name;
        });
    }
    const PortDesc* MapOutputPort(const QPointF& pos) const
    {
        for (const auto& p : mOPorts)
        {
            if (p.rect.contains(pos))
                return &p;
        }
        return nullptr;
    }
    const PortDesc* FindInputPort(const std::string& name) const
    {
        return base::SafeFind(mIPorts, [&name](const auto& p) {
            return p.name == name;
        });
    }
    const PortDesc* MapInputPort(const QPointF& pos) const
    {
        for (const auto& p : mIPorts)
        {
            if (p.rect.contains(pos))
                return &p;
        }
        return nullptr;
    }
    void AddInputPort()
    {
        const auto count = mIPorts.size();
        const auto name  = base::FormatString("in%1", count);
        mIPorts.push_back({name});
        ComputePorts();
    }
    void AddOutputPort()
    {
        const auto count = mOPorts.size();
        const auto name  = base::FormatString("out%1", count);
        mOPorts.push_back({name});
        ComputePorts();
    }
    std::string RemoveInputPort()
    {
        auto name = mIPorts.back().name;
        mIPorts.pop_back();
        ComputePorts();
        return name;
    }
    std::string RemoveOutputPort()
    {
        auto name = mOPorts.back().name;
        mOPorts.pop_back();
        ComputePorts();
        return name;
    }
    bool CanAddInputPort() const
    {
        if (mType == "Mixer" || mType == "Playlist")
            return true;
        return false;
    }
    bool CanRemoveInputPort() const
    {
        if ((mType == "Mixer" || mType == "Playlist") && mIPorts.size() > 2)
            return true;
        return false;
    }
    bool CanAddOutputPort() const
    {
        if (mType == "Splitter")
            return true;
        return false;
    }
    bool CanRemoveOutputPort() const
    {
        if ((mType == "Splitter") && mOPorts.size() > 2)
            return true;
        return false;
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        const auto& palette = option->palette;
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);

        QRectF rc(0.0f, 0.0f, mWidth, mHeight);

        QPainterPath path;
        path.addRoundedRect(rc, 10, 10);

        if (isSelected())
        {
            QPen pen;
            pen.setColor(palette.color(QPalette::HighlightedText));
            painter->setPen(pen);
            painter->fillPath(path, palette.color(QPalette::Highlight));
            painter->drawPath(path);
        }
        else
        {
            QPen pen;
            pen.setColor(palette.color(QPalette::Text));
            painter->setPen(pen);
            painter->fillPath(path, palette.color(QPalette::Base));
            painter->drawPath(path);
        }

        auto big_font = painter->font();
        big_font.setPixelSize(20);

        painter->drawText(rc, Qt::AlignVCenter | Qt::AlignHCenter,
        QString("<%1>\n\n%2")
                .arg(app::FromUtf8(mType))
                .arg(app::FromUtf8(mName)));

        for (unsigned i=0; i<mIPorts.size(); ++i)
        {
            const auto& port = mIPorts[i];
            QPainterPath p;
            p.addRoundedRect(port.rect, 5, 5);
            painter->fillPath(p, QColor(0, 128, 0));
            painter->drawText(port.rect, Qt::AlignVCenter | Qt::AlignHCenter, app::FromUtf8(port.name));
        }
        for (unsigned i=0; i<mOPorts.size(); ++i)
        {
            const auto& port = mOPorts[i];
            QPainterPath p;
            p.addRoundedRect(port.rect, 5, 5);
            painter->fillPath(p, QColor(0, 128, 0));
            painter->drawText(port.rect, Qt::AlignVCenter | Qt::AlignHCenter, app::FromUtf8(port.name));
        }
        //if (mIsValid) return;
        QPen failure;
        failure.setColor(QColor(200, 0, 0));
        painter->setPen(failure);
        painter->setFont(big_font);
        painter->drawText(0, mHeight + 25, mMessage);
    }

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
        // this is the wrong place to do things, try to fix this API mess
        // and dispatch the call to the right place, i.e the scene.
        auto* ugly = dynamic_cast<gui::AudioGraphScene*>(scene());
        if (ugly == nullptr) return value;
        ugly->NotifyItemChange(change, this);

        return QGraphicsItem::itemChange(change, value);
    }

    void LoadState(const app::Resource& resource)
    {
        float x = 0.0f, y=0.0f;
        resource.GetProperty(app::FromUtf8("elem_" + mId + "_pos_x"), &x);
        resource.GetProperty(app::FromUtf8("elem_" + mId + "_pos_y"), &y);
        setPos(QPointF(x, y));
    }

    void SaveState(app::Resource& resource) const
    {
        const auto& p = this->pos();
        resource.SetProperty(app::FromUtf8("elem_" + mId + "_pos_x"), p.x());
        resource.SetProperty(app::FromUtf8("elem_" + mId + "_pos_y"), p.y());
    }

    void IntoJson(data::Writer& writer) const
    {
        writer.Write("id",   mId);
        writer.Write("type", mType);
        writer.Write("name", mName);
        writer.Write("position", gui::ToVec2(this->pos()));
        for (const auto& arg : mArgs)
        {
            const auto& name    = "arg_" + arg.name;
            const auto& variant = arg.arg;
            std::visit([&writer, &name](const auto& variant_value) {
                writer.Write(name.c_str(), variant_value);
            }, variant);
        }
        if (mType == "Mixer" || mType == "Playlist")
            writer.Write("iports", (unsigned)mIPorts.size());
        if (mType == "Splitter")
            writer.Write("oports", (unsigned)mOPorts.size());
    }
    void FromJson(const data::Reader& reader)
    {
        glm::vec2 position;
        reader.Read("id",   &mId);
        reader.Read("type", &mType);
        reader.Read("name", &mName);
        reader.Read("position", &position);
        for (auto& arg : mArgs)
        {
            const auto& name = "arg_" + arg.name;
            auto& variant = arg.arg;
            std::visit([&reader, &name](auto& variant_value) {
                reader.Read(name.c_str(), &variant_value);
            }, variant);
        }
        this->setPos(QPointF(position.x, position.y));

        unsigned ports = 0;
        if (reader.Read("iports", &ports))
        {
            mIPorts.clear();
            for (unsigned i=0; i<ports; ++i)
                AddInputPort();
        }
        if (reader.Read("oports", &ports))
        {
            mOPorts.clear();
            for (unsigned i=0; i<ports; ++i)
                AddOutputPort();
        }
    }
    void ComputePorts()
    {
        mHeight = std::max(100.0f, std::max(mIPorts.size(), mOPorts.size()) * 40.0f);
        const auto otop = (mHeight - mOPorts.size() * 30.0f) * 0.5f;
        const auto itop = (mHeight - mIPorts.size() * 30.0f) * 0.5f;
        for (unsigned i=0; i<mIPorts.size(); ++i)
        {
            const auto top = itop + 30.0f * i + 5;
            mIPorts[i].rect = QRectF(0.0f, top, 40.0f, 20.0f);
            mIPorts[i].link_pos = QPointF(0.0f, top + 10.0f);
        }

        for (unsigned i=0; i<mOPorts.size(); ++i)
        {
            const auto top = otop + 30.0f * i + 5;
            mOPorts[i].rect = QRectF(mWidth - 40.0f, top, 40.0f, 20.0f);
            mOPorts[i].link_pos = QPointF(mWidth, top + 10.0f);
        }
    }
protected:
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* mickey) override
    {
        if (CanAddInputPort())
            AddInputPort();
        else if (CanAddOutputPort())
            AddOutputPort();

        // stupid... no other way??
        scene()->invalidate();
    }
private:
    ArgDesc* FindArg(const std::string& name)
    {
        for (auto& arg : mArgs)
            if (arg.name == name) return &arg;
        return nullptr;
    }
    const ArgDesc* FindArg(const std::string& name) const
    {
        for (auto& arg : mArgs)
            if (arg.name == name) return &arg;
        return nullptr;
    }
private:
    float mWidth  = 200.0f;
    float mHeight = 100.0f;
    std::string mId;
    std::string mType;
    std::string mName;
    std::vector<PortDesc> mIPorts;
    std::vector<PortDesc> mOPorts;
    std::vector<ArgDesc> mArgs;
    mutable QString mMessage;
    mutable bool mIsValid = true;
};
} // namespace

namespace gui
{

void AudioGraphScene::NotifyItemChange(QGraphicsItem::GraphicsItemChange change, QGraphicsItem* item)
{
    ChangeEvent event;
    event.change = change;
    event.item   = item;
    //mChanges.push(event);
    ApplyChange(event);
}

void AudioGraphScene::ApplyItemChanges()
{
    while (!mChanges.empty())
    {
        ChangeEvent event = mChanges.front();
        ApplyChange(event);
        mChanges.pop();
    }
}

void AudioGraphScene::DeleteItems(const QList<QGraphicsItem*>& items)
{
    UnlinkItems(items);

    qDeleteAll(items);
}
void AudioGraphScene::LinkItems(const std::string& src_elem, const std::string& src_port,
                                const std::string& dst_elem, const std::string& dst_port)
{
    auto* src = dynamic_cast<AudioElement*>(FindItem(src_elem));
    auto* dst = dynamic_cast<AudioElement*>(FindItem(dst_elem));
    ASSERT(src && dst);
    const auto* src_p = src->FindOutputPort(src_port);
    const auto* dst_p = dst->FindInputPort(dst_port);
    auto link = new AudioLink();
    link->SetSrc(src_elem, src_port);
    link->SetDst(dst_elem, dst_port);
    link->SetCurve(src->mapToScene(src_p->link_pos), dst->mapToScene(dst_p->link_pos));
    addItem(link);
    mLinkMap[base::FormatString("%1:%2", src_elem, src_port)] = link;
    mLinkMap[base::FormatString("%1:%2", dst_elem, dst_port)] = link;
}

void AudioGraphScene::UnlinkItems(const QList<QGraphicsItem*>& items)
{
    std::unordered_set<AudioLink*> dead_links;
    for (const auto* item : items)
    {
        if (const auto* elem = dynamic_cast<const AudioElement*>(item))
        {
            for (unsigned i=0; i<elem->GetNumOutputPorts(); ++i)
            {
                const auto& port = elem->GetOutputPort(i);
                const auto& key  = base::FormatString("%1:%2", elem->GetId(), port.name);
                auto it = mLinkMap.find(key);
                if (it == mLinkMap.end()) continue;
                auto* link = dynamic_cast<AudioLink*>(it->second);
                dead_links.insert(link);
            }
            for (unsigned i=0; i<elem->GetNumInputPorts(); ++i)
            {
                const auto& port = elem->GetInputPort(i);
                const auto& key  = base::FormatString("%1:%2", elem->GetId(), port.name);
                auto it = mLinkMap.find(key);
                if (it == mLinkMap.end()) continue;
                auto* link = dynamic_cast<AudioLink*>(it->second);
                dead_links.insert(link);
            }
        }
    }

    for (auto it = mLinkMap.begin(); it != mLinkMap.end();)
    {
        auto* link = dynamic_cast<AudioLink*>(it->second);
        auto dl = dead_links.find(link);
        if (dl == dead_links.end()) {
            ++it;
        } else it = mLinkMap.erase(it);
    }
    qDeleteAll(dead_links);
}

void AudioGraphScene::UnlinkPort(const std::string& element, const std::string& port)
{
    const auto& key = base::FormatString("%1:%2", element, port);
    auto it = mLinkMap.find(key);
    if (it == mLinkMap.end()) return;

    auto* dead_link = it->second;
    for (auto it = mLinkMap.begin(); it != mLinkMap.end();)
    {
        auto* link = dynamic_cast<AudioLink*>(it->second);
        if (link == dead_link)
            it = mLinkMap.erase(it);
        else ++it;
    }
    delete dead_link;
}

void AudioGraphScene::IntoJson(data::Writer& writer) const
{
    const auto& items = this->items();
    for (const auto* item : items)
    {
        auto chunk = writer.NewWriteChunk();
        if (const auto* ptr = dynamic_cast<const AudioElement*>(item))
        {
            ptr->IntoJson(*chunk);
            writer.AppendChunk("element", std::move(chunk));
        }
        else if (const auto* ptr = dynamic_cast<const AudioLink*>(item))
        {
            ptr->IntoJson(*chunk);
            writer.AppendChunk("link", std::move(chunk));
        } else BUG("???");
    }
    for (const auto& p : mLinkMap)
    {
        const auto* link = dynamic_cast<const AudioLink*>(p.second);
        auto chunk = writer.NewWriteChunk();
        chunk->Write("port", p.first);
        chunk->Write("link", link->GetLinkId());
        writer.AppendChunk("mapping", std::move(chunk));
    }
}

bool AudioGraphScene::FromJson(const data::Reader& reader)
{
    for (unsigned i=0; i<reader.GetNumChunks("element"); ++i)
    {
        const auto& chunk = reader.GetReadChunk("element", i);
        std::string type;
        chunk->Read("type", &type);
        auto* element = new AudioElement(FindElementDescription(type));
        element->FromJson(*chunk);
        addItem(element);
    }
    std::unordered_map<std::string, AudioLink*> links;
    for (unsigned i=0; i<reader.GetNumChunks("link"); ++i)
    {
        const auto& chunk = reader.GetReadChunk("link", i);
        auto* link = new AudioLink();
        link->FromJson(*chunk);
        links[link->GetLinkId()] = link;
        addItem(link);
    }
    for (unsigned i=0; i<reader.GetNumChunks("mapping"); ++i)
    {
        const auto& chunk = reader.GetReadChunk("mapping", i);
        std::string port;
        std::string link;
        chunk->Read("port", &port);
        chunk->Read("link", &link);
        mLinkMap[port] = links[link];
    }
    return true;
}

void AudioGraphScene::ApplyChange(const ChangeEvent& event)
{
    if (event.change != QGraphicsItem::ItemPositionChange)
        return;

    if (const auto* elem = dynamic_cast<AudioElement*>(event.item))
    {
        for (unsigned i=0; i<elem->GetNumOutputPorts(); ++i)
        {
            const auto& port = elem->GetOutputPort(i);
            const auto& key  = base::FormatString("%1:%2", elem->GetId(), port.name);
            auto it = mLinkMap.find(key);
            if (it == mLinkMap.end()) continue;
            auto* link = dynamic_cast<AudioLink*>(it->second);

            link->SetCurve(elem->mapToScene(port.link_pos), link->GetDstPoint());
        }
        for (unsigned i=0; i<elem->GetNumInputPorts(); ++i)
        {
            const auto& port = elem->GetInputPort(i);
            const auto& key  = base::FormatString("%1:%2", elem->GetId(), port.name);
            auto it = mLinkMap.find(key);
            if (it == mLinkMap.end()) continue;
            auto* link = dynamic_cast<AudioLink*>(it->second);

            link->SetCurve(link->GetSrcPoint(), elem->mapToScene(port.link_pos));
        }
    }
}

void AudioGraphScene::SaveState(app::Resource& resource) const
{
    const auto& items = this->items();
    for (const auto* item : items)
    {
        if (const auto* elem = dynamic_cast<const AudioElement*>(item))
            elem->SaveState(resource);
        else if (const auto* link = dynamic_cast<const AudioLink*>(item))
            link->SaveState(resource);
        else BUG("???");
    }

    resource.SetProperty("mapping_count", (unsigned)mLinkMap.size());
    unsigned link_counter = 0;
    for (const auto& p : mLinkMap)
    {
        const auto* link = dynamic_cast<const AudioLink*>(p.second);
        resource.SetProperty(QString("mapping_%1_port").arg(link_counter), app::FromUtf8(p.first));
        resource.SetProperty(QString("mapping_%1_link").arg(link_counter), app::FromUtf8(link->GetLinkId()));
        ++link_counter;
    }
}
void AudioGraphScene::LoadState(const app::Resource& resource)
{
    const audio::GraphClass* klass;
    resource.GetContent(&klass);

    for (size_t i=0; i<klass->GetNumElements(); ++i)
    {
        AudioElement* elem = new AudioElement(klass->GetElement(i));
        elem->LoadState(resource);
        addItem(elem);
    }
    std::unordered_map<std::string, AudioLink*> links;
    for (size_t i=0; i<klass->GetNumLinks(); ++i)
    {
        AudioLink* link = new AudioLink(klass->GetLink(i));
        links[link->GetLinkId()] = link;
        link->LoadState(resource);
        addItem(link);
    }
    unsigned link_counter = 0;
    resource.GetProperty("mapping_count", &link_counter);
    for (unsigned i=0; i<link_counter; ++i)
    {
        QString port;
        QString link;
        resource.GetProperty(QString("mapping_%1_port").arg(i), &port);
        resource.GetProperty(QString("mapping_%1_link").arg(i), &link);
        mLinkMap[app::ToUtf8(port)] = links[app::ToUtf8(link)];
    }
}

void AudioGraphScene::ApplyState(audio::GraphClass& klass) const
{
    const auto& items = this->items();
    for (const auto* item :items)
    {
        if (const auto* elem = dynamic_cast<const AudioElement*>(item))
            elem->ApplyState(klass);
        else if (const auto* link = dynamic_cast<const AudioLink*>(item))
            link->ApplyState(klass);
    }
}

bool AudioGraphScene::ValidateGraphContent() const
{
    bool valid = true;
    const auto& items = this->items();
    for (const auto* item : items)
    {
        if (const auto* elem = dynamic_cast<const AudioElement*>(item))
            valid = valid & elem->Validate();
        else if (const auto* link = dynamic_cast<const AudioLink*>(item))
            valid = valid & link->Validate();
    }
    return valid;
}

QGraphicsItem* AudioGraphScene::FindItem(const std::string& id)
{
    for (auto* item : items())
    {
        if (auto* elem = dynamic_cast<AudioElement*>(item))
            if (elem->GetId() == id) return elem;
        else if (auto* link = dynamic_cast<AudioLink*>(item))
            if (link->GetLinkId() == id) return link;
    }
    return nullptr;
}

void AudioGraphScene::mousePressEvent(QGraphicsSceneMouseEvent* mickey)
{
    if (mickey->button() != Qt::LeftButton)
        return;

    const auto pos = mickey->scenePos();
    auto* item = itemAt(pos, QTransform());
    if (item == nullptr)
        return QGraphicsScene::mousePressEvent(mickey);
    const auto* elem = dynamic_cast<AudioElement*>(item);
    if (elem == nullptr)
        return QGraphicsScene::mousePressEvent(mickey);
    const auto item_pos = elem->mapFromScene(pos);
    const auto* port = elem->MapOutputPort(item_pos);
    if (port == nullptr)
        return QGraphicsScene::mousePressEvent(mickey);
    const auto link_pos = elem->mapToScene(port->link_pos);

    mSrcElem = elem->GetId();
    mSrcPort = port->name;

    QPen pen;
    pen.setColor(QColor(0, 128, 0));
    pen.setWidth(10);

    auto* link = new AudioLink();
    link->SetCurve(link_pos, link_pos);
    addItem(link);
    mLine.reset(link);
    return QGraphicsScene::mousePressEvent(mickey);
}
void AudioGraphScene::mouseMoveEvent(QGraphicsSceneMouseEvent* mickey)
{
    if (mLine == nullptr)
        return QGraphicsScene::mouseMoveEvent(mickey);

    auto* link = dynamic_cast<AudioLink*>(mLine.get());
    link->SetCurve(link->GetSrcPoint(), mickey->scenePos());
}
void AudioGraphScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* mickey)
{
    if (mLine == nullptr)
        return QGraphicsScene::mouseReleaseEvent(mickey);

    auto carcass = std::move(mLine);

    const auto pos = mickey->scenePos();
    auto* item = itemAt(pos, QTransform());
    if (item == nullptr)
        return QGraphicsScene::mouseReleaseEvent(mickey);
    const auto* elem = dynamic_cast<AudioElement*>(item);
    if (elem== nullptr)
        return QGraphicsScene::mouseReleaseEvent(mickey);
    const auto item_pos = elem->mapFromScene(pos);
    const auto* port = elem->MapInputPort(item_pos);
    if (port == nullptr)
        return QGraphicsScene::mouseReleaseEvent(mickey);
    const auto link_pos = elem->mapToScene(port->link_pos);

    auto* link = dynamic_cast<AudioLink*>(carcass.get());
    link->SetSrc(mSrcElem, mSrcPort);
    link->SetDst(elem->GetId(), port->name);
    link->SetCurve(link->GetSrcPoint(), link_pos);

    UnlinkPort(mSrcElem, mSrcPort);
    UnlinkPort(elem->GetId(), port->name);

    mLinkMap[base::FormatString("%1:%2", mSrcElem, mSrcPort)] = link;
    mLinkMap[base::FormatString("%1:%2", elem->GetId(), port->name)] = link;

    carcass.release();
    QGraphicsScene::mouseReleaseEvent(mickey);
}

AudioWidget::AudioWidget(app::Workspace* workspace)
  : mWorkspace(workspace)
{
    DEBUG("Create AudioWidget");
    mUI.setupUi(this);
    mScene.reset(new AudioGraphScene);

    connect(mScene.get(), &AudioGraphScene::selectionChanged, this, &AudioWidget::SceneSelectionChanged);
    mUI.view->setScene(mScene.get());
    mUI.view->setInteractive(true);
    mUI.view->setBackgroundBrush(QBrush(QColor(0x23, 0x23, 0x23, 0xff)));

    // start periodic refresh timer. this is low frequency timer that is used
    // to update the widget UI if needed, such as change the icon/window title
    // and tick the workspace for periodic cleanup and stuff.
    QObject::connect(&mRefreshTimer, &QTimer::timeout, this, &AudioWidget::RefreshTimer);
    mRefreshTimer.setInterval(10);

    PopulateFromEnum<audio::SampleType>(mUI.sampleType);
    PopulateFromEnum<audio::Channels>(mUI.channels);
    PopulateFromEnum<audio::Effect::Kind>(mUI.effect);
    PopulateFromEnum<audio::IOStrategy>(mUI.ioStrategy);
    SetValue(mUI.graphName, QString("My Graph"));
    SetValue(mUI.graphID, base::RandomString(10));
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionStop, false);
    mUI.afDuration->SetEditable(false);
    GetSelectedElementProperties();
    mGraphHash = GetHash();

    setWindowTitle("My Graph");
}
AudioWidget::AudioWidget(app::Workspace* workspace, const app::Resource& resource)
  : AudioWidget(workspace)
{
    DEBUG("Editing audio graph: '%1'.", resource.GetName());
    SetValue(mUI.graphName, resource.GetName());
    SetValue(mUI.graphID,   resource.GetId());
    mScene->LoadState(resource);
    mScene->invalidate();

    // initialize our random access cache.
    auto items = mScene->items();
    for (auto* item : items) {
        if (auto* ptr = dynamic_cast<AudioElement*>(item))
            mItems.push_back(ptr);
    }

    const audio::GraphClass* klass = nullptr;
    resource.GetContent(&klass);

    UpdateElementList();
    SetValue(mUI.outElem, ListItemId(klass->GetGraphOutputElementId()));
    on_outElem_currentIndexChanged(0);
    SetValue(mUI.outPort, ListItemId(klass->GetGraphOutputElementPort()));
    GetSelectedElementProperties();

    mGraphHash = GetHash();
}
AudioWidget::~AudioWidget()
{
    DEBUG("Destroy AudioWidget");

    if (mCurrentId)
        mPlayer->Cancel(mCurrentId);

    // BRAIN DAMAGED SIGNALLING!
    QSignalBlocker hell(mScene.get());

    qDeleteAll(mUI.view->items());

    ClearList(mUI.elements);
}

QString AudioWidget::GetId() const
{
    return GetValue(mUI.graphID);
}

bool AudioWidget::IsAccelerated() const
{ return false; }

bool AudioWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    const auto& selection = mScene->selectedItems();

    switch (action)
    {
        case Actions::CanPaste:
            if (!mUI.view->hasFocus())
                return false;
            else if (clipboard->IsEmpty())
                return false;
            else if (clipboard->GetType() != "application/json/audio-element")
                return false;
            return true;
        case Actions::CanCopy:
        case Actions::CanCut:
            if (!mUI.view->hasFocus())
                return false;
            if (selection.isEmpty())
                return false;
            return true;
    }
    return false;
}
void AudioWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
}
void AudioWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
}
void AudioWidget::Save()
{
    on_actionSave_triggered();
}

void AudioWidget::Refresh()
{
    audio::Player::Event event;
    while (mPlayer && mPlayer->GetEvent(&event))
    {
        if (const auto* ptr = std::get_if<audio::Player::SourceCompleteEvent>(&event))
            OnAudioPlayerEvent(*ptr);
        else if (const auto* ptr = std::get_if<audio::Player::SourceEvent>(&event))
            OnAudioPlayerEvent(*ptr);
        else if (const auto* ptr = std::get_if<audio::Player::SourceProgressEvent>(&event))
            OnAudioPlayerEvent(*ptr);
        else BUG("Unexpected audio player event.");
    }
}

bool AudioWidget::SaveState(Settings& settings) const
{
    data::JsonObject json;
    mScene->IntoJson(json);
    settings.SetValue("Audio", "content", json);
    settings.SetValue("Audio", "hash", mGraphHash);
    settings.SetValue("Audio", "graph_out_elem", (QString)GetItemId(mUI.outElem));
    settings.SetValue("Audio", "graph_out_port", (QString)GetItemId(mUI.outPort));
    settings.SaveWidget("Audio", mUI.graphName);
    settings.SaveWidget("Audio", mUI.graphID);
    return true;
}
bool AudioWidget::LoadState(const Settings& settings)
{
    QString graph_out_elem;
    QString graph_out_port;
    data::JsonObject json;
    settings.GetValue("Audio", "content", &json);
    settings.GetValue("Audio", "hash", &mGraphHash);
    settings.GetValue("Audio", "graph_out_elem", &graph_out_elem);
    settings.GetValue("Audio", "graph_out_port", &graph_out_port);
    settings.LoadWidget("Audio", mUI.graphName);
    settings.LoadWidget("Audio", mUI.graphID);

    mScene->FromJson(json);

    // initialize our random access cache.
    auto items = mScene->items();
    for (auto* item : items) {
        if (auto* ptr = dynamic_cast<AudioElement*>(item))
            mItems.push_back(ptr);
    }
    UpdateElementList();

    SetValue(mUI.outElem, ListItemId(graph_out_elem));
    on_outElem_currentIndexChanged(0);
    SetValue(mUI.outPort, ListItemId(graph_out_port));

    GetSelectedElementProperties();
    return true;
}

void AudioWidget::Cut(Clipboard& clipboard)
{
    const auto& selection = mScene->selectedItems();
    if (selection.isEmpty())
        return;

    Copy(clipboard);

    on_actionDelete_triggered();
}
void AudioWidget::Copy(Clipboard& clipboard) const
{
    const auto& mouse_pos = mUI.view->mapFromGlobal(QCursor::pos());
    const auto& scene_pos = mUI.view->mapToScene(mouse_pos);
    const auto& selection = mScene->selectedItems();
    if (selection.isEmpty())
        return;

    std::unordered_set<std::string> copied_element_ids;

    data::JsonObject json;
    for(const auto& item : selection)
    {
        if (const auto* elem = dynamic_cast<const AudioElement*>(item))
        {
            const auto& pos = elem->pos();
            const auto& mouse_offset = pos - scene_pos;
            auto chunk = json.NewWriteChunk();
            elem->IntoJson(*chunk);
            chunk->Write("mouse_offset", gui::ToVec2(mouse_offset));
            json.AppendChunk("elements", std::move(chunk));
            copied_element_ids.insert(elem->GetId());
        }
    }
    for (const auto& item : mScene->items())
    {
        if (const auto* link = dynamic_cast<const AudioLink*>(item))
        {
            const auto copied_src_elem = base::Contains(copied_element_ids, link->GetSrcElem());
            const auto copied_dst_elem = base::Contains(copied_element_ids, link->GetDstElem());
            if (!copied_src_elem || !copied_dst_elem)
                continue;
            auto chunk = json.NewWriteChunk();
            link->IntoJson(*chunk);
            json.AppendChunk("links", std::move(chunk));
        }
    }
    clipboard.Clear();
    clipboard.SetType("application/json/audio-element");
    clipboard.SetText(json.ToString());
    NOTE("Copied JSON to application clipboard.");
}
void AudioWidget::Paste(const Clipboard& clipboard)
{
    if (!mUI.view->hasFocus())
        return;
    if (clipboard.GetType() != "application/json/audio-element")
    {
        NOTE("No audio element JSON data found in clipboard.");
        return;
    }
    data::JsonObject json;
    auto [success, _] = json.ParseString(clipboard.GetText());
    if (!success)
    {
        NOTE("Clipboard parse failed.");
        return;
    }

    std::unordered_map<std::string, std::string> idmap;
    const auto& mouse_pos = mUI.view->mapFromGlobal(QCursor::pos());
    const auto& scene_pos = mUI.view->mapToScene(mouse_pos);
    for (unsigned i=0; i<json.GetNumChunks("elements"); ++i)
    {
        const auto& id = base::RandomString(10);
        const auto& chunk = json.GetReadChunk("elements", i);
        std::string type;
        glm::vec2 mouse_offset;
        chunk->Read("type", &type);
        chunk->Read("mouse_offset", &mouse_offset);
        auto* element = new AudioElement(FindElementDescription(type));
        element->FromJson(*chunk);
        idmap[element->GetId()] = id;
        element->setPos(mouse_offset.x + scene_pos.x(),
                        mouse_offset.y + scene_pos.y());
        element->SetId(id);
        element->SetName(base::FormatString("Copy of %1", element->GetName()));
        element->ComputePorts();
        mScene->addItem(element);
        mItems.push_back(element);
    }

    for (unsigned i=0; i<json.GetNumChunks("links"); ++i)
    {
        const auto& chunk = json.GetReadChunk("links", i);
        auto link = std::make_unique<AudioLink>();
        link->FromJson(*chunk);
        const auto& original_src_id = link->GetSrcElem();
        const auto& original_dst_id = link->GetDstElem();
        mScene->LinkItems(idmap[original_src_id], link->GetSrcPort(),
                          idmap[original_dst_id], link->GetDstPort());
    }

    mScene->invalidate();

    UpdateElementList();
}

bool AudioWidget::HasUnsavedChanges() const
{
    if (!mGraphHash)
        return false;
    return GetHash() != mGraphHash;
}

bool AudioWidget::GetStats(Stats* stats) const
{
    stats->time = mPlayTime;
    stats->graphics.valid = false;
    return true;
}

void AudioWidget::on_btnSelectFile_clicked()
{
    const auto& file = QFileDialog::getOpenFileName((QWidget*)this,
        tr("Select Audio File"), "",
        tr("Audio (*.mp3 *.ogg *.wav *.flac)"));
    if (file.isEmpty()) return;

    const QFileInfo info(file);
    const auto& uri = mWorkspace->MapFileToWorkspace(info.absoluteFilePath());
    SetValue(mUI.fileSource, uri);
    SetSelectedElementProperties();
    GetSelectedElementProperties();
}

void AudioWidget::on_btnEditFile_clicked()
{
    emit OpenExternalAudio(GetValue(mUI.fileSource));
}

void AudioWidget::on_actionPlay_triggered()
{
    if (!InitializeAudio())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to connect to the platform audio device.\n"
                       "Please see the log for more details."));
        msg.exec();
        return;
    }

    if (mCurrentId == 0)
    {
        // schedule a view update.
        mScene->invalidate();
        if (!mScene->ValidateGraphContent())
            return;

        const std::string& src_elem = GetItemId(mUI.outElem);
        const std::string& src_port = GetItemId(mUI.outPort);
        if (src_elem.empty() || src_port.empty())
        {
            QMessageBox msg(this);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setIcon(QMessageBox::Information);
            msg.setText(tr("You haven't selected any element for the final graph output.\n"
                           "You can select an element and a port in 'Graph output'."));
            msg.exec();
            return;
        }

        audio::GraphClass klass(GetValue(mUI.graphName),GetValue(mUI.graphID));
        klass.SetGraphOutputElementId(src_elem);
        klass.SetGraphOutputElementPort(src_port);
        mScene->ApplyState(klass);

        audio::Graph graph(std::move(klass));
        auto source = std::make_unique<audio::AudioGraph>(GetValue(mUI.graphName), std::move(graph));
        audio::AudioGraph::PrepareParams params;
        params.enable_pcm_caching = false;

        if (!source->Prepare(*mWorkspace, params))
        {
            QMessageBox msg(this);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setIcon(QMessageBox::Critical);
            msg.setText(tr("Failed to prepare the audio graph.\n"
                           "Please see the application log for more details."));
            msg.exec();
            return;
        }
        const auto& desc_strs = (*source)->Describe();
        for (const auto& str : desc_strs)
            DEBUG(str);

        const auto& port = (*source)->GetOutputPort(0);
        NOTE("Graph output %1", port.GetFormat());
        mCurrentId = mPlayer->Play(std::move(source));
    }
    else
    {
        mPlayer->Resume(mCurrentId);
    }
    mRefreshTimer.start();
    SetEnabled(mUI.actionPlay, false);
    SetEnabled(mUI.actionPause, true);
    SetEnabled(mUI.actionStop, true);

}
void AudioWidget::on_actionPause_triggered()
{
    ASSERT(mCurrentId);
    mPlayer->Pause(mCurrentId);
    SetEnabled(mUI.actionPlay, true);
    SetEnabled(mUI.actionPause, false);
    mRefreshTimer.stop();

}
void AudioWidget::on_actionStop_triggered()
{
    ASSERT(mCurrentId);
    mPlayer->Cancel(mCurrentId);
    mCurrentId = 0;
    mPlayTime  = 0;
    SetEnabled(mUI.actionPlay, true);
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionStop, false);
    mRefreshTimer.stop();
}
void AudioWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.graphName))
        return;

    audio::GraphClass klass(GetValue(mUI.graphName),GetValue(mUI.graphID));
    klass.SetGraphOutputElementId(GetItemId(mUI.outElem));
    klass.SetGraphOutputElementPort(GetItemId(mUI.outPort));
    mScene->ApplyState(klass);
    const auto hash = klass.GetHash();

    QStringList errors;

    mScene->invalidate();
    if  (!mScene->ValidateGraphContent())
    {
        errors << "* The audio graph has invalid elements.";
    }

    const std::string& src_elem = GetItemId(mUI.outElem);
    const std::string& src_port = GetItemId(mUI.outPort);
    if (src_elem.empty() || src_port.empty())
    {
        errors << "* The audio graph has no output element/port selected.";
    }

    audio::Graph graph(std::move(klass));
    audio::Graph::PrepareParams p;
    p.enable_pcm_caching = false;
    if (!graph.Prepare(*mWorkspace, p))
    {
        errors << "* The audio graph failed to prepare.\n";
    }
    else
    {
        const auto& settings = mWorkspace->GetProjectSettings();
        audio::Format format;
        format.sample_rate = settings.audio_sample_rate;
        format.sample_type = settings.audio_sample_type;
        format.channel_count = static_cast<int>(settings.audio_channels);
        const auto& port = graph.GetOutputPort(0);
        if (port.GetFormat() != format)
        {
            errors << app::toString("* The audio graph output format %1 is not compatible with current audio settings %2.\n", port.GetFormat(), format);
        }
    }

    if (!errors.isEmpty())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Warning);
        msg.setText(tr("The following problems were detected\n\n%1\n"
                       "Are you sure you want to continue?").arg(errors.join("\n")));
        if (msg.exec() == QMessageBox::No)
            return;
    }

    app::AudioResource resource(std::move(klass), GetValue(mUI.graphName));
    mScene->SaveState(resource);

    mWorkspace->SaveResource(resource);
    mGraphHash = hash;
}

void AudioWidget::on_actionDelete_triggered()
{
    QSignalBlocker hell(mScene.get());

    auto selected = mScene->selectedItems();
    for (auto* carcass : selected)
    {
        const std::string& id = GetItemId(mUI.outElem);
        if (dynamic_cast<AudioElement*>(carcass)->GetId() == id)
        {
            SetValue(mUI.outElem, -1);
            SetValue(mUI.outPort, -1);
        }
        auto it = std::find(mItems.begin(), mItems.end(), carcass);
        ASSERT(it != mItems.end());
        mItems.erase(it);
    }
    mScene->DeleteItems(selected);

    UpdateElementList();
    GetSelectedElementProperties();
}

void AudioWidget::on_actionUnlink_triggered()
{
    auto selected = mScene->selectedItems();

    mScene->UnlinkItems(selected);
}

void AudioWidget::on_actionAddInputPort_triggered()
{
    auto selected = mScene->selectedItems();
    if (selected.isEmpty())
        return;
    for (auto* item : selected)
    {
        if (auto* element = dynamic_cast<AudioElement*>(item))
        {
            if (element->CanAddInputPort())
                element->AddInputPort();
        }
    }
    mScene->invalidate();
}

void AudioWidget::on_actionRemoveInputPort_triggered()
{
    auto selected = mScene->selectedItems();
    for (auto* item : selected)
    {
        if (auto* element = dynamic_cast<AudioElement*>(item))
            mScene->UnlinkPort(element->GetId(), element->RemoveInputPort());
    }
    mScene->invalidate();
}

void AudioWidget::on_actionAddOutputPort_triggered()
{
    auto selected = mScene->selectedItems();
    if (selected.isEmpty())
        return;
    for (auto* item : selected)
    {
        if (auto* element = dynamic_cast<AudioElement*>(item))
        {
            if (element->CanAddOutputPort())
                element->AddOutputPort();
        }
    }
    mScene->invalidate();
}
void AudioWidget::on_actionRemoveOutputPort_triggered()
{
    auto selected = mScene->selectedItems();
    for (auto* item : selected)
    {
        if (auto* element = dynamic_cast<AudioElement*>(item))
            mScene->UnlinkPort(element->GetId(), element->RemoveOutputPort());
    }
    mScene->invalidate();
}

void AudioWidget::on_view_customContextMenuRequested(QPoint pos)
{
    QMenu menu(this);

    const auto& mouse_pos = mUI.view->mapFromGlobal(QCursor::pos());
    const auto& scene_pos = mUI.view->mapToScene(mouse_pos);
    const auto* item = dynamic_cast<AudioElement*>(mScene->itemAt(scene_pos, QTransform()));
    const auto& selected = mScene->selectedItems();
    mUI.actionDelete->setEnabled(!selected.isEmpty() && item != nullptr);
    mUI.actionUnlink->setEnabled(!selected.isEmpty() && item != nullptr);
    mUI.actionAddInputPort->setEnabled(!selected.isEmpty() && item != nullptr && item->CanAddInputPort());
    mUI.actionRemoveInputPort->setEnabled(!selected.isEmpty() && item != nullptr && item->CanRemoveInputPort());
    mUI.actionAddOutputPort->setEnabled(!selected.isEmpty() && item != nullptr && item->CanAddOutputPort());
    mUI.actionRemoveOutputPort->setEnabled(!selected.isEmpty() && item != nullptr && item->CanRemoveOutputPort());
    const auto& map = GetElementMap();
    for (const auto& pair : map)
    {
        auto* action = menu.addAction(QIcon("icons:add.png"),
            QString("New %1").arg(app::FromUtf8(pair.first)));
        action->setData(app::FromUtf8(pair.first));
        connect(action, &QAction::triggered, this, &AudioWidget::AddElementAction);
    }

    menu.addSeparator();
    QMenu input_port_menu("Input Ports");
    input_port_menu.addAction(mUI.actionAddInputPort);
    input_port_menu.addAction(mUI.actionRemoveInputPort);
    QMenu output_port_menu("Output Ports");
    output_port_menu.addAction(mUI.actionAddOutputPort);
    output_port_menu.addAction(mUI.actionRemoveOutputPort);
    menu.addMenu(&input_port_menu);
    menu.addMenu(&output_port_menu);
    menu.addSeparator();
    menu.addAction(mUI.actionUnlink);
    menu.addAction(mUI.actionDelete);
    menu.exec(QCursor::pos());
}

void AudioWidget::on_elements_itemSelectionChanged()
{
    QSignalBlocker hell(mScene.get());

    for (auto* item : mItems)
        item->setSelected(false);

    auto selected = mUI.elements->selectedItems();

    for (auto* selected : selected)
    {
        const auto& id = app::ToUtf8(selected->data(Qt::UserRole).toString());
        auto it = std::find_if(mItems.begin(), mItems.end(), [id](const auto* item) {
            return dynamic_cast<const AudioElement*>(item)->GetId() == id;
        });
        ASSERT(it != mItems.end());
        auto* item = dynamic_cast<AudioElement*>(*it);
        item->setSelected(true);
    }
    mUI.view->update();

    GetSelectedElementProperties();
}

void AudioWidget::on_elements_customContextMenuRequested(QPoint)
{
    QMenu menu(this);
    const auto& selected = mScene->selectedItems();
    mUI.actionDelete->setEnabled(!selected.isEmpty());
    mUI.actionUnlink->setEnabled(!selected.isEmpty());
    mUI.actionAddInputPort->setEnabled(true);
    mUI.actionRemoveInputPort->setEnabled(true);
    mUI.actionAddOutputPort->setEnabled(true);
    mUI.actionRemoveOutputPort->setEnabled(true);
    for (auto* item : selected)
    {
        const auto* elem = dynamic_cast<const AudioElement*>(item);
        if (!elem->CanAddInputPort())
            mUI.actionAddInputPort->setEnabled(false);
        if (!elem->CanRemoveInputPort())
            mUI.actionRemoveInputPort->setEnabled(false);
        if (!elem->CanAddOutputPort())
            mUI.actionAddOutputPort->setEnabled(false);
        if (!elem->CanRemoveOutputPort())
            mUI.actionRemoveOutputPort->setEnabled(false);
    }
    menu.addSeparator();
    QMenu input_port_menu("Input Ports");
    input_port_menu.addAction(mUI.actionAddInputPort);
    input_port_menu.addAction(mUI.actionRemoveInputPort);
    QMenu output_port_menu("Output Ports");
    output_port_menu.addAction(mUI.actionAddOutputPort);
    output_port_menu.addAction(mUI.actionRemoveOutputPort);
    menu.addMenu(&input_port_menu);
    menu.addMenu(&output_port_menu);
    menu.addSeparator();
    menu.addAction(mUI.actionUnlink);
    menu.addAction(mUI.actionDelete);
    menu.exec(QCursor::pos());
}


void AudioWidget::on_outElem_currentIndexChanged(int)
{
    const std::string& id = GetItemId(mUI.outElem);

    auto it = std::find_if(mItems.begin(), mItems.end(), [&id](const auto* item) {
        return dynamic_cast<const AudioElement*>(item)->GetId() == id;
    });
    if(it == mItems.end()) return;

    const auto* item = dynamic_cast<const AudioElement*>(*it);
    std::vector<ResourceListItem> ports;
    for (unsigned i=0; i<item->GetNumOutputPorts();++i)
    {
        const auto& port = item->GetOutputPort(i);
        ResourceListItem  li;
        li.name = app::FromUtf8(port.name);
        li.id   = app::FromUtf8(port.name);
        ports.push_back(li);
    }
    SetList(mUI.outPort, ports);
}

void AudioWidget::on_elemName_textChanged(QString)
{
    SetSelectedElementProperties();
}

void AudioWidget::on_sampleType_currentIndexChanged(int)
{
    SetSelectedElementProperties();
}
void AudioWidget::on_sampleRate_currentIndexChanged(int)
{
    SetSelectedElementProperties();
}
void AudioWidget::on_channels_currentIndexChanged(int)
{
    SetSelectedElementProperties();
}
void AudioWidget::on_ioStrategy_currentIndexChanged(int)
{
    SetSelectedElementProperties();
}
void AudioWidget::on_gainValue_valueChanged(double)
{
    SetSelectedElementProperties();
}

void AudioWidget::on_frequency_valueChanged(int)
{
    SetSelectedElementProperties();
}

void AudioWidget::on_duration_valueChanged(int)
{
    SetSelectedElementProperties();
}
void AudioWidget::on_delay_valueChanged(int)
{
    SetSelectedElementProperties();
}

void AudioWidget::on_startTime_valueChanged(int)
{
    SetSelectedElementProperties();
}

void AudioWidget::on_effect_currentIndexChanged(int)
{
    SetSelectedElementProperties();
}

void AudioWidget::on_loopCount_valueChanged(int)
{
    SetSelectedElementProperties();
}

void AudioWidget::on_pcmCaching_stateChanged(int)
{
    SetSelectedElementProperties();
}
void AudioWidget::on_fileCaching_stateChanged(int)
{
    SetSelectedElementProperties();
}

void AudioWidget::SceneSelectionChanged()
{
    GetSelectedElementProperties();

    UpdateElementList();
}

void AudioWidget::AddElementAction()
{
    QAction* action = qobject_cast<QAction*>(sender());

    const auto& type = app::ToUtf8(action->data().toString());

    auto* element = new AudioElement(FindElementDescription(type));
    std::string name;
    for (unsigned i=0; i<1000; ++i)
    {
        name = i == 0 ? base::FormatString("%1", type)
                      : base::FormatString("%1_%2", type, i);
        auto it = std::find_if(mItems.begin(), mItems.end(), [&name](const auto* item) {
            const auto* elem = dynamic_cast<const AudioElement*>(item);
            if (elem->GetName() == name) return true;
            return false;
        });
        if (it != mItems.end()) continue;
        break;
    }
    // todo: this is off somehow... weird
    const auto& mouse_pos = mUI.view->mapFromGlobal(QCursor::pos());
    const auto& scene_pos = mUI.view->mapToScene(mouse_pos);
    element->setPos(scene_pos);
    element->SetName(name);
    // scene takes ownership of the graphics item
    mScene->addItem(element);
    mItems.push_back(element);
    UpdateElementList();
}

void AudioWidget::RefreshTimer()
{
    if (mCurrentId && mPlayer)
        mPlayer->AskProgress(mCurrentId);

    emit RefreshRequest();
}

bool AudioWidget::InitializeAudio()
{
    try
    {
        const auto& settings = mWorkspace->GetProjectSettings();
        static std::weak_ptr<audio::Player> shared_player;
        static unsigned audio_buffer_size = 0;

        mPlayer = shared_player.lock();
        if (!mPlayer || audio_buffer_size != settings.audio_buffer_size)
        {
            auto device = audio::Device::Create(APP_TITLE);
            device->SetBufferSize(settings.audio_buffer_size);
            mPlayer = std::make_shared<audio::Player>(std::move(device));
            shared_player = mPlayer;
            audio_buffer_size = settings.audio_buffer_size;
            DEBUG("Created new audio player with audio buffer set to %1ms.", audio_buffer_size);
        }
    }
    catch (const std::exception& e)
    {
        ERROR("Failed to create audio device.'%1'", e.what());
        return false;
    }
    return true;
}

size_t AudioWidget::GetHash() const
{
    audio::GraphClass klass(GetValue(mUI.graphName),GetValue(mUI.graphID));
    klass.SetGraphOutputElementId(GetItemId(mUI.outElem));
    klass.SetGraphOutputElementPort(GetItemId(mUI.outPort));
    mScene->ApplyState(klass);
    return klass.GetHash();
}

void AudioWidget::GetSelectedElementProperties()
{
    SetValue(mUI.elemName, QString(""));
    SetValue(mUI.elemID,   QString(""));
    SetValue(mUI.sampleType, audio::SampleType::Float32);
    SetValue(mUI.channels, audio::Channels::Stereo);
    SetValue(mUI.sampleRate,   QString("44100"));
    SetValue(mUI.ioStrategy, audio::FileSource::IOStrategy::Default);
    SetValue(mUI.fileSource, QString(""));
    SetValue(mUI.gainValue,    1.0f);
    SetValue(mUI.frequency, 0);
    SetValue(mUI.duration, 0);
    SetValue(mUI.delay, 0);
    SetValue(mUI.startTime, 0);
    SetValue(mUI.afChannels,   QString(""));
    SetValue(mUI.afSampleRate, QString(""));
    SetValue(mUI.afFrames,     QString(""));
    SetValue(mUI.afSize,       QString(""));
    SetValue(mUI.afDuration,   0);
    SetValue(mUI.loopCount, 1);
    SetValue(mUI.pcmCaching, false);
    SetValue(mUI.fileCaching, false);

    SetEnabled(mUI.sampleType, false);
    SetEnabled(mUI.sampleRate, false);
    SetEnabled(mUI.ioStrategy, false);
    SetEnabled(mUI.channels, false);
    SetEnabled(mUI.fileSource, false);
    SetEnabled(mUI.btnSelectFile, false);
    SetEnabled(mUI.gainValue,  false);
    SetEnabled(mUI.frequency, false);
    SetEnabled(mUI.duration, false);
    SetEnabled(mUI.delay, false);
    SetEnabled(mUI.startTime, false);
    SetEnabled(mUI.effect, false);
    SetEnabled(mUI.audioFile, false);
    SetEnabled(mUI.actionDelete, false);
    SetEnabled(mUI.loopCount, false);
    SetEnabled(mUI.pcmCaching, false);
    SetEnabled(mUI.fileCaching, false);

    /*
    SetVisible(mUI.sampleType, false);
    SetVisible(mUI.sampleRate, false);
    SetVisible(mUI.channels, false);
    SetVisible(mUI.fileSource, false);
    SetVisible(mUI.btnSelectFile, false);
    SetVisible(mUI.gainValue,  false);
    SetVisible(mUI.frequency, false);
    SetVisible(mUI.audioFile, false);
    SetVisible(mUI.lblSampleType, false);
    SetVisible(mUI.lblSampleRate, false);
    SetVisible(mUI.lblFileSource, false);
    SetVisible(mUI.lblGain, false);
    SetVisible(mUI.lblChannels, false);
    SetVisible(mUI.lblFrequency, false);
    */

    auto items = mScene->selectedItems();
    if (items.isEmpty()) return;
    const auto* item = dynamic_cast<AudioElement*>(items[0]);

    SetEnabled(mUI.elemName, true);
    SetEnabled(mUI.elemID, true);
    SetValue(mUI.elemName, item->GetName());
    SetValue(mUI.elemID, item->GetId());

    if (const auto* val = item->GetArgValue<audio::Format>("format"))
    {
        SetEnabled(mUI.sampleType, true);
        SetEnabled(mUI.sampleRate, true);
        SetEnabled(mUI.channels, true);
        SetVisible(mUI.sampleType, true);
        SetVisible(mUI.sampleRate, true);
        SetVisible(mUI.channels, true);
        SetVisible(mUI.lblSampleType, true);
        SetVisible(mUI.lblSampleRate, true);
        SetVisible(mUI.lblChannels, true);
        SetValue(mUI.sampleType, val->sample_type);
        SetValue(mUI.sampleRate, val->sample_rate);
        SetValue(mUI.channels, static_cast<audio::Channels>(val->channel_count));
    }
    if (const auto* val = item->GetArgValue<audio::SampleType>("type"))
    {
        SetEnabled(mUI.sampleType, true);
        SetVisible(mUI.sampleType, true);
        SetVisible(mUI.lblSampleType, true);
        SetValue(mUI.sampleType, *val);
    }
    if (const auto* val = item->GetArgValue<unsigned>("sample_rate"))
    {
        SetEnabled(mUI.sampleRate, true);
        SetVisible(mUI.sampleRate, true);
        SetVisible(mUI.lblSampleRate, true);
        SetValue(mUI.sampleRate, *val);
    }

    if (const auto* val = item->GetArgValue<std::string>("file"))
    {
        SetEnabled(mUI.fileSource, true);
        SetEnabled(mUI.btnSelectFile, true);
        SetVisible(mUI.fileSource, true);
        SetVisible(mUI.btnSelectFile, true);
        SetVisible(mUI.lblFileSource, true);
        SetVisible(mUI.loopCount, true);
        SetEnabled(mUI.loopCount, true);
        SetValue(mUI.fileSource, *val);
    }
    if (const auto* val = item->GetArgValue<audio::FileSource::IOStrategy>("io_strategy"))
    {
        SetEnabled(mUI.ioStrategy, true);
        SetVisible(mUI.ioStrategy, true);
        SetValue(mUI.ioStrategy, *val);
    }

    if (const auto* val = item->GetArgValue<unsigned>("loops"))
    {
        SetEnabled(mUI.loopCount, true);
        SetVisible(mUI.loopCount, true);
        SetValue(mUI.loopCount, *val);
    }
    if (const auto* val = item->GetArgValue<bool>("pcm_caching"))
    {
        SetEnabled(mUI.pcmCaching, true);
        SetVisible(mUI.pcmCaching, true);
        SetValue(mUI.pcmCaching, *val);
    }
    if (const auto* val = item->GetArgValue<bool>("file_caching"))
    {
        SetEnabled(mUI.fileCaching, true);
        SetVisible(mUI.fileCaching, true);
        SetValue(mUI.fileCaching, *val);
    }

    if (const auto* val = item->GetArgValue<float>("gain"))
    {
        SetEnabled(mUI.gainValue, true);
        SetVisible(mUI.gainValue, true);
        SetVisible(mUI.lblGain, true);
        SetValue(mUI.gainValue, *val);
    }

    if (const auto* val = item->GetArgValue<unsigned>("frequency"))
    {
        SetEnabled(mUI.frequency, true);
        SetVisible(mUI.frequency, true);
        SetVisible(mUI.lblFrequency, true);
        SetValue(mUI.frequency, *val);
    }
    if (const auto* val = item->GetArgValue<unsigned>("duration"))
    {
        SetEnabled(mUI.duration, true);
        SetValue(mUI.duration, *val);
    }
    if (const auto* val = item->GetArgValue<unsigned>("delay"))
    {
        SetEnabled(mUI.delay, true);
        SetValue(mUI.delay, *val);
    }
    if (const auto* val = item->GetArgValue<unsigned>("time"))
    {
        SetEnabled(mUI.startTime, true);
        SetValue(mUI.startTime, *val);
    }
    if (const auto* val = item->GetArgValue<audio::Effect::Kind>("effect"))
    {
        SetEnabled(mUI.effect, true);
        SetValue(mUI.effect, *val);
    }

    if (item->IsFileSource())
    {
        SetEnabled(mUI.audioFile, true);
        SetVisible(mUI.audioFile, true);
        SetValue(mUI.afChannels,   QString(""));
        SetValue(mUI.afSampleRate, QString(""));
        SetValue(mUI.afFrames,     QString(""));
        SetValue(mUI.afDuration,   0);
        SetValue(mUI.afSize,       QString(""));
        const std::string& URI = *item->GetArgValue<std::string>("file");
        if (URI.empty())
            return;

        const std::string& file = app::ToUtf8(mWorkspace->MapFileToFilesystem(URI));
        audio::FileSource::FileInfo info;
        if (FindAudioFileInfo(file, &info))
        {
            SetValue(mUI.afChannels,   info.channels);
            SetValue(mUI.afSampleRate, info.sample_rate);
            SetValue(mUI.afFrames,     info.frames);
            SetValue(mUI.afSize,       app::Bytes{info.bytes});
            SetValue(mUI.afDuration, unsigned(info.seconds * 1000));
        }
        else ERROR("Failed to probe audio file. [file='%1']", file);
    }
}

void AudioWidget::SetSelectedElementProperties()
{
    auto items = mScene->selectedItems();
    if (items.isEmpty()) return;
    auto* item = dynamic_cast<AudioElement*>(items[0]);

    item->SetName(GetValue(mUI.elemName));

    if (auto* val = item->GetArgValue<audio::Format>("format"))
    {
        val->sample_type = GetValue(mUI.sampleType);
        val->sample_rate = GetValue(mUI.sampleRate);
        val->channel_count = static_cast<unsigned>((audio::Channels)GetValue(mUI.channels));
    }
    if (auto* val = item->GetArgValue<audio::SampleType>("type"))
        *val = GetValue(mUI.sampleType);
    if (auto* val = item->GetArgValue<unsigned>("sample_rate"))
        *val = GetValue(mUI.sampleRate);
    if (auto* val = item->GetArgValue<std::string>("file"))
        *val = GetValue(mUI.fileSource);
    if (auto* val = item->GetArgValue<float>("gain"))
        *val = GetValue(mUI.gainValue);
    if (auto* val = item->GetArgValue<unsigned>("frequency"))
        *val = GetValue(mUI.frequency);
    if (auto* val = item->GetArgValue<unsigned>("duration"))
        *val = GetValue(mUI.duration);
    if (auto* val = item->GetArgValue<unsigned>("delay"))
        *val = GetValue(mUI.delay);
    if (auto* val = item->GetArgValue<unsigned>("time"))
        *val = GetValue(mUI.startTime);
    if (auto* val = item->GetArgValue<audio::Effect::Kind>("effect"))
        *val = GetValue(mUI.effect);
    if (auto* val = item->GetArgValue<unsigned>("loops"))
        *val = GetValue(mUI.loopCount);
    if (auto* val = item->GetArgValue<bool>("pcm_caching"))
        *val = GetValue(mUI.pcmCaching);
    if (auto* val = item->GetArgValue<bool>("file_caching"))
        *val = GetValue(mUI.fileCaching);
    if (auto* val = item->GetArgValue<audio::FileSource::IOStrategy>("io_strategy"))
        *val = GetValue(mUI.ioStrategy);

    mScene->invalidate();
}

void AudioWidget::UpdateElementList()
{
    std::vector<ResourceListItem> items;
    for (auto* item : mItems)
    {
        auto* element = dynamic_cast<AudioElement*>(item);
        ResourceListItem li;
        li.id   = app::FromUtf8(element->GetId());
        li.name = app::FromUtf8(element->GetName());
        li.selected = element->isSelected();
        items.push_back(li);
    }
    SetList(mUI.elements, items);
    SetList(mUI.outElem, items);
}

void AudioWidget::OnAudioPlayerEvent(const audio::Player::SourceCompleteEvent& event)
{
    if (event.id == mCurrentId)
    {
        SetEnabled(mUI.actionPlay, true);
        SetEnabled(mUI.actionStop, false);
        SetEnabled(mUI.actionPause, false);
        mCurrentId = 0;
        mPlayTime = 0;
        mRefreshTimer.stop();
    }
}
void AudioWidget::OnAudioPlayerEvent(const audio::Player::SourceProgressEvent& event)
{
    if (event.id == mCurrentId)
        mPlayTime = event.time / 1000.0;
}

void AudioWidget::OnAudioPlayerEvent(const audio::Player::SourceEvent& event)
{

}

void AudioWidget::keyPressEvent(QKeyEvent* key)
{
    // QAction doesn't trigger unless it's been added
    // to some widget (toolbar/menu)
    if (key->key() == Qt::Key_Delete)
    {
        on_actionDelete_triggered();
        return;
    }
    QWidget::keyPressEvent(key);
}

} // namespace

