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

#include "app/types.h"

namespace gui
{
    class TimelineWidget : public QAbstractScrollArea
    {
        Q_OBJECT

    public:
        // One item that appears on the visualization of the timeline
        struct TimelineItem
        {
            enum class Type {
                // A span (block) of time. Start and duration are used to define
                // the span.
                Span,
                // A point (such as an event) in time. Startime is used to define
                // the point in time.
                Point
            };
            Type type = Type::Span;

            // color of the visualized item.
            QColor  color;
            // application defined item id.
            app::AnyString id;
            // text (name) of the item.
            app::AnyString text;
            // icon (if any) of the item.
            QPixmap icon;
            // application defined data value.
            std::any user;
            // normalized (with respect to the timeline widget's time duration)
            // start time of the item on its timeline.
            float starttime = 0.0f;
            // normalized (with respect to the timeline widget's time duration)
            // duration of the time.
            float duration  = 0.0f;
        };

        class Timeline
        {
        public:
            explicit Timeline(QString name) : mName(std::move(name))
            {}
            Timeline() = default;
            void AddItem(const TimelineItem& item)
            {
                mItems.push_back(item);
            }
            void SetName(const app::AnyString& name)
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

        explicit TimelineWidget(QWidget* parent);

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
        { return mSelectedItem; }
        const TimelineItem* GetSelectedItem() const
        { return mSelectedItem; }
        void ClearSelection()
        { mSelectedItem = nullptr; }
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
        float MapToSeconds(QPoint pos) const;

        const TimelineItem* SelectItem(const app::AnyString& itemId);
    signals:
        void TimeDragged(float seconds);
        void SelectedItemChanged(const TimelineItem* selected);
        void SelectedItemDragged(const TimelineItem* selected);
        void DeleteSelectedItem(const TimelineItem* selected);

    private:
        void paintEvent(QPaintEvent* event) override;
        void mouseMoveEvent(QMouseEvent* mickey) override;
        void mousePressEvent(QMouseEvent* mickey) override;
        void mouseReleaseEvent(QMouseEvent* mickey) override;
        void wheelEvent(QWheelEvent* wheel) override;
        void scrollContentsBy(int dx, int dy) override;
        void enterEvent(QEvent*) override;
        void leaveEvent(QEvent*) override;
        void resizeEvent(QResizeEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
    private:
        void ComputeVerticalScrollbars();
        void ComputeHorizontalScrollbars();
        float GetPixelsPerSecond() const;
        QPoint MapFromView(QPoint click_pos) const;

        enum class HotSpot {
            LeftMargin, RightMargin, Ruler, Content
        };
        HotSpot TestHotSpot(const QPoint& click_pos) const;

    private:
        // the provider of the timeline data.
        TimelineModel* mModel = nullptr;
        TimelineItem* mHoveredItem  = nullptr;
        TimelineItem* mSelectedItem = nullptr;
        // The current list of items being visualized.
        std::vector<Timeline> mTimelines;
        // Index of the currently selected item's timeline.
        std::size_t mSelectedTimeline = 0;
        // Index of the currently hovered timeline.
        std::size_t mHoveredTimeline = std::numeric_limits<size_t>::max();

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
