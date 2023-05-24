// dms
#include "RtcInterface.h"
#include "StxInterface.h"
#include "dbg/Debug.h"
#include "dbg/DebugLog.h"
#include "utl/Environment.h"

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
#include "DmsEventLog.h"
#include "DmsTreeView.h"
#include "DmsDetailPages.h"
#include <string>

#include "mymodel.h"

MainWindow::MainWindow()
{ 
    ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::XmlCompressionEnabled, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FloatingContainerHasWidgetIcon, false);
    m_DockManager = new ads::CDockManager(this);

    QListWidget* dms_eventlog_widget_pointer = new QListWidget(this);
    dms_eventlog_widget_pointer->addItems(QStringList()
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


    QLabel* label = new QLabel();
    label->setText("dms client area");
    label->setAlignment(Qt::AlignCenter);
    ads::CDockWidget* CentralDockWidget = new ads::CDockWidget("CentralWidget");
    CentralDockWidget->setWidget(label);
    CentralDockWidget->setFeature(ads::CDockWidget::NoTab, true);

    auto* CentralDockArea = m_DockManager->setCentralWidget(CentralDockWidget);
    

    QTableWidget* propertiesTable = new QTableWidget();
    propertiesTable->setColumnCount(3);
    propertiesTable->setRowCount(10);
    ads::CDockWidget* PropertiesDockWidget = new ads::CDockWidget("Properties");
    PropertiesDockWidget->setWidget(propertiesTable);
    PropertiesDockWidget->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
    PropertiesDockWidget->resize(250, 150);
    PropertiesDockWidget->setMinimumSize(200, 150);
    m_DockManager->addDockWidget(ads::DockWidgetArea::CenterDockWidgetArea, PropertiesDockWidget, CentralDockArea);

    auto fusion_style = QStyleFactory::create("Fusion"); // TODO: does this change appearance of widgets?
    setStyle(fusion_style);
    setupDmsCallbacks();

    // read initial last config file
    std::string geodms_last_config_file = GetGeoDmsRegKey("LastConfigFile").c_str();
    if (!geodms_last_config_file.empty())
        m_Root = DMS_CreateTreeFromConfiguration(geodms_last_config_file.c_str());

    // set example table view
    /*m_table_view_model = new MyModel;
    m_table_view = new QTableView;
    m_table_view->setModel(m_table_view_model);

    setCentralWidget(m_table_view);
    */

    createActions();
    createStatusBar();
    createDockWindows();
    setWindowTitle(tr("GeoDMS"));
    setUnifiedTitleAndToolBarOnMac(true);
}

MainWindow::~MainWindow()
{
    m_CurrentItem.reset();
    if (m_Root.has_ptr())
        m_Root->EnableAutoDelete();

    m_Root.reset();
}

void MainWindow::print() {} // TODO: remove
void MainWindow::save() {} // TODO: remove
void MainWindow::undo() {} // TODO: remove
void MainWindow::insertCustomer(const QString &customer) {} // TODO: remove
void MainWindow::addParagraph(const QString &paragraph) {} // TODO: remove

void OnVersionComponentVisit(ClientHandle clientHandle, UInt32 componentLevel, CharPtr componentName)
{
    auto& stream = *reinterpret_cast<FormattedOutStream*>(clientHandle);
    while (componentLevel)
    {
        stream << "-  ";
        componentLevel--;
    }
    for (char ch; ch = *componentName; ++componentName)
        if (ch == '\n')
            stream << "; ";
        else
            stream << ch;
    stream << '\n';
}

auto getGeoDMSAboutText() -> std::string
{
    VectorOutStreamBuff buff;
    FormattedOutStream stream(&buff, FormattingFlags::None);

    stream << DMS_GetVersion();
    stream << ", Copyright (c) Object Vision b.v.\n";
    DMS_VisitVersionComponents(&stream, OnVersionComponentVisit);
    return { buff.GetData(), buff.GetDataEnd() };
}

void MainWindow::about()
{
    auto dms_about_text = getGeoDMSAboutText();
    QMessageBox::about(this, tr("About"),
            tr(dms_about_text.c_str()));
}

void MainWindow::newLetter() {} // TODO: remove

void GeoDMSContextMessage(ClientHandle clientHandle, CharPtr msg)
{
    auto dms_main_window = reinterpret_cast<MainWindow*>(clientHandle);
    dms_main_window->statusBar()->showMessage(msg);
    return;
}

void MainWindow::setupDmsCallbacks()
{
    //DMS_SetGlobalCppExceptionTranslator(&m_EventLog.GeoDMSExceptionMessage);
    //DMS_RegisterMsgCallback(&m_EventLog.GeoDMSMessage, this);
    DMS_SetContextNotification(&GeoDMSContextMessage, this);
    //DMS_RegisterStateChangeNotification(&m_Views.OnOpenEditPaletteWindow, this);
    //SHV_SetCreateViewActionFunc(&m_DetailPages.OnViewAction);
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