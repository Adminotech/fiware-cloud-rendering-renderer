/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#include "CloudRenderingPlugin.h"
#include "LoggingFunctions.h"

#include "WebRTCRenderer.h"
#include "WebRTCClient.h"

#include "Framework.h"
#include "CoreDefines.h"
#include "CoreTypes.h"

CloudRenderingPlugin::CloudRenderingPlugin() :
    IModule("CloudRendering"),
    LC("[CloudRendering]: ")
{
}

CloudRenderingPlugin::~CloudRenderingPlugin()
{
}

void CloudRenderingPlugin::Initialize()
{
    bool startRenderer = framework_->HasCommandLineParameter("--cloudRenderer");
    bool startClient = framework_->HasCommandLineParameter("--cloudRenderingClient");
    
    if (startRenderer && startClient)
    {
        LogError(LC + "Same instance cannot be both --cloudRenderer and --cloudRenderingClient");
        return;
    }

    if (startRenderer)
        renderer_ = WebRTCRendererPtr(new WebRTC::Renderer(this));
    else if (startClient)
        client_ = WebRTCClientPtr(new WebRTC::Client(this));
}

void CloudRenderingPlugin::Uninitialize()
{
    renderer_.reset();
    client_.reset();
}

WebRTCRendererPtr CloudRenderingPlugin::Renderer() const
{
    return renderer_;
}

WebRTCClientPtr CloudRenderingPlugin::Client() const
{
    return client_;
}

bool CloudRenderingPlugin::IsRenderer() const
{
    return (renderer_.get() != 0);
}

bool CloudRenderingPlugin::IsClient() const
{
    return (client_.get() != 0);
}

extern "C" DLLEXPORT void TundraPluginMain(Framework *fw)
{
    Framework::SetInstance(fw); // Inside this DLL, remember the pointer to the global framework object.
    fw->RegisterModule(new CloudRenderingPlugin());
}
