#ifndef DMSVIEWAREA_H
#define DMSVIEWAREA_H

#include <QMainWindow.h>
#include "ShvUtils.h"

struct TreeItem;
class DataView;

class QDmsViewArea : public QWidget
{
    Q_OBJECT
public:
    QDmsViewArea(QWidget* parent, void* hWndMain, TreeItem* viewContext, const TreeItem* currItem, ViewStyle viewStyle);
    ~QDmsViewArea();
    auto getDataView()->DataView* { return m_DataView;}
    auto getHwnd() -> void* { return m_HWnd; }

private:
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void UpdatePos();

    void*   m_HWnd = nullptr;
    DataView* m_DataView = nullptr;
    QPoint m_Pos = QPoint(100, 100);
    QSize m_Size = QSize(300, 200);
};

#endif // DMSVIEWAREA_H
