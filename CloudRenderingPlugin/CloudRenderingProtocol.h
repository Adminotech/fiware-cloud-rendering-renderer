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

#include "CoreTypes.h"

#include <QString>
#include <QVariant>
#include <QList>
#include <QDebug>

namespace CloudRenderingProtocol
{
    /// Cloud Rendering Room
    struct CloudRenderingRoom
    {
        CloudRenderingRoom();

        /// Room id.
        QString id;

        /// Room peers.
        QStringList peers;

        /// Returns if this room a peer.
        bool HasPeer(const QString &peerId) const;

        /// Adds a peer to this room.
        /** @return True if did not already exist and was added. */
        bool AddPeer(const QString &peerId);

        /// Removes a peer from this room.
        void RemovePeer(const QString &peerId);

        /// Clears the channel from all peers.
        void Reset();
    };

    // ConnectionState

    enum ConnectionState
    {
        CS_Invalid = 0,
        CS_Error = 1,
        CS_Connected = 2,
        CS_Disconnected = 3
    };
    
    // ChannelType

    enum ChannelType
    {
        CT_Invalid = 0,
        CT_Signaling = 1,
        CT_Room = 2,
        CT_State = 3,
        CT_Application = 4
    };

    const QString SignalingChannelType = "Signaling";
    const QString RoomChannelType = "Room";
    const QString StateChannelType = "State";
    const QString ApplicationChannelType = "Application";

    /// Converts channel type string to enum.
    inline static ChannelType ToChannelType(const QString &type)
    {
        if (type.isEmpty())
            return CT_Invalid;
        else if (type == SignalingChannelType)
            return CT_Signaling;
        else if (type == RoomChannelType)
            return CT_Room;
        else if (type == StateChannelType)
            return CT_State;
        else if (type == ApplicationChannelType)
            return CT_Application;
        return CT_Invalid;
    }

    /// Converts channel type enum to string.
    inline static QString ToChannelTypeName(ChannelType type)
    {
        if (type == CT_Signaling)
            return SignalingChannelType;
        else if (type == CT_Room)
            return RoomChannelType;
        else if (type == CT_State)
            return StateChannelType;
        else if (type == CT_Application)
            return ApplicationChannelType;
        return "";
    }

    // MessageType
    
    enum MessageType
    {
        MT_Invalid = 0,

        // "State" channel
        MT_Registration         = 1,
        MT_RendererStateChange  = 2,

        // "Signaling" channel
        MT_Offer                = 10,
        MT_Answer               = 11,
        MT_IceCandidates        = 12,

        // "Room" channel
        MT_RoomAssigned         = 20,
        MT_RoomUserJoined       = 21,
        MT_RoomUserLeft         = 22,
        
        // "Application" channel
        MT_RoomCustomMessage    = 30,
        MT_PeerCustomMessage    = 31
    };

    // "State" channel
    const QString Registration = "Registration";
    const QString RendererStateChange = "RendererStateChange";

    // "Signaling" channel
    const QString Offer = "Offer";
    const QString Answer = "Answer";
    const QString IceCandidates = "IceCandidates";

    // "Room" channel
    const QString RoomAssigned = "RoomAssigned";
    const QString RoomUserJoined = "RoomUserJoined";
    const QString RoomUserLeft = "RoomUserLeft";
    
    // "Application" channel
    const QString RoomCustom = "RoomCustomMessage";
    const QString PeerCustom = "PeerCustomMessage";

    /// Converts message type string to enum.
    inline static MessageType ToMessageType(const QString &type)
    {
        if (type.isEmpty())
            return MT_Invalid;
        // "State" channel
        else if (type == Registration)
            return MT_Registration;
        else if (type == RendererStateChange)
            return MT_RendererStateChange;
        // "Signaling" channel
        else if (type == Offer)
            return MT_Offer;
        else if (type == Answer)
            return MT_Answer;
        else if (type == IceCandidates)
            return MT_IceCandidates;
        // "Room" channel
        else if (type == RoomAssigned)
            return MT_RoomAssigned;
        else if (type == RoomUserJoined)
            return MT_RoomUserJoined;
        else if (type == RoomUserLeft)
            return MT_RoomUserLeft;
        // "Application" channel
        else if (type == RoomCustom)
            return MT_RoomCustomMessage;
        else if (type == PeerCustom)
            return MT_PeerCustomMessage;
        return MT_Invalid;
    }

