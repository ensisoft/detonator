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
    enum class LuaCodeBlockType {
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


    class LuaTheme
    {
    public:
        enum class Theme {
            Monokai
        };
        using Key = LuaCodeBlockType;

        void SetTheme(Theme theme);

        const QColor* GetColor(Key key) const noexcept;
    private:
        std::unordered_map<Key, QColor> mTable;
    };

    class LuaParser
    {
    public:
        using BlockType = LuaCodeBlockType;

        struct CodeBlock {
            BlockType type = BlockType::Other;
            // character position of the highlight in the current document.
            uint32_t start = 0;
            // length of the highlight in characters
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

        const CodeBlock* FindBlock(uint32_t text_position) const noexcept;

        using BlockList = std::vector<CodeBlock>;

        BlockList FindBlocks(uint32_t text_position, uint32_t text_length) const noexcept;

        size_t GetNumBlocks() const
        { return mBlocks.size(); }
        const CodeBlock& GetBlock(size_t index) const
        { return mBlocks[index]; }

        const bool HasParseState() const noexcept
        { return mTree != nullptr; }

        LuaParser& operator=(const LuaParser&) = delete;
    private:
        void ConsumeTree(TSTree* ast);
        void FindBuiltins(const QString& source);
    private:
        std::vector<CodeBlock> mBlocks;
    private:
        TSParser* mParser = nullptr;
        TSQuery* mQuery = nullptr;
        TSTree* mTree = nullptr;
    };

} // namespace
