/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "ScoreWidget.h"
#include "ScoreFinder.h"
#include "ScoreParser.h"

#include <QPainter>
#include <QMouseEvent>
#include <QSvgRenderer>
#include <QDomDocument>
#include <QDomElement>

#include "base/Debug.h"

#include <vector>

#include "verovio/include/vrv/toolkit.h"
#include "verovio/include/vrv/vrv.h"

#include "vrvtrim.h"

#define DEBUG_SCORE_WIDGET 1
//#define DEBUG_EVENT_FINDING 1

static QColor navigateHighlightColour("#59c4df");
static QColor editHighlightColour("#ffbd00");
static QColor selectHighlightColour(150, 150, 255);

using std::vector;
using std::pair;
using std::string;
using std::shared_ptr;
using std::make_shared;

ScoreWidget::ScoreWidget(QWidget *parent) :
    QFrame(parent),
    m_page(-1),
    m_mode(InteractionMode::None),
    m_mouseActive(false)
{
    setFrameStyle(Panel | Plain);
    setMinimumSize(QSize(100, 100));
    setMouseTracking(true);
    m_verovioResourcePath = ScoreParser::getResourcePath();
}

ScoreWidget::~ScoreWidget()
{

}

QString
ScoreWidget::getCurrentScore() const
{
    return m_scoreName;
}

int
ScoreWidget::getCurrentPage() const
{
    return m_page;
}

int
ScoreWidget::getPageCount() const
{
    return m_svgPages.size();
}

void
ScoreWidget::loadAScore(QString scoreName)
{
    QString errorString;
    if (!loadAScore(scoreName, errorString)) {
        emit loadFailed(scoreName, tr("Failed to load score %1: %2")
                        .arg(scoreName).arg(errorString));
        return;
    }

    clearSelection();
}

bool
ScoreWidget::loadAScore(QString scoreName, QString &errorString)
{
    SVDEBUG << "ScoreWidget::loadAScore: Score \"" << scoreName
            << "\" requested" << endl;

    if (m_verovioResourcePath == "") {
        SVDEBUG << "ScoreWidget::loadAScore: No Verovio resource path available" << endl;
        return false;
    }
    
    clearSelection();

    m_svgPages.clear();
    m_noteSystemExtentMap.clear();
    
    m_page = -1;
    
    string scorePath =
        ScoreFinder::getScoreFile(scoreName.toStdString(), "mei");
    if (scorePath == "") {
        errorString = "Score file (.mei) not found!";
        SVDEBUG << "ScoreWidget::loadAScore: " << errorString << endl;
        return false;
    }
    
    SVDEBUG << "ScoreWidget::loadAScore: Asked to load MEI file \""
            << scorePath << "\" for score \"" << scoreName << "\"" << endl;

    vrv::Toolkit toolkit(false);
    if (!toolkit.SetResourcePath(m_verovioResourcePath)) {
        SVDEBUG << "ScoreWidget::loadAScore: Failed to set Verovio resource path" << endl;
        return false;
    }
    if (!toolkit.LoadFile(scorePath)) {
        SVDEBUG << "ScoreWidget::loadAScore: Load failed in Verovio toolkit" << endl;
        return false;
    }

    int pp = toolkit.GetPageCount();
    for (int p = 0; p < pp; ++p) {

        std::string svgText = toolkit.RenderToSVG(p + 1); // (verovio is 1-based)

        // Verovio generates SVG 1.1, this transforms its output to
        // SVG 1.2 Tiny required by Qt
        svgText = VrvTrim::transformSvgToTiny(svgText);

        QByteArray svgData = QByteArray::fromStdString(svgText);
        
        shared_ptr<QSvgRenderer> renderer = make_shared<QSvgRenderer>(svgData);
        renderer->setAspectRatioMode(Qt::KeepAspectRatio);

        SVDEBUG << "ScoreWidget::showPage: created renderer from "
                << svgData.size() << "-byte SVG data" << endl;

        m_svgPages.push_back(renderer);

        findSystemExtents(svgData, renderer);
    }
    
    m_scoreName = scoreName;
    m_scoreFilename = QString::fromStdString(scorePath);

    SVDEBUG << "ScoreWidget::loadAScore: Load successful, showing first page"
            << endl;
    showPage(0);
    return true;
}

