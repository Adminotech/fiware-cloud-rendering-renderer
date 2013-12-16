/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#pragma once

#include "CloudRenderingPluginApi.h"
#include "CloudRenderingPluginFwd.h"
#include "CloudRenderingProtocol.h"

#include <QThread>
#include <QString>
#include <QVariant>
#include <QMutex>
#include <QDebug>

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

namespace WebRTC
{
    /// @cond PRIVATE
    class WebSocketThread;
    
    namespace WebSocket
    {
        typedef websocketpp::client<websocketpp::config::asio_client> Client;
    }
    /// @endcond
       
    // WebSocketClient

    class CLOUDRENDERING_API WebSocketClient : public QObject
    {
        Q_OBJECT

    public:
        WebSocketClient(CloudRenderingPlugin *plugin);
        ~WebSocketClient();
        
        /// Cleans the host to have ws:// protocol and adds @c port if > 0
        /// and not already set in @c host.
        static QString CleanHost(QString host, u16 port = 0);

        /// Returns framework ptr.
        Framework *GetFramework() const;
        
    public slots:
        /// If port is 0 it wont be appended to the host string.
        /// You need to ensure the port is there in that case.
        void Connect(QString host, u16 port = 0);
        
        /// Disconnect current connection.
        void Disconnect();
        
        /// Returns if currently connected to a server.
        bool IsConnected() const;
        
        /// Send a message to the server.
        bool Send(CloudRenderingProtocol::IMessage *message);
        
    private slots:
        void OnConnectionStateChange(CloudRenderingProtocol::ConnectionState newState);
        
        void OnNewMessages();
        
    signals:
        /// Emitted once connected to the server.
        void Connected();
        
        /// Emitted when disconnected from the server.
        void Disconnected();
        
        /// Emitted when connection could not be established to the host in Connect().
        void ConnectingFailed();

        /// Emitted when a new message is received from the websocket connection.
        /** @note The signal is emitted in the main thread context, do not use Qt::QueuedConnection. */
        void Message(CloudRenderingProtocol::MessageSharedPtr message);

    private:        
        QString LC;
        
        CloudRenderingPlugin *plugin_;
        WebSocketThread *thread_;
    };
    
    // WebSocketThread
    
    class CLOUDRENDERING_API WebSocketThread : public QThread
    {
    Q_OBJECT
    
    public:
        WebSocketThread(WebSocketClient *owner, const QString &host);
        ~WebSocketThread();

        /// Internal use only for websocket connection opened.
        void OnConnectionOpened(websocketpp::connection_hdl connection);

        /// Internal use only for websocket connection closed.
        void OnConnectionClosed(websocketpp::connection_hdl connection);

        /// Internal use only for websocket connection failed.
        void OnConnectingFailed(websocketpp::connection_hdl connection);

        /// Internal use only for incoming websocket messages.
        void OnMessage(websocketpp::connection_hdl connection, WebSocket::Client::message_ptr msg);
        
        /// Send message.
        websocketpp::lib::error_code Send(QByteArray &data, websocketpp::frame::opcode::value opcode = websocketpp::frame::opcode::TEXT);
        
        /// Returns the currently pending messages, this thread will forget the ptrs once returned.
        /** Only fetch the messages if you intend to process them! This should be done as a response to NewMessages() signal. */
        CloudRenderingProtocol::MessageSharedPtrList PendingMessages();

        friend class WebSocketClient;
        
    signals:
        void ConnectionStateChange(CloudRenderingProtocol::ConnectionState newState);
        void NewMessages();

    protected:
        /// QThread override.
        void run();
        
        /// QObject override.
        void timerEvent(QTimerEvent *event);
        
        void Reset();
        
        bool IsDebugRun() const;
        void DumpPrettyJSON(const QByteArray &json) const;
        
        WebSocket::Client client_;
        websocketpp::connection_hdl connectionHandle_;
        
    private:
        WebSocketClient *owner_;
        
        QMutex mutexMessages_;
        CloudRenderingProtocol::MessageSharedPtrList messageQueue_;

        QString LC;
        QString host_;
        bool debugRun_;
    };
}
