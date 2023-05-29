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
    //auto fusion_style = QStyleFactory::create("Fusion"); // TODO: does this change appearance of widgets?
    //setStyle(fusion_style);

    // test widget for ads and mdi
    QTableWidget* propertiesTable_1 = new QTableWidget();
    propertiesTable_1->setColumnCount(3);
    propertiesTable_1->setRowCount(10);

    QTableWidget* propertiesTable_2 = new QTableWidget();
    propertiesTable_2->setColumnCount(3);
    propertiesTable_2->setRowCount(10);

    // Qt Advanced Docking System test
    
    ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::XmlCompressionEnabled, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);
    //ads::CDockManager::setConfigFlag(ads::CDockManager::FloatingContainerHasWidgetIcon, false);
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


    ads::CDockWidget* PropertiesDockWidget_1 = new ads::CDockWidget("Properties");
    PropertiesDockWidget_1->setWidget(propertiesTable_1);
    PropertiesDockWidget_1->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
    PropertiesDockWidget_1->resize(250, 150);
    PropertiesDockWidget_1->setMinimumSize(200, 150);
    m_DockManager->addDockWidget(ads::DockWidgetArea::CenterDockWidgetArea, PropertiesDockWidget_1, CentralDockArea);

    ads::CDockWidget* PropertiesDockWidget_2 = new ads::CDockWidget("Properties");
    PropertiesDockWidget_2->setWidget(propertiesTable_2);
    PropertiesDockWidget_2->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
    PropertiesDockWidget_2->resize(250, 150);
    PropertiesDockWidget_2->setMinimumSize(200, 150);
    m_DockManager->addDockWidget(ads::DockWidgetArea::CenterDockWidgetArea, PropertiesDockWidget_2, CentralDockArea);

    createActions();
    createStatusBar();
    createDmsHelperWindowDocks();
    createDetailPagesToolbar();

    // connect current item changed signal to appropriate slots
    connect(this, &MainWindow::currentItemChanged, m_detail_pages, &DmsDetailPages::newCurrentItem);
    
    setupDmsCallbacks();

    // read initial last config file
    std::string geodms_last_config_file = GetGeoDmsRegKey("LastConfigFile").c_str();
    if (!geodms_last_config_file.empty())
        m_root = DMS_CreateTreeFromConfiguration(geodms_last_config_file.c_str());

    if (m_root)
        setCurrentTreeitem(m_root); // as an example set current item to root, which emits signal currentItemChanged

    // set example table view
    /*m_table_view_model = new MyModel;
    m_table_view = new QTableView;
    m_table_view->setModel(m_table_view_model);

    setCentralWidget(m_table_view);
    */

    setWindowTitle(tr("GeoDMS"));
    setUnifiedTitleAndToolBarOnMac(true);
}

MainWindow::~MainWindow()
{
    m_current_item.reset();
    if (m_root.has_ptr())
        m_root->EnableAutoDelete();

    m_root.reset();
}

void MainWindow::setCurrentTreeitem(TreeItem* new_current_item)
{
    m_current_item = new_current_item;
    emit currentItemChanged();
}

#include <QFileDialog>

void MainWindow::fileOpen() 
{
    auto configFileName = QFileDialog::getOpenFileName(this, "Open configuration", {}, "*.dms");
    if (m_root)
        m_root->EnableAutoDelete();
    auto newRoot = DMS_CreateTreeFromConfiguration(configFileName.toUtf8().data());
    if (newRoot)
    {
        m_root = newRoot;
        setCurrentTreeitem(m_root);
        m_treeview->setModel(new DmsModel( m_root )); // TODO: check Ownership ?
    }
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

void geoDMSContextMessage(ClientHandle clientHandle, CharPtr msg)
{
    auto dms_main_window = reinterpret_cast<MainWindow*>(clientHandle);
    dms_main_window->statusBar()->showMessage(msg);
    return;
}

void MainWindow::setupDmsCallbacks()
{
    //DMS_SetGlobalCppExceptionTranslator(&m_EventLog.GeoDMSExceptionMessage);
    DMS_RegisterMsgCallback(&geoDMSMessage, m_eventlog);
    DMS_SetContextNotification(&geoDMSContextMessage, this);
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
    QAction *fileOpenAct = new QAction(newIcon, tr("File &Open"), this);
    fileOpenAct->setShortcuts(QKeySequence::Open);
    fileOpenAct->setStatusTip(tr("Open an existing configuration file"));
    connect(fileOpenAct, &QAction::triggered, this, &MainWindow::fileOpen);
    fileMenu->addAction(fileOpenAct);
    fileToolBar->addAction(fileOpenAct);

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

void MainWindow::createDetailPagesToolbar()
{
    QToolBar* detail_pages_toolBar = new QToolBar(tr("DetailPagesActions"), this);
    addToolBar(Qt::ToolBarArea::RightToolBarArea, detail_pages_toolBar);

    const QIcon general_icon = QIcon::fromTheme("detailpages-general", QIcon(":res/images/DP_properties.bmp"));
    QAction* general_page_act = new QAction(general_icon, tr("&General"), this);
    detail_pages_toolBar->addAction(general_page_act);
    connect(general_page_act, &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleGeneral);

    const QIcon explore_icon = QIcon::fromTheme("detailpages-explore", QIcon(":res/images/DP_explore.bmp"));
    QAction* explore_page_act = new QAction(explore_icon, tr("&Explore"), this);
    detail_pages_toolBar->addAction(explore_page_act);

    const QIcon properties_icon = QIcon::fromTheme("detailpages-properties", QIcon(":res/images/DP_properties.bmp"));
    QAction* properties_page_act = new QAction(properties_icon, tr("&Properties"), this);
    detail_pages_toolBar->addAction(properties_page_act);

    const QIcon value_info_icon = QIcon::fromTheme("detailpages-valueinfo", QIcon(":res/images/DP_ValueInfo.bmp"));
    QAction* value_info_page_act = new QAction(value_info_icon, tr("&Value info"), this);
    detail_pages_toolBar->addAction(value_info_page_act);
}

void MainWindow::createDetailPagesDock()
{
    m_detailpages_dock = new QDockWidget(QObject::tr("DetailPages"), this);
    m_detailpages_dock->setTitleBarWidget(new QWidget(m_detailpages_dock));
    m_detail_pages = new DmsDetailPages(m_detailpages_dock);
    m_detail_pages->connectDetailPagesAnchorClicked();
    m_detailpages_dock->setWidget(m_detail_pages);
    addDockWidget(Qt::RightDockWidgetArea, m_detailpages_dock);
}

void MainWindow::createDmsHelperWindowDocks()
{
    createDetailPagesDock();
    m_detail_pages->setDummyText();

    m_treeview = createTreeview(this);    
    m_eventlog = createEventLog(this);
}