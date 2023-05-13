// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include "warnpush.h"
#  include <QGraphicsItem>
#  include <QMenu>
#  include <QVariant>
#  include <QVariantMap>
#include "warnpop.h"

#include <cmath>

#include "game/animation.h"
#include "game/entity.h"
#include "editor/app/eventlog.h"
#include "editor/gui/entitywidget.h"
#include "editor/gui/dlganimator.h"
#include "editor/gui/utility.h"

namespace gui::detail {
class StateItem : public QGraphicsItem
{
public:
    StateItem()
      : mId(base::RandomString(10))
    {
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);
    }
    void SetName(const QString& name)
    { mName = name; }
    app::AnyString GetId() const
    { return mId; }
    app::AnyString GetName() const
    { return mName; }

    QPointF MapLinkPointTowards(const StateItem* other) const
    {
        return scenePos();
    }

    bool IsLinkHotZone(const QPointF& scene_pos) const
    {
        const auto& local_pos = mapFromScene(scene_pos);
        const auto& bounds = boundingRect();
        if (!bounds.contains(local_pos))
            return false;

        const auto width = mWidth - 40.0f;
        const auto height = mHeight - 20.0f;
        const auto& inside = QRectF(-width*0.5f, -height*0.5, width, height);
        if (inside.contains(local_pos))
            return false;
        return true;
    }

    void ApplyState(game::AnimatorClass& klass) const
    {
        game::AnimationStateClass state(mId);
        state.SetName(GetName());
        klass.AddState(std::move(state));
    }
    void LoadState(const game::AnimationStateClass& klass)
    {
        mId = klass.GetId();
        mName = app::FromUtf8(klass.GetName());
    }

    void SaveProperties(QVariantMap& props) const
    {
        const auto& pos = scenePos();
        props[app::PropertyKey("scene_pos_x", mId)] = pos.x();
        props[app::PropertyKey("scene_pos_y", mId)] = pos.y();
    }
    void LoadProperties(const QVariantMap& props)
    {
        const float x = props[app::PropertyKey("scene_pos_x", mId)].toFloat();
        const float y = props[app::PropertyKey("scene_pos_y", mId)].toFloat();
        setPos(QPointF(x, y));
    }

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
        const auto& palette = option->palette;
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);

        const auto& rect = boundingRect();

        QPainterPath path;
        path.addRoundedRect(rect, 10, 10);

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
        painter->drawText(rect, Qt::AlignVCenter | Qt::AlignHCenter, mName);
    }
    virtual QRectF boundingRect() const override
    {
        return {-mWidth*0.5f, -mHeight*0.5f, mWidth, mHeight};
    }
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
        // this is the wrong place to do things, try to fix this API mess
        // and dispatch the call to the right place, i.e the scene.
        auto* ugly = dynamic_cast<AnimatorGraphScene*>(scene());
        if (ugly == nullptr) return value;
        ugly->NotifyItemChange(change, this);

        return QGraphicsItem::itemChange(change, value);
    }
private:
    float mWidth  = 200.0f;
    float mHeight = 50.0f;
    std::string mId;
    QString mName;
};

