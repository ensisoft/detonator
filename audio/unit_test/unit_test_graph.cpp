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

#include "config.h"

#include <iostream>
#include <string>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/logging.h"
#include "audio/element.h"
#include "audio/graph.h"

class TestBuffer : public audio::Buffer
{
public:
    virtual Format GetFormat() const
    {
        Format format;
        return format;
    }
    virtual const void* GetPtr() const
    { return mData.data(); }
    virtual void* GetPtr()
    { return mData.data(); }
    virtual size_t GetByteSize() const override
    { return mData.size(); }

    void AppendTag(const std::string& tag)
    {
        mData += tag;
    }
private:
    std::string mData;
};

class TestPort : public audio::Port
{
public:
    TestPort(const std::string& name)
        : mName(name)
    {}

    virtual bool PushBuffer(BufferHandle buffer) override
    {
        if (mBuffer) return false;
        mBuffer = buffer;
        return true;
    }
    virtual bool PullBuffer(BufferHandle& buffer) override
    {
        if (!mBuffer) return false;

        buffer = mBuffer;
        mBuffer.reset();
        return true;
    }
    virtual std::string GetName() const override
    { return mName; }
    virtual Format GetFormat() const override
    { return mFormat; }
    virtual void SetFormat(const Format& format) override
    { mFormat = format; }

    virtual bool CanAccept(const Format& format) const override
    {
        return true;
    }
    virtual bool HasBuffers() const override
    {
        return !!mBuffer;
    }
private:
    const std::string mName;
    BufferHandle mBuffer;
    Format mFormat;
};

struct TestState  {
    std::vector<audio::Element*> prepare_list;
    std::vector<audio::Element*> process_list;
    std::string ConcatPrepareNames() const
    {
        std::string ret;
        for (auto* e : prepare_list)
            ret += e->GetName();
        return ret;
    }
};

class SrcElement : public audio::Element
{
public:
    SrcElement(const std::string& id, const std::string& name)
      : mId(id)
      , mName(name)
      , mOut("out")
    {}
    SrcElement(const std::string& id, const std::string& name, unsigned buffers)
      : mId(id)
      , mName(name)
      , mShouldFinish(true)
      , mNumOutBuffers(buffers)
      , mOut("out")
    {}
    virtual std::string GetId() const override
    { return mId; }
    virtual std::string GetName() const override
    { return mName; }
    virtual bool IsSourceDone() const override
    {
        if (!mShouldFinish) return false;
        if (mNumOutBuffers == mBufferCount)
            return true;
        return false;
    }
    virtual bool IsSource() const override
    { return true; }
    virtual void Process(EventQueue& events, unsigned milliseconds) override
    {
        if (mShouldFinish)
            TEST_REQUIRE(mBufferCount < mNumOutBuffers);

        auto buffer = std::make_shared<TestBuffer>();
        mOut.PushBuffer(buffer);
        ++mBufferCount;
    }
    virtual unsigned GetNumOutputPorts() const override
    { return 1; }
    virtual audio::Port& GetOutputPort(unsigned index)
    { return mOut; }
private:
    const std::string mId;
    const std::string mName;
    const bool mShouldFinish = false;
    const unsigned mNumOutBuffers = 0;
    unsigned mBufferCount = 0;
    TestPort mOut;
};

class TestElement : public audio::Element
{
public:
    TestElement(const std::string& id, const std::string& name, TestState& state)
      : mId(id)
      , mName(name)
      , mState(state)
    {}
    virtual std::string GetId() const override
    { return mId; }
    virtual std::string GetName() const override
    { return mName; }
    virtual bool Prepare() override
    {
        mState.prepare_list.push_back(this);
        return !mPrepareError;
    }
    virtual void Process(EventQueue& events, unsigned milliseconds) override
    {
        TEST_REQUIRE(mInputPorts.size() == mOutputPorts.size());
        for (unsigned i=0; i<mInputPorts.size(); ++i)
        {
            audio::BufferHandle handle;
            mInputPorts[i]->PullBuffer(handle);
            if (handle == nullptr)
                continue;
            auto* test = dynamic_cast<TestBuffer*>(handle.get());
            test->AppendTag(base::FormatString("-> %1:%2 ", mName, mInputPorts[i]->GetName()));
            test->AppendTag(base::FormatString("-> %1:%2 ", mName, mOutputPorts[i]->GetName()));
            mOutputPorts[i]->PushBuffer(handle);
        }
    }

