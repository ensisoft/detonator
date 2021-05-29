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
#  include <QWidget>
#  include <QObject>
#  include <QString>
#  include <QIcon>
#  include <QColor>
#  include <QAbstractScrollArea>
#include "warnpop.h"

#include <any>
#include <vector>
#include <memory>
#include <limits>

namespace gui
{
    class TimelineWidget : public QAbstractScrollArea
    {
        Q_OBJECT

    public:
        // One item that appears on the visualization of the timeline
        struct TimelineItem
        {
            // color of the visualized item.
            QColor  color;
            // application defined item id.
            QString id;
            // text (name) of the item.
            QString text;
            // icon (if any) of the item.
            QIcon   icon;
            // application defined data value.
            std::any user;
            // normalized (with respect to the timeline widget's time duration)
            // start time of the item on its timeline.
            float   starttime = 0.0f;
            // normalized (with respect to the timeline widget's time duration)
            // duration of the time.
            float   duration  = 0.0f;
        };

        class Timeline
        {
        public:
            Timeline(const QString& name) : mName(name)
            {}
            Timeline() = default;
            void AddItem(const TimelineItem& item)
            {
                mItems.push_back(item);
            }
            void SetName(const QString& name)
            { mName = name; }
            size_t GetNumItems() const
            { return mItems.size(); }
            TimelineItem& GetItem(size_t i)
            { return mItems[i]; }
            const TimelineItem& GetItem(size_t i) const
            { return mItems[i]; }
            const QString& GetName() const
            { return mName; }
        private:
            QString mName;
            std::vector<TimelineItem> mItems;
        };

        class TimelineModel
        {
        public:
            virtual ~TimelineModel() = default;
            virtual void Fetch(std::vector<Timeline>* list) = 0;
        private:
        };

        TimelineWidget(QWidget* parent);

        // Rebuild the widget's data representation.
        // This will ask the model to provide a new list of data
        // and then trigger a repaint.
        void Rebuild();

        // Ask the widget to do a repaint. You should call this
        // if you change some items' properties and want to
        // have them reflected in the widget.
        void Update();
        // Repaint immediately.
        void Repaint();

        void SetModel(TimelineModel* model)
        { mModel = model; }
        void SetDuration(float seconds)
        { mDuration = seconds; }
        void SetCurrentTime(float seconds)
        { mCurrentTime = seconds; }
        void SetZoom(double zoom)
        { mZoomFactor = zoom; }
        TimelineItem* GetSelectedItem()
        { return mSelected; }
        const TimelineItem* GetSelectedItem() const
        { return mSelected; }
        void ClearSelection()
        { mSelected = nullptr; }
        void SetFreezeItems(bool freeze)
        { mFreezeItems = freeze; }
        const Timeline* GetCurrentTimeline() const
        {
            if (mHoveredTimeline >= mTimelines.size())
                return nullptr;
            return &mTimelines[mHoveredTimeline];
        }
        Timeline* GetCurrentTimeline()
        {
            if (mHoveredTimeline >= mTimelines.size())
                return nullptr;
            return &mTimelines[mHoveredTimeline];
        }
        size_t GetCurrentTimelineIndex() const
        { return mHoveredTimeline; }
        float MapToSeconds(const QPoint& pos) const;
    signals:
        void TimeDragged(float seconds);
        void SelectedItemChanged(const TimelineItem* selected);
        void SelectedItemDragged(const TimelineItem* selected);

    private:
        virtual void paintEvent(QPaintEvent* event) override;
        virtual void mouseMoveEvent(QMouseEvent* mickey) override;
        virtual void mousePressEvent(QMouseEvent* mickey) override;
        virtual void mouseReleaseEvent(QMouseEvent* mickey) override;
        virtual void wheelEvent(QWheelEvent* wheel) override;
        virtual void scrollContentsBy(int dx, int dy) override;
        virtual void enterEvent(QEvent*) override;
        virtual void leaveEvent(QEvent*) override;
        virtual void resizeEvent(QResizeEvent* event) override;
    private:
        void ComputeVerticalScrollbars();
        void ComputeHorizontalScrollbars();
    private:
        // the provider of the timeline data.
        TimelineModel* mModel = nullptr;
        TimelineItem* mHovered = nullptr;
        TimelineItem* mSelected = nullptr;
        // The current list of items being visualized.
        std::vector<Timeline> mTimelines;
        // Index of the currently selected item's timeline.
        std::size_t mSelectedTimeline = 0;
        // Index of the currently hovered timeline.
        std::size_t mHoveredTimeline = std::numeric_limits<size_t>::max();
        unsigned mNumVisibleTimelines = 0;
        float mDuration    = 10.0f; // length of timeline in seconds.
        float mZoomFactor  = 1.0f;
        float mCurrentTime = 0.0f;
        // scroll view offset.
        int mXOffset = 0;
        int mYOffset = 0;
        // set to true when items cannot be moved/edited.
        bool mFreezeItems = false;

        bool mAlignmentBar = false;
        float mAlignmentBarTime = 0.0f;

        QPoint mDragStart;
        QPoint mDragPoint;
        bool mDragging = false;
        bool mDraggingFromStart = false;
        bool mDraggingFromEnd   = false;
    };
} // namespace
