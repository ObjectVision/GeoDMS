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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();

private slots:
    void newLetter();
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
    void createDockWindows();

    SharedPtr<TreeItem> m_Root;
    SharedPtr<TreeItem> m_CurrentItem;

    ads::CDockManager* m_DockManager;
    ads::CDockAreaWidget* StatusDockArea;
    ads::CDockWidget* TimelineDockWidget;

    QPointer<QMdiArea> m_mdi_area;

    MyModel *m_table_view_model;
    QPointer<QTableView> m_table_view; // TODO: remove
    QPointer<QTextBrowser> m_detailpages;
    QPointer<QListWidget> m_eventlog;
    QPointer<QListWidget> m_treeview;

    QMenu *viewMenu;
};

#endif
