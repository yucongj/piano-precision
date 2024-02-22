/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "ScoreParser.h"

#include "verovio-replace/include/vrv/timemap.h"
#include "verovio-replace/include/vrv/toolkit.h"
#include "verovio/include/vrv/vrv.h"

#include "jsonxx.h"

#include "base/Debug.h"

#include <string>
#include <vector>

using std::string;
using std::vector;

#include <QDir>
#include <QTemporaryDir>
#include <QString>
#include <QStringList>

bool
ScoreParser::generateScoreFiles(string dir, string scoreName, string meiFile)
{
    vrv::Toolkit toolkit(false);

    string resourcePath = getResourcePath();
    if (resourcePath == "" || !toolkit.SetResourcePath(resourcePath)) {
        SVDEBUG << "ScoreParser::generateScoreFiles: Failed to set Verovio resource path" << endl;
        return false;
    }
    toolkit.LoadFile(meiFile);

    jsonxx::Array timemap;
    string option = "{\"includeMeasures\" : true,}";
    toolkit.RenderToTimemapFile(dir + "/" + scoreName + ".json", option);
    timemap.parse(toolkit.RenderToTimemap(option));

    std::vector<string> meters; // starting from measure 1
    for (int i = 0; i < int(timemap.size()); i++) {
        auto event = timemap.get<jsonxx::Object>(i);
        if (event.has<jsonxx::String>("meterSig")) {
            string meter = event.get<jsonxx::String>("meterSig");
            auto m = meter.find(" ");
            meters.push_back(meter.substr(m+1));
        }
    }
    // Writing to the .meter file
    vector<std::pair<int, string>> meterChanges;
    string outputString;
    for (int m = 0; m + 1 < int(meters.size()); m++) {
        if ((m == 0) || (meters.at(m) != meters.at(m-1))) {
            outputString += std::to_string(m+1) + "\t" + meters.at(m) + "\n";
            meterChanges.push_back(std::pair<int, std::string>(m+1, meters.at(m)));
        }
    }
    string outfile(dir + "/" + scoreName + ".meter");
    std::ofstream output(outfile);
    output << outputString;
    if (!output.good()) {
        SVDEBUG << "Failed to write meter data to " << outfile << endl;
        return false;
    }
    SVDEBUG << "Wrote meter data to " << outfile << endl;

    // Calculating cumulative fraction for the beginning of each measure
    vector<vrv::Fraction> cumulativeMeasureFraction;
    if (meters.size() > 0)  cumulativeMeasureFraction.push_back(vrv::Fraction(0, 1));
    for (int m = 1; m < int(meters.size()); m++) {
        cumulativeMeasureFraction.push_back(cumulativeMeasureFraction.back() + vrv::Fraction::fromString(meters.at(m-1)));
    }

    // Extracting individual notes
    vector<vrv::SoloNote> soloNotes;
    for (int i = 0; i < int(timemap.size()); i++) {
        auto event = timemap.get<jsonxx::Object>(i);
        if (event.has<jsonxx::Array>("on")) {
            auto onNotes = event.get<jsonxx::Array>("on");
            for (int n = 0; n < int(onNotes.size()); n++) {
                string id = onNotes.get<jsonxx::String>(n);
                jsonxx::Object times;
                times.parse(toolkit.GetTimesForElement(id));
                float tiedDur = times.get<jsonxx::Array>("scoreTimeTiedDuration").get<jsonxx::Number>(0);
                if (tiedDur == -1) continue; // skipping tied notes that are not leading notes

                vrv::SoloNote newNote;
                newNote.measureIndex = toolkit.GetMeasureIndexForNote(id);

                vrv::Fraction beat = toolkit.GetClosestFraction(times.get<jsonxx::Array>("scoreTimeOnset").get<jsonxx::Number>(0) / 4.);
                newNote.beat = beat;

                newNote.duration = tiedDur + times.get<jsonxx::Array>("scoreTimeDuration").get<jsonxx::Number>(0);
                newNote.duration = newNote.duration / 4.;

                jsonxx::Object note;
                note.parse(toolkit.GetMIDIValuesForElement(id));
                newNote.pitch = note.get<jsonxx::Number>("pitch");

                newNote.noteId = id;

                newNote.cumulative = cumulativeMeasureFraction.at(newNote.measureIndex-1) + newNote.beat;

                newNote.on = 1;

                soloNotes.push_back(newNote);
            }
        }
    }

    // Adding off notes to soloNotes
    vector<vrv::SoloNote> lines = soloNotes;
    for (const auto &note : soloNotes) {
        if (!note.on)   continue;
        vrv::Fraction end = note.cumulative + toolkit.GetClosestFraction(note.duration);
        int endMeasure;
        vrv::Fraction endBeat;
        int m = 0;
        while (m < int(meters.size()) && cumulativeMeasureFraction.at(m) < end)   m++;
        if (m < int(meters.size()) && end == cumulativeMeasureFraction.at(m)) {
            endMeasure = m+1;
            endBeat = vrv::Fraction(0, 1);
        } else {
            endMeasure = m;
            endBeat = end - cumulativeMeasureFraction.at(m-1);
        }
        vrv::SoloNote offNote = vrv::SoloNote(endMeasure, endBeat, end, note.duration, note.pitch, note.noteId, 0);
        lines.push_back(offNote);
    }

    std::sort(lines.begin(), lines.end(), [](vrv::SoloNote &a, vrv::SoloNote &b){
        if (a.cumulative < b.cumulative)    return true;
        if (b.cumulative < a.cumulative)    return false;
        if (a.on < b.on)    return true;
        if (b.on < a.on)    return false;
        if (a.pitch < b.pitch)  return true;
        if (b.pitch < a.pitch)  return false;
        return false; // not sure why "return trule" wouldn't also work here.
    });

    // Writing to the .solo file
    string content;
    for (const auto &line : lines) {
        content += std::to_string(line.measureIndex) + "+" + std::to_string(line.beat.numerator) + "/" + std::to_string(line.beat.denominator) + "\t";
        content += std::to_string(line.cumulative.numerator) + "/" + std::to_string(line.cumulative.denominator)  + "\t90\t";
        content += std::to_string(line.pitch) + "\t";
        if (line.on)    content += "80\t";
        else    content += "0\t";
        content += line.noteId + "\n";
    }
    outfile = dir + "/" + scoreName + ".solo";
    std::ofstream file(outfile);
    file << content;
    if (!file.good()) {
        SVDEBUG << "Failed to write solo data to " << outfile << endl;
        return false;
    }
    SVDEBUG << "Wrote solo data to " << outfile << endl;

    return true;
}

