/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SCORE_POSITION_READER_H
#define SV_SCORE_POSITION_READER_H

#include "ScoreElement.h"

#include <QString>
#include <vector>

class ScorePositionReader
{
public:
    ScorePositionReader();
    ~ScorePositionReader();

    bool loadAScore(QString scoreName);

    ScoreElements getElements() const {
	return m_elements;
    }
    
private:
    QString m_scoreName;
    QString m_scoreFilename;

    bool loadScoreFile(QString filename);
	
    ScoreElements m_elements;
};

#endif
