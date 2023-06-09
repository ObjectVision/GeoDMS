#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QLineEdit>
#include <QCompleter>
#include <QToolBar>
#include <QTextEdit>

#include "ptr/SharedPtr.h"
#include "ShvUtils.h"
#include "dataview.h"

#include "DockManager.h"
#include "DockAreaWidget.h"
#include "DockWidget.h"

QT_BEGIN_NAMESPACE
class QAction;
class QListWidget;
class QTreeWidget;
class QMenu;
class QTextEdit;
class QTextBrowser;
class QTreeView;
class QTableView;

class QMdiArea;
class QMdiSubWindow;
QT_END_NAMESPACE

struct TreeItem;
class DmsDetailPages;
class DmsTreeView;
class DmsModel;

class DmsCurrentItemBar : public QLineEdit
{
Q_OBJECT
public:
    using QLineEdit::QLineEdit;
    void setDmsCompleter();

public slots:
    void onEditingFinished();

};

enum class ButtonType
{
    SINGLE,
    SINGLE_EXCLUSIVE,
    MULTISTATE,
    MODAL
};

struct ToolbarButtonData
{
    ButtonType type;
    QString text;
    std::vector<ToolButtonID> ids;
    std::vector<QString> icons;
};

class DmsToolbuttonAction : public QAction
{
    Q_OBJECT
public:
    DmsToolbuttonAction(const QIcon& icon, const QString& text, QObject* parent = nullptr, ToolbarButtonData button_data = {});

public slots:
    void onToolbuttonPressed();

private:
    ToolbarButtonData m_data;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();
    auto getDmsModel() -> DmsModel* { return m_dms_model.get(); }
    auto getRootTreeItem() -> TreeItem* { return m_root; }
    auto getCurrentTreeItem() -> TreeItem* { return m_current_item; }
    void setCurrentTreeItem(TreeItem* new_current_item);
    auto getDmsTreeViewPtr() -> QPointer<DmsTreeView> { return m_treeview; }
    auto getDmsMdiAreaPtr() -> QMdiArea* { return m_mdi_area.get(); }
    auto getDefaultviewAction() -> QAction* { return m_defaultview_action.get(); };
    auto getTableviewAction() -> QAction* { return m_tableview_action.get(); };
    auto getMapviewAction() -> QAction* { return m_mapview_action.get(); };


    static MainWindow* TheOne();
    static void EventLog(SeverityTypeID st, CharPtr msg);

signals:
    void currentItemChanged();

public slots:
    void defaultView();
    void tableView();
    void mapView();
    void openConfigSource();

private slots:
    void fileOpen();
    void reOpen();
    void aboutGeoDms();
    void createView(ViewStyle viewStyle);

    void updateToolbar(QMdiSubWindow* active_mdi_subwindow);
    //void showTreeviewContextMenu(const QPoint& pos);

private:
    void LoadConfig(CharPtr filename);
    void setupDmsCallbacks();
    void createActions();
    void createStatusBar();
    void createDetailPagesDock();
    void createDetailPagesToolbar();
    void createDmsHelperWindowDocks();
    static void OnViewAction(const TreeItem* tiContext, CharPtr sAction, Int32 nCode, Int32 x, Int32 y, bool doAddHistory, bool isUrl, bool mustOpenDetailsPage);

    SharedStr m_currConfigFileName;
    SharedPtr<TreeItem> m_root;
    SharedPtr<TreeItem> m_current_item;

    // helper window docks
    QPointer<QDockWidget> m_detailpages_dock;

    // shared actions
    std::unique_ptr<QAction> m_defaultview_action;
    std::unique_ptr<QAction> m_tableview_action;
    std::unique_ptr<QAction> m_mapview_action;

    // unique application objects
    std::unique_ptr<QMdiArea> m_mdi_area;
    std::unique_ptr<DmsModel> m_dms_model;
    std::unique_ptr<DmsCurrentItemBar> m_current_item_bar;

    // helper windows
    QPointer<DmsDetailPages> m_detail_pages;
    QPointer<QListWidget> m_eventlog;
    QPointer<DmsTreeView> m_treeview;
    QPointer<QToolBar> m_toolbar;
};

#endif
