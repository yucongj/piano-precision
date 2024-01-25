/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SCORE_WIDGET_PDF_H
#define SV_SCORE_WIDGET_PDF_H

#include "ScoreWidgetBase.h"
#include <map>

class QPdfDocument;
    
class ScoreWidgetPDF : public ScoreWidgetBase
{
    Q_OBJECT

public:
    ScoreWidgetPDF(QWidget *parent);
    virtual ~ScoreWidgetPDF();
    
    /** 
     * Load the named score. If loading fails, return false and set
     * the error string accordingly.
     */
    bool loadAScore(QString name, QString &error) override;

    /** 
     * Set the page coord/position elements for the current score,
     * containing correspondences between coordinate and position in
     * time for the notes etc in the score.
     */
    void setElements(const ScoreElements &elements) override;

    bool requiresElements() const override { return true; }
    
    /** 
     * Set the musical event list for the current score, containing
     * (among other things) an ordered-by-metrical-time correspondence
     * between metrical time and score element ID.
     */
    void setMusicalEvents(const Score::MusicalEventList &musicalEvents) override;
    
    /** 
     * Return the current score name, or an empty string if none
     * loaded.
     */
    QString getCurrentScore() const override;

    /**
     * Return the current page number.
     */
    int getCurrentPage() const override;

    /**
     * Return the total number of pages, or 0 if no score is loaded.
     */
    int getPageCount() const override;

    /**
     * Return the start and end score positions of the current
     * selection, or -1 if there is no constraint at either end.
     */
    void getSelection(int &start, int &end) const override;

    /**
     * Return the current interaction mode.
     */
    ScoreInteractionMode getInteractionMode() const override {
        return m_mode;
    }
                                                 
public slots:
    /** 
     * Load the named score. If loading fails, emit the loadFailed
     * signal.
     */
    void loadAScore(QString name) override;

    /**
     * Set the current page number and update the widget.
     */
    void showPage(int page) override;

    /**
     * Set the current position to be highlighted, as the element on
     * the score closest to the given position in time, according to
     * the current set of score elements. The type of highlighting
     * will depend on the current interaction mode.
     */
    void setScorePosition(int scorePosition) override;

    /**
     * Set the current element to be highlighted. The type of
     * highlighting will depend on the current interaction mode.
     */
    void setScoreHighlightEvent(QString) override { }

    /**
     * Select an interaction mode.
     */
    void setInteractionMode(ScoreInteractionMode mode) override;

    /**
     * Clear the selection back to the default (everything
     * selected). If a selection was present, also emit
     * selectionChanged.
     */
    void clearSelection() override;

protected:
    void resizeEvent(QResizeEvent *) override;
    void enterEvent(QEnterEvent *) override;
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

    ScoreInteractionMode m_mode;
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
    QString labelForPosition(int pos);
    
    ScoreElements m_elements;
    typedef std::multimap<int, ScoreElement> PositionElementMap;
    PositionElementMap m_elementsByPosition;
};

#endif
