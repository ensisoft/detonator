// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "config.h"

#include "warnpush.h"
#  include <QCoreApplication>
#include "warnpop.h"

#include <chrono>
#include <iostream>

#include "base/test_minimal.h"
#include "base/logging.h"
#include "graphics/material.h"
#include "editor/app/ipc.h"
#include "editor/app/resource.h"
#include "editor/app/workspace.h"
#include "editor/app/eventlog.h"

void unit_test_ipc_host()
{
    app::IPCHost::Cleanup("test-socket");

    app::IPCHost host;
    TEST_REQUIRE(host.IsConnected() == false);
    TEST_REQUIRE(host.IsOpen() == false);
    TEST_REQUIRE(host.Open("test-socket"));
    TEST_REQUIRE(host.IsOpen() == true);
    TEST_REQUIRE(host.IsConnected() == false);

    // try open 2nd host on the same name
    {
        app::IPCHost host;
        TEST_REQUIRE(host.Open("test-socket") == false);
    }
    host.Close();
    TEST_REQUIRE(host.IsConnected() == false);
    TEST_REQUIRE(host.IsOpen() == false);
}

void unit_test_ipc_client()
{
    app::IPCHost::Cleanup("test-socket");

    app::IPCClient client;
    TEST_REQUIRE(client.IsOpen() == false);
    TEST_REQUIRE(client.Open("foobar") == false);
    TEST_REQUIRE(client.IsOpen() == false);

    app::IPCHost host;
    TEST_REQUIRE(host.Open("test-socket"));
    TEST_REQUIRE(client.Open("test-socket"));
    TEST_REQUIRE(client.IsOpen());
    QEventLoop footgun;
    footgun.processEvents();
    TEST_REQUIRE(host.IsConnected());

    client.Close();
    footgun.processEvents();
    TEST_REQUIRE(host.IsConnected() == false);
    TEST_REQUIRE(host.IsOpen() == true);
}

void unit_test_ipc_send_recv()
{
    app::IPCHost::Cleanup("test_socket_name");

    app::IPCHost host;
    app::IPCClient client;
    TEST_REQUIRE(host.Open("test_socket_name"));
    TEST_REQUIRE(client.Open("test_socket_name"));

    QEventLoop footgun;
    footgun.processEvents();
    TEST_REQUIRE(host.IsConnected());

    for (int i=0; i<100; ++i)
    {
        app::Workspace workspace;
        QObject::connect(&client, &app::IPCClient::ResourceUpdated, &workspace,
                         &app::Workspace::UpdateResource);
        QObject::connect(&host, &app::IPCHost::UserPropertyUpdated, &workspace,
                         &app::Workspace::UpdateUserProperty);

        std::string id;
        std::size_t hash = 0;
        {
            gfx::MaterialClass test;
            test.SetBaseColor(gfx::Color::DarkGreen);
            test.SetBlendFrames(true);
            test.SetGamma(4.0f);
            id = test.GetId();
            hash = test.GetHash();
            app::MaterialResource resource(std::move(test), "foobar1234");
            host.ResourceUpdated(&resource);
        }

        {
            client.UserPropertyUpdated("user-int", 123);
            client.UserPropertyUpdated("user-str", "foobar");
        }

        const app::Resource* resource = nullptr;
        bool has_int_property = false;
        bool has_str_property = false;

        for (int i=0; i<100; ++i)
        {
            footgun.processEvents();
            resource = workspace.FindResourceById(app::FromUtf8(id));
            has_int_property = workspace.HasUserProperty("user-int");
            has_str_property = workspace.HasUserProperty("user-str");
            if (resource && has_int_property && has_str_property)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        TEST_REQUIRE(resource != nullptr);
        TEST_REQUIRE(has_int_property);
        TEST_REQUIRE(has_str_property);

        const gfx::MaterialClass *material = nullptr;
        resource->GetContent(&material);
        TEST_REQUIRE(resource->GetType() == app::Resource::Type::Material);
        TEST_REQUIRE(resource->GetName() == "foobar1234");
        TEST_REQUIRE(material->GetHash() == hash);
        TEST_REQUIRE(material->GetId() == id);
        TEST_REQUIRE(workspace.GetUserProperty("user-int", 0) == 123);
        TEST_REQUIRE(workspace.GetUserProperty("user-str", QString("")) == "foobar");
    }
}

int test_main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    base::OStreamLogger logger(std::cout);
    base::SetGlobalLog(&logger);
    base::EnableDebugLog(true);
    app::EventLog::get().OnNewEvent = [](const app::Event& event) {
        std::cout << app::ToUtf8(event.message);
        std::cout << std::endl;
    };

    //unit_test_ipc_host();
    //unit_test_ipc_client();
    unit_test_ipc_send_recv();
    return 0;
}
