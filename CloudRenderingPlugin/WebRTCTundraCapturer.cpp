/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

// Hack around Windows GDI Polygon define
#include "Win.h"
#include "EC_Camera.h"

#include "WebRTCTundraCapturer.h"
#include "CloudRenderingPlugin.h"

#include "Framework.h"
#include "FrameAPI.h"
#include "IRenderer.h"
#include "Profiler.h"
#include "UiAPI.h"
#include "UiMainWindow.h"

#include <QImage>
#include <QDebug>

#include "OgreRenderingModule.h"
#include "Renderer.h"
#include "OgreTextureManager.h"
#include "OgreTexture.h"
#include "OgreHardwarePixelBuffer.h"

#ifdef DIRECTX_ENABLED
#include <d3d9.h>
#include <OgreD3D9HardwarePixelBuffer.h>
#include <OgreD3D9RenderWindow.h>
#endif

#include "talk/base/scoped_ptr.h"

namespace WebRTC
{
    TundraCapturer::TundraCapturer(Framework *framework) :
        framework_(framework),
        running_(false),
        time_(talk_base::Time())
    {        
        // Default supported formats. Use ResetSupportedFormats to over write.
        std::vector<cricket::VideoFormat> formats;
        formats.push_back(cricket::VideoFormat(1280, 720,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_ARGB));
        formats.push_back(cricket::VideoFormat(640, 480,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_ARGB));
        formats.push_back(cricket::VideoFormat(320, 240,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_ARGB));
        formats.push_back(cricket::VideoFormat(160, 120,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_ARGB));
        SetSupportedFormats(formats);
    }

    TundraCapturer::~TundraCapturer()
    {
        selfShared_.reset();
    }
    
    void TundraCapturer::OnTundraFrame(const QImage *frame)
    {
        PROFILE(CloudRendering_TundraCapturer_OnTundraFrame)
        
        const cricket::VideoFormat *format = GetCaptureFormat();
        if (!frame || !running_ || !format || format->fourcc != cricket::FOURCC_ARGB)
            return;
        
        // Frame
        cricket::CapturedFrame out;

        // Time
        uint64 currentTime = talk_base::Time();
        out.elapsed_time = static_cast<int64>(talk_base::TimeDiff(currentTime, time_)) * talk_base::kNumNanosecsPerMillisec;
        out.time_stamp = static_cast<int64>(currentTime) * talk_base::kNumNanosecsPerMillisec;
        time_ = currentTime;
        
        int numBytes = frame->byteCount();
        talk_base::scoped_ptr<char[]> data(new char[numBytes]);
        memcpy(static_cast<void*>(data.get()), static_cast<const void*>(frame->bits()), numBytes);

        out.fourcc = format->fourcc;
        out.width = frame->width();
        out.height = frame->height();
        out.data_size = numBytes;
        out.data = data.get();
        
        SignalFrameCaptured(this, &out);
    }
    
    cricket::CaptureState TundraCapturer::Start(const cricket::VideoFormat& format)
    {
        if (IsLogChannelEnabled(LogChannelDebug))
        {
            qDebug() << "TundraCapturer::Start()";
            qDebug() << "  -- Capture format      " << format.fourcc << "size =" << format.width << "x" << format.height << "interval =" << format.interval;
        }
        
        cricket::VideoFormat supported;
        if (GetBestCaptureFormat(format, &supported))
        {
            if (IsLogChannelEnabled(LogChannelDebug))
                qDebug() << "  -- Best capture format " << supported.fourcc << "size =" << supported.width << "x" << supported.height << "interval =" << supported.interval;
            SetCaptureFormat(&supported);
        }

        selfShared_ = shared_ptr<TundraCapturer>(this);

        CloudRenderingPlugin *plugin = framework_->Module<CloudRenderingPlugin>();
        if (plugin && plugin->Renderer() && plugin->Renderer()->ApplicationRenderer())
        {
            plugin->Renderer()->ApplicationRenderer()->SetInterval(format.framerate());
            if (plugin->GetFramework()->HasCommandLineParameter("--cloudRenderingNoForceResize")) // @todo: document this
                plugin->Renderer()->ApplicationRenderer()->SetSize(format.width, format.height - 21); // Hack for QMenuBar height
            else
                plugin->Renderer()->ApplicationRenderer()->SetSize(1280, 720 - 21);

            plugin->Renderer()->ApplicationRenderer()->Register(selfShared_);
        }

        running_ = true;
        time_ = talk_base::Time();
        
        SetCaptureState(cricket::CS_RUNNING);
        return cricket::CS_RUNNING;
    }
    
    void TundraCapturer::Stop()
    {
        selfShared_.reset();

        running_ = false;
        SetCaptureFormat(NULL);
        SetCaptureState(cricket::CS_STOPPED);
        
        if (IsLogChannelEnabled(LogChannelDebug))
            qDebug() << "TundraCapturer() Stop";
    }
    bool TundraCapturer::IsRunning()
    {
        if (IsLogChannelEnabled(LogChannelDebug))
            qDebug() << "TundraCapturer() IsRunning" << running_;
        return running_;
    }
    
    bool TundraCapturer::GetPreferredFourccs(std::vector<uint32>* fourccs)
    {
        if (IsLogChannelEnabled(LogChannelDebug))
            qDebug() << "TundraCapturer() GetPreferredFourccs";
        //fourccs->push_back(cricket::FOURCC_I420);
        //fourccs->push_back(cricket::FOURCC_MJPG);
        fourccs->push_back(cricket::FOURCC_ARGB);
        return true;
    }

    bool TundraCapturer::IsScreencast() const
    {
        return true;
    }
}
