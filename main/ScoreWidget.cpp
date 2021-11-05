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

#include "base/Debug.h"
#include "base/ResourceFinder.h"

ScoreWidget::ScoreWidget(QWidget *parent) :
    QLabel(parent),
    m_document(new QPdfDocument(this)),
    m_page(-1)
{
    setFrameStyle(Panel | Plain);
    setAlignment(Qt::AlignCenter);
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
ScoreWidget::loadAScore(QString name)
{
    SVDEBUG << "ScoreWidget::loadAScore: Score \"" << name << "\" requested"
            << endl;

    QString filename = ResourceFinder().getResourcePath("scores", name + ".pdf");
    if (filename == "") {
        SVDEBUG << "ScoreWidget::loadAScore: Unable to find a suitable "
                << "resource location for score file" << endl;
        SVDEBUG << "ScoreWidget::loadAScore: Expected directory location is: \""
                << ResourceFinder().getResourceSaveDir("scores") << "\"" << endl;
        emit loadFailed(name, tr("Failed to load score %1: Score file or resource path not found").arg(name));
        return;
    }
        
    auto result = m_document->load(filename);
    
    SVDEBUG << "ScoreWidget::loadAScore: Asked to load pdf file \""
            << filename << "\" for score \"" << name
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
        m_scoreName = name;
        m_scoreFilename = filename;
        SVDEBUG << "ScoreWidget::loadAScore: Load successful, showing first page"
                << endl;
        showPage(0);
    } else {
        emit loadFailed(name, tr("Failed to load score %1: %2")
                        .arg(name).arg(error));
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
    QSize scaled(pageSize.width() * scale, pageSize.height() * scale);

    SVDEBUG << "ScoreWidget::showPage: Using scaled size "
            << scaled.width() << " x " << scaled.height() << endl;

    setPixmap(QPixmap::fromImage(m_document->render(page, scaled)));
    m_page = page;
}

