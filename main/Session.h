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

#include "view/Pane.h"

#include "data/model/Model.h"

#include "piano-precision-aligner/Score.h"

class Session : public QObject
{
    Q_OBJECT
    
public:
    Session();
    virtual ~Session();

    struct AlignmentEntry
    {
        string label;
        float tick;
        int frame;

        AlignmentEntry(string l, float t, int f): label{l}, tick{t}, frame{f} { }
    };

    TimeValueLayer *getOnsetsLayer();
    Pane *getPaneContainingOnsetsLayer();
    
    TimeValueLayer *getTempoLayer();
    Pane *getPaneContainingTempoLayer();

    bool exportAlignmentTo(QString filename);
    bool importAlignmentFrom(QString filename);

    void setMusicalEvents(const Score::MusicalEventList *musicalEvents);

public slots:
    void setDocument(Document *,
                     Pane *topPane,
                     Pane *bottomPane,
                     Layer *timeRuler);

    void unsetDocument();
    
    void setMainModel(ModelId modelId, QString scoreId);

    void beginAlignment();

    void beginPartialAlignment(int scorePositionStart,
                               int scorePositionEnd,
                               sv_frame_t audioFrameStart,
                               sv_frame_t audioFrameEnd);
    void acceptAlignment();
    void rejectAlignment();
    
signals:
    void alignmentReadyForReview();
    void alignmentAccepted();
    void alignmentRejected();
    void alignmentFrameIlluminated(sv_frame_t);
                                       
protected slots:
    void modelReady(ModelId);
    
private:
    // I don't own any of these. The SV main window owns the document
    // and panes; the document owns the layers and models
    Document *m_document;
    QString m_scoreId;
    ModelId m_mainModel;

    Pane *m_topPane;
    Pane *m_bottomPane;
    Layer *m_timeRulerLayer;
    WaveformLayer *m_waveformLayer;
    SpectrogramLayer *m_spectrogramLayer;

    sv_frame_t m_partialAlignmentAudioStart;
    sv_frame_t m_partialAlignmentAudioEnd;
    
    TimeValueLayer *m_displayedOnsetsLayer;
    TimeValueLayer *m_acceptedOnsetsLayer;
    TimeValueLayer *m_pendingOnsetsLayer;
    bool m_awaitingOnsetsLayer;
    
    TimeValueLayer *m_tempoLayer;

    void alignmentComplete();
    void mergeLayers(TimeValueLayer *from, TimeValueLayer *to,
                     sv_frame_t overlapStart, sv_frame_t overlapEnd);
    void recalculateTempoLayer();

    const Score::MusicalEventList *m_musicalEvents; // I don't own this.
};

#endif
