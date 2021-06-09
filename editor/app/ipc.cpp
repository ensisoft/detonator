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

#define LOGTAG "host"

#include "config.h"

#include "warnpush.h"
#  include <QByteArray>
#  include <QDataStream>
#include "warnpop.h"

#include <iostream>

#include "base/assert.h"
#include "base/utility.h"
#include "data/reader.h"
#include "data/json.h"
#include "uikit/window.h"
#include "editor/app/ipc.h"
#include "editor/app/resource.h"
#include "editor/app/eventlog.h"
#include "editor/app/utility.h"

namespace {
    enum MessageType {
        ResourceUpdate,
        UserPropertyUpdate
    };
}

namespace app
{

IPCHost::~IPCHost()
{
    DEBUG("Destroy IPCHost");
    Close();
}

bool IPCHost::Open(const QString& name)
{
    ASSERT(mClient == nullptr);
    DEBUG("Opening IPC host '%1'", name);
    if (!mServer.listen(name))
    {
        ERROR("Failed to open server '%1'", mServer.errorString());
        return false;
    }
    connect(&mServer, &QLocalServer::newConnection, this, &IPCHost::NewConnection);
    DEBUG("Host open.");
    return true;
}
void IPCHost::Close()
{
    if (mClient)
    {
        mClient->blockSignals(true);
        mClient->close();
        mClient->deleteLater();
        mClient = nullptr;
        mClientStream.setDevice(nullptr);
    }
    if (mServer.isListening())
        mServer.close();
}

//static
void IPCHost::Cleanup(const QString& name)
{
    QLocalServer::removeServer(name);
}

void IPCHost::ResourceUpdated(const Resource* resource)
{
    if (!mClient)
        return;

    data::JsonObject json;
    resource->Serialize(json);

    ASSERT(!json.HasValue("__type"));
    ASSERT(!json.HasValue("__name"));
    json.Write("__type", resource->GetType());
    json.Write("__name", resource->GetNameUtf8());
    const auto& data = json.ToString();
    const auto& str  = FromUtf8(data);

    QByteArray block;
    QDataStream stream(&block, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_10);
    stream << (quint32)MessageType::ResourceUpdate;
    stream << str;

    if (mClient->write(block) != block.size())
        ERROR("Socket write error.");
    mClient->flush();
    DEBUG("Wrote resource update '%1' '%2' %3 bytes", resource->GetId(),
          resource->GetName(), str.size());
}

void IPCHost::NewConnection()
{
    ASSERT(mClient == nullptr);
    DEBUG("Got new connection.");
    QLocalSocket* client = mServer.nextPendingConnection();
    if (client == nullptr)
    {
        ERROR("Error in client accept. '%1'", mServer.errorString());
        return;
    }
    mClient = client;
    connect(mClient, &QLocalSocket::disconnected, this, &IPCHost::ClientDisconnected);
    connect(mClient, &QLocalSocket::readyRead, this, &IPCHost::ReadMessage);
    mClientStream.setDevice(mClient);
}

void IPCHost::ClientDisconnected()
{
    ASSERT(mClient);
    mClient->deleteLater();
    mClient->close();
    mClient = nullptr;
    mClientStream.setDevice(nullptr);
}

void IPCHost::ReadMessage()
{
    // The readyRead signal is emitted once when there's
    // data available for reading.
    while (!mClientStream.atEnd())
    {
        // start a new read transaction trying to read all the
        // expected data. if not possible (i.e buffer doesn't yet
        // contain all data) then rollback.
        mClientStream.startTransaction();
        quint32 type = 0;
        mClientStream >> type;

        if (type == MessageType::UserPropertyUpdate)
        {
            QVariant data;
            QString name;
            mClientStream >> name;
            mClientStream >> data;
            if (!mClientStream.commitTransaction())
                return;

            DEBUG("Read new property '%1'", name);
            emit UserPropertyUpdated(name, data);
        }
        else
        {
            BUG("Unhandled IPC message type.");
        }
    }
}

IPCClient::IPCClient()
{
    DEBUG("Create IPC Client");
    connect(&mSocket, &QLocalSocket::readyRead, this, &IPCClient::ReadMessage);
    // only from qt 5.15 onwards
    //connect(&mSocket, &QLocalSocket::errorOccurred, this, &IPCClient::ReadError);
    mStream.setDevice(&mSocket);
}

IPCClient::~IPCClient()
{
    DEBUG("Destroy IPC Client");
    if (mSocket.isOpen())
    {
        mSocket.disconnectFromServer();
    }
    mSocket.close();
}

bool IPCClient::Open(const QString& name)
{
    mSocket.connectToServer(name);
    if (!mSocket.waitForConnected())
    {
        ERROR("Socket connection failed. ");
        return false;
    }
    DEBUG("Socket connected to host.");
    return true;
}

void IPCClient::Close()
{
    if (mSocket.isOpen())
    {
        mSocket.disconnectFromServer();
    }
    mSocket.close();
}

template<typename ClassType>
std::unique_ptr<Resource> CreateResource(const char* type, const data::Reader& data, const std::string& name)
{
    const auto& chunk = data.GetReadChunk(type, 0);
    if (!chunk)
        return nullptr;
    std::optional<ClassType> ret = ClassType::FromJson(*chunk);
    if (!ret.has_value())
        return nullptr;
    return std::make_unique<GameResource<ClassType>>(std::move(ret.value()), FromUtf8(name));
}

void IPCClient::UserPropertyUpdated(const QString& name, const QVariant& data)
{
    if (!mSocket.isOpen())
        return;

    QByteArray block;
    QDataStream stream(&block, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_10);
    stream << (quint32)MessageType::UserPropertyUpdate;
    stream << name;
    stream << data;

    if (mSocket.write(block) != block.size())
        ERROR("Socket write error.");

    mSocket.flush();
    DEBUG("Wrote new property '%1'", name);
}

void IPCClient::ReadMessage()
{
    // start a new read transaction trying to read all the
    // expected data. if not possible (i.e buffer doesn't yet
    // contain all data) then rollback.
    mStream.startTransaction();
    quint32 type = 0;
    mStream >> type;

    if (type == MessageType::ResourceUpdate)
    {
        QString message;
        mStream >> message;
        if (!mStream.commitTransaction())
            return;

        data::JsonObject json;
        const auto [ok, error] = json.ParseString(ToUtf8(message));
        if (!ok)
        {
            ERROR("JSON parse error (%1') in IPC message.", error);
            return;
        }
        //std::cout << json.dump(2);
        //std::cout << std::endl;
        ASSERT(json.HasValue("__name"));
        ASSERT(json.HasValue("__type"));
        std::unique_ptr<Resource> resource;
        std::string name;
        Resource::Type type;
        json.Read("__type", &type);
        json.Read("__name", &name);
        if (type == Resource::Type::Entity)
            resource = CreateResource<game::EntityClass>("entities", json, name);
        else if (type == Resource::Type::Scene)
            resource = CreateResource<game::SceneClass>("scenes", json, name);
        else if (type == Resource::Type::Material)
            resource = CreateResource<gfx::MaterialClass>("materials", json, name);
        else if (type == Resource::Type::Shape)
            resource = CreateResource<gfx::PolygonClass>("shapes", json, name);
        else if (type == Resource::Type::ParticleSystem)
            resource = CreateResource<gfx::KinematicsParticleEngineClass>("particles", json, name);
        else if (type == Resource::Type::Script)
            resource = CreateResource<Script>("scripts", json, name);
        else if (type == Resource::Type::AudioFile)
            resource = CreateResource<AudioFile>("audio_files", json, name);
        else if (type == Resource::Type::DataFile)
            resource = CreateResource<DataFile>("data_files", json, name);
        else if (type == Resource::Type::UI)
            resource = CreateResource<uik::Window>("uis", json, name);
        else BUG("Unhandled resource type.");

        if (!resource)
        {
            ERROR("Load Resource class object from JSON failed.");
            return;
        }
        DEBUG("Read resource update '%1' '%2' %3 bytes",
              resource->GetId(), resource->GetName(), message.size());
        emit ResourceUpdated(resource.get());
    }
    else
    {
        BUG("Unhandled IPC message type.");
    }
}
void IPCClient::ReadError(QLocalSocket::LocalSocketError error)
{
    ERROR("Socket error: %1", error);
    ERROR(mSocket.errorString());
}

} // namespace
