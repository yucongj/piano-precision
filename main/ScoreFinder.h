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
#include <vector>

class ScoreFinder
{
public:
    /** Return the full path of the directory in which user-installed
     *  score directories are found. If the directory does not exist
     *  yet, create it before returning its path. If creation fails or
     *  the path cannot be determined for any reason, return the empty
     *  string.
     */
    static std::string getUserScoreDirectory();

    /** Return the full path of the directory in which app-bundled
     *  score directories are found. If the directory does not exist
     *  or the path cannot be determined for any reason, return the
     *  empty string.
     */
    static std::string getBundledScoreDirectory();

    /** Scan the score directories (getUserScoreDirectory() and
     *  getBundledScoreDirectory()) and return the names of all scores
     *  found there. The names are returned in no particular order and
     *  may include duplicates if a score appears in both user and
     *  bundled locations.
     */
    static std::vector<std::string> getScoreNames();
    
    /** Look in the score directories (getUserScoreDirectory() and
     *  getBundledScoreDirectory()) for a score of the given name, and
     *  return the score file of the given extension for that score,
     *  or an empty string if it is not found.
     *
     *  For example, if a score directory is "/path/to/scores", then
     *  getScoreFile("BothHandsC", "spos") returns
     *  "/path/to/scores/BothHandsC/BothHandsC.spos" if that file
     *  exists, or "" otherwise.
     *
     *  Note that this function may return an empty string even if the
     *  score exists, if it is incomplete and lacks a file of the
     *  required extension. To determine whether the score exists at
     *  all or is entirely absent, use scoreExists().
     *
     *  Note that if a score of a given name appears in both user and
     *  bundled directories, the user directory takes priority.
     */
    static std::string getScoreFile(std::string scoreName, std::string extension);

    /** Set up the appropriate environment variables to cause the
     *  aligner plugin to look for scores in the user and bundled
     *  paths.
     */
    static void initialiseAlignerEnvironmentVariables();

    /** Return the full path of the directory in which recordings of
     *  the given score should be saved. If the directory does not
     *  exist yet, create it before returning its path. If creation
     *  fails or the path cannot be determined for any reason, return
     *  the empty string.
     */
    static std::string getUserRecordingDirectory(std::string scoreName);

    /** Return the full path of the directory in which app-bundled
     *  recordings of the given score may be found. If the directory
     *  does not exist or the path cannot be determined for any
     *  reason, return the empty string.
     */
    static std::string getBundledRecordingDirectory(std::string scoreName);

    /** Populate the user score and recording directories from bundled
     *  copies. Do not overwrite any existing files.
     */
    static void populateUserDirectoriesFromBundled();
};


#endif
