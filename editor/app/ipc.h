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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QObject>
#  include <QLocalServer>
#  include <QtNetwork>
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

    // This is the "host" part of the communication. Created
    // by the editor process. Opens a new local socket and
    // accepts 1 connection.
    class IPCHost : public QObject
    {
        Q_OBJECT

    public:
        IPCHost() = default;
        IPCHost(const IPCHost& other) = delete;
        ~IPCHost() ;

        bool Open(const QString& name);
        void Close();
        bool IsConnected() const
        { return mClient != nullptr; }
        bool IsOpen() const
        {
            return mServer.isListening();
        }

        IPCHost& operator=(const IPCHost&) = delete;

        static
        void Cleanup(const QString& name);

    signals:
        void UserPropertyUpdated(const QString& name, const QVariant& data);

    public slots:
        // On invocation serialize the contents of the resource
        // object and send to the connected client if any.
        void ResourceUpdated(const Resource* resource);

    private slots:
        void NewConnection();
        void ClientDisconnected();
        void ReadMessage();

    private:
        QLocalServer mServer;
        QLocalSocket* mClient = nullptr;
        QDataStream mClientStream;
    };

    // This is the "client" part of the communication. Created
    // by the game host process. Opens a new local socket and
    // tries to connect to the local host.
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
        bool Open(const QString& name);

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
    public slots:
        void UserPropertyUpdated(const QString& name, const QVariant& data);
    private slots:
        void ReadMessage();
        void ReadError(QLocalSocket::LocalSocketError error);
    private:
        QLocalSocket mSocket;
        QDataStream  mStream;
    };

} // namespace
