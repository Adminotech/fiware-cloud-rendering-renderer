/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#include "WebRTCWebSocketClient.h"

#include "CloudRenderingPlugin.h"
#include "Framework.h"
#include "CoreJsonUtils.h"
#include "LoggingFunctions.h"

#include <QThread>
#include <QMutexLocker>
#include <QTimer>

namespace WebRTC
{   
    /// WebSocketClient

    WebSocketClient::WebSocketClient(CloudRenderingPlugin *plugin) :
        LC("[WebRTC::WebSocketClient]: "),
        plugin_(plugin),
        thread_(0)
    {
        CloudRenderingProtocol::RegisterMetaTypes();
    }

    WebSocketClient::~WebSocketClient()
    {
        Disconnect();
    }

    QString WebSocketClient::CleanHost(QString host, u16 port)
    {
        host = host.trimmed();
        if (host.isEmpty())
            return "";

        // Strip https:// and http://
        if (host.startsWith("https://", Qt::CaseInsensitive))
            host = host.mid(8);
        if (host.startsWith("http://", Qt::CaseInsensitive))
            host = host.mid(7);

        // Prepend ws:// schema
        if (!host.startsWith("ws://", Qt::CaseInsensitive))
            host.prepend("ws://");

        // Add port if defined and not in the host string already
        if (port > 0 && host.indexOf(":", 5) == -1)
            host = QString("%1:%2").arg(host).arg(port);
        
        return host;
    }   
     
    void WebSocketClient::Connect(QString host, u16 port)
    {
        Disconnect();

        thread_ = new WebSocketThread(this, CleanHost(host, port));
        thread_->moveToThread(thread_);
        thread_->start(QThread::NormalPriority);
        
        connect(thread_, SIGNAL(ConnectionStateChange(CloudRenderingProtocol::ConnectionState)),
            this, SLOT(OnConnectionStateChange(CloudRenderingProtocol::ConnectionState)), Qt::QueuedConnection);
        connect(thread_, SIGNAL(NewMessages()), this, SLOT(OnNewMessages()), Qt::QueuedConnection);
    }
    
    void WebSocketClient::Disconnect()
    {       
        if (thread_ && thread_->isRunning())
        {
            thread_->exit();
            thread_->wait(1000);
        }
        SAFE_DELETE(thread_);
    }

    bool WebSocketClient::IsConnected() const
    {
        return (!thread_ || !thread_->client_.stopped() && !thread_->connectionHandle_.expired());
    }
    
    bool WebSocketClient::Send(CloudRenderingProtocol::IMessage *message)
    {
        if (!message)
            return false;
        if (!IsConnected())
        {
            LogError(LC + QString("Cannot send %1 message, WebSocket connection is not open!").arg(message->MessageTypeName()));
            return false;
        }

        bool ok = false;
        QByteArray json = message->ToJSON(&ok);
        if (ok)
        {
            if (thread_->IsDebugRun())
            {
                qDebug() << "Sending message: channel =" << message->ChannelTypeName() << "type =" << message->MessageTypeName() << "    raw size =" << json.size() << "bytes";
                thread_->DumpPrettyJSON(json);
            }

            websocketpp::lib::error_code ec = thread_->Send(json);
            if (ec)
            {
                LogError(LC + "Failed to send message: " + ec.message().c_str());
                ok = false;
            }
        }
        return ok;
    }
    
    void WebSocketClient::OnConnectionStateChange(CloudRenderingProtocol::ConnectionState newState)
    {
        if (!thread_)
            return;

        if (newState == CloudRenderingProtocol::CS_Connected)
        {
            if (thread_->IsDebugRun()) qDebug() << "Emitting CS_Connected";
            emit Connected();
        }
        else if (newState == CloudRenderingProtocol::CS_Disconnected)
        {
            if (thread_->IsDebugRun()) qDebug() << "Emitting CS_Disconnected";
            emit Disconnected();
        }
        else if (newState == CloudRenderingProtocol::CS_Error)
        {
            if (thread_->IsDebugRun()) qDebug() << "Emitting CS_Error";
            emit ConnectingFailed();
        }
    }
    
    void WebSocketClient::OnNewMessages()
    {
        if (!thread_)
            return;
            
        CloudRenderingProtocol::MessageSharedPtrList messages = thread_->PendingMessages();
        foreach(CloudRenderingProtocol::MessageSharedPtr message, messages)
            emit Message(message);
    }

    Framework *WebSocketClient::GetFramework() const
    {
        return plugin_->GetFramework();
    }

    // WebSocketThread

