#include "DmsSplashScreen.h"

#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <qfontdatabase.h>

void DmsSplashScreen::drawContents(QPainter* painter) {
    QPixmap textPix = QSplashScreen::pixmap();
    painter->setPen(this->m_color);
    painter->drawText(this->m_rect, this->m_alignment, this->m_message);
}

void DmsSplashScreen::showStatusMessage(const QString& message, const QColor& color) 
{
    this->m_message = message;
    this->m_color = color;
    this->showMessage(this->m_message, this->m_alignment, this->m_color);
}

void DmsSplashScreen::setMessageRect(QRect rect, int alignment) 
{
    this->m_rect = rect;
    this->m_alignment = alignment;
}

auto showSplashScreen() -> std::unique_ptr<DmsSplashScreen>
{
    int id = QFontDatabase::addApplicationFont(":/res/fonts/dmstext.ttf");
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    QFont dms_text_font(family, 25);

//    QPixmap pixmap(":/res/images/HackedWorld.jpg");
    QPixmap pixmap(":/res/images/WorldHeatMap.jpg");
    std::unique_ptr<DmsSplashScreen> splash = std::make_unique<DmsSplashScreen>(pixmap);
    splash->setMessageRect(QRect(splash->rect().topLeft(), QSize(1024, 200)), Qt::AlignCenter);
    dms_text_font.setBold(true);
    splash->setFont(dms_text_font);

    auto app = dynamic_cast<QApplication*>(QCoreApplication::instance());
    assert(app);
    auto screen_at_mouse_pos = app->screenAt(QCursor::pos());
    const QPoint currentDesktopsCenter = screen_at_mouse_pos->geometry().center();
    assert(splash->rect().top() == 0);
    assert(splash->rect().left() == 0);
    auto projectedTopLeft = currentDesktopsCenter - splash->rect().center();
    if (projectedTopLeft.y() < screen_at_mouse_pos->geometry().top())
        projectedTopLeft.setY(screen_at_mouse_pos->geometry().top());
    splash->move(projectedTopLeft);
    splash->show();
    return splash;
}
