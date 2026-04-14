#include "DmsSplashScreen.h"

#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QFontDatabase>

void DmsSplashScreen::drawContents(QPainter* painter) {
    QPixmap textPix = QSplashScreen::pixmap();
    painter->setPen(this->m_color);
    painter->drawText(this->m_rect, this->m_alignment, this->m_message);
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

    QPixmap pixmap(":/res/images/WorldHeatMap.jpg");

    auto app = dynamic_cast<QApplication*>(QCoreApplication::instance());
    assert(app);
    auto screen_at_mouse_pos = app->screenAt(QCursor::pos());
    QScreen* screen = screen_at_mouse_pos ? screen_at_mouse_pos : app->primaryScreen();
    const QRect screenGeom = screen->availableGeometry(); // use availableGeometry to avoid taskbar
    const double scaleFactor = 0.75;

    // target size = 75% of current screen, keep pixmap aspect ratio
    const QSize targetSize(qRound(screenGeom.width() * scaleFactor),
        qRound(screenGeom.height() * scaleFactor));
    QPixmap scaledPixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    std::unique_ptr<DmsSplashScreen> splash = std::make_unique<DmsSplashScreen>(scaledPixmap);

    // place message rect relative to splash size (bottom band)
    const QRect srect = splash->rect();
    const QSize msgSize(srect.width(), qRound(srect.height() * 0.15)); // 15% of splash height
    splash->setMessageRect(QRect(QPoint(srect.left(), srect.bottom() - msgSize.height()), msgSize), Qt::AlignCenter);

    dms_text_font.setBold(true);
    splash->setFont(dms_text_font);
    splash->m_color = QColor(255, 255, 255);

    // position splash centered on the screen containing the mouse, but make sure it stays on-screen
    const QPoint screenCenter = screenGeom.center();
    auto projectedTopLeft = screenCenter - splash->rect().center();
    if (projectedTopLeft.y() < screenGeom.top())
        projectedTopLeft.setY(screenGeom.top());
    if (projectedTopLeft.x() < screenGeom.left())
        projectedTopLeft.setX(screenGeom.left());
    // ensure splash fully visible horizontally and vertically
    if (projectedTopLeft.x() + splash->rect().width() > screenGeom.right() + 1)
        projectedTopLeft.setX(screenGeom.right() + 1 - splash->rect().width());
    if (projectedTopLeft.y() + splash->rect().height() > screenGeom.bottom() + 1)
        projectedTopLeft.setY(screenGeom.bottom() + 1 - splash->rect().height());

    splash->move(projectedTopLeft);
    splash->show();
    return splash;
}
