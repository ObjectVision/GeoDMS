#ifndef DMSVIEWAREA_H
#define DMSVIEWAREA_H

#include "ShvUtils.h"

#include <QMdiSubWindow.h>

struct TreeItem;
class DataView;

class QDmsViewArea : public QMdiSubWindow
{
    Q_OBJECT

    using base_class = QMdiSubWindow;

public:
    QDmsViewArea(QWidget* parent);
    //auto getDataView()->DataView* { return m_DataView;}
    //auto getHwnd() -> void* { return m_HWnd; }

private:
    //void moveEvent(QMoveEvent* event) override;
    //void resizeEvent(QResizeEvent* event) override;
    //void UpdatePos();

    //void*   m_HWnd = nullptr;
    //DataView* m_DataView = nullptr;
    //QPoint m_Pos = QPoint(100, 100);
    //QSize m_Size = QSize(300, 200);
};

class DmsViewWidget : public QWidget
{
    Q_OBJECT
public:
    DmsViewWidget(QWidget* parent, void* hWndMain, TreeItem* viewContext, const TreeItem* currItem, ViewStyle viewStyle);
    ~DmsViewWidget();
    auto getDataView() -> DataView* { return m_DataView; }
    auto getHwnd() -> void* { return m_HWnd; } // QEvent::WinIdChange

private:
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void UpdatePosAndSize();

    void* m_HWnd = nullptr;
    DataView* m_DataView = nullptr;
    QPoint m_Pos = QPoint(100, 100);
    QSize m_Size = QSize(300, 200);
};

#endif // DMSVIEWAREA_H
