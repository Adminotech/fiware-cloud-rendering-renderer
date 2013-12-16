
// For conditions of distribution and use, see copyright notice in LICENSE

#pragma once

#include "CloudRenderingPluginApi.h"
#include "CloudRenderingPluginFwd.h"
#include "CloudRenderingDefines.h"
#include "CloudRenderingProtocol.h"

#include <QSize>

namespace Ogre
{
    class D3D9RenderWindow;
}

struct IDirect3DDevice9;
struct IDirect3DTexture9;

namespace WebRTC
{
    /// Cloud Rendering Renderer implementation.
    class CLOUDRENDERING_API Renderer : public QObject
    {
        Q_OBJECT

    public:
        Renderer(CloudRenderingPlugin *plugin);
        ~Renderer();
        
        /// Returns the current room.
        CloudRenderingProtocol::CloudRenderingRoom Room() const;
        
        /// Returns the Tundra Renderer.
        /** This can be used to register frame consumers. */
        TundraRenderer *ApplicationRenderer() const;

    private slots:
        void OnServiceConnected();
        void OnServiceDisconnected();
        void OnServiceConnectingFailed();
        void OnServiceMessage(CloudRenderingProtocol::MessageSharedPtr message);
        
        /// Signal handler when peers local data information has been resolved and we can send the AnswerMessage.
        void OnLocalConnectionDataResolved(WebRTC::SDP sdp, WebRTC::ICECandidateList candidates);
        
        /// Returns peer for a peer id, or null ptr if not found.
        WebRTCPeerConnectionPtr Peer(const QString &peerId) const;
        
        /// Gets or created peer with id.
        WebRTCPeerConnectionPtr GetOrCreatePeer(const QString &peerId);

    private:
        QString LC;
        QString serviceHost_;
        
        CloudRenderingProtocol::CloudRenderingRoom room_;

        CloudRenderingPlugin *plugin_;
        
        WebRTCTundraRendererPtr tundraRenderer_;
        WebRTCWebSocketClientPtr websocket_;
        WebRTCPeerConnectionList connections_;
    };
    
    /// Tundra renderer consumer receives frame updates from TundraRenderer.
    class CLOUDRENDERING_API TundraRendererConsumer
    {
        public:
            virtual void OnTundraFrame(const QImage *frame) = 0;
    };
    typedef weak_ptr<TundraRendererConsumer> TundraRendererConsumerWeakPtr;
    
    /// Provides a API to the Tundra rendering content.
    class CLOUDRENDERING_API TundraRenderer : public QObject
    {
        Q_OBJECT

    public:
        TundraRenderer(CloudRenderingPlugin *plugin, uint updateFps = 30);
        ~TundraRenderer();
      
    public slots:
        void SetInterval(uint updateFps);
        void SetSize(int width, int height);
        void Register(TundraRendererConsumerWeakPtr consumer);
        void Unregister(TundraRendererConsumerWeakPtr consumer);
        
    private slots:
        void OnPostFrameUpdate(float frametime);
        
    private:
        CloudRenderingPlugin *plugin_;
        Framework *framework_;
        
        void CheckTexture(int width = -1, int height = -1);
        
        Ogre::D3D9RenderWindow *D3DRenderWindow() const;
        IDirect3DDevice9 *D3DDevice() const;
        IDirect3DTexture9 *d3dTexture_;
        
        QSize pendingWindowResize_;
        bool fatalTextureError_;
        float interval_;
        float t_;
        QList<TundraRendererConsumerWeakPtr> consumers_;
    };
}
