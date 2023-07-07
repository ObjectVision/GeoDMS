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

    void closeAllButActiveSubWindow();
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
    auto getDataViewHwnd() -> void* { return m_DataViewHWnd; } // QEvent::WinIdChange
    void UpdatePosAndSize();

protected:
    void paintEvent(QPaintEvent* event);

private:
    auto contentsRectInPixelUnits() -> QRect;
    void CreateDmsView(QMdiArea* parent);
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    DataView* m_DataView = nullptr;
    void* m_DataViewHWnd = nullptr;
    DWORD m_cookie = 0; // used for RegisterScaleChangeNotifications
};

#endif // DMSVIEWAREA_H
