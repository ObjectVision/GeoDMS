// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

//! [0]

#include "RtcInterface.h"
#include <QtWidgets>
#include <QTextBrowser>
#if defined(QT_PRINTSUPPORT_LIB)
#include <QtPrintSupport/qtprintsupportglobal.h>
#if QT_CONFIG(printdialog)
#include <QtPrintSupport>
#endif
#endif

#include <QTableView>
#include "DmsMainWindow.h"

#include "mymodel.h"

#include "DmsEventLog.h"
#include "DmsTreeView.h"
#include "DmsDetailPages.h"

MainWindow::MainWindow()
{
    //auto valid_keys = QStyleFactory::keys();
    auto fusion_style = QStyleFactory::create("Fusion"); // TODO: does this change appearance of widgets?
    //auto fusion_style = QStyleFactory::create("windows");
    setStyle(fusion_style);

    // set example table view
    m_table_view_model = new MyModel;
    m_table_view = new QTableView;
    m_table_view->setModel(m_table_view_model);

    
    //setCentralWidget(textEdit);
    setCentralWidget(m_table_view);
    //setCentralWidget(nullptr);
    
    
    createActions();
    createStatusBar();

    statusBar()->showMessage(DMS_GetVersion());

    createDockWindows();

    setWindowTitle(tr("GeoDMS"));

    setUnifiedTitleAndToolBarOnMac(true);
}

void MainWindow::print()
{

}

void MainWindow::save()
{

}

void MainWindow::undo()
{

}

void MainWindow::insertCustomer(const QString &customer)
{

}

void MainWindow::addParagraph(const QString &paragraph)
{

}

void MainWindow::about()
{
   QMessageBox::about(this, tr("About Dock Widgets"),
            tr("The <b>Dock Widgets</b> example demonstrates how to "
               "use Qt's dock widgets. You can enter your own text, "
               "click a customer to add a customer name and "
               "address, and click standard paragraphs to add them."));
}

void MainWindow::newLetter()
{

}

void MainWindow::createActions()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));


    QToolBar* current_item_bar_container = addToolBar(tr("test"));
    QLineEdit* current_item_bar = new QLineEdit(this);
    current_item_bar_container->addWidget(current_item_bar);

    addToolBarBreak();

    QToolBar *fileToolBar = addToolBar(tr("File"));

    const QIcon newIcon = QIcon::fromTheme("document-new", QIcon(":res/images/new.png"));
    QAction *newLetterAct = new QAction(newIcon, tr("&New Letter"), this);
    newLetterAct->setShortcuts(QKeySequence::New);
    newLetterAct->setStatusTip(tr("Create a new form letter"));
    connect(newLetterAct, &QAction::triggered, this, &MainWindow::newLetter);
    fileMenu->addAction(newLetterAct);
    fileToolBar->addAction(newLetterAct);

    const QIcon saveIcon = QIcon::fromTheme("document-save", QIcon(":res/images/save.png"));
    QAction *saveAct = new QAction(saveIcon, tr("&Save..."), this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save the current form letter"));
    connect(saveAct, &QAction::triggered, this, &MainWindow::save);
    fileMenu->addAction(saveAct);
    fileToolBar->addAction(saveAct);

    const QIcon printIcon = QIcon::fromTheme("document-print", QIcon(":res/images/print.png"));
    QAction *printAct = new QAction(printIcon, tr("&Print..."), this);
    printAct->setShortcuts(QKeySequence::Print);
    printAct->setStatusTip(tr("Print the current form letter"));
    connect(printAct, &QAction::triggered, this, &MainWindow::print);
    fileMenu->addAction(printAct);
    fileToolBar->addAction(printAct);

    fileMenu->addSeparator();

    QAction *quitAct = fileMenu->addAction(tr("&Quit"), qApp, &QCoreApplication::quit);
    quitAct->setShortcuts(QKeySequence::Quit);
    quitAct->setStatusTip(tr("Quit the application"));

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    QToolBar *editToolBar = addToolBar(tr("Edit"));



    const QIcon undoIcon = QIcon::fromTheme("edit-undo", QIcon(":res/images/undo.png"));
    QAction *undoAct = new QAction(undoIcon, tr("&Undo"), this);
    undoAct->setShortcuts(QKeySequence::Undo);
    undoAct->setStatusTip(tr("Undo the last editing action"));
    connect(undoAct, &QAction::triggered, this, &MainWindow::undo);
    editMenu->addAction(undoAct);
    editToolBar->addAction(undoAct);

    viewMenu = menuBar()->addMenu(tr("&View"));

    menuBar()->addSeparator();

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

    QAction *aboutAct = helpMenu->addAction(tr("&About"), this, &MainWindow::about);
    aboutAct->setStatusTip(tr("Show the application's About box"));

    QAction *aboutQtAct = helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::createDockWindows()
{
    m_treeview = createTreeview(this);
    m_detailpages = createDetailPages(this);
    m_eventlog = createEventLog(this);
}