class StateLink : public QGraphicsItem
{
public:
    StateLink()
      : mId(base::RandomString(10))
    {
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemIsMovable, false);
    }

    virtual QRectF boundingRect() const override
    {
        const auto& src = mapFromScene(mSrcPoint);
        const auto& dst = mapFromScene(mDstPoint);
        const auto top    = std::min(src.y(), dst.y());
        const auto left   = std::min(src.x(), dst.x());
        const auto right  = std::max(src.x(), dst.x());
        const auto bottom = std::max(src.y(), dst.y());
        return QRectF(left, top, right-left, bottom-top);
    }

    // Set curve start and end points in scene coordinates.
    void SetCurve(const QPointF& src_point, const QPointF& dst_point)
    {
        mSrcPoint = src_point;
        mDstPoint = dst_point;
        this->update();
    }
    QPointF GetSrcPoint() const
    { return mSrcPoint; }
    QPointF GetDstPoint() const
    { return mDstPoint; }
    QString GetSrcNodeId() const
    { return mSrcState->GetId(); }
    QString GetDstNodeId() const
    { return mDstState->GetId(); }

    void SetSrcState(StateItem* state)
    { mSrcState = state; }
    void SetDstState(StateItem* state)
    { mDstState = state; }

    StateItem* GetDstState()
    { return mDstState; }
    StateItem* GetSrcState()
    { return mSrcState; }

    const StateItem* GetDstState() const
    { return mDstState; }
    const StateItem* GetSrcState() const
    { return mSrcState; }

    app::AnyString GetName() const
    { return mName; }
    app::AnyString GetId() const
    { return mId; }
    void SetName(const app::AnyString& name)
    { mName = name; }
    void SetDuration(float duration)
    { mDuration = duration; }
    float GetDuration() const
    { return mDuration; }

    void ApplyState(game::AnimatorClass& klass) const
    {
        game::AnimationStateTransitionClass transition(mId);
        transition.SetDstStateId(mDstState->GetId());
        transition.SetSrcStateId(mSrcState->GetId());
        transition.SetName(GetName());
        transition.SetDuration(mDuration);
        klass.AddTransition(std::move(transition));
    }
    void LoadState(const game::AnimationStateTransitionClass& transition)
    {
        mId = transition.GetId();
        mName = app::FromUtf8(transition.GetName());
    }

    void SaveProperties(QVariantMap& props) const
    {
        props[app::PropertyKey("src_point_x", mId)] = mSrcPoint.x();
        props[app::PropertyKey("src_point_y", mId)] = mSrcPoint.y();
        props[app::PropertyKey("dst_point_x", mId)] = mDstPoint.x();
        props[app::PropertyKey("dst_point_y", mId)] = mDstPoint.y();
        props[app::PropertyKey("scene_pos_x", mId)] = scenePos().x();
        props[app::PropertyKey("scene_pos_y", mId)] = scenePos().x();
    }
    void LoadProperties(const QVariantMap& props)
    {
        float x = 0.0f;
        float y = 0.0f;
        x = props[app::PropertyKey("src_point_x", mId)].toFloat();
        y = props[app::PropertyKey("src_point_y", mId)].toFloat();
        mSrcPoint = QPointF(x, y);
        x = props[app::PropertyKey("dst_point_x", mId)].toFloat();
        y = props[app::PropertyKey("dst_point_y", mId)].toFloat();
        mDstPoint = QPointF(x, y);
        x = props[app::PropertyKey("scene_pos_x", mId)].toFloat();
        y = props[app::PropertyKey("scene_pos_y", mId)].toFloat();
        setPos(QPointF(x, y));
    }

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        // drawing in the item's coordinate space.

        const auto& palette = option->palette;
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);

        const auto& src = mapFromScene(mSrcPoint);
        const auto& dst = mapFromScene(mDstPoint);
        const QLineF line(src, dst);

        QPen pen;
        if (isSelected())
            pen.setColor(palette.color(QPalette::Highlight));
        else pen.setColor(palette.color(QPalette::Light));

        pen.setWidth(3);
        painter->setPen(pen);
        painter->drawLine(line);

        QPolygonF arrow;
        arrow << QPointF( 5.0f,  0.0f);
        arrow << QPointF(-5.0f,  5.0f);
        arrow << QPointF(-5.0f, -5.0f);

        const auto length = line.length();
        const auto arrow_size = 10.0 / length;

        QPointF arrow_pos = src + (dst-src)*(0.5+arrow_size); //line.center();
        QTransform transform;
        transform.translate(arrow_pos.x(), arrow_pos.y());
        transform.rotate(-line.angle());

        painter->setTransform(transform, true);
        painter->drawPolygon(arrow);
    }
