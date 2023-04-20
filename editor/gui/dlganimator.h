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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include "ui_dlganimator.h"
#  include <QDialog>
#  include <QGraphicsScene>
#include "warnpop.h"

#include <memory>

#include "game/entity.h"
#include "game/animation.h"

namespace gui
{
    namespace detail {
        class StateItem;
        class StateLink;
    }
    class EntityWidget;

    class AnimatorGraphScene : public QGraphicsScene
    {
        Q_OBJECT

    public:
        ~AnimatorGraphScene() noexcept override;

        void NotifyItemChange(QGraphicsItem::GraphicsItemChange change, QGraphicsItem* item);

        detail::StateItem* FindState(const QString& id);
        detail::StateLink* FindLink(const QString& id);
        QGraphicsItem* FindItem(const QString& id);

        void DeleteState(detail::StateItem* state);
        void ApplyGraphState(game::AnimatorClass& klass) const;
        void SaveGraphProperties(QVariantMap& props) const;

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
    private:
        void AdjustLink(detail::StateLink* link);
    private:
        std::unique_ptr<detail::StateLink> mLink;
    };

    class DlgAnimator : public QDialog
    {
        Q_OBJECT

    public:
        DlgAnimator(QWidget* parent,
                    const game::EntityClass& entity,
                    const game::AnimatorClass& animator,
                    const QVariantMap& props);
       ~DlgAnimator() override;
        void SetEntityWidget(EntityWidget* widget)
        { mEntityWidget = widget; }
    private:
        void UpdateStateList();
        void SetStateProperties(detail::StateItem* state) const;
        void SetLinkProperties(detail::StateLink* link) const;
        void ShowProperties(detail::StateItem* state);
        void ShowProperties(detail::StateLink* link);
        detail::StateItem* GetSelectedState();
        detail::StateLink* GetSelectedLink();

    private slots:
        void on_btnClose_clicked();
        void on_btnAccept_clicked();
        void on_stateView_customContextMenuRequested(QPoint pos);
        void on_stateList_itemSelectionChanged();
        void on_actionStateNew_triggered();
        void on_actionStateDel_triggered();
        void on_stateName_textChanged(const QString&);
        void on_linkName_textChanged(const QString&);

        void SceneSelectionChanged();

    private:
        Ui::DlgAnimator mUI;
    private:
        std::unique_ptr<AnimatorGraphScene> mScene;
        const game::EntityClass* mEntity = nullptr;
        game::AnimatorClass mAnimator;
        EntityWidget* mEntityWidget = nullptr;
    };

} // namespace