    /// Converts message type enum to string.
    inline static QString ToMessageTypeName(MessageType type)
    {
        // "State" channel
        if (type == MT_Registration)
            return Registration;
        else if (type == MT_RendererStateChange)
            return RendererStateChange;
        // "Signaling" channel
        else if (type == MT_Offer)
            return Offer;
        else if (type == MT_Answer)
            return Answer;
        else if (type == MT_IceCandidates)
            return IceCandidates;
        // "Room" channel
        else if (type == MT_RoomAssigned)
            return RoomAssigned;
        else if (type == MT_RoomUserJoined)
            return RoomUserJoined;
        else if (type == MT_RoomUserLeft)
            return RoomUserLeft;
        // "Application" channel
        else if (type == MT_RoomCustomMessage)
            return RoomCustom;
        else if (type == MT_PeerCustomMessage)
            return PeerCustom;
        return "";
    }
    
    /// A generic message interface.
    /** The base interface provides the message type and the 
        full raw data available for the parsing logic. 
        
        @code
        {
            "channel" : <message-channel-as-string>,
            "message" :
            {
                "type" : <message-type-as-string>,
                "data" : 
                {
                    <message-data>
                }
            }
        }
        @endcode */
    class CLOUDRENDERING_API IMessage : public QObject
    {
        Q_OBJECT
        
    public:
        IMessage(ChannelType channelType_, MessageType messageType_);
        ~IMessage();

        /// Channel type.
        ChannelType channelType;

        /// Message type.
        MessageType messageType;

        /// Message raw data as a variant map.
        QVariantMap data;

    public slots:
        /// Returns the channel type.
        ChannelType Channel() const;
        
        /// Returns the message type.
        MessageType Type() const;

        /// Returns the channel type name.
        QString ChannelTypeName() const;
        
        /// Returns the message type name.
        QString MessageTypeName() const;

        /// Returns the message data.
        QVariantMap Data() const;

        /// Returns if this message is valid.
        /** @return True if message type could be resolved to a known protocol message type. */
        bool IsValid(bool serialization = false) const;

        /// Returns if this message is null.
        /** @note Message can be valid but still null for its data content.
            @return True if message data has any content. */
        bool IsNull() const;

        /// Deserializes the message from pre-parsed data.
        /** @param data_ Message data.
            @return True if deserialization succeeded. */
        bool FromData(const QVariantMap &data_);
        
        /// Deserializes the message from the input @c json.
        /** @param json JSON data.
            @return True if deserialization succeeded. */
        bool FromJSON(const QByteArray &json);
        
        /// Serializes the current type and data to JSON.
        /** @return JSON data. */
        QByteArray ToJSON(bool *ok = 0);

    private:
        /// IMessage implementation CAN override this function to validate
        /// data before serialization is invoked. Returning false will
        /// stop the message from being serialized. Log out errors if you can.
        virtual bool IsValidForSerialization() const { return true; }
        
        /// IMessage implementations MUST override this function.
        /** This function is called when the messages raw QVariantMap data
            needs to be prepared for serialization. Implementations should
            update their current state into the member variable IMessage::data. */
        virtual void Serialize()
        {
            qDebug() << "IMessage::Serialize() not overridden in a IMessage implementation!";
        }
        
        /// IMessage implementations MUST override this function.
        /** This function is called when the messages raw QVariantMap data
            has been read and the implementation should parse the message from it.
            Perform your parsing logic on the member variable IMessage::data.
            @return Returns if deserialization was successful. */
        virtual bool Deserialize()
        {
            qDebug() << "IMessage::Deserialize() not overridden in a IMessage implementation!";
            return false;
        }
    };
    
    /// Message shared ptr.
    typedef shared_ptr<IMessage> MessageSharedPtr;
    typedef QList<MessageSharedPtr> MessageSharedPtrList;

    /// Binary message.
    typedef QByteArray BinaryMessageData;

