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
#  include <QAbstractTableModel>
#  include <boost/circular_buffer.hpp>
#include "warnpop.h"

#include <chrono>

#include "base/assert.h"
#include "base/trace.h"
#include "editor/gui/dlgtrace.h"

namespace gui
{

class DlgTrace::TableModel : public QAbstractTableModel,
                             public base::Trace
{
public:
    TableModel(unsigned size, unsigned frame)
       : mCallTrace(size)
       , mCurrentFrame(frame)
    {
        mCallTrace.resize(size);
    }
    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        const auto row = static_cast<size_t>(index.row());
        const auto col = static_cast<size_t>(index.column());
        if (role == Qt::SizeHintRole)
            return QSize(0, 16);
        else if (role == Qt::DisplayRole)
        {
            if (row >= mTraceIndex)
                return QVariant();

            if (col == 0)
                return mCallTrace[index.row()].name;
            else if (col == 1)
                return mCallTrace[index.row()].start_time;
            else if (col == 2)
                return mCallTrace[index.row()].finish_time;
            else if (col == 3)
                return (mCallTrace[row].finish_time - mCallTrace[row].start_time) / 1000.0f;
        }
        else if (role == Qt::DecorationRole && index.column() == 0)
        {
            // todo:
        }
        return QVariant();
    }
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            if (section == 0)      return "Name";
            else if (section == 1) return "Start";
            else if (section == 2) return "Finish";
            else if (section == 3) return "Duration";
        }
        return QVariant();
    }
    virtual int rowCount(const QModelIndex&) const override
    { return static_cast<int>(mCallTrace.size()); }
    virtual int columnCount(const QModelIndex&) const override
    { return 4; }

    void BeginFrame()
    {
        mCurrentFrame++;
    }

    void EndFrame()
    {
    }

    virtual void Clear()
    {
        mTraceIndex = 0;
        mStackDepth = 0;
    }
    virtual void Start()
    {
        mTraceIndex = 0;
        mStackDepth = 0;
        mStartTime  = std::chrono::high_resolution_clock::now();
    }
    virtual void Write(base::TraceWriter& writer) const override
    {
        for (size_t i=0; i<mTraceIndex; ++i)
            writer.Write(mCallTrace[i]);
    }
    virtual unsigned BeginScope(const char* name) override
    {
        ASSERT(mTraceIndex < mCallTrace.size());
        base::TraceEntry entry;
        entry.name  = name;
        entry.level = mStackDepth++;
        entry.start_time = GetTime();
        mCallTrace[mTraceIndex] =  entry;

        emit dataChanged(this->index(mTraceIndex, 0),
                         this->index(mTraceIndex, 4));
        return mTraceIndex++;
    }
    virtual void EndScope(unsigned index) override
    {
        ASSERT(index < mCallTrace.size());
        ASSERT(mStackDepth);
        mCallTrace[index].finish_time = GetTime();
        mStackDepth--;

        emit dataChanged(this->index(index, 0),
                         this->index(index, 4));
    }
private:
    unsigned GetTime() const
    {
        using clock = std::chrono::high_resolution_clock;
        const auto now = clock::now();
        const auto gone = now - mStartTime;
        return std::chrono::duration_cast<std::chrono::microseconds>(gone).count();
    }
private:
    std::vector<base::TraceEntry> mCallTrace;
    std::size_t mTraceIndex = 0;
    std::size_t mStackDepth = 0;
    std::chrono::high_resolution_clock::time_point mStartTime;
    unsigned mCurrentFrame = 0;
};

DlgTrace::DlgTrace(QWidget* parent, engine::Engine* engine) : QDialog(parent)
{
    mUI.setupUi(this);
    mTableModel.reset(new TableModel(1000, 0));
    mUI.tableView->setModel(mTableModel.get());
    mEngine = engine;
}

DlgTrace::~DlgTrace() = default;

void DlgTrace::BeginFrame()
{
    mTableModel->BeginFrame();
}
void DlgTrace::EndFrame()
{
    mTableModel->EndFrame();
}

void DlgTrace::on_btnClose_clicked()
{
    mClosed = true;
}
void DlgTrace::on_btnStart_clicked()
{
    base::SetThreadTrace(mTableModel.get());
    mEngine->SetTracer(mTableModel.get());
}
void DlgTrace::on_btnStop_clicked()
{
    base::SetThreadTrace(nullptr);
    mEngine->SetTracer(nullptr);
}

} // namespace
