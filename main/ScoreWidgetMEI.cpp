/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "ScoreWidgetMEI.h"
#include "ScoreFinder.h"

#include <QPainter>
#include <QMouseEvent>
#include <QSvgRenderer>

#include "base/Debug.h"

#include <vector>

#include "verovio/include/vrv/toolkit.h"
#include "verovio/include/vrv/vrv.h"

#include "vrvtrim.h"

#define DEBUG_SCORE_WIDGET 1

static QColor navigateHighlightColour("#59c4df");
static QColor editHighlightColour("#ffbd00");
static QColor selectHighlightColour(150, 150, 255);

using std::vector;
using std::pair;
using std::string;
using std::shared_ptr;
using std::make_shared;

ScoreWidgetMEI::ScoreWidgetMEI(QWidget *parent) :
    ScoreWidgetBase(parent),
    m_page(-1),
    m_mode(ScoreInteractionMode::None),
    m_scorePosition(-1),
    m_mousePosition(-1),
    m_mouseActive(false)
{
    setFrameStyle(Panel | Plain);
    setMinimumSize(QSize(100, 100));
    setMouseTracking(true);

    if (!m_tempDir.isValid()) {
        SVCERR << "ScoreWidgetMEI: Temporary directory is not valid! Can't unbundle resources; rendering will fail" << endl;
    } else {
        bool success = true;
        m_tempDir.setAutoRemove(true);
        QDir sourceRoot(":verovio/data/");
        QDir targetRoot(QDir(m_tempDir.path()).filePath("verovio"));
        QStringList names = sourceRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        names.push_back(".");
        for (auto name: names) {
            QDir sourceDir(sourceRoot.filePath(name));
            QDir targetDir(targetRoot.filePath(name));
            if (!QDir().mkpath(targetDir.path())) {
                SVCERR << "ScoreWidgetMEI: Failed to create directory \""
                       << targetDir.path() << "\"" << endl;
                success = false;
                break;
            }
            SVCERR << "ScoreWidgetMEI: scanning dir \"" << sourceDir.path()
                   << "\"..." << endl;
            for (auto f: sourceDir.entryInfoList(QDir::Files)) {
                QString sourcePath(f.filePath());
                SVCERR << "ScoreWidgetMEI: found \"" << sourcePath
                       << "\"..." << endl;
                QString targetPath(targetDir.filePath(f.fileName()));
                if (!QFile(sourcePath).copy(targetPath)) {
                    SVCERR << "ScoreWidgetMEI: Failed to copy file from \""
                           << sourcePath << "\" to \"" << targetPath << "\""
                           << endl;
                    success = false;
                    break;
                }
            }
        }
        if (success) {
            m_verovioResourcePath = targetRoot.canonicalPath();
            SVDEBUG << "ScoreWidgetMEI: Unbundled Verovio resources to \""
                    << m_verovioResourcePath << "\"" << endl;
        }
    }
}

ScoreWidgetMEI::~ScoreWidgetMEI()
{

}

QString
ScoreWidgetMEI::getCurrentScore() const
{
    return m_scoreName;
}

int
ScoreWidgetMEI::getCurrentPage() const
{
    return m_page;
}

int
ScoreWidgetMEI::getPageCount() const
{
    return m_svgPages.size();
}

