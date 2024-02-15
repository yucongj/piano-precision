/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SCORE_PARSER_H
#define SV_SCORE_PARSER_H

#include <string>

class ScoreParser
{
public:
    /** Generate necessary score files.
     */
    static bool generateScoreFiles(std::string scoreDir,
                                   std::string scoreName,
                                   std::string meiFile);

    /** Obtain the resource path to pass to Verovio. Resources are
     *  unpacked from the binary bundle the first time this is called,
     *  so the resulting resource path is local to this invocation of
     *  the program.
     */
    static std::string getResourcePath();
};

#endif