    virtual unsigned GetNumInputPorts() const override
    { return mInputPorts.size(); }
    virtual unsigned GetNumOutputPorts() const override
    { return mOutputPorts.size(); }
    virtual audio::Port& GetInputPort(unsigned index) override
    { return *base::SafeIndex(mInputPorts, index); }
    virtual audio::Port& GetOutputPort(unsigned index) override
    { return *base::SafeIndex(mOutputPorts, index); }

    void AddInputPort(std::unique_ptr<audio::Port> port)
    { mInputPorts.push_back(std::move(port)); }
    void AddOutputPort(std::unique_ptr<audio::Port> port)
    { mOutputPorts.push_back(std::move(port)); }

    void AddOutputPort(const std::string& name, const audio::Format& format)
    {
        auto p = std::make_unique<TestPort>(name);
        p->SetFormat(format);
        mOutputPorts.push_back(std::move(p));
    }

    void AddInputPort(const std::string& name)
    {
        auto p = std::make_unique<TestPort>(name);
        mInputPorts.push_back(std::move(p));
    }

    void SetPrepareError(bool error)
    { mPrepareError = error; }
private:
    const std::string mId;
    const std::string mName;
    const bool mFailPrepare = false;
    TestState& mState;
    std::vector<std::unique_ptr<audio::Port>> mInputPorts;
    std::vector<std::unique_ptr<audio::Port>> mOutputPorts;
    bool mPrepareError = false;
};

void Link(audio::Graph& graph,
          const std::string& src_elem_name, const std::string& src_port_name,
          const std::string& dst_elem_name, const std::string& dst_port_name)
{
    auto* src_elem = graph.FindElementByName(src_elem_name);
    auto* dst_elem = graph.FindElementByName(dst_elem_name);
    TEST_REQUIRE(src_elem);
    TEST_REQUIRE(dst_elem);
    auto* src_port = src_elem->FindOutputPortByName(src_port_name);
    auto* dst_port = dst_elem->FindInputPortByName(dst_port_name);
    TEST_REQUIRE(src_port);
    TEST_REQUIRE(dst_port);
    graph.LinkElements(src_elem, src_port, dst_elem, dst_port);
}

void Link(audio::Graph& graph, const std::string& src_elem_id, const std::string& src_port_name)
{
    auto* src_elem = graph.FindElementById(src_elem_id);
    auto* src_port = src_elem->FindOutputPortByName(src_port_name);
    TEST_REQUIRE(src_elem);
    TEST_REQUIRE(src_port);
    graph.LinkGraph(src_elem, src_port);
}

void unit_test_basic()
{
    class TestElement : public audio::Element
    {
    public:
        TestElement(const std::string& id, const std::string& name, TestState state)
          : mId(id)
          , mName(name)
        {}
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
    private:
        const std::string mId;
        const std::string mName;
    };

    audio::Graph graph("foo");
    TEST_REQUIRE(graph.GetName() == "foo");
    TEST_REQUIRE(graph.GetNumElements() == 0);
    TEST_REQUIRE(graph.FindElementByName("foo") == nullptr);
    TEST_REQUIRE(graph.FindElementById("bar") == nullptr);

    TestState state;
    graph.AddElementPtr(std::make_unique<TestElement>("1", "one", state));
    graph.AddElementPtr(std::make_unique<TestElement>("2", "two", state));
    TEST_REQUIRE(graph.GetNumElements() == 2);
    TEST_REQUIRE(graph.GetElement(0).GetName() == "one");
    TEST_REQUIRE(graph.GetElement(1).GetName() == "two");
    TEST_REQUIRE(graph.FindElementByName("one"));
    TEST_REQUIRE(graph.FindElementById("2"));
}