string
ScoreParser::getResourcePath()
{
    static string resourcePath;
    static std::unique_ptr<QTemporaryDir> tempDir;
    static std::once_flag f;

    std::call_once(f, [&]() {

        tempDir = std::make_unique<QTemporaryDir>();
        tempDir->setAutoRemove(true);
        bool success = true;

        QDir sourceRoot(":verovio/data/");
        QDir targetRoot(QDir(tempDir->path()).filePath("verovio"));
        QStringList names = sourceRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        names.push_back(".");

        for (auto name: names) {
            QDir sourceDir(sourceRoot.filePath(name));
            QDir targetDir(targetRoot.filePath(name));
            if (!QDir().mkpath(targetDir.path())) {
                SVDEBUG << "ScoreParser: Failed to create directory \""
                        << targetDir.path() << "\"" << endl;
                success = false;
                break;
            }
            SVDEBUG << "ScoreParser: scanning dir \"" << sourceDir.path()
                    << "\"..." << endl;
            for (auto f: sourceDir.entryInfoList(QDir::Files)) {
                QString sourcePath(f.filePath());
                QString targetPath(targetDir.filePath(f.fileName()));
                if (!QFile(sourcePath).copy(targetPath)) {
                    SVDEBUG << "ScoreParser: Failed to copy file from \""
                            << sourcePath << "\" to \"" << targetPath << "\""
                            << endl;
                    success = false;
                    break;
                }
            }
        }

        if (success) {
            resourcePath = targetRoot.canonicalPath().toStdString();
            SVDEBUG << "ScoreParser: Unbundled Verovio resources to \""
                    << resourcePath << "\"" << endl;
        }
    });

    return resourcePath;
}

