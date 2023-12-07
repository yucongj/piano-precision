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

#include "data/fileio/CSVFormat.h"
#include "data/fileio/CSVFileReader.h"
#include "data/fileio/CSVFileWriter.h"

#include <QMessageBox>
#include <QFileInfo>

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
                     Pane *topPane,
                     Pane *bottomPane,
                     Layer *timeRuler)
{
    SVDEBUG << "Session::setDocument(" << doc << ")" << endl;
    
    m_document = doc;
    m_scoreId = "";
    m_mainModel = {};
    
    m_topPane = topPane;
    m_bottomPane = bottomPane;
    m_timeRulerLayer = timeRuler;
    m_waveformLayer = nullptr;
    m_spectrogramLayer = nullptr;

    m_partialAlignmentAudioStart = -1;
    m_partialAlignmentAudioEnd = -1;
    
    m_displayedOnsetsLayer = nullptr;
    m_acceptedOnsetsLayer = nullptr;
    m_pendingOnsetsLayer = nullptr;
    m_awaitingOnsetsLayer = false;
    
    m_tempoLayer = nullptr;
}

void
Session::unsetDocument()
{
    setDocument(nullptr, nullptr, nullptr, nullptr);
}

TimeValueLayer *
Session::getOnsetsLayer()
{
    return m_displayedOnsetsLayer;
}

Pane *
Session::getPaneContainingOnsetsLayer()
{
    return m_topPane;
}

TimeValueLayer *
Session::getTempoLayer()
{
    return m_tempoLayer;
}