void unit_test_prepare_topologies()
{
    // single audio element
    {
        TestState state;

        audio::Graph graph("joo");
        auto elem = std::make_unique<TestElement>("foo", "foo", state);
        auto port = std::make_unique<TestPort>("src");
        audio::Format format;
        format.sample_rate   = 77777;
        format.channel_count = 5;
        format.sample_type   = audio::SampleType::Float32;
        port->SetFormat(format);
        elem->AddOutputPort(std::move(port));

        graph.AddElementPtr(std::move(elem));
        Link(graph, "foo", "src");
        TEST_REQUIRE(graph.Prepare());
        TEST_REQUIRE(graph.GetFormat() == format);
        TEST_REQUIRE(state.prepare_list.size() == 1);
        TEST_REQUIRE(state.prepare_list[0]->GetName() == "foo");
    }

    // two root nodes link to a 3rd node.
    {
        TestState state;

        audio::Format format;
        format.sample_rate   = 77777;
        format.channel_count = 5;
        format.sample_type   = audio::SampleType::Float32;

        audio::Graph graph("joo");
        auto a = std::make_unique<TestElement>("a", "a", state);
        auto b = std::make_unique<TestElement>("b", "b", state);
        auto c = std::make_unique<TestElement>("c", "c", state);
        a->AddOutputPort("out", format);
        b->AddOutputPort("out", format),
        c->AddInputPort("in0");
        c->AddInputPort("in1");
        c->AddOutputPort("out", format);

        graph.AddElementPtr(std::move(a));
        graph.AddElementPtr(std::move(c));
        graph.AddElementPtr(std::move(b));
        Link(graph, "a", "out", "c", "in0");
        Link(graph, "b", "out", "c", "in1");
        TEST_REQUIRE(graph.Prepare());

        std::string topo = state.ConcatPrepareNames();
        TEST_REQUIRE(topo == "abc" || topo == "bac");
    }

    //
    {
        TestState state;

        audio::Format format;
        format.sample_rate   = 77777;
        format.channel_count = 5;
        format.sample_type   = audio::SampleType::Float32;

        audio::Graph graph("joo");
        auto a = std::make_unique<TestElement>("a", "a", state);
        auto b = std::make_unique<TestElement>("b", "b", state);
        auto c = std::make_unique<TestElement>("c", "c", state);
        auto d = std::make_unique<TestElement>("d", "d", state);
        auto e = std::make_unique<TestElement>("e", "e", state);
        a->AddOutputPort("out0", format);
        a->AddOutputPort("out1", format);

        b->AddInputPort("in");
        b->AddOutputPort("out", format);

        c->AddInputPort("in");
        c->AddOutputPort("out0", format);
        c->AddOutputPort("out1", format);

        d->AddInputPort("in");
        d->AddOutputPort("out", format);

        e->AddInputPort("in0");
        e->AddInputPort("in1");
        e->AddInputPort("in2");
        e->AddOutputPort("out", format);

        graph.AddElementPtr(std::move(b));
        graph.AddElementPtr(std::move(d));
        graph.AddElementPtr(std::move(e));
        graph.AddElementPtr(std::move(a));
        graph.AddElementPtr(std::move(c));

        Link(graph, "a", "out0", "b", "in");
        Link(graph, "a", "out1", "c", "in");
        Link(graph, "b", "out", "e", "in0");
        Link(graph, "c", "out0", "e", "in1");
        Link(graph, "c", "out1", "d", "in");
        Link(graph, "d", "out", "e", "in2");
        TEST_REQUIRE(graph.Prepare());

        std::string topo = state.ConcatPrepareNames();
        TEST_REQUIRE(topo == "abcde" || topo == "acbde");
    }
}

void unit_test_buffer_flow()
{
    TestState state;

    audio::Format format;
    format.sample_rate   = 8000;
    format.channel_count = 1;
    format.sample_type   = audio::SampleType::Int16;

    audio::Graph graph("joo");
    auto s = std::make_unique<SrcElement>("s", "s");
    auto a = std::make_unique<TestElement>("a", "a", state);
    auto b = std::make_unique<TestElement>("b", "b", state);
    a->AddInputPort("in");
    a->AddOutputPort("out", format);
    b->AddInputPort("in");
    b->AddOutputPort("out", format);

    graph.AddElementPtr(std::move(s));
    graph.AddElementPtr(std::move(a));
    graph.AddElementPtr(std::move(b));
    Link(graph, "s", "out", "a", "in");
    Link(graph, "a", "out", "b", "in");
    Link(graph, "b", "out");
    TEST_REQUIRE(graph.Prepare());

    audio::Element::EventQueue  queue;
    audio::BufferHandle buffer;
    graph.Process(queue, 1);
    graph.GetOutputPort(0).PullBuffer(buffer);

    const std::string outcome((const char*)buffer->GetPtr(), buffer->GetByteSize());
    //std::cout << outcome;
    TEST_REQUIRE(outcome == "-> a:in -> a:out -> b:in -> b:out ");
}