private:
    std::string mId;
    QString mName;
    QPointF mSrcPoint;
    QPointF mDstPoint;
    StateItem* mSrcState = nullptr;
    StateItem* mDstState = nullptr;
    float mDuration = 0.0f;
};

} // namespace

namespace gui
{
AnimatorGraphScene::~AnimatorGraphScene() noexcept
{}

detail::StateItem* AnimatorGraphScene::FindState(const QString& id)
{
    const auto& list = this->items();
    for (const auto& item : list) {
        if (auto* ptr = dynamic_cast<detail::StateItem*>(item); ptr && ptr->GetId() == id)
            return ptr;
    }
    return nullptr;
}
detail::StateLink* AnimatorGraphScene::FindLink(const QString& id)
{
    const auto& list = this->items();
    for (const auto& item : list) {
        if (auto* ptr = dynamic_cast<detail::StateLink*>(item); ptr && ptr->GetId() == id)
            return ptr;
    }
    return nullptr;
}

QGraphicsItem* AnimatorGraphScene::FindItem(const QString& id)
{
    const auto& list = this->items();
    for (const auto& item : list) {
        if (auto* ptr = dynamic_cast<detail::StateLink*>(item); ptr && ptr->GetId() == id)
            return ptr;
        else if (auto* ptr = dynamic_cast<detail::StateItem*>(item); ptr && ptr->GetId() == id)
            return ptr;
    }
    return nullptr;
}


void AnimatorGraphScene::DeleteLink(detail::StateLink* link)
{

    delete link;
}

void AnimatorGraphScene::DeleteState(detail::StateItem* state)
{
    const auto& list = this->items();
    // delete the links that are linked to this state.
    for (const auto& item : list)
    {
        if (auto* ptr = dynamic_cast<detail::StateLink*>(item))
        {
            if (ptr->GetSrcNodeId() == state->GetId() ||
                ptr->GetDstNodeId() == state->GetId())
                delete ptr;
        }
    }
    delete state;
}


void AnimatorGraphScene::ApplyGraphState(game::AnimatorClass& klass) const
{
    const auto& list = this->items();
    for (const auto& item : list)
    {
        if (const auto* ptr = dynamic_cast<const detail::StateLink*>(item))
            ptr->ApplyState(klass);
        else if (const auto* ptr = dynamic_cast<const detail::StateItem*>(item))
            ptr->ApplyState(klass);
    }
}

void AnimatorGraphScene::SaveGraphProperties(QVariantMap& props) const
{
    const auto& list = this->items();
    for (const auto& item : list)
    {
        if (const auto* ptr = dynamic_cast<const detail::StateLink*>(item))
            ptr->SaveProperties(props);
        else if (const auto* ptr = dynamic_cast<const detail::StateItem*>(item))
            ptr->SaveProperties(props);
    }
}

void AnimatorGraphScene::NotifyItemChange(QGraphicsItem::GraphicsItemChange change, QGraphicsItem* item)
{
    if (change != QGraphicsItem::ItemPositionChange)
        return;

    if (auto* state = dynamic_cast<detail::StateItem*>(item))
    {
        const auto& list = items();
        for (auto* item : list)
        {
            if (auto* link = dynamic_cast<detail::StateLink*>(item))
            {
                const auto& src_node = link->GetSrcNodeId();
                const auto& dst_node = link->GetDstNodeId();
                if (src_node == state->GetId())
                    link->SetCurve(state->scenePos(), link->GetDstPoint());
                else if (dst_node == state->GetId())
                    link->SetCurve(link->GetSrcPoint(), state->scenePos());
            }
        }
    }
}

void AnimatorGraphScene::mousePressEvent(QGraphicsSceneMouseEvent* mickey)
{
    if (mickey->button() != Qt::LeftButton)
        return;

    const auto pos = mickey->scenePos();
    auto* item = itemAt(pos, QTransform());
    if (item == nullptr)
        return QGraphicsScene::mousePressEvent(mickey);
    auto* state = dynamic_cast<detail::StateItem*>(item);
    if (state == nullptr)
        return QGraphicsScene::mousePressEvent(mickey);
    if (!state->IsLinkHotZone(pos))
        return QGraphicsScene::mousePressEvent(mickey);

    auto* link = new detail::StateLink();
    link->SetSrcState(state);
    link->SetCurve(pos, pos);
    addItem(link);
    mLink.reset(link);
}
void AnimatorGraphScene::mouseMoveEvent(QGraphicsSceneMouseEvent* mickey)
{
    if (mLink == nullptr)
        return QGraphicsScene::mouseMoveEvent(mickey);

    mLink->SetCurve(mLink->GetSrcPoint(), mickey->scenePos());
}
void AnimatorGraphScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* mickey)
{
    if (mLink == nullptr)
        return QGraphicsScene::mouseReleaseEvent(mickey);

    auto carcass = std::move(mLink);

    const auto pos = mickey->scenePos();

    // check which item (if any) is under the mouse at release
    auto* item = itemAt(pos, QTransform());
    if (item == nullptr)
        return QGraphicsScene::mouseReleaseEvent(mickey);

    // check if it's a state item
    auto* state = dynamic_cast<detail::StateItem*>(item);
    if (state == nullptr)
        return QGraphicsScene::mouseReleaseEvent(mickey);

    // check if trying to self connect (src and dst are same)
    const auto& id = state->GetId();
    if (id == carcass->GetSrcNodeId())
        return QGraphicsScene::mouseReleaseEvent(mickey);

    // check if the link already exists.
    const auto& list = this->items();
    for (const auto* item : list)
    {
        if (item == carcass.get())
            continue;
        if (const auto* link = dynamic_cast<const detail::StateLink*>(item))
        {
            if (link->GetSrcNodeId() == carcass->GetSrcNodeId() &&
                link->GetDstNodeId() == state->GetId())
                return QGraphicsScene::mouseReleaseEvent(mickey);
        }
    }

    carcass->SetDstState(state);
    AdjustLink(carcass.get());

    carcass.release();
    QGraphicsScene::mouseReleaseEvent(mickey);
}