void
ScoreWidgetMEI::loadAScore(QString scoreName)
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
ScoreWidgetMEI::loadAScore(QString scoreName, QString &errorString)
{
    SVDEBUG << "ScoreWidgetMEI::loadAScore: Score \"" << scoreName
            << "\" requested" << endl;

    if (m_verovioResourcePath == QString()) {
        SVDEBUG << "ScoreWidgetMEI::loadAScore: No Verovio resource path available" << endl;
        return false;
    }
    
    clearSelection();
    m_svgPages.clear();
    m_page = -1;
    
    string scorePath =
        ScoreFinder::getScoreFile(scoreName.toStdString(), "mei");
    if (scorePath == "") {
        errorString = "Score file (.mei) not found!";
        SVDEBUG << "ScoreWidgetMEI::loadAScore: " << errorString << endl;
        return false;
    }
    
    SVDEBUG << "ScoreWidgetMEI::loadAScore: Asked to load MEI file \""
            << scorePath << "\" for score \"" << scoreName << "\"" << endl;

    vrv::Toolkit toolkit(false);
    if (!toolkit.SetResourcePath(m_verovioResourcePath.toStdString())) {
        SVDEBUG << "ScoreWidgetMEI::loadAScore: Failed to set Verovio resource path" << endl;
        return false;
    }
    if (!toolkit.LoadFile(scorePath)) {
        SVDEBUG << "ScoreWidgetMEI::loadAScore: Load failed in Verovio toolkit" << endl;
        return false;
    }

    int pp = toolkit.GetPageCount();
    for (int p = 0; p < pp; ++p) {
        // For the first cut, just store the SVG texts for each page
        // here. Two alternative extremes are (i) convert them to Qt's
        // internal format by loading to QSvgRenderer and store those,
        // or (ii) store only the MEI toolkit object and render to SVG
        // on demand (useful for reflow?)
        std::string svgText = toolkit.RenderToSVG(p + 1); // (verovio 1-based)
        svgText = VrvTrim::transformSvgToTiny(svgText);

        shared_ptr<QSvgRenderer> renderer = make_shared<QSvgRenderer>
            (QByteArray::fromStdString(svgText));
        renderer->setAspectRatioMode(Qt::KeepAspectRatio);

        SVDEBUG << "ScoreWidgetMEI::showPage: created renderer from "
                << svgText.size() << "-byte SVG data" << endl;
        
        m_svgPages.push_back(renderer);
    }
    
    m_scoreName = scoreName;
    m_scoreFilename = QString::fromStdString(scorePath);

    SVDEBUG << "ScoreWidgetMEI::loadAScore: Load successful, showing first page"
            << endl;
    showPage(0);
    return true;
}

void
ScoreWidgetMEI::setElements(const ScoreElements &elements)
{
    SVDEBUG << "ScoreWidgetMEI::setElements: NOTE: Not used by this implementation" << endl;
}

void
ScoreWidgetMEI::setMusicalEvents(const Score::MusicalEventList &events)
{
    m_musicalEvents = events;

    SVDEBUG << "ScoreWidgetMEI::setMusicalEvents: " << events.size()
            << " events" << endl;

    m_idPageMap.clear();
    m_idLocationMap.clear();
    
    if (m_svgPages.empty()) {
        SVDEBUG << "ScoreWidgetMEI::setMusicalEvents: WARNING: No SVG pages, score should have been set before this" << endl;
        return;
    }
    
    int p = 0;
    int npages = m_svgPages.size();
    
    for (const auto &ev : m_musicalEvents) {
        for (const auto &n : ev.notes) {
            QString id = QString::fromStdString(n.noteId);
            if (p + 1 < npages &&
                !m_svgPages[p]->elementExists(id) &&
                m_svgPages[p + 1]->elementExists(id)) {
                ++p;
            }
            if (m_svgPages[p]->elementExists(id)) {
                m_idPageMap[id] = p;
                QRectF rect = m_svgPages[p]->boundsOnElement(id); 
                rect = m_svgPages[p]->transformForElement(id).mapRect(rect);
                m_idLocationMap[id] = rect;
                SVDEBUG << "id " << id << " -> page " << p << ", rect "
                        << rect.x() << "," << rect.y() << " " << rect.width()
                        << "x" << rect.height() << endl;
            }
        }
    }

    SVDEBUG << "ScoreWidgetMEI::setMusicalEvents: Done" << endl;
}

void
ScoreWidgetMEI::resizeEvent(QResizeEvent *)
{
    if (m_page >= 0) {
        showPage(m_page);
    }
}

void
ScoreWidgetMEI::enterEvent(QEnterEvent *)
{
    m_mouseActive = true;
    update();
}

void
ScoreWidgetMEI::leaveEvent(QEvent *)
{
    if (m_mouseActive) {
        emit interactionEnded(m_mode);
    }
    m_mouseActive = false;
    update();
}

void
ScoreWidgetMEI::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_mouseActive) return;

    m_mousePosition = positionForPoint(e->pos());
    update();

    if (m_mousePosition >= 0) {
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidgetMEI::mouseMoveEvent: Emitting scorePositionHighlighted at " << m_mousePosition << endl;
#endif
        emit scorePositionHighlighted(m_mousePosition, m_mode);
    }
}

void
ScoreWidgetMEI::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        return;
    }
    
    mouseMoveEvent(e);
