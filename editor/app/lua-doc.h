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
#  include <QString>
#  include <QSortFilterProxyModel>
#  include <QAbstractTableModel>
#include "warnpop.h"

#include <string>
#include <vector>

#include "base/bitflag.h"

namespace app
{
    struct LuaMethodArg {
        QString name;
        QString type;
    };
    enum class LuaMemberType {
        TableProperty,
        ObjectProperty,
        Function,
        Method,
        MetaMethod,
        Table
    };

    struct LuaMemberDoc {
        LuaMemberType type = LuaMemberType::Function;
        QString table;
        QString name;
        QString desc;
        QString ret;
        std::vector<LuaMethodArg> args;
    };

    void InitLuaDoc();

    std::size_t GetNumLuaMethodDocs();
    const LuaMemberDoc& GetLuaMethodDoc(size_t index);

    enum class LuaHelpStyle {
        FunctionCallFormat, DescriptionFormat
    };
    enum class LuaHelpFormat {
        PlainText // HTML not implemented yet.
    };

    QString FormatArgHelp(const LuaMemberDoc& doc, LuaHelpStyle style, LuaHelpFormat format);
    QString FormatHelp(const LuaMemberDoc& doc, LuaHelpFormat format);
    QString FormatArgCompletion(const LuaMemberDoc& doc);
    QString ParseLuaDocTypeString(const QString& str);
    QString FindLuaDocTableMatch(const QString& word);
    QString FindLuaDocFieldMatch(const QString& word);
    QString GenerateLuaDocHtml();
    QString GenerateLuaDocHtmlAnchor(const LuaMemberDoc& doc);

    class LuaDocTableModel : public QAbstractTableModel
    {
    public:
        enum class Mode {
            HelpView, CodeCompletion
        };
        explicit LuaDocTableModel(Mode mode = Mode::HelpView)
          : mMode(mode)
        {}

        QVariant data(const QModelIndex& index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        int rowCount(const QModelIndex&) const override;
        int columnCount(const QModelIndex&) const override;

        const LuaMemberDoc& GetDocItem(size_t index) const;
        const LuaMemberDoc& GetDocItem(const QModelIndex& index)  const;

        void SetMode(Mode mode)
        { mMode = mode; }

        void ClearDynamicCompletions()
        { mDynamicCompletions.clear(); }
        void SetDynamicCompletions(const std::vector<LuaMemberDoc>& data)
        { mDynamicCompletions = data; }
        void SetDynamicCompletions(std::vector<LuaMemberDoc>&& data)
        { mDynamicCompletions = std::move(data); }
    private:
        Mode mMode = Mode::HelpView;
        std::vector<LuaMemberDoc> mDynamicCompletions;
    };

    class LuaDocModelProxy : public QSortFilterProxyModel
    {
    public:
        using Show = app::LuaMemberType;

        LuaDocModelProxy()
        { mBits.set_from_value(~0u); }

        void SetVisible(Show what, bool on_off)
        { mBits.set(what, on_off); }
        void SetVisible(unsigned bits)
        { mBits.set_from_value(bits); }

        void ClearFilter()
        {
            mBits.set_from_value(~0u);
            mFindString.clear();
            mTableName.clear();
            mFieldName.clear();
        }
        void SetTableModel(LuaDocTableModel* model)
        {
            mModel = model;
            setSourceModel(model);
        }
        void SetFindFilter(const QString& filter);
        void SetTableNameFilter(const QString& name);
        void SetFieldNameFilter(const QString& name);

        const auto& GetDocItemFromSource(size_t index) const
        { return mModel->GetDocItem(index); }
        const auto& GetDocItemFromSource(const QModelIndex& index) const
        { return mModel->GetDocItem(index); }

    protected:
        virtual bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
    private:
        QString mFindString;
        QString mTableName;
        QString mFieldName;
        LuaDocTableModel* mModel = nullptr;
        base::bitflag<Show> mBits;
    };

} // namespace
