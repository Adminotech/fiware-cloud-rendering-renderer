/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#pragma once

#include <string>
#include <vector>

#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/base/stringencode.h"

/// @note MediaContraints base code structure copied from the webrtc library sources.

namespace WebRTC
{
    class MediaConstraints : public webrtc::MediaConstraintsInterface
    {

    public:
        MediaConstraints() {}
        virtual ~MediaConstraints() {}
        
        void Reset()
        {
            mandatory_.clear();
            optional_.clear();
        }

        virtual const Constraints& GetMandatory() const
        {
            return mandatory_;
        }

        virtual const Constraints& GetOptional() const
        {
            return optional_;
        }

        template <class T>
        void AddMandatory(const std::string& key, const T& value)
        {
            mandatory_.push_back(Constraint(key, talk_base::ToString<T>(value)));
        }

        template <class T>
        void AddOptional(const std::string& key, const T& value)
        {
            optional_.push_back(Constraint(key, talk_base::ToString<T>(value)));
        }

        void SetMandatoryMinAspectRatio(double ratio)
        {
            AddMandatory(MediaConstraintsInterface::kMinAspectRatio, ratio);
        }

        void SetMandatoryMinWidth(int width)
        {
            AddMandatory(MediaConstraintsInterface::kMinWidth, width);
        }

        void SetMandatoryMinHeight(int height)
        {
            AddMandatory(MediaConstraintsInterface::kMinHeight, height);
        }

        void SetOptionalMaxWidth(int width)
        {
            AddOptional(MediaConstraintsInterface::kMaxWidth, width);
        }

        void SetMandatoryMaxFrameRate(int frame_rate)
        {
            AddMandatory(MediaConstraintsInterface::kMaxFrameRate, frame_rate);
        }

        void SetMandatoryReceiveAudio(bool enable)
        {
            AddMandatory(MediaConstraintsInterface::kOfferToReceiveAudio, enable);
        }

        void SetMandatoryReceiveVideo(bool enable)
        {
            AddMandatory(MediaConstraintsInterface::kOfferToReceiveVideo, enable);
        }

        void SetMandatoryUseRtpMux(bool enable)
        {
            AddMandatory(MediaConstraintsInterface::kUseRtpMux, enable);
        }

        void SetMandatoryIceRestart(bool enable)
        {
            AddMandatory(MediaConstraintsInterface::kIceRestart, enable);
        }

        void SetOptionalVAD(bool enable)
        {
            AddOptional(MediaConstraintsInterface::kVoiceActivityDetection, enable);
        }

        void SetAllowRtpDataChannels()
        {
            AddMandatory(MediaConstraintsInterface::kEnableRtpDataChannels, true);
        }
#ifndef __APPLE__
        void SetAllowDtlsSctpDataChannels()
        {
            AddMandatory(MediaConstraintsInterface::kEnableSctpDataChannels, true);
            AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp, true);
        }
#endif
    private:
        Constraints mandatory_;
        Constraints optional_;
    };
}
