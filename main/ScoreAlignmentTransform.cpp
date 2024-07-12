/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "ScoreAlignmentTransform.h"

#include "transform/TransformFactory.h"

#include <QMutexLocker>

using namespace sv;

const QString ALIGNMENT_OUTPUT_NAME = "audio-to-score-alignment";
const QString DEFAULT_PREFIX = "vamp:score-aligner:pianoaligner:";

bool
ScoreAlignmentTransform::m_queried = false;

TransformList
ScoreAlignmentTransform::m_transforms = {};

QMutex
ScoreAlignmentTransform::m_mutex = {};

TransformList
ScoreAlignmentTransform::getAvailableAlignmentTransforms()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_queried) {
        return m_transforms;
    }

    m_transforms.clear();

    auto allTransforms =
        TransformFactory::getInstance()->getInstalledTransformDescriptions();

    for (const auto &desc : allTransforms) {
        TransformId identifier = desc.identifier;
        Transform transform;
        transform.setIdentifier(identifier);
        SVDEBUG << "ScoreAlignmentTransform: looking at transform "
                << identifier << " with output \"" << transform.getOutput()
                << "\"" << endl;
        if (transform.getOutput() == ALIGNMENT_OUTPUT_NAME) {
            SVDEBUG << "ScoreAlignmentTransform: it's a candidate" << endl;
            m_transforms.push_back(desc);
        }
    }

    m_queried = true;
    return m_transforms;
}

TransformId
ScoreAlignmentTransform::getDefaultAlignmentTransform()
{
    // We return a TransformId rather than a filled-out Transform
    // because the latter depends on sample rate of input, which we
    // can't know at this point
    
    auto transforms = getAvailableAlignmentTransforms();
    if (transforms.empty()) {
        return {};
    } else {
        for (const auto &t : transforms) {
            if (t.identifier.startsWith(DEFAULT_PREFIX)) {
                SVDEBUG << "ScoreAlignmentTransform::getDefaultAlignmentTransform: Found \"" << t.identifier << "\" which starts with hardcoded default \"" << DEFAULT_PREFIX << "\", using this" << endl;
                return t.identifier;
            }
        }
        SVDEBUG << "ScoreAlignmentTransform::getDefaultAlignmentTransform: Found nothing to match hardcoded default prefix, returning first aligner in list which is \"" << transforms.begin()->identifier << "\"" << endl;
        return transforms.begin()->identifier;
    }
}
