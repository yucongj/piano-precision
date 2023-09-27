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
#include <QDir>

using std::string;
using std::vector;

string
ScoreFinder::getUserScoreDirectory()
{
    QString home = QDir::homePath();
    std::filesystem::path dir = home.toStdString() + "/Documents/PianoPrecision/Scores";
    if (!std::filesystem::exists(dir)) {
        SVDEBUG << "ScoreFinder::getUserScoreDirectory: Score directory "
                << dir << " does not exist, attempting to create it"
                << endl;
        if (std::filesystem::create_directories(dir)) {
            SVDEBUG << "ScoreFinder::getUserScoreDirectory: Succeeded" << endl;
        } else {
            SVDEBUG << "ScoreFinder::getUserScoreDirectory: Failed to create it" << endl;
            return {};
        }
    } else if (!std::filesystem::is_directory(dir)) {
        SVDEBUG << "ScoreFinder::getUserScoreDirectory: Location " << dir
                << " exists but is not a directory!"
                << endl;
        return {};
    }
    return dir.string();
}

static
string
getBundledDirectory(QString dirname)
{
    // We look in:
    // 
    // Mac: <mydir>/../Resources/<dirname>
    //
    // Linux: <mydir>/../share/application-name/<dirname>
    //
    // Other: <mydir>/<dirname>

    QString appName = QCoreApplication::applicationName();
    QString myDir = QCoreApplication::applicationDirPath();
    QString binaryName = QFileInfo(QCoreApplication::arguments().at(0))
        .fileName();

    QString qdir;
    
#if defined(Q_OS_MAC)
    qdir = myDir + "/../Resources/" + dirname;
#elif defined(Q_OS_LINUX)
    if (binaryName != "") {
        qdir = myDir + "/../share/" + binaryName + "/" + dirname;
    } else {
        qdir = myDir + "/../share/" + appName + "/" + dirname;
    }
#else
    qdir = myDir + "/" + dirname;
#endif
    
    string sdir(qdir.toUtf8().data());
    std::filesystem::path dir(sdir);

    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        SVDEBUG << "ScoreFinder::getBundledDirectory: Directory "
                << dir << " does not exist or is not a directory"
                << endl;
        return "";
    } else {
        SVDEBUG << "ScoreFinder::getBundledDirectory: Directory "
                << dir << " exists, returning it"
                << endl;
        return sdir;
    }
}

string
ScoreFinder::getBundledScoreDirectory()
{
    return getBundledDirectory("Scores");
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
ScoreFinder::getUserRecordingDirectory(string scoreName)
{
    QString home = QDir::homePath();
    std::filesystem::path dir = home.toStdString() + "/Documents/PianoPrecision/Recordings/" + scoreName;
    if (!std::filesystem::exists(dir)) {
        SVDEBUG << "ScoreFinder::getUserRecordingDirectory: Recording directory "
                << dir << " does not exist, attempting to create it"
                << endl;
        if (std::filesystem::create_directories(dir)) {
            SVDEBUG << "ScoreFinder::getUserRecordingDirectory: Succeeded" << endl;
        } else {
            SVDEBUG << "ScoreFinder::getUserRecordingDirectory: Failed to create it" << endl;
            return {};
        }
    } else if (!std::filesystem::is_directory(dir)) {
        SVDEBUG << "ScoreFinder::getUserRecordingDirectory: Location " << dir
                << " exists but is not a directory!"
                << endl;
        return {};
    }
    return dir.string();
}

string
ScoreFinder::getBundledRecordingDirectory(string scoreName)
{
    string rdir = getBundledDirectory("Recordings");
    if (rdir == "") {
        return rdir;
    }
    std::filesystem::path dir = rdir + "/" + scoreName;
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        SVDEBUG << "ScoreFinder::getBundledRecordingDirectory: Directory "
                << dir << " does not exist or is not a directory"
                << endl;
        return "";
    } else {
        SVDEBUG << "ScoreFinder::getBundledRecordingDirectory: Directory "
                << dir << " exists, returning it"
                << endl;
        return dir.string();
    }
}
