/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#pragma once

#include "CloudRenderingProtocol.h"
#include "CoreJsonUtils.h"

#include "LoggingFunctions.h"

namespace CloudRenderingProtocol
{
    // CloudRenderingRoom

    CloudRenderingRoom::CloudRenderingRoom()
    {
        Reset();
    }

    bool CloudRenderingRoom::HasPeer(const QString &peerId) const
    {
        return peers.contains(peerId);
    }

    bool CloudRenderingRoom::AddPeer(const QString &peerId)
    {
        if (HasPeer(peerId))
            return false;
        peers << peerId;
        return true;
    }

    void CloudRenderingRoom::RemovePeer(const QString &peerId)
    {
        peers.removeAll(peerId);
    }

    void CloudRenderingRoom::Reset()
    {
        id = "";
        peers.clear();
    }

    // Utils

    void DumpPrettyJSON(const QByteArray &json)
    {
        QVariant temp = TundraJson::Parse(json);
        qDebug() << endl << qPrintable(TundraJson::Serialize(temp, TundraJson::IndentFull)) << endl;
    }

    // Message parser

    MessageSharedPtr CreateMessageFromJSON(const QByteArray &json)
    {
        bool ok = false;
        QVariantMap in = TundraJson::Parse(json, &ok).toMap();
        if (!ok)
            MessageSharedPtr();
        
        // Read initial messageType information.
        QString channelTypeName = TundraJson::ValueForAnyKey(in, QStringList() << "channel" << "Channel", "").toString().trimmed();
        QVariantMap msgData = TundraJson::ValueForAnyKey(in, QStringList() << "message" << "Message", QVariantMap()).toMap();

        QString messageTypeName = TundraJson::ValueForAnyKey(msgData, QStringList() << "type" << "Type", "").toString().trimmed();
        QVariantMap data = TundraJson::ValueForAnyKey(msgData, QStringList() << "data" << "Data", QVariantMap()).toMap();
        MessageType messageType = ToMessageType(messageTypeName);           
        
        // Construct the correct message.
        MessageSharedPtr message;

        // Signaling
        if (messageType == Signaling::OfferMessage::MessageTypeStatic())
            message = MessageSharedPtr(new Signaling::OfferMessage());
        else if (messageType == Signaling::AnswerMessage::MessageTypeStatic())
            message = MessageSharedPtr(new Signaling::AnswerMessage());
        else if (messageType == Signaling::IceCandidatesMessage::MessageTypeStatic())
            message = MessageSharedPtr(new Signaling::IceCandidatesMessage());
        // Room
        else if (messageType == Room::RoomAssignedMessage::MessageTypeStatic())
            message = MessageSharedPtr(new Room::RoomAssignedMessage());
        else if (messageType == Room::RoomUserJoinedMessage::MessageTypeStatic())
            message = MessageSharedPtr(new Room::RoomUserJoinedMessage());
        else if (messageType == Room::RoomUserLeftMessage::MessageTypeStatic())
            message = MessageSharedPtr(new Room::RoomUserLeftMessage());
        // Application
        else if (messageType == Application::RoomCustomMessage::MessageTypeStatic())
            message = MessageSharedPtr(new Application::RoomCustomMessage());
        else if (messageType == Application::PeerCustomMessage::MessageTypeStatic())
            message = MessageSharedPtr(new Application::PeerCustomMessage());

        if (message.get())
        {
            // If from data deserialization fails, return a null ptr.
            if (!message->FromData(data))
            {
                LogError(QString("CreateMessageFromJSON: Failed to serialize message with type name '%1' from data").arg(messageTypeName));
                message.reset();
            }
        }
        else
            LogError(QString("CreateMessageFromJSON: Unknown message with type name '%1'").arg(messageTypeName));

        return message;
    }
    
    // IMessage

    IMessage::IMessage(ChannelType channelType_, MessageType messageType_) :
        channelType(channelType_),
        messageType(messageType_)
    {
    }
    
    IMessage::~IMessage()
    {
    }

    ChannelType IMessage::Channel() const
    {
        return channelType;
    }

    MessageType IMessage::Type() const
    {
        return messageType;
    }
    
    QString IMessage::ChannelTypeName() const
    {
        return ToChannelTypeName(Channel());
    }
    
    QString IMessage::MessageTypeName() const
    {
        return ToMessageTypeName(Type());
    }
    
