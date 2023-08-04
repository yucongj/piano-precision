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

static QColor positionHighlightColour("#59c4df");
static QColor mouseTrackingHighlightColour("#ffbd00");

using std::vector;
using std::pair;
using std::string;

ScoreWidget::ScoreWidget(QWidget *parent) :
    QFrame(parent),
    m_document(new QPdfDocument(this)),
    m_page(-1),
    m_highlightPosition(-1),
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
    m_mousePosition = -1;
    update();
}

void
ScoreWidget::leaveEvent(QEvent *)
{
    m_mouseActive = false;
    update();
}

void
ScoreWidget::mouseMoveEvent(QMouseEvent *e)
{
    m_mousePosition = positionForPoint(e->pos());
    update();
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

    ScoreElement elt = itr->second;

#ifdef DEBUG_SCORE_WIDGET
    SVDEBUG << "ScoreWidget::rectForPosition: Position "
            << pos << " has corresponding element id="
            << elt.id << " on page=" << elt.page << " with x="
            << elt.x << ", y=" << elt.y << ", sy=" << elt.sy << endl;
#endif
            
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

    const vector<pair<QRectF, QColor>> highlights {
        { rectForPosition(m_highlightPosition), positionHighlightColour },
        { rectForPosition(m_mousePosition), mouseTrackingHighlightColour }
    };

    for (const auto &h : highlights) {
        auto rect = h.first;
        if (rect.isNull()) continue;
        QColor colour(h.second);
        colour.setAlpha(160);
        paint.setPen(Qt::NoPen);
        paint.setBrush(colour);
#ifdef DEBUG_SCORE_WIDGET
        SVDEBUG << "ScoreWidget::paint: highlighting rect with origin "
                << rect.x() << "," << rect.y() << " and size "
                << rect.width() << "x" << rect.height()
                << " using colour " << colour.name() << endl;
#endif
        paint.drawRect(rect);
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

    m_image = m_document->render(page, scaled);
    m_page = page;
    update();
}

void
ScoreWidget::highlightPosition(int position)
{
    m_highlightPosition = position;
    
    if (m_highlightPosition >= 0) {
        auto itr = m_elementsByPosition.lower_bound(m_highlightPosition);
        if (itr == m_elementsByPosition.end()) {
#ifdef DEBUG_SCORE_WIDGET
            SVDEBUG << "ScoreWidget::highlightPosition: Highlight position "
                    << m_highlightPosition
                    << " does not have any corresponding element"
                    << endl;
#endif
        } else {
            ScoreElement elt = itr->second;
            if (elt.page != m_page) {
#ifdef DEBUG_SCORE_WIDGET
                SVDEBUG << "ScoreWidget::highlightPosition: Flipping to page "
                        << elt.page << endl;
#endif
                showPage(elt.page);
            }
        }
    }
            
    update();
}

void
ScoreWidget::removeHighlight()
{
    m_highlightPosition = -1;
    update();
}

