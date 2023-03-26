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
#  include <QSyntaxHighlighter>
#  include <QTextFormat>
#  include <QColor>
#  include <tree_sitter/api.h>
#include "warnpop.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace app
{
    class LuaTheme
    {
    public:
        enum class Theme {
            Monokai
        };
        enum class Key {
            Keyword,
            Literal,
            BuiltIn,
            Comment,
            Property,
            FunctionBody,
            FunctionCall,
            MethodCall,
            Punctuation,
            Bracket,
            Operator,
            Other
        };
        void SetTheme(Theme theme);

        const QColor* GetColor(Key key) const noexcept;
    private:
        std::unordered_map<Key, QColor> mTable;
    };

    class LuaParser : public QSyntaxHighlighter
    {
    public:
        using HighlightKey = LuaTheme::Key;
        using ColorTheme = LuaTheme::Theme;

        struct Highlight {
            using Type = HighlightKey;
            Type type = Type::Keyword;
            // character position of the highlight in the current document.
            uint32_t start = 0;
            // length of the highlight in characters
            uint32_t length = 0;
        };

        LuaParser(const LuaParser&) = delete;
        explicit LuaParser(QTextDocument* document)
          : QSyntaxHighlighter(document)
        {}
       ~LuaParser();

        bool Parse(const QString& str);
        void SetTheme(LuaTheme&& theme)
        { mTheme = std::move(theme); }
        void SetTheme(ColorTheme  theme)
        { mTheme.SetTheme(theme); }

        size_t GetNumHighlights() const noexcept
        { return mHighlights.size(); }
        const Highlight& GetHighlight(size_t i) const noexcept
        { return mHighlights[i]; }

        const Highlight* FindBlockByOffset(uint32_t position) const noexcept;

        LuaParser& operator=(const LuaParser&) = delete;
    protected:
        virtual void highlightBlock(const QString &text) override;

    private:
        std::vector<Highlight> mHighlights;
        std::unordered_map<Highlight::Type, QTextFormat> mColors;
        LuaTheme mTheme;
    private:
        TSTree* mTree = nullptr;
        TSParser* mParser = nullptr;
        TSQuery* mQuery = nullptr;
        std::unordered_map<std::string, uint16_t> mFields;
    };

} // namespace