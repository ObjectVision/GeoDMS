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
    Q_OBJECT
public:
    QDmsMdiArea(QWidget* parent);
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeAllButActiveSubWindow();
    void setTabbedViewModeStyle();
    QSize sizeHint() const override;


public slots:
    void onCascadeSubWindows();
    void onTileSubWindows();
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
    void closeEvent(QCloseEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    auto getDataView() -> DataView* { return m_DataView; }
    auto getDataViewHwnd() -> void* { return m_DataViewHWnd; } // QEvent::WinIdChange
    void UpdatePosAndSize();

    void on_rescale();

public slots:
    void onWindowStateChanged(Qt::WindowStates oldState, Qt::WindowStates newState);

private:
    void CreateDmsView(QMdiArea* parent, ViewStyle viewStyle);
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    auto contentsRectInPixelUnits() -> QRect;
    //bool eventFilter(QObject* object, QEvent* event) override;
    void keyPressEvent(QKeyEvent* keyEvent) override;

    DataView* m_DataView = nullptr;
    void* m_DataViewHWnd = nullptr;
    DWORD m_cookie = 0; // used for RegisterScaleChangeNotifications
    DPoint m_LastScaleFactors;
};

#endif // DMSVIEWAREA_H