    QVariantMap IMessage::Data() const
    {
        return data;
    }
    
    bool IMessage::IsValid(bool serialization) const
    {
        bool valid = (channelType != CT_Invalid && messageType != MT_Invalid);
        if (valid && serialization)
            valid = IsValidForSerialization();
        return valid;
    }
    
    bool IMessage::IsNull() const
    {
        return (!data.isEmpty());
    }
    
    bool IMessage::FromJSON(const QByteArray &json)
    {
        bool ok = false;
        QVariantMap in = TundraJson::Parse(json, &ok).toMap();
        if (ok)
        {
            // Parse preliminary information.
            QString channelTypeName = TundraJson::ValueForAnyKey(in, QStringList() << "channel" << "Channel", "").toString().trimmed();
            channelType = ToChannelType(channelTypeName);
            QVariantMap msgData = TundraJson::ValueForAnyKey(in, QStringList() << "message" << "Message", QVariantMap()).toMap();
            
            QString messageTypeName = TundraJson::ValueForAnyKey(msgData, QStringList() << "type" << "Type", "").toString().trimmed();
            messageType = ToMessageType(messageTypeName);
            data = TundraJson::ValueForAnyKey(msgData, QStringList() << "data" << "Data", QVariantMap()).toMap();

            // Request the implementation to parse its contents from data.
            if (IsValid())
                ok = Deserialize();
        }
        return ok;
    }
    
    bool IMessage::FromData(const QVariantMap &data_)
    {
        data = data_;
        return (IsValid() ? Deserialize() : false);
    }
    
    QByteArray IMessage::ToJSON(bool *ok)
    {
        if (ok != 0)
            *ok = false;

        if (!IsValid(true))
        {
            LogError("IMessage::ToJSON: Message is not in a valid state.");
            return QByteArray();
        }

        // Request the implementation to update the data member.
        Serialize();

        QVariantMap msg;
        msg["type"] = MessageTypeName();
        msg["data"] = data;
        
        QVariantMap out;
        out["channel"] = ChannelTypeName();
        out["message"] = msg;

        return TundraJson::Serialize(out, TundraJson::IndentNone, ok);
    }

    namespace Signaling
    {    
        // OfferMessage
        
        OfferMessage::OfferMessage(const QString &receiverId_) :
            IMessage(ChannelTypeStatic(), MessageTypeStatic()),
            receiverId(receiverId_)
        {
        }
        
        OfferMessage::OfferMessage(int receiverId_) :
            IMessage(ChannelTypeStatic(), MessageTypeStatic())
        {
            if (receiverId_ != -10000)
                receiverId = QString::number(receiverId_);
        }
        
        void OfferMessage::Serialize()
        {
            // 'senderId' will not be written to data
            // so they cant be spoofed. It is injected by the service.
            data["receiverId"] = receiverId;

            data["sdp"] = sdp.ToVariant();
            if (!iceCandidates.isEmpty())
                data["iceCandidates"] = WebRTC::VariantFromIceCandidates(iceCandidates);
        }
        
        bool OfferMessage::Deserialize()
        {
            bool ok = data.contains("senderId");
            if (ok)
            {
                senderId = data.value("senderId", "").toString();
                if (data.contains("receiverId"))
                    receiverId = data.value("receiverId", "").toString();
            }
            else
                LogError("OfferMessage::Deserialize: 'senderId' property is not defined.");

            if (ok)
            {
                ok = sdp.FromVariant(data.value("sdp", QVariantMap()).toMap());
                if (ok)
                {
                    if (data.contains("iceCandidates"))
                        iceCandidates = WebRTC::IceCandidatesFromVariant(data.value("iceCandidates", QVariantList()).toList());
                }
                else
                    LogError("OfferMessage::Deserialize: 'sdp' property could not be parsed, assuming invalid format.");
            }
            return ok;
        }
        
        // AnswerMessage
        
        AnswerMessage::AnswerMessage(const QString &receiverId_) :
            IMessage(ChannelTypeStatic(), MessageTypeStatic()),
            receiverId(receiverId_)
        {
        }

        AnswerMessage::AnswerMessage(int receiverId_) :
            IMessage(ChannelTypeStatic(), MessageTypeStatic())
        {
            if (receiverId_ != -10000)
                receiverId = QString::number(receiverId_);
        }
        
