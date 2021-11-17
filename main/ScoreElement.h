/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SCORE_ELEMENT_H
#define SV_SCORE_ELEMENT_H

#include <vector>
#include <map>

struct ScoreElement {
    int id;
    double x;
    double y;
    double sy;
    int page;
    int position;
    
    ScoreElement() : id(0), x(0.0), y(0.0), sy(0.0), page(0), position(0) { }
};

typedef std::vector<ScoreElement> ScoreElements;

#endif