    /// Register script types.
    inline static void RegisterMetaTypes()
    {
        qRegisterMetaType<CloudRenderingProtocol::ConnectionState>("CloudRenderingProtocol::ConnectionState");
        qRegisterMetaType<CloudRenderingProtocol::ChannelType>("CloudRenderingProtocol::ChannelType");
        qRegisterMetaType<CloudRenderingProtocol::MessageType>("CloudRenderingProtocol::MessageType");
        qRegisterMetaType<CloudRenderingProtocol::MessageSharedPtr>("CloudRenderingProtocol::MessageSharedPtr");
        qRegisterMetaType<CloudRenderingProtocol::MessageSharedPtrList>("CloudRenderingProtocol::MessageSharedPtrList");
        qRegisterMetaType<CloudRenderingProtocol::BinaryMessageData>("CloudRenderingProtocol::BinaryMessageData");
    }

    /// Parses a message from input JSON data.
    /** @param json JSON data.
        @return Created message, null if parsing from @c json failed. */
    MessageSharedPtr CreateMessageFromJSON(const QByteArray &json);
    
    /// Dump json with pretty indentation to stdout.
    void DumpPrettyJSON(const QByteArray &json);
    
    namespace Signaling
    {
        /// Offer message
        /** The offer message is the first step in signaling a peer to peer WebRTC connection.
            It includes the offer senders SDP information and optionally ICE candidate(s) for the 
            WebRTC connection setup.
            
            The appropriate response for this message is an Answer message.
            
            @code
            // Web Client and Renderer -> Cloud Rendering Service
            {
                "channel" : "Signaling",
                "message" :
                {
                    "type" : "Offer",
                    "data" : 
                    {
                        // If not defined when sender is a web client
                        // "renderer" is assumed by the web service.
                        "receiverId" : <receiver-peer-id-as-string>,

                        "sdp" :
                        {
                            "type" : <sdp-type-as-string>,
                            "sdp"  : <sdp-as-string>
                        },
                        "iceCandidates" :
                        [
                            {
                                "sdpMLineIndex" : <ice-m-line-index-as-number>,
                                "sdpMid"        : <ice-sdp-mid-as-string>,
                                "candidate"     : <ice-candidate-as-string>
                            },
                            ...
                        ]
                    }
                }
            }
            
            // Cloud Rendering Service -> Web Client and Renderer
            {
                "channel" : "Signaling",
                "message" :
                {
                    "type" : "Offer",
                    "data" : 
                    {
                        "receiverId" : <receiver-peer-id-as-string>,
                        "senderId"   : <sender-peer-id-as-string>,

                        "sdp" :
                        {
                            "type" : <sdp-type-as-string>,
                            "sdp"  : <sdp-as-string>
                        },
                        "iceCandidates" :
                        [
                            {
                                "sdpMLineIndex" : <ice-m-line-index-as-number>,
                                "sdpMid"        : <ice-sdp-mid-as-string>,
                                "candidate"     : <ice-candidate-as-string>
                            },
                            ...
                        ]
                    }
                }
            }
            @endcode 
            
            @note ICE candidates may be empty, in that case 
            they will be sent with separate IceCandidatesMessage(s). */
        class CLOUDRENDERING_API OfferMessage : public IMessage
        {
            Q_OBJECT
        
        public:
            OfferMessage(const QString &receiverId_ = "");
            OfferMessage(int receiverId_);
                      
            /// Receivers peer ID.
            QString receiverId;

            /// Senders peer ID.
            QString senderId;

            /// Offers SDP information.
            WebRTC::SDP sdp;
            
            /// Offers ICE candidates.
            /** @note If empty the candidates will be sent 
                with separate IceCandidatesMessage(s). */
            WebRTC::ICECandidateList iceCandidates;

            /// Channel type.
            static ChannelType ChannelTypeStatic() { return CT_Signaling; }

            /// Message type.
            static MessageType MessageTypeStatic() { return MT_Offer; }

        private:
            /// IMessage override.
            void Serialize();

            /// IMessage override.
            bool Deserialize();
        };
        
