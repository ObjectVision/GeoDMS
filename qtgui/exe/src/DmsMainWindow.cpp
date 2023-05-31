// dms
#include "RtcInterface.h"
#include "StxInterface.h"
#include "act/MainThread.h"
#include "dbg/Debug.h"
#include "dbg/DebugLog.h"
#include "dbg/SeverityType.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"
#include "TreeItem.h"
#include "ShvUtils.h"

#include <QtWidgets>
#include <QTextBrowser>

#include "DmsMainWindow.h"
#include "DmsEventLog.h"
#include "DmsViewArea.h"
#include "DmsTreeView.h"
#include "DmsDetailPages.h"
//#include <string>

static MainWindow* s_CurrMainWindow = nullptr;

MainWindow::MainWindow()
{ 
    assert(s_CurrMainWindow == nullptr);
    s_CurrMainWindow = this;

    //auto fusion_style = QStyleFactory::create("Fusion"); // TODO: does this change appearance of widgets?
    //setStyle(fusion_style);

    // test widget for ads and mdi
    QTableWidget* propertiesTable_1 = new QTableWidget();
    propertiesTable_1->setColumnCount(3);
    propertiesTable_1->setRowCount(10);

    auto tv2 = new QTreeView;

    // Qt Advanced Docking System test
    
    ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::XmlCompressionEnabled, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);
    //ads::CDockManager::setConfigFlag(ads::CDockManager::FloatingContainerHasWidgetIcon, false);
    m_DockManager = new ads::CDockManager(this);

    QLabel* label = new QLabel();
    label->setText("dms client area");
    label->setAlignment(Qt::AlignCenter);
    ads::CDockWidget* CentralDockWidget = new ads::CDockWidget("CentralWidget");
    CentralDockWidget->setWidget(label);
    CentralDockWidget->setFeature(ads::CDockWidget::NoTab, true);
    centralDockArea = m_DockManager->setCentralWidget(CentralDockWidget);

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
        LoadConfig(geodms_last_config_file.c_str());
 
    if (m_root)
    {
        setCurrentTreeitem(m_root); // as an example set current item to root, which emits signal currentItemChanged
        m_treeview->setModel(new DmsModel(m_root));
        auto test = m_treeview->rootIsDecorated();

        //m_treeview->expandAll();
        //tv2->setModel(new DmsModel(m_root));
    }

    setWindowTitle(tr("GeoDMS"));
    setUnifiedTitleAndToolBarOnMac(true);
}

MainWindow::~MainWindow()
{
    assert(s_CurrMainWindow == this);
    s_CurrMainWindow = nullptr;

    m_current_item.reset();
    if (m_root.has_ptr())
        m_root->EnableAutoDelete();

    m_root.reset();
}

MainWindow* MainWindow::TheOne()
{
    assert(IsMainThread()); // or use a mutex to guard access to TheOne.
    assert(s_CurrMainWindow);
    return s_CurrMainWindow;
}

void MainWindow::EventLog(SeverityTypeID st, CharPtr msg)
{
    if (!s_CurrMainWindow)
        return;

    // TODO: make eventlog lazy using custom model
    // TODO: create fancy styling of eventlog items using view implementation of model/view

    auto eventLogWidget = TheOne()->m_eventlog;
    eventLogWidget->addItem(msg);

    // https://stackoverflow.com/questions/2210402/how-to-change-the-text-color-of-items-in-a-qlistwidget

    Qt::GlobalColor clr;
    switch (st) {
    case SeverityTypeID::ST_Error: clr = Qt::red; break;
    case SeverityTypeID::ST_Warning: clr = Qt::darkYellow; break;
    case SeverityTypeID::ST_MajorTrace: clr = Qt::darkBlue; break;
    default: return;
    }
    eventLogWidget->item(eventLogWidget->count() - 1)->setForeground(clr);
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
    LoadConfig(configFileName.toUtf8().data());
}

void MainWindow::reOpen()
{
    // TODO: auto current_item_path = addressBar.GetString();
    if (m_root)
        m_root->EnableAutoDelete();
    LoadConfig(m_currConfigFileName.c_str());
    // TODO: addressBar.Goto(current_item_path);
}

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

void MainWindow::aboutGeoDms()
{
    auto dms_about_text = getGeoDMSAboutText();
    QMessageBox::about(this, tr("About GeoDms"),
            tr(dms_about_text.c_str()));
}


void MainWindow::defaultView()
{
    static UInt32 s_ViewCounter = 0;

    auto currItem = getCurrentTreeitem();
    if (!currItem)
        return;

    auto desktopItem = GetDefaultDesktopContainer(m_root); // rootItem->CreateItemFromPath("DesktopInfo");
    auto viewContextItem = desktopItem->CreateItemFromPath(mySSPrintF("View%d", s_ViewCounter++).c_str());

    HWND hWndMain = (HWND)winId();

    auto dataViewDockWidget = new ads::CDockWidget("DefaultView");
    auto dmsControl = new QDmsViewArea(dataViewDockWidget, hWndMain, viewContextItem, currItem);
    //    m_dms_views.emplace_back(name, vs, dv);
    //    m_dms_view_it = --m_dms_views.end();
    //    dvm_dms_view_it->UpdateParentWindow(); // m_Views.back().UpdateParentWindow(); // Needed before InitWindow

    dataViewDockWidget->setWidget(dmsControl);
    dataViewDockWidget->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
    dataViewDockWidget->resize(250, 150);
    dataViewDockWidget->setMinimumSize(200, 150);
    m_DockManager->addDockWidget(ads::DockWidgetArea::CenterDockWidgetArea, dataViewDockWidget, centralDockArea);
}