void
ScoreWidget::findSystemExtents(QByteArray svgData, shared_ptr<QSvgRenderer> renderer)
{
    // Study the system dimensions in order to calculate proper
    // highlight positions, and add them to m_noteSystemExtentMap.

    // We are now parsing the SVG XML in three different ways! But I
    // still don't think it's a significant overhead
    
    QDomDocument doc;
    doc.setContent(svgData, false);
    auto groups = doc.elementsByTagName("g");

    for (int i = 0; i < groups.size(); ++i) {

        auto systemElt = groups.at(i).toElement();
        if (!systemElt.attribute("class").split(" ", Qt::SkipEmptyParts)
            .contains("system")) {
            continue;
        }

        auto systemId = systemElt.attribute("id");
        
        Extent extent;
        bool haveExtent = false;

        auto children = systemElt.childNodes();
        
        for (int j = 0; j < children.size(); ++j) {
            
            auto childElt = children.at(j).toElement();
            if (childElt.tagName() == "path") {
                QStringList dparts = childElt.attribute("d")
                    .split(" ", Qt::SkipEmptyParts);
                if (dparts.size() == 4) {
                    bool ok1 = false, ok3 = false;
                    extent = { dparts[1].toDouble(&ok1), dparts[3].toDouble(&ok3) };
                    if (ok1 && ok3) {
#ifdef DEBUG_SCORE_WIDGET
                        SVDEBUG << "Found possible extent for system with id \""
                                << systemId << "\": from "
                                << extent.first << " -> " << extent.second << endl;
#endif
                        QRectF mapped = renderer->transformForElement(systemId)
                            .mapRect(QRectF(0, extent.first,
                                            1, extent.second - extent.first));
                        extent.first = mapped.y();
                        extent.second = extent.first + mapped.height();
#ifdef DEBUG_SCORE_WIDGET
                        SVDEBUG << "Mapped extent through element transform into "
                                << extent.first << " -> " << extent.second << endl;
#endif
                        haveExtent = true;
                        break;
                    }
                }
            }
        }

        if (haveExtent) {
            assignExtentToNotesBelow(systemElt, extent);
        } else {
            SVDEBUG << "Failed to find extent for system with id \""
                    << systemElt.attribute("id") << "\"" << endl;
        }
    }
}

void
ScoreWidget::assignExtentToNotesBelow(const QDomElement &systemElt, Extent extent)
{
    // We're querying all group elements (of which there are many) in
    // the doc as a whole, and then doing so here again for each
    // system element. That's quadratic complexity but with only a
    // small number of systems expected, I think it will be ok.
    
    auto groups = systemElt.elementsByTagName("g");

    for (int i = 0; i < groups.size(); ++i) {

        auto noteElt = groups.at(i).toElement();
        if (!noteElt.attribute("class").split(" ", Qt::SkipEmptyParts)
            .contains("note")) {
            assignExtentToNotesBelow(noteElt, extent);
            continue;
        }

        auto noteId = noteElt.attribute("id");
        if (noteId == "") {
            continue;
        }

#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "Assigning system extent " << extent.first
                << " -> " << extent.second
                << " to note with id \"" << noteId << "\"" << endl;
#endif
        m_noteSystemExtentMap[noteId] = extent;
    }
}

