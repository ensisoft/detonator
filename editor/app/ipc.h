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

#include "warnpush.h"
#  include <QObject>
#  include <QLocalServer>
#  include <QtNetwork>
#  include <QJsonObject>
#include "warnpop.h"

namespace app
{
    class Resource;

    // IPC host and client are for serializing and sending workspace
    // changes over some ICP method from one process to another.
    // When the game is launched the editor application spawns a
    // a new EditorGameHost process and communicates with it over
    // an IPC channel. Whenever changes to the game resources are
    // made those changes are serialized and sent over the IPC
    // channel from the Editor process to the EditorGameHost process
    // so that the changes can be shown to the user in the game
    // window and edits are "live":

    // This is the "host" part of the IPC communication channel.
    // Opens a new local socket and accepts 1 incoming client connection.
    class IPCHost : public QObject
    {
        Q_OBJECT

    public:
        IPCHost() = default;
        IPCHost(const IPCHost& other) = delete;
        ~IPCHost() ;

        bool Open(const QString& ipc_socke_name);
        void Close();
        bool IsConnected() const
        { return mClient != nullptr; }
        bool IsOpen() const
        {
            return mServer.isListening();
        }

        IPCHost& operator=(const IPCHost&) = delete;

        static void CleanupSocket(const QString& ipc_socket_name);

    signals:
        void UserPropertyUpdated(const QString& name, const QVariant& data);
        void JsonMessageReceived(const QJsonObject& json);
        void ClientConnected();

    public slots:
        // On invocation serialize the contents of the resource
        // object and send to the connected client if any.
        void ResourceUpdated(const Resource* resource);
        void SendJsonMessage(const QJsonObject& json);

    private slots:
        void NewConnection();
        void ClientDisconnected();
        void ReadMessage();

    private:
        QLocalServer mServer;
        QLocalSocket* mClient = nullptr;
        QDataStream mClientStream;
    };

    // This is the "client" part of the IPC communication channel.
    // The IPC Client opens a new local socket and tries to connect
    // to the specified local host socket.
    class IPCClient : public QObject
    {
        Q_OBJECT

    public:
        IPCClient();
       ~IPCClient();
        IPCClient(const IPCClient& other) = delete;

        // Try to connect to the named local server identified
        // by its name. This function will wait for the connection
        // to complete. Returns true on success or false on error.
        bool Open(const QString& ipc_socket_name);

        void Close();

        bool IsOpen() const
        { return mSocket.isOpen(); }

        IPCClient& operator=(const IPCClient&) = delete;
    signals:
        // Emitted when new data has been read and a resource
        // object has been deserialized from the communication
        // message. The resource object contains the exact same
        // state that was stored on the host side.
        void ResourceUpdated(const Resource* resource);
        void JsonMessageReceived(const QJsonObject& json);
    public slots:
        void UserPropertyUpdated(const QString& name, const QVariant& data);
        void SendJsonMessage(const QJsonObject& json);
    private slots:
        void ReadMessage();
        void ReadError(QLocalSocket::LocalSocketError error);
    private:
        QLocalSocket mSocket;
        QDataStream  mStream;
    };

} // namespace