void geoDMSContextMessage(ClientHandle clientHandle, CharPtr msg)
{
    auto dms_main_window = reinterpret_cast<MainWindow*>(clientHandle);
    dms_main_window->statusBar()->showMessage(msg);
    return;
}

void MainWindow::LoadConfig(CharPtr fileName)
{
    auto newRoot = DMS_CreateTreeFromConfiguration(fileName);
    if (newRoot)
    {
        m_root = newRoot;
        setCurrentTreeitem(m_root);
        m_treeview->setRootIsDecorated(true);
        m_treeview->setUniformRowHeights(true);
        m_treeview->setItemsExpandable(true);
        m_treeview->setModel(new DmsModel(m_root)); // TODO: check Ownership ?
        m_treeview->setRootIndex({});
        m_treeview->scrollTo({});
    }
    m_currConfigFileName = fileName;
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
    auto fileMenu = menuBar()->addMenu(tr("&File"));
    auto current_item_bar_container = addToolBar(tr("test"));
    auto current_item_bar = new QLineEdit(this);
    current_item_bar_container->addWidget(current_item_bar);

    addToolBarBreak();

    auto fileToolBar = addToolBar(tr("File"));
    auto openIcon = QIcon::fromTheme("document-open", QIcon(":res/images/open.png"));
    auto fileOpenAct = new QAction(openIcon, tr("&Open Configuration File"), this);
    fileOpenAct->setShortcuts(QKeySequence::Open);
    fileOpenAct->setStatusTip(tr("Open an existing configuration file"));
    connect(fileOpenAct, &QAction::triggered, this, &MainWindow::fileOpen);
    fileMenu->addAction(fileOpenAct);
    fileToolBar->addAction(fileOpenAct);

    auto reOpenAct = new QAction(openIcon, tr("&Reopen current Configuration"), this);
    reOpenAct->setShortcuts(QKeySequence::Refresh);
    reOpenAct->setStatusTip(tr("Reopen the current configuration and reactivate the current active item"));
    connect(reOpenAct, &QAction::triggered, this, &MainWindow::reOpen);
    fileMenu->addAction(reOpenAct);
    fileToolBar->addAction(reOpenAct);

    fileMenu->addSeparator();
    auto epdm = fileMenu->addMenu(tr("Export Primary Data"));
    auto epdmBmp = new QAction(tr("Bitmap (*.tif or *.bmp)"));
    auto epdmDbf = new QAction(tr("Table (*.dbf with a *.shp and *.shx if Feature data can be related)"));
    auto epdmCsv = new QAction(tr("Text Table (*.csv with semiColon Separated Values"));
    epdm->addAction(epdmBmp);
    epdm->addAction(epdmDbf);
    epdm->addAction(epdmCsv);

/*
    const QIcon saveIcon = QIcon::fromTheme("document-save", QIcon(":res/images/save.png"));
    QAction *saveAct = new QAction(saveIcon, tr("&Save..."), this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save the current form letter"));
    connect(saveAct, &QAction::triggered, this, &MainWindow::save);
    fileMenu->addAction(saveAct);
    fileToolBar->addAction(saveAct);
*/
    fileMenu->addSeparator();
    QAction *quitAct = fileMenu->addAction(tr("&Quit"), qApp, &QCoreApplication::quit);
    quitAct->setShortcuts(QKeySequence::Quit);
    quitAct->setStatusTip(tr("Quit the application"));

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    QToolBar *editToolBar = addToolBar(tr("Edit"));

/*
    const QIcon undoIcon = QIcon::fromTheme("edit-undo", QIcon(":res/images/undo.png"));
    QAction *undoAct = new QAction(undoIcon, tr("&Undo"), this);
    undoAct->setShortcuts(QKeySequence::Undo);
    undoAct->setStatusTip(tr("Undo the last editing action"));
    connect(undoAct, &QAction::triggered, this, &MainWindow::undo);
    editMenu->addAction(undoAct);
    editToolBar->addAction(undoAct);
*/
    auto viewMenu = menuBar()->addMenu(tr("&View"));
    auto viewDefaultAct = new QAction("Default View");
    connect(viewDefaultAct, &QAction::triggered, this, &MainWindow::defaultView);
    viewMenu->addAction(viewDefaultAct);

    menuBar()->addSeparator();

    auto helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAct = helpMenu->addAction(tr("&About GeoDms"), this, &MainWindow::aboutGeoDms);
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
    connect(explore_page_act, &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleExplorer);

    const QIcon properties_icon = QIcon::fromTheme("detailpages-properties", QIcon(":res/images/DP_properties.bmp"));
    QAction* properties_page_act = new QAction(properties_icon, tr("&Properties"), this);
    detail_pages_toolBar->addAction(properties_page_act);
    connect(properties_page_act, &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleProperties);

    const QIcon configuraion_icon = QIcon::fromTheme("detailpages-configuration", QIcon(":res/images/DP_configuration.bmp"));
    QAction* configuration_page_act = new QAction(properties_icon, tr("&Configuration"), this);
    detail_pages_toolBar->addAction(configuration_page_act);
    connect(configuration_page_act, &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleConfiguration);

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
//    m_detail_pages->setDummyText();

    m_treeview = createTreeview(this);    
    m_eventlog = createEventLog(this);
}