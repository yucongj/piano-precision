/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "ScorePositionReader.h"
#include "ScoreFinder.h"

#include "base/Debug.h"

#include <QXmlStreamReader>
#include <QFile>

#include <map>

using std::map;

static std::ostream &operator<<(std::ostream &o, const QStringRef &r) {
    return o << r.toString();
}

ScorePositionReader::ScorePositionReader()
{
}

ScorePositionReader::~ScorePositionReader()
{
}

bool
ScorePositionReader::loadAScore(QString scoreName)
{
    m_scoreName = scoreName;

    SVDEBUG << "ScorePositionReader::loadAScore: Score \"" << scoreName
	    << "\" requested" << endl;

    std::string scorePath =
        ScoreFinder::getScoreFile(scoreName.toStdString(), "spos");
    if (scorePath == "") {
        std::cerr << "Score file (.spos) not found!" << '\n';
        return false;
    }
        
    QString filename = scorePath.c_str();
    SVDEBUG << "ScorePositionReader::loadAScore: Found file, calling "
	    << "loadScoreFile with filename \"" << filename << "\"" << endl;
    return loadScoreFile(filename);
}

bool
ScorePositionReader::loadScoreFile(QString filename)
{
    QFile file(filename);

    if (!file.open(QFile::ReadOnly)) {
	SVDEBUG << "ScorePositionReader::loadScoreFile: Failed to open file "
		<< filename << endl;
	return false;
    }
    
    QXmlStreamReader reader(&file);

    const map<QString, QString> transitions {
	// To, from
	{ "score", "document" },
	{ "elements", "score" },
	{ "element", "elements" },
	{ "events", "score" },
	{ "event", "events" },
    };

    QString state = "toplevel";

    map<int, ScoreElement> elements;
    
    while (!reader.atEnd()) {

	auto token = reader.readNext();

	switch (token) {

	case QXmlStreamReader::Invalid:
	    SVDEBUG << "ScorePositionReader::loadScoreFile: Error in XML: "
		    << reader.errorString() << endl;
	    return false;

	case QXmlStreamReader::StartDocument:
	{
	    if (state != "toplevel") {
		SVDEBUG << "ScorePositionReader::loadScoreFile: "
			<< "Unexpected StartDocument token when already in "
			<< "state [" << state << "]" << endl;
		return false;
	    }

	    state = "document";
	    break;
	}
	    
	case QXmlStreamReader::StartElement:
	{
	    QString e = reader.name().toString();
	    
	    if (transitions.find(e) == transitions.end()) {
		SVDEBUG << "ScorePositionReader::loadScoreFile: "
			<< "NOTE: Unknown element <" << e << ">, ignoring"
			<< endl;
		break;
	    } else if (state != transitions.at(e)) {
		SVDEBUG << "ScorePositionReader::loadScoreFile: "
			<< "Unexpected element <" << e << "> in state ["
			<< state << "]" << endl;
		return false;
	    }
	    
	    state = e;

	    if (state == "element") {
		auto attrs = reader.attributes();
		if (!attrs.hasAttribute("id") ||
		    !attrs.hasAttribute("x") ||
		    !attrs.hasAttribute("y") ||
		    !attrs.hasAttribute("page")) {
		    SVDEBUG << "ScorePositionReader::loadScoreFile: "
			    << "Element element lacks required attributes "
			    << "(one or more of id, x, y, page)" << endl;
		    return false;
		}
		bool ok = false;
		int id = attrs.value("id").toInt(&ok);
		if (!ok) {
		    SVDEBUG << "ScorePositionReader::loadScoreFile: "
			    << "Invalid id " << attrs.value("id") << endl;
		    return false;
		}
		double x = attrs.value("x").toDouble(&ok);
		if (!ok) {
		    SVDEBUG << "ScorePositionReader::loadScoreFile: "
			    << "Invalid x-coord " << attrs.value("x") << endl;
		    return false;
		}
		double y = attrs.value("y").toDouble(&ok);
		if (!ok) {
		    SVDEBUG << "ScorePositionReader::loadScoreFile: "
			    << "Invalid y-coord " << attrs.value("y") << endl;
		    return false;
		}
		double sy = attrs.value("sy").toDouble(&ok);
		if (!ok) {
		    SVDEBUG << "ScorePositionReader::loadScoreFile: "
			    << "Invalid sy value " << attrs.value("sy") << endl;
		    return false;
		}
		int page = attrs.value("page").toInt(&ok);
		if (!ok) {
		    SVDEBUG << "ScorePositionReader::loadScoreFile: "
			    << "Invalid page " << attrs.value("page") << endl;
		    return false;
		}
		elements[id].x = x;
		elements[id].y = y;
		elements[id].sy = sy;
		elements[id].page = page;

	    } else if (state == "event") {
		
		auto attrs = reader.attributes();
		if (!attrs.hasAttribute("elid") ||
		    !attrs.hasAttribute("position")) {
		    SVDEBUG << "ScorePositionReader::loadScoreFile: "
			    << "Event element lacks required attributes "
			    << "(one or more of elid, position)" << endl;
		    return false;
		}
		bool ok = false;
		int elid = attrs.value("elid").toInt(&ok);
		if (!ok) {
		    SVDEBUG << "ScorePositionReader::loadScoreFile: "
			    << "Invalid elid " << attrs.value("elid") << endl;
		    return false;
		}
		int position = attrs.value("position").toDouble(&ok);
		if (!ok) {
		    SVDEBUG << "ScorePositionReader::loadScoreFile: "
			    << "Invalid position " << attrs.value("position") << endl;
		    return false;
		}
		if (elements.find(elid) == elements.end()) {
		    SVDEBUG << "ScorePositionReader::loadScoreFile: "
			    << "Event refers to known element " << elid
			    << endl;
		    return false;
		}
		elements[elid].position = position;
	    }
	    break;
	}

	case QXmlStreamReader::EndElement:
	{
	    QString e = reader.name().toString();

	    if (e != state) {
		SVDEBUG << "ScorePositionReader::loadScoreFile: "
			<< "Note: leaving unknown or unexpected element " << e
			<< ", ignoring" << endl;
		break;
	    }

	    state = transitions.at(e);
	    break;
	}

	case QXmlStreamReader::Characters:
	    // disregard these without even mentioning the fact
	    break;
	
	default:
	    SVDEBUG << "ScorePositionReader::loadScoreFile: "
		    << "Note: disregarding token type "
		    << reader.tokenString() << endl;
	    break;
	}
    }

    for (auto e: elements) {
	m_elements.push_back(e.second);
    }

    SVDEBUG << "ScorePositionReader::loadScoreFile: Reached end, have read "
	    << m_elements.size() << " elements" << endl;
    
    return true;
}
