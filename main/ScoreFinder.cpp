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
#include "system/System.h"

#include <filesystem>
#include <vector>
#include <set>

#include <QCoreApplication>
#include <QFileInfo>

#define USE_PIANO_ALIGNER_CODE_FOR_SCORE_PATH 1

using std::string;
using std::vector;

string
ScoreFinder::getUserScoreDirectory()
{
    string home;
    if (!getEnvUtf8("HOME", home)) {
        SVDEBUG << "ScoreFinder::getUserScoreDirectory: HOME environment variable is not set, can't proceed!" << endl;
        return {};
    }
    std::filesystem::path dir = home + "/Documents/PianoPrecision/Scores";
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

string
ScoreFinder::getBundledScoreDirectory()
{
    // We look in:
    // 
    // Mac: <mydir>/../Resources/Scores
    //
    // Linux: <mydir>/../share/application-name/Scores
    //
    // Other: <mydir>/Scores

    QString appName = QCoreApplication::applicationName();
    QString myDir = QCoreApplication::applicationDirPath();
    QString binaryName = QFileInfo(QCoreApplication::arguments().at(0))
        .fileName();

    QString qdir;
    
#if defined(Q_OS_MAC)
    qdir = myDir + "/../Resources/Scores";
#elif defined(Q_OS_LINUX)
    if (binaryName != "") {
        qdir = myDir + "/../share/" + binaryName + "/Scores";
    } else {
        qdir = myDir + "/../share/" + appName + "/Scores";
    }
#else
    qdir = myDir + "/Scores";
#endif
    
    string sdir(qdir.toUtf8().data());
    std::filesystem::path dir(sdir);

    if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
        return sdir;
    } else {
        return "";
    }
}

vector<string>
ScoreFinder::getScoreNames()
{
    vector<string> scoreDirs
        { getUserScoreDirectory(), getBundledScoreDirectory() };
    vector<string> names;
    
    for (auto scoreDir : scoreDirs) {
        
        if (scoreDir == "") continue;

        for (const auto& entry : std::filesystem::directory_iterator(scoreDir)) {

            string name = entry.path().filename().string();
            if (name.size() == 0 || name[0] == '.') continue;

            std::filesystem::path dir(entry.path());
            if (std::filesystem::exists(dir) &&
                std::filesystem::is_directory(dir)) {
                names.push_back(name);
            }
        }

        SVDEBUG << "ScoreFinder::getScoreNames: Found \"" << names.size()
                << "\" potential scores in " << scoreDir << endl;
    }

    return names;
}

string
ScoreFinder::getScoreFile(string scoreName, string extension)
{
    vector<string> scoreDirs
        { getUserScoreDirectory(), getBundledScoreDirectory() };
    vector<string> names;
    
    for (auto scoreDir : scoreDirs) {
        
        if (scoreDir == "") continue;

        // Inefficient
        
        for (const auto& entry : std::filesystem::directory_iterator(scoreDir)) {

            string name = entry.path().filename().string();
            if (name != scoreName) continue;

            std::filesystem::path filePath =
                entry.path().string() + "/" + scoreName + "." + extension;

            if (!std::filesystem::exists(filePath)) {
                SVDEBUG << "ScoreFinder::getScoreFile: Score file \""
                        << filePath << "\" does not exist" << endl;
                return {};
            } else {
                return filePath.string();
            }
        }
    }

    SVDEBUG << "ScoreFinder::getScoreFile: Score \""
            << scoreName << "\" not found" << endl;
    return {};
}

void
ScoreFinder::initialiseAlignerEnvironmentVariables()
{
    string userDir = getUserScoreDirectory();
    string bundledDir = getBundledScoreDirectory();

    string separator =
#ifdef Q_OS_WIN
        ";"
#else
        ":"
#endif
        ;

    string envPath = userDir + separator + bundledDir;

    putEnvUtf8("PIANO_ALIGNER_SCORE_PATH", envPath);

    SVDEBUG << "ScoreFinder::initialiseAlignerEnvironmentVariables: set "
            << "PIANO_ALIGNER_SCORE_PATH to " << envPath << endl;
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
