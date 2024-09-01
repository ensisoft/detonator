// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include <deque>
#include <stack>
#include <mutex>
#include <iterator>
#include <type_traits>

#include "base/assert.h"
#include "base/utility.h"

namespace base
{
    namespace detail {

        // suppress warning about non-standard extension
        // regarding the zero length array
#if defined(__MSVC__)
#  pragma warning(push)
#  pragma warning(disable: 4200)
#endif
        struct Memory {
            enum Flags {
                Created = 0x1
            };
            uint32_t flags = 0;
            char data[]; // non-standard
        };
#if defined(__MSVC__)
#  pragma warning(pop)
#endif

        class ObjectAllocator {
        public:
            [[nodiscard]]
            virtual Memory* Allocate(size_t index)  = 0;
            [[nodiscard]]
            virtual Memory* Get(size_t index) noexcept = 0;
        };

        template<typename T>
        class TypedAllocator : public ObjectAllocator {
        public:
            inline size_t GetSize() const noexcept
            { return mObjects.size(); }

            [[nodiscard]]
            virtual Memory* Allocate(size_t index)  override
            {
                for (size_t i=mObjects.size(); i <= index; ++i)
                    mObjects.emplace_back(Object {0});

                ASSERT(index < mObjects.size());

                auto& object = mObjects[index];
                return reinterpret_cast<Memory*>(&object);
            }
            [[nodiscard]]
            virtual Memory* Get(size_t index) noexcept override
            {
                if (index >= mObjects.size())
                    return nullptr;

                auto& object = mObjects[index];
                return reinterpret_cast<Memory*>(&object);
            }
        private:
            static auto constexpr Padding = sizeof(T) % sizeof(intptr_t);
            static auto constexpr AlignedSize = sizeof(T) + (sizeof(intptr_t) - Padding);

            struct Object {
                uint32_t flags = 0;
                char memory[AlignedSize];
            };

            // important to use deque here so that push_back doesn't
            // invalidate any previous iterators (pointers)
            std::deque<Object> mObjects;
        };

        template<size_t N, typename... Ts> using NthTypeOf =
        typename std::tuple_element<N, std::tuple<Ts...>>::type;

        template<size_t Index, typename T, typename... Types> constexpr
        size_t IndexFromType()
        {
            static_assert(Index < sizeof...(Types));

            using NthType = NthTypeOf<Index, Types...>;

            if constexpr (std::is_same_v<NthType, T>)
                return Index;
            else if constexpr (Index + 1 < sizeof...(Types))
                return IndexFromType<Index + 1, T, Types...>();
            return 0;
        }

        template<typename... Rest>
        struct Allocator;

        template<typename T, typename... Rest>
        struct Allocator<T, Rest...>
        {
            using ThisAllocatorType = TypedAllocator<T>;
            using NextType = Allocator<Rest...>;
            using DataType = T;

            template<typename F>
            [[nodiscard]] Memory* Allocate(size_t index) {
                if constexpr (std::is_same_v<F, DataType>) {
                    return this_allocator.Allocate(index);
                } else {
                    return next.template Allocate<F>(index);
                }
            }

            template<typename F>
            [[nodiscard]] Memory* Get(size_t index) noexcept {
                if constexpr (std::is_same_v<F, DataType>) {
                    return this_allocator.Get(index);
                } else {
                    return next.template Get<F>(index);
                }
            }
            void Destroy(size_t index) noexcept {
                auto* memory = this_allocator.Get(index);
                if (memory && (memory->flags & Memory::Flags::Created)) {
                    auto* object = reinterpret_cast<DataType*>(memory->data);
                    object->~DataType();
                    memory->flags &= ~Memory::Flags::Created;
                }
                next.Destroy(index);
            }

            ThisAllocatorType this_allocator;
            NextType next;
        };

        template<typename T>
        struct Allocator<T>
        {
            using ThisAllocatorType = TypedAllocator<T>;
            using DataType = T;

            template<typename F>
            [[nodiscard]] Memory* Allocate(size_t index) {
                static_assert(std::is_same_v<F, T>, "oops");
                return this_allocator.Allocate(index);
            }
            template<typename F>
            [[nodiscard]] Memory* Get(size_t index) noexcept {
                static_assert(std::is_same_v<F, T>, "oops");
                return this_allocator.Get(index);
            }
            void Destroy(size_t index) noexcept {
                auto* memory = this_allocator.Get(index);
                if (memory && (memory->flags & Memory::Flags::Created)) {
                    auto* object = reinterpret_cast<DataType*>(memory->data);
                    object->~DataType();
                    memory->flags &= ~Memory::Flags::Created;
                }
            }

            ThisAllocatorType this_allocator;
        };


    }// detail

