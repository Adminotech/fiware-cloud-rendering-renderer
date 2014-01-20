/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

// Go around Windows GDI Polygon (conflicts with the Tundra math library) define by including it first.

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
#include "UiGraphicsView.h"
#include "InputAPI.h"

#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
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
        CloudRenderingProtocol::RegisterMetaTypes();

        connect(websocket_.get(), SIGNAL(Connected()), SLOT(OnServiceConnected()));
        connect(websocket_.get(), SIGNAL(Disconnected()), SLOT(OnServiceDisconnected()));
        connect(websocket_.get(), SIGNAL(ConnectingFailed()), SLOT(OnServiceConnectingFailed()));
        connect(websocket_.get(), SIGNAL(Message(CloudRenderingProtocol::MessageSharedPtr)),
            SLOT(OnServiceMessage(CloudRenderingProtocol::MessageSharedPtr)));
            
        // We are going to be injecting input events when the window is inactive, disable auto releasing keys.
        plugin_->GetFramework()->Input()->SetReleaseInputWhenApplicationInactive(false);
        
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
            connect(peer.get(), SIGNAL(DataChannelMessage(CloudRenderingProtocol::MessageSharedPtr)), 
                SLOT(OnDataChannelMessage(CloudRenderingProtocol::MessageSharedPtr)), Qt::QueuedConnection);
            connect(peer.get(), SIGNAL(DataChannelMessage(const CloudRenderingProtocol::BinaryMessageData&)), 
                SLOT(OnDataChannelMessage(const CloudRenderingProtocol::BinaryMessageData&)), Qt::QueuedConnection);
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
    
    void Renderer::OnDataChannelMessage(CloudRenderingProtocol::MessageSharedPtr message)
    {
        OnDataChannelMessage(dynamic_cast<WebRTC::PeerConnection*>(sender()), message);
    }
    
    void Renderer::OnDataChannelMessage(WebRTC::PeerConnection *sender, CloudRenderingProtocol::MessageSharedPtr message)
    {
        if (!sender)
            return;
            
        switch (message->Channel())
        {
            // Application
            case CloudRenderingProtocol::CT_Application:
            {
                switch (message->Type())
                {
                    case CloudRenderingProtocol::MT_PeerCustomMessage:
                    {
                        CloudRenderingProtocol::Application::PeerCustomMessage *peerMessage = dynamic_cast<CloudRenderingProtocol::Application::PeerCustomMessage*>(message.get());
                        if (peerMessage)
                        {
                            QString type = peerMessage->payload.value("type", "").toString();
                            if (type == "InputKeyboard")
                                PostKeyboardEvent(peerMessage->payload);
                            else if (type == "InputMouse")
                                PostMouseEvent(peerMessage->payload);
                            else
                                LogWarning("Unknown PeerCustomMessage with type " + type);
                        }
                        break;
                    }
                }
            }
        }
    }

    Qt::Key KeyFromHtmlKeyCode(int c)
    {
        switch(c)
        {
            case 8:     return Qt::Key_Backspace;
            case 9:     return Qt::Key_Tab;
            case 13:    return Qt::Key_Enter;
            case 16:    return Qt::Key_Shift;
            case 17:    return Qt::Key_Control;
            case 18:    return Qt::Key_Alt;
            case 19:    return Qt::Key_Pause;
            case 20:    return Qt::Key_CapsLock;
            case 27:    return Qt::Key_Escape;
            
            case 32:    return Qt::Key_Space;
            case 33:    return Qt::Key_PageUp;
            case 34:    return Qt::Key_PageDown;
            
            case 35:    return Qt::Key_End;
            case 36:    return Qt::Key_Home;
            case 37:    return Qt::Key_Left;
            case 38:    return Qt::Key_Up;
            case 39:    return Qt::Key_Right;
            case 40:    return Qt::Key_Down;
            
            case 45:    return Qt::Key_Insert;
            case 46:    return Qt::Key_Delete;
            
            case 48:    return Qt::Key_0;
            case 49:    return Qt::Key_1;
            case 50:    return Qt::Key_2;
            case 51:    return Qt::Key_3;
            case 52:    return Qt::Key_4;
            case 53:    return Qt::Key_5;
            case 54:    return Qt::Key_6;
            case 55:    return Qt::Key_7;
            case 56:    return Qt::Key_8;
            case 57:    return Qt::Key_9;
            
            case 65:    return Qt::Key_A;
            case 66:    return Qt::Key_B;
            case 67:    return Qt::Key_C;
            case 68:    return Qt::Key_D;
            case 69:    return Qt::Key_E;
            case 70:    return Qt::Key_F;
            case 71:    return Qt::Key_G;
            case 72:    return Qt::Key_H;
            case 73:    return Qt::Key_I;
            case 74:    return Qt::Key_J;
            case 75:    return Qt::Key_K;
            case 76:    return Qt::Key_L;
            case 77:    return Qt::Key_M;
            case 78:    return Qt::Key_N;
            case 79:    return Qt::Key_O;
            case 80:    return Qt::Key_P;
            case 81:    return Qt::Key_Q;
            case 82:    return Qt::Key_R;
            case 83:    return Qt::Key_S;
            case 84:    return Qt::Key_T;
            case 85:    return Qt::Key_U;
            case 86:    return Qt::Key_V;
            case 87:    return Qt::Key_W;
            case 88:    return Qt::Key_X;
            case 89:    return Qt::Key_Y;
            case 90:    return Qt::Key_Z;

            //case 91:    return Qt::Key_; // left window key?
            //case 92:    return Qt::Key_; // right window key?
            case 93:    return Qt::Key_Select;

            case 96:    return Qt::Key_0; // Numpad numbers
            case 97:    return Qt::Key_1;
            case 98:    return Qt::Key_2;
            case 99:    return Qt::Key_3;
            case 100:   return Qt::Key_4;
            case 101:   return Qt::Key_5;
            case 102:   return Qt::Key_6;
            case 103:   return Qt::Key_7;
            case 104:   return Qt::Key_8;
            case 105:   return Qt::Key_9;
            
            case 106:   return Qt::Key_Asterisk; // multiply
            case 107:   return Qt::Key_Plus;     // add
            case 109:   return Qt::Key_Minus;    // subtract
            case 110:   return Qt::Key_Comma;    // decimal point
            case 111:   return Qt::Key_Slash;    // divide
            
            case 112:   return Qt::Key_F1;
            case 113:   return Qt::Key_F2;
            case 114:   return Qt::Key_F3;
            case 115:   return Qt::Key_F4;
            case 116:   return Qt::Key_F5;
            case 117:   return Qt::Key_F6;
            case 118:   return Qt::Key_F7;
            case 119:   return Qt::Key_F8;
            case 120:   return Qt::Key_F9;
            case 121:   return Qt::Key_F10;
            case 122:   return Qt::Key_F11;
            case 123:   return Qt::Key_F12;
            
            case 144:   return Qt::Key_NumLock;
            case 145:   return Qt::Key_ScrollLock;
            
            case 186:   return Qt::Key_Semicolon;
            case 187:   return Qt::Key_Equal;
            case 188:   return Qt::Key_Comma;
            //case 189:   return Qt::Key_;     // dash
            case 190:   return Qt::Key_Period;
            case 191:   return Qt::Key_Slash;  // forward slash
            //case 192:   return Qt::Key_;     // grave accent
            
            case 219:   return Qt::Key_BracketLeft;  // open bracket
            case 220:   return Qt::Key_Backslash;
            case 221:   return Qt::Key_BracketRight; // close bracket
            case 222:   return Qt::Key_QuoteLeft;    // single quote
        }
        return Qt::Key_unknown;
    }

    void Renderer::PostKeyboardEvent(const QVariantMap &data)
    {
        UiGraphicsView *view = (plugin_ ? plugin_->GetFramework()->Ui()->GraphicsView() : 0);
        if (!view)
            return;
        
        // type: 'keyDown' or 'keyUp'
        QEvent::Type type = (data.value("action", "").toString() == "keyDown" ? QEvent::KeyPress : QEvent::KeyRelease);
        
        // modifiers
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        if (data.value("altKey", false).toBool())
            modifiers |= Qt::AltModifier;
        if (data.value("shiftKey", false).toBool())
            modifiers |= Qt::ShiftModifier;
        if (data.value("ctrlKey", false).toBool())
            modifiers |= Qt::ControlModifier;
        if (data.value("metaKey", false).toBool())
            modifiers |= Qt::MetaModifier;
            
        inputState_.keyboardModifiers = modifiers;
            
        Qt::Key key = KeyFromHtmlKeyCode(data.value("key").toInt());
        if (key == Qt::Key_unknown)
        {
            if (IsLogChannelEnabled(LogChannelDebug))
                qWarning() << "Failed to map HTML keyCode" << data.value("key").toInt() << "to a Qt key";
            return;
        }
        
        QString text = QKeySequence(key).toString(QKeySequence::NativeText).toLower();
        if (text == "space")
            text = " ";
        else if (text == "tab")
            text = "    ";
        if (data.value("shiftKey", false).toBool())
            text = text.toUpper();
        
        QKeyEvent *e = new QKeyEvent(type, key, modifiers, text);
        if (IsLogChannelEnabled(LogChannelDebug))
            qDebug() << e;
        QApplication::postEvent(view, e);
    }

    void Renderer::PostMouseEvent(const QVariantMap &data)
    {
        UiGraphicsView *view = (plugin_ ? plugin_->GetFramework()->Ui()->GraphicsView() : 0);
        if (!view)
            return;
            
        if (!data.contains("x") || !data.contains("y"))
            return;

        bool ok = false;
        float x = data.value("x").toFloat(&ok);
        if (!ok) return; ok = false;       
        if (x > 1.0f)
        {
            LogWarning(LC + "Received mouse event with x >= 1.0!");
            x = 1.0f;
        }
        float y = data.value("y").toFloat(&ok);
        if (!ok) return;
        if (y > 1.0f)
        {
            LogWarning(LC + "Received mouse event with y >= 1.0!");
            y = 1.0f;
        }

        // type: 'move', 'press' or 'release'
        QString typeStr = data.value("action", "").toString();
        QEvent::Type type = (typeStr == "move" ? QEvent::MouseMove : (typeStr == "press" ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease));       

        // button
        if (type != QEvent::MouseMove)
            inputState_.mouseButtons = Qt::NoButton;

        Qt::MouseButton button = Qt::NoButton;
        if (data.value("leftButton", false).toBool())
        {
            button = Qt::LeftButton;
            inputState_.mouseButtons |= Qt::LeftButton;
        }
        if (data.value("rightButton", false).toBool())
        {
            button = Qt::RightButton;
            inputState_.mouseButtons |= Qt::RightButton;
        }
        if (data.value("middleButton", false).toBool())
        {
            button = Qt::MiddleButton;
            inputState_.mouseButtons |= Qt::MiddleButton;
        }

        // position [0.0, 1.0]
        QRect mainWindowRect = view->geometry();
        QPoint mousePos(mainWindowRect.width() * x, mainWindowRect.height() * y);
        
        QMouseEvent *e = 0;
        if (type != QEvent::MouseMove)
            e = new QMouseEvent(type, mousePos, button, inputState_.mouseButtons, inputState_.keyboardModifiers);
        else
            e = new QMouseEvent(type, mousePos, QCursor::pos(), Qt::NoButton, inputState_.mouseButtons, inputState_.keyboardModifiers);
        if (IsLogChannelEnabled(LogChannelDebug))
            qDebug() << e;
        QApplication::postEvent(view->viewport(), e);
    }
    
    void Renderer::OnDataChannelMessage(const CloudRenderingProtocol::BinaryMessageData &data)
    {
        OnDataChannelMessage(dynamic_cast<WebRTC::PeerConnection*>(sender()), data);
    }
    
    void Renderer::OnDataChannelMessage(WebRTC::PeerConnection *sender, const CloudRenderingProtocol::BinaryMessageData &data)
    {
        if (!sender)
            return;
            
        LogWarning(LC + "Handling binary data channel messages not implemented!");
        UNREFERENCED_PARAM(data);
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
            if (framework_->Ui()->MainWindow()->isMinimized() || framework_->Ui()->MainWindow()->isMaximized())
                framework_->Ui()->MainWindow()->showNormal();

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