void AnimatorGraphScene::AdjustLink(detail::StateLink* link)
{
    const detail::StateItem* src_state = link->GetSrcState();
    const detail::StateItem* dst_state = link->GetDstState();
    ASSERT(src_state);
    ASSERT(dst_state);

    const auto& src_point = src_state->scenePos(); //MapLinkPointTowards(dst_state);
    const auto& dst_point = dst_state->scenePos(); //MapLinkPointTowards(src_state);
    link->SetCurve(src_point, dst_point);
    link->setZValue(-1.0f);
    update();
    invalidate();
}


DlgAnimator::DlgAnimator(QWidget* parent,
                         const game::EntityClass& entity,
                         const game::AnimatorClass& animator,
                         const QVariantMap& props)
  : QDialog(parent)
  , mEntity(&entity)
  , mAnimator(animator)
{
    mUI.setupUi(this);
    mScene = std::make_unique<AnimatorGraphScene>();
    mUI.stateView->setScene(mScene.get());
    mUI.stateView->setInteractive(true);
    mUI.stateView->setBackgroundBrush(QBrush(QColor(0x23, 0x23, 0x23, 0xff)));
    connect(mScene.get(), &AnimatorGraphScene::selectionChanged, this, &DlgAnimator::SceneSelectionChanged);

    SetValue(mUI.animName, mAnimator.GetName());
    SetValue(mUI.animID,   mAnimator.GetId());
    SetVisible(mUI.linkProperties, false);
    SetVisible(mUI.nodeProperties, false);
    SetVisible(mUI.propertyHelp, true);

    std::unordered_map<std::string, detail::StateItem*> states;

    for (unsigned i=0; i<mAnimator.GetNumStates(); ++i)
    {
        const auto& state = mAnimator.GetState(i);
        auto* item = new detail::StateItem();
        item->LoadState(state);
        item->LoadProperties(props);
        mScene->addItem(item);
        states[state.GetId()] = item;
    }
    for (unsigned i=0; i<mAnimator.GetNumTransitions(); ++i)
    {
        const auto& link = mAnimator.GetTransition(i);
        auto* item = new detail::StateLink();
        item->LoadState(link);
        item->LoadProperties(props);
        item->SetDstState(states[link.GetDstStateId()]);
        item->SetSrcState(states[link.GetSrcStateId()]);
        item->setZValue(-1.0f);
        mScene->addItem(item);
    }

    ShowProperties((detail::StateItem*)nullptr);
    ShowProperties((detail::StateLink*)nullptr);
    UpdateStateList();
    SetValue(mUI.cmbInitState, ListItemId(mAnimator.GetInitialStateId()));
}

