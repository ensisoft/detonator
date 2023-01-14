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
#include <unordered_set>
#include <set>
#include <optional>

#include "base/assert.h"
#include "base/platform.h"

namespace base
{

template<typename T>
T EvenMultiple(T value, T multiple)
{
    return ((value + multiple - 1) / multiple) * multiple;
}

template<typename T, size_t N> inline
size_t ArraySize(const T (&array)[N])
{ return N; }

inline bool IsPowerOfTwo(unsigned i)
{
    return (i & (i-1)) == 0;
}

inline unsigned NextPOT(unsigned v)
{
    // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

template<typename T> inline
const T* GetOpt(const std::optional<T>& opt)
{
    if (opt.has_value())
        return &opt.value();
    return nullptr;
}
template<typename T> inline
T* GetOpt(std::optional<T>& opt)
{
    if (opt.has_value())
        return &opt.value();
    return nullptr;
}

template<typename Key> inline
bool Contains(const std::unordered_set<Key>& set, const Key& k)
{ return set.find(k) != set.end(); }

template<typename Key> inline
bool Contains(const std::set<Key>& set, const Key& k)
{ return set.find(k) != set.end(); }

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

template<typename T, typename Predicate> inline
void EraseRemove(std::vector<T>& vector, Predicate pred)
{
    // remove shuffles the values in the vector such that the
    // removed items are at the end of the vector and the iterator
    // returned is the first item in the sequence of removed items
    vector.erase(std::remove_if(vector.begin(), vector.end(), pred), vector.end());
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

template<typename T, typename Predicate>
const T* SafeFind(const std::vector<T>& vector, Predicate predicate)
{
    for (auto& item : vector)
    {
        if (predicate(item))
            return &item;
    }
    return nullptr;
}

template<typename T, typename Predicate>
const T* SafeFind(const std::vector<std::unique_ptr<T>>& vector, Predicate predicate)
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

// Get the time in seconds since some undefined point in time i.e. epoch.
// Only intended purpose is to facilitate measuring elapsed time
// between consecutive events. Don't assume any relation to any
// system time etc.
double GetTime();

class ElapsedTimer
{
public:
    // Start measuring the passing of time.
    inline void Start()
    { mStartTime = base::GetTime(); }
    // Return the time in seconds since Start.
    inline double SinceStart()
    { return base::GetTime() - mStartTime; }
    // Return the time in seconds since last Delta.
    inline double Delta()
    {
        const auto now = base::GetTime();
        const auto dt  = now - mDeltaTime;
        mDeltaTime = now;
        return dt;
    }
private:
    bool mStarted = false;
    double mStartTime = 0.0;
    double mDeltaTime = 0.0;
};

// when using these functions with narrow byte strings think about
// the string encoding and whether that can be a problem!
inline bool Contains(const std::string& str, const std::string& what)
{ return str.find(what) != std::string::npos; }
inline bool Contains(const std::wstring& str, const std::wstring& what)
{ return str.find(what) != std::wstring::npos; }
inline bool StartsWith(const std::string& str, const std::string& what)
{ return str.find(what) == 0; }
inline bool StartsWith(const std::wstring& str, const std::wstring& what)
{ return str.find(what) == 0; }
inline bool EndsWith(const std::string& str, const std::string& what)
{
    // SO saves the day..
    if (what.size() > str.size()) return false;
    return std::equal(what.rbegin(), what.rend(), str.rbegin());
}
inline bool EndsWith(const std::wstring& str, const std::wstring& what)
{
    // SO saves the day...
    if (what.size() > str.size()) return false;
    return std::equal(what.rbegin(), what.rend(), str.rbegin());
}

std::string RandomString(size_t len);
std::string ToUtf8(const std::wstring& str);
std::wstring FromUtf8(const std::string& str);
// ToUpper/Lower are only provided for wide strings in order to avoid
// potential issues with multibyte narrow string encoding.
std::wstring ToUpper(const std::wstring& str);
std::wstring ToLower(const std::wstring& str);
std::string ToUpperUtf8(const std::string& str);
std::string ToLowerUtf8(const std::string& str);

std::string JoinPath(const std::string& a, const std::string& b);

std::ifstream OpenBinaryInputStream(const std::string& filename);
std::ofstream OpenBinaryOutputStream(const std::string& filename);
bool OverwriteTextFile(const std::string& file, const std::string& text);
bool FileExists(const std::string& filename);

std::vector<char> LoadBinaryFile(const std::string& filename);

} // base


