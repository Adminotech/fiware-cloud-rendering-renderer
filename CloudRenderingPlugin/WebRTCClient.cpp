/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#include "WebRTCClient.h"
#include "WebRTCWebSocketClient.h"
#include "WebRTCPeerConnection.h"

#include "CloudRenderingPlugin.h"

#include "Framework.h"
#include "LoggingFunctions.h"

namespace WebRTC
{
    Client::Client(CloudRenderingPlugin *plugin) :
        LC("[WebRTC::Client]: "),
        plugin_(plugin),
        websocket_(new WebRTC::WebSocketClient(plugin))
    {
        WebRTC::RegisterMetaTypes();
        CloudRenderingProtocol::RegisterMetaTypes();

        connect(websocket_.get(), SIGNAL(Connected()), SLOT(OnServiceConnected()));
        connect(websocket_.get(), SIGNAL(Disconnected()), SLOT(OnServiceDisconnected()));
        connect(websocket_.get(), SIGNAL(ConnectingFailed()), SLOT(OnServiceConnectingFailed()));
        connect(websocket_.get(), SIGNAL(Message(CloudRenderingProtocol::MessageSharedPtr)),
            SLOT(OnServiceMessage(CloudRenderingProtocol::MessageSharedPtr)));

        serverPeer_ = WebRTCPeerConnectionPtr(new WebRTC::PeerConnection(plugin_->GetFramework(), 0)); /// @todo Will this be the reserver server id?
        connect(serverPeer_.get(), SIGNAL(LocalConnectionDataResolved(WebRTC::SDP, WebRTC::ICECandidateList)), 
            SLOT(OnLocalConnectionDataResolved(WebRTC::SDP, WebRTC::ICECandidateList)), Qt::QueuedConnection);

        // Connect to service
        serviceHost_ = WebRTC::WebSocketClient::CleanHost(plugin_->GetFramework()->CommandLineParameters("--cloudRenderingClient").first());
        if (!serviceHost_.isEmpty())
            websocket_->Connect(serviceHost_);
        else
            LogError(LC + "--cloudRenderingClient <cloudRenderingServiceHost> parameter not defined, cannot connect to service for client registration!");
    }
    
    Client::~Client()
    {
        websocket_.reset();
        serverPeer_.reset();
    }
    
    CloudRenderingProtocol::CloudRenderingRoom Client::Room() const
    {
        return room_;
    }
    
    void Client::OnServiceConnected()
    {
        CloudRenderingProtocol::State::RegistrationMessage *message = new CloudRenderingProtocol::State::RegistrationMessage(
            CloudRenderingProtocol::State::RegistrationMessage::R_Client);
        message->deleteLater();
        websocket_->Send(message);

        room_.Reset();
    }

    void Client::OnServiceDisconnected()
    {
        LogInfo(LC + QString("WebSocket connection disconnected from %1").arg(serviceHost_));
        room_.Reset();
    }

    void Client::OnServiceConnectingFailed()
    {
        LogError(LC + QString("WebSocket connection failed to Cloud Rendering Service at %1").arg(serviceHost_));
        room_.Reset();
    }