    template<class... Types>
    class Allocator
    {
    public:
        // Number of types the allocator can allocate
        static constexpr auto TypeCount = sizeof...(Types);
        static constexpr auto AllocatorCount = sizeof...(Types);

        Allocator() = default;
        Allocator(const Allocator& other) = delete;

       ~Allocator()
        {
            ASSERT(mCount == 0);
        }

        inline size_t GetCount() const noexcept
        { return mCount; }

        inline size_t GetHighIndex() const noexcept
        { return mHighIndex; }

        template<typename Type>
        Type* GetObject(size_t index) noexcept
        {
            auto* memory = mAllocators.template Get<Type>(index);
            if (memory && (memory->flags & detail::Memory::Flags::Created))
                return reinterpret_cast<Type*>(memory->data);

            return nullptr;
        }

        [[nodiscard]]
        size_t GetNextIndex() noexcept
        {
            ++mCount;

            if (mFreeIndices.empty())
                return mHighIndex++;

            auto index = mFreeIndices.top();
            mFreeIndices.pop();
            return index;
        }
        void FreeIndex(size_t index) noexcept
        {
            ASSERT(mCount > 0);
            --mCount;

            mFreeIndices.push(index);
        }

        template<typename Type, typename... Args>
        Type* CreateObject(size_t index, Args&&... args) 
        {
            auto* memory = mAllocators.template Allocate<Type>(index);
            ASSERT((memory->flags & detail::Memory::Flags::Created) == 0);
            Type* object = new (memory->data) Type(std::forward<Args>(args)...);
            memory->flags = detail::Memory::Flags::Created;
            return object;
        }

        template<typename Type>
        void DestroyObject(size_t index, const Type* carcass) noexcept
        {
            auto* memory = mAllocators.template Get<Type>(index);
            ASSERT(memory);
            ASSERT(memory->flags & detail::Memory::Flags::Created);
            ASSERT((void*)memory->data == (void*)carcass);

            carcass->~Type();

            memory->flags &= ~detail::Memory::Flags::Created;
        }

        void DestroyAll(size_t index) noexcept
        {
            mAllocators.Destroy(index);
        }

        // for testing mostly.
        void Cleanup() noexcept
        {
            for (size_t i=0; i<mHighIndex; ++i)
                DestroyAll(i);

            mCount = 0;
            mHighIndex = 0;
        }
        auto& GetMutex() const noexcept
        { return mMutex; }

        Allocator& operator=(const Allocator&) = delete;
    private:
        mutable std::mutex mMutex;
        detail::Allocator<Types...> mAllocators;
        std::stack<size_t> mFreeIndices;
        std::size_t mHighIndex = 0;
        std::size_t mCount = 0;
    };


    template<class T, class... Types>
    class AllocatorSequence
    {
    public:
        using AllocatorType = Allocator<Types...>;
        using ObjectType = T;
        using value_type = T;
        using pointer    = T*;
        using reference  = T&;
        using size_type  = size_t;
        using difference_type = std::ptrdiff_t;

        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;

            iterator(size_t index, AllocatorType* allocator) noexcept
              : mIndex(index)
              , mAllocator(allocator)
            {}
            inline iterator& operator++() noexcept
            {
                scan_next();
                return *this;
            }
            inline iterator operator++(int) noexcept
            {
                iterator old(mIndex, mAllocator);
                scan_next();
                return old;
            }
            inline T& operator*() noexcept
            {
                return *mAllocator->template GetObject<T>(mIndex);
            }
            inline T* operator->() noexcept
            {
                return mAllocator->template GetObject<T>(mIndex);
            }

            inline bool operator==(const iterator& other) const noexcept
            {
                return mIndex == other.mIndex;
            }
            inline bool operator!=(const iterator& other) const noexcept
            {
                return mIndex != other.mIndex;
            }
        private:
            void scan_next() noexcept
            {
                do {
                    if (mAllocator->template GetObject<T>(++mIndex))
                        break;
                } while (mIndex < mAllocator->GetHighIndex());
            }
        private:
            size_t mIndex = 0;
            AllocatorType* mAllocator = nullptr;
        };

        explicit AllocatorSequence(AllocatorType* allocator) noexcept
          : mAllocator(allocator)
        {}

        inline iterator begin() noexcept
        {
            return iterator { 0, mAllocator };
        }
        inline iterator end() noexcept
        {
            return iterator { mAllocator->GetHighIndex(), mAllocator };
        }
        inline size_type size() const noexcept
        {
            return mAllocator->GetHighIndex();
        }
        inline size_type max_size() const noexcept
        {
            return mAllocator->GetHighIndex();
        }
        inline void push_back(T* value) noexcept
        {
            ASSERT("NOT SUPPORTED");
        }
        inline bool empty() const noexcept
        {
            return mAllocator->GetCount() == 0;
        }
    private:
        AllocatorType* mAllocator;
    };

} // namespace

