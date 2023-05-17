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
#  include <QTextFormat>
#  include <QColor>
#  include <tree_sitter/api.h>
#include "warnpop.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace app
{
    enum class LuaSyntax {
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

    enum class LuaSymbol {
        Function,
        LocalVariable
    };

    class LuaTheme
    {
    public:
        enum class Theme {
            Monokai
        };
        using Key = LuaSyntax;

        void SetTheme(Theme theme);

        const QColor* GetColor(Key key) const noexcept;
    private:
        std::unordered_map<Key, QColor> mTable;
    };

    class LuaParser
    {
    public:
        using LuaSyntax = app::LuaSyntax;
        using LuaSymbol = app::LuaSymbol;

        struct SyntaxBlock {
            LuaSyntax type = LuaSyntax::Other;
            // character position of the syntax highlight in the current document.
            uint32_t start = 0;
            // length of the syntax highlight in characters
            uint32_t length = 0;
        };

        struct Symbol {
            LuaSymbol type = LuaSymbol::Function;
            // character position of the highlight in the current document.
            uint32_t start  = 0;
            // length of the symbol name in characters
            uint32_t length = 0;
        };

        LuaParser(const LuaParser&) = delete;
        LuaParser();
       ~LuaParser();

        void ClearParseState();

        bool ParseSource(const QString& source);

        struct Edit {
            QString* old_source = nullptr;
            QString* new_source = nullptr;
            uint32_t position   = 0;
            uint32_t characters_added = 0;
            uint32_t characters_removed = 0;
        };

        void EditSource(const Edit& edit);

        const SyntaxBlock* FindBlock(uint32_t text_position) const noexcept;

        const Symbol* FindSymbol(const QString& key) const noexcept
        { return base::SafeFind(mSymbols, key); }

        using BlockList = std::vector<SyntaxBlock>;

        BlockList FindBlocks(uint32_t text_position, uint32_t text_length) const noexcept;

        inline size_t GetNumBlocks() const noexcept
        { return mBlocks.size(); }
        inline const SyntaxBlock& GetBlock(size_t index) const noexcept
        { return base::SafeIndex(mBlocks, index); }
        inline const bool HasParseState() const noexcept
        { return mTree != nullptr; }

        LuaParser& operator=(const LuaParser&) = delete;
    private:
        void QuerySyntax(const QByteArray& source, TSTree* ast);
        void QuerySymbols(const QByteArray& source, TSTree* ast);
        void FindBuiltins(const QByteArray& source);
    private:
        std::vector<SyntaxBlock> mBlocks;
        std::unordered_map<QString, Symbol> mSymbols;
    private:
        TSParser* mParser = nullptr;
        TSQuery* mSyntaxQuery = nullptr;
        TSQuery* mSymbolQuery = nullptr;
        TSTree* mTree = nullptr;
    };

} // namespace