        /// Answer message
        /** The answer message is the second step in signaling a peer to peer WebRTC connection.
            It includes the answer senders SDP information and optionally ICE candidate(s) for the 
            WebRTC connection setup. 

            @code
            // Web Client and Renderer -> Cloud Rendering Service
            {
                "channel" : "Signaling",
                "message" :
                {
                    "type" : "Answer",
                    "data" : 
                    {
                        // If not defined when sender is a web client
                        // "renderer" is assumed by the web service.
                        "receiverId" : <receiver-peer-id-as-string>,

                        "sdp" :
                        {
                            "type" : <sdp-type-as-string>,
                            "sdp"  : <sdp-as-string>
                        },
                        "iceCandidates" :
                        [
                            {
                                "sdpMLineIndex" : <ice-m-line-index-as-number>,
                                "sdpMid"        : <ice-sdp-mid-as-string>,
                                "candidate"     : <ice-candidate-as-string>
                            },
                            ...
                        ]
                    }
                }
            }
            
            // Cloud Rendering Service -> Web Client and Renderer
            {
                "channel" : "Signaling",
                "message" :
                {
                    "type" : "Answer",
                    "data" : 
                    {
                        "receiverId" : <receiver-peer-id-as-string>,
                        "senderId"   : <sender-peer-id-as-string>,

                        "sdp" :
                        {
                            "type" : <sdp-type-as-string>,
                            "sdp"  : <sdp-as-string>
                        },
                        "iceCandidates" :
                        [
                            {
                                "sdpMLineIndex" : <ice-m-line-index-as-number>,
                                "sdpMid"        : <ice-sdp-mid-as-string>,
                                "candidate"     : <ice-candidate-as-string>
                            },
                            ...
                        ]
                    }
                }
            }
            @endcode
                        
            @note ICE candidates may be empty, in that case 
            they MUST be sent with separate IceCandidates message(s). */
        class CLOUDRENDERING_API AnswerMessage : public IMessage
        {
            Q_OBJECT

        public:
            AnswerMessage(const QString &receiverId_ = "");
            AnswerMessage(int receiverId_);

            /// Receivers peer ID.
            QString receiverId;

            /// Senders peer ID.
            QString senderId;

            /// Offers SDP information.
            WebRTC::SDP sdp;

            /// Offers ICE candidates.
            /** @note If empty the candidates will be sent 
                with separate IceCandidatesMessage(s). */
            WebRTC::ICECandidateList iceCandidates;

            /// Channel type.
            static ChannelType ChannelTypeStatic() { return CT_Signaling; }

            /// Message type.
            static MessageType MessageTypeStatic() { return MT_Answer; }

        private:
            /// IMessage override.
            void Serialize();

            /// IMessage override.
            bool Deserialize();
        };
        
        /// ICE candidates message
        /** This message can be sent from peer to peer to inform the other
            party of ICE candidates for establishing the WebRTC connection.
            
            @code
            // Web Client and Renderer -> Cloud Rendering Service
            {
                "channel" : "Signaling",
                "message" :
                {
                    "type"   : "IceCandidates",
                    "data"   : 
                    {
                        "receiverId" : <receiver-peer-id-as-string>,

                        "iceCandidates" :
                        [
                            {
                                "sdpMLineIndex" : <ice-m-line-index-as-number>,
                                "sdpMid"        : <ice-sdp-mid-as-string>,
                                "candidate"     : <ice-candidate-as-string>
                            },
                            ...
                        ]
                    }
                }
            }

            // Cloud Rendering Service -> Web Client and Renderer
            {
                "channel" : "Signaling",
                "message" :
                {
                    "type" : "IceCandidates",
                    "data" : 
                    {
                        "receiverId" : <receiver-peer-id-as-string>,
                        "senderId"   : <sender-peer-id-as-string>,
    
                        "iceCandidates" :
                        [
                            {
                                "sdpMLineIndex" : <ice-m-line-index-as-number>,
                                "sdpMid"        : <ice-sdp-mid-as-string>,
                                "candidate"     : <ice-candidate-as-string>
                            },
                            ...
                        ]
                    }
                }
            }
            @endcode */
        class CLOUDRENDERING_API IceCandidatesMessage : public IMessage
        {
            Q_OBJECT

        public:
            IceCandidatesMessage(const QString &receiverId_ = "", WebRTC::ICECandidateList iceCandidates_ = WebRTC::ICECandidateList());
            IceCandidatesMessage(int receiverId_, WebRTC::ICECandidateList iceCandidates_ = WebRTC::ICECandidateList());

            /// Receivers peer ID.
            QString receiverId;

            /// Senders peer ID.
            QString senderId;

            /// ICE candidates.
            WebRTC::ICECandidateList iceCandidates;

            /// Channel type.
            static ChannelType ChannelTypeStatic() { return CT_Signaling; }

            /// Message type.
            static MessageType MessageTypeStatic() { return MT_IceCandidates; }

        private:
            /// IMessage override.
            void Serialize();

            /// IMessage override.
            bool Deserialize();
        };
    }
    
    namespace State
    {
        /// Registration message.
        /** A renderer and a client needs to register itself to the Cloud Rendering Service,
            this is the first message that MUST be sent once the WebSocket connection has been
            established.
            
            <b>Renderer Registration</b>
            
            For the renderer registration implies that it is online and can be assigned to any
            room without a renderer by the service. The renderer will receive a RoomAssignedMessage 
            response to a registration message.
            
            The renderer may also choose to create a new private room. The intention here is that
            a renderer registers itself and wants to create a room, not be assigned as the renderer
            to any client room request. This allows the renderer to immediately get a new empty room
            and send the room information forward to clients it wants to invite to its rendering.
            This option is especially needed when a renderer (eg. web browser renderer implementation)
            wants to share its web camera or desktop feed with specific individual clients. It is
            then needed for the renderer to notify this intent to the service so the scenario can
            be supported.
            
            Renderers creating their own rooms is very important for certain type of
            application that do not want to provide renderers just for anyone to use, but for an
            approach when the implementation is more in control who gets the room information and
            can join into a specific renderers room.
            
            The "createPrivateRoom" boolean property is defaulted to false if not defined. The
            renderer MUST set this property to true if it wants a private room assignment after the
            registration.
            
            <b>Client Registration</b>
            
            For a client it is a request for a new room or if roomId is a non-empty string
            a the client will be joined to an existing room. The response for the registration
            message is a RoomAssignedMessage. This message will have the error code RQE_DoesNotExist
            set if the requested room did not exist.

            Any custom registration properties needs to be set to message.data. These custom
            properties can be used by the Cloud Rendering GE implementation for example 
            for authentication logic.

            @code
            // Web Client -> Cloud Rendering Service
            {
                "channel" : "State",
                "message" :
                {
                    "type" : "Registration",
                    "data" : 
                    {
                        "registrant" : "client",
                        "roomId"     : <optional-room-id-to-join-as-string>,

                        // The remaining structure of the data is decided by the Cloud Rendering GE
                        // implementation. For example auth tokens to authenticate the registrant.
                        <application-specific-data>
                    }
                }
            }

            // Renderer -> Cloud Rendering Service
            {
                "channel" : "State",
                "message" :
                {
                    "type" : "Registration",
                    "data" : 
                    {
                        "registrant"        : "renderer",
                        "createPrivateRoom" : <boolean>,

                        // The remaining structure of the data is decided by the Cloud Rendering GE
                        // implementation. For example auth tokens to authenticate the registrant.
                        <application-specific-data>
                    }
                }
            }
            @endcode */
        class CLOUDRENDERING_API RegistrationMessage : public IMessage
        {
            Q_OBJECT
            
        public:
            // Registrant type enumeration.
            /** @code
                R_Renderer    1 = Registering a renderer
                R_Client      2 = Registering a client
                @endcode 
                @see ToRegistrantTypeName() ToRegistrantType() */
            enum Registrant
            {
                R_Unkown = 0,
                R_Renderer = 1,
                R_Client = 2
            };

            RegistrationMessage(Registrant registrant_ = R_Unkown, const QString &roomId_ = "");
            
            /// Registrant type.
            Registrant registrant;

            /// Room id.
            /** @note This property is only relevant for client registrations
                to join a existing room instead of creating a new room. */
            QString roomId;

            /// Returns a valid registrant type name "renderer" or "client" for @c reg.
            QString ToRegistrantTypeName(Registrant reg);

            /// Returns a valid registrant type enum for type name @c reg.
            Registrant ToRegistrantType(const QString &reg);
            
            /// Channel type.
            static ChannelType ChannelTypeStatic() { return CT_State; }

            /// Message type.
            static MessageType MessageTypeStatic() { return MT_Registration; }

        private:
            /// IMessage override.
            void Serialize();

            /// IMessage override.
            bool Deserialize();
        };

        /// Renderer state change message
        /** This message is sent from renderer to the cloud rendering 
            web service to inform about state changes.
            
            @code
            {
                "channel" : "State",
                "message" :
                {
                    "type" : "RendererStateChange",
                    "data" : 
                    {
                        "state" : <state-id-as-number>
                    }
                }
            }
            @endcode 
            
            @note When RegistrationMessage is sent the default state is
            RendererStateChangeMessage::RS_Online. It does not need to be sent separately. */
        class CLOUDRENDERING_API RendererStateChangeMessage : public IMessage
        {
            Q_OBJECT

        public:           
            /// Renderer state enumeration.
            /** @code
                RS_Offline   1 = Renderer is offline, new clients should not be redirected here.
                RS_Online    2 = Renderer is online and ready to server new clients.
                RS_Full      3 = Renderers client limit has been reached, no new clients should be redirected here.
                @endcode */
            enum State
            {
                RS_Invalid = 0,
                RS_Offline = 1,
                RS_Online = 2,
                RS_Full = 3
            };

            RendererStateChangeMessage(State state_);
        
            /// New renderer state.
            State state;

            /// Channel type.
            static ChannelType ChannelTypeStatic() { return CT_State; }

            /// Message type.
            static MessageType MessageTypeStatic() { return MT_RendererStateChange; }

        private:
            /// IMessage override.
            void Serialize();

            /// IMessage override.
            bool Deserialize();
        };
    }
    
    namespace Room
    {        
        /// %Room assigned message
        /** This message is sent as a response to RegistrationMessage.
            
            The data contains the room id you have been assigned to and your
            unique peer id inside the room. If the error code is set to anything but
            0 (NoError) the roomId and peerId properties SHOULD be omitted by the
            sender and SHOULD be ignored by the receiver if set to a value.
            
            The error code will tell you why you could not be assigned to a room.
            
            This message is also sent to the renderer once its assigned to a room.
            The peer id CAN be omitted when the message is sent to a renderer.
            
            This message is followed by a RoomUserJoined message that will have
            the full list of peer id:s currently in the channel. This set will also
            includes your own peer id that has been sent to you in this message.
            
            @code
            {
                "channel" : "Room",
                "message" :
                {
                    "type" : "RoomAssigned",
                    "data" : 
                    {
                        "error"  : <error-as-number>,
                        
                        "roomId" : <room-id-as-string>,
                        "peerId" : <your-peer-id-in-the-room-as-number>
                    }
                }
            }
            @endcode */
        class CLOUDRENDERING_API RoomAssignedMessage : public IMessage
        {
            Q_OBJECT

        public:
            /// Request error enumeration.
            /** @code
                RQE_NoError       0 = No errors, you have been assigned to "roomId".
                RQE_ServiceError  1 = Room could not be server, internal service error.
                RQE_DoesNotExist  2 = Requested room id does not exist. Send new request without room id to create a new room.
                RQE_Full          3 = Requested room is full, try again later.
                @endcode */
            enum RoomRequestError
            {
                RQE_NoError = 0,
                RQE_ServiceError = 1,
                RQE_DoesNotExist = 2,
                RQE_Full = 3
            };

            RoomAssignedMessage(const QString &roomId_ = "", RoomRequestError error_ = RQE_NoError);

            /// Room id.
            QString roomId;
            
            /// Your assigned peer id in this room.
            QString peerId;
            
            /// Request error.
            RoomRequestError error;

            /// Channel type.
            static ChannelType ChannelTypeStatic() { return CT_Room; }

            /// Message type.
            static MessageType MessageTypeStatic() { return MT_RoomAssigned; }

        private:
            /// IMessage override.
            void Serialize();

            /// IMessage override.
            bool Deserialize();
        };
        
        /// %Room user joined message
        /** This message is sent when new user(s) join the current room you are assigned to.

            This message MUST NOT be sent to any client or a renderer before they have been
            assigned to a room with a RoomAssigned message.

            @code
            {
                "channel" : "Room",
                "message" :
                {
                    "type" : "RoomUserJoined",
                    "data" : 
                    {
                        "peerIds" :
                        [
                            <peer-id-1-as-string>,
                            <peer-id-2-as-string>,
                            ...
                        ]
                    }
                }
            }
            @endcode */
        class CLOUDRENDERING_API RoomUserJoinedMessage : public IMessage
        {
            Q_OBJECT

        public:
            RoomUserJoinedMessage(const QStringList &peerIds_ = QStringList());

            /// List of user id:s that joined the room.
            QStringList peerIds;
            
            /// Channel type.
            static ChannelType ChannelTypeStatic() { return CT_Room; }

            /// Message type.
            static MessageType MessageTypeStatic() { return MT_RoomUserJoined; }

        private:
            /// IMessage override.
            void Serialize();

            /// IMessage override.
            bool Deserialize();
        };
        
        /// %Room user left message
        /** This message is sent when user(s) leave the current room you are assigned to.

            This message MUST NOT be sent to any client or a renderer before they have been
            assigned to a room with a RoomAssigned message.

            @code
            {
                "channel" : "Room",
                "message" :
                {
                    "type" : "RoomUserLeft",
                    "data" : 
                    {
                        "peerIds" :
                        [
                            <peer-id-1-as-string>,
                            <peer-id-2-as-string>,
                            ...
                        ]
                    }
                }
            }
            @endcode */
        class CLOUDRENDERING_API RoomUserLeftMessage : public IMessage
        {
            Q_OBJECT

        public:
            RoomUserLeftMessage(const QStringList &peerIds_ = QStringList());

            /// List of user id:s that left the room.
            QStringList peerIds;
            
            /// Channel type.
            static ChannelType ChannelTypeStatic() { return CT_Room; }

            /// Message type.
            static MessageType MessageTypeStatic() { return MT_RoomUserLeft; }

        private:
            /// IMessage override.
            void Serialize();

            /// IMessage override.
            bool Deserialize();
        };       
    }
    
