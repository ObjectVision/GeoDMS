#ifndef DMSVIEWAREA_H
#define DMSVIEWAREA_H

#include <QMainWindow.h>
#include "ShvUtils.h"

struct TreeItem;
class DataView;
using HANDLE = void*;

class QDmsViewArea : public QWidget
{
    Q_OBJECT
public:
    QDmsViewArea(QWidget* parent, HANDLE hWndMain, TreeItem* viewContext, TreeItem* currItem);
    ~QDmsViewArea();
    auto getDataView()->DataView* { return dv;}

private slots:

private:
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void UpdatePos();

    HANDLE wnd = nullptr;
    DataView* dv = nullptr;
    QPoint pos = QPoint(100, 100);
    QSize size = QSize(300, 200);
};

#endif // DMSVIEWAREA_H
