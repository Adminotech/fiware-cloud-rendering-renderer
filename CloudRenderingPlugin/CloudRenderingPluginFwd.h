
// For conditions of distribution and use, see copyright notice in LICENSE

#pragma once

#include "FrameworkFwd.h"
#include "CoreTypes.h"

// Qt emit define is used in sigslots
// which makes the include break.
#undef emit
#include "talk/base/sigslot.h"
#define emit

class CloudRenderingPlugin;

namespace WebRTC
{
    class Renderer;
    class Client;

    class PeerConnection;
    class WebSocketClient;
    
    class TundraRenderer;
    class VideoRenderer;
}

typedef shared_ptr<WebRTC::Renderer> WebRTCRendererPtr;
typedef shared_ptr<WebRTC::Client> WebRTCClientPtr;
typedef shared_ptr<WebRTC::TundraRenderer> WebRTCTundraRendererPtr;
typedef shared_ptr<WebRTC::WebSocketClient> WebRTCWebSocketClientPtr;

typedef shared_ptr<WebRTC::PeerConnection> WebRTCPeerConnectionPtr;
typedef QList<WebRTCPeerConnectionPtr> WebRTCPeerConnectionList;
