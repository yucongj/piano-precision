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
#include <QToolButton>
#include <QGridLayout>
#include <QSettings>

#include "base/Debug.h"
#include "widgets/IconLoader.h"

#include <vector>

#include "verovio/include/vrv/toolkit.h"
#include "verovio/include/vrv/vrv.h"

#include "vrvtrim.h"

//#define DEBUG_SCORE_WIDGET 1
//#define DEBUG_EVENT_FINDING 1

static QColor navigateHighlightColour("#59c4df");
static QColor editHighlightColour("#ffbd00");
static QColor selectHighlightColour(150, 150, 255);

using std::vector;
using std::pair;
using std::string;
using std::shared_ptr;
using std::make_shared;

ScoreWidget::ScoreWidget(bool withZoomControls, QWidget *parent) :
    QFrame(parent),
    m_page(-1),
    m_scale(100),
    m_mode(InteractionMode::None),
    m_mouseActive(false)
{
    setFrameStyle(Panel | Plain);
    setMinimumSize(QSize(100, 100));
    setMouseTracking(true);
    m_verovioResourcePath = ScoreParser::getResourcePath();

    if (withZoomControls) {
        sv::IconLoader il;
        auto zoomOut = new QToolButton;
        zoomOut->setText(QString(QChar(0x2212))); // mathematical minus
        zoomOut->setToolTip(tr("Decrease Staff Size"));
        connect(zoomOut, &QToolButton::clicked, this, &ScoreWidget::zoomOut);
        auto zoomReset = new QToolButton;
        zoomReset->setText(QString(QChar(0x2218))); // mathematical ring operator, I just quite liked it!
        zoomReset->setToolTip(tr("Reset Staff Size to Default"));
        connect(zoomReset, &QToolButton::clicked, this, &ScoreWidget::zoomReset);
        auto zoomIn = new QToolButton;
        zoomIn->setText(QString(QChar(0x002b))); // mathematical plus
        zoomIn->setToolTip(tr("Increase Staff Size"));
        connect(zoomIn, &QToolButton::clicked, this, &ScoreWidget::zoomIn);
        auto layout = new QGridLayout;
        layout->addWidget(zoomOut, 1, 0);
        layout->addWidget(zoomReset, 1, 1);
        layout->addWidget(zoomIn, 1, 2);
        layout->setRowStretch(0, 10);
        layout->setColumnStretch(3, 10);
        setLayout(layout);

        QSettings settings;
        settings.beginGroup("ScoreWidget");
        m_scale = settings.value("scale", m_scale).toInt();
        settings.endGroup();
    }
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
ScoreWidget::setScale(int scale)
{
    if (m_scale == scale) {
        return;
    }
    m_scale = scale;
    auto scoreName = m_scoreName;
    auto scoreFilename = m_scoreFilename;
    auto musicalEvents = m_musicalEvents;
    QString errorString;
    if (!loadScoreFile(scoreName, scoreFilename, errorString)) {
        SVCERR << "ScoreWidget::setScale: Failed to reload score "
               << scoreName << ": " << errorString << endl;
        return;
    }
    setMusicalEvents(musicalEvents);
    if (m_highlightEventLabel != "") {
        setHighlightEventByLabel(m_highlightEventLabel);
    }
    update();
    
    QSettings settings;
    settings.beginGroup("ScoreWidget");
    settings.setValue("scale", m_scale);
    settings.endGroup();
}

int
ScoreWidget::getScale() const
{
    return m_scale;
}

bool
ScoreWidget::loadScoreFile(QString scoreName, QString scoreFile, QString &errorString)
{
    clearSelection();

    if (m_verovioResourcePath == "") {
        SVDEBUG << "ScoreWidget::loadScoreFile: No Verovio resource path available" << endl;
        errorString = "No Verovio resource path available: application was not packaged properly";
        return false;
    }
    
    clearSelection();

    m_svgPages.clear();
    m_noteSystemExtentMap.clear();

    m_highlightEventLabel = {};
    m_eventToHighlight = {};
    m_selectStart = {};
    m_selectEnd = {};
    
    m_page = -1;
    
    SVDEBUG << "ScoreWidget::loadScoreFile: Asked to load MEI file \""
            << scoreFile << "\" for score \"" << scoreName << "\"" << endl;

    vrv::Toolkit toolkit(false);
    if (!toolkit.SetResourcePath(m_verovioResourcePath)) {
        SVDEBUG << "ScoreWidget::loadScoreFile: Failed to set Verovio resource path" << endl;
        return false;
    }

    string defaultOptions = "\"footer\": \"none\"";
    
    if (m_scale != 100) {
        toolkit.SetOptions("{\"scaleToPageSize\": true, " + defaultOptions + "}");
        if (!toolkit.SetScale(m_scale)) {
            SVDEBUG << "ScoreWidget::loadScoreFile: Failed to set rendering scale" << endl;
        } else {
            SVDEBUG << "ScoreWidget::loadScoreFile: Set scale to " << m_scale << endl;
        }
        SVDEBUG << "options: " << toolkit.GetOptions() << endl;
    } else {
        toolkit.SetOptions("{" + defaultOptions + "}");
    }
    
    if (!toolkit.LoadFile(scoreFile.toStdString())) {
        SVDEBUG << "ScoreWidget::loadScoreFile: Load failed in Verovio toolkit" << endl;
        return false;
    }

    int pp = toolkit.GetPageCount();

    SVDEBUG << "ScoreWidget::loadScoreFile: Have " << pp << " pages" << endl;
    
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
    m_scoreFilename = scoreFile;

    SVDEBUG << "ScoreWidget::loadScoreFile: Load successful, showing first page"
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
    doc.setContent(svgData);

    Extent currentExtent;
    vector<double> staffLines;

    auto extractExtent = [&](QDomElement path,
                             QString systemId,
                             QString staffId) -> Extent {
        
        // We're looking for a path of the form Mx0 y0 Lx1 y1
        
        QStringList dd = path.attribute("d").split(" ", Qt::SkipEmptyParts);
        if (dd.size() != 4) return {};

        if (dd[0].startsWith("M", Qt::CaseInsensitive)) {
            dd[0] = dd[0].right(dd[0].length()-1);
        } else return {};

        if (dd[2].startsWith("L", Qt::CaseInsensitive)) {
            dd[2] = dd[2].right(dd[2].length()-1);
        } else return {};
        
        bool ok = false;
        double x0 = dd[0].toDouble(&ok); if (!ok) return {};
        double y0 = dd[1].toDouble(&ok); if (!ok) return {};
        double x1 = dd[2].toDouble(&ok); if (!ok) return {};
        double y1 = dd[3].toDouble(&ok); if (!ok) return {};
        
        if (systemId != "" && y1 > y0 && x1 == x0) {
#ifdef DEBUG_SCORE_WIDGET
            SVDEBUG << "Found possible extent for system with id \""
                    << systemId << "\": from "
                    << y0 << " -> " << y1 << endl;
#endif
            QRectF mapped = renderer->transformForElement(systemId)
                .mapRect(QRectF(0, y0, 1, y1 - y0));
            return Extent(mapped.y(), mapped.height());
        }

        if (staffId != "" && x1 > x0 && y1 == y0 &&
            staffLines.size() < 5) {
#ifdef DEBUG_SCORE_WIDGET
            SVDEBUG << "Found possible staff line for staff with id \""
                    << staffId << "\": from "
                    << x0 << " to " << x1 << " at y = " << y0 << endl;
#endif
            staffLines.push_back(y0);
            if (staffLines.size() == 5) {
                y0 = staffLines[0];
                y1 = staffLines[4];
#ifdef DEBUG_SCORE_WIDGET
                SVDEBUG << "Deducing extent from staff lines as "
                        << y0 << " -> " << y1 << endl;
#endif
                QRectF mapped = renderer->transformForElement(staffId)
                    .mapRect(QRectF(0, y0, 1, y1 - y0));
                return Extent(mapped.y(), mapped.height());
            }
        }                
        
        return {};
    };
    
    std::function<void(QDomNode, QString, QString)> descend =
        [&](QDomNode node, QString systemId, QString staffId) {

        if (!node.isElement()) {
            return;
        }

        QDomElement elt = node.toElement();
        QString tag = elt.tagName();

        if (systemId != "" || staffId != "") {
            if (currentExtent.isNull() && tag == "path") {
                // Haven't yet seen system dimensions, defined using
                // either the vertical path that joins the systems, or
                // if there is only one staff, locations of the first
                // and fifth staff lines. This might be one of the
                // bits of evidence we need
                currentExtent = extractExtent(elt, systemId, staffId);
            }
        }
        
        if (tag == "g") {
            // The remaining elements we're interested in (system,
            // staff, note) are all defined using group tags in SVG

            QStringList classes =
                elt.attribute("class").split(" ", Qt::SkipEmptyParts);

            if (systemId == "" && classes.contains("system")) {
                systemId = elt.attribute("id");
                currentExtent = {};
            }

            if (staffId == "" && classes.contains("staff")) {
                staffId = elt.attribute("id");
                staffLines.clear();
                if (systemId == "") { // a staff outside a system
                    currentExtent = {};
                }
            }
            
            if (!currentExtent.isNull() && classes.contains("note")) {
                QString noteId = elt.attribute("id");
                if (noteId != "") {
#ifdef DEBUG_EVENT_FINDING
                    SVDEBUG << "Assigning system extent ("
                            << currentExtent.y << ","
                            << currentExtent.height
                            << ") to note with id \"" << noteId << "\"" << endl;
#endif
                    m_noteSystemExtentMap[noteId] = currentExtent;
                }
            }
        }
            
        auto children = node.childNodes();
        for (int i = 0; i < children.size(); ++i) {
            descend(children.at(i), systemId, staffId);
        }
    };

    descend(doc.documentElement(), "", "");
}                       

void
ScoreWidget::setMusicalEvents(const Score::MusicalEventList &events)
{
    m_musicalEvents = events;

#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidget::setMusicalEvents: " << events.size()
            << " events" << endl;
#endif

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
            if (!n.isNewNote) {
                continue;
            }
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

#ifdef DEBUG_EVENT_FINDING
                SVDEBUG << "found note id " << id << " for event at "
                        << ev.measureInfo.toLabel()
                        << " -> page " << p << ", rect "
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

#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidget::setMusicalEvents: Done" << endl;
#endif
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

#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidget::mouseMoveEvent: id under mouse = "
            << m_eventUnderMouse.id << endl;
#endif
    
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

void
ScoreWidget::zoomIn()
{
    if (m_scale < 240) {
        setScale(m_scale + 20);
    }
}

void
ScoreWidget::zoomOut()
{
    if (m_scale > 20) {
        setScale(m_scale - 20);
    }
}

void
ScoreWidget::zoomReset()
{
    setScale(100);
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
            << "," << point.y() << " -> element id " << found.id
            << " with x = " << foundX << endl;
#endif
    
    return found;
}

QRectF
ScoreWidget::getHighlightRectFor(const EventData &event)
{
    QRectF rect = event.boxOnPage;

    if (m_noteSystemExtentMap.find(event.id) != m_noteSystemExtentMap.end()) {
        Extent extent = m_noteSystemExtentMap.at(event.id);
        rect = QRectF(rect.x(), extent.y, rect.width(), extent.height);
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

#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidget::paint: widget size " << ww << "x" << wh
            << ", page size " << pw << "x" << ph << endl;
#endif
    
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
#ifdef DEBUG_SCORE_WIDGET
            SVDEBUG << "ScoreWidget::paint: under mouse = "
                    << event.label << endl;
#endif
        } else {
            event = m_eventToHighlight;
#ifdef DEBUG_SCORE_WIDGET
            SVDEBUG << "ScoreWidget::paint: to highlight = "
                    << event.label << endl;
#endif
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

        auto exclusiveComparator =
            [](const Score::MusicalEvent &e, const Fraction &f) {
                return e.measureInfo.measureFraction < f;
        };
        auto inclusiveComparator =
            [](const Score::MusicalEvent &e, const Fraction &f) {
                return !(f < e.measureInfo.measureFraction);
        };
        
        Score::MusicalEventList::iterator i0 = m_musicalEvents.begin();
        if (!m_selectStart.isNull()) {
            i0 = lower_bound(m_musicalEvents.begin(), m_musicalEvents.end(),
                             m_selectStart.location, exclusiveComparator);
        }
        Score::MusicalEventList::iterator i1 = m_musicalEvents.end();
        if (!m_selectEnd.isNull()) {
            i1 = lower_bound(m_musicalEvents.begin(), m_musicalEvents.end(),
                             m_selectEnd.location, inclusiveComparator);
        }

#ifdef DEBUG_SCORE_WIDGET
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
#endif

        double lineOrigin = m_pageToWidget.map(QPointF(0, 0)).x();
        double lineWidth = m_pageToWidget.map(QPointF(pageSize.width(), 0)).x();
        
        double prevY = -1.0;
        double furthestX = 0.0;

        for (auto i = i0; i != i1 && i != m_musicalEvents.end(); ++i) {
            EventData data = getEventForMusicalEvent(*i);
            if (data.page < m_page) {
                continue;
            }
            if (data.page > m_page) {
                break;
            }
            QRectF rect = getHighlightRectFor(data);
#ifdef DEBUG_EVENT_FINDING                    
            SVDEBUG << "I'm at " << rect.x() << "," << rect.y() << " with width "
                    << rect.width() << " (furthest X so far = " << furthestX
                    << ")" << endl;
#endif
            if (rect == QRectF()) {
                continue;
            }
            if (i == i0) {
                prevY = rect.y();
            }
            if (rect.y() > prevY) {
#ifdef DEBUG_EVENT_FINDING
                SVDEBUG << "New line, resetting x and furthestX to " << lineOrigin << endl;
#endif
                rect.setX(lineOrigin);
                furthestX = lineOrigin;
            } else if (rect.x() < furthestX - 0.001) {
                continue;
            }
            auto j = i;
            ++j;
            if (j != i1) {
                rect.setWidth(lineWidth - rect.x());
            }
            while (j != m_musicalEvents.end()) {
                EventData nextData = getEventForMusicalEvent(*j);
                QRectF nextRect = getHighlightRectFor(nextData);
                if (nextData.page == m_page &&
                    nextRect.y() <= rect.y() &&
                    nextRect.x() >= rect.x() &&
                    nextRect.width() > 0) {
#ifdef DEBUG_EVENT_FINDING                    
                    SVDEBUG << "next event is at " << nextRect.x()
                            << " with width " << nextRect.width() << endl;
#endif
                    if (nextRect.x() - rect.x() < rect.width()) {
                        rect.setWidth(nextRect.x() - rect.x());
                    }
                    break;
                }
                if (nextData.page > m_page ||
                    nextRect.y() > rect.y()) {
                    break;
                }
                ++j;
            }
            paint.drawRect(rect);
            prevY = rect.y();
            furthestX = rect.x() + rect.width();
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
ScoreWidget::setHighlightEventByLabel(EventLabel label)
{
    m_eventToHighlight = getEventWithLabel(label);
    if (m_eventToHighlight.isNull()) {
        SVDEBUG << "ScoreWidget::setHighlightEventByLabel: Label \"" << label
                << "\" not found" << endl;
        m_highlightEventLabel = "";
        return;
    }

    m_highlightEventLabel = label;
    
#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidget::setHighlightEventByLabel: Event with label \""
            << label << "\" found at " << m_eventToHighlight.location << endl;
#endif
    
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
