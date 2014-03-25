/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#include "WebRTCPeerConnection.h"
#include "WebRTCUtils.h"
#include "WebRTCVideoRenderer.h"
#include "WebRTCTundraCapturer.h"

#include "CloudRenderingPlugin.h"

#include "Framework.h"
#include "LoggingFunctions.h"

#include <QTimer>

#include "talk/app/webrtc/videosourceinterface.h"
#include "talk/media/devices/devicemanager.h"
#include "talk/base/windowpicker.h"
#include "talk/base/ssladapter.h"

namespace WebRTC
{       
    static std::string kAudioLabel = "audio_label";
    static std::string kVideoLabel = "video_label";
    static std::string kDataLabel = "data_label";
    static std::string kStreamLabel = "stream_label";
    
    PeerConnection::PeerConnection(Framework *framework, const QString &peerId) :
        LC("[WebRTC::PeerConnection]: "),
        framework_(framework),
        peerId_(peerId),
        refCount_(0),        
        localSDPResolved_(false),
        remoteSDPSet_(false),
        localIceCandidatesResolved_(false),
        localSDPEmitted_(false),
        localIceEmitted_(false),
        localBothEmitted_(false)
    {       
    }

    PeerConnection::~PeerConnection()
    {
        Reset();
    }
    
    QString PeerConnection::Id() const
    {
        return peerId_;
    }

    QVariantMap PeerConnection::CustomData() const 
    {
        return peerCustomData_;
    }

    QVariant PeerConnection::CustomData(const QString &key) const
    {
        return peerCustomData_.value(key, QVariant());
    }

    void PeerConnection::SetCustomData(const QVariantMap &customData)
    {
        peerCustomData_ = customData;
    }

    void PeerConnection::SetCustomData(const QString &key, const QVariant &value)
    {
        peerCustomData_[key] = value;
    }
    
    void PeerConnection::Reset()
    {
        talk_base::CleanupSSL();

        // Release media/track shader ptrs
        foreach(QPointer<VideoRenderer> renderer, activeRenderers_)
            if (renderer) renderer->Close();
        activeRenderers_.clear();
        
        // These are scoped ptrs that decrement the shared ref in the dtor.
        peerConnection_ = 0;
        peerConnectionFactory_ = 0;
        
        localSDPResolved_ = false;
        remoteSDPSet_ = false;
        localIceCandidatesResolved_ = false;
        
        localSDPEmitted_ = false;
        localIceEmitted_ = false;
        localBothEmitted_ = false;
    }
    
    void PeerConnection::CreateOffer(ConnectionSettings settings)
    {
        if (InitializePeerConnection(settings))
            peerConnection_->CreateOffer(this, NULL);
    }

    void PeerConnection::Disconnect()
    {
        if (peerConnection_.get())
            peerConnection_->Close();
        Reset();
    }
    
    void PeerConnection::AddRemoteIceCandidates(const WebRTC::ICECandidateList iceCandidates)
    {
        if (!peerConnection_.get())
        {
            LogError(LC + "AddRemoteIceCandidates: PeerConnection not initialized!");
            return;
        }
        
        if (!localIceCandidatesResolved_)
        {
            LogDebug(QString("  >> Storing %1 remote ICE candidates for later use...").arg(iceCandidates.size()));
            pendingRemoteIceCandidates_ += iceCandidates;
            return;
        }
        else
            LogDebug(LC + "Setting remote ICE candidates");

        for (int i=0; i<iceCandidates.size(); ++i)
        {
            const ICECandidate &iceCandidate = iceCandidates[i];
            talk_base::scoped_ptr<webrtc::IceCandidateInterface> candidate(
                webrtc::CreateIceCandidate(iceCandidate.sdpMid.toStdString(), iceCandidate.sdpMLineIndex, iceCandidate.candidate.toStdString()));
            if (!candidate.get())
            {
                LogError(LC + "Failed to create WebRTC IceCandidateInterface from remote peer information.");
                return;
            }

            if (peerConnection_->AddIceCandidate(candidate.get()))
                LogDebug(LC + QString("  >> Added remote ICE candidate %1").arg(i));
            else
                LogError(LC + QString("  >> Failed to add received ice candidate %1").arg(i));
        }
    }
    
