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

namespace audio
{
    // this silly construct is ad-hoc solution to the problem
    // that std::any can only work with copyable types.
    template<unsigned>
    class Message
    {
    public:
        virtual ~Message() = default;
        template<typename CmdT> inline
        bool HasType() const
        { return GetPtr(typeid(CmdT)) != nullptr; }
        template<typename CmdT> inline
        CmdT* GetIf()
        {
            if (auto* p = GetPtr(typeid(CmdT)))
                return static_cast<CmdT*>(p);
            return nullptr;
        }
        template<typename CmdT> inline
        const CmdT* GetIf() const
        {
            if (const auto* ptr = GetPtr(typeid(CmdT)))
                return static_cast<const CmdT*>(ptr);
            return nullptr;
        }
    protected:
        virtual void* GetPtr(const std::type_info& type) = 0;
        virtual const void* GetPtr(const std::type_info& type) const = 0;
    private:
    };

    namespace detail {
        template<typename T, unsigned D>
        class MessageImpl : public Message<D> {
        public:
            MessageImpl() = default;
            MessageImpl(T&& msg) : mMsg(std::move(msg))
            {}
            T* operator->()
            { return &mMsg; }
            const T* operator->() const
            { return &mMsg; }
        protected:
            virtual void* GetPtr(const std::type_info& type) override
            {
                if (typeid(mMsg) == type) return &mMsg;
                return nullptr;
            }
            virtual const void* GetPtr(const std::type_info& type) const override
            {
                if (typeid(mMsg) == type) return &mMsg;
                return nullptr;
            }
        private:
            T mMsg;
        };
    } // namespace detail

    using Command = Message<0>;
    using Event   = Message<1>;

    template<typename CmdT> inline
    std::unique_ptr<Command> MakeCommand(CmdT&& cmd)
    {
        using MsgType = detail::MessageImpl<std::remove_reference_t<CmdT>, 0>;
        return std::make_unique<MsgType>(std::forward<CmdT>(cmd));
    }
    template<typename CmdT, typename... Args> inline
    std::unique_ptr<Command> MakeCommand(Args&&... args)
    {
        using MsgType = detail::MessageImpl<std::remove_reference_t<CmdT>, 0>;
        return std::make_unique<MsgType>(std::forward<CmdT>(args...));
    }

    template<typename EventT> inline
    std::unique_ptr<Event> MakeEvent(EventT&& cmd)
    {
        using MsgType = detail::MessageImpl<std::remove_reference_t<EventT>, 1>;
        return std::make_unique<MsgType>(std::forward<EventT>(cmd));
    }

} // namespace
