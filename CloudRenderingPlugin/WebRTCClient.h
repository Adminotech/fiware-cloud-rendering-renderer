/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#pragma once

#include "CloudRenderingPluginApi.h"
#include "CloudRenderingPluginFwd.h"
#include "CloudRenderingDefines.h"
#include "CloudRenderingProtocol.h"

namespace WebRTC
{
    /// Cloud Rendering Client implementation.
    class CLOUDRENDERING_API Client : public QObject
    {
        Q_OBJECT

    public:
        Client(CloudRenderingPlugin *plugin);
        ~Client();
        
        /// Returns the current room.
        CloudRenderingProtocol::CloudRenderingRoom Room() const;

    private slots:
        void OnServiceConnected();
        void OnServiceDisconnected();
        void OnServiceConnectingFailed();
        void OnServiceMessage(CloudRenderingProtocol::MessageSharedPtr message);

        /// Signal handler when peers local data information has been resolved and we can send the AnswerMessage.
        void OnLocalConnectionDataResolved(WebRTC::SDP sdp, WebRTC::ICECandidateList candidates);

    private:
        QString LC;
        QString serviceHost_;
        
        WebRTCPeerConnectionPtr serverPeer_;

        CloudRenderingProtocol::CloudRenderingRoom room_;

        CloudRenderingPlugin *plugin_;
        WebRTCWebSocketClientPtr websocket_;
    };
}
