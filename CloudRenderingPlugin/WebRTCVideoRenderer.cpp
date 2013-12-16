/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#include "WebRTCVideoRenderer.h"

#include <QHBoxLayout>
#include <QImage>
#include <QPixmap>
#include <QMutexLocker>
#include <QDebug>

#include "talk/media/base/videocommon.h"
#include "talk/media/base/videoframe.h"
#include "talk/media/base/videocapturer.h"
#include "talk/app/webrtc/videosourceinterface.h"

namespace WebRTC
{
    VideoRenderer::VideoRenderer(webrtc::VideoTrackInterface *track, bool flipHorizontal, QString windowTitle) :
        track_(track),
        flipHorizontal_(flipHorizontal),
        frameConsumed_(true)
    {
        track_->AddRenderer(this);

        // UI.
        setLayout(new QHBoxLayout());
        layout()->setContentsMargins(0,0,0,0);
        layout()->setSpacing(0);
        
        label_ = new QLabel(this);
        layout()->addWidget(label_);

        // Delete on close to ensure releasing of the track resource.  
        setAttribute(Qt::WA_DeleteOnClose, true);
        
        // Internal signal passing to main UI thread.
        connect(this, SIGNAL(Resize(QSize)), this, SLOT(OnResize(QSize)), Qt::QueuedConnection);
        connect(this, SIGNAL(Render(QImage)), this, SLOT(OnRender(QImage)), Qt::QueuedConnection);
        
        // Window title and show
        if (windowTitle.isEmpty())
            windowTitle = (track_->GetSource() && track_->GetSource()->GetVideoCapturer() ? track_->GetSource()->GetVideoCapturer()->GetId().c_str() : "");
        setWindowTitle("WebRTC Video Renderer" + (!windowTitle.isEmpty() ? ": " + windowTitle : ""));

        resize(480, 480);
        show();
    }

    VideoRenderer::~VideoRenderer()
    {
        Reset();
    }
    
    void VideoRenderer::Close()
    {
        Reset();
        close();
    }
    
    void VideoRenderer::Reset()
    {
        label_ = 0;
        if (track_)
            track_->RemoveRenderer(this);
        track_ = 0;
    }
    
    void VideoRenderer::SetSize(int width, int height)
    {
        emit Resize(QSize(width, height));
    }
    
    void VideoRenderer::RenderFrame(const cricket::VideoFrame* frame)
    {
        QMutexLocker lock(&mutex_);
        if (!frameConsumed_)
            return;
        frameConsumed_ = false;

        QImage image(frame->GetWidth(), frame->GetWidth(), QImage::Format_ARGB32);       
        frame->ConvertToRgbBuffer(cricket::FOURCC_ARGB, image.bits(), image.byteCount(), image.bytesPerLine());
        if (flipHorizontal_)
            image = image.mirrored(true, false);
        emit Render(image);
    }

    void VideoRenderer::OnResize(QSize size)
    {
        resize(size);
    }
    
    void VideoRenderer::OnRender(QImage image)
    {
        QMutexLocker lock(&mutex_);
        if (label_)
            label_->setPixmap(QPixmap::fromImage(image));
        frameConsumed_ = true;
    }
}
