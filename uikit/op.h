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

#include "warnpush.h"

#include "warnpop.h"

#include <stack>
#include <utility>
#include <cstddef>

#include "uikit/types.h"

// functions that operate on a widget hierarchy.

namespace uik
{
    namespace detail {
        // Callback interface for visiting the widget hierarchy.
        template<typename T>
        class TVisitor
        {
        public:
            // Called when entering a widget during the widget
            // tree traversal. In pre-order traversal the parent
            // widget will be entered first before its children.
            // Return true to stop further tree traversal and
            // return early.
            virtual bool EnterWidget(T widget)
            { return false; }
            // Called when leaving a widget during the widget
            // tree traversal. In pre-order traversal this is
            // called for any widget after its children have been
            // visited/traversed.
            // Return true to stop further tree traversal and
            // return early.
            virtual bool LeaveWidget(T widget)
            { return false; }
        private:
        };

        template<typename T>
        bool VisitRecursive(TVisitor<T>& visitor , T current_widget)
        {
            if (visitor.EnterWidget(current_widget))
                return true;
            for (size_t i = 0; i < current_widget->GetNumChildren(); ++i)
            {
                if (VisitRecursive(visitor , &current_widget->GetChild(i)))
                    return true;
            }
            return visitor.LeaveWidget(current_widget);
        }
    } // detail

    template<typename T, typename Function>
    void ForEachWidget(Function callback, T start_widget)
    {
        class PrivateVisitor : public detail::TVisitor<T> {
        public:
            PrivateVisitor(Function func) : mFunc(std::move(func))
            {}
            virtual bool EnterWidget(T widget) override
            {
                mFunc(widget);
                return false;
            }
        private:
            Function mFunc;
        };
        PrivateVisitor pv(std::move(callback));
        detail::VisitRecursive<T>(pv, start_widget);
    }

    template<typename T, typename Predicate>
    T FindWidget(Predicate predicate, T start_widget)
    {
        class PrivateVisitor : public detail::TVisitor<T> {
        public:
            PrivateVisitor(Predicate predicate) : mPredicate(std::move(predicate))
            {}
            virtual bool EnterWidget(T widget) override
            {
                if (mPredicate(widget))
                    mResult = widget;
                return mResult != nullptr;
            }
            T GetResult() const
            { return mResult; }
        private:
            Predicate mPredicate;
            T mResult = nullptr;
        };
        PrivateVisitor pv(std::move(predicate));
        detail::VisitRecursive<T>(pv, start_widget);
        return pv.GetResult();
    }

    namespace detail {
        template<typename T>
        T FindParentRecursive(T child, T current, T parent)
        {
            if (child == current)
                return parent;
            for (size_t i=0; i<current->GetNumChildren(); ++i)
            {
                auto& ascendant = current->GetChild(i);
                auto ret = FindParentRecursive(child, &ascendant, current);
                if (ret) return ret;
            }
            return nullptr;
        }
    } // detail

    template<typename T>
    T FindParent(T child, T start_widget)
    {
        return detail::FindParentRecursive<T>(child, start_widget, nullptr);
    }

    template<typename T>
    FRect FindWidgetRect(T which, T start_widget)
    {
        class PrivateVisitor : public detail::TVisitor<T> {
        public:
            PrivateVisitor(T widget) : mWidget(widget)
            {}
            virtual bool EnterWidget(T widget) override
            {
                if (mWidget == widget)
                {
                    FRect rect = widget->GetRect();
                    rect.Translate(widget->GetPosition());
                    rect.Translate(mWidgetOrigin);
                    mRect = rect;
                    return true;
                }
                mWidgetOrigin += widget->GetPosition();
                return false;
            }
            virtual bool LeaveWidget(T widget) override
            {
                mWidgetOrigin -= widget->GetPosition();
                return false;
            }
            FRect GetRect() const
            { return mRect; }
        private:
            T mWidget = nullptr;
            FRect mRect;
            FPoint mWidgetOrigin;
        };
        PrivateVisitor visitor(which);
        detail::VisitRecursive<T>(visitor, start_widget);
        return visitor.GetRect();
    }

} // namespace