/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "Session.h"

#include "transform/TransformFactory.h"
#include "transform/ModelTransformer.h"
#include "layer/ColourDatabase.h"
#include "layer/ColourMapper.h"

#include <QMessageBox>

using namespace std;

Session::Session()
{
    SVDEBUG << "Session::Session" << endl;
    setDocument(nullptr, nullptr, nullptr, nullptr);
}

Session::~Session()
{
}

void
Session::setDocument(Document *doc,
                     View *topView,
                     View *bottomView,
                     Layer *timeRuler)
{
    SVDEBUG << "Session::setDocument(" << doc << ")" << endl;
    
    m_document = doc;
    m_scoreId = "";
    m_mainModel = {};
    
    m_topView = topView;
    m_bottomView = bottomView;
    m_timeRulerLayer = timeRuler;
    m_waveformLayer = nullptr;
    m_spectrogramLayer = nullptr;

    m_partialAlignmentAudioStart = -1;
    m_partialAlignmentAudioEnd = -1;
    
    m_onsetsLayer = nullptr;
    m_pendingOnsetsLayer = nullptr;
    m_awaitingOnsetsLayer = false;
    
    m_tempoLayer = nullptr;
    m_pendingTempoLayer = nullptr;
    m_awaitingTempoLayer = false;
}

void
Session::unsetDocument()
{
    setDocument(nullptr, nullptr, nullptr, nullptr);
}

void
Session::setMainModel(ModelId modelId, QString scoreId)
{
    SVDEBUG << "Session::setMainModel(" << modelId << ")" << endl;
    
    m_mainModel = modelId;
    m_scoreId = scoreId;

    if (!m_document) {
        SVDEBUG << "Session::setMainModel: WARNING: No document; one should have been set first" << endl;
        return;
    }

    if (m_waveformLayer) {
        //!!! Review this
        SVDEBUG << "Session::setMainModel: Waveform layer already exists - currently we expect a process by which the document and panes are created and then setMainModel called here only once per document" << endl;
        return;
    }

    m_document->addLayerToView(m_bottomView, m_timeRulerLayer);
    
    ColourDatabase *cdb = ColourDatabase::getInstance();

    m_waveformLayer = qobject_cast<WaveformLayer *>
        (m_document->createLayer(LayerFactory::Waveform));
    m_waveformLayer->setBaseColour(cdb->getColourIndex(tr("Orange")));
    
    m_document->addLayerToView(m_topView, m_waveformLayer);
    m_document->setModel(m_waveformLayer, modelId);

    m_spectrogramLayer = qobject_cast<SpectrogramLayer *>
        (m_document->createLayer(LayerFactory::MelodicRangeSpectrogram));
    m_spectrogramLayer->setColourMap(ColourMapper::BlackOnWhite);

    m_document->addLayerToView(m_bottomView, m_spectrogramLayer);
    m_document->setModel(m_spectrogramLayer, modelId);
}

void
Session::beginAlignment()
{
    if (m_mainModel.isNone()) {
        SVDEBUG << "Session::beginAlignment: WARNING: No main model; one should have been set first" << endl;
        return;
    }

    beginPartialAlignment(-1, -1, -1, -1);
}

void
Session::beginPartialAlignment(int scorePositionStart,
                               int scorePositionEnd,
                               sv_frame_t audioFrameStart,
                               sv_frame_t audioFrameEnd)
{
    if (m_mainModel.isNone()) {
        SVDEBUG << "Session::beginPartialAlignment: WARNING: No main model; one should have been set first" << endl;
        return;
    }

    ModelTransformer::Input input(m_mainModel);

    vector<pair<QString, pair<View *, TimeValueLayer **>>> layerDefinitions {
        { "vamp:score-aligner:pianoaligner:chordonsets",
          { m_topView, &m_pendingOnsetsLayer }
        },
        { "vamp:score-aligner:pianoaligner:eventtempo",
          { m_bottomView, &m_pendingTempoLayer }
        }
    };

    vector<Layer *> newLayers;

    sv_samplerate_t sampleRate = ModelById::get(m_mainModel)->getSampleRate();
    RealTime audioStart, audioEnd;
    if (audioFrameStart == -1) {
        audioStart = RealTime::fromSeconds(-1.0);
    } else {
        audioStart = RealTime::frame2RealTime(audioFrameStart, sampleRate);
    }
    if (audioFrameEnd == -1) {
        audioEnd = RealTime::fromSeconds(-1.0);
    } else {
        audioEnd = RealTime::frame2RealTime(audioFrameEnd, sampleRate);
    }

    SVDEBUG << "Session::beginPartialAlignment: score position start = "
            << scorePositionStart << ", end = " << scorePositionEnd
            << ", audio start = " << audioStart << ", end = "
            << audioEnd << endl;
    
    Transform::ParameterMap params {
        { "score-position-start", scorePositionStart },
        { "score-position-end", scorePositionEnd },
        { "audio-start", float(audioStart.toDouble()) },
        { "audio-end", float(audioEnd.toDouble()) }
    };

    // General principle is to create new layers using
    // m_document->createDerivedLayer, which creates and attaches a
    // model and runs a transform in the background.
    //
    // If we have an existing layer of the same type already, we don't
    // delete it but we do temporarily hide it.
    //
    // When the model is complete, our callback is called; at this
    // moment, if we had a layer which is now hidden, we merge its
    // model with the new one (into the new layer, not the old) and
    // ask the user if they want to keep the new one. If so, we delete
    // the old; if not, we restore the old and delete the new.
    //
    //!!! What should we do if the user requests an alignment when we
    //!!! are already waiting for one to complete?
    
    for (auto defn : layerDefinitions) {

        auto transformId = defn.first;
        auto view = defn.second.first;
        auto layerPtr = defn.second.second;
        
        Transform t = TransformFactory::getInstance()->
            getDefaultTransformFor(transformId);

        t.setProgram(m_scoreId);
        t.setParameters(params);

        //!!! return error codes
    
        Layer *layer = m_document->createDerivedLayer(t, input);
        if (!layer) {
            SVDEBUG << "Session::beginPartialAlignment: Transform failed to initialise" << endl;
            return;
        }
        if (layer->getModel().isNone()) {
            SVDEBUG << "Session::beginPartialAlignment: Transform failed to create a model" << endl;
            return;
        }

        TimeValueLayer *tvl = qobject_cast<TimeValueLayer *>(layer);
        if (!tvl) {
            SVDEBUG << "Session::beginPartialAlignment: Transform resulted in wrong layer type" << endl;
            return;
        }

        if (*layerPtr) {
            m_document->deleteLayer(*layerPtr, true);
        }
            
        *layerPtr = tvl;
        
        m_document->addLayerToView(view, layer);

        Model *model = ModelById::get(layer->getModel()).get();
        connect(model, SIGNAL(ready(ModelId)),
                this, SLOT(modelReady(ModelId)));
    }

    // Hide the existing layers. This is only a temporary method of
    // removing them, normally we would go through the document if we
    // wanted to delete them entirely
    if (m_onsetsLayer) {
        m_topView->removeLayer(m_onsetsLayer);
    }
    if (m_tempoLayer) {
        m_bottomView->removeLayer(m_tempoLayer);
    }
        
    ColourDatabase *cdb = ColourDatabase::getInstance();
    
    m_pendingOnsetsLayer->setPlotStyle(TimeValueLayer::PlotSegmentation);
    m_pendingOnsetsLayer->setDrawSegmentDivisions(true);
    m_pendingOnsetsLayer->setFillSegments(false);

    m_pendingTempoLayer->setPlotStyle(TimeValueLayer::PlotLines);
    m_pendingTempoLayer->setBaseColour(cdb->getColourIndex(tr("Blue")));

    m_partialAlignmentAudioStart = audioFrameStart;
    m_partialAlignmentAudioEnd = audioFrameEnd;
    
    m_awaitingOnsetsLayer = true;
    m_awaitingTempoLayer = true;
}