void unit_test_completion()
{
    // test completion with just a source element
    {
        audio::Format format;
        format.sample_type   = audio::SampleType::Int32;
        format.channel_count = 2;
        format.sample_rate   = 16000;

        audio::Graph graph("test");
        auto* src = graph.AddElement(SrcElement("src", "src", 10));
        src->GetOutputPort(0).SetFormat(format);
        TEST_REQUIRE(graph.LinkGraph("src", "out"));
        TEST_REQUIRE(graph.Prepare());

        audio::Element::EventQueue queue;

        for (int i=0; i<10; ++i)
        {
            graph.Process(queue, 1);
            auto& port = graph.GetOutputPort(0);
            audio::BufferHandle  buffer;
            TEST_REQUIRE(port.PullBuffer(buffer));
        }
        TEST_REQUIRE(graph.IsSourceDone());
    }

    // test completion with 2 sources.
    {
        audio::Format format;
        format.sample_type   = audio::SampleType::Int32;
        format.channel_count = 2;
        format.sample_rate   = 16000;

        TestState state;

        audio::Graph graph("test");
        auto* src0  = graph.AddElement(SrcElement("src0", "src0", 10));
        auto* src1  = graph.AddElement(SrcElement("src1", "src1", 20));
        auto* test = graph.AddElement(TestElement("test", "test", state));
        test->AddInputPort("in0");
        test->AddInputPort("in1");
        test->AddOutputPort("out0", format);
        test->AddOutputPort("out1", format);
        src0->GetOutputPort(0).SetFormat(format);
        src1->GetOutputPort(0).SetFormat(format);
        TEST_REQUIRE(graph.LinkElements("src0", "out", "test", "in0"));
        TEST_REQUIRE(graph.LinkElements("src1", "out", "test", "in1"));
        TEST_REQUIRE(graph.LinkGraph("test", "out0"));
        TEST_REQUIRE(graph.Prepare());

        audio::Element::EventQueue queue;

        std::size_t bytes_read = 0;
        for (int i=0; i<10; ++i)
        {
            graph.Process(queue, 1);
        }
        TEST_REQUIRE(graph.IsSourceDone() == false);
        TEST_REQUIRE(src0->IsSourceDone() == true);
        TEST_REQUIRE(src1->IsSourceDone() == false);

        for (int i=0; i<10; ++i)
        {
            graph.Process(queue, 1);
        }
        TEST_REQUIRE(graph.IsSourceDone() == true);
        TEST_REQUIRE(src0->IsSourceDone() == true);
        TEST_REQUIRE(src1->IsSourceDone() == true);

    }
}

void unit_test_graph_in_graph()
{
    TestState state;

    audio::Format format;
    format.sample_rate   = 77777;
    format.channel_count = 5;
    format.sample_type   = audio::SampleType::Float32;

    auto sub_graph = std::make_unique<audio::Graph>("sub-graph");
    auto s = std::make_unique<SrcElement>("s", "s");
    auto a = std::make_unique<TestElement>("a", "a", state);
    auto b = std::make_unique<TestElement>("b", "b", state);
    a->AddInputPort("in");
    a->AddOutputPort("out", format);
    b->AddInputPort("in");
    b->AddOutputPort("out", format);

    sub_graph->AddElementPtr(std::move(s));
    sub_graph->AddElementPtr(std::move(a));
    sub_graph->AddElementPtr(std::move(b));

    Link(*sub_graph, "s", "out", "a", "in");
    Link(*sub_graph, "a", "out", "b", "in");
    Link(*sub_graph, "b", "out");

    audio::Graph graph("graph");
    auto c = std::make_unique<TestElement>("c", "c", state);
    c->AddInputPort("in");
    c->AddOutputPort("out", format);
    graph.AddElementPtr(std::move(sub_graph));
    graph.AddElementPtr(std::move(c));

    Link(graph, "sub-graph", "out", "c", "in");
    Link(graph, "c", "out");
    TEST_REQUIRE(graph.Prepare());

    audio::Element::EventQueue queue;
    audio::BufferHandle buffer;
    graph.Process(queue, 1);
    graph.GetOutputPort(0).PullBuffer(buffer);

    const std::string outcome((const char*)buffer->GetPtr(), buffer->GetByteSize());
    //std::cout << outcome;
    TEST_REQUIRE(outcome == "-> a:in -> a:out -> b:in -> b:out -> c:in -> c:out ");
}

int test_main(int argc, char* argv[])
{
    base::OStreamLogger logger(std::cout);
    base::SetGlobalLog(&logger);
    base::EnableDebugLog(true);

    unit_test_basic();
    unit_test_prepare_topologies();
    unit_test_buffer_flow();
    unit_test_completion();
    unit_test_graph_in_graph();
    return 0;
}