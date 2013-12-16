/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#pragma once

#include "CloudRenderingPluginApi.h"
#include "CloudRenderingPluginFwd.h"

#include <QString>
#include <QVariant>
#include <QList>

namespace WebRTC
{
    /// %SDP is a structure used in the %WebRTC peer to peer connection setup.
    struct SDP
    {
        SDP(const QString &type_ = "", const QString &sdp_ = "");

        QByteArray ToJSON() const;
        QVariant ToVariant() const;

        bool FromJSON(const QByteArray &sdp_);
        bool FromVariant(const QVariantMap &v);

        void Dump();

        QString type;
        QString sdp;
    };

    /// %ICECandidate is a structure used in the %WebRTC peer to peer connection setup.
    struct ICECandidate
    {
        ICECandidate(int sdpMLineIndex_ = 0, const QString &sdpMid_ = "", const QString &candidate_ = "");

        QByteArray ToJSON() const;
        QVariant ToVariant() const;

        bool FromJSON(const QByteArray &sdp);
        bool FromVariant(const QVariantMap &v);

        void Dump();

        int sdpMLineIndex;
        QString sdpMid;
        QString candidate;
    };
    
    /// ICE candidate list.
    typedef QList<ICECandidate> ICECandidateList;
    
    /// Creates ICECandidateList from QVariantList.
    ICECandidateList IceCandidatesFromVariant(const QVariantList &candidateVariants);
    
    /// Creates QVariantList from ICECandidateList.
    QVariantList VariantFromIceCandidates(const ICECandidateList &candidates);
    
    inline static void RegisterMetaTypes()
    {
        qRegisterMetaType<WebRTC::SDP>("WebRTC::SDP");
        qRegisterMetaType<WebRTC::ICECandidate>("WebRTC::ICECandidate");
        qRegisterMetaType<WebRTC::ICECandidateList>("WebRTC::ICECandidateList");
    }
}
