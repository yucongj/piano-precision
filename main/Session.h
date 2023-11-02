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
#include "layer/TimeValueLayer.h"

#include "view/View.h"

class Session
{
public:


private:
    // I don't own any of these. The SV main window owns the document
    // and views; the document owns the layers and models
    Document *m_document;
    View *m_topView;
    View *m_bottomView;
    TimeRulerLayer *m_timeRulerLayer;
    WaveformLayer *m_waveformLayer;
    TimeValueLayer *m_onsetsLayer;
    TimeValueLayer *m_tempoLayer;
};

#endif