    namespace Application
    {
        /// %Room custom message
        /** This message can be used by the application to send custom message between clients, the Cloud Rendering Service
            and the Renderer in the same room.
            
            The Service and Renderer do not have a peer id assigned to them, when you want to send messages to these two
            components you will use reserved special meaning string identifiers instead. The reserver string
            identifiers are "service" and "renderer". These identifiers can be encountered in the 'sender' property
            and can be used in the 'receivers' property.
            
            You can send a 1-to-1 message by assigning the other peers id to peerIds, or 1-to-many by assigning multiple id:s.
            
            For 1-to-all the peerIds list can be empty, if empty the message will be sent to all other peers and the Renderer in the room.
            You don't need to fill the peerIds list with all the peer ids for each client in the room, this is done automatically by
            the service if the list is empty.
            
            Any custom message properties MAY be set to message.data.payload.
            
            Custom messages MUST NOT be sent before you have been assigned to a room with a RoomAssigned message.
            
            The Cloud Rendering Service MUST inject the 'sender' property into the message before passing it forward to the receivers.
            Meaning that the sender SHOULD NOT set the 'sender' property to and if it will the service MUST override it.

            @code
            {
                "channel" : "Application",
                "message" :
                {
                    "type" : "RoomCustomMessage",
                    "data" : 
                    {
                        // Service will inject this, does not needed to be filled 
                        // on the client or the renderer when sending the message.
                        "sender" : <sender-peer-or-reserved-id-as-string>,

                        "receivers" :
                        [
                            <reserved-string-identifier-1>,
                            ...
                            <receiver-peer-1-id-as-string>,
                            <receiver-peer-2-id-as-string>,
                            ...
                        ],

                        "payload" :
                        {
                            // The structure of the data is decided by the application and the web client implementation.
                            <application-specific-data>
                        }
                    }
                }
            }
            
            // Example 1: Client sends to all peers and the renderer in the room.
            {
                "channel" : "Application",
                "message" :
                {
                    "type" : "RoomCustomMessage",
                    "data" : 
                    {
                        "payload" : { "nick" : "John Doe" }
                    }
                }
            }
            
            // Example 2: Client sends to message to a single peer.
            {
                "channel" : "Application",
                "message" :
                {
                    "type" : "RoomCustomMessage",
                    "data" : 
                    {
                        "receivers"  : [ "2" ]
                        "payload"    :
                        { 
                            "type"    : "PrivateMessage",
                            "message" : "Hey, long time no see. Whats up?"
                        }
                    }
                }
            }
            
            // Example 3: Client sends message to peers 3, 5 and the renderer
            {
                "channel" : "Application",
                "message" :
                {
                    "type" : "RoomCustomMessage",
                    "data" : 
                    {
                        "receivers"  : [ "renderer", "3", "5" ],
                        "payload"    :
                        {
                            "message" : "Hello my name is John",
                            "nick"    : "John Doe"
                        }
                    }
                }
            }
            
            // Example 4: Renderer sending a message to the Service
            {
                "channel" : "Application",
                "message" :
                {
                    "type" : "RoomCustomMessage",
                    "data" : 
                    {
                        "receivers"  : [ "service" ],
                        "payload"    :
                        {
                            "type"   : "KickUser",
                            "peerId" : 2
                        }
                    }
                }
            }
            @endcode */
        class CLOUDRENDERING_API RoomCustomMessage : public IMessage
        {
            Q_OBJECT

        public:
            RoomCustomMessage(const QVariantList &receivers_ = QVariantList(), const QVariantMap &payload_ = QVariantMap());
            
            /// Sender of the message.
            /** Either a valid peer id as a number or a special string identifier "renderer" or "service". */
            QVariant sender;
            
            /// List of receiving peer id:s that this message should be sent to.
            /** @note If this list is empty the Cloud Rendering Service MUST fill it with
                all peer id:s and the special identifier "renderer" in the room. 
                Clients sending a custom message can leave this list empty
                if it wants to send the message to all participants. */
            QVariantList receivers;

            /// Message payload.
            QVariantMap payload;
            
            /// Channel type.
            static ChannelType ChannelTypeStatic() { return CT_Application; }

            /// Message type.
            static MessageType MessageTypeStatic() { return MT_RoomCustomMessage; }

        private:
            /// IMessage override.
            void Serialize();

            /// IMessage override.
            bool Deserialize();
        };

        /// %Peer custom message
        /** This message can be used by the application to send custom message between any two peers via a WebRTC
            data channel. This message does not contain sender or receiver id information because it is never relayed
            via the web service. The needed sender information is known from the WebRTC protocol by the application
            handling the messages.

            These data channel messages should be used when there is no need for one-to-many communication and
            when you require real time/very frequent communication. The data channel skips the overhead and latency
            that is added by relaying the message via the web service with WebSocket.

            Any custom message properties MAY be set to message.data.payload.

            @code
            {
                "channel" : "Application",
                "message" :
                {
                    "type" : "PeerCustomMessage",
                    "data" :
                    {
                        "payload" :
                        {
                            // The structure of the data is decided by the application and the web client implementation.
                            <application-specific-data>
                        }
                    }
                }
            }
            @endcode */
        class CLOUDRENDERING_API PeerCustomMessage : public IMessage
        {
            Q_OBJECT

        public:
            PeerCustomMessage(const QVariantMap &payload_ = QVariantMap());

            /// Message payload.
            QVariantMap payload;

            /// Channel type.
            static ChannelType ChannelTypeStatic() { return CT_Application; }

            /// Message type.
            static MessageType MessageTypeStatic() { return MT_PeerCustomMessage; }

        private:
            /// IMessage override.
            void Serialize();

            /// IMessage override.
            bool Deserialize();
        };
    }
}
