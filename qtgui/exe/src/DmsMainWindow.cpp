// dms
#include "RtcInterface.h"
#include "StxInterface.h"
#include "TicInterface.h"
#include "act/MainThread.h"
#include "dbg/Debug.h"
#include "dbg/DebugLog.h"
#include "dbg/DmsCatch.h"
#include "dbg/SeverityType.h"
#include "dbg/Timer.h"

#include "utl/Environment.h"
#include "utl/mySPrintF.h"

#include "TreeItem.h"
#include "DataView.h"
#include "ShvDllInterface.h"

#include <QtWidgets>
#include <QTextBrowser>
#include <QCompleter>

#include "DmsMainWindow.h"
#include "DmsEventLog.h"
#include "DmsViewArea.h"
#include "DmsTreeView.h"
#include "DmsDetailPages.h"
#include "StateChangeNotification.h"

#include "dataview.h"

static MainWindow* s_CurrMainWindow = nullptr;


DmsOptionsWindow::DmsOptionsWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Options"));
    auto grid_layout = new QGridLayout(this);

    // path widgets
    auto path_ld = new QLabel("Local data:", this);
    auto path_sd = new QLabel("Source data:", this);
    auto path_ld_input = new QLineEdit(this);
    auto path_sd_input = new QLineEdit(this);
    auto path_ld_fldr = new QPushButton(QIcon(":/res/images/DP_explore.bmp"), "", this);
    path_ld_fldr->setFlat(true);
    //path_ld_fldr->setStyleSheet(QString("QPushButton {outline: none;}"));
    auto path_sd_fldr = new QPushButton(QIcon(":/res/images/DP_explore.bmp"), "", this);
    path_sd_fldr->setFlat(true);

    grid_layout->addWidget(path_ld, 0, 0);
    grid_layout->addWidget(path_ld_input, 0, 1);
    grid_layout->addWidget(path_ld_fldr, 0, 2);
    grid_layout->addWidget(path_sd, 1, 0);
    grid_layout->addWidget(path_sd_input, 1, 1);
    grid_layout->addWidget(path_sd_fldr, 1, 2);

    auto path_line = new QFrame(this);
    path_line->setFrameShape(QFrame::HLine);
    path_line->setFrameShadow(QFrame::Plain);
    path_line->setLineWidth(1);
    path_line->setMidLineWidth(1);
    grid_layout->addWidget(path_line, 2, 0, 1, 3);

    // editor widgets
    auto editor_text = new QLabel("Editor:", this);
    auto editor_input = new QLineEdit(this);
    grid_layout->addWidget(editor_text, 3, 0, 1, 3);
    grid_layout->addWidget(editor_input, 3, 1, 1, 3);

    auto line_editor = new QFrame(this);
    line_editor->setFrameShape(QFrame::HLine);
    line_editor->setFrameShadow(QFrame::Plain);
    line_editor->setLineWidth(1);
    line_editor->setMidLineWidth(1);
    grid_layout->addWidget(line_editor, 4, 0, 1, 3);

    // parallel processing widgets
    auto pp_text = new QLabel("Parallel processing:", this);
    auto pp_checkbox_s0 = new QCheckBox(this);
    auto pp_checkbox_s1 = new QCheckBox(this);
    auto pp_checkbox_s2 = new QCheckBox(this);
    auto pp_checkbox_s3 = new QCheckBox(this);
    auto pp_text_s0 = new QLabel("0: Suspend view updates to favor gui", this);
    auto pp_text_s1 = new QLabel("1: Tile/segment tasks", this);
    auto pp_text_s2 = new QLabel("2: Multiple operations simultaneously", this);
    auto pp_text_s3 = new QLabel("3: Pipelined tile operations:", this);

    grid_layout->addWidget(pp_text, 5, 0);
    grid_layout->addWidget(pp_checkbox_s0, 6, 0);
    grid_layout->addWidget(pp_text_s0, 6, 1);
    grid_layout->addWidget(pp_checkbox_s1, 7, 0);
    grid_layout->addWidget(pp_text_s1, 7, 1);
    grid_layout->addWidget(pp_checkbox_s2, 8, 0);
    grid_layout->addWidget(pp_text_s2, 8, 1);
    grid_layout->addWidget(pp_checkbox_s3, 9, 0);
    grid_layout->addWidget(pp_text_s3, 9, 1);

    auto pp_line = new QFrame(this);
    pp_line->setFrameShape(QFrame::HLine);
    pp_line->setFrameShadow(QFrame::Plain);
    pp_line->setLineWidth(1);
    pp_line->setMidLineWidth(1);
    grid_layout->addWidget(pp_line, 10, 0, 1, 3);

    // flush treshold
    auto ft_text = new QLabel("Flush treshold:", this);
    auto ft_percentage = new QLabel("%", this);
    auto ft_slider = new QSlider(Qt::Orientation::Horizontal, this);
    ft_slider->setTickPosition(QSlider::TickPosition::TicksAbove);
    ft_slider->setMinimum(50);
    ft_slider->setMaximum(100);
    auto ft_text_tracelog = new QLabel("Tracelog file:", this);
    auto ft_tracelog = new QCheckBox(this);
    grid_layout->addWidget(ft_text, 11, 0);
    grid_layout->addWidget(ft_slider, 11, 1);
    grid_layout->addWidget(ft_percentage, 11, 2);
    grid_layout->addWidget(ft_text_tracelog, 12, 0);
    grid_layout->addWidget(ft_tracelog, 12, 1);

    auto ft_line = new QFrame(this);
    ft_line->setFrameShape(QFrame::HLine);
    ft_line->setFrameShadow(QFrame::Plain);
    ft_line->setLineWidth(1);
    ft_line->setMidLineWidth(1);
    grid_layout->addWidget(ft_line, 13, 0, 1, 3);

    auto ok_button = new QPushButton("Ok");
    auto apply_button = new QPushButton("Apply");
    auto cancel_button = new QPushButton("Cancel");
    grid_layout->addWidget(ok_button, 14, 0);
    grid_layout->addWidget(apply_button, 14, 1);
    grid_layout->addWidget(cancel_button, 14, 2);

    setWindowModality(Qt::ApplicationModal);
}