        void AnswerMessage::Serialize()
        {
            // 'senderId' will not be written to data
            // so they cant be spoofed. It is injected by the service.
            data["receiverId"] = receiverId;

            data["sdp"] = sdp.ToVariant();
            if (!iceCandidates.isEmpty())
                data["iceCandidates"] = WebRTC::VariantFromIceCandidates(iceCandidates);
        }

        bool AnswerMessage::Deserialize()
        {
            bool ok = data.contains("senderId");
            if (ok)
            {
                senderId = data.value("senderId", "").toString();
                if (data.contains("receiverId"))
                    receiverId = data.value("receiverId", "").toString();
            }
            else
                LogError("AnswerMessage::Deserialize: 'senderId' property is not defined.");

            if (ok)
            {
                ok = sdp.FromVariant(data.value("sdp", QVariantMap()).toMap());
                if (ok)
                {
                    if (data.contains("iceCandidates"))
                        iceCandidates = WebRTC::IceCandidatesFromVariant(data.value("iceCandidates", QVariantList()).toList());
                }
                else
                    LogError("AnswerMessage::Deserialize: 'sdp' property could not be parsed, assuming invalid format.");
            }
            return ok;
        }
        
        // IceCandidatesMessage
        
        IceCandidatesMessage::IceCandidatesMessage(const QString &receiverId_, WebRTC::ICECandidateList iceCandidates_) :
            IMessage(ChannelTypeStatic(), MessageTypeStatic()),
            receiverId(receiverId_),
            iceCandidates(iceCandidates_)
        {
        }
        
        IceCandidatesMessage::IceCandidatesMessage(int receiverId_, WebRTC::ICECandidateList iceCandidates_) :
            IMessage(ChannelTypeStatic(), MessageTypeStatic()),
            iceCandidates(iceCandidates_)
        {
            if (receiverId_ != -10000)
                receiverId = QString::number(receiverId_);
        }
        
        void IceCandidatesMessage::Serialize()
        {
            data["receiverId"] = receiverId;
            if (!senderId.isEmpty())
                data["senderId"] = senderId;

            data["iceCandidates"] = WebRTC::VariantFromIceCandidates(iceCandidates);
        }

        bool IceCandidatesMessage::Deserialize()
        {
            bool ok = data.contains("senderId");
            if (ok)
            {
                senderId = data.value("senderId", "").toString();
                if (data.contains("receiverId"))
                    receiverId = data.value("receiverId", "").toString();

                iceCandidates = WebRTC::IceCandidatesFromVariant(data.value("iceCandidates", QVariantList()).toList());
            }
            else
                LogError("IceCandidatesMessage::Deserialize: 'senderId' property is not defined.");
            
            return (!iceCandidates.isEmpty());
        }
    }

    namespace State
    {
        // RegistrationMessage
        
        RegistrationMessage::RegistrationMessage(Registrant registrant_, const QString &roomId_) :
            IMessage(ChannelTypeStatic(), MessageTypeStatic()),
            registrant(registrant_),
            roomId(roomId_)
        {
        }
        
        QString RegistrationMessage::ToRegistrantTypeName(Registrant reg)
        {
            if (reg == R_Renderer)
                return "renderer";
            else if (reg == R_Client)
                return "client";
            return "";        
        }
        
        RegistrationMessage::Registrant RegistrationMessage::ToRegistrantType(const QString &reg)
        {
            if (reg.trimmed().compare("renderer", Qt::CaseInsensitive) == 0)
                return R_Renderer;
            else if (reg.trimmed().compare("client", Qt::CaseInsensitive) == 0)
                return R_Client;                
            return R_Unkown;
        }

        void RegistrationMessage::Serialize()
        {
            data["registrant"] = ToRegistrantTypeName(registrant);
            if (registrant == R_Client && !roomId.isEmpty())
                data["roomId"] = roomId;
        }

        bool RegistrationMessage::Deserialize() 
        {
            registrant = ToRegistrantType(data.value("registrant", "").toString());
            if (registrant != R_Renderer && registrant != R_Client)
            {
                LogError(QString("RegistrationMessage::Deserialize: Failed to convert message registrant '%1' to a valid enum").arg(data.value("registrant", "").toString()));
                return false;
            }
            if (registrant == R_Client)
                roomId = data.value("roomId", "").toString();
            return true;
        }
        
        // RendererStateChangeMessage