/*!!!
    if (!m_elements.empty() && m_mousePosition >= 0 &&
        (m_mode == ScoreInteractionMode::SelectStart ||
         m_mode == ScoreInteractionMode::SelectEnd)) {

        if (m_mode == ScoreInteractionMode::SelectStart) {
            m_selectStartPosition = m_mousePosition;
            if (m_selectEndPosition <= m_selectStartPosition) {
                m_selectEndPosition = -1;
            }
        } else {
            m_selectEndPosition = m_mousePosition;
            if (m_selectStartPosition >= m_selectEndPosition) {
                m_selectStartPosition = -1;
            }
        }
        
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidgetMEI::mousePressEvent: Set select start to "
                << m_selectStartPosition << " and end to "
                << m_selectEndPosition << endl;
#endif

        int start = m_selectStartPosition;
        int end = m_selectEndPosition;
        if (start == -1) start = getStartPosition();
        if (end == -1) end = getEndPosition();
        emit selectionChanged(start,
                              isSelectedFromStart(),
                              labelForPosition(start),
                              end,
                              isSelectedToEnd(),
                              labelForPosition(end));
    }
*/    
    if (m_mousePosition >= 0) {
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidgetMEI::mousePressEvent: Emitting scorePositionActivated at " << m_mousePosition << endl;
#endif
        emit scorePositionActivated(m_mousePosition, m_mode);
    }
}

void
ScoreWidgetMEI::clearSelection()
{
#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidgetMEI::clearSelection" << endl;
#endif

    if (m_selectStartPosition == -1 && m_selectEndPosition == -1) {
        return;
    }

    m_selectStartPosition = -1;
    m_selectEndPosition = -1;

    emit selectionChanged(m_selectStartPosition,
                          true,
                          labelForPosition(getStartPosition()),
                          m_selectEndPosition,
                          true,
                          labelForPosition(getEndPosition()));

    update();
}

int
ScoreWidgetMEI::getStartPosition() const
{
/*!!!
  if (m_elementsByPosition.empty()) {
        return 0;
    }
    return m_elementsByPosition.begin()->second.position;
*/
    return 0;
}

bool
ScoreWidgetMEI::isSelectedFromStart() const
{
    return true;
    /*!!!
    return (m_elementsByPosition.empty() ||
            m_selectStartPosition < 0 ||
            m_selectStartPosition <= getStartPosition());
    */
}

int
ScoreWidgetMEI::getEndPosition() const
{
    return 0;
    /*
    if (m_elementsByPosition.empty()) {
        return 0;
    }
    return m_elementsByPosition.rbegin()->second.position;
    */
}

bool
ScoreWidgetMEI::isSelectedToEnd() const
{
    return true;
    /*
    return (m_elementsByPosition.empty() ||
            m_selectEndPosition < 0 ||
            m_selectEndPosition >= getEndPosition());
    */
}

bool
ScoreWidgetMEI::isSelectedAll() const
{
    return isSelectedFromStart() && isSelectedToEnd();
}

void
ScoreWidgetMEI::getSelection(int &start, int &end) const
{
    start = m_selectStartPosition;
    end = m_selectEndPosition;
}

void
ScoreWidgetMEI::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        return;
    }

#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidgetMEI::mouseDoubleClickEvent" << endl;
#endif
        
    if (m_mode == ScoreInteractionMode::Navigate) {
        setInteractionMode(ScoreInteractionMode::Edit);
    }

    mousePressEvent(e);
}
    
QRectF
ScoreWidgetMEI::rectForPosition(int pos)
{
    if (pos < 0) {
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidgetMEI::rectForPosition: No position" << endl;
#endif
        return {};
    }

    return {};
    /*!!!
    auto itr = m_elementsByPosition.lower_bound(pos);
    if (itr == m_elementsByPosition.end()) {
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidgetMEI::rectForPosition: Position " << pos
                << " does not have any corresponding element" << endl;
#endif
        return {};
    }

    // just use the first element for now...

    const ScoreElement &elt = itr->second;
    
#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidgetMEI::rectForPosition: Position "
            << pos << " has corresponding element id="
            << elt.id << " on page=" << elt.page << " with x="
            << elt.x << ", y=" << elt.y << ", sy=" << elt.sy
            << ", label= " << elt.label << endl;
#endif

    return rectForElement(elt);
    */
}
    