MainWindow::MainWindow(CmdLineSetttings& cmdLineSettings)
{ 
    assert(s_CurrMainWindow == nullptr);
    s_CurrMainWindow = this;

    m_mdi_area = std::make_unique<QDmsMdiArea>(this);

    QFont dms_text_font(":/res/fonts/dmstext.ttf", 10);
    QApplication::setFont(dms_text_font);

    setCentralWidget(m_mdi_area.get());

    m_mdi_area->show();

    createStatusBar();
    createDmsHelperWindowDocks();
    createDetailPagesToolbar();

    // connect current item changed signal to appropriate slots
    connect(this, &MainWindow::currentItemChanged, m_detail_pages, &DmsDetailPages::newCurrentItem);

    setupDmsCallbacks();

    m_dms_model = std::make_unique<DmsModel>();
    m_treeview->setModel(m_dms_model.get());

    // read initial last config file
    if (!cmdLineSettings.m_NoConfig)
    {
        if (cmdLineSettings.m_ConfigFileName.empty())
            cmdLineSettings.m_ConfigFileName = GetGeoDmsRegKey("LastConfigFile");
        if (!cmdLineSettings.m_ConfigFileName.empty())
            LoadConfig(cmdLineSettings.m_ConfigFileName.c_str());
    }
    if (m_root)
    {
        auto test = m_treeview->rootIsDecorated();
    }

    createActions();
    m_current_item_bar->setDmsCompleter();

    updateCaption();
    setUnifiedTitleAndToolBarOnMac(true);
    if (!cmdLineSettings.m_CurrItemFullNames.empty())
        m_current_item_bar->setPath(cmdLineSettings.m_CurrItemFullNames.back().c_str());
}

MainWindow::~MainWindow()
{
    CloseConfig();

    assert(s_CurrMainWindow == this);
    s_CurrMainWindow = nullptr;

    DMS_SetGlobalCppExceptionTranslator(nullptr);
    DMS_ReleaseMsgCallback(&geoDMSMessage, m_eventlog);
    DMS_SetContextNotification(nullptr, nullptr);
    SHV_SetCreateViewActionFunc(nullptr);
}

auto MainWindow::getDmsTreeViewPtr() -> DmsTreeView*
{ 
    return m_treeview.get(); 
}

void DmsCurrentItemBar::setDmsCompleter()
{
    TreeModelCompleter* completer = new TreeModelCompleter(this);
    completer->setModel(MainWindow::TheOne()->getDmsModel());
    completer->setSeparator("/");
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    setCompleter(completer);
}

void DmsCurrentItemBar::setPath(CharPtr itemPath)
{
    setText(itemPath);
    onEditingFinished();
}

