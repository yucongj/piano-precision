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

using namespace std;

Session::Session() :
    m_document(nullptr),
    m_topView(nullptr),
    m_bottomView(nullptr),
    m_timeRulerLayer(nullptr),
    m_waveformLayer(nullptr),
    m_onsetsLayer(nullptr),
    m_tempoLayer(nullptr)
{
    SVDEBUG << "Session::Session" << endl;
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
    m_onsetsLayer = nullptr;
    m_onsetsModel = {};
    m_tempoLayer = nullptr;
    m_tempoModel = {};
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
        SVDEBUG << "Session::setMainModel: Waveform layer already exists - currently we expect a process by which the document and panes are created and then setMainModel called here only once per document" << endl;
        return;
    }

    ColourDatabase *cdb = ColourDatabase::getInstance();
    
    m_document->addLayerToView(m_bottomView, m_timeRulerLayer);
    
    m_waveformLayer = qobject_cast<WaveformLayer *>
        (m_document->createLayer(LayerFactory::Waveform));
    m_waveformLayer->setBaseColour(cdb->getColourIndex(tr("Bright Blue")));
    
    m_document->addLayerToView(m_topView, m_waveformLayer);
    m_document->setModel(m_waveformLayer, modelId);

    ModelTransformer::Input input(modelId);

    vector<pair<QString, View *>> layerDefinitions {
        { "vamp:score-aligner:pianoaligner:chordonsets", m_topView },
        { "vamp:score-aligner:pianoaligner:eventtempo", m_bottomView }
    };

    vector<Layer *> newLayers;
    
    for (auto defn : layerDefinitions) {
        
        Transform t = TransformFactory::getInstance()->
            getDefaultTransformFor(defn.first);
        t.setProgram(scoreId);

        //!!! return error codes
    
        Layer *layer = m_document->createDerivedLayer(t, input);
        if (!layer) {
            SVDEBUG << "Session::setMainModel: Transform failed to initialise" << endl;
            return;
        }
        newLayers.push_back(layer);

        m_document->addLayerToView(defn.second, layer);

        Model *model = ModelById::get(layer->getModel()).get();
        if (!model) {
            SVDEBUG << "Session::setMainModel: Transform failed to create a model" << endl;
            return;
        }
                    
        connect(model, SIGNAL(ready(ModelId)), this, SLOT(modelReady(ModelId)));
    }
    
    m_onsetsLayer = qobject_cast<TimeValueLayer *>(newLayers[0]);
    m_tempoLayer = qobject_cast<TimeValueLayer *>(newLayers[1]);
    if (!m_onsetsLayer || !m_tempoLayer) {
        SVDEBUG << "Session::setMainModel: Layers are not of the expected type" << endl;
        return;
    }
    
    m_onsetsModel = m_onsetsLayer->getModel();
    m_tempoModel = m_tempoLayer->getModel();

    m_onsetsLayer->setPlotStyle(TimeValueLayer::PlotSegmentation);
    m_onsetsLayer->setDrawSegmentDivisions(true);
    m_onsetsLayer->setFillSegments(false);

    m_tempoLayer->setPlotStyle(TimeValueLayer::PlotLines);
    m_tempoLayer->setBaseColour(cdb->getColourIndex(tr("Blue")));
}

void
Session::modelReady(ModelId id)
{
    SVDEBUG << "Session::modelReady: model is " << id << endl;
}

