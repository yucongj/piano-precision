/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SCORE_FINDER_H
#define SV_SCORE_FINDER_H

#include <string>

class ScoreFinder
{
public:
    
    /** Return the full path of the directory in which all score
     *  directories are found. Return the path regardless of whether
     *  the directory actually exists yet or not. If the path cannot
     *  be determined because of some other error, such as a missing
     *  environment variable, return the empty string.
     */
    static std::string getScoreDirectory();

    /** Scan the score directory (see getScoreDirectory()) and return
     *  the names of all scores found there. The names are returned in
     *  no particular order.
     */
    static std::vector<std::string> getScoreNames();

    /** Look in the score directory (see getScoreDirectory()) for a
     *  score of the given name, and return true if it exists, false
     *  otherwise.
     */
    static bool scoreExists(std::string scoreName);
    
    /** Look in the score directory (see getScoreDirectory()) for a
     *  score of the given name, and return the score file of the
     *  given extension for that score, or an empty string if it is
     *  not found.
     *
     *  For example, if the score directory is "/path/to/scores", then
     *  getScoreFile("BothHandsC", "spos") returns
     *  "/path/to/scores/BothHandsC/BothHandsC.spos" if that file
     *  exists, or "" otherwise.
     *
     *  Note that this function may return an empty string even if the
     *  score exists, if it is incomplete and lacks a file of the
     *  required extension. To determine whether the score exists at
     *  all or is entirely absent, use scoreExists().
     */
    static std::string getScoreFile(std::string scoreName, std::string extension);
};


#endif
