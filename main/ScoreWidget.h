/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    SV Piano Precision
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SCORE_WIDGET_MEI_H
#define SV_SCORE_WIDGET_MEI_H

#include <QTemporaryDir>
#include <QFrame>

#include <map>

#include "piano-precision-aligner/Score.h"

class QSvgRenderer;

class ScoreWidget : public QFrame
{
    Q_OBJECT

public:
    ScoreWidget(QWidget *parent);
    virtual ~ScoreWidget();

    /**
     * EventLabel is for bar-beat labels used within the GUI to
     * identify specific score events relative to the audio timeline.
     *
     * We intentionally use different string types for EventLabel and
     * EventId, partly because these are the types used by the classes
     * that generate or deal with them directly, but mostly to avoid
     * accidentally using one when the other is expected!
     */
    typedef std::string EventLabel;

    /**
     * EventId is for MEI-derived note IDs used within this class to
     * identify specific elements.
     */
    typedef QString EventId;
    
    /** 
     * Load the named score. If loading fails, return false and set
     * the error string accordingly.
     */
    bool loadAScore(QString name, QString &error);
    
    /** 
     * Set the musical event list for the current score, containing
     * (among other things) an ordered-by-metrical-time correspondence
     * between metrical time and score element ID.
     */
    void setMusicalEvents(const Score::MusicalEventList &musicalEvents);
    
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
     * Return the start and end score labels of the current selection,
     * or empty labels if there is no constraint at either end.
     */
    void getSelection(EventLabel &start, EventLabel &end) const;
 
    /**
     * Mode for mouse interaction.
     */
    enum class InteractionMode {
        None,
        Navigate,
        Edit,
        SelectStart,
        SelectEnd
    };

    /**
     * Return the current interaction mode.
     */
    InteractionMode getInteractionMode() const {
        return m_mode;
    }
                                                 
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
     * Set the current element/position to be highlighted. The type of
     * highlighting will depend on the current interaction mode.
     */
    void setHighlightEvent(EventLabel position);

    /**
     * Select an interaction mode.
     */
    void setInteractionMode(InteractionMode mode);

    /**
     * Clear the selection back to the default (everything
     * selected). If a selection was present, also emit
     * selectionChanged.
     */
    void clearSelection();

signals:
    void loadFailed(QString scoreName, QString errorMessage);
    void interactionModeChanged(ScoreWidget::InteractionMode newMode);
    void scorePositionHighlighted(EventLabel, ScoreWidget::InteractionMode);
    void scorePositionActivated(EventLabel, ScoreWidget::InteractionMode);
    void interactionEnded(ScoreWidget::InteractionMode); // e.g. because mouse left widget

    /**
     * Emitted when the selected region of score changes. The start
     * and end are given using score positions. The toStartOfScore and
     * toEndOfScore flags are set if the start and/or end correspond
     * to the very start/end of the whole score, in which case the UI
     * may prefer to show the value using terms like "start" or "end"
     * rather than positional values.
     */
    void selectionChanged(EventLabel start,
                          bool toStartOfScore,
                          EventLabel end,
                          bool toEndOfScore);
    
    void pageChanged(int page);

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
    QTemporaryDir m_tempDir;
    QString m_verovioResourcePath;
    std::vector<std::shared_ptr<QSvgRenderer>> m_svgPages;
    int m_page;

    InteractionMode m_mode;
    EventId m_idUnderMouse;
    EventId m_idToHighlight;
    EventLabel m_selectStartLabel;
    EventLabel m_selectEndLabel;
    bool m_mouseActive;

    EventLabel getScoreStartLabel() const;
    bool isSelectedFromStart() const;

    EventLabel getScoreEndLabel() const;
    bool isSelectedToEnd() const;

    bool isSelectedAll() const;
    
    EventId idAtPoint(QPoint);
    QRectF rectForId(EventId id);
    
    Score::MusicalEventList m_musicalEvents;
    struct EventData {
        int page;
        QRectF locationOnPage;
        EventLabel label;
        int indexInEvents;
    };
    std::map<EventId, EventData> m_idDataMap;
    std::map<int, std::vector<EventId>> m_pageEventsMap;
    std::map<EventLabel, EventId> m_labelIdMap;
    QTransform m_widgetToPage;
    QTransform m_pageToWidget;
};

#endif
