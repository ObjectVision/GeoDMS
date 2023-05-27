#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include "TreeItem.h"
#include "ptr/SharedPtr.h"

#include "DockManager.h"
#include "DockAreaWidget.h"
#include "DockWidget.h"

QT_BEGIN_NAMESPACE
class QAction;
class QListWidget;
class QMenu;
class QTextEdit;
class QTextBrowser;
class QTreeView;
class QTableView;
class MyModel; // TODO: remove from namespace

class QMdiArea;
class QMdiSubWindow;
QT_END_NAMESPACE

class DmsDetailPages;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();
    auto getCurrentTreeitem() -> TreeItem* { return m_current_item; } ;
    void setCurrentTreeitem(TreeItem* new_current_item);

signals:
    void currentItemChanged();

private slots:
    void fileOpen();
    void save();
    void print();
    void undo();
    void about();
    void insertCustomer(const QString &customer);
    void addParagraph(const QString &paragraph);

private:
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


    MyModel *m_table_view_model; // TODO: remove
    QPointer<QTableView> m_table_view; // TODO: remove
    QPointer<DmsDetailPages> m_detail_pages;
    //QPointer<QTextBrowser> m_detailpages;
    QPointer<QListWidget> m_eventlog;
    QPointer<QListWidget> m_treeview;

    QMenu *viewMenu;
};

#endif