void
ScoreWidget::setMusicalEvents(const Score::MusicalEventList &events)
{
    m_musicalEvents = events;

    SVDEBUG << "ScoreWidget::setMusicalEvents: " << events.size()
            << " events" << endl;

    m_idDataMap.clear();
    m_labelIdMap.clear();
    m_pageEventsMap.clear();
    
    if (m_svgPages.empty()) {
        SVDEBUG << "ScoreWidget::setMusicalEvents: WARNING: No SVG pages, score should have been set before this" << endl;
        return;
    }
    
    int p = 0;
    int npages = m_svgPages.size();
    int ix = 0;
    
    for (const auto &ev : m_musicalEvents) {
        for (const auto &n : ev.notes) {
            if (!n.isNewNote)    continue;
            EventId id = QString::fromStdString(n.noteId);
            if (id == "") {
                SVDEBUG << "ScoreWidget::setMusicalEvents: NOTE: found note with no id" << endl;
                continue;
            }
            if (p + 1 < npages &&
                !m_svgPages[p]->elementExists(id) &&
                m_svgPages[p + 1]->elementExists(id)) {
                ++p;
            }

            if (m_svgPages[p]->elementExists(id)) {

                QRectF rect = m_svgPages[p]->boundsOnElement(id); 
                rect = m_svgPages[p]->transformForElement(id).mapRect(rect);
#ifdef DEBUG_SCORE_WIDGET
                SVDEBUG << "id " << id << " -> page " << p << ", rect "
                        << rect.x() << "," << rect.y() << " " << rect.width()
                        << "x" << rect.height() << endl;
#endif

                EventData data;
                data.id = id;
                data.page = p;
                data.boxOnPage = rect;
                data.location = ev.measureInfo.measureFraction;
                data.label = ev.measureInfo.toLabel();
                data.indexInEvents = ix;
                m_idDataMap[id] = data;
                m_pageEventsMap[p].push_back(id);
                m_labelIdMap[data.label] = id;
            }
        }
        ++ix;
    }

    SVDEBUG << "ScoreWidget::setMusicalEvents: Done" << endl;
}

void
ScoreWidget::resizeEvent(QResizeEvent *)
{
    if (m_page >= 0) {
        showPage(m_page);
    }
}

void
ScoreWidget::enterEvent(QEnterEvent *)
{
    m_mouseActive = true;
    update();
}

void
ScoreWidget::leaveEvent(QEvent *)
{
    if (m_mouseActive) {
        emit interactionEnded(m_mode);
    }
    m_mouseActive = false;
    update();
}

void
ScoreWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_mouseActive) return;

    m_eventUnderMouse = getEventAtPoint(e->pos());

    SVDEBUG << "ScoreWidget::mouseMoveEvent: id under mouse = "
            << m_eventUnderMouse.id << endl;
    
    update();

    if (!m_eventUnderMouse.isNull()) {
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidget::mouseMoveEvent: Emitting scorePositionHighlighted at " << m_eventUnderMouse.location << endl;
#endif
        emit scoreLocationHighlighted(m_eventUnderMouse.location,
                                      m_eventUnderMouse.label,
                                      m_mode);
    }
}

void
ScoreWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        return;
    }
    
    mouseMoveEvent(e);

    if (!m_musicalEvents.empty() && !m_eventUnderMouse.isNull() &&
        (m_mode == InteractionMode::SelectStart ||
         m_mode == InteractionMode::SelectEnd)) {

        if (m_mode == InteractionMode::SelectStart) {
            m_selectStart = m_eventUnderMouse;
            if (!(m_selectStart.location < m_selectEnd.location)) {
                m_selectEnd = {};
            }
        } else {
            m_selectEnd = m_eventUnderMouse;
            if (!(m_selectStart.location < m_selectEnd.location)) {
                m_selectStart = {};
            }
        }
        
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidget::mousePressEvent: Set select start to "
                << m_selectStart.location << " and end to "
                << m_selectEnd.location << endl;
#endif

        auto start = m_selectStart;
        auto end = m_selectEnd;
        if (start.isNull()) start = getScoreStartEvent();
        if (end.isNull()) end = getScoreEndEvent();
        emit selectionChanged(start.location,
                              isSelectedFromStart(),
                              start.label,
                              end.location,
                              isSelectedToEnd(),
                              end.label);
        update();
    }

    if (!m_eventUnderMouse.isNull()) {
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidget::mousePressEvent: Emitting scorePositionActivated at " << m_eventUnderMouse.location << endl;
#endif
        emit scoreLocationActivated(m_eventUnderMouse.location,
                                    m_eventUnderMouse.label,
                                    m_mode);
        update();
    }
}

