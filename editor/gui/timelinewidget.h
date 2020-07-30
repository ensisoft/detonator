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

#include <vector>
#include <memory>

namespace gui
{
    class TimelineWidget : public QAbstractScrollArea
    {
        Q_OBJECT

    public:
        struct TimelineItem
        {
            QColor  color;
            QString id;
            QString text;
            QIcon   icon;
            void*   user = nullptr;
            float   starttime = 0.0f;
            float   duration  = 0.0f;
        };

        class Timeline
        {
        public:
            Timeline(const QString& name) : mName(name)
            {}
            void AddItem(const TimelineItem& item)
            {
                mItems.push_back(item);
            }

            size_t GetNumItems() const
            { return mItems.size(); }
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
        };

        TimelineWidget(QWidget* parent);

        void SetDuration(float seconds)
        { mDuration = seconds; }
        void SetCurrentTime(float seconds)
        { mCurrentTime = seconds; }
        void SetZoom(double zoom)
        { mZoomFactor = zoom; }

    signals:
        void TimeChanged(float seconds);

    private:
        virtual void paintEvent(QPaintEvent* event) override;
        virtual void mouseMoveEvent(QMouseEvent* mickey) override;
        virtual void mousePressEvent(QMouseEvent* mickey) override;
        virtual void mouseReleaseEvent(QMouseEvent* mickey) override;
        virtual void wheelEvent(QWheelEvent* wheel) override;
        virtual void scrollContentsBy(int dx, int dy) override;

    private:
        std::vector<Timeline> mTimelines;

        float mDuration    = 10.0f; // length of timeline in seconds.
        float mZoomFactor  = 1.0f;
        float mCurrentTime = 0.0f;
        // view offset.
        int mXOffset = 0;
        int mYOffset = 0;

        unsigned mMouseX = 0;
        unsigned mMouseY = 0;

    };
} // namespace
