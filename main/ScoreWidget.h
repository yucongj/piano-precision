/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SCORE_WIDGET_H
#define SV_SCORE_WIDGET_H

#include "ScoreElement.h"

#include <QFrame>
#include <QString>

#include <map>

class QPdfDocument;

class ScoreWidget : public QFrame
{
    Q_OBJECT

public:
    ScoreWidget(QWidget *parent);
    ~ScoreWidget();

    /** 
     * Load the named score. If loading fails, return false and set
     * the error string accordingly.
     */
    bool loadAScore(QString name, QString &error);

    /** 
     * Set the page coord/position elements for the current score,
     * containing correspondences between coordinate and position in
     * time for the notes etc in the score.
     */
    void setElements(ScoreElements elements);

    /** 
     * Return the current score name, or an empty string if none
     * loaded.
     */
    QString getCurrentScore() const;

    /**
     * Return the current page number.
     */
    int getCurrentPage() const;
                  
public slots:
    /** 
     * Load the named score. If loading fails, emit the loadFailed
     * signal.
     */
    void loadAScore(QString name);

    /**
     * Set the current page number and update the widget.
     */
    void showPage(int page);

    /**
     * Highlight the element on the score closest to the given
     * position in time, according to the current set of score
     * elements.
     */
    void highlightPosition(int scorePosition);

    /**
     * Remove any highlight set with highlightPosition.
     */
    void removeHighlight();

signals:
    void loadFailed(QString scoreName, QString errorMessage);

protected:
    void resizeEvent(QResizeEvent *);
    void paintEvent(QPaintEvent *);
    
private:
    QString m_scoreName;
    QString m_scoreFilename;
    QPdfDocument *m_document;
    int m_page;
    QImage m_image;
    int m_highlight;

    ScoreElements m_elements;
    std::multimap<int, ScoreElement> m_elementsByPosition;
};

#endif
