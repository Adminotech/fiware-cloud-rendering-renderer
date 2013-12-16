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

#include <d3d9.h>
#include <OgreD3D9HardwarePixelBuffer.h>
#include <OgreD3D9RenderWindow.h>

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
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_ARGB)); // FOURCC_ARGB
        formats.push_back(cricket::VideoFormat(640, 480,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_ARGB));
        formats.push_back(cricket::VideoFormat(320, 240,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_ARGB));
        formats.push_back(cricket::VideoFormat(160, 120,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_ARGB));
        SetSupportedFormats(formats);
        
        //connect(framework_->Frame(), SIGNAL(PostFrameUpdate(float)), SLOT(OnPostUpdate(float)));
    }

    TundraCapturer::~TundraCapturer()
    {
        qDebug() << "~TundraCapturer dtor";
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
        talk_base::scoped_array<char> data(new char[numBytes]);
        memcpy(static_cast<void*>(data.get()), static_cast<const void*>(frame->bits()), numBytes);

        out.fourcc = format->fourcc;
        out.width = frame->width();
        out.height = frame->height();
        out.data_size = numBytes;
        out.data = data.get();
        
        SignalFrameCaptured(this, &out);
    }
        
    void TundraCapturer::OnPostUpdate(float frametime)
    {
        PROFILE(CloudRendering_TundraCapturer_Render)
        return;

        const cricket::VideoFormat *format = GetCaptureFormat();
        if (!running_ || !format || format->fourcc != cricket::FOURCC_ARGB)
            return;

        OgreRenderer::OgreRenderingModule *ogreModule = framework_->Module<OgreRenderer::OgreRenderingModule>();
        if (!ogreModule || !ogreModule->Renderer())
            return;
        Ogre::D3D9RenderWindow *d3d9rw = dynamic_cast<Ogre::D3D9RenderWindow*>(ogreModule->Renderer()->GetCurrentRenderWindow());
        if (!d3d9rw || !d3d9rw->getD3D9Device()) // We're not using D3D9.
            return;
/*            
        IDirect3DSurface9 *surface = 0;
        if (FAILED(d3d9rw->getD3D9Device()->GetFrontBufferData(0, surface)))
            return;

        D3DSURFACE_DESC desc;
        if (FAILED(surface->GetDesc(&desc)))
            return;
            
        if (desc.Format != D3DFMT_R8G8B8)
        {
            qDebug() << "Shit format" << (int)desc.Format;
        }

        D3DLOCKED_RECT lock;
        RECT lockRect = { 0, 0, 640, 480 };
        if (FAILED(surface->LockRect(&lock, &lockRect, 0)))
            return;
            
        surface->UnlockRect();

        talk_base::scoped_array<char> data(new char[640 * 480 * 3]);
*/
        EC_Camera *camera = framework_->Renderer()->MainCameraComponent();
        if (!camera)
            return;
     
        int width = format->width;
        int height = format->height;
        u32 fourcc = format->fourcc;
        
        PROFILE(CloudRendering_TundraCapturer_Update_Render_Texture)
        //Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().getByName("MainWindow RTT");
        Ogre::Texture *texture = camera->UpdateRenderTexture(true).get();
        Ogre::D3D9HardwarePixelBuffer *buffer = texture ? dynamic_cast<Ogre::D3D9HardwarePixelBuffer*>(texture->getBuffer().get()) : 0;
        if (!buffer || buffer->getFormat() != Ogre::PF_A8R8G8B8)
            return;
        ELIFORP(CloudRendering_TundraCapturer_Update_Render_Texture)
        
        PROFILE(CloudRendering_TundraCapturer_Copy_Frame)
        
        // Frame        
        cricket::CapturedFrame frame;
        frame.width = width;
        frame.height = height;
        frame.fourcc = fourcc;

        // Time
        uint64 currentTime = talk_base::Time();
        frame.elapsed_time = static_cast<int64>(talk_base::TimeDiff(currentTime, time_)) * talk_base::kNumNanosecsPerMillisec;
        frame.time_stamp = static_cast<int64>(currentTime) * talk_base::kNumNanosecsPerMillisec;
        time_ = currentTime;
        
        IDirect3DSurface9 *surface = buffer->getSurface(d3d9rw->getD3D9Device());
        if (!surface)
            return;

        D3DSURFACE_DESC desc;
        if (FAILED(surface->GetDesc(&desc)))
            return;

        D3DLOCKED_RECT lock;
        RECT lockRect = { 0, 0, desc.Width, desc.Height };
        HRESULT hr = surface->LockRect(&lock, NULL, D3DLOCK_READONLY);
        qDebug() << hr;
        if (FAILED(hr))
            return;

        int numBytes = desc.Width * desc.Height * 4;
        talk_base::scoped_array<char> data(new char[numBytes]);
        memcpy(static_cast<void*>(data.get()), lock.pBits, numBytes);
        surface->UnlockRect();
        
        frame.width = desc.Width;
        frame.height = desc.Height;
        frame.data_size = numBytes;
        frame.data = data.get();
        SignalFrameCaptured(this, &frame);

/*
        try
        {       
            PROFILE(TundraCapturer_Ogre_Blit_To_Memory)
            Ogre::PixelBox destPixels(buffer->getWidth(), buffer->getHeight(), 1, Ogre::PF_A8R8G8B8);
            talk_base::scoped_array<char> data(new char[destPixels.getConsecutiveSize()]);
            destPixels.data = static_cast<void*>(data.get());

            buffer->blitToMemory(destPixels);
            
            // Scaling needed
            if (buffer->getWidth() != width || buffer->getHeight() != height)
            {
                PROFILE(TundraCapturer_Ogre_Scale)
                Ogre::PixelBox scaledPixels(width, height, 1, Ogre::PF_A8R8G8B8);
                talk_base::scoped_array<char> scaledData(new char[scaledPixels.getConsecutiveSize()]);
                scaledPixels.data = static_cast<void*>(scaledData.get());

                Ogre::Image::scale(destPixels, scaledPixels, Ogre::Image::FILTER_NEAREST);
                ELIFORP(TundraCapturer_Ogre_Scale)

                frame.data_size = scaledPixels.getConsecutiveSize();    
                frame.data = scaledData.get();
                SignalFrameCaptured(this, &frame);
            }
            // We did a 1:1 data copy
            else
            {
                frame.data_size = destPixels.getConsecutiveSize();
                frame.data = data.get();
                SignalFrameCaptured(this, &frame);
            }
        }
        catch (Ogre::Exception &e)
        {
            LogError(QString("[TundraCapturer]: Ogre exception while blitting frame to WebRTC video: ") + e.what());
            running_ = false;
        }
*/
/*        
        int size = pixels.getConsecutiveSize();
        int sizeRow = pixels.rowPitch();

        QImage img = camera->ToQImage(true);
        if (img.isNull())
            return;
       
        if (img.format() != QImage::Format_ARGB32 && img.format() != QImage::Format_ARGB32_Premultiplied)
        {
            PROFILE(TundraCapturer_Convert_Texture)
            img = img.convertToFormat(QImage::Format_ARGB32);
        }

        if (img.width() != width || img.height() != height)
        {
            PROFILE(TundraCapturer_Resize_Texture)
            img = img.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
*/
    }
    
    cricket::CaptureState TundraCapturer::Start(const cricket::VideoFormat& format)
    {
        qDebug() << "TundraCapturer() Start";
        qDebug() << "  -- Capture format      " << format.fourcc << "size =" << format.width << "x" << format.height << "interval =" << format.interval;
        
        cricket::VideoFormat supported;
        if (GetBestCaptureFormat(format, &supported)) {
            qDebug() << "  -- Best capture format " << supported.fourcc << "size =" << supported.width << "x" << supported.height << "interval =" << supported.interval;
            SetCaptureFormat(&supported);
        }
        
        selfShared_ = shared_ptr<TundraCapturer>(this);

        CloudRenderingPlugin *plugin = framework_->Module<CloudRenderingPlugin>();
        if (plugin && plugin->Renderer() && plugin->Renderer()->ApplicationRenderer())
        {
            plugin->Renderer()->ApplicationRenderer()->SetInterval(format.framerate());
            plugin->Renderer()->ApplicationRenderer()->SetSize(format.width, format.height);
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
        qDebug() << "TundraCapturer() Stop";
    }
    bool TundraCapturer::IsRunning()
    {
        qDebug() << "TundraCapturer() IsRunning";
        return running_;
    }
    
    bool TundraCapturer::GetPreferredFourccs(std::vector<uint32>* fourccs)
    {
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
