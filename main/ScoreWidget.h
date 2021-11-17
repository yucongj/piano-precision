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

#include <QFrame>
#include <QString>

class QPdfDocument;

class ScoreWidget : public QFrame
{
    Q_OBJECT

public:
    ScoreWidget(QWidget *parent);
    ~ScoreWidget();

    QString getCurrentScore() const;
    int getCurrentPage() const;
                  
public slots:
    void loadAScore(QString name);
    void showPage(int page);

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
};

#endif