void
ScoreWidget::clearSelection()
{
#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidget::clearSelection" << endl;
#endif

    if (m_selectStart.isNull() && m_selectEnd.isNull()) {
        return;
    }

    m_selectStart = {};
    m_selectEnd = {};

    emit selectionChanged(m_selectStart.location,
                          true,
                          getScoreStartEvent().label,
                          m_selectEnd.location,
                          true,
                          getScoreEndEvent().label);

    update();
}

ScoreWidget::EventData
ScoreWidget::getScoreStartEvent() const
{
    if (m_musicalEvents.empty()) return {};
    return getEventForMusicalEvent(*m_musicalEvents.begin());
}

bool
ScoreWidget::isSelectedFromStart() const
{
    return (m_musicalEvents.empty() ||
            m_selectStart.isNull() ||
            m_selectStart.indexInEvents == 0);
}

ScoreWidget::EventData
ScoreWidget::getScoreEndEvent() const
{
    if (m_musicalEvents.empty()) return {};
    return getEventForMusicalEvent(*m_musicalEvents.rbegin());
}

ScoreWidget::EventData
ScoreWidget::getEventForMusicalEvent(const Score::MusicalEvent &mev) const
{
    if (mev.notes.empty()) return {};
    return getEventWithId(mev.notes.begin()->noteId);
}

ScoreWidget::EventData
ScoreWidget::getEventWithId(EventId id) const
{
    auto itr = m_idDataMap.find(id);
    if (itr == m_idDataMap.end()) {
        return {};
    }
    return itr->second;
}

ScoreWidget::EventData
ScoreWidget::getEventWithId(const std::string &id) const
{
    return getEventWithId(QString::fromStdString(id));
}

ScoreWidget::EventData
ScoreWidget::getEventWithLabel(EventLabel label) const
{
    if (m_labelIdMap.find(label) == m_labelIdMap.end()) return {};
    return getEventWithId(m_labelIdMap.at(label));
}

bool
ScoreWidget::isSelectedToEnd() const
{
    return (m_musicalEvents.empty() ||
            m_selectEnd.isNull() ||
            m_selectEnd.indexInEvents + 1 >= int(m_musicalEvents.size()));
}

bool
ScoreWidget::isSelectedAll() const
{
    return isSelectedFromStart() && isSelectedToEnd();
}

void
ScoreWidget::getSelection(Fraction &start, EventLabel &startLabel,
                          Fraction &end, EventLabel &endLabel) const
{
    start = m_selectStart.location;
    startLabel = m_selectStart.label;
    end = m_selectEnd.location;
    endLabel = m_selectEnd.label;
}

void
ScoreWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        return;
    }

#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidget::mouseDoubleClickEvent" << endl;
#endif
        
    if (m_mode == InteractionMode::Navigate) {
        setInteractionMode(InteractionMode::Edit);
    }

    mousePressEvent(e);
}

ScoreWidget::EventData
ScoreWidget::getEventAtPoint(QPoint point)
{
    const auto &events = m_pageEventsMap[m_page];
    
    double px = point.x();
    double py = point.y();

    EventData found;
    double foundX = 0.0;

#ifdef DEBUG_EVENT_FINDING
    SVDEBUG << "ScoreWidget::getEventAtPoint: point " << px << "," << py << endl;
#endif
    
    for (auto itr = events.begin(); itr != events.end(); ++itr) {

        EventId id = *itr;
        EventData edata = getEventWithId(id);
        if (edata.isNull()) continue;
        
        QRectF r = getHighlightRectFor(edata);
        if (r == QRectF()) continue;

#ifdef DEBUG_EVENT_FINDING
        SVDEBUG << "ScoreWidget::getEventAtPoint: id " << *itr
                << " has rect " << r.x() << "," << r.y() << " "
                << r.width() << "x" << r.height() << " (seeking " << px
                << "," << py << ")" << endl;
#endif
    
        if (py < r.y() || py > r.y() + r.height()) {
            continue;
        }
        if (px < r.x() || r.x() < foundX) {
            continue;
        }
        
        found = edata;
        foundX = r.x();
    }

#ifdef DEBUG_EVENT_FINDING
    SVDEBUG << "ScoreWidget::idAtPoint: point " << point.x()
            << "," << point.y() << " -> element id " << found.id << endl;
#endif
    
    return found;
}