Pane *
Session::getPaneContainingTempoLayer()
{
    return m_bottomPane;
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

    m_document->addLayerToView(m_bottomPane, m_timeRulerLayer);
    
    ColourDatabase *cdb = ColourDatabase::getInstance();

    m_waveformLayer = qobject_cast<WaveformLayer *>
        (m_document->createLayer(LayerFactory::Waveform));
    m_waveformLayer->setBaseColour(cdb->getColourIndex(tr("Orange")));
    
    m_document->addLayerToView(m_bottomPane, m_waveformLayer);
    m_document->setModel(m_waveformLayer, modelId);

    m_spectrogramLayer = qobject_cast<SpectrogramLayer *>
        (m_document->createLayer(LayerFactory::MelodicRangeSpectrogram));
    m_spectrogramLayer->setColourMap(ColourMapper::Green);

    m_document->addLayerToView(m_topPane, m_spectrogramLayer);
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

    vector<pair<QString, pair<Pane *, TimeValueLayer **>>> layerDefinitions {
        { "vamp:score-aligner:pianoaligner:chordonsets",
          { m_topPane, &m_pendingOnsetsLayer }
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
        auto pane = defn.second.first;
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
        
        m_document->addLayerToView(pane, layer);

        ModelId modelId = layer->getModel();
        auto model = ModelById::get(modelId);
        if (model->isReady(nullptr)) {
            modelReady(modelId);
        } else {
            connect(model.get(), SIGNAL(ready(ModelId)),
                    this, SLOT(modelReady(ModelId)));
        }
    }

    // Hide the existing layers. This is only a temporary method of
    // removing them, normally we would go through the document if we
    // wanted to delete them entirely
    if (m_displayedOnsetsLayer) {
        m_acceptedOnsetsLayer = m_displayedOnsetsLayer;
        m_topPane->removeLayer(m_displayedOnsetsLayer);
    }
    if (m_tempoLayer) {
        m_bottomPane->removeLayer(m_tempoLayer);
    }
        
    m_displayedOnsetsLayer = m_pendingOnsetsLayer;

    m_pendingOnsetsLayer->setPlotStyle(TimeValueLayer::PlotSegmentation);
    m_pendingOnsetsLayer->setDrawSegmentDivisions(true);
    m_pendingOnsetsLayer->setFillSegments(false);
    m_pendingOnsetsLayer->setPermitValueEditOfSegmentation(false);

    connect(m_pendingOnsetsLayer, SIGNAL(frameIlluminated(sv_frame_t)),
            this, SIGNAL(alignmentFrameIlluminated(sv_frame_t)));

    m_partialAlignmentAudioStart = audioFrameStart;
    m_partialAlignmentAudioEnd = audioFrameEnd;
    
    m_awaitingOnsetsLayer = true;
}

void
Session::modelReady(ModelId id)
{
    SVDEBUG << "Session::modelReady: model is " << id << endl;

    if (m_pendingOnsetsLayer && id == m_pendingOnsetsLayer->getModel()) {
        m_awaitingOnsetsLayer = false;
    }

    if (!m_awaitingOnsetsLayer) {
        alignmentComplete();
    }
}

void
Session::alignmentComplete()
{
    SVDEBUG << "Session::alignmentComplete" << endl;

    if (m_tempoLayer) {
        m_bottomPane->addLayer(m_tempoLayer);
    }

    recalculateTempoLayer();
    
    emit alignmentReadyForReview();
}

void
Session::rejectAlignment()
{
    SVDEBUG << "Session::rejectAlignment" << endl;
    
    if (!m_pendingOnsetsLayer) {
        SVDEBUG << "Session::rejectAlignment: No alignment waiting to be rejected" << endl;
        return;
    }        
    
    m_document->deleteLayer(m_pendingOnsetsLayer, true);
    m_pendingOnsetsLayer = nullptr;

    if (m_acceptedOnsetsLayer) {
        m_topPane->addLayer(m_acceptedOnsetsLayer);
        m_displayedOnsetsLayer = m_acceptedOnsetsLayer;
        m_acceptedOnsetsLayer = nullptr;
    } else {
        m_displayedOnsetsLayer = nullptr;
    }

    recalculateTempoLayer();
    
    emit alignmentRejected();
}

void
Session::acceptAlignment()
{
    SVDEBUG << "Session::acceptAlignment" << endl;
    
    if (!m_pendingOnsetsLayer) {
        SVDEBUG << "Session::acceptAlignment: No alignment waiting to be accepted" << endl;
        return;
    }        
        
    if (m_acceptedOnsetsLayer) {
        mergeLayers(m_acceptedOnsetsLayer, m_pendingOnsetsLayer,
                    m_partialAlignmentAudioStart, m_partialAlignmentAudioEnd);
    }
    
    if (m_acceptedOnsetsLayer) {
        m_document->deleteLayer(m_acceptedOnsetsLayer, true);
        m_acceptedOnsetsLayer = nullptr;
    }
    m_displayedOnsetsLayer = m_pendingOnsetsLayer;
    m_pendingOnsetsLayer = nullptr;
    
    recalculateTempoLayer();
    
    emit alignmentAccepted();
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

bool
Session::exportAlignmentTo(QString path)
{
    if (!m_displayedOnsetsLayer) {
        SVDEBUG << "Session::exportAlignmentTo: Internal error: no onsets layer"
                << endl;
        return false;
    }

    if (QFileInfo(path).suffix() == "") {
        path += ".csv";
    }

    auto model = ModelById::get(m_displayedOnsetsLayer->getModel());
    if (!model) {
        SVDEBUG << "Session::exportAlignmentTo: Internal error: unknown model"
                << endl;
        return false;
    }

    CSVFileWriter writer(path,
                         model.get(),
                         nullptr,
                         ",",
                         DataExportIncludeHeader |
                         DataExportAlwaysIncludeTimestamp |
                         DataExportWriteTimeInFrames);

    writer.write();

    if (!writer.isOK()) {
        SVDEBUG << "Session::exportAlignmentTo: Failed to export alignment to "
                << path << ": error is: " << writer.getError() << endl;
        return false;
    }

    return true;
}

bool
Session::importAlignmentFrom(QString path)
{
    if (m_mainModel.isNone() || !m_displayedOnsetsLayer) {
        //!!! todo 1: create onsets layer here if it does not exist
        //!!! todo 2: calculate tempo layer as well
        return false;
    }

    auto mainModel = ModelById::get(m_mainModel);
    if (!mainModel) {
        SVDEBUG << "Session::importAlignmentFrom: No main model to refer to" << endl;
        return false;
    }
    
    auto existingModel = ModelById::getAs<SparseTimeValueModel>
        (m_displayedOnsetsLayer->getModel());
    if (!existingModel) {
        SVDEBUG << "Session::importAlignmentFrom: No existing model to import to - this is not yet implemented" << endl;
        return false;
    }
    
    CSVFormat format;
    
    format.setSeparator(QChar(','));
    format.setColumnCount(3);
    format.setHeaderStatus(CSVFormat::HeaderPresent);
    
    format.setModelType(CSVFormat::TwoDimensionalModel);
    format.setTimingType(CSVFormat::ExplicitTiming);
    format.setTimeUnits(CSVFormat::TimeAudioFrames);

    QList<CSVFormat::ColumnPurpose> purposes {
        CSVFormat::ColumnStartTime,
        CSVFormat::ColumnValue,
        CSVFormat::ColumnLabel
    };
    format.setColumnPurposes(purposes);
    
    CSVFileReader reader(path, format, mainModel->getSampleRate(), nullptr);

    if (!reader.isOK()) {
        SVDEBUG << "Session::importAlignmentFrom: Failed to construct CSV reader: " << reader.getError() << endl;
        return false;
    }

    Model *imported = reader.load();
    if (!imported) {
        SVDEBUG << "Session::importAlignmentFrom: Failed to import model from CSV file" << endl;
        return false;
    }

    auto stvm = qobject_cast<SparseTimeValueModel *>(imported);
    if (!stvm) {
        SVDEBUG << "Session::importAlignmentFrom: Imported model is of the wrong type" << endl;
        delete imported;
        return false;
    }

    EventVector oldEvents = existingModel->getAllEvents();
    EventVector newEvents = stvm->getAllEvents();
    
    for (auto e : oldEvents) {
        existingModel->remove(e);
    }
    for (auto e : newEvents) {
        existingModel->add(e);
    }

    delete imported;
    return true;
}

void
Session::recalculateTempoLayer()
{
    if (m_mainModel.isNone()) return;
    sv_samplerate_t sampleRate = ModelById::get(m_mainModel)->getSampleRate();
    auto newModel = make_shared<SparseTimeValueModel>(sampleRate, 1);
    auto newModelId = ModelById::add(newModel);
    m_document->addNonDerivedModel(newModelId);

    if (!m_tempoLayer) {
        m_tempoLayer = qobject_cast<TimeValueLayer *>
            (m_document->createLayer(LayerFactory::TimeValues));
        ColourDatabase *cdb = ColourDatabase::getInstance();
        m_tempoLayer->setBaseColour(cdb->getColourIndex(tr("Blue")));
        m_document->addLayerToView(m_bottomPane, m_tempoLayer);
    }

    if (!m_displayedOnsetsLayer) {
        m_document->setModel(m_tempoLayer, newModelId);
        return;
    }

    auto onsetsModel = ModelById::getAs<SparseTimeValueModel>
        (m_displayedOnsetsLayer->getModel());
    
    auto onsets = onsetsModel->getAllEvents();

    for (int i = 0; in_range_for(onsets, i + 1); ++i) {
        auto thisFrame = onsets[i].getFrame();
        auto nextFrame = onsets[i+1].getFrame();
        auto thisSec = RealTime::frame2RealTime(thisFrame, sampleRate).toDouble();
        auto nextSec = RealTime::frame2RealTime(nextFrame, sampleRate).toDouble();
        double tempo = 60. / (nextSec - thisSec);
        Event tempoEvent(thisFrame, float(tempo), QString());
        newModel->add(tempoEvent);
    }
    
    m_document->setModel(m_tempoLayer, newModelId);
}


