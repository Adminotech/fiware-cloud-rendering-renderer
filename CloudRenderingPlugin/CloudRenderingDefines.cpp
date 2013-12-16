
// For conditions of distribution and use, see copyright notice in LICENSE

#include "CloudRenderingPluginApi.h"
#include "CloudRenderingPluginFwd.h"
#include "CloudRenderingDefines.h"

#include "CoreJsonUtils.h"
#include "LoggingFunctions.h"

#include <QDebug>

namespace WebRTC
{
    // SDP

    SDP::SDP(const QString &type_, const QString &sdp_)
    {
        type = type_;
        sdp = sdp_;
    }

    QByteArray SDP::ToJSON() const
    {
        bool ok = false;
        QByteArray json = TundraJson::Serialize(ToVariant(), TundraJson::IndentNone, &ok);
        return (ok ? json : "");
    }

    QVariant SDP::ToVariant() const
    {
        QVariantMap v;
        v["type"] = type;
        v["sdp"] = sdp;
        return v;
    }

    bool SDP::FromJSON(const QByteArray &sdp_)
    {
        bool ok = false;
        QVariantMap v = TundraJson::Parse(sdp_, &ok).toMap();
        if (ok)
            return FromVariant(v);
        return false;
    }

    bool SDP::FromVariant(const QVariantMap &v)
    {
        type = v.value("type", "").toString();
        sdp = v.value("sdp", "").toString();
        return (!type.isEmpty() && !sdp.isEmpty());
    }

    void SDP::Dump()
    {
        qDebug() << endl << "SDP:" << qPrintable(type) << endl
                 << sdp << endl;
    }

    // ICECandidate

    ICECandidate::ICECandidate(int sdpMLineIndex_, const QString &sdpMid_, const QString &candidate_)
    {
        sdpMLineIndex = sdpMLineIndex_;
        sdpMid = sdpMid_;
        candidate = candidate_;
    }

    QByteArray ICECandidate::ToJSON() const
    {
        bool ok = false;
        QByteArray json = TundraJson::Serialize(ToVariant(), TundraJson::IndentNone, &ok);
        return (ok ? json : "");
    }

    QVariant ICECandidate::ToVariant() const
    {
        QVariantMap v;
        v["sdpMLineIndex"] = sdpMLineIndex;
        v["sdpMid"] = sdpMid;
        v["candidate"] = candidate;
        return v;
    }

    bool ICECandidate::FromJSON(const QByteArray &sdp)
    {
        bool ok = false;
        QVariantMap v = TundraJson::Parse(sdp, &ok).toMap();
        if (ok)
            return FromVariant(v);
        return false;
    }

    bool ICECandidate::FromVariant(const QVariantMap &v)
    {
        sdpMLineIndex = v.value("sdpMLineIndex", 0).toInt();
        sdpMid = v.value("sdpMid", "").toString();
        candidate = v.value("candidate", "").toString();

        return (!sdpMid.isEmpty() && !candidate.isEmpty());
    }

    void ICECandidate::Dump()
    {
        qDebug() << endl << "ICECandidate:" << sdpMLineIndex << qPrintable(sdpMid) << endl
                 << candidate << endl;
    }
    
    ICECandidateList IceCandidatesFromVariant(const QVariantList &candidateVariants)
    {
        ICECandidateList outCandidates;
        foreach(const QVariant &candidateVariant, candidateVariants)
        {
            if (!TundraJson::IsMap(candidateVariant))
            {
                LogError("IceCandidatesFromVariant: One of the given ICE candidates is not type of QVariantMap, skipping...");
                continue;
            }
            WebRTC::ICECandidate candidate; 
            if (candidate.FromVariant(candidateVariant.toMap()))
                outCandidates << candidate;
        }
        return outCandidates;
    }
    
    QVariantList VariantFromIceCandidates(const ICECandidateList &candidates)
    {
        QVariantList candidateVariants;
        foreach(WebRTC::ICECandidate candidate, candidates)
            candidateVariants << candidate.ToVariant();
        return candidateVariants;
    }
}
