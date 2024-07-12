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

#include "ScoreAlignmentTransform.h"

#include "transform/TransformFactory.h"
#include "transform/ModelTransformer.h"
#include "layer/ColourDatabase.h"
#include "layer/ColourMapper.h"

#include "data/fileio/CSVFormat.h"
#include "data/fileio/CSVFileReader.h"
#include "data/fileio/CSVFileWriter.h"

#include "base/TempWriteFile.h"
#include "base/StringBits.h"

#include <QMessageBox>
#include <QFileInfo>

using namespace std;
using namespace sv;

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

    if (m_pendingOnsetsLayer) {
        emit alignmentRejected();
    }
    
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
    m_inEditMode = false;

    resetAlignmentEntries();
}

void
Session::unsetDocument()
{
    setDocument(nullptr, nullptr, nullptr, nullptr);
}

TimeInstantLayer *
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
        if (m_mainModel.isNone()) {
            SVDEBUG << "Session::setMainModel: Cleared main model and no document set" << endl;
        } else {
            SVDEBUG << "Session::setMainModel: WARNING: No document; one should have been set first" << endl;
        }
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
    m_spectrogramLayer->setBinScale(BinScale::Linear);
    m_spectrogramLayer->setColourMap(ColourMapper::Green);
    m_spectrogramLayer->setColourScale(ColourScaleType::Log);
    m_spectrogramLayer->setColourScaleMultiple(2.0);

    m_document->addLayerToView(m_topPane, m_spectrogramLayer);
    m_document->setModel(m_spectrogramLayer, modelId);
}

void
Session::setAlignmentTransformId(TransformId alignmentTransformId)
{
    SVDEBUG << "Session::setAlignmentTransformId: Setting to \""
            << alignmentTransformId << "\"" << endl;
    m_alignmentTransformId = alignmentTransformId;
}

void
Session::beginAlignment()
{
    if (m_mainModel.isNone()) {
        SVDEBUG << "Session::beginAlignment: WARNING: No main model; one should have been set first" << endl;
        return;
    }

    beginPartialAlignment(-1, -1, -1, -1, -1, -1);
}

