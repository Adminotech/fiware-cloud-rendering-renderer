
// For conditions of distribution and use, see copyright notice in LICENSE

// Hack around Windows GDI Polygon define
#include "Win.h"
#include "EC_Camera.h"

#include "WebRTCRenderer.h"
#include "WebRTCWebSocketClient.h"
#include "WebRTCPeerConnection.h"

#include "CloudRenderingPlugin.h"

#include "Framework.h"
#include "LoggingFunctions.h"
#include "FrameAPI.h"
#include "Profiler.h"
#include "Application.h"
#include "UiAPI.h"
#include "UiMainWindow.h"

#include <QImage>
#include <QDebug>

#include "OgreRenderingModule.h"
#include "Renderer.h"
#include "TextureAsset.h"
#include "OgreTextureManager.h"
#include "OgreTexture.h"
#include "OgreHardwarePixelBuffer.h"

#include <OgreD3D9HardwarePixelBuffer.h>
#include <OgreD3D9RenderWindow.h>

#include <d3d9.h>
#include <d3dx9tex.h>

namespace WebRTC
{
    // Renderer
    
    Renderer::Renderer(CloudRenderingPlugin *plugin) :
        LC("[WebRTC::Renderer]: "),
        plugin_(plugin),
        websocket_(new WebRTC::WebSocketClient(plugin)),
        tundraRenderer_(new TundraRenderer(plugin))
    {
        WebRTC::RegisterMetaTypes();

        connect(websocket_.get(), SIGNAL(Connected()), SLOT(OnServiceConnected()));
        connect(websocket_.get(), SIGNAL(Disconnected()), SLOT(OnServiceDisconnected()));
        connect(websocket_.get(), SIGNAL(ConnectingFailed()), SLOT(OnServiceConnectingFailed()));
        connect(websocket_.get(), SIGNAL(Message(CloudRenderingProtocol::MessageSharedPtr)),
            SLOT(OnServiceMessage(CloudRenderingProtocol::MessageSharedPtr)));
        
        // Connect to service
        serviceHost_ = WebRTC::WebSocketClient::CleanHost(plugin_->GetFramework()->CommandLineParameters("--cloudRenderer").first());
        if (!serviceHost_.isEmpty())
            websocket_->Connect(serviceHost_);
        else
            LogError(LC + "--cloudRenderer <cloudRenderingServiceHost> parameter not defined, cannot connect to service for renderer registration!");
    }
    
    Renderer::~Renderer()
    {
        tundraRenderer_.reset();
        websocket_.reset();
        connections_.clear();
    }
    
    CloudRenderingProtocol::CloudRenderingRoom Renderer::Room() const
    {
        return room_;
    }
    
    WebRTCPeerConnectionPtr Renderer::Peer(const QString &peerId) const
    {
        foreach(WebRTCPeerConnectionPtr peer, connections_)
            if (peer->Id() == peerId)
                return peer;
        return WebRTCPeerConnectionPtr();
    }
    
    WebRTCPeerConnectionPtr Renderer::GetOrCreatePeer(const QString &peerId)
    {
        WebRTCPeerConnectionPtr peer = Peer(peerId);
        if (!peer.get())
        {
            peer = WebRTCPeerConnectionPtr(new WebRTC::PeerConnection(plugin_->GetFramework(), peerId));
            connect(peer.get(), SIGNAL(LocalConnectionDataResolved(WebRTC::SDP, WebRTC::ICECandidateList)), 
                SLOT(OnLocalConnectionDataResolved(WebRTC::SDP, WebRTC::ICECandidateList)), Qt::QueuedConnection);
            connections_ << peer;
        }
        return peer;
    }
    
    void Renderer::OnServiceConnected()
    {
        CloudRenderingProtocol::State::RegistrationMessage *message = new CloudRenderingProtocol::State::RegistrationMessage(
            CloudRenderingProtocol::State::RegistrationMessage::R_Renderer);
        message->deleteLater();
        websocket_->Send(message);
        
        room_.Reset();
    }
    
    void Renderer::OnServiceDisconnected()
    {
        LogInfo(LC + QString("WebSocket connection disconnected from %1").arg(serviceHost_));
        room_.Reset();
    }

    void Renderer::OnServiceConnectingFailed()
    {
        LogError(LC + QString("WebSocket connection failed to Cloud Rendering Service at %1").arg(serviceHost_));
        room_.Reset();
    }

