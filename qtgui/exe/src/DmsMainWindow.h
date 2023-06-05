#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QLineEdit>
#include <QCompleter>
#include <QToolBar>
//#include <QPoint>
#include "ptr/SharedPtr.h"
#include "ShvUtils.h"
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

class DmsCurrentItemBar : public QLineEdit
{
public:
    using QLineEdit::QLineEdit;
    void setDmsCompleter(TreeItem* root);
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

signals:


private slots:


private:
    
    ToolbarButtonData m_data;
        
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();
    auto getCurrentTreeItem() -> TreeItem* { return m_current_item; } ;
    void setCurrentTreeItem(TreeItem* new_current_item);

    static MainWindow* TheOne();
    static void EventLog(SeverityTypeID st, CharPtr msg);

signals:
    void currentItemChanged();

private slots:
    void fileOpen();
    void reOpen();
    void aboutGeoDms();
    void defaultView();
    //void tableView();
    //void mapView();
    void updateToolbar(int index);
    void showTreeviewContextMenu(const QPoint& pos);

private:
    void LoadConfig(CharPtr filename);
    void setupDmsCallbacks();
    void createActions();
    void createStatusBar();
    void createDetailPagesDock();
    void createDetailPagesToolbar();
    void createDmsHelperWindowDocks();

    SharedStr m_currConfigFileName;
    SharedPtr<TreeItem> m_root;
    SharedPtr<TreeItem> m_current_item;

    // helper window docks
    QPointer<QDockWidget> m_detailpages_dock;

    // advanced central widget dock
    ads::CDockManager* m_DockManager;
    ads::CDockAreaWidget* StatusDockArea;
    ads::CDockWidget* TimelineDockWidget;
    ads::CDockAreaWidget* centralDockArea;

    //toolbars
    QPointer<DmsCurrentItemBar> m_current_item_bar;

    // helper windows
    QPointer<DmsDetailPages> m_detail_pages;
    QPointer<QListWidget> m_eventlog;
    QPointer<DmsTreeView> m_treeview;
    QPointer<QToolBar> m_toolbar;

    // (context) menus
    QPointer<QMenu> m_treeview_context_menu;

};

#endif
