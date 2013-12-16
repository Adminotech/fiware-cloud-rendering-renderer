/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#pragma once

#include "CloudRenderingPluginApi.h"
#include "CloudRenderingPluginFwd.h"

#include "IModule.h"

class CLOUDRENDERING_API CloudRenderingPlugin : public IModule
{
    Q_OBJECT

public:
    CloudRenderingPlugin();
    ~CloudRenderingPlugin();

    /// IModule override.
    void Initialize();

    /// IModule override.
    void Uninitialize();
    
public slots:
    WebRTCRendererPtr Renderer() const;
    WebRTCClientPtr Client() const;
    
    bool IsRenderer() const;
    bool IsClient() const;
    
private:
    QString LC;

    WebRTCRendererPtr renderer_;
    WebRTCClientPtr client_;
};
