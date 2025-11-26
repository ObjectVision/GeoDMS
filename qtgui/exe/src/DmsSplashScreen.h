// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(SPLASHSCREEN_H)
#define SPLASHSCREEN_H

#include "RtcInterface.h"

#include <QSplashScreen>

class DmsSplashScreen : public QSplashScreen {
public:
    DmsSplashScreen(const QPixmap& pixmap)
        : QSplashScreen(pixmap) {}
    ~DmsSplashScreen() {}

    virtual void drawContents(QPainter* painter);

    void setMessageRect(QRect rect, int alignment = Qt::AlignLeft);

    QString m_message = DMS_GetVersion();
    int     m_alignment = Qt::AlignCenter;
    QColor  m_color;
    QRect   m_rect;
};

auto showSplashScreen() -> std::unique_ptr<DmsSplashScreen>;

#endif //defined(SPLASHSCREEN_H)
