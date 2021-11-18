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

#include <QPdfDocument>
#include <QPainter>

#include "base/Debug.h"
#include "base/ResourceFinder.h"

ScoreWidget::ScoreWidget(QWidget *parent) :
    QFrame(parent),
    m_document(new QPdfDocument(this)),
    m_page(-1),
    m_highlight(-1)
{
    setFrameStyle(Panel | Plain);
    setMinimumSize(QSize(100, 100));
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
    SVDEBUG << "ScoreWidget::loadAScore: Score \"" << scoreName
            << "\" requested" << endl;

    QString filebase = scoreName + ".pdf";
    QString filename = ResourceFinder().getResourcePath("scores", filebase);
    if (filename == "") {
        SVDEBUG << "ScoreWidget::loadAScore: Unable to find a suitable "
                << "resource location for score file " << filebase << endl;
        SVDEBUG << "ScoreWidget::loadAScore: Expected directory location is: \""
                << ResourceFinder().getResourceSaveDir("scores")
                << "\"" << endl;
        emit loadFailed(scoreName, tr("Failed to load score %1: Score file or resource path not found").arg(scoreName));
        return;
    }
        
    auto result = m_document->load(filename);
    
    SVDEBUG << "ScoreWidget::loadAScore: Asked to load pdf file \""
            << filename << "\" for score \"" << scoreName
            << "\", result is " << result << endl;

    QString error;
    switch (result) {
    case QPdfDocument::NoError:
        break;
    case QPdfDocument::FileNotFoundError:
        error = tr("File not found");
        break;
    case QPdfDocument::InvalidFileFormatError:
        error = tr("File is in the wrong format");
        break;
    case QPdfDocument::IncorrectPasswordError:
        error = tr("File is password-protected");
        break;
    default:
        error = tr("Unable to read PDF");
    }

    if (error == "") {
        m_scoreName = scoreName;
        m_scoreFilename = filename;
        SVDEBUG << "ScoreWidget::loadAScore: Load successful, showing first page"
                << endl;
        showPage(0);
    } else {
        emit loadFailed(scoreName, tr("Failed to load score %1: %2")
                        .arg(scoreName).arg(error));
    }
}

void
ScoreWidget::setElements(ScoreElements elements)
{
    m_elements = elements;

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
    
    paint.drawImage
        (QRect(xorigin, yorigin,
               imageSize.width() / dpr,
               imageSize.height() / dpr),
         m_image,
         QRect(0, 0, imageSize.width(), imageSize.height()));

    if (m_highlight < 0) {
        SVDEBUG << "ScoreWidget::paintEvent: No highlight specified" << endl;
    } else {
        auto itr = m_elementsByPosition.lower_bound(m_highlight);
        if (itr == m_elementsByPosition.end()) {
            SVDEBUG << "ScoreWidget::paintEvent: Highlight position "
                    << m_highlight << " does not have any corresponding element"
                    << endl;
        } else {
            
            // just highlight the first for now...

            // Now, we know the units are a bit mad for these values.
            // I think(?) they are in inches * dpi * constant where
            // dpi = 360 and constant = 12, so inches * 4320 or mm *
            // 170.08 approx.

            // To map these correctly we will need to know the page
            // size at which the pdf was rendered - since the defaults
            // are presumably locale dependent (I'm guessing US Letter
            // in US, A4 everywhere else). This info must be in the
            // PDF one way or another, can QPdfDocument tell us it?

            // Check that later, but for the mo let's hard code A4 and
            // see what comes out. So 297x210mm which would make the
            // whole page 50513.4 units tall and 35716.5 wide. Thus
            // our y ratio will be (pixel_height)/51513.4 and x ratio
            // (pixel_width)/35716.5. Oh, and that's times the device
            // pixel ratio.

            ScoreElement elt = itr->second;

            SVDEBUG << "ScoreWidget::paintEvent: Highlight position "
                    << m_highlight << " has corresponding element id="
                    << elt.id << " on page=" << elt.page << " with x="
                    << elt.x << ", y=" << elt.y << ", sy=" << elt.sy << endl;
            
            double xratio = double(imageSize.width()) / (35716.5 * dpr);
            double yratio = double(imageSize.height()) / (51513.4 * dpr);

            // We don't get a valid element width - hardcode &
            // reconsider this later
            double fakeWidth = 500.0;
            
            QRectF rect(xorigin + elt.x * xratio,
                        yorigin + elt.y * yratio,
                        fakeWidth * xratio,
                        elt.sy * yratio);
            QColor colour("#59c4df");
            colour.setAlpha(160);
            paint.setPen(Qt::NoPen);
            paint.setBrush(colour);

            SVDEBUG << "ScoreWidget::paint: highlighting rect with origin "
                    << rect.x() << "," << rect.y() << " and size "
                    << rect.width() << "x" << rect.height() << endl;
            
            paint.drawRect(rect);
        }
    }
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
    m_highlight = position;
    update();
}

void
ScoreWidget::removeHighlight()
{
    m_highlight = -1;
    update();
}

