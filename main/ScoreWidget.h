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

#include <QTemporaryDir>
#include <QFrame>

#include <map>

#include "piano-precision-aligner/Score.h"

class QSvgRenderer;
class QDomElement;

class ScoreWidget : public QFrame
{
    Q_OBJECT

public:
    /**
     * EventLabel is for labels derived from event position
     * information and given to us by MeasureInfo::toString().
     * Although typically in the form bar+beat/count, these
     * are opaque to us within this class and are only
     * compared, not parsed.
     */
    typedef std::string EventLabel;
    
    ScoreWidget(QWidget *parent);
    virtual ~ScoreWidget();
    
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
     * Return the current page number (0-based).
     */
    int getCurrentPage() const;

    /**
     * Return the total number of pages, or 0 if no score is loaded.
     */
    int getPageCount() const;

    /**
     * Return the start and end locations and labels of the current
     * selection, or empty labels if there is no constraint at either
     * end.
     */
    void getSelection(Fraction &start, EventLabel &startLabel,
                      Fraction &end, EventLabel &endLabel) const;
 
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
     * Set the current location (and by implication, event) to be
     * highlighted. The type of highlighting will depend on the
     * current interaction mode.
     */
    void setHighlightEventByLocation(Fraction location);

    /**
     * Set the current event (and by implication, location) to be
     * highlighted. The type of highlighting will depend on the
     * current interaction mode.
     */
    void setHighlightEventByLabel(EventLabel label);

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
    void interactionModeChanged(InteractionMode newMode);
    void scoreLocationHighlighted(Fraction, EventLabel, InteractionMode);
    void scoreLocationActivated(Fraction, EventLabel, InteractionMode);
    void interactionEnded(InteractionMode); // e.g. because mouse left widget

    /**
     * Emitted when the selected region of score changes. The start
     * and end are given using score locations. The toStartOfScore and
     * toEndOfScore flags are set if the start and/or end correspond
     * to the very start/end of the whole score, in which case the UI
     * may prefer to show the value using terms like "start" or "end"
     * rather than positional values.
     */
    void selectionChanged(Fraction start,
                          bool toStartOfScore,
                          EventLabel startLabel,
                          Fraction end,
                          bool toEndOfScore,
                          EventLabel endLabel);
    
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
    /**
     * EventId is for MEI-derived note IDs (or other MEI element IDs)
     * used within this class to identify specific elements. These are
     * not exposed in the API.
     */
    typedef QString EventId;
    
    QString m_scoreName;
    QString m_scoreFilename;
    QTemporaryDir m_tempDir;
    QString m_verovioResourcePath;
    std::vector<std::shared_ptr<QSvgRenderer>> m_svgPages;
    int m_page;

    Score::MusicalEventList m_musicalEvents;
    
    struct EventData {
        EventId id;
        int page;
        QRectF boxOnPage;
        Fraction location;
        EventLabel label;
        int indexInEvents;

        bool isNull() const { return id == ""; }
    };

    // MEI id-to-extent relations: these are generated from the SVG
    // XML when the score is loaded
    typedef std::pair<double, double> Extent;
    std::map<EventId, Extent> m_noteSystemExtentMap;

    // Relations between MEI IDs and musical events: these are
    // generated when the musical event data is set, after the score
    // has been loaded
    std::map<EventId, EventData> m_idDataMap;
    std::map<EventLabel, EventId> m_labelIdMap;
    std::map<int, std::vector<EventId>> m_pageEventsMap;

    InteractionMode m_mode;
    EventData m_eventUnderMouse;
    EventData m_eventToHighlight;
    EventData m_selectStart;
    EventData m_selectEnd;
    bool m_mouseActive;
    
    EventData getEventAtPoint(QPoint);

    EventData getEventWithId(EventId id) const;
    EventData getEventWithId(const std::string &id) const;
    EventData getEventWithLabel(EventLabel label) const;
    EventData getEventForMusicalEvent(const Score::MusicalEvent &) const;
    
    EventData getScoreStartEvent() const;
    EventData getScoreEndEvent() const;

    bool isSelectedFromStart() const;
    bool isSelectedToEnd() const;
    bool isSelectedAll() const;

    QRectF getHighlightRectFor(const EventData &);
    
    void findSystemExtents(QByteArray, std::shared_ptr<QSvgRenderer>);
    void assignExtentToNotesBelow(const QDomElement &, Extent);
    
    QTransform m_widgetToPage;
    QTransform m_pageToWidget;
};

#endif
