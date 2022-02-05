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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include "ui_audiowidget.h"
#  include <QGraphicsScene>
#  include <QTimer>
#include "warnpop.h"

#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>

#include "audio/player.h"
#include "audio/fwd.h"
#include "data/fwd.h"
#include "editor/gui/mainwidget.h"

namespace app {
    class Resource;
    class Workspace;
} // app

namespace gui
{
    class AudioGraphScene : public QGraphicsScene
    {
        Q_OBJECT

    public:
        void NotifyItemChange(QGraphicsItem::GraphicsItemChange change,
                              QGraphicsItem* item);
        void ApplyItemChanges();
        void DeleteItems(const QList<QGraphicsItem*>& items);
        void LinkItems(const std::string& src_elem, const std::string& src_port,
                       const std::string& dst_elem, const std::string& dst_port);
        void UnlinkItems(const QList<QGraphicsItem*>& items);
        void UnlinkPort(const std::string& element, const std::string& port);
        void IntoJson(data::Writer& writer) const;
        bool FromJson(const data::Reader& reader);
        void SaveState(app::Resource& resource) const;
        void LoadState(const app::Resource& resource);
        void ApplyState(audio::GraphClass& klass) const;
        bool ValidateGraphContent() const;
        QGraphicsItem* FindItem(const std::string& id);
    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
    private:
        struct ChangeEvent;
        void ApplyChange(const ChangeEvent& event);
    private:
        std::unique_ptr<QGraphicsItem> mLine;
        std::string mSrcElem;
        std::string mSrcPort;
        struct ChangeEvent {
            QGraphicsItem::GraphicsItemChange change;
            QGraphicsItem* item = nullptr;
        };
        std::queue<ChangeEvent> mChanges;
        std::unordered_map<std::string, QGraphicsItem*> mLinkMap;
    };

    class AudioWidget : public MainWidget
    {
        Q_OBJECT

    public:
        AudioWidget(app::Workspace* workspace);
        AudioWidget(app::Workspace* workspace, const app::Resource& resource);
       ~AudioWidget();

        virtual bool IsAccelerated() const override;
        virtual bool CanTakeAction(Actions action, const Clipboard*) const override;
        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual void Refresh() override;
        virtual void Save() override;
        virtual bool SaveState(Settings& settings) const override;
        virtual bool LoadState(const Settings& settings) override;
        virtual void Cut(Clipboard& clipboard) override;
        virtual void Copy(Clipboard& clipboard) const override;
        virtual void Paste(const Clipboard& clipboard) override;
        virtual bool HasUnsavedChanges() const override;
        virtual bool ConfirmClose() override;
        virtual bool GetStats(Stats* stats) const override;
    private:
        void GetSelectedElementProperties();
        void SetSelectedElementProperties();
        void UpdateElementList();
        void OnAudioPlayerEvent(const audio::Player::SourceCompleteEvent& event);
        void OnAudioPlayerEvent(const audio::Player::SourceProgressEvent& event);
        void OnAudioPlayerEvent(const audio::Player::SourceEvent& event);
        virtual void keyPressEvent(QKeyEvent* key) override;
    private slots:
        void on_btnSelectFile_clicked();
        void on_btnEditFile_clicked();

        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionDelete_triggered();
        void on_actionUnlink_triggered();
        void on_actionAddInputPort_triggered();
        void on_actionRemoveInputPort_triggered();
        void on_view_customContextMenuRequested(QPoint);
        void on_elements_itemSelectionChanged();
        void on_elements_customContextMenuRequested(QPoint);
        void on_outElem_currentIndexChanged(int);
        void on_elemName_textChanged(QString);
        void on_sampleType_currentIndexChanged(int);
        void on_sampleRate_currentIndexChanged(int);
        void on_channels_currentIndexChanged(int);
        void on_gainValue_valueChanged(double);
        void on_frequency_valueChanged(int);
        void on_duration_valueChanged(int);
        void on_delay_valueChanged(int);
        void on_startTime_valueChanged(int);
        void on_effect_currentIndexChanged(int);
        void on_loopCount_valueChanged(int);
        void on_caching_stateChanged(int);

        void SceneSelectionChanged();
        void AddElementAction();
        void RefreshTimer();

        bool InitializeAudio();
        size_t GetHash() const;

    private:
        Ui::AudioWidget mUI;
    private:
        app::Workspace* mWorkspace = nullptr;
        std::unique_ptr<AudioGraphScene> mScene;
        std::shared_ptr<audio::Player> mPlayer;
        std::vector<QGraphicsItem*> mItems;
        std::size_t mCurrentId = 0;
        std::size_t mGraphHash = 0;
        double mPlayTime = 0.0;
    private:
        QTimer mRefreshTimer;
    };
} // namespace