    void PeerConnection::HandleOfferOrAnswer(const WebRTC::SDP &remoteSdp, const WebRTC::ICECandidateList &iceCandidates, ConnectionSettings answerSettings)
    {
        if (IsLogChannelEnabled(LogChannelDebug))
            qDebug() << "HandleOfferOrAnswer";

        if (!peerConnection_.get() && !InitializePeerConnection(answerSettings))
            return;
            
        webrtc::SessionDescriptionInterface *sdp = webrtc::CreateSessionDescription(remoteSdp.type.toStdString(), remoteSdp.sdp.toStdString());
        if (!sdp)
        {
            LogError(LC + "Failed to create WebRTC SessionDescriptionInterface from remote peer information.");
            return;
        }
        if (IsLogChannelEnabled(LogChannelDebug))
            qDebug() << "  >> Setting remote SDP";
        peerConnection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(), sdp);
        if (IsLogChannelEnabled(LogChannelDebug))
            qDebug() << "  >> DONE";
        remoteSDPSet_ = true;
        
        if (sdp->type() == webrtc::SessionDescriptionInterface::kOffer)
        {
            LogDebug(LC + "  >> Creating answer SDP");
            peerConnection_->CreateAnswer(this, NULL);
        }
        
        AddRemoteIceCandidates(iceCandidates);
    }
    
    void PeerConnection::AddStreams(const ConnectionSettings &settings)
    {
        if (settings.AllDisabled())
            return;

        talk_base::scoped_refptr<webrtc::MediaStreamInterface> stream = peerConnectionFactory_->CreateLocalMediaStream(kStreamLabel);
        
        if (settings.audio)
        {
            if (IsLogChannelEnabled(LogChannelDebug))
                qDebug() << "  >> Adding audio stream";

            talk_base::scoped_refptr<webrtc::AudioTrackInterface> audioTrack(peerConnectionFactory_->CreateAudioTrack(
                kAudioLabel, peerConnectionFactory_->CreateAudioSource(NULL)));
            stream->AddTrack(audioTrack);
        }
        
        if (settings.webcamera)
        {
            if (IsLogChannelEnabled(LogChannelDebug))
                qDebug() << "  >> Adding web camera video stream";

            talk_base::scoped_refptr<webrtc::VideoTrackInterface> videoTrack(peerConnectionFactory_->CreateVideoTrack(
                kVideoLabel, peerConnectionFactory_->CreateVideoSource(OpenVideoCaptureDevice(), NULL)));
            stream->AddTrack(videoTrack);

            if (IsPreviewRenderingEnabled())
                activeRenderers_ << new VideoRenderer(videoTrack, true, "Local Video Stream");
        }

        // Server rendering video from server
        if (settings.rendering)
        {
            if (IsLogChannelEnabled(LogChannelDebug))
                qDebug() << "  >> Adding 3D rendering stream";

            talk_base::scoped_refptr<webrtc::VideoTrackInterface> videoTrack(peerConnectionFactory_->CreateVideoTrack(
                kVideoLabel, peerConnectionFactory_->CreateVideoSource(OpenTundraCaptureDevice(), NULL)));
            stream->AddTrack(videoTrack);

            if (IsPreviewRenderingEnabled())
                activeRenderers_ << new VideoRenderer(videoTrack, false, "Local Tundra Stream"); 
        }
        
        if (settings.data)
        {
            if (IsLogChannelEnabled(LogChannelDebug))
                qDebug() << "  >> Adding data channel";

            dataChannel_ = peerConnection_->CreateDataChannel(kDataLabel, NULL);
            if (dataChannel_.get())
                dataChannel_->RegisterObserver(this);
            else
                LogError(LC + "Failed to create data channel!");
        }

        if (!peerConnection_->AddStream(stream, NULL))
            LogError(LC + "Adding stream to PeerConnection failed");
    }
    
    cricket::VideoCapturer* PeerConnection::OpenTundraCaptureDevice()
    {
        return new WebRTC::TundraCapturer(framework_);
    }

    cricket::VideoCapturer* PeerConnection::OpenVideoCaptureDevice()
    {
        talk_base::scoped_ptr<cricket::DeviceManagerInterface> deviceManager(cricket::DeviceManagerFactory::Create());
        if (!deviceManager->Init())
        {
            LogError(LC + "Can't create device manager");
            return 0;
        }
        std::vector<cricket::Device> devices;
        if (!deviceManager->GetVideoCaptureDevices(&devices))
        {
            LogError(LC + "Can't enumerate video devices");
            return 0;
        }
        
        cricket::VideoCapturer* capturer = 0;
        for (std::vector<cricket::Device>::iterator iter = devices.begin(); iter != devices.end(); ++iter)
        {
            capturer = deviceManager->CreateVideoCapturer(*iter);
            if (capturer != 0)
                break;
        }
        return capturer;
    }
    
    QString PeerConnection::SignalingStateToString(webrtc::PeerConnectionInterface::SignalingState state)
    {
        switch(state)
        {
            case webrtc::PeerConnectionInterface::kStable: return "Stable";
            case webrtc::PeerConnectionInterface::kHaveLocalOffer: return "HaveLocalOffer";
            case webrtc::PeerConnectionInterface::kHaveLocalPrAnswer: return "HaveLocalPrAnswer";
            case webrtc::PeerConnectionInterface::kHaveRemoteOffer: return "HaveRemoteOffer";
            case webrtc::PeerConnectionInterface::kHaveRemotePrAnswer: return "HaveRemotePrAnswer";
            case webrtc::PeerConnectionInterface::kClosed: return "Closed";
        }
        return "Unknown Signaling State";
    }
    
    QString PeerConnection::IceConnectionStateToString(webrtc::PeerConnectionInterface::IceConnectionState state)
    {
        switch(state)
        {
        case webrtc::PeerConnectionInterface::kIceConnectionNew: return "IceConnectionNew";
        case webrtc::PeerConnectionInterface::kIceConnectionChecking: return "IceConnectionChecking";
        case webrtc::PeerConnectionInterface::kIceConnectionConnected: return "IceConnectionConnected";
        case webrtc::PeerConnectionInterface::kIceConnectionCompleted: return "IceConnectionCompleted";
        case webrtc::PeerConnectionInterface::kIceConnectionFailed: return "IceConnectionFailed";
        case webrtc::PeerConnectionInterface::kIceConnectionDisconnected: return "IceConnectionDisconnected";
        case webrtc::PeerConnectionInterface::kIceConnectionClosed: return "IceConnectionClosed";
        }
        return "Unknown Ice Connection State";
    }
    
    QString PeerConnection::IceGatheringStateToString(webrtc::PeerConnectionInterface::IceGatheringState state)
    {
        switch(state)
        {
        case webrtc::PeerConnectionInterface::kIceGatheringNew: return "IceGatheringNew";
        case webrtc::PeerConnectionInterface::kIceGatheringGathering: return "IceGatheringGathering";
        case webrtc::PeerConnectionInterface::kIceGatheringComplete: return "IceGatheringComplete";
        }
        return "Unknown Ice Gathering State";
    }
    
    /// webrtc::PeerConnectionObserver overrides

    void PeerConnection::OnError()
    {
        LogError(LC + "OnError()");
    }
    
    void PeerConnection::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state)
    {
        LogDebug(LC + "OnSignalingChange() " + SignalingStateToString(new_state));
    }

    void PeerConnection::OnStateChange(StateType state_changed)
    {
        LogDebug(LC + "OnStateChange : " + (state_changed == kSignalingState ? "SignalingState" : "IceState"));
    }

    void PeerConnection::OnAddStream(webrtc::MediaStreamInterface* stream)
    {
        LogInfo(LC + QString("Remote stream added: %1 Video tracks = %2 Audio tracks = %3")
            .arg(stream->label().c_str()).arg(stream->GetVideoTracks().size()).arg(stream->GetAudioTracks().size()));
        
        talk_base::scoped_refptr<webrtc::AudioTrackInterface> audioTrack = stream->FindAudioTrack(kAudioLabel);
        if (audioTrack.get() && IsLogChannelEnabled(LogChannelDebug))
        {
            qDebug() << "  >> Audio track:" << audioTrack->id().c_str() << audioTrack->enabled();
            qDebug() << audioTrack->GetRenderer();
        }
        talk_base::scoped_refptr<webrtc::VideoTrackInterface> videoTrack = stream->FindVideoTrack(kVideoLabel);
        if (videoTrack.get())
        {
            if (IsLogChannelEnabled(LogChannelDebug))
                qDebug() << "  >> Video track:" << videoTrack->id().c_str() << videoTrack->enabled();
                
            if (IsPreviewRenderingEnabled())
                activeRenderers_ << new VideoRenderer(videoTrack, false, QString("Remote Stream %1").arg(activeRenderers_.size()+1));
        }
        
        //stream->Release();
    }

    void PeerConnection::OnRemoveStream(webrtc::MediaStreamInterface* stream)
    {
        LogDebug(LC + "OnRemoveStream()");
    }

    void PeerConnection::OnDataChannel(webrtc::DataChannelInterface* data_channel)
    {
        LogDebug(LC + "Incoming data channel with label = " + QString::fromStdString(data_channel->label()) + ". Registering as observer.");
        data_channel->RegisterObserver(this);
    }

    void PeerConnection::OnRenegotiationNeeded()
    {
        LogDebug(LC + "-------------------------------------- OnRenegotiationNeeded()");
        /// @todo Call CreateOffer when this is triggered on an ongoing connection (open state)
        //if (peerConnection_)
        //    peerConnection_->CreateOffer(this, NULL);
    }

    void PeerConnection::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
    {
        LogDebug(LC + "OnIceConnectionChange: " + IceConnectionStateToString(new_state));
    }

    void PeerConnection::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state)
    {
        LogDebug(LC + "OnIceGatheringChange: " + IceGatheringStateToString(new_state));
    }

    void PeerConnection::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
    {
        LogDebug(LC + "OnIceCandidate()");
        localIceCandidatesResolved_ = false;
        
        std::string sdp; candidate->ToString(&sdp);
        pendingLocalIceCandidates_ << WebRTC::ICECandidate(candidate->sdp_mline_index(), QString::fromStdString(candidate->sdp_mid()), QString::fromStdString(sdp));
    }

    void PeerConnection::OnIceComplete()
    {
        LogDebug(LC + "OnIceComplete()");
        if (IsLogChannelEnabled(LogChannelDebug))
            qDebug() << "  >> Pending remote ICE candidates" << pendingRemoteIceCandidates_.size();
        
        localIceCandidatesResolved_ = true;
        
        /// @todo Move to last block of EmitResolvedSignals??
        if (pendingRemoteIceCandidates_.size() > 0)
            AddRemoteIceCandidates(pendingRemoteIceCandidates_);
        pendingRemoteIceCandidates_.clear();

        EmitResolvedSignals();
    }

    /// webrtc::CreateSessionDescriptionObserver overrides

    void PeerConnection::OnSuccess(webrtc::SessionDescriptionInterface* desc)
    {
        if (!peerConnection_.get())
            return;

        LogDebug(LC + "  >> Setting local SDP");
        peerConnection_->SetLocalDescription(DummySetSessionDescriptionObserver::Create(), desc);
        LogDebug(LC + "  >> DONE");

        std::string sdp; desc->ToString(&sdp);
        localSDP_ = WebRTC::SDP(QString::fromStdString(desc->type()), QString::fromStdString(sdp));
        localSDPResolved_ = true;

        EmitResolvedSignals();
    }

    void PeerConnection::OnFailure(const std::string& error)
    {
        LogError(LC + "OnFailure() " + QString::fromStdString(error));
    }
    
    void PeerConnection::EmitResolvedSignals()
    {
        if (localSDPResolved_ && !localSDPEmitted_)
        {
            LogDebug(LC + "Emitting local SDP");
            localSDPEmitted_ = true;
            emit LocalSDPResolved(localSDP_);
        }
            
        if (localIceCandidatesResolved_ && !localIceEmitted_)
        {
            LogDebug(LC + "Emitting local ICE");
            localIceEmitted_ = true;
            emit LocaleIceCandidatesResolved(pendingLocalIceCandidates_);
        }

        if (localSDPResolved_ && localIceCandidatesResolved_ && !localBothEmitted_)
        {
            LogDebug(LC + "Emitting BOTH local SDP and ICE");
            localBothEmitted_ = true;
            emit LocalConnectionDataResolved(localSDP_, pendingLocalIceCandidates_);
        }
    }

    int PeerConnection::AddRef()
    {
        /// @todo Mutex?
        refCount_++;
        return refCount_;
    }

    int PeerConnection::Release()
    {
        /// @todo Mutex?
        refCount_--;
        return refCount_;
    }
    
    void PeerConnection::OnStateChange()
    {
        if (dataChannel_.get())
        {
            if (IsLogChannelEnabled(LogChannelDebug))
            {
                qDebug() << "webrtc::DataChannelObserver::OnStateChange" << dataChannel_->state();
                if (dataChannel_->state() == webrtc::DataChannelInterface::kOpen)
                {
                    qDebug() << "    >> Sending 'hello world'";
                    webrtc::DataBuffer buff(std::string("hello world"));
                    dataChannel_->Send(buff);
                }
            }
        }
        else
            LogError("webrtc::DataChannelObserver::OnStateChange but data channel ptr is null!");
    }
    
    void PeerConnection::OnMessage(const webrtc::DataBuffer& buffer)
    {
        if (!buffer.binary)
        {
            QByteArray json = QString::fromUtf8(buffer.data.data(), buffer.data.length()).toUtf8();

            CloudRenderingProtocol::MessageSharedPtr message = CloudRenderingProtocol::CreateMessageFromJSON(json);
            if (!message.get())
            {
                LogError(LC + "Error while parsing incoming JSON message");
                if (IsLogChannelEnabled(LogChannelDebug))
                    CloudRenderingProtocol::DumpPrettyJSON(json);
                return;
            }
            if (!message->IsValid())
            {
                LogError(LC + "Failed to resolve incoming JSON messages type");
                return;
            }

            if (IsLogChannelEnabled(LogChannelDebug))
            {
                qDebug() << "PEER DATA CHANNEL - Received new message: channel =" << message->ChannelTypeName() << "type =" << message->MessageTypeName() << "    raw size =" << json.size() << "bytes";
                CloudRenderingProtocol::DumpPrettyJSON(json);
            }
            
            emit DataChannelMessage(message);
        }
        else
        {
            CloudRenderingProtocol::BinaryMessageData data = CloudRenderingProtocol::BinaryMessageData::fromRawData(buffer.data.data(), buffer.data.length());
            
            if (IsLogChannelEnabled(LogChannelDebug))
                qDebug() << "PEER DATA CHANNEL - Received new binary message of" << data.size() << "bytes";

            emit DataChannelMessage(data);
        }
    }

    bool PeerConnection::InitializePeerConnection(const ConnectionSettings &settings)
    {
        if (peerConnectionFactory_.get() || peerConnection_.get())
        {
            LogError(LC + "PeerConnection already initialized!");
            return false;
        }

        peerConnectionFactory_ = webrtc::CreatePeerConnectionFactory();
        if (peerConnectionFactory_.get())
        {            
            webrtc::PeerConnectionInterface::IceServer server;
            server.uri = WebRTC::GetPeerConnectionString();

            webrtc::PeerConnectionInterface::IceServers servers;
            servers.push_back(server);

            if (!settings.data)
                peerConnection_ = peerConnectionFactory_->CreatePeerConnection(servers, NULL, NULL, this);
            else
            {
                mediaConstraints_.Reset();
                mediaConstraints_.SetAllowRtpDataChannels();
                //mediaConstraints_.SetAllowDtlsSctpDataChannels(); // Does not work on current webrtc lib, should work in Chrome >=31.
                talk_base::InitializeSSL();
                peerConnection_ = peerConnectionFactory_->CreatePeerConnection(servers, &mediaConstraints_, NULL, this);
            }
            if (peerConnection_.get())
            {
                AddStreams(settings);
                return true;
            }
            else
                LogError(LC + "Failed to create PeerConnection!");
        }
        else
            LogError(LC + "Failed to create PeerConnectionFactory!");
            
        return false;
    }
    
    bool PeerConnection::IsPreviewRenderingEnabled() const
    {
        return (framework_ ? framework_->HasCommandLineParameter("--cloudRenderingShowStreamPreview") : false);
    }

    bool PeerConnection::AreLocalIceCandidatesResolved() const
    {
        return localIceCandidatesResolved_;
    }

    WebRTC::ICECandidateList PeerConnection::PendingLocalIceCandidates() const
    {
        return pendingLocalIceCandidates_;
    }

    void PeerConnection::ClearLocalIceCandidates()
    {
        pendingLocalIceCandidates_.clear();
    }
}
