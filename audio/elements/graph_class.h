// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include <string>
#include <vector>

#include "data/fwd.h"
#include "audio/element.h"
#include "audio/elements//element.h"

namespace audio
{

    class GraphClass
    {
    public:
        using ElementArg = audio::ElementArg;
        using Element    = audio::ElementCreateArgs;

        struct Link {
            std::string id;
            std::string src_element;
            std::string src_port;
            std::string dst_element;
            std::string dst_port;
        };
        GraphClass() = default;
        GraphClass(std::string name, std::string id);
        explicit GraphClass(std::string name);

        const Element& AddElement(const Element& element)
        {
            mElements.push_back(element);
            return mElements.back();
        }
        const Element& AddElement(Element&& element)
        {
            mElements.push_back(std::move(element));
            return mElements.back();
        }
        void AddLink(const Link& edge)
        { mLinks.push_back(edge); }
        void AddLink(Link&& edge)
        { mLinks.push_back(std::move(edge)); }
        void SetName(const std::string& name)
        { mName = name; }
        void SetGraphOutputElementId(const std::string& id)
        { mSrcElemId = id; }
        void SetGraphOutputElementPort(const std::string& name)
        { mSrcElemPort = name; }
        std::size_t GetHash() const;
        std::string GetName() const
        { return mName; }
        std::string GetGraphOutputElementId() const
        { return mSrcElemId; }
        std::string GetGraphOutputElementPort() const
        { return mSrcElemPort; }
        std::string GetId() const
        { return mId; }
        std::size_t GetNumElements() const
        { return mElements.size(); }
        std::size_t GetNumLinks() const
        { return mLinks.size(); }
        const Element& GetElement(size_t index) const
        { return base::SafeIndex(mElements, index); }
        const Link& GetLink(size_t index) const
        { return base::SafeIndex(mLinks, index); }
        const Element* FindElementById(const std::string& id) const
        {
            return base::SafeFind(mElements, [&id](const auto& elem) {
                return elem.id == id;
            });
        }
        Element& GetElement(size_t index)
        { return base::SafeIndex(mElements, index); }
        Element* FindElementById(const std::string& id)
        {
            return base::SafeFind(mElements, [&id](const auto& elem) {
                return elem.id == id;
            });
        }
        const Link* FindLinkById(const std::string& id) const
        {
            return base::SafeFind(mLinks, [&id](const auto& link) {
                return link.id == id;
            });
        }
        Link& GetLink(size_t index)
        { return base::SafeIndex(mLinks, index); }
        Link* FindLinkById(const std::string& id)
        {
            return base::SafeFind(mLinks, [&id](const auto& link) {
                return link.id == id;
            });
        }
        std::unique_ptr<GraphClass> Copy() const
        { return std::make_unique<GraphClass>(*this); }
        std::unique_ptr<GraphClass> Clone() const
        {
            auto ret = std::make_unique<GraphClass>(*this);
            ret->mId = base::RandomString(10);
            return ret;
        }

        struct PreloadParams {
            bool enable_pcm_caching = false;
        };

        void Preload(const Loader& loader, const PreloadParams& params) const;

        void IntoJson(data::Writer& writer) const;
        bool FromJson(const data::Reader& reader);
    private:
        std::string mName;
        std::string mId;
        std::string mSrcElemId;
        std::string mSrcElemPort;
        std::vector<Link> mLinks;
        std::vector<Element> mElements;
    };

} // namespace