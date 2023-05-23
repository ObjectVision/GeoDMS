// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QAction;
class QListWidget;
class QMenu;
class QTextEdit;
class QTextBrowser;
class QTreeView;
class TreeModel;
class QTableView;
class MyModel;
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

    QTextEdit *textEdit;
    MyModel *m_table_view_model;
    QTableView *m_table_view;
    QListWidget *m_tree_view;
    QListWidget *m_detail_pages;
    QTextBrowser *m_detail_pages_textbrowser;
    QListWidget* m_eventlog;
    QTreeView* m_tree_view_widget;
    TreeModel* m_tree_model;

    QMenu *viewMenu;
};
//! [0]

#endif
