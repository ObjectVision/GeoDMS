#ifndef DMSVIEWAREA_H
#define DMSVIEWAREA_H

#include "ShvUtils.h"

#include <QMdiArea.h>
#include <QMdiSubWindow.h>

struct TreeItem;
class DataView;
struct MdiCreateStruct;

class QDmsMdiArea : public QMdiArea
{
public:
    QDmsMdiArea(QWidget* parent);
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};

class QDmsViewArea : public QMdiSubWindow
{
    Q_OBJECT
        using base_class = QMdiSubWindow;

public:
    QDmsViewArea(QMdiArea* parent, TreeItem* viewContext, const TreeItem* currItem, ViewStyle viewStyle);
    QDmsViewArea(QMdiArea* parent, MdiCreateStruct* createStruct);
    ~QDmsViewArea();

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    auto getDataView() -> DataView* { return m_DataView; }
    auto getHwnd() -> void* { return m_HWnd; } // QEvent::WinIdChange

private:
    auto contentsRectInPixelUnits() -> QRect;
    void CreateDmsView(QMdiArea* parent);
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void UpdatePosAndSize();

    void* m_HWnd = nullptr;
    DataView* m_DataView = nullptr;
};

#endif // DMSVIEWAREA_H