QString
ScoreWidgetMEI::labelForPosition(int pos)
{
    /*!!!
    auto itr = m_elementsByPosition.lower_bound(pos);
    if (itr == m_elementsByPosition.end()) {
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidgetMEI::rectForPosition: Position " << pos
                << " does not have any corresponding element" << endl;
#endif
        return {};
    }

    return itr->second.label;
    */
    return {};
}

QRectF
ScoreWidgetMEI::rectForElement(const ScoreElement &elt)
{
    return {};
}

int
ScoreWidgetMEI::positionForPoint(QPoint point)
{

    QSize mySize = size();
//!!!    QSize imageSize = m_image.size();
    QSize imageSize = mySize;

    double dpr = devicePixelRatio();
    int xorigin = round((mySize.width() - imageSize.width() / dpr) / 2);
    int yorigin = round((mySize.height() - imageSize.height() / dpr) / 2);

    double xratio = double(imageSize.width()) / (35716.5 * dpr);
    double yratio = double(imageSize.height()) / (50513.4 * dpr);

    //!!! Slow but ok to start with

    int pos = -1;

    int x = (point.x() - xorigin) / xratio;
    int y = (point.y() - yorigin) / yratio;
/*!!!    
    for (auto itr = m_elementsByPosition.begin();
         itr != m_elementsByPosition.end();
         ++itr) {

        const auto &elt = itr->second;

        if (elt.page < m_page) {
            continue;
        }
        if (elt.page > m_page) {
            break;
        }
            
        if (y < elt.y || y > elt.y + elt.sy || x < elt.x) {
            continue;
        }
        auto jtr = itr;
        if (++jtr != m_elementsByPosition.end()) {
            if (jtr->second.x > elt.x && x > jtr->second.x) {
                continue;
            }
        }
        pos = itr->first;
        break;
    }
*/

#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidgetMEI::positionForPoint: point " << point.x()
            << "," << point.y() << " -> position " << pos << endl;
#endif
    
    return pos;
}

void
ScoreWidgetMEI::paintEvent(QPaintEvent *e)
{
    QFrame::paintEvent(e);

    if (m_page < 0 || m_page >= m_svgPages.size()) {
        SVDEBUG << "ScoreWidgetMEI::paintEvent: No page or page out of range, painting nothing" << endl;
        return;
    }

    auto renderer = m_svgPages[m_page];
    
    QPainter paint(this);
    QSize mySize = size();
//!!!    QSize imageSize = m_image.size();
    QSize imageSize = mySize;

    if (!mySize.width() || !mySize.height() ||
        !imageSize.width() || !imageSize.height()) {
        return;
    }
    
    double dpr = devicePixelRatio();

    int xorigin = round((mySize.width() - imageSize.width() / dpr) / 2);
    int yorigin = round((mySize.height() - imageSize.height() / dpr) / 2);

    // Show a highlight bar under the mouse if the interaction mode is
    // anything other than None - the colour depends on the mode
    
    if (m_mode != ScoreInteractionMode::None) {

        int position = m_scorePosition;
        if (m_mouseActive) {
            position = m_mousePosition;
        }
        
        QRectF rect = rectForPosition(position);
        if (!rect.isNull()) {

            QColor highlightColour;

            switch (m_mode) {
            case ScoreInteractionMode::Navigate:
                highlightColour = navigateHighlightColour;
                break;
            case ScoreInteractionMode::Edit:
                highlightColour = editHighlightColour;
                break;
            case ScoreInteractionMode::SelectStart:
            case ScoreInteractionMode::SelectEnd:
                highlightColour = selectHighlightColour.darker();
                break;
            default: // None already handled in conditional above
                throw std::runtime_error("Unhandled case in mode switch");
            }
            
            highlightColour.setAlpha(160);
            paint.setPen(Qt::NoPen);
            paint.setBrush(highlightColour);
            
#ifdef DEBUG_SCORE_WIDGET
            SVDEBUG << "ScoreWidgetMEI::paint: highlighting rect with origin "
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
/*!!!
    if (!m_elements.empty() &&
        (!isSelectedAll() ||
         (m_mode == ScoreInteractionMode::SelectStart ||
          m_mode == ScoreInteractionMode::SelectEnd))) {

        QColor fillColour = selectHighlightColour;
        fillColour.setAlpha(100);
        paint.setPen(Qt::NoPen);
        paint.setBrush(fillColour);
        
        PositionElementMap::iterator i0 = m_elementsByPosition.begin();
        if (m_selectStartPosition > 0) {
            i0 = m_elementsByPosition.lower_bound(m_selectStartPosition);
        }
        PositionElementMap::iterator i1 = m_elementsByPosition.end();
        if (m_selectEndPosition > 0) {
            i1 = m_elementsByPosition.lower_bound(m_selectEndPosition);
        }

        int prevY = -1;
        for (auto i = i0; i != i1 && i != m_elementsByPosition.end(); ++i) {
            if (i->second.page < m_page) {
                continue;
            }
            if (i->second.page > m_page) {
                break;
            }
            const ScoreElement &elt(i->second);
            QRectF rect = rectForElement(elt);
            if (rect == QRectF()) {
                continue;
            }
            auto j = i;
            ++j;
            if (i == i0) {
                prevY = elt.y;
            }
            if (elt.y != prevY) {
                rect.setX(0);
//!!!                rect.setWidth(m_image.width());
            } else {
//!!!                rect.setWidth(m_image.width() - rect.x());
            }
            if (j != m_elementsByPosition.end() && j->second.y == elt.y) {
                QRectF nextRect = rectForElement(j->second);
                if (nextRect != QRectF()) {
                    rect.setWidth(nextRect.x() - rect.x());
                }
            }
            paint.drawRect(rect);
            prevY = elt.y;
        }
    }
*/
#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidgetMEI::paint: have image of size " << imageSize.width()
            << " x " << imageSize.height() << ", painting to widget of size "
            << mySize.width() << " x " << mySize.height() << ", xorigin = "
            << xorigin << ", yorigin = " << yorigin << ", devicePixelRatio = "
            << dpr << endl;
#endif

    SVDEBUG << "ScoreWidgetMEI: have renderer of defaultSize "
            << renderer->defaultSize().width() << " x "
            << renderer->defaultSize().height() << endl;

    paint.setPen(Qt::black);
    paint.setBrush(Qt::black);

    auto vb = renderer->viewBoxF();
    SVDEBUG << "SVG view box = " << vb.x() << "," << vb.y() << " " << vb.width()
            << " x " << vb.height() << endl;

    //!!! deal with page origin later
    m_pageToWidget = QTransform::fromScale(mySize.width() / vb.width(),
                                           mySize.height() / vb.height());
    m_widgetToPage = QTransform::fromScale(vb.width() / mySize.width(),
                                           vb.height() / mySize.height());
    
    renderer->render(&paint, QRectF(0, 0, mySize.width(), mySize.height()));
}

void
ScoreWidgetMEI::showPage(int page)
{
    if (page < 0 || page >= getPageCount()) {
        SVDEBUG << "ScoreWidgetMEI::showPage: page number " << page
                << " out of range; have " << getPageCount() << " pages" << endl;
        return;
    }
    
    m_page = page;
    emit pageChanged(m_page);
    update();
}

void
ScoreWidgetMEI::setScorePosition(int position)
{
/*
  m_scorePosition = position;

    if (m_scorePosition >= 0) {
        auto itr = m_elementsByPosition.lower_bound(m_scorePosition);
        if (itr == m_elementsByPosition.end()) {
#ifdef DEBUG_SCORE_WIDGET
            SVDEBUG << "ScoreWidgetMEI::setScorePosition: Position "
                    << m_scorePosition
                    << " does not have any corresponding element"
                    << endl;
#endif
        } else {
            ScoreElement elt = itr->second;
            if (elt.page != m_page) {
#ifdef DEBUG_SCORE_WIDGET
                SVDEBUG << "ScoreWidgetMEI::setScorePosition: Flipping to page "
                        << elt.page << endl;
#endif
                showPage(elt.page);
            }
        }
    }
            
*/
    update();
}

void
ScoreWidgetMEI::setInteractionMode(ScoreInteractionMode mode)
{
    if (mode == m_mode) {
        return;
    }

#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidgetMEI::setInteractionMode: switching from " << int(m_mode)
            << " to " << int(mode) << endl;
#endif
    
    m_mode = mode;
    update();
    emit interactionModeChanged(m_mode);
}