    void Client::OnServiceMessage(CloudRenderingProtocol::MessageSharedPtr message)
    {
        if (!serverPeer_)
            return;

        switch (message->Channel())
        {
            // Signaling
            case CloudRenderingProtocol::CT_Signaling:
            {
                switch (message->Type())
                {
                    // Offer
                    case CloudRenderingProtocol::MT_Offer:
                    {
                        CloudRenderingProtocol::Signaling::OfferMessage *offer = dynamic_cast<CloudRenderingProtocol::Signaling::OfferMessage*>(message.get());
                        if (offer)
                            serverPeer_->HandleOfferOrAnswer(offer->sdp, offer->iceCandidates, PeerConnection::ConnectionSettings());
                        else
                            LogError(LC + "Failed to cast MT_Offer message to OfferMessage*");
                        break;
                    }
                    // Answer
                    case CloudRenderingProtocol::MT_Answer:
                    {
                        CloudRenderingProtocol::Signaling::AnswerMessage *answer = dynamic_cast<CloudRenderingProtocol::Signaling::AnswerMessage*>(message.get());
                        if (answer)
                            serverPeer_->HandleOfferOrAnswer(answer->sdp, answer->iceCandidates, PeerConnection::ConnectionSettings());
                        else
                            LogError(LC + "Failed to cast MT_Answer message to AnswerMessage*");
                        break;
                    }
                    // Ice candidates
                    case CloudRenderingProtocol::MT_IceCandidates:
                    {
                        CloudRenderingProtocol::Signaling::IceCandidatesMessage *candidates = dynamic_cast<CloudRenderingProtocol::Signaling::IceCandidatesMessage*>(message.get());
                        if (candidates)
                            serverPeer_->AddRemoteIceCandidates(candidates->iceCandidates);
                        else
                            LogError(LC + "Failed to cast MT_IceCandidates message to IceCandidatesMessage*");
                        break;
                    }
                }
                break;
            }
            // Room
            case CloudRenderingProtocol::CT_Room:
            {
                switch (message->Type())
                {
                    case CloudRenderingProtocol::MT_RoomAssigned:
                    {
                        room_.Reset();

                        CloudRenderingProtocol::Room::RoomAssignedMessage *assigned = dynamic_cast<CloudRenderingProtocol::Room::RoomAssignedMessage*>(message.get());
                        if (assigned)
                        {
                            if (assigned->error == CloudRenderingProtocol::Room::RoomAssignedMessage::RQE_NoError)
                            {
                                room_.id = assigned->roomId;
                                LogInfo(LC + "Client was assigned to room " + room_.id);
                            }
                            else
                                LogError(LC + QString("RoomAssignedMessage sent a error code %1 to renderer, this should never happen as we are not requesting for a room!")
                                    .arg(static_cast<int>(assigned->error)));
                        }
                        else
                            LogError(LC + "Failed to cast MT_RoomAssigned message to RoomAssignedMessage*");
                        break;
                    }
                    case CloudRenderingProtocol::MT_RoomUserJoined:
                    {
                        CloudRenderingProtocol::Room::RoomUserJoinedMessage *joined = dynamic_cast<CloudRenderingProtocol::Room::RoomUserJoinedMessage*>(message.get());
                        if (joined)
                        {
                            LogInfo(LC + "Peers joined to the clients room");
                            foreach(const QString &joinedPeerId, joined->peerIds)
                            {
                                LogInfo(LC + QString("  peerId = %1").arg(joinedPeerId));
                                room_.AddPeer(joinedPeerId);
                            }
                        }
                        else
                            LogError(LC + "Failed to cast MT_RoomUserJoined message to RoomUserJoinedMessage*");
                        break;
                    }
                    case CloudRenderingProtocol::MT_RoomUserLeft:
                    {
                        CloudRenderingProtocol::Room::RoomUserLeftMessage *left = dynamic_cast<CloudRenderingProtocol::Room::RoomUserLeftMessage*>(message.get());
                        if (left)
                        {
                            LogInfo(LC + "Peers left from the renderers room");
                            foreach(const QString &leftPeerId, left->peerIds)
                            {
                                LogInfo(LC + QString("  peerId = %1").arg(leftPeerId));
                                room_.RemovePeer(leftPeerId);
                            }
                        }
                        else
                            LogError(LC + "Failed to cast MT_RoomUserLeft message to RoomUserJoinedMessage*");
                        break;
                    }
                }
                break;
            }
        }
    }

    void Client::OnLocalConnectionDataResolved(WebRTC::SDP sdp, WebRTC::ICECandidateList candidates)
    {
        WebRTC::PeerConnection *peer = dynamic_cast<WebRTC::PeerConnection*>(sender());
        if (!peer)
        {
            LogError(LC + "Failed to cast signal sender as WebRTC::PeerConnection*");
            return;
        }
        if (peer != serverPeer_.get())
        {
            LogError(LC + "Signal sender not same as server peer. This should never happen!");
            return;
        }
        
        if (sdp.type.compare("answer", Qt::CaseInsensitive))
        {
            CloudRenderingProtocol::Signaling::AnswerMessage *message = new CloudRenderingProtocol::Signaling::AnswerMessage(peer->Id());
            message->deleteLater();
            message->sdp = sdp;
            message->iceCandidates = candidates;

            if (!websocket_->Send(message))
                LogWarning(LC + "Failed to send " + message->MessageTypeName());
        }
        else if (sdp.type.compare("offer", Qt::CaseInsensitive))
        {
            CloudRenderingProtocol::Signaling::OfferMessage *message = new CloudRenderingProtocol::Signaling::OfferMessage(peer->Id());
            message->deleteLater();
            message->sdp = sdp;
            message->iceCandidates = candidates;

            if (!websocket_->Send(message))
                LogWarning(LC + "Failed to send " + message->MessageTypeName());
        }
        else
            LogError(LC + QString("Resolved SDP type is not 'offer' or 'answer' but '%1', doing nothing.").arg(sdp.type));
    }
}
