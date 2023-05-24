#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>

QT_BEGIN_NAMESPACE
class QAction;
class QListWidget;
class QMenu;
class QTextEdit;
class QTextBrowser;
class QTreeView;
class TreeModel;
class QTableView;
class MyModel; // TODO: remove from namespace
QT_END_NAMESPACE

//! [0]
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();

private slots:
    void newLetter();
    void save();
    void print();
    void undo();
    void about();
    void insertCustomer(const QString &customer);
    void addParagraph(const QString &paragraph);

private:
    void createActions();
    void createStatusBar();
    void createDockWindows();

    MyModel *m_table_view_model;
    QPointer<QTableView> m_table_view; // TODO: remove
    QPointer<QTextBrowser> m_detailpages;
    QPointer<QListWidget> m_eventlog;
    QPointer<QTreeView> m_treeview;
    TreeModel* m_tree_model;

    QMenu *viewMenu;
};

#endif