DlgAnimator::~DlgAnimator()
{
    mUI.stateView->blockSignals(true);
    mUI.stateList->blockSignals(true);
    mScene->blockSignals(true);
    mScene->clear();
}

void DlgAnimator::UpdateStateList()
{
    std::vector<ResourceListItem> all_items;
    std::vector<ResourceListItem> states;

    const auto& list = mScene->items();
    for (const auto* item : list)
    {
        if (const auto* ptr = dynamic_cast<const detail::StateItem*>(item))
        {
            ResourceListItem li;
            li.id   = ptr->GetId();
            li.name = ptr->GetName();
            li.selected = ptr->isSelected();
            all_items.push_back(li);
            states.push_back(li);
        }
        else if (const auto* ptr = dynamic_cast<const detail::StateLink*>(item))
        {
            const auto* src = ptr->GetSrcState();
            const auto* dst = ptr->GetDstState();

            ResourceListItem li;
            li.id   = ptr->GetId();
            li.name = app::toString("%1 -> %2", src->GetName(), dst->GetName());
            li.selected = ptr->isSelected();
            all_items.push_back(li);
        }
    }
    SetList(mUI.stateList, all_items);
    SetList(mUI.cmbInitState, states);
}

void DlgAnimator::SetStateProperties(detail::StateItem* state) const
{
    state->SetName(GetValue(mUI.stateName));

    mScene->update();
}

void DlgAnimator::SetLinkProperties(detail::StateLink* link) const
{
    link->SetName(GetValue(mUI.linkName));
    link->SetDuration(GetValue(mUI.linkDuration));

    mScene->update();
}

void DlgAnimator::ShowProperties(detail::StateItem* state)
{
    SetEnabled(mUI.nodeProperties, false);
    SetVisible(mUI.nodeProperties, false);
    SetVisible(mUI.propertyHelp, true);
    if (state == nullptr)
        return;

    SetEnabled(mUI.nodeProperties, true);
    SetVisible(mUI.propertyHelp, false);
    SetVisible(mUI.nodeProperties, true);
    SetValue(mUI.stateID, state->GetId());
    SetValue(mUI.stateName, state->GetName());
}

void DlgAnimator::ShowProperties(detail::StateLink* link)
{
    SetEnabled(mUI.linkProperties, false);
    SetVisible(mUI.linkProperties, false);
    SetVisible(mUI.propertyHelp, true);
    if (link == nullptr)
        return;

    SetEnabled(mUI.linkProperties, true);
    SetVisible(mUI.propertyHelp, false);
    SetVisible(mUI.linkProperties, true);
    SetValue(mUI.linkID, link->GetId());
    SetValue(mUI.linkName, link->GetName());
    SetValue(mUI.linkDuration, link->GetDuration());

}

detail::StateItem* DlgAnimator::GetSelectedState()
{
    const auto& selected = mScene->selectedItems();
    if (selected.isEmpty())
        return nullptr;
    return dynamic_cast<detail::StateItem*>(selected[0]);
}

detail::StateLink* DlgAnimator::GetSelectedLink()
{
    const auto& selected = mScene->selectedItems();
    if (selected.isEmpty())
        return nullptr;
    return dynamic_cast<detail::StateLink*>(selected[0]);
}