QRectF
ScoreWidget::getHighlightRectFor(const EventData &event)
{
    QRectF rect = event.boxOnPage;

    if (m_noteSystemExtentMap.find(event.id) != m_noteSystemExtentMap.end()) {
        Extent extent = m_noteSystemExtentMap.at(event.id);
        rect = QRectF(rect.x(), extent.first,
                      rect.width(), extent.second - extent.first);
    }
                      
    return m_pageToWidget.mapRect(rect);
}

void
ScoreWidget::paintEvent(QPaintEvent *e)
{
    QFrame::paintEvent(e);

    if (m_page < 0 || m_page >= getPageCount()) {
        SVDEBUG << "ScoreWidget::paintEvent: No page or page out of range, painting nothing" << endl;
        return;
    }

    QPainter paint(this);

    auto renderer = m_svgPages[m_page];

    // When we actually paint the SVG, we just tell Qt to stick it on
    // the paint device scaled while preserving aspect. But we still
    // need to do the same calculations ourselves to construct the
    // transforms needed for mapping to e.g. mouse interaction space
    
    QSizeF widgetSize = size();
    QSizeF pageSize = renderer->viewBoxF().size();

    double ww = widgetSize.width(), wh = widgetSize.height();
    double pw = pageSize.width(), ph = pageSize.height();

    SVDEBUG << "ScoreWidget::paint: widget size " << ww << "x" << wh
            << ", page size " << pw << "x" << ph << endl;
    
    if (!ww || !wh || !pw || !ph) {
        SVDEBUG << "ScoreWidgetPDF::paint: one of our dimensions is zero, can't proceed" << endl;
        return;
    }
    
    double scale = std::min(ww / pw, wh / ph);
    double xorigin = (ww - (pw * scale)) / 2.0;
    double yorigin = (wh - (ph * scale)) / 2.0;

    m_pageToWidget = QTransform();
    m_pageToWidget.translate(xorigin, yorigin);
    m_pageToWidget.scale(scale, scale);

    m_widgetToPage = QTransform();
    m_widgetToPage.scale(1.0 / scale, 1.0 / scale);
    m_widgetToPage.translate(-xorigin, -yorigin);
    
    // Show a highlight bar if the interaction mode is anything other
    // than None - the colour and location depend on the mode
    
    if (m_mode != InteractionMode::None) {

        EventData event;

        if (m_mouseActive) {
            event = m_eventUnderMouse;
            SVDEBUG << "ScoreWidget::paint: under mouse = "
                    << event.label << endl;
        } else {
            event = m_eventToHighlight;
            SVDEBUG << "ScoreWidget::paint: to highlight = "
                    << event.label << endl;
        }

        if (!event.isNull()) {
        
            QRectF rect = getHighlightRectFor(event);

            QColor highlightColour;

            switch (m_mode) {
            case InteractionMode::Navigate:
                highlightColour = navigateHighlightColour;
                break;
            case InteractionMode::Edit:
                highlightColour = editHighlightColour;
                break;
            case InteractionMode::SelectStart:
            case InteractionMode::SelectEnd:
                highlightColour = selectHighlightColour.darker();
                break;
            default: // None already handled in conditional above
                throw std::runtime_error("Unhandled case in mode switch");
            }
            
            highlightColour.setAlpha(160);
            paint.setPen(Qt::NoPen);
            paint.setBrush(highlightColour);
            
#ifdef DEBUG_SCORE_WIDGET
            SVDEBUG << "ScoreWidget::paint: highlighting rect with origin "
                    << rect.x() << "," << rect.y() << " and size "
                    << rect.width() << "x" << rect.height()
                    << " using colour " << highlightColour.name() << endl;
#endif

            if (rect != QRectF()) {
                paint.drawRect(rect);
            }
        }
    }

    // Highlight the current selection if there is one
    if (!m_musicalEvents.empty() &&
        (!isSelectedAll() ||
         (m_mode == InteractionMode::SelectStart ||
          m_mode == InteractionMode::SelectEnd))) {

        QColor fillColour = selectHighlightColour;
        fillColour.setAlpha(100);
        paint.setPen(Qt::NoPen);
        paint.setBrush(fillColour);

        auto comparator = [](const Score::MusicalEvent &e, const Fraction &f) {
            return e.measureInfo.measureFraction < f;
        };
        
        Score::MusicalEventList::iterator i0 = m_musicalEvents.begin();
        if (!m_selectStart.isNull()) {
            i0 = lower_bound(m_musicalEvents.begin(), m_musicalEvents.end(),
                             m_selectStart.location, comparator);
        }
        Score::MusicalEventList::iterator i1 = m_musicalEvents.end();
        if (!m_selectEnd.isNull()) {
            i1 = lower_bound(m_musicalEvents.begin(), m_musicalEvents.end(),
                             m_selectEnd.location, comparator);
        }

        SVDEBUG << "ScoreWidget::paint: selection spans from "
                << m_selectStart.location << " to " << m_selectEnd.location
                << " giving us iterators at "
                << (i0 == m_musicalEvents.end() ? "(end)" :
                    i0->notes.empty() ? "(location without note)" :
                    i0->notes[0].noteId)
                << " to " 
                << (i1 == m_musicalEvents.end() ? "(end)" :
                    i1->notes.empty() ? "(location without note)" :
                    i1->notes[0].noteId)
                << endl;

        double lineOrigin = m_pageToWidget.map(QPointF(0, 0)).x();
        double lineWidth = m_pageToWidget.map(QPointF(pageSize.width(), 0)).x();
        
        double prevY = -1.0;
        for (auto i = i0; i != i1 && i != m_musicalEvents.end(); ++i) {
            EventData data = getEventForMusicalEvent(*i);
            if (data.page < m_page) {
                continue;
            }
            if (data.page > m_page) {
                break;
            }
            QRectF rect = getHighlightRectFor(data);
            if (rect == QRectF()) {
                continue;
            }
            if (i == i0) {
                prevY = rect.y();
            }
            if (rect.y() > prevY) {
                rect.setX(lineOrigin);
            }
            rect.setWidth(lineWidth - rect.x());
            auto j = i;
            ++j;
            if (j != m_musicalEvents.end()) {
                EventData nextData = getEventForMusicalEvent(*j);
                QRectF nextRect = getHighlightRectFor(nextData);
                if (nextData.page == m_page && nextRect.y() <= rect.y()) {
                    rect.setWidth(nextRect.x() - rect.x());
                }
            }
            paint.drawRect(rect);
            prevY = rect.y();
        }
    }

    paint.setPen(Qt::black);
    paint.setBrush(Qt::black);

    renderer->render(&paint, QRectF(0, 0, ww, wh));
}

