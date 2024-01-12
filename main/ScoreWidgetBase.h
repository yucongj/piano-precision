/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SCORE_WIDGET_BASE_H
#define SV_SCORE_WIDGET_BASE_H

#include "ScoreElement.h"

#include <QFrame>
#include <QString>

/**
 * Mode for mouse interaction.
 */
enum class ScoreInteractionMode {
    None,
    Navigate,
    Edit,
    SelectStart,
    SelectEnd
};

class ScoreWidgetBase : public QFrame
{
    Q_OBJECT

public:
    ScoreWidgetBase(QWidget *parent) : QFrame(parent) { }
    virtual ~ScoreWidgetBase() { }
    
    /** 
     * Load the named score. If loading fails, return false and set
     * the error string accordingly.
     */
    virtual bool loadAScore(QString name, QString &error) = 0;

    /** 
     * Set the page coord/position elements for the current score,
     * containing correspondences between coordinate and position in
     * time for the notes etc in the score.
     */
    virtual void setElements(ScoreElements elements) = 0;

    /** 
     * Return the current score name, or an empty string if none
     * loaded.
     */
    virtual QString getCurrentScore() const = 0;

    /**
     * Return the current page number.
     */
    virtual int getCurrentPage() const = 0;

    /**
     * Return the total number of pages, or 0 if no score is loaded.
     */
    virtual int getPageCount() const = 0;

    /**
     * Return the start and end score positions of the current
     * selection, or -1 if there is no constraint at either end.
     */
    virtual void getSelection(int &start, int &end) const = 0;

    /**
     * Return the current interaction mode.
     */
    virtual ScoreInteractionMode getInteractionMode() const = 0;
                                                 
public slots:
    /** 
     * Load the named score. If loading fails, emit the loadFailed
     * signal.
     */
    virtual void loadAScore(QString name) = 0;

    /**
     * Set the current page number and update the widget.
     */
    virtual void showPage(int page) = 0;

    /**
     * Set the current position to be highlighted, as the element on
     * the score closest to the given position in time, according to
     * the current set of score elements. The type of highlighting
     * will depend on the current interaction mode.
     */
    virtual void setScorePosition(int scorePosition) = 0;

    /**
     * Select an interaction mode.
     */
    virtual void setInteractionMode(ScoreInteractionMode mode) = 0;

    /**
     * Clear the selection back to the default (everything
     * selected). If a selection was present, also emit
     * selectionChanged.
     */
    virtual void clearSelection() = 0;

signals:
    void loadFailed(QString scoreName, QString errorMessage);
    void interactionModeChanged(ScoreInteractionMode newMode);
    void scorePositionHighlighted(int, ScoreInteractionMode);
    void scorePositionActivated(int, ScoreInteractionMode);
    void interactionEnded(ScoreInteractionMode); // e.g. because mouse left widget

    /**
     * Emitted when the selected region of score changes. The start
     * and end are given using score positions. The toStartOfScore and
     * toEndOfScore flags are set if the start and/or end correspond
     * to the very start/end of the whole score, in which case the UI
     * may prefer to show the value using terms like "start" or "end"
     * rather than positional values. The labels contain any label
     * found associated with the element at the given score position,
     * but may be empty.
     */
    void selectionChanged(int startPosition,
                          bool toStartOfScore,
                          QString startLabel,
                          int endPosition,
                          bool toEndOfScore,
                          QString endLabel);
    
    void pageChanged(int page);
};

#endif
