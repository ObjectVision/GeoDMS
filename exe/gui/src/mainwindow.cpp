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

#include <QPointer>
#include <QTreeView>
#include <QTableView>
#include "mainwindow.h"
#include "treemodel.h"
#include "mymodel.h"

MainWindow::MainWindow()
{
    //auto valid_keys = QStyleFactory::keys();
    //auto fusion_style = QStyleFactory::create("Fusion");
    //auto fusion_style = QStyleFactory::create("windows");
    //setStyle(fusion_style);

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
    // treeview
    QDockWidget *dock = new QDockWidget(tr("TreeView"), this);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    //m_tree_view = new QListWidget(dock);
    
    QFile file(":res/images/default.txt");
    
    file.open(QIODevice::ReadOnly);
    //auto test = file.readAll();
    m_tree_model = new TreeModel(file.readAll());
    //TreeModel model(file.readAll());
    file.close();

    m_tree_view_widget = new QTreeView(dock);
    m_tree_view_widget->setModel(m_tree_model);
    m_tree_view_widget->setHeaderHidden(true);
    m_tree_view_widget->setAnimated(false);
    m_tree_view_widget->setUniformRowHeights(true);
    
    /*m_tree_view->addItems(QStringList()
            << "John Doe, Harmony Enterprises, 12 Lakeside, Ambleton"
            << "Jane Doe, Memorabilia, 23 Watersedge, Beaton"
            << "Tammy Shea, Tiblanka, 38 Sea Views, Carlton"
            << "Tim Sheen, Caraba Gifts, 48 Ocean Way, Deal"
            << "Sol Harvey, Chicos Coffee, 53 New Springs, Eccleston"
            << "Sally Hobart, Tiroli Tea, 67 Long River, Fedula");
    
    */
    dock->setTitleBarWidget(new QWidget(dock));
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dock->setWidget(m_tree_view_widget);
    //dock->setWidget(m_tree_view);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    viewMenu->addAction(dock->toggleViewAction());


    dock = new QDockWidget(tr("DetailPages"), this);
    m_detail_pages_textbrowser = new QTextBrowser(dock);
    m_detail_pages_textbrowser->setMarkdown("# Fisher's Natural Breaks Classification complexity proof\n"
        "An ***O(k x n x log(n))*** algorithm is presented here, with proof of its validity and complexity, for the [[classification]] of an array of* n* numeric\n"
        "values into * k * classes such that the sum of the squared deviations from the class means is minimal, known as Fisher's Natural Breaks Classification. This algorithm is an improvement of [Jenks' Natural Breaks Classification Method](http://en.wikipedia.org/wiki/Jenks_natural_breaks_optimization),\n"
        "which is a(re)implementation of the algorithm described by Fisher within the context of [choropleth maps](http://en.wikipedia.org/wiki/Choropleth_map), which has time complexity ***O(k x n <sup>2</sup>)***.\n"
        "\n"
        "Finding breaks for 15 classes for a data array of 7 million unique values now takes 20 seconds on a desktop PC.\n"
        "\n"
        "## definitions\n"
        "Given a sequence of strictly increasing values ***v<sub>i</sub>*** and positive weights ***w<sub>i</sub>*** for ***i ? {1..n}***, and a given number of classes ***k*** with ***k = n***,\n"
        "\n"
        "choose classification function ***I(x)*** : {1..***n***} ? {1..***k***}, such that ***SSD<sub>n, k***</sub>, the sum of the squares of the deviations from the class means, is minimal, where :\n"
        "\n"
        //"$$ \\begin{align} SSD_{n,k} &: = \\min\\limits_{I: \\{1..n\\} \\to \\{1..k\\}} \\sum\\limits ^ k_{j = 1} ssd(S_j) & &\\text{minimal sum of sum of squared deviations} \\\\ S_j & : = \\{i\\in \\{1..n\\}\ | I(i) = j\\}&& \\text{set of indices that map to class}\\, j\\\\ ssd(S)& : = {\\sum\\limits_{i \\in S} {w_i(v_i - m(S)) ^ 2}} && \\text{the sum of squared deviations of the values of any index set}\\, S\\\\ m(S)& : = {s(S) \\over w(S)} && \\text{the weighted mean of the values of any index set}\\, S\\\\ s(S)& : = {\\sum\\limits_{i \\in S} {w_i v_i}} && \\text{the weighted sum of the values of any index set}\\, S\\\\ w(S)& : = \\sum\\limits_{i \\in S} {w_i}&& \\text{the sum of the value - weights of any index set}\\, S \\end{align} $$\n"
        //"\n"
        "Note that any array of n unsorted and not necessarily unique values can be sorted and made into unique*** v<sub>i*** < / sub> by removing duplicates with * **w<sub>i * **< / sub> representing the occurrence frequency of each value in * **O(nlog(n)) * **time.);\n");

    m_detail_pages_textbrowser->setOpenExternalLinks(true);

    dock->setWidget(m_detail_pages_textbrowser);
    dock->setTitleBarWidget(new QWidget(dock));
    //dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    viewMenu->addAction(dock->toggleViewAction());

    dock = new QDockWidget(tr("EventLog"), this);
    m_eventlog = new QListWidget(dock);
    m_eventlog->addItems(QStringList()
        << "Thank you for your payment which we have received today."
        << "Your order has been dispatched and should be with you "
        "within 28 days."
        << "We have dispatched those items that were in stock. The "
        "rest of your order will be dispatched once all the "
        "remaining items have arrived at our warehouse. No "
        "additional shipping charges will be made."
        << "You made a small overpayment (less than $5) which we "
        "will keep on account for you, or return at your request."
        << "You made a small underpayment (less than $1), but we have "
        "sent your order anyway. We'll add this underpayment to "
        "your next bill."
        << "Unfortunately you did not send enough money. Please remit "
        "an additional $. Your order will be dispatched as soon as "
        "the complete amount has been received."
        << "You made an overpayment (more than $5). Do you wish to "
        "buy more items, or should we return the excess to you?");
    dock->setWidget(m_eventlog);
    dock->setTitleBarWidget(new QWidget(dock));
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::BottomDockWidgetArea, dock);
    viewMenu->addAction(dock->toggleViewAction());

    connect(m_tree_view, &QListWidget::currentTextChanged,
            this, &MainWindow::insertCustomer);
    connect(m_detail_pages, &QListWidget::currentTextChanged,
            this, &MainWindow::addParagraph);
}