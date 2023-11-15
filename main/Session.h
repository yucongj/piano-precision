/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_PP_SESSION_H
#define SV_PP_SESSION_H

#include "framework/Document.h"

#include "layer/TimeRulerLayer.h"
#include "layer/WaveformLayer.h"
#include "layer/SpectrogramLayer.h"
#include "layer/TimeValueLayer.h"

#include "view/View.h"

#include "data/model/Model.h"

class Session : public QObject
{
    Q_OBJECT
    
public:
    Session();
    virtual ~Session();

    void setDocument(Document *,
                     View *topView,
                     View *bottomView,
                     Layer *timeRuler);

    void unsetDocument();
    
    void setMainModel(ModelId modelId, QString scoreId);

    void beginAlignment();

    void beginPartialAlignment(int scorePositionStart,
                               int scorePositionEnd,
                               sv_frame_t audioFrameStart,
                               sv_frame_t audioFrameEnd);

protected slots:
    void modelReady(ModelId);
    
private:
    // I don't own any of these. The SV main window owns the document
    // and views; the document owns the layers and models
    Document *m_document;
    QString m_scoreId;
    ModelId m_mainModel;

    View *m_topView;
    View *m_bottomView;
    Layer *m_timeRulerLayer;
    WaveformLayer *m_waveformLayer;
    SpectrogramLayer *m_spectrogramLayer;

    sv_frame_t m_partialAlignmentAudioStart;
    sv_frame_t m_partialAlignmentAudioEnd;
    
    TimeValueLayer *m_onsetsLayer;
    TimeValueLayer *m_pendingOnsetsLayer;
    bool m_awaitingOnsetsLayer;
    
    TimeValueLayer *m_tempoLayer;
    TimeValueLayer *m_pendingTempoLayer;
    bool m_awaitingTempoLayer;

    void alignmentComplete();
    void mergeLayers(TimeValueLayer *from, TimeValueLayer *to,
                     sv_frame_t overlapStart, sv_frame_t overlapEnd);
};

#endif