void
Session::beginPartialAlignment(int scorePositionStartNumerator,
                               int scorePositionStartDenominator,
                               int scorePositionEndNumerator,
                               int scorePositionEndDenominator,
                               sv_frame_t audioFrameStart,
                               sv_frame_t audioFrameEnd)
{
    if (m_mainModel.isNone()) {
        SVDEBUG << "Session::beginPartialAlignment: WARNING: No main model; one should have been set first" << endl;
        return;
    }

    ModelTransformer::Input input(m_mainModel);

    TransformId alignmentTransformId = m_alignmentTransformId;
    if (alignmentTransformId == "") {
        alignmentTransformId =
            ScoreAlignmentTransform::getDefaultAlignmentTransform();
    }

    if (alignmentTransformId == "") {
        SVDEBUG << "Session::beginPartialAlignment: ERROR: No alignment transform found" << endl;
        emit alignmentFailedToRun("No suitable score alignment plugin found");
        return;
    }
    
    vector<pair<QString, pair<Pane *, TimeInstantLayer **>>> layerDefinitions {
        { alignmentTransformId,
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
            << scorePositionStartNumerator << "/"
            << scorePositionStartDenominator
            << ", end = " << scorePositionEndNumerator << "/"
            << scorePositionEndDenominator
            << ", audio start = " << audioStart << ", end = "
            << audioEnd << endl;
    
    Transform::ParameterMap params {
        { "score-position-start-numerator", scorePositionStartNumerator },
        { "score-position-start-denominator", scorePositionStartDenominator },
        { "score-position-end-numerator", scorePositionEndNumerator },
        { "score-position-end-denominator", scorePositionEndDenominator },
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

        SVDEBUG << "Session::beginPartialAlignment: Setting plugin's program to \"" << m_scoreId << "\"" << endl;
            
        t.setProgram(m_scoreId);
        t.setParameters(params);

        Layer *layer = m_document->createDerivedLayer(t, input);
        if (!layer) {
            SVDEBUG << "Session::beginPartialAlignment: Transform failed to initialise" << endl;
            emit alignmentFailedToRun(QString("Unable to initialise score alignment plugin \"%1\"").arg(transformId));
            return;
        }
        if (layer->getModel().isNone()) {
            SVDEBUG << "Session::beginPartialAlignment: Transform failed to create a model" << endl;
            emit alignmentFailedToRun(QString("Score alignment plugin \"%1\" did not produce the expected output").arg(transformId));
            return;
        }

        TimeInstantLayer *tl = qobject_cast<TimeInstantLayer *>(layer);
        if (!tl) {
            SVDEBUG << "Session::beginPartialAlignment: Transform resulted in wrong layer type" << endl;
            emit alignmentFailedToRun(QString("Score alignment plugin \"%1\" did not produce the expected output format").arg(transformId));
            return;
        }

        if (*layerPtr) {
            m_document->deleteLayer(*layerPtr, true);
        }
            
        *layerPtr = tl;
        
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

    setOnsetsLayerProperties(m_pendingOnsetsLayer);

    m_partialAlignmentAudioStart = audioFrameStart;
    m_partialAlignmentAudioEnd = audioFrameEnd;
    
    m_awaitingOnsetsLayer = true;
}

void
Session::setOnsetsLayerProperties(TimeInstantLayer *onsetsLayer)
{
//    onsetsLayer->setPlotStyle(TimeInstantLayer::PlotSegmentation);
//    onsetsLayer->setFillSegments(false);

    connect(onsetsLayer, &TimeInstantLayer::frameIlluminated,
            this, &Session::alignmentFrameIlluminated);

    auto playParams = PlayParameterRepository::getInstance()->getPlayParameters
        (onsetsLayer->getModel().untyped);
    if (playParams) {
        playParams->setPlayGain(0.1);
    }
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
Session::modelChanged(ModelId id)
{
    SVDEBUG << "Session::modelChanged: model is " << id << endl;

    if (m_displayedOnsetsLayer && id == m_displayedOnsetsLayer->getModel()) {
        recalculateTempoLayer();
        emit alignmentModified();
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
    updateOnsetColours();
    
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
    updateOnsetColours();
    
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
        
    if (m_acceptedOnsetsLayer && m_partialAlignmentAudioEnd >= 0) {
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
    updateOnsetColours();
    
    emit alignmentAccepted();

    connect(ModelById::get(m_displayedOnsetsLayer->getModel()).get(),
            SIGNAL(modelChanged(ModelId)), this, SLOT(modelChanged(ModelId)));
}

void
Session::signifyEditMode()
{
    m_inEditMode = true;
    updateOnsetColours();
}

void
Session::signifyNavigateMode()
{
    m_inEditMode = false;
    updateOnsetColours();
}

void
Session::mergeLayers(TimeInstantLayer *from, TimeInstantLayer *to,
                     sv_frame_t overlapStart, sv_frame_t overlapEnd)
{
    // Currently the way we are handling this is by having "to"
    // contain *only* the new events, within overlapStart to
    // overlapEnd.  So the merge just copies all events outside that
    // range from "from" to "to". There are surely cleverer ways

    //!!! We should also use a command
    
    auto fromModel = ModelById::getAs<SparseOneDimensionalModel>(from->getModel());
    auto toModel = ModelById::getAs<SparseOneDimensionalModel>(to->getModel());

    EventVector beforeOverlap = fromModel->getEventsWithin(0, overlapStart);
    EventVector afterOverlap = fromModel->getEventsWithin
        (overlapEnd, fromModel->getEndFrame() - overlapEnd);

    for (auto e : beforeOverlap) toModel->add(e);
    for (auto e : afterOverlap) toModel->add(e);
}

bool
Session::exportAlignmentTo(QString path)
{
    if (QFileInfo(path).suffix() == "") {
        path += ".csv";
    }
    
    bool success = updateAlignmentEntries();
    if (success)    success = exportAlignmentEntriesTo(path);
    return success;
}

bool
Session::exportAlignmentEntriesTo(QString path)
{
    if (m_mainModel.isNone()) return false;
    sv_samplerate_t sampleRate = ModelById::get(m_mainModel)->getSampleRate();

    // Write to a temporary file and then move it into place at the
    // end, so as to avoid overwriting existing file if for any reason
    // the write fails
    
    TempWriteFile temp(path);
    QFile file(temp.getTemporaryFilename());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        SVCERR << "Session::exportAlignmentEntriesTo: Failed to open file "
               << temp.getTemporaryFilename() << " for writing" << endl;
        return false;
    }
    
    QTextStream out(&file);

    out << "LABEL,TIME,FRAME\n";
    
    for (const auto &entry : m_alignmentEntries) {
        QVector<QString> columns;
        columns << QString::fromStdString(entry.label);
        auto frame = entry.frame;
        if (frame < 0) {
            columns << "N" << "N";
        } else {
            columns << QString("%1").arg(RealTime::frame2RealTime
                                         (frame, sampleRate).toDouble())
                    << QString("%1").arg(frame);
        }
        out << StringBits::joinDelimited(columns, ",") << '\n';
    }

    file.close();
    temp.moveToTarget();
    return true;
}

bool
Session::importAlignmentFrom(QString path)
{
    SVDEBUG << "Session::importAlignmentFrom(" << path << ")" << endl;

    auto mainModel = ModelById::get(m_mainModel);
    if (!mainModel) {
        SVDEBUG << "Session::importAlignmentFrom: No main model, nothing for the alignment to be an alignment against" << endl;
        return false;
    }

    // We support two different CSV formats:
    //
    // * The one we export is is LABEL,TIME,FRAME where LABEL is text,
    // TIME is a number in seconds (not an integer), and FRAME is an
    // integer audio sample frame number. We use FRAME as the
    // authoritative timestamp. The TIME column was derived from FRAME
    // and should not be imported.
    //
    // * We also support a simpler two-column format LABEL,TIME where
    // LABEL is text, TIME is a number in seconds. Here we use TIME as
    // the timestamp and convert it back to frame ourselves.
    //
    // Either way we want to import to an onsets layer whose contents
    // are time instants indexed by audio sample frame, with a label
    // taken from LABEL.

    bool haveFrame = (CSVFormat(path).getColumnCount() > 2);
    
    CSVFormat format;
    
    format.setSeparator(QChar(','));
    format.setHeaderStatus(CSVFormat::HeaderPresent);
    format.setModelType(CSVFormat::OneDimensionalModel);
    format.setTimingType(CSVFormat::ExplicitTiming);

    if (haveFrame) {
        SVDEBUG << "Session::importAlignmentFrom: Have [at least] 3 columns, assuming we have label, [derived] time, and [authoritative] frame" << endl;
        format.setColumnCount(3);
        format.setTimeUnits(CSVFormat::TimeAudioFrames);
        QList<CSVFormat::ColumnPurpose> purposes {
            CSVFormat::ColumnLabel,    // LABEL
            CSVFormat::ColumnUnknown,  // TIME - Derived column, don't import
            CSVFormat::ColumnStartTime // FRAME
        };
        format.setColumnPurposes(purposes);
    } else {
        SVDEBUG << "Session::importAlignmentFrom: Have fewer than 3 columns, assuming we have label and time" << endl;
        format.setColumnCount(2);
        format.setTimeUnits(CSVFormat::TimeSeconds);
        QList<CSVFormat::ColumnPurpose> purposes {
            CSVFormat::ColumnLabel,    // LABEL
            CSVFormat::ColumnStartTime // TIME
        };
        format.setColumnPurposes(purposes);
    }
    
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

    auto stvm = qobject_cast<SparseOneDimensionalModel *>(imported);
    if (!stvm) {
        SVDEBUG << "Session::importAlignmentFrom: Imported model is of the wrong type" << endl;
        delete imported;
        return false;
    }

    if (!m_displayedOnsetsLayer) {
        m_displayedOnsetsLayer = dynamic_cast<TimeInstantLayer *>
            (m_document->createEmptyLayer(LayerFactory::TimeInstants));
        m_document->addLayerToView(m_topPane, m_displayedOnsetsLayer);
        setOnsetsLayerProperties(m_displayedOnsetsLayer);
    }
    
    auto existingModel = ModelById::getAs<SparseOneDimensionalModel>
        (m_displayedOnsetsLayer->getModel());
    if (!existingModel) {
        SVDEBUG << "Session::importAlignmentFrom: Internal error: onsets layer has no model!" << endl;
        delete imported;
        return false;
    }

    disconnect(existingModel.get(), SIGNAL(modelChanged(ModelId)),
               this, nullptr);
    
    EventVector oldEvents = existingModel->getAllEvents();
    EventVector newEvents = stvm->getAllEvents();
    
    for (auto e : oldEvents) {
        existingModel->remove(e);
    }
    for (auto e : newEvents) {
        existingModel->add(e);
    }

    delete imported;

    recalculateTempoLayer();
    updateOnsetColours();
    emit alignmentAccepted();    

    connect(existingModel.get(), SIGNAL(modelChanged(ModelId)),
            this, SLOT(modelChanged(ModelId)));

    return true;
}

void
Session::setMusicalEvents(const Score::MusicalEventList &musicalEvents)
{
    m_musicalEvents = musicalEvents;
    resetAlignmentEntries();
}

void
Session::resetAlignmentEntries()
{
    m_alignmentEntries.clear();
    // Calculating the mapping from score musical events to m_alignmentEntries
    for (auto &event : m_musicalEvents) {
        Score::MeasureInfo info = event.measureInfo;
        std::string label = info.toLabel();
        m_alignmentEntries.push_back(AlignmentEntry(label, -1)); // -1 is placeholder
    }
}

bool
Session::updateAlignmentEntries()
{
    if (m_displayedOnsetsLayer) {

        shared_ptr<SparseOneDimensionalModel> model =
                ModelById::getAs<SparseOneDimensionalModel>
            (m_displayedOnsetsLayer->getModel());
        
        if (model) {

            // Overwriting the frame values
            auto onsets = model->getAllEvents();
            for (auto onset : onsets) {
                // finding the alignment entry with the same label
                std::string target = onset.getLabel().toStdString();
                bool found = false;
                int i = 0;
                while (!found && i < int(m_alignmentEntries.size())) {
                    if (m_alignmentEntries[i].label == target) found = true;
                    else    i++;
                }
                if (!found) {
                    SVCERR<<"ERROR: In Session::updateAlignmentEntries, label "
                          << target << " not found!"<<endl;
                    return false;
                }
                m_alignmentEntries[i].frame = onset.getFrame();
            }
        }
    }

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

    updateAlignmentEntries();

    int start = -1, end = -2;
    bool stop = false;
    while (!stop && end <= int(m_alignmentEntries.size()) - 4) {
        start = end + 2;
        while (m_alignmentEntries[start].frame < 0) {
            start++;
            if (start >= int(m_alignmentEntries.size()) - 1) {
                stop = true; // no more aligned sections
                break;
            }
        }
        end = start;
        while (!stop && m_alignmentEntries[end+1].frame >= 0) {
            end++;
            if (end >= int(m_alignmentEntries.size()) - 1) {
                stop = true; // reached the last event
                break;
            }
        }
        for (int i = start; i < end; ++i) {
            auto thisFrame = m_alignmentEntries[i].frame;
            auto nextFrame = m_alignmentEntries[i+1].frame;
            auto thisSec = RealTime::frame2RealTime(thisFrame, sampleRate).toDouble();
            auto nextSec = RealTime::frame2RealTime(nextFrame, sampleRate).toDouble();
            Fraction dur = m_musicalEvents[i].duration;
            if (abs(nextSec - thisSec) > 0) {
                double tempo = (4. * dur.numerator / dur.denominator) * 60. / (nextSec - thisSec); // num of quarter notes per minutes
                Event tempoEvent(thisFrame, float(tempo), QString());
                newModel->add(tempoEvent);
            }
        }
    }
    
    m_document->setModel(m_tempoLayer, newModelId);
}

void
Session::updateOnsetColours()
{
    if (!m_displayedOnsetsLayer) {
        return;
    }
    
    QString colour;

    if (m_pendingOnsetsLayer) {
        colour = "Bright Red";
    } else if (m_inEditMode) {
        colour = "Orange";
    } else {
        colour = "Purple";
    }
    
    ColourDatabase *cdb = ColourDatabase::getInstance();
    m_displayedOnsetsLayer->setBaseColour(cdb->getColourIndex(colour));
}


