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
#  include <QSettings>
#  include <QVariant>
#  include <QString>
#  include <QtWidgets>
#include "warnpop.h"

namespace gui
{
    // Settings wrapper
    class Settings
    {
    public:
        Settings(const QString& organization,
                 const QString& application)
            : mSettings(organization, application)
        {}

        template<typename T>
        T get(const QString& module, const QString& key, const T& defaultValue) const
        {
            const auto& value = mSettings.value(module + "/" + key);
            if (!value.isValid())
                return defaultValue;
            return qvariant_cast<T>(value);
        }

        template<typename T>
        void set(const QString& module, const QString& key, const T& value)
        {
            mSettings.setValue(module + "/" + key, value);
        }

        void set(const QString& module, const QTableView* table)
        {
            const auto* model  = table->model();
            const auto objName = table->objectName();
            const auto numCols = model->columnCount();

            for (int i=0; i<numCols-1; ++i)
            {
                const auto& key = QString("%1_column_%2").arg(objName).arg(i);
                const auto width = table->columnWidth(i);
                set(module, key, width);
            }
            if (table->isSortingEnabled())
            {
                const QHeaderView* header = table->horizontalHeader();
                const auto sortColumn = header->sortIndicatorSection();
                const auto sortOrder  = header->sortIndicatorOrder();
                set(module, objName + "/sort_column", sortColumn);
                set(module, objName + "/sort_order", sortOrder);
            }
        }

        void set(const QString& module, const QCheckBox* chk)
        {
            set(module, chk->objectName(), chk->isChecked());
        }

        void set(const QString& module, const QSplitter* splitter)
        {
            set(module, splitter->objectName(), splitter->saveState());
        }

        void get(const QString& module, QSplitter* splitter) const
        {
            splitter->restoreState(get(module, splitter->objectName(), splitter->saveState()));
        }

        void get(const QString& module, QTableView* table) const
        {
            const auto* model  = table->model();
            const auto objName = table->objectName();
            const auto numCols = model->columnCount();

            for (int i=0; i<numCols-1; ++i)
            {
                const auto& key  = QString("%1_column_%2").arg(objName).arg(i);
                const auto width = get(module, key, table->columnWidth(i));
                table->setColumnWidth(i, width);
            }
            if (table->isSortingEnabled())
            {
                const QHeaderView* header = table->horizontalHeader();
                const auto column = get(module, objName + "/sort_column",
                    (int)header->sortIndicatorSection());
                const auto order = get(module, objName + "/sort_order",
                    (int)header->sortIndicatorOrder());
                table->sortByColumn(column,(Qt::SortOrder)order);
            }
        }

        void get(const QString& module, QCheckBox* chk) const
        {
            const bool value = get<bool>(module, chk->objectName(), false);
            chk->setChecked(value);
        }


    private:
        QSettings mSettings;
    };
} // namespace