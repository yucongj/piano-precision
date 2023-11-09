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

#include <QPdfDocument>
#include <QPainter>
#include <QMouseEvent>

#include "base/Debug.h"

#include <vector>

#define DEBUG_SCORE_WIDGET 1

static QColor navigateHighlightColour("#59c4df");
static QColor editHighlightColour("#ffbd00");
static QColor selectHighlightColour("#913d88");

using std::vector;
using std::pair;
using std::string;

ScoreWidget::ScoreWidget(QWidget *parent) :
    QFrame(parent),
    m_document(new QPdfDocument(this)),
    m_page(-1),
    m_mode(InteractionMode::None),
    m_scorePosition(-1),
    m_mousePosition(-1),
    m_mouseActive(false)
{
    setFrameStyle(Panel | Plain);
    setMinimumSize(QSize(100, 100));
    setMouseTracking(true);
}

ScoreWidget::~ScoreWidget()
{
    delete m_document;
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
    return m_document->pageCount();
}

void
ScoreWidget::loadAScore(QString scoreName)
{
    QString errorString;
    if (!loadAScore(scoreName, errorString)) {
        emit loadFailed(scoreName, tr("Failed to load score %1: %2")
                        .arg(scoreName).arg(errorString));
    }
}

bool
ScoreWidget::loadAScore(QString scoreName, QString &errorString)
{
    SVDEBUG << "ScoreWidget::loadAScore: Score \"" << scoreName
            << "\" requested" << endl;

    m_page = -1;
    
    string scorePath =
        ScoreFinder::getScoreFile(scoreName.toStdString(), "pdf");
    if (scorePath == "") {
        errorString = "Score file (.pdf) not found!";
        SVDEBUG << "ScoreWidget::loadAScore: " << errorString << endl;
        return false;
    }

    QString filename = scorePath.c_str();
    auto result = m_document->load(filename);
    
    SVDEBUG << "ScoreWidget::loadAScore: Asked to load pdf file \""
            << filename << "\" for score \"" << scoreName
            << "\", result is " << result << endl;

    QString error;
    switch (result) {
    case QPdfDocument::NoError:
        break;
    case QPdfDocument::FileNotFoundError:
        errorString = tr("File not found");
        break;
    case QPdfDocument::InvalidFileFormatError:
        errorString = tr("File is in the wrong format");
        break;
    case QPdfDocument::IncorrectPasswordError:
        errorString = tr("File is password-protected");
        break;
    default:
        errorString = tr("Unable to read PDF");
    }

    if (error == "") {
        m_scoreName = scoreName;
        m_scoreFilename = filename;
        SVDEBUG << "ScoreWidget::loadAScore: Load successful, showing first page"
                << endl;
        showPage(0);
        return true;
    } else {
        SVDEBUG << "ScoreWidget::loadAScore: " << errorString << endl;
        return false;
    }
}

void
ScoreWidget::setElements(ScoreElements elements)
{
    m_elements = elements;

    m_elementsByPosition.clear();
    for (auto e: elements) {
        m_elementsByPosition.insert({ e.position, e });
    }

    m_selectStartPosition = -1;
    m_selectEndPosition = -1;
}

void
ScoreWidget::resizeEvent(QResizeEvent *)
{
    if (m_page >= 0) {
        showPage(m_page);
    }
}

void
ScoreWidget::enterEvent(QEvent *)
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

    m_mousePosition = positionForPoint(e->pos());
    update();

    if (m_mousePosition >= 0) {
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidget::mouseMoveEvent: Emitting scorePositionHighlighted at " << m_mousePosition << endl;
#endif
        emit scorePositionHighlighted(m_mousePosition, m_mode);
    }
}

void
ScoreWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        return;
    }
    
    mouseMoveEvent(e);

    if (!m_elements.empty() && m_mousePosition >= 0 &&
        (m_mode == InteractionMode::SelectStart ||
         m_mode == InteractionMode::SelectEnd)) {

        if (m_mode == InteractionMode::SelectStart) {
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
        SVDEBUG << "ScoreWidget::mousePressEvent: Set select start to "
                << m_selectStartPosition << " and end to "
                << m_selectEndPosition << endl;
#endif

        int start = m_selectStartPosition;
        int end = m_selectEndPosition;
        if (start == -1) start = getStartPosition();
        if (end == -1) end = getEndPosition();
        emit selectionChanged(start, isSelectedFromStart(),
                              end, isSelectedToEnd());
    }
    
    if (m_mousePosition >= 0) {
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidget::mousePressEvent: Emitting scorePositionActivated at " << m_mousePosition << endl;
#endif
        emit scorePositionActivated(m_mousePosition, m_mode);
    }
}

int
ScoreWidget::getStartPosition() const
{
    if (m_elementsByPosition.empty()) {
        return 0;
    }
    return m_elementsByPosition.begin()->second.position;
}

bool
ScoreWidget::isSelectedFromStart() const
{
    return (m_elementsByPosition.empty() ||
            m_selectStartPosition < 0 ||
            m_selectStartPosition <= getStartPosition());
}

int
ScoreWidget::getEndPosition() const
{
    if (m_elementsByPosition.empty()) {
        return 0;
    }
    return m_elementsByPosition.rbegin()->second.position;
}

bool
ScoreWidget::isSelectedToEnd() const
{
    return (m_elementsByPosition.empty() ||
            m_selectEndPosition < 0 ||
            m_selectEndPosition >= getEndPosition());
}

bool
ScoreWidget::isSelectedAll() const
{
    return isSelectedFromStart() && isSelectedToEnd();
}

void
ScoreWidget::getSelection(int &start, int &end) const
{
    start = m_selectStartPosition;
    end = m_selectEndPosition;
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
    
QRectF
ScoreWidget::rectForPosition(int pos)
{
    if (pos < 0) {
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidget::rectForPosition: No position" << endl;
#endif
        return {};
    }

    auto itr = m_elementsByPosition.lower_bound(pos);
    if (itr == m_elementsByPosition.end()) {
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidget::rectForPosition: Position " << pos
                << " does not have any corresponding element" << endl;
#endif
        return {};
    }

    // just use the first element for now...

    const ScoreElement &elt = itr->second;
    
#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidget::rectForPosition: Position "
            << pos << " has corresponding element id="
            << elt.id << " on page=" << elt.page << " with x="
            << elt.x << ", y=" << elt.y << ", sy=" << elt.sy << endl;
#endif

    return rectForElement(elt);
}

QRectF
ScoreWidget::rectForElement(const ScoreElement &elt)
{
    // Now, we know the units are a bit mad for these values.  I
    // think(?) they are in inches * dpi * constant where dpi = 360
    // and constant = 12, so inches * 4320 or mm * 170.08 approx.

    // To map these correctly we will need to know the page size at
    // which the pdf was rendered - since the defaults are presumably
    // locale dependent (I'm guessing US Letter in US, A4 everywhere
    // else). This info must be in the PDF one way or another, can
    // QPdfDocument tell us it?

    // Check that later, but for the mo let's hard code A4 and see
    // what comes out. So 297x210mm which would make the whole page
    // 50513.4 units tall and 35716.5 wide. Thus our y ratio will be
    // (pixel_height)/50513.4 and x ratio (pixel_width)/35716.5. Oh,
    // and that's times the device pixel ratio.

    if (elt.page != m_page) {
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidget::rectForElement: Element at " << elt.position
                << " is not on the current page (page " << elt.page
                << ", we are on " << m_page << ")" << endl;
#endif
        return {};
    }
    
    QSize mySize = size();
    QSize imageSize = m_image.size();

    int dpr = devicePixelRatio();
    int xorigin = (mySize.width() - imageSize.width() / dpr) / 2;
    int yorigin = (mySize.height() - imageSize.height() / dpr) / 2;

    double xratio = double(imageSize.width()) / (35716.5 * dpr);
    double yratio = double(imageSize.height()) / (50513.4 * dpr);

    // We don't get a valid element width - hardcode & reconsider this
    // later
    double fakeWidth = 500.0;
            
    return QRectF(xorigin + elt.x * xratio,
                  yorigin + elt.y * yratio,
                  fakeWidth * xratio,
                  elt.sy * yratio);
}

int
ScoreWidget::positionForPoint(QPoint point)
{
    // See above for discussion of units! But this is all a dupe, we
    // should pull it out

    QSize mySize = size();
    QSize imageSize = m_image.size();

    int dpr = devicePixelRatio();
    int xorigin = (mySize.width() - imageSize.width() / dpr) / 2;
    int yorigin = (mySize.height() - imageSize.height() / dpr) / 2;

    double xratio = double(imageSize.width()) / (35716.5 * dpr);
    double yratio = double(imageSize.height()) / (50513.4 * dpr);

    //!!! Slow but ok to start with

    int pos = -1;

    int x = (point.x() - xorigin) / xratio;
    int y = (point.y() - yorigin) / yratio;
    
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
            
        /*
        SVDEBUG << "comparing point " << point.x() << "," << point.y()
                << " -> adjusted coords " << x << "," << y
                << " with x, y, sy " << elt.x << "," << elt.y << ","
                << elt.sy << endl;
        */        
        
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

#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidget::positionForPoint: point " << point.x()
            << "," << point.y() << " -> position " << pos << endl;
#endif
    
    return pos;
}

void
ScoreWidget::paintEvent(QPaintEvent *e)
{
    QFrame::paintEvent(e);
    
    QPainter paint(this);
    QSize mySize = size();
    QSize imageSize = m_image.size();

    if (!mySize.width() || !mySize.height() ||
        !imageSize.width() || !imageSize.height()) {
        return;
    }
    
    int dpr = devicePixelRatio();

    int xorigin = (mySize.width() - imageSize.width() / dpr) / 2;
    int yorigin = (mySize.height() - imageSize.height() / dpr) / 2;

    // Show a highlight bar under the mouse if the interaction mode is
    // anything other than None - the colour depends on the mode
    
    if (m_mode != InteractionMode::None) {

        int position = m_scorePosition;
        if (m_mouseActive) {
            position = m_mousePosition;
        }
        
        QRectF rect = rectForPosition(position);
        if (!rect.isNull()) {

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
                highlightColour = selectHighlightColour;
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

    if (!m_elements.empty() &&
        (!isSelectedAll() ||
         (m_mode == InteractionMode::SelectStart ||
          m_mode == InteractionMode::SelectEnd))) {

        QColor fillColour = selectHighlightColour.lighter();
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
                rect.setWidth(m_image.width());
            } else {
                rect.setWidth(m_image.width() - rect.x());
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
    
    paint.drawImage
        (QRect(xorigin, yorigin,
               imageSize.width() / dpr,
               imageSize.height() / dpr),
         m_image,
         QRect(0, 0, imageSize.width(), imageSize.height()));
}

void
ScoreWidget::showPage(int page)
{
    int pages = m_document->pageCount();
    
    if (page < 0 || page >= pages) {
        SVDEBUG << "ScoreWidget::showPage: Requested page " << page
                << " is outside range of " << pages << "-page document"
                << endl;
        return;
    }
    
    QSize mySize = contentsRect().size();
    QSizeF pageSize = m_document->pageSize(page);
    
    SVDEBUG << "ScoreWidget::showPage: Rendering page " << page
            << " of " << pages << " (my size = " << mySize.width()
            << " x " << mySize.height() << ", page size = " << pageSize.width()
            << " x " << pageSize.height() << ")" << endl;

    if (!mySize.width() || !mySize.height() ||
        !pageSize.width() || !pageSize.height()) {
        SVDEBUG << "ScoreWidget::showPage: one of these dimensions is zero, can't proceed" << endl;
        return;
    }

    float scale = std::min(mySize.width() / pageSize.width(),
                           mySize.height() / pageSize.height());
    QSize scaled(pageSize.width() * scale * devicePixelRatio(),
                 pageSize.height() * scale * devicePixelRatio());

    SVDEBUG << "ScoreWidget::showPage: Using scaled size "
            << scaled.width() << " x " << scaled.height() << endl;

    QImage rendered = m_document->render(page, scaled);

    QImage converted = rendered.convertToFormat(QImage::Format_ARGB32);
    QRgb *pixels = reinterpret_cast<QRgb *>(converted.bits());

    bool needsTransparency = false;
    size_t iw = converted.width(), ih = converted.height();
    
    // Check whether the image has white or white-ish opaque pixels;
    // if so we need to make them transparent. We do a quick single
    // scan along the diagonal to check for these:
    for (size_t i = 0; i < std::min(iw, ih); ++i) {
        QRgb pixel = pixels[i * iw + i];
        if (qAlpha(pixel) < 240) continue;
        if (qRed(pixel) > 127 && qGreen(pixel) > 127 && qBlue(pixel) > 127) {
            needsTransparency = true;
            break;
        }
    }
    if (needsTransparency) {
        for (size_t ix = 0; ix < iw * ih; ++ix) {
            QRgb pixel = pixels[ix];
            int r = qRed(pixel), g = qGreen(pixel), b = qBlue(pixel);
            int maxlevel = std::max(r, std::max(g, b));
            // 255 -> 0, but everything from 128 down -> 255
            int alpha = 255 - (maxlevel - 128) * 2;
            if (alpha < 0) alpha = 0;
            if (alpha > 255) alpha = 255;
            pixels[ix] = qRgba(r, g, b, std::min(qAlpha(pixel), alpha));
        }
    }
    m_image = converted;
    
    m_page = page;
    emit pageChanged(m_page);
    update();
}

void
ScoreWidget::setScorePosition(int position)
{
    m_scorePosition = position;
    
    if (m_scorePosition >= 0) {
        auto itr = m_elementsByPosition.lower_bound(m_scorePosition);
        if (itr == m_elementsByPosition.end()) {
#ifdef DEBUG_SCORE_WIDGET
            SVDEBUG << "ScoreWidget::setScorePosition: Position "
                    << m_scorePosition
                    << " does not have any corresponding element"
                    << endl;
#endif
        } else {
            ScoreElement elt = itr->second;
            if (elt.page != m_page) {
#ifdef DEBUG_SCORE_WIDGET
                SVDEBUG << "ScoreWidget::setScorePosition: Flipping to page "
                        << elt.page << endl;
#endif
                showPage(elt.page);
            }
        }
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