void DmsCurrentItemBar::onEditingFinished()
{
    auto root = MainWindow::TheOne()->getRootTreeItem();
    if (!root)
        return;

    auto best_item_ref = TreeItem_GetBestItemAndUnfoundPart(root, text().toUtf8());
    auto found_treeitem = best_item_ref.first;
    if (found_treeitem)
    {
        MainWindow::TheOne()->setCurrentTreeItem(const_cast<TreeItem*>(found_treeitem));
        auto treeView = MainWindow::TheOne()->getDmsTreeViewPtr();
        treeView->scrollTo(treeView->currentIndex(), QAbstractItemView::ScrollHint::PositionAtBottom);
    }
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
    while (eventLogWidget->count() > 1000)
        delete eventLogWidget->takeItem(0);

    eventLogWidget->addItem(msg);
    
    static Timer t;
    if (t.PassedSecs(5))
        eventLogWidget->scrollToBottom();

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

void MainWindow::setCurrentTreeItem(TreeItem* new_current_item)
{
    if (m_current_item == new_current_item)
        return;

    m_current_item = new_current_item;
    if (m_current_item_bar)
    {
        if (m_current_item)
            m_current_item_bar->setText(m_current_item->GetFullName().c_str());
        else
            m_current_item_bar->setText("");
    }

    m_treeview->setNewCurrentItem(new_current_item);
    m_treeview->scrollTo(m_treeview->currentIndex()); // , QAbstractItemView::ScrollHint::PositionAtCenter);
    emit currentItemChanged();
}

#include <QFileDialog>

void MainWindow::fileOpen() 
{
    auto configFileName = QFileDialog::getOpenFileName(this, "Open configuration", {}, "*.dms");
    LoadConfig(configFileName.toUtf8().data());
}

void MainWindow::reOpen()
{
    auto cip = m_current_item_bar->text();

    LoadConfig(m_currConfigFileName.c_str());
    m_current_item_bar->setPath(cip.toUtf8());
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

DmsToolbuttonAction::DmsToolbuttonAction(const QIcon& icon, const QString& text, QObject* parent, ToolbarButtonData button_data)
    : QAction(icon, text, parent)
{
    m_data = std::move(button_data);
}

void DmsToolbuttonAction::onToolbuttonPressed()
{
    auto mdi_area = MainWindow::TheOne()->getDmsMdiAreaPtr();
    if (!mdi_area)
        return;

    auto subwindow_list = mdi_area->subWindowList(QMdiArea::WindowOrder::StackingOrder);
    auto subwindow_highest_in_z_order = subwindow_list.back();
    if (!subwindow_highest_in_z_order)
        return;

    auto dms_view_area = dynamic_cast<QDmsViewArea*>(subwindow_highest_in_z_order);
    if (!dms_view_area)
        return;
    dms_view_area->getDataView()->GetContents()->OnCommand(m_data.ids[0]);
}

auto getToolbarButtonData(ToolButtonID button_id) -> ToolbarButtonData
{
    switch (button_id)
    {
    case TB_Export: return { ButtonType::SINGLE, "", {TB_Export}, {":/res/images/TB_save.bmp"} };
    case TB_TableCopy: return { ButtonType::SINGLE, "", {TB_TableCopy}, {":/res/images/TB_copy.bmp"} };
    case TB_Copy: return { ButtonType::SINGLE, "", {TB_Copy}, {":/res/images/TB_copy.bmp"} };
    case TB_CopyLC: return { ButtonType::SINGLE, "", {TB_CopyLC}, {":/res/images/TB_vcopy.bmp"} };
    case TB_ZoomSelectedObj: return { ButtonType::SINGLE, "", {TB_ZoomSelectedObj}, {":/res/images/TB_zoom_selected.bmp"} };
    case TB_SelectRows: return { ButtonType::SINGLE, "", {TB_SelectRows}, {":/res/images/TB_table_select_row.bmp"} };
    case TB_SelectAll: return { ButtonType::SINGLE, "", {TB_SelectAll}, {":/res/images/TB_select_all.bmp"} };
    case TB_SelectNone: return { ButtonType::SINGLE, "", {TB_SelectNone}, {":/res/images/TB_select_none.bmp"} };
    case TB_ShowSelOnlyOn: return { ButtonType::SINGLE, "", {TB_ShowSelOnlyOn}, {":/res/images/TB_show_selected_features.bmp"} };
    case TB_TableGroupBy: return { ButtonType::SINGLE, "", {TB_TableGroupBy}, {":/res/images/TB_group_by.bmp"} };
    case TB_ZoomAllLayers: return { ButtonType::SINGLE, "", {TB_ZoomAllLayers}, {":/res/images/TB_zoom_all_layers.bmp"} };
    case TB_ZoomIn2: return { ButtonType::SINGLE, "", {TB_ZoomIn2}, {":/res/images/TB_zoomin_button.bmp"} };
    case TB_ZoomOut2: return { ButtonType::SINGLE, "", {TB_ZoomOut2}, {":/res/images/TB_zoomout_button.bmp"} };
    case TB_SelectObject: return { ButtonType::SINGLE, "", {TB_SelectObject}, {":/res/images/TB_select_object.bmp"} };
    case TB_SelectRect: return { ButtonType::SINGLE, "", {TB_SelectRect}, {":/res/images/TB_select_rect.bmp"} };
    case TB_SelectCircle: return { ButtonType::SINGLE, "", {TB_SelectCircle}, {":/res/images/TB_select_circle.bmp"} };
    case TB_SelectPolygon: return { ButtonType::SINGLE, "", {TB_SelectPolygon}, {":/res/images/TB_select_poly.bmp"} };
    case TB_SelectDistrict: return { ButtonType::SINGLE, "", {TB_SelectDistrict}, {":/res/images/TB_select_district.bmp"} };
    case TB_Show_VP: return { ButtonType::SINGLE, "", {TB_Show_VP}, {":/res/images/TB_toggle_layout_3.bmp"} };
    case TB_SP_All: return { ButtonType::SINGLE, "", {TB_SP_All}, {":/res/images/TB_toggle_palette.bmp"} };
    case TB_NeedleOn: return { ButtonType::SINGLE, "", {TB_NeedleOn}, {":/res/images/TB_toggle_needle.bmp"} };
    case TB_ScaleBarOn: return { ButtonType::SINGLE, "", {TB_ScaleBarOn}, {":/res/images/TB_toggle_scalebar.bmp"} };
    }

    return {};
}

void MainWindow::updateToolbar(QMdiSubWindow* active_mdi_subwindow)
{
    if (m_tooled_mdi_subwindow == active_mdi_subwindow)
        return;

    m_tooled_mdi_subwindow = active_mdi_subwindow;
    if (!active_mdi_subwindow)
        return;

    auto active_dms_view_area = dynamic_cast<QDmsViewArea*>(active_mdi_subwindow);

    addToolBarBreak();
    if (!m_toolbar)
    {
        m_toolbar = addToolBar(tr("dmstoolbar"));
        m_toolbar->setStyleSheet("QToolBar { background: rgb(117, 117, 138); }\n");
        m_toolbar->setIconSize(QSize(32, 32));
    }
    m_toolbar->clear();

    if (!active_dms_view_area)
        return;
    // create new actions
    auto* dv = active_dms_view_area->getDataView();// ->OnCommandEnable();
    if (!dv)
        return;

    auto view_style = dv->GetViewType();


    static ToolButtonID available_map_buttons[] = { TB_Export , TB_TableCopy, TB_Copy, TB_CopyLC, TB_ZoomSelectedObj, TB_SelectRows,
                                                    TB_SelectAll, TB_SelectNone, TB_ShowSelOnlyOn, TB_TableGroupBy, TB_ZoomAllLayers,
                                                    TB_ZoomIn2, TB_ZoomOut2, TB_SelectObject, TB_SelectRect, TB_SelectCircle,
                                                    TB_SelectPolygon, TB_SelectDistrict, TB_SelectAll, TB_SelectNone, TB_ShowSelOnlyOn,
                                                    TB_Show_VP, TB_SP_All, TB_NeedleOn, TB_ScaleBarOn };
    static ToolButtonID available_table_buttons[] = { TB_Export , TB_TableCopy, TB_Copy };
    ToolButtonID* button_id_ptr = available_map_buttons;
    SizeT button_id_count = sizeof(available_map_buttons) / sizeof(ToolButtonID);
    if (view_style == ViewStyle::tvsTableView)
    {
        button_id_ptr = available_table_buttons;
        button_id_count = sizeof(available_table_buttons) / sizeof(ToolButtonID);
    }

    while (button_id_count--)
    {
        auto button_id = *button_id_ptr++;
        auto is_command_enabled = dv->OnCommandEnable(button_id) == CommandStatus::ENABLED;
        if (!is_command_enabled)
            continue;

        auto button_data = getToolbarButtonData(button_id);
        auto button_icon = QIcon(button_data.icons[0]);
        auto action = new  DmsToolbuttonAction(button_icon, tr("&export"), m_toolbar, button_data);
        m_toolbar->addAction(action);

        // connections
        connect(action, &DmsToolbuttonAction::triggered, action, &DmsToolbuttonAction::onToolbuttonPressed);
    }
}

std::string fillOpenConfigSourceCommand(const std::string_view command, const std::string_view filename, const std::string_view line)
{
    //"%env:ProgramFiles%\Notepad++\Notepad++.exe" "%F" -n%L
    std::string result = command.data();
    auto fn_part = result.find("%F");
    auto tmp_str = result.substr(fn_part + 2);
    if (fn_part != std::string::npos)
    {
        result.replace(fn_part, fn_part + 2, filename);
        result += tmp_str;
    }

    auto ln_part = result.find("%L");

    if (ln_part != std::string::npos)
        result.replace(ln_part, ln_part + 2, line);

    return result;
}

void MainWindow::openConfigSource()
{
    std::string command = GetGeoDmsRegKey("DmsEditor").c_str(); // TODO: replace with Qt application persistent data 
    std::string filename = getCurrentTreeItem()->GetConfigFileName().c_str();
    std::string line = std::to_string(getCurrentTreeItem()->GetConfigFileLineNr());
    std::string open_config_source_command = "";
    if (!filename.empty() && !line.empty() && !command.empty())
    {
        auto unexpanded_open_config_source_command = fillOpenConfigSourceCommand(command, filename, line);
        const TreeItem* ti = getCurrentTreeItem();

        if (!ti)
            ti = getRootTreeItem();

        if (!ti)
            open_config_source_command = AbstrStorageManager::GetFullStorageName("", unexpanded_open_config_source_command.c_str()).c_str();
        else
            open_config_source_command = AbstrStorageManager::GetFullStorageName(ti, SharedStr(unexpanded_open_config_source_command.c_str())).c_str();

        assert(!open_config_source_command.empty());
        reportF(SeverityTypeID::ST_MajorTrace, open_config_source_command.c_str());
        QProcess process;
        process.startDetached(open_config_source_command.c_str());
    }
}

void MainWindow::exportOkButton()
{
    int i = 0;
}

TIC_CALL BestItemRef TreeItem_GetErrorSourceCaller(const TreeItem* src);

void MainWindow::stepToFailReason()
{
    auto ti = getCurrentTreeItem();
    if (!ti)
        return;

    BestItemRef result = TreeItem_GetErrorSourceCaller(ti);
    if (result.first)
        setCurrentTreeItem(const_cast<TreeItem*>(result.first));

    if (!result.second.empty())
    {
        auto textWithUnfoundPart = m_current_item_bar->text() + " " + result.second.c_str();
        m_current_item_bar->setText(textWithUnfoundPart);
    }
}

void MainWindow::runToFailReason()
{
    auto current_ti = getCurrentTreeItem();
    while (current_ti->WasFailed())
    {
        stepToFailReason();
        auto new_ti = getCurrentTreeItem();
        if (current_ti == new_ti)
            break;
        current_ti = new_ti;
    }   
}

auto getAvailableDrivers() -> std::vector<gdal_driver_id>
{
    std::vector<gdal_driver_id> available_drivers;
    available_drivers.emplace_back("ESRI Shapefile", "ESRI Shapefile / DBF", "shp", driver_characteristics::tableset_is_folder);
    available_drivers.emplace_back("GPKG", "GeoPackage vector (*.gpkg)", nullptr);
    available_drivers.emplace_back("CSV", "Comma Separated Value (*.csv)", "csv", driver_characteristics::native_is_default | driver_characteristics::tableset_is_folder);
    available_drivers.emplace_back("GML", "Geography Markup Language (*.GML)", nullptr);
    available_drivers.emplace_back("GeoJSON", "GeoJSON", nullptr);
    available_drivers.emplace_back("GTiff", "GeoTIFF File Format", "tif", driver_characteristics::is_raster | driver_characteristics::tableset_is_folder);
    available_drivers.emplace_back("netCDF", "NetCDF: Network Common Data Form", nullptr, driver_characteristics::is_raster);
    available_drivers.emplace_back("PNG", "Portable Network Graphics (*.png)", nullptr, driver_characteristics::is_raster | driver_characteristics::tableset_is_folder);
    available_drivers.emplace_back("JPEG", "JPEG JFIF File Format (*.jpg)", nullptr, driver_characteristics::is_raster | driver_characteristics::tableset_is_folder);
    return available_drivers;
}

void MainWindow::options()
{
    if (!m_options_window)
        m_options_window = new DmsOptionsWindow(this);

    m_options_window->show();
}

void MainWindow::exportPrimaryData()
{
    QWidget* export_primary_data_window = new QDialog(this);
    export_primary_data_window->setWindowTitle(QString("Export ") + getCurrentTreeItem()->GetFullName().c_str());
    auto grid_layout_box = new QGridLayout(export_primary_data_window);
    auto format_label = new QLabel("Format", this);

    auto format_driver_selection_box = new QComboBox(this);
    QStringList driver_namesnames;
    auto available_drivers = getAvailableDrivers();
    for (auto& driver : available_drivers)
        format_driver_selection_box->addItem(driver.Caption());


    format_driver_selection_box->addItems(driver_namesnames);
    auto format_native_driver_checkbox = new QCheckBox("Use native driver", this);
    grid_layout_box->addWidget(format_label, 0, 0);
    grid_layout_box->addWidget(format_driver_selection_box, 0, 1);
    grid_layout_box->addWidget(format_native_driver_checkbox, 0, 2);

    auto export_button = new QPushButton("Export");
    connect(export_button, &QPushButton::clicked, this, &MainWindow::exportOkButton);
    grid_layout_box->addWidget(export_button, 3, 0);

    auto cancel_button = new QPushButton("Cancel");
    connect(cancel_button, &QPushButton::clicked, this, &MainWindow::exportOkButton);
    grid_layout_box->addWidget(cancel_button, 3, 1);

    export_primary_data_window->setWindowModality(Qt::ApplicationModal);
    export_primary_data_window->show();
}

void MainWindow::createView(ViewStyle viewStyle)
{
    try
    {
        static UInt32 s_ViewCounter = 0;

        auto currItem = getCurrentTreeItem();
        if (!currItem)
            return;

        auto desktopItem = GetDefaultDesktopContainer(m_root); // rootItem->CreateItemFromPath("DesktopInfo");
        auto viewContextItem = desktopItem->CreateItemFromPath(mySSPrintF("View%d", s_ViewCounter++).c_str());

        HWND hWndMain = (HWND)winId();

        //auto dataViewDockWidget = new ads::CDockWidget("DefaultView");
        SuspendTrigger::Resume();
        auto dms_mdi_subwindow = new QDmsViewArea(m_mdi_area.get(), hWndMain, viewContextItem, currItem, viewStyle);
        m_mdi_area->addSubWindow(dms_mdi_subwindow);
        dms_mdi_subwindow->showMaximized();
        dms_mdi_subwindow->setMinimumSize(200, 150);
//        m_mdi_area->show();

        //mdiSubWindow->setMinimumSize(200, 150);
        //mdiSubWindow->showMaximized();

        //    m_dms_views.emplace_back(name, vs, dv);
        //    m_dms_view_it = --m_dms_views.end();
        //    dvm_dms_view_it->UpdateParentWindow(); // m_Views.back().UpdateParentWindow(); // Needed before InitWindow

        //m_mdi_area->

        /*dataViewDockWidget->setWidget(dmsControl);
        dataViewDockWidget->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
        dataViewDockWidget->resize(250, 150);
        dataViewDockWidget->setMinimumSize(200, 150);
        m_DockManager->addDockWidget(ads::DockWidgetArea::CenterDockWidgetArea, dataViewDockWidget, centralDockArea);*/
    }
     
    catch (...)
    {
        auto errMsg = catchException(false);
        MainWindow::EventLog(SeverityTypeID::ST_Error, errMsg->Why().c_str());
    }
}

void MainWindow::defaultView()
{
    createView(SHV_GetDefaultViewStyle(getCurrentTreeItem()));
}

void MainWindow::mapView()
{
    createView(ViewStyle::tvsMapView);
}

void MainWindow::tableView()
{
    createView(ViewStyle::tvsTableView);
}

void geoDMSContextMessage(ClientHandle clientHandle, CharPtr msg)
{
    auto dms_main_window = reinterpret_cast<MainWindow*>(clientHandle);
    dms_main_window->statusBar()->showMessage(msg);
    return;
}

void MainWindow::CloseConfig()
{
    if (m_mdi_area)
        for (auto* sw : m_mdi_area->subWindowList())
            sw->close();

    if (m_root)
    {
        m_detail_pages->setActiveDetailPage(ActiveDetailPage::NONE); // reset ValueInfo cached results
        m_treeview->reset();
        m_dms_model->setRoot(nullptr);
        m_root->EnableAutoDelete();
        m_root = nullptr;
    }
}

void MainWindow::LoadConfig(CharPtr configFilePath)
{
    CloseConfig();

    auto fileNameCharPtr = configFilePath + StrLen(configFilePath);
    while (fileNameCharPtr != configFilePath)
    {
        char delimCandidate = *--fileNameCharPtr;
        if (delimCandidate == '\\' || delimCandidate == '/')
        {
            auto dirName = SharedStr(configFilePath, fileNameCharPtr);
            SetCurrentDir(dirName.c_str());
            ++fileNameCharPtr;
            break;
        }
    }
    m_currConfigFileName = fileNameCharPtr;
    auto newRoot = DMS_CreateTreeFromConfiguration(m_currConfigFileName.c_str());
    if (newRoot)
    {
        m_root = newRoot;

        m_treeview->setItemDelegate(new TreeItemDelegate());
        m_treeview->setRootIsDecorated(true);
        m_treeview->setUniformRowHeights(true);
        m_treeview->setItemsExpandable(true);
        m_treeview->setModel(m_dms_model.get());
        m_treeview->setRootIndex(m_treeview->rootIndex().parent());// m_treeview->model()->index(0, 0));
        m_treeview->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_treeview, &DmsTreeView::customContextMenuRequested, m_treeview, &DmsTreeView::showTreeviewContextMenu);
        m_treeview->scrollTo({}); // :/res/images/TV_branch_closed_selected.png
        m_treeview->setDragEnabled(true);
        m_treeview->setDragDropMode(QAbstractItemView::DragOnly);
        m_treeview->setStyleSheet(
            "QTreeView::branch:has-siblings:!adjoins-item {\n"
            "    border-image: url(:/res/images/TV_vline.png) 0;\n"
            "}\n"
            "QTreeView::branch:!has-children:!has-siblings:adjoins-item {\n"
            "    border-image: url(:/res/images/TV_branch_end.png) 0;\n"
            "}\n"
            "QTreeView::branch:has-siblings:adjoins-item {\n"
            "    border-image: url(:/res/images/TV_branch_more.png) 0;\n"
            "}\n"
            "QTreeView::branch:has-children:!has-siblings:closed,"
            "QTreeView::branch:closed:has-children:has-siblings {"
            "        border-image: none;"
            "        image: url(:/res/images/right_arrow.png);"
            "}"
            "QTreeView::branch:closed:hover:has-children {"
            "        border-image: none;"
            "        image: url(:/res/images/right_arrow_hover.png);"
            "}"
            "QTreeView::branch:open:has-children:!has-siblings,"
            "QTreeView::branch:open:has-children:has-siblings {"
            "           border-image: none;"
            "           image: url(:/res/images/down_arrow.png);"
            "}"
            "QTreeView::branch:open:hover:has-children {"
            "           border-image: none;"
            "           image: url(:/res/images/down_arrow_hover.png);"
            "}");
    }
    m_dms_model->setRoot(m_root);
    setCurrentTreeItem(m_root); // as an example set current item to root, which emits signal currentItemChanged
}

void MainWindow::OnViewAction(const TreeItem* tiContext, CharPtr sAction, Int32 nCode, Int32 x, Int32 y, bool doAddHistory, bool isUrl, bool mustOpenDetailsPage)
{
    MainWindow::TheOne()->m_detail_pages->DoViewAction(const_cast<TreeItem*>(tiContext), sAction);
}

void CppExceptionTranslator(CharPtr msg)
{
    MainWindow::EventLog(SeverityTypeID::ST_Error, msg);
}

void AnyTreeItemStateHasChanged(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode)
{
    auto mainWindow = reinterpret_cast<MainWindow*>(clientHandle);
    if (notificationCode == NC_Deleting)
    {
        // TODO: remove self from any representation to avoid accessing it's dangling pointer
    }

    // MainWindow could already be destroyed
    if (s_CurrMainWindow)
    {
        assert(s_CurrMainWindow == mainWindow);
        mainWindow->getDmsTreeViewPtr()->update(); // this actually only invalidates any drawn area and causes repaint later
    }
}

void MainWindow::setupDmsCallbacks()
{
    DMS_SetGlobalCppExceptionTranslator(CppExceptionTranslator);
    DMS_RegisterMsgCallback(&geoDMSMessage, m_eventlog);
    DMS_SetContextNotification(&geoDMSContextMessage, this);
    DMS_RegisterStateChangeNotification(AnyTreeItemStateHasChanged, this);
    SHV_SetCreateViewActionFunc(&OnViewAction);
}



void MainWindow::createActions()
{
    auto fileMenu = menuBar()->addMenu(tr("&File"));
    auto current_item_bar_container = addToolBar(tr("test"));
    m_current_item_bar = std::make_unique<DmsCurrentItemBar>(this);
    
    current_item_bar_container->addWidget(m_current_item_bar.get());

    connect(m_current_item_bar.get(), &DmsCurrentItemBar::editingFinished, m_current_item_bar.get(), &DmsCurrentItemBar::onEditingFinished);

    addToolBarBreak();

    connect(m_mdi_area.get(), &QDmsMdiArea::subWindowActivated, this, &MainWindow::updateToolbar);
    auto openIcon = QIcon::fromTheme("document-open", QIcon(":res/images/open.png"));
    auto fileOpenAct = new QAction(openIcon, tr("&Open Configuration File"), this);
    fileOpenAct->setShortcuts(QKeySequence::Open);
    fileOpenAct->setStatusTip(tr("Open an existing configuration file"));
    connect(fileOpenAct, &QAction::triggered, this, &MainWindow::fileOpen);
    fileMenu->addAction(fileOpenAct);

    auto reOpenAct = new QAction(openIcon, tr("&Reopen current Configuration"), this);
    reOpenAct->setShortcuts(QKeySequence::Refresh);
    reOpenAct->setStatusTip(tr("Reopen the current configuration and reactivate the current active item"));
    connect(reOpenAct, &QAction::triggered, this, &MainWindow::reOpen);
    fileMenu->addAction(reOpenAct);
    //fileToolBar->addAction(reOpenAct);

    fileMenu->addSeparator();
    auto epdm = fileMenu->addMenu(tr("Export Primary Data")); 
    auto epdmBmp = new QAction(tr("Bitmap (*.tif or *.bmp)")); // TODO: memory leak, QAction will not transfer ownership from addAction
    auto epdmDbf = new QAction(tr("Table (*.dbf with a *.shp and *.shx if Feature data can be related)"));
    auto epdmCsv = new QAction(tr("Text Table (*.csv with semiColon Separated Values"));
    epdm->addAction(epdmBmp);
    epdm->addAction(epdmDbf);
    epdm->addAction(epdmCsv);

    fileMenu->addSeparator();
    QAction *quitAct = fileMenu->addAction(tr("&Quit"), qApp, &QCoreApplication::quit);
    quitAct->setShortcuts(QKeySequence::Quit);
    quitAct->setStatusTip(tr("Quit the application"));

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));

    // export primary data
    m_export_primary_data_action = std::make_unique<QAction>(tr("&Export Primary Data"));
    connect(m_export_primary_data_action.get(), &QAction::triggered, this, &MainWindow::exportPrimaryData);

    // step to failreason
    m_step_to_failreason_action = std::make_unique<QAction>(tr("&Step up to FailReason"));
    m_step_to_failreason_action->setShortcut(QKeySequence(tr("F2")));
    connect(m_step_to_failreason_action.get(), &QAction::triggered, this, &MainWindow::stepToFailReason);

    // go to causa prima
    m_go_to_causa_prima_action = std::make_unique<QAction>(tr("&Run up to Causa Prima (i.e. repeated Step up)"));
    m_go_to_causa_prima_action->setShortcut(QKeySequence(tr("Shift+F2")));
    connect(m_go_to_causa_prima_action.get(), &QAction::triggered, this, &MainWindow::runToFailReason);
    
    // open config source
    m_edit_config_source_action = std::make_unique<QAction>(tr("&Edit Config Source"));
    m_edit_config_source_action->setShortcut(QKeySequence(tr("Ctrl+E")));
    connect(m_edit_config_source_action.get(), &QAction::triggered, this, &MainWindow::openConfigSource);
    editMenu->addAction(m_edit_config_source_action.get());

    // update treeitem
    m_update_treeitem_action = std::make_unique<QAction>(tr("&Update TreeItem"));
    m_update_treeitem_action->setShortcut(QKeySequence(tr("Ctrl+U")));
    //connect(m_update_treeitem_action.get(), &QAction::triggered, this, & #TODO);

    // update subtree
    m_update_subtree_action = std::make_unique<QAction>(tr("&Update Subtree"));
    m_update_subtree_action->setShortcut(QKeySequence(tr("Ctrl+T")));
    //connect(m_update_subtree_action.get(), &QAction::triggered, this, & #TODO);
    
    // invalidate action
    m_invalidate_action = std::make_unique<QAction>(tr("&Invalidate"));
    m_invalidate_action->setShortcut(QKeySequence(tr("Ctrl+I")));
    //connect(m_invalidate_action.get(), &QAction::triggered, this, & #TODO);


    auto viewMenu = menuBar()->addMenu(tr("&View"));

    m_defaultview_action = std::make_unique<QAction>(tr("Default View"));
    m_defaultview_action->setShortcut(QKeySequence(tr("Ctrl+Alt+D")));
    m_defaultview_action->setStatusTip(tr("Open current selected TreeItem's default view."));
    connect(m_defaultview_action.get(), &QAction::triggered, this, &MainWindow::defaultView);
    viewMenu->addAction(m_defaultview_action.get());

    // table view
    m_tableview_action = std::make_unique<QAction>(tr("&Table View"));
    m_tableview_action->setShortcut(QKeySequence(tr("Ctrl+D")));
    m_tableview_action->setStatusTip(tr("Open current selected TreeItem's in a table view."));
    connect(m_tableview_action.get(), &QAction::triggered, this, &MainWindow::tableView);
    viewMenu->addAction(m_tableview_action.get());

    // map view
    m_mapview_action = std::make_unique<QAction>(tr("&Map View"));
    m_mapview_action->setShortcut(QKeySequence(tr("Ctrl+M")));
    m_mapview_action->setStatusTip(tr("Open current selected TreeItem's in a map view."));
    connect(m_mapview_action.get(), &QAction::triggered, this, &MainWindow::mapView);
    viewMenu->addAction(m_mapview_action.get());

    // histogram view
    m_histogramview_action = std::make_unique<QAction>(tr("&Histogram View"));
    m_histogramview_action->setShortcut(QKeySequence(tr("Ctrl+H")));
    //connect(m_histogramview_action.get(), &QAction::triggered, this, & #TODO);
    
    // provess schemes
    m_process_schemes_action = std::make_unique<QAction>(tr("&Process Schemes"));
    //connect(m_process_schemes_action.get(), &QAction::triggered, this, & #TODO);

    // code analysis
    m_code_analysis_action = std::make_unique<QAction>(tr("&Code analysis.."));
    //connect(m_code_analysis_action.get(), &QAction::triggered, this, & #TODO);


    // tools menu
    auto tools_menu = menuBar()->addMenu(tr("&Tools"));
    m_options_action = std::make_unique<QAction>(tr("&Options"));
    connect(m_options_action.get(), &QAction::triggered, this, &MainWindow::options);
    tools_menu->addAction(m_options_action.get());


    // window menu
    m_window_menu = menuBar()->addMenu(tr("&Window"));
    auto win1_action = new QAction(tr("&Tile Windows"), this);
    win1_action->setShortcut(QKeySequence(tr("Ctrl+Alt+W")));
    connect(win1_action, &QAction::triggered, m_mdi_area.get(), &QMdiArea::tileSubWindows);

    //auto win2_action = new QAction(tr("&Tile Vertical"), this);
    //win2_action->setShortcut(QKeySequence(tr("Ctrl+Alt+V")));
    auto win3_action = new QAction(tr("&Cascade"), this);
    win3_action->setShortcut(QKeySequence(tr("Shift+Ctrl+W")));
    connect(win3_action, &QAction::triggered, m_mdi_area.get(), &QMdiArea::cascadeSubWindows);

    auto win4_action = new QAction(tr("&Close"), this);
    win4_action->setShortcut(QKeySequence(tr("Ctrl+W")));
    connect(win4_action, &QAction::triggered, m_mdi_area.get(), &QMdiArea::closeActiveSubWindow);

    auto win5_action = new QAction(tr("&Close All"), this);
    win5_action->setShortcut(QKeySequence(tr("Ctrl+L")));
    connect(win5_action, &QAction::triggered, m_mdi_area.get(), &QMdiArea::closeActiveSubWindow);

    /*vauto win6_action = new QAction(tr("&Close All But This"), this);
    win6_action->setShortcut(QKeySequence(tr("Ctrl+B")));
    connect(win6_action, &QAction::triggered, m_mdi_area.get(), &QMdiArea::closeActiveSubWindow);*/
    m_window_menu->addActions({win1_action, win3_action, win4_action, win5_action});
    m_window_menu->addSeparator();
    connect(m_window_menu, &QMenu::aboutToShow, this, &MainWindow::updateWindowMenu);

    // help menu
    auto helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAct = helpMenu->addAction(tr("&About GeoDms"), this, &MainWindow::aboutGeoDms);
    aboutAct->setStatusTip(tr("Show the application's About box"));
    QAction *aboutQtAct = helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
}