void DlgAnimator::on_btnCancel_clicked()
{
    reject();
}

void DlgAnimator::on_btnAccept_clicked()
{
    mAnimator.SetName(GetValue(mUI.animName));
    mAnimator.SetInitialStateId(GetItemId(mUI.cmbInitState));
    mAnimator.ClearStates();
    mAnimator.ClearTransitions();

    QVariantMap properties;

    mScene->ApplyGraphState(mAnimator);
    mScene->SaveGraphProperties(properties);

    mEntityWidget->SaveAnimator(mAnimator, properties);

    accept();
}

void DlgAnimator::on_stateView_customContextMenuRequested(QPoint pos)
{
    auto* state = GetSelectedState();
    auto* link  = GetSelectedLink();
    SetEnabled(mUI.actionStateDel, state!= nullptr);
    SetEnabled(mUI.actionLinkDel, link!=nullptr);

    QMenu menu(this);
    menu.addAction(mUI.actionStateNew);
    menu.addAction(mUI.actionStateDel);
    menu.addSeparator();
    menu.addAction(mUI.actionLinkDel);
    menu.exec(QCursor::pos());
}

void DlgAnimator::on_stateList_itemSelectionChanged()
{
    QSignalBlocker hell(mScene.get());
    mScene->clearSelection();

    auto selected = mUI.stateList->selectedItems();

    for (auto* list_item : selected)
    {
        const auto id = gui::GetItemId(list_item);
        auto* scene_item = mScene->FindItem(id);
        scene_item->setSelected(true);
    }
    mUI.stateView->update();

    SceneSelectionChanged();
}

void DlgAnimator::on_actionStateNew_triggered()
{
    QSignalBlocker hell(mScene.get());
    mScene->clearSelection();

    auto* element = new detail::StateItem();

    // todo: this is off somehow... weird
    const auto& mouse_pos = mUI.stateView->mapFromGlobal(QCursor::pos());
    const auto& scene_pos = mUI.stateView->mapToScene(mouse_pos);
    element->setPos(scene_pos);
    element->SetName("New State");
    element->setSelected(true);
    // scene takes ownership of the graphics item
    mScene->addItem(element);

    UpdateStateList();
    ShowProperties(element);
}

void DlgAnimator::on_actionStateDel_triggered()
{
    if (auto* selected = GetSelectedState())
    {
        bool was_initial_state = mAnimator.GetInitialStateId() == selected->GetId();

        mAnimator.DeleteStateById(selected->GetId());
        mScene->DeleteState(selected);
        UpdateStateList();
        ShowProperties(GetSelectedState());
        ShowProperties(GetSelectedLink());

        if (was_initial_state)
        {
            mAnimator.SetInitialStateId("");
            if (mAnimator.GetNumStates())
                mAnimator.SetInitialStateId(mAnimator.GetState(0).GetId());

            SetValue(mUI.cmbInitState, ListItemId(mAnimator.GetInitialStateId()));
        }
    }
}

void DlgAnimator::on_actionLinkDel_triggered()
{
    if (auto* selected = GetSelectedLink())
    {
        mAnimator.DeleteTransitionById(selected->GetId());
        mScene->DeleteLink(selected);

        UpdateStateList();
        ShowProperties(GetSelectedState());
        ShowProperties(GetSelectedLink());
    }
}

void DlgAnimator::on_stateName_textChanged(const QString&)
{
    SetStateProperties(GetSelectedState());
    UpdateStateList();
}

void DlgAnimator::on_linkName_textChanged(const QString&)
{
    SetLinkProperties(GetSelectedLink());
}

void DlgAnimator::SceneSelectionChanged()
{
    SetVisible(mUI.linkProperties, false);
    SetVisible(mUI.nodeProperties, false);
    SetVisible(mUI.propertyHelp, true);

    if (auto* state = GetSelectedState())
        ShowProperties(state);
    else if (auto* link = GetSelectedLink())
        ShowProperties(link);

    UpdateStateList();
}

}// namespace