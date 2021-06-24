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

#include <memory> // for unique_ptr
#include <string>
#include <vector>
#include <iosfwd>
#include <algorithm>
#include <iterator>
#include <unordered_map>

#include "base/assert.h"
#include "base/platform.h"

namespace base
{

template<typename T>
void AppendVector(std::vector<T>& head, const std::vector<T>& tail)
{
    std::copy(tail.begin(), tail.end(), std::back_inserter(head));
}

template<typename T>
void AppendVector(std::vector<T>& head, std::vector<T>&& tail)
{
    head.insert(head.end(), std::make_move_iterator(tail.begin()),
                std::make_move_iterator(tail.end()));
}

template<typename K, typename T>
T* SafeFind(std::unordered_map<K, T>& map, const K& key)
{
    auto it = map.find(key);
    if (it == map.end())
        return nullptr;
    return &it->second;
}

template<typename K, typename T>
T* SafeFind(std::unordered_map<K, std::unique_ptr<T>>& map, const K& key)
{
    auto it = map.find(key);
    if (it == map.end())
        return nullptr;
    return it->second.get();
}
template<typename K, typename T>
const T* SafeFind(const std::unordered_map<K, T>& map, const K& key)
{
    auto it = map.find(key);
    if (it == map.end())
        return nullptr;
    return &it->second;
}

template<typename K, typename T>
const T* SafeFind(const std::unordered_map<K, std::unique_ptr<T>>& map, const K& key)
{
    auto it = map.find(key);
    if (it == map.end())
        return nullptr;
    return it->second.get();
}


template<typename T>
T& SafeIndex(std::vector<T>& vector, std::size_t index)
{
    ASSERT(index < vector.size());
    return vector[index];
}
template<typename T>
const T& SafeIndex(const std::vector<T>& vector, std::size_t index)
{
    ASSERT(index < vector.size());
    return vector[index];
}
template<typename T>
void SafeErase(std::vector<T>& vector, std::size_t index)
{
    ASSERT(index < vector.size());
    vector.erase(vector.begin() + index);
}

template<typename T, typename Predicate>
T* SafeFind(std::vector<T>& vector, Predicate predicate)
{
    for (auto& item : vector)
    {
        if (predicate(item))
            return &item;
    }
    return nullptr;
}

template<typename T, typename Predicate>
T* SafeFind(std::vector<std::unique_ptr<T>>& vector, Predicate predicate)
{
    for (auto& item : vector)
    {
        if (predicate(item))
            return item.get();
    }
    return nullptr;
}

template<typename T, typename Deleter>
std::unique_ptr<T, Deleter> MakeUniqueHandle(T* ptr, Deleter del)
{ return std::unique_ptr<T, Deleter>(ptr, del); }

// get the time in seconds since the first call to this function.
double GetRuntimeSec();

inline bool Contains(const std::string& str, const std::string& what)
{ return str.find(what) != std::string::npos; }
inline bool StartsWith(const std::string& str, const std::string& what)
{ return str.find(what) == 0; }

std::string RandomString(size_t len);
std::string ToUtf8(const std::wstring& str);
std::wstring FromUtf8(const std::string& str);
std::wstring ToUpper(const std::wstring& str);
std::wstring ToLower(const std::wstring& str);
std::wstring Widen(const std::string& str);
std::string JoinPath(const std::string& a, const std::string& b);

std::ifstream OpenBinaryInputStream(const std::string& filename);
std::ofstream OpenBinaryOutputStream(const std::string& filename);
bool OverwriteTextFile(const std::string& file, const std::string& text);
bool FileExists(const std::string& filename);

} // base


