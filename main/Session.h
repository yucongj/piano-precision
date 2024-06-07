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
#include "layer/TimeInstantLayer.h"
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
        std::string label;
        int frame;

        AlignmentEntry(std::string l, int f): label{l}, frame{f} { }
    };

    sv::TimeInstantLayer *getOnsetsLayer();
    sv::Pane *getPaneContainingOnsetsLayer();
    
    sv::TimeValueLayer *getTempoLayer();
    sv::Pane *getPaneContainingTempoLayer();

    bool exportAlignmentTo(QString filename);
    bool importAlignmentFrom(QString filename);

    void setMusicalEvents(const Score::MusicalEventList &musicalEvents);

public slots:
    void setDocument(sv::Document *,
                     sv::Pane *topPane,
                     sv::Pane *bottomPane,
                     sv::Layer *timeRuler);

    void unsetDocument();
    
    void setMainModel(sv::ModelId modelId, QString scoreId);

    void beginAlignment();

    void beginPartialAlignment(int scorePositionStartNumerator,
                               int scorePositionStartDenominator,
                               int scorePositionEndNumerator,
                               int scorePositionEndDenominator,
                               sv::sv_frame_t audioFrameStart,
                               sv::sv_frame_t audioFrameEnd);
    void acceptAlignment();
    void rejectAlignment();

    void signifyEditMode();
    void signifyNavigateMode();
    
signals:
    void alignmentReadyForReview();
    void alignmentAccepted();
    void alignmentRejected();
    void alignmentModified();
    void alignmentFrameIlluminated(sv::sv_frame_t);
                                       
protected slots:
    void modelChanged(sv::ModelId);
    void modelReady(sv::ModelId);
    
private:
    // I don't own any of these. The SV main window owns the document
    // and panes; the document owns the layers and models
    sv::Document *m_document;
    QString m_scoreId;
    sv::ModelId m_mainModel;

    sv::Pane *m_topPane;
    sv::Pane *m_bottomPane;
    sv::Layer *m_timeRulerLayer;
    sv::WaveformLayer *m_waveformLayer;
    sv::SpectrogramLayer *m_spectrogramLayer;

    sv::sv_frame_t m_partialAlignmentAudioStart;
    sv::sv_frame_t m_partialAlignmentAudioEnd;
    
    sv::TimeInstantLayer *m_displayedOnsetsLayer; // An alias for one of:
    sv::TimeInstantLayer *m_acceptedOnsetsLayer;
    sv::TimeInstantLayer *m_pendingOnsetsLayer;
    bool m_awaitingOnsetsLayer;
    
    sv::TimeValueLayer *m_tempoLayer;

    bool m_inEditMode;
    
    void setOnsetsLayerProperties(sv::TimeInstantLayer *);
    void alignmentComplete();
    void mergeLayers(sv::TimeInstantLayer *from, sv::TimeInstantLayer *to,
                     sv::sv_frame_t overlapStart, sv::sv_frame_t overlapEnd);
    void recalculateTempoLayer();
    void updateOnsetColours();

    void resetAlignmentEntries();
    bool updateAlignmentEntries();
    bool exportAlignmentEntriesTo(QString path);

    Score::MusicalEventList m_musicalEvents;
    std::vector<AlignmentEntry> m_alignmentEntries;
};

#endif