        RendererStateChangeMessage::RendererStateChangeMessage(State state_) : 
            IMessage(ChannelTypeStatic(), MessageTypeStatic()),
            state(state_)
        {
        }

        void RendererStateChangeMessage::Serialize()
        {
            data["state"] = static_cast<int>(state);
        }

        bool RendererStateChangeMessage::Deserialize() 
        {
            int intState = data.value("state", -1).toInt();
            if (intState <= static_cast<int>(RS_Invalid) || intState > static_cast<int>(RS_Full))
            {
                LogError(QString("RendererStateChangeMessage::Deserialize: Failed to convert message state %1 to a valid state enum").arg(intState));
                return false;
            }
            state = static_cast<State>(intState);
            return true;
        }
    }
    
    namespace Room
    {       
        // RoomAssignedMessage

        RoomAssignedMessage::RoomAssignedMessage(const QString &roomId_, RoomRequestError error_) :
            IMessage(ChannelTypeStatic(), MessageTypeStatic()),
            roomId(roomId_),
            error(error_)
        {
        }

        void RoomAssignedMessage::Serialize()
        {
            data["error"] = static_cast<int>(error);
            // Don't fill the rest of the data if error has been set
            if (error == RQE_NoError)
            {
                data["roomId"] = roomId;
                data["peerId"] = peerId;
            }
        }

        bool RoomAssignedMessage::Deserialize() 
        {
            error = static_cast<RoomRequestError>(data.value("error", 0).toInt());
            // Read rest of the data only if error has not been set.
            if (error == RQE_NoError)
            {
                roomId = data.value("roomId", "").toString();
                if (roomId.isEmpty())
                {
                    LogError("RoomAssignedMessage::Deserialize: Room id is empty AND no error enum is set!");
                    return false;
                }
                peerId = data.value("peerId", "").toString();
                if (!data.contains("peerId"))
                {
                    LogError("RoomAssignedMessage::Deserialize: Peer id is not set AND no error enum is set!");
                    return false;
                }
            }
            return true;
        }

        // RoomUserJoinedMessage

        RoomUserJoinedMessage::RoomUserJoinedMessage(const QStringList &peerIds_) :
            IMessage(ChannelTypeStatic(), MessageTypeStatic()),
            peerIds(peerIds_)
        {
        }

        void RoomUserJoinedMessage::Serialize()
        {
            data["peerIds"] = peerIds;
        }

        bool RoomUserJoinedMessage::Deserialize() 
        {
            peerIds = data.value("peerIds", QStringList()).toStringList();
            return !peerIds.isEmpty();
        }

        // RoomUserLeftMessage

        RoomUserLeftMessage::RoomUserLeftMessage(const QStringList &peerIds_) :
            IMessage(ChannelTypeStatic(), MessageTypeStatic()),
            peerIds(peerIds_)
        {
        }

        void RoomUserLeftMessage::Serialize()
        {
            data["peerIds"] = peerIds;
        }

        bool RoomUserLeftMessage::Deserialize() 
        {
            peerIds = data.value("peerIds", QStringList()).toStringList();
            return !peerIds.isEmpty();
        }
    }
   
    namespace Application
    {
        // RoomCustomMessage

        RoomCustomMessage::RoomCustomMessage(const QVariantList &receivers_, const QVariantMap &payload_) :
            IMessage(ChannelTypeStatic(), MessageTypeStatic()),
            receivers(receivers_),
            payload(payload_)
        {
        }

        void RoomCustomMessage::Serialize()
        {
            // The service will inject "sender" property.
            data["receivers"] = receivers;
            data["payload"] = payload;
        }

        bool RoomCustomMessage::Deserialize() 
        {
            sender = data.value("sender", QVariant());
            receivers = data.value("receivers", QVariantList()).toList();
            payload = data.value("payload", QVariantMap()).toMap();
            return true;
        }

        // PeerCustomMessage

        PeerCustomMessage::PeerCustomMessage(const QVariantMap &payload_) :
            IMessage(ChannelTypeStatic(), MessageTypeStatic()),
            payload(payload_)
        {
        }

        void PeerCustomMessage::Serialize()
        {
            data["payload"] = payload;
        }

        bool PeerCustomMessage::Deserialize()
        {
            payload = data.value("payload", QVariantMap()).toMap();
            return true;
        }
    }
}
