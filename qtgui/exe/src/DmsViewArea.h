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

    void on_rescale();

//protected:
//    void paintEvent(QPaintEvent* event);

private:
    void CreateDmsView(QMdiArea* parent);
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    auto contentsRectInPixelUnits() -> QRect;

    DataView* m_DataView = nullptr;
    void* m_DataViewHWnd = nullptr;
    DWORD m_cookie = 0; // used for RegisterScaleChangeNotifications
    DPoint m_LastScaleFactors;
};

#endif // DMSVIEWAREA_H
