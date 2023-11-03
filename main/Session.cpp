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
    m_mainModel = {};
    m_topView = topView;
    m_bottomView = bottomView;
    m_timeRulerLayer = timeRuler;
    m_waveformLayer = nullptr;
    m_onsetsLayer = nullptr;
    m_tempoLayer = nullptr;
}

void
Session::unsetDocument()
{
    setDocument(nullptr, nullptr, nullptr, nullptr);
}

void
Session::setMainModel(ModelId modelId)
{
    SVDEBUG << "Session::setMainModel(" << modelId << ")" << endl;
    
    ModelId oldModel(m_mainModel);
    m_mainModel = modelId;

    if (!m_document) {
        SVDEBUG << "Session::setMainModel: WARNING: No document; one should have been set first" << endl;
        return;
    }
    
    if (!m_waveformLayer) {
        m_waveformLayer = qobject_cast<WaveformLayer *>
            (m_document->createLayer(LayerFactory::Waveform));
        m_document->addLayerToView(m_topView, m_waveformLayer);
    }

    m_document->setModel(m_waveformLayer, modelId);
}

