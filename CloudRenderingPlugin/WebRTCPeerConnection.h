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

#include "talk/base/common.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/base/scoped_ref_ptr.h"
#include "talk/base/scoped_ptr.h"

#include "WebRTCMediaConstraints.h"

#include <QVariant>
#include <QPointer>
#include <QDebug>

namespace WebRTC
{          
    class CLOUDRENDERING_API PeerConnection : public QObject, 
                                         public webrtc::PeerConnectionObserver,
                                         public webrtc::CreateSessionDescriptionObserver,
                                         public webrtc::DataChannelObserver
    {
        Q_OBJECT

    public:
        PeerConnection(Framework *framework, const QString &peerId);
        ~PeerConnection();
        
        /// Connection settings.
        struct ConnectionSettings
        {
            /// If default OS microphone audio is sent.
            bool audio;
            /// If default OS web camera video is sent.
            bool webcamera;
            /// If Tundra rendering video is sent.
            bool rendering;
            /// If a data channel is used.
            bool data;
            
            ConnectionSettings(bool _audio = false,
                               bool _webcamera = false,
                               bool _rendering = false,
                               bool _data = false) :
                audio(_audio),
                webcamera(_webcamera), 
                rendering(_rendering),
                data(_data)
            {
            }

            /// Returns if anything should be sent to the WebRTC connection.
            bool AllDisabled() const
            {
                return (!audio && !webcamera && !rendering && !data);
            }
        };

    public slots:
        /// Returns the peer id.
        QString Id() const;
        
        /// Returns the full peer custom data.
        QVariantMap CustomData() const;
        
        /// Returns a value for key from the peer custom data.
        QVariant CustomData(const QString &key) const;
        
        /// Sets the full peer custom data.
        void SetCustomData(const QVariantMap &customData);
        
        /// Sets a key to a value to the peer custom data.
        void SetCustomData(const QString &key, const QVariant &value);
        
        /// Disconnects this peer connection.
        void Disconnect();

        /// Creates a WebRTC offer.
        /** The connection data signals are emitted when the offer is ready.
            The emitted SDP will be type of 'offer' and can be sent to the answering peer. */
        void CreateOffer(ConnectionSettings settings);
        
        /// Handles an incoming offer.
        /** Call this function when you receive a offer from the other peer.
            The connection data signals are emitted when the answer for the offer is ready.
            The emitted SDP will be type of 'answer' and can be sent to the offer peer. */
        void HandleOfferOrAnswer(const WebRTC::SDP &sdp, const WebRTC::ICECandidateList &iceCandidates, ConnectionSettings answerSettings);
        
        /// Add remote ICE candidates.
        void AddRemoteIceCandidates(const WebRTC::ICECandidateList iceCandidates);

        /// Returns if local ICE candidates have been resolved.
        bool AreLocalIceCandidatesResolved() const;
        
        /// Returns local ICE candidates.
        WebRTC::ICECandidateList PendingLocalIceCandidates() const;
        
        /// Clears local ICE candidates.
        void ClearLocalIceCandidates();

    private slots:
        void Reset();
        void AddStreams(const ConnectionSettings &settings);
        
        cricket::VideoCapturer* OpenTundraCaptureDevice();
        cricket::VideoCapturer* OpenVideoCaptureDevice();
        
        void EmitResolvedSignals();
        
    signals:
        /// Emitted when creating a offer or an answer when both SDP and ICE candidates have been resolved.
        /** Listen to this signal when you want to send a complete offer or answer with full SDP and ICE candidate information. */
        void LocalConnectionDataResolved(WebRTC::SDP sdp, WebRTC::ICECandidateList candidates);
        
        /// Emitted when local SDP information has been resolved.
        void LocalSDPResolved(WebRTC::SDP sdp);
        
        /// Emitted when local ICE candidates information has been resolved.
        void LocaleIceCandidatesResolved(WebRTC::ICECandidateList candidates);
        
        /// Emitted when a data channel message has been received.
        void DataChannelMessage(CloudRenderingProtocol::MessageSharedPtr message);

        /// Emitted when a binary data channel message has been received.    
        void DataChannelMessage(const CloudRenderingProtocol::BinaryMessageData &data);

    public:
        /// webrtc::PeerConnectionObserver overrides.
        void OnError();
        void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state);
        void OnStateChange(StateType state_changed);
        void OnAddStream(webrtc::MediaStreamInterface* stream);
        void OnRemoveStream(webrtc::MediaStreamInterface* stream);
        void OnDataChannel(webrtc::DataChannelInterface* data_channel);
        void OnRenegotiationNeeded();
        void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state);
        void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state);
        void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
        void OnIceComplete();

        /// webrtc::CreateSessionDescriptionObserver overrides.
        void OnSuccess(webrtc::SessionDescriptionInterface* desc);
        void OnFailure(const std::string& error);
        
        /// talk_base::RefCountInterface overrides.
        int AddRef();
        int Release();
        
        /// webrtc::DataChannelObserver overrides.
        void OnStateChange();
        void OnMessage(const webrtc::DataBuffer& buffer);
    
    private:
        bool InitializePeerConnection(const ConnectionSettings &settings);
        bool IsPreviewRenderingEnabled() const;
        
        QString LC;
        
        Framework *framework_;
        int refCount_;

        QString peerId_;
        QVariantMap peerCustomData_;
        
        QString SignalingStateToString(webrtc::PeerConnectionInterface::SignalingState state);
        QString IceConnectionStateToString(webrtc::PeerConnectionInterface::IceConnectionState state);
        QString IceGatheringStateToString(webrtc::PeerConnectionInterface::IceGatheringState state);

        talk_base::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection_;
        talk_base::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory_;
        talk_base::scoped_refptr<webrtc::DataChannelInterface> dataChannel_;
        
        MediaConstraints mediaConstraints_;

        QList<QPointer<VideoRenderer> > activeRenderers_;

        ICECandidateList pendingLocalIceCandidates_;
        ICECandidateList pendingRemoteIceCandidates_;
        
        SDP localSDP_;
        SDP remoteSDP_;

        bool localSDPResolved_;
        bool remoteSDPSet_;
        bool localIceCandidatesResolved_;
        
        bool localSDPEmitted_;
        bool localIceEmitted_;
        bool localBothEmitted_;
    };
    
    /// @cond PRIVATE
    
    class DummySetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver
    {
    public:
        static DummySetSessionDescriptionObserver* Create()
        {
            return new talk_base::RefCountedObject<DummySetSessionDescriptionObserver>();
        }
        
        virtual void OnSuccess() {}
        virtual void OnFailure(const std::string& error) {}

    protected:
        DummySetSessionDescriptionObserver() {}
        ~DummySetSessionDescriptionObserver() {}
    };
    
    /// @endcond
}