    void Renderer::OnServiceMessage(CloudRenderingProtocol::MessageSharedPtr message)
    {
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
                        {
                            bool sendWebCamera = plugin_->GetFramework()->HasCommandLineParameter("--cloudRenderingSendWebCamera");
                            
                            WebRTCPeerConnectionPtr peer = GetOrCreatePeer(offer->senderId);
                            peer->HandleOfferOrAnswer(offer->sdp, offer->iceCandidates, PeerConnection::ConnectionSettings(false, sendWebCamera, !sendWebCamera, true));
                        }
                        else
                            LogError(LC + "Failed to cast MT_Offer message to OfferMessage*");
                        break;
                    }
                    // Answer
                    case CloudRenderingProtocol::MT_Answer:
                    {
                        CloudRenderingProtocol::Signaling::AnswerMessage *answer = dynamic_cast<CloudRenderingProtocol::Signaling::AnswerMessage*>(message.get());
                        if (answer)
                        {
                            bool sendWebCamera = plugin_->GetFramework()->HasCommandLineParameter("--cloudRenderingSendWebCamera");
                            
                            WebRTCPeerConnectionPtr peer = GetOrCreatePeer(answer->senderId);
                            peer->HandleOfferOrAnswer(answer->sdp, answer->iceCandidates, PeerConnection::ConnectionSettings(false, sendWebCamera, !sendWebCamera, true));
                        }
                        else
                            LogError(LC + "Failed to cast MT_Answer message to AnswerMessage*");
                        break;
                    }
                    // Ice candidates
                    case CloudRenderingProtocol::MT_IceCandidates:
                    {
                        CloudRenderingProtocol::Signaling::IceCandidatesMessage *candidates = dynamic_cast<CloudRenderingProtocol::Signaling::IceCandidatesMessage*>(message.get());
                        if (candidates)
                        {
                            WebRTCPeerConnectionPtr peer = Peer(candidates->senderId);
                            if (peer.get())
                                peer->AddRemoteIceCandidates(candidates->iceCandidates);
                            else
                                LogError(LC + QString("Failed to find sender peer with id %1 for a IceCandidatesMessage.").arg(candidates->senderId));
                        }
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
                                LogInfo(LC + "Renderer was assigned to room " + room_.id);
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
                            LogInfo(LC + "Peers joined to the renderers room");
                            foreach(const QString &joinedPeerId, joined->peerIds)
                            {
                                LogInfo(LC + QString("  peerId = %1").arg(joinedPeerId));
                                room_.AddPeer(joinedPeerId);
                                
                                /// @todo This will be changed to something else in the future, for now send offer to each joining client.
                                bool sendWebCamera = plugin_->GetFramework()->HasCommandLineParameter("--cloudRenderingSendWebCamera");
                                
                                WebRTCPeerConnectionPtr peer = GetOrCreatePeer(joinedPeerId);
                                peer->CreateOffer(PeerConnection::ConnectionSettings(false, sendWebCamera, !sendWebCamera, true));
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

    void Renderer::OnLocalConnectionDataResolved(WebRTC::SDP sdp, WebRTC::ICECandidateList candidates)
    {
        WebRTC::PeerConnection *peer = dynamic_cast<WebRTC::PeerConnection*>(sender());
        if (!peer)
        {
            LogError(LC + "Failed to cast signal sender as WebRTC::PeerConnection*");
            return;
        }

        if (sdp.type.compare("answer", Qt::CaseInsensitive) == 0)
        {
            CloudRenderingProtocol::Signaling::AnswerMessage *message = new CloudRenderingProtocol::Signaling::AnswerMessage(peer->Id());
            message->deleteLater();
            message->sdp = sdp;
            message->iceCandidates = candidates;

            if (!websocket_->Send(message))
                LogWarning(LC + "Failed to send " + message->MessageTypeName());
        }
        else if (sdp.type.compare("offer", Qt::CaseInsensitive) == 0)
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

    TundraRenderer *Renderer::ApplicationRenderer() const
    {
        return tundraRenderer_.get();
    }

    // TundraRendererConsumer

    TundraRenderer::TundraRenderer(CloudRenderingPlugin *plugin, uint updateFps) :
        plugin_(plugin),
        framework_(plugin->GetFramework()),
        interval_(1.0f / static_cast<float>(updateFps)),
        t_(1.0f),
        d3dTexture_(0),
        fatalTextureError_(false)
    {
        connect(framework_->Frame(), SIGNAL(PostFrameUpdate(float)), SLOT(OnPostFrameUpdate(float)));
    }
    
    TundraRenderer::~TundraRenderer()
    {
        consumers_.clear();
        
        if (d3dTexture_)
            d3dTexture_->Release();
        d3dTexture_ = 0;
    }
    
    void TundraRenderer::CheckTexture(int width, int height)
    {
        if (fatalTextureError_)
            return;
        if (d3dTexture_)
            return;

        Ogre::D3D9RenderWindow *renderWindow = D3DRenderWindow();
        IDirect3DDevice9 *d3dDevice = (renderWindow != 0 ? renderWindow->getD3D9Device() : 0);
        if (!d3dDevice)
        {
            LogError("IDirect3DDevice9 is null");
            return;
        }
        
        if (width <= 0)
            width = renderWindow->getWidth();
        if (height <= 0)
            height = renderWindow->getHeight();

        if (D3DXCreateTexture(d3dDevice, static_cast<UINT>(width), static_cast<UINT>(height), 
            1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &d3dTexture_) != D3D_OK)
        {
            LogError("Failed to create d3d texture");
            return;
        }
    }
    
    void TundraRenderer::SetInterval(uint updateFps)
    {
        if (updateFps == 0)
            updateFps = 1;
        qDebug() << "SetInterval" << updateFps;
        interval_ = 1.0f / static_cast<float>(updateFps);
    }

    void TundraRenderer::SetSize(int width, int height)
    {
        qDebug() << "SetSize" << width << height;
        pendingWindowResize_ = QSize(width, height + 21); // + 21 is the magic hack for QMenuBar height
    }

    void TundraRenderer::Register(TundraRendererConsumerWeakPtr consumer)
    {
        if (consumer.expired())
            return;

        // Already registered?
        for (int i=0; i<consumers_.size(); ++i)
        {
            TundraRendererConsumerWeakPtr &iter = consumers_[i];
            if (iter.lock().get() == consumer.lock().get())
                return;
        }

        qDebug() << "Registering consumer" << consumer.lock().get();
        consumers_ << consumer;
    }
    
    void TundraRenderer::Unregister(TundraRendererConsumerWeakPtr consumer)
    {
        for (int i=0; i<consumers_.size(); ++i)
        {
            // Throw out expired consumers and the consumer that unregistered
            TundraRendererConsumerWeakPtr &iter = consumers_[i];
            if (iter.expired() || iter.lock().get() == consumer.lock().get())
            {
                qDebug() << "Removing consumer" << consumer.lock().get();
                consumers_.removeAt(i);
                i--;
            }
        }
    }
    
    Ogre::D3D9RenderWindow *TundraRenderer::D3DRenderWindow() const
    {
        OgreRenderer::OgreRenderingModule *ogreModule = framework_->Module<OgreRenderer::OgreRenderingModule>();
        if (!ogreModule || !ogreModule->Renderer())
            return 0;
        return dynamic_cast<Ogre::D3D9RenderWindow*>(ogreModule->Renderer()->GetCurrentRenderWindow());
    }
    
    IDirect3DDevice9 *TundraRenderer::D3DDevice() const
    {
        Ogre::D3D9RenderWindow *d3d9rw = D3DRenderWindow();
        return (d3d9rw != 0 ? d3d9rw->getD3D9Device() : 0);
    }
    
    void TundraRenderer::OnPostFrameUpdate(float frametime)
    {
        // Choking
        t_ += frametime;
        if (t_ < interval_)
            return;
        t_ = 0.0f;

        // Apply pending window resize
        if (pendingWindowResize_.isValid() && framework_->Ui()->MainWindow())
        {
            qDebug() << "Executing resize" << pendingWindowResize_;
            framework_->Ui()->MainWindow()->resize(pendingWindowResize_); 
            pendingWindowResize_ = QSize();
            return;
        }

        PROFILE(CloudRendering_TundraRenderer_PostFrameUpdate)

        // No consumers, don't do any work.
        if (consumers_.isEmpty())
            return;
            
        CheckTexture();
        
        Ogre::D3D9RenderWindow *renderWindow = D3DRenderWindow();
        IDirect3DDevice9 *d3dDevice = (renderWindow != 0 ? renderWindow->getD3D9Device() : 0);
        if (!d3dDevice || !d3dTexture_)
            return;
   
        PROFILE(CloudRendering_TundraRenderer_Update_Render_Texture)
        
        Ogre::TexturePtr texture;
        
        // Select camera texture for both UI and 3D rendering.
        // If not camera use the UI overlay texture as the source.
        EC_Camera *camera = framework_->Renderer()->MainCameraComponent();
        if (camera)
            texture = camera->UpdateRenderTexture(true);
        else
            texture = Ogre::TextureManager::getSingleton().getByName("MainWindow RTT");

        Ogre::D3D9HardwarePixelBuffer *buffer = texture.get() ? dynamic_cast<Ogre::D3D9HardwarePixelBuffer*>(texture->getBuffer().get()) : 0;
        if (!buffer || buffer->getFormat() != Ogre::PF_A8R8G8B8)
            return;

        ELIFORP(CloudRendering_TundraRenderer_Update_Render_Texture)
        PROFILE(CloudRendering_TundraRenderer_Check_Target_Size)
        
        ELIFORP(CloudRendering_TundraRenderer_Check_Target_Size)
        PROFILE(CloudRendering_TundraRenderer_Check_Surfaces)

        IDirect3DSurface9 *surfaceRenderTarget = buffer->getSurface(d3dDevice);
        //IDirect3DSurface9 *surfaceRenderTarget = renderWindow->getRenderSurface();
        IDirect3DSurface9 *surfaceTexture = 0; 
        if (d3dTexture_->GetSurfaceLevel(0, &surfaceTexture) != D3D_OK)
        {
            LogError("Failed to get texture surface");
            fatalTextureError_ = true;
            d3dTexture_->Release();
            d3dTexture_ = 0;
            return;
        }

        D3DSURFACE_DESC renderTargetDesc, textureDesc;
        if (!surfaceRenderTarget || surfaceRenderTarget->GetDesc(&renderTargetDesc) != D3D_OK || surfaceTexture->GetDesc(&textureDesc) != D3D_OK)
        {
            LogError("Failed to get descs");
            fatalTextureError_ = true;
            d3dTexture_->Release();
            d3dTexture_ = 0;
            return;
        }
        if (renderTargetDesc.Width != textureDesc.Width || renderTargetDesc.Height != textureDesc.Height)
        {
            LogError("Incompatible texture sizes! Resizing target texture...");
            d3dTexture_->Release();
            d3dTexture_ = 0;
            CheckTexture(renderTargetDesc.Width, renderTargetDesc.Height);
            return;
        }
        
        ELIFORP(CloudRendering_TundraRenderer_Check_Surfaces)
        PROFILE(CloudRendering_TundraRenderer_Update_Texture)
       
        if (texture->getUsage() == Ogre::TU_RENDERTARGET)
        {
            // Render target, direct locking is not supported. Use special function to copy from
            // GPU to CPU surface. Then lock on the CPU side one. This function has lots of rules and conditions
            // for it to work: http://msdn.microsoft.com/en-us/library/windows/desktop/bb174405(v=vs.85).aspx
            if (d3dDevice->GetRenderTargetData(surfaceRenderTarget, surfaceTexture) != D3D_OK)
            {
                LogError("Render target GetRenderTargetData data copy failed!");
                fatalTextureError_ = true;
                d3dTexture_->Release();
                d3dTexture_ = 0;
                return;
            }
            
            // Other alternatives for various other conditions...
            //if (D3DXLoadSurfaceFromSurface(surfaceTexture, NULL, NULL,
            //    surfaceRenderTarget, NULL, NULL, D3DX_FILTER_BOX, 0) != D3D_OK)
            //if (d3dDevice->StretchRect(surfaceRenderTarget, NULL, surfaceTexture, NULL, D3DTEXF_LINEAR) != D3D_OK)
        }
        else
        {
            // Not a render target, direct lock to the surface.
            surfaceTexture = surfaceRenderTarget;
        }
        
        ELIFORP(CloudRendering_TundraRenderer_Update_Texture)
        PROFILE(CloudRendering_TundraRenderer_Texture_Lock)
        
        D3DLOCKED_RECT lock;

        RECT lockRect = { 0, 0, texture->getWidth(), texture->getHeight() };
        if (surfaceTexture->LockRect(&lock, &lockRect, D3DLOCK_READONLY) != D3D_OK)
        {
            LogError("Failed to lock in mem texture");
            fatalTextureError_ = true;
            d3dTexture_->Release();
            d3dTexture_ = 0;
            return;
        }

        ELIFORP(CloudRendering_TundraRenderer_Texture_Lock)
        PROFILE(CloudRendering_TundraRenderer_Copy_Data)
        
        QImage image(texture->getWidth(), texture->getHeight(), QImage::Format_ARGB32);        
        memcpy(static_cast<void*>(image.bits()), lock.pBits, image.byteCount()); 
        surfaceTexture->UnlockRect();
        
        ELIFORP(CloudRendering_TundraRenderer_Copy_Data)
        PROFILE(CloudRendering_TundraRenderer_UpdateConsumers)
        
        for (int i=0; i<consumers_.size(); ++i)
        {
            TundraRendererConsumerWeakPtr &iter = consumers_[i];
            if (!iter.expired())
                iter.lock()->OnTundraFrame(&image);
            else
            {
                consumers_.removeAt(i);
                i--;   
            }            
        }
    }
}
