/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SCORE_ALIGNMENT_TRANSFORM_H
#define SV_SCORE_ALIGNMENT_TRANSFORM_H

#include "transform/TransformDescription.h"

#include <QMutex>

class ScoreAlignmentTransform
{
public:
    static sv::TransformList getAvailableAlignmentTransforms();
    static sv::TransformId getDefaultAlignmentTransform();

private:
    static QMutex m_mutex;
    static bool m_queried;
    static sv::TransformList m_transforms;
};

#endif