void
ScoreWidget::showPage(int page)
{
    if (page < 0 || page >= getPageCount()) {
        SVDEBUG << "ScoreWidget::showPage: page number " << page
                << " out of range; have " << getPageCount() << " pages" << endl;
        return;
    }
    
    m_page = page;
    emit pageChanged(m_page);
    update();
}

void
ScoreWidget::setHighlightEventByLocation(Fraction location)
{
    //!!! do we need this?
    throw std::runtime_error("Not yet implemented");
}

void
ScoreWidget::setHighlightEventByLabel(EventLabel label)
{
    m_eventToHighlight = getEventWithLabel(label);
    if (m_eventToHighlight.isNull()) {
        SVDEBUG << "ScoreWidget::setHighlightEventByLabel: Label \"" << label
                << "\" not found" << endl;
        return;
    }

    SVDEBUG << "ScoreWidget::setHighlightEventByLabel: Event with label \""
            << label << "\" found at " << m_eventToHighlight.location << endl;
    
    int page = m_eventToHighlight.page;
    if (page != m_page) {
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidget::setHighlightEventByLabel: Flipping to page "
                << page << endl;
#endif
        showPage(page);
    }

    update();
}

void
ScoreWidget::setInteractionMode(InteractionMode mode)
{
    if (mode == m_mode) {
        return;
    }

#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidget::setInteractionMode: switching from " << int(m_mode)
            << " to " << int(mode) << endl;
#endif
    
    m_mode = mode;
    update();
    emit interactionModeChanged(m_mode);
}
