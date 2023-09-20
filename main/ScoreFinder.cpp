/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "ScoreFinder.h"
#include "base/Debug.h"

#include <filesystem>
#include <vector>

using std::string;
using std::vector;

string
ScoreFinder::getScoreDirectory()
{
    const char *home = getenv("HOME");
    if (!home) {
        SVDEBUG << "ScoreFinder::getScoreDirectory: HOME environment variable is not set, can't proceed!" << endl;
        return {};
    }
    std::filesystem::path dir = string(home) + "/Documents/PianoPrecision/Scores";
    if (!std::filesystem::exists(dir)) {
        SVDEBUG << "ScoreFinder::getScoreDirectory: Score directory "
                << dir << " does not exist, attempting to create it"
                << endl;
        if (std::filesystem::create_directories(dir)) {
            SVDEBUG << "ScoreFinder::getScoreDirectory: Succeeded" << endl;
        } else {
            SVDEBUG << "ScoreFinder::getScoreDirectory: Failed to create it" << endl;
            return {};
        }
    } else if (!std::filesystem::is_directory(dir)) {
        SVDEBUG << "ScoreFinder::getScoreDirectory: Location " << dir
                << " exists but is not a directory!"
                << endl;
        return {};
    }
    return dir.string();
}

vector<string>
ScoreFinder::getScoreNames()
{
    auto scoreDir = getScoreDirectory();
    vector<string> names;
    if (scoreDir == "") {
        return names;
    }
    if (!std::filesystem::exists(scoreDir)) {
        SVDEBUG << "ScoreFinder::getScoreNames: Score directory \""
                << scoreDir << "\" does not exist" << endl;
        return names;
    }
    for (const auto& entry : std::filesystem::directory_iterator(scoreDir)) {
        string folderName = entry.path().filename().string();
        if (folderName[0] == '.') continue;
        names.push_back(folderName);
    }
    SVDEBUG << "ScoreFinder::getScoreNames: Found \"" << names.size()
            << "\" potential scores in " << scoreDir << endl;
    return names;
}

bool
ScoreFinder::scoreExists(string scoreName)
{
    auto scoreDir = getScoreDirectory();
    if (scoreDir == "") {
        return false;
    }
    if (!std::filesystem::exists(scoreDir)) {
        SVDEBUG << "ScoreFinder::scoreExists: Score directory \""
                << scoreDir << "\" does not exist" << endl;
        return false;
    }
    for (const auto& entry : std::filesystem::directory_iterator(scoreDir)) {
        string folderName = entry.path().filename().string();
        if (folderName[0] == '.') continue;
        if (folderName == scoreName) return true;
    }
    return false;
}

string
ScoreFinder::getScoreFile(string scoreName, string extension)
{
    string scoreDir = getScoreDirectory();
    if (scoreDir == "") {
        return {};
    }
    if (!std::filesystem::exists(scoreDir)) {
        SVDEBUG << "ScoreFinder::getScoreFile: Score directory \""
                << scoreDir << "\" does not exist" << endl;
        return {};
    }
    
    std::filesystem::path scorePath =
        scoreDir + "/" + scoreName + "/" + scoreName + "." + extension;

    if (!std::filesystem::exists(scorePath)) {
        SVDEBUG << "ScoreFinder::getScoreFile: Score file \"" << scorePath
                << "\" does not exist" << endl;
        return {};
    } else {
        return scorePath.string();
    }
}

string
ScoreFinder::getRecordingDirectory(string scoreName)
{
    const char *home = getenv("HOME");
    if (!home) {
        SVDEBUG << "ScoreFinder::getRecordingDirectory: HOME environment variable is not set, can't proceed!" << endl;
        return {};
    }
    std::filesystem::path dir = string(home) + "/Documents/PianoPrecision/Recordings/" + scoreName;
    if (!std::filesystem::exists(dir)) {
        SVDEBUG << "ScoreFinder::getRecordingDirectory: Recording directory "
                << dir << " does not exist, attempting to create it"
                << endl;
        if (std::filesystem::create_directories(dir)) {
            SVDEBUG << "ScoreFinder::getRecordingDirectory: Succeeded" << endl;
        } else {
            SVDEBUG << "ScoreFinder::getRecordingDirectory: Failed to create it" << endl;
            return {};
        }
    } else if (!std::filesystem::is_directory(dir)) {
        SVDEBUG << "ScoreFinder::getRecordingDirectory: Location " << dir
                << " exists but is not a directory!"
                << endl;
        return {};
    }
    return dir.string();
}