void
Session::modelReady(ModelId id)
{
    SVDEBUG << "Session::modelReady: model is " << id << endl;

    if (m_pendingOnsetsLayer && id == m_pendingOnsetsLayer->getModel()) {
        m_awaitingOnsetsLayer = false;
    }
    if (m_pendingTempoLayer && id == m_pendingTempoLayer->getModel()) {
        m_awaitingTempoLayer = false;
    }

    if (!m_awaitingOnsetsLayer && !m_awaitingTempoLayer) {
        alignmentComplete();
    }
}

void
Session::alignmentComplete()
{
    SVDEBUG << "Session::alignmentComplete" << endl;

    if (QMessageBox::question
        (m_topView, tr("Save alignment?"),
         tr("<b>Alignment finished</b><p>Do you want to keep this alignment?"),
         QMessageBox::Save | QMessageBox::Cancel,
         QMessageBox::Save) !=
        QMessageBox::Save) {

        SVDEBUG << "Session::alignmentComplete: Cancel chosen" << endl;
        
        m_document->deleteLayer(m_pendingOnsetsLayer, true);
        m_pendingOnsetsLayer = nullptr;

        m_document->deleteLayer(m_pendingTempoLayer, true);
        m_pendingTempoLayer = nullptr;

        if (m_onsetsLayer) {
            m_topView->addLayer(m_onsetsLayer);
        }
        if (m_tempoLayer) {
            m_topView->addLayer(m_tempoLayer);
        }

        return;
    }
        
    SVDEBUG << "Session::alignmentComplete: Save chosen" << endl;

    if (m_onsetsLayer) {
        mergeLayers(m_onsetsLayer, m_pendingOnsetsLayer,
                    m_partialAlignmentAudioStart, m_partialAlignmentAudioEnd);
    }
    if (m_tempoLayer) {
        mergeLayers(m_tempoLayer, m_pendingTempoLayer,
                    m_partialAlignmentAudioStart, m_partialAlignmentAudioEnd);
    }
    
    if (m_onsetsLayer) {
        m_document->deleteLayer(m_onsetsLayer, true);
    }
    m_onsetsLayer = m_pendingOnsetsLayer;
    m_pendingOnsetsLayer = nullptr;
    
    if (m_tempoLayer) {
        m_document->deleteLayer(m_tempoLayer, true);
    }
    m_tempoLayer = m_pendingTempoLayer;
    m_pendingTempoLayer = nullptr;
}

void
Session::mergeLayers(TimeValueLayer *from, TimeValueLayer *to,
                     sv_frame_t overlapStart, sv_frame_t overlapEnd)
{
    // Currently the way we are handling this is by having "to"
    // contain *only* the new events, within overlapStart to
    // overlapEnd.  So the merge just copies all events outside that
    // range from "from" to "to". There are surely cleverer ways

    //!!! We should also use a command
    
    auto fromModel = ModelById::getAs<SparseTimeValueModel>(from->getModel());
    auto toModel = ModelById::getAs<SparseTimeValueModel>(to->getModel());

    EventVector beforeOverlap = fromModel->getEventsWithin(0, overlapStart);
    EventVector afterOverlap = fromModel->getEventsWithin
        (overlapEnd, fromModel->getEndFrame() - overlapEnd);

    for (auto e : beforeOverlap) toModel->add(e);
    for (auto e : afterOverlap) toModel->add(e);
}