void MainWindow::updateWindowMenu() 
{
    static QList<QAction*> m_CurrWindowActions;
    for (auto* swPtr : m_CurrWindowActions)
    {
        m_window_menu->removeAction(swPtr);
        delete swPtr;
    }
    m_CurrWindowActions.clear(); // delete old actions;

    auto asw = m_mdi_area->currentSubWindow();
    for (auto* sw : m_mdi_area->subWindowList())
    {
        auto qa = new QAction(sw->windowTitle(), this);
        connect(qa, &QAction::triggered, sw, [this, sw] { this->m_mdi_area->setActiveSubWindow(sw); });
        if (sw == asw)
        {
            qa->setCheckable(true);
            qa->setChecked(true);
        }
        m_window_menu->addAction(qa);
        m_CurrWindowActions.append(qa);
    }
    m_window_menu->addActions(m_CurrWindowActions);
}

void MainWindow::updateCaption()
{
    VectorOutStreamBuff buff;
    FormattedOutStream out(&buff, FormattingFlags());
    if (!m_currConfigFileName.empty())
        out << m_currConfigFileName << " in ";
    out << GetCurrentDir();

    if (!GetRegStatusFlags() & RSF_AdminMode) out << "[Hiding]";
    if (!GetRegStatusFlags() & RSF_ShowStateColors) out << "[HSC]";
    if (GetRegStatusFlags() & RSF_TraceLogFile) out << "[TL]";
    if (!IsMultiThreaded0()) out << "[C0]";
    if (!IsMultiThreaded1()) out << "[C1]";
    if (!IsMultiThreaded2()) out << "[C2]";
    if (!IsMultiThreaded3()) out << "[C3]";
    if (ShowThousandSeparator()) out << "[,]";

    out << " - ";
    out << DMS_GetVersion();
    out << char(0);

    setWindowTitle(buff.GetData());
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
    auto configuration_page_act = std::make_unique<QAction>(properties_icon, tr("&Configuration"), this);
    configuration_page_act->setStatusTip("Show item configuration script of the active item in the detail-page; the script is generated from the internal representation of the item in the syntax of the read .dms file and is therefore similar to how it was defined there.");
//    configuration_page_act->setWhatsThis("");
    detail_pages_toolBar->addAction(configuration_page_act.get());
    connect(configuration_page_act.get(), &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleConfiguration);
    m_garbage_bag |= std::move(configuration_page_act);

    const QIcon value_info_icon = QIcon::fromTheme("detailpages-valueinfo", QIcon(":res/images/DP_ValueInfo.bmp"));
    QAction* value_info_page_act = new QAction(value_info_icon, tr("&Value info"), this);
    detail_pages_toolBar->addAction(value_info_page_act);
}

void MainWindow::createDetailPagesDock()
{
    m_detailpages_dock = new QDockWidget(QObject::tr("DetailPages"), this);
    m_detailpages_dock->setTitleBarWidget(new QWidget(m_detailpages_dock));
    m_detail_pages = new DmsDetailPages(m_detailpages_dock);
    m_detailpages_dock->setWidget(m_detail_pages);
    addDockWidget(Qt::RightDockWidgetArea, m_detailpages_dock);
    m_detail_pages->connectDetailPagesAnchorClicked();
}

void MainWindow::createDmsHelperWindowDocks()
{
    createDetailPagesDock();
//    m_detail_pages->setDummyText();

    m_treeview = createTreeview(this);    
    m_eventlog = createEventLog(this);
}