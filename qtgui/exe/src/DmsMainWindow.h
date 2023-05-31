#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include "ptr/SharedPtr.h"

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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();
    auto getCurrentTreeitem() -> TreeItem* { return m_current_item; } ;
    void setCurrentTreeitem(TreeItem* new_current_item);

    static MainWindow* TheOne();
    static void EventLog(SeverityTypeID st, CharPtr msg);

signals:
    void currentItemChanged();

private slots:
    void fileOpen();
    void aboutGeoDms();
    void defaultView();

private:
    void LoadConfig(CharPtr filename);
    void setupDmsCallbacks();
    void createActions();
    void createStatusBar();

    void createDetailPagesDock();
    void createDetailPagesToolbar();
    void createDmsHelperWindowDocks();

    SharedPtr<TreeItem> m_root;
    SharedPtr<TreeItem> m_current_item;

    // helper window docks
    QPointer<QDockWidget> m_detailpages_dock;

    // advanced central widget dock
    ads::CDockManager* m_DockManager;
    ads::CDockAreaWidget* StatusDockArea;
    ads::CDockWidget* TimelineDockWidget;
    ads::CDockAreaWidget* centralDockArea;

    QPointer<DmsDetailPages> m_detail_pages;
    //QPointer<QTextBrowser> m_detailpages;
    QPointer<QListWidget> m_eventlog;
    QPointer<QTreeView> m_treeview;
};

#endif
