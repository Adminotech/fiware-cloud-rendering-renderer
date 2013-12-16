/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#pragma once

#include "CloudRenderingPluginApi.h"
#include "CloudRenderingPluginFwd.h"

#include <QWidget>
#include <QLabel>
#include <QImage>
#include <QMutex>

#include "talk/app/webrtc/mediastreaminterface.h"

namespace WebRTC
{
    class VideoRenderer : public QWidget, public webrtc::VideoRendererInterface
    {
    Q_OBJECT
    
    public:
        VideoRenderer(webrtc::VideoTrackInterface *track, bool flipHorizontal = false, QString windowTitle = "");
        ~VideoRenderer();
        
        void Close();
        
        void SetSize(int width, int height);
        void RenderFrame(const cricket::VideoFrame* frame);

    signals:
        void Resize(QSize size);
        void Render(QImage image);
        void PaintRequest();

    private slots:
        void OnResize(QSize size);
        void OnRender(QImage image);
        
    private:
        void Reset();
        
        talk_base::scoped_refptr<webrtc::VideoTrackInterface> track_;
        
        QLabel *label_;
        
        QMutex mutex_;
        bool flipHorizontal_;
        bool frameConsumed_;        
    };
}
