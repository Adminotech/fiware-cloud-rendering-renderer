/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#pragma once

#include "CloudRenderingPluginApi.h"
#include "CloudRenderingPluginFwd.h"
#include "WebRTCRenderer.h"

#include <QObject>

#include "talk/media/base/videocommon.h"
#include "talk/media/base/videocapturer.h"
#include "talk/base/timeutils.h"

namespace WebRTC
{
    class TundraCapturer : public QObject, 
                           public cricket::VideoCapturer, 
                           public TundraRendererConsumer
    {
        Q_OBJECT

    public:
        TundraCapturer(Framework *framework);
        ~TundraCapturer();
        
        /// cricket::VideoCapturer overrides.
        cricket::CaptureState Start(const cricket::VideoFormat& capture_format);
        void Stop();
        bool IsRunning();
        bool IsScreencast() const;
        
        /// TundraRendererConsumer implementation
        void OnTundraFrame(const QImage *frame);
        
    protected:
        /// cricket::VideoCapturer overrides.
        bool GetPreferredFourccs(std::vector<uint32>* fourccs);
        
    private:
        Framework *framework_;
        bool running_;
        
        shared_ptr<TundraCapturer> selfShared_;
        
        uint64 time_;
    };
}