    WebSocketThread::WebSocketThread(WebSocketClient *owner, const QString &host) :
        owner_(owner),
        host_(host),
        debugRun_(false),
        LC("[WebRTC::WebSocketThread]: ")
    {
        client_.clear_access_channels(websocketpp::log::alevel::all);
        client_.clear_error_channels(websocketpp::log::elevel::all);

        // Debug logging
        if (owner_->GetFramework()->HasCommandLineParameter("--loglevel"))
        {
            QString logLevel = owner_->GetFramework()->CommandLineParameters("--loglevel").first().trimmed().toLower();
            if (logLevel == "debug")
            {
                LogDebug(LC + "Setting websocketpp loglevel to debug");
                client_.set_access_channels(websocketpp::log::alevel::all);
                client_.set_error_channels(websocketpp::log::elevel::all);
                debugRun_ = true;
            }
        }

        client_.set_open_handler(bind(&WebSocketThread::OnConnectionOpened, this, ::_1));
        client_.set_close_handler(bind(&WebSocketThread::OnConnectionClosed, this, ::_1));
        client_.set_fail_handler(bind(&WebSocketThread::OnConnectingFailed, this, ::_1));
        client_.set_message_handler(bind(&WebSocketThread::OnMessage, this, ::_1, ::_2));
    }

    WebSocketThread::~WebSocketThread()
    {
    }
    
    void WebSocketThread::Reset()
    {
        client_.stop();
        client_.reset();
        connectionHandle_.reset();
        messageQueue_.clear();
        
        emit ConnectionStateChange(CloudRenderingProtocol::CS_Disconnected);
    }
    
    bool WebSocketThread::IsDebugRun() const
    {
        return debugRun_;
    }
    
    void WebSocketThread::DumpPrettyJSON(const QByteArray &json) const
    {
        QVariant temp = TundraJson::Parse(json);
        qDebug() << endl << qPrintable(TundraJson::Serialize(temp, TundraJson::IndentFull)) << endl;
    }
    
    void WebSocketThread::run()
    {
        // Initialize ASIO
        client_.init_asio();

        /// @todo Check error code.
        websocketpp::lib::error_code err;
        WebSocket::Client::connection_ptr connection = client_.get_connection(host_.toStdString(), err);

        connectionHandle_ = connection->get_handle();
        client_.connect(connection);

        // Start poller timer and exec thread ~60 FPS
        int timerId = startTimer(16);
        int ret = exec();
        killTimer(timerId);
        
        Reset();
    }

    void WebSocketThread::timerEvent(QTimerEvent *event)
    {
        client_.poll();
    }

    void WebSocketThread::OnConnectionOpened(websocketpp::connection_hdl connection)
    {
        emit ConnectionStateChange(CloudRenderingProtocol::CS_Connected);
    }

    void WebSocketThread::OnConnectionClosed(websocketpp::connection_hdl connection)
    {
        emit ConnectionStateChange(CloudRenderingProtocol::CS_Disconnected);
    }

    void WebSocketThread::OnConnectingFailed(websocketpp::connection_hdl connection)
    {
        emit ConnectionStateChange(CloudRenderingProtocol::CS_Error);
    }

    void WebSocketThread::OnMessage(websocketpp::connection_hdl connection, WebSocket::Client::message_ptr msg)
    {
        if (msg->get_opcode() == websocketpp::frame::opcode::TEXT)
        {
            QByteArray json = QString::fromStdString(msg->get_payload()).toUtf8();

            CloudRenderingProtocol::MessageSharedPtr message = CloudRenderingProtocol::CreateMessageFromJSON(json);
            if (!message.get())
            {
                LogError(LC + "Error while parsing incoming JSON message");
                if (IsDebugRun())
                    DumpPrettyJSON(json);
                return;
            }
            if (!message->IsValid())
            {
                LogError(LC + "Failed to resolve incoming JSON messages type");
                return;
            }

            {
                QMutexLocker lock(&mutexMessages_);
                messageQueue_ << message;
            }
            
            if (IsDebugRun())
            {
                qDebug() << "Received new message: channel =" << message->ChannelTypeName() << "type =" << message->MessageTypeName() << "    raw size =" << json.size() << "bytes";
                DumpPrettyJSON(json);
            }

            emit NewMessages();
        }
        else
            LogWarning(LC + "Got BINARY type message from server... not supported!");
    }
    
    websocketpp::lib::error_code WebSocketThread::Send(QByteArray &data, websocketpp::frame::opcode::value opcode)
    {
        websocketpp::lib::error_code ec;
        client_.send(connectionHandle_, static_cast<void*>(data.data()), data.size(), opcode, ec);
        return ec;
    }
    
    CloudRenderingProtocol::MessageSharedPtrList WebSocketThread::PendingMessages()
    {
        CloudRenderingProtocol::MessageSharedPtrList pending;
        {
            QMutexLocker lock(&mutexMessages_);
            foreach(CloudRenderingProtocol::MessageSharedPtr message, messageQueue_)
                pending << message;
            messageQueue_.clear();
        }
        return pending;
    }
}
