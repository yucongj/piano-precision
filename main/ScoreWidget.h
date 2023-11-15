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

    /**
     * Return the total number of pages, or 0 if no score is loaded.
     */
    int getPageCount() const;

    /**
     * Return the start and end score positions of the current
     * selection, or -1 if there is no constraint at either end.
     */
    void getSelection(int &start, int &end) const;
    
    /**
     * Mode for mouse interaction.
     */
    enum class InteractionMode { None, Navigate, Edit, SelectStart, SelectEnd };

    /**
     * Return the current interaction mode.
     */
    InteractionMode getInteractionMode() const { return m_mode; }
                                                 
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
     * Set the current position to be highlighted, as the element on
     * the score closest to the given position in time, according to
     * the current set of score elements. The type of highlighting
     * will depend on the current interaction mode.
     */
    void setScorePosition(int scorePosition);

    /**
     * Select an interaction mode.
     */
    void setInteractionMode(InteractionMode mode);

signals:
    void loadFailed(QString scoreName, QString errorMessage);
    void interactionModeChanged(ScoreWidget::InteractionMode newMode);
    void scorePositionHighlighted(int, ScoreWidget::InteractionMode);
    void scorePositionActivated(int, ScoreWidget::InteractionMode);
    void interactionEnded(ScoreWidget::InteractionMode); // e.g. because mouse left widget

    /**
     * Emitted when the selected region of score changes. The start
     * and end are given using score positions. The toStartOfScore and
     * toEndOfScore flags are set if the start and/or end correspond
     * to the very start/end of the whole score, in which case the UI
     * may prefer to show the value using terms like "start" or "end"
     * rather than positional values.
     */
    void selectionChanged(int startPosition, bool toStartOfScore,
                          int endPosition, bool toEndOfScore);
    
    void pageChanged(int page);

protected:
    void resizeEvent(QResizeEvent *) override;
    void enterEvent(QEvent *) override;
    void leaveEvent(QEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void paintEvent(QPaintEvent *) override;
    
private:
    QString m_scoreName;
    QString m_scoreFilename;
    QPdfDocument *m_document;
    int m_page;
    QImage m_image;

    InteractionMode m_mode;
    int m_scorePosition;
    int m_mousePosition;
    int m_selectStartPosition;
    int m_selectEndPosition;
    bool m_mouseActive;

    int getStartPosition() const;
    bool isSelectedFromStart() const;

    int getEndPosition() const;
    bool isSelectedToEnd() const;

    bool isSelectedAll() const;
    
    QRectF rectForPosition(int pos);
    QRectF rectForElement(const ScoreElement &elt);
    int positionForPoint(QPoint point);
    
    ScoreElements m_elements;
    typedef std::multimap<int, ScoreElement> PositionElementMap;
    PositionElementMap m_elementsByPosition;
};

#endif
