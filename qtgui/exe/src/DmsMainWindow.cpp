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

#include "utl/mySPrintF.h"
#include "utl/splitPath.h"

#include "DataView.h"
#include "TreeItem.h"
#include "SessionData.h"

#include "ClcInterface.h"
#include "ShvDllInterface.h"

#include <QtWidgets>
#include <QCompleter>
#include <QMdiArea>

#include "DmsMainWindow.h"
#include "DmsEventLog.h"
#include "DmsViewArea.h"
#include "DmsTreeView.h"
#include "DmsDetailPages.h"
#include "DmsOptions.h"
#include "DmsExport.h"
#include "DataView.h"
#include "StateChangeNotification.h"
#include <regex>

#include "dataview.h"

static MainWindow* s_CurrMainWindow = nullptr;

void DmsFileChangedWindow::ignore()
{
    done(QDialog::Rejected);
}

void DmsFileChangedWindow::reopen()
{
    MainWindow::TheOne()->reOpen();
    done(QDialog::Accepted);
}

void DmsFileChangedWindow::onAnchorClicked(const QUrl& link)
{
    auto clicked_file_link = link.toString().toStdString();
    MainWindow::TheOne()->openConfigSourceDirectly(clicked_file_link, "0");
}

DmsFileChangedWindow::DmsFileChangedWindow(QWidget* parent = nullptr)
{
    setWindowTitle(QString("Source changed.."));
    setMinimumSize(600, 200);

    auto grid_layout = new QGridLayout(this);
    m_message = new QTextBrowser(this);
    m_message->setOpenLinks(false);
    m_message->setOpenExternalLinks(false);
    connect(m_message, &QTextBrowser::anchorClicked, this, &DmsFileChangedWindow::onAnchorClicked);
    grid_layout->addWidget(m_message, 0, 0, 1, 3);

    // ok/apply/cancel buttons
    auto box_layout = new QHBoxLayout(this);
    m_ignore = new QPushButton("Ignore");
    m_ignore->setMaximumSize(75, 30);
    m_ignore->setAutoDefault(true);
    m_ignore->setDefault(true);


    m_reopen = new QPushButton("Reopen");
    connect(m_ignore, &QPushButton::released, this, &DmsFileChangedWindow::ignore);
    connect(m_reopen, &QPushButton::released, this, &DmsFileChangedWindow::reopen);
    m_reopen->setMaximumSize(75, 30);
    box_layout->addWidget(m_ignore);
    box_layout->addWidget(m_reopen);
    grid_layout->addLayout(box_layout, 14, 0, 1, 3);


    setWindowModality(Qt::ApplicationModal);
}

void DmsFileChangedWindow::setFileChangedMessage(std::string_view changed_files)
{
    std::string file_changed_message_markdown = "The following files have been changed:\n\n";
    size_t curr_pos = 0;
    while (curr_pos < changed_files.size())
    {
        auto curr_line_end = changed_files.find_first_of('\n', curr_pos);
        auto link = std::string(changed_files.substr(curr_pos, curr_line_end - curr_pos));
        file_changed_message_markdown += "[" + link + "](" + link + ")\n";
        curr_pos = curr_line_end + 1;
    }
    file_changed_message_markdown += "\n\nDo you want to reopen the configuration?";
    m_message->setMarkdown(file_changed_message_markdown.c_str());
}

void DmsErrorWindow::ignore()
{
    done(QDialog::Rejected);
}
void DmsErrorWindow::terminate()
{
    
    QCoreApplication::exit(EXIT_FAILURE);
    done(QDialog::Rejected);
}

void DmsErrorWindow::reopen()
{
    MainWindow::TheOne()->reOpen();
    done(QDialog::Accepted);
}

struct link_info
{
    bool is_valid = false;
    size_t start = 0;
    size_t stop = 0;
    size_t endline = 0;
    std::string filename = "";
    std::string line = "";
    std::string col = "";
};

auto getLinkFromErrorMessage(std::string_view error_message, unsigned int lineNumber = 0) -> link_info
{
    std::string html_error_message = "";
    //auto error_message_text = std::string(error_message->Why().c_str());
    std::size_t currPos = 0, currLineNumber = 0;
    link_info lastFoundLink;
    while (currPos < error_message.size())
    {
        auto currLineEnd = error_message.find_first_of('\n', currPos);
        if (currLineEnd == std::string::npos)
            currLineEnd = error_message.size();

        auto lineView = std::string_view(&error_message[currPos], currLineEnd - currPos);
        auto round_bracked_open_pos = lineView.find_first_of('(');
        auto comma_pos = lineView.find_first_of(',');
        auto round_bracked_close_pos = lineView.find_first_of(')');

        if (round_bracked_open_pos < comma_pos && comma_pos < round_bracked_close_pos && round_bracked_close_pos != std::string::npos)
        {
            auto filename = lineView.substr(0, round_bracked_open_pos);
            auto line_number = lineView.substr(round_bracked_open_pos + 1, comma_pos - (round_bracked_open_pos + 1));
            auto col_number = lineView.substr(comma_pos + 1, round_bracked_close_pos - (comma_pos + 1));

            lastFoundLink = link_info(true, currPos, currPos + round_bracked_close_pos, currLineEnd, std::string(filename), std::string(line_number), std::string(col_number));
        }
        if (lineNumber <= currLineNumber && lastFoundLink.is_valid)
            break;

        currPos = currLineEnd + 1;
        currLineNumber++;
    }

    return lastFoundLink;
}

void DmsErrorWindow::onAnchorClicked(const QUrl& link)
{
    auto clicked_error_link = link.toString().toStdString();
    auto parsed_clicked_error_link = getLinkFromErrorMessage(clicked_error_link);
    MainWindow::TheOne()->openConfigSourceDirectly(parsed_clicked_error_link.filename, parsed_clicked_error_link.line);
}

DmsErrorWindow::DmsErrorWindow(QWidget* parent = nullptr)
{
    setWindowTitle(QString("Error"));
    setMinimumSize(800, 400);

    auto grid_layout = new QGridLayout(this);
    m_message = new QTextBrowser(this);
    m_message->setOpenLinks(false);
    m_message->setOpenExternalLinks(false);
    connect(m_message, &QTextBrowser::anchorClicked, this, &DmsErrorWindow::onAnchorClicked);
    grid_layout->addWidget(m_message, 0, 0, 1, 3);

    // ok/apply/cancel buttons
    auto box_layout = new QHBoxLayout(this);
    m_ignore = new QPushButton("Ignore");
    m_ignore->setMaximumSize(75, 30);
    m_ignore->setAutoDefault(true);
    m_ignore->setDefault(true);
    m_terminate = new QPushButton("Terminate");
    m_terminate->setMaximumSize(75, 30);

    m_reopen = new QPushButton("Reopen");
    connect(m_ignore, &QPushButton::released, this, &DmsErrorWindow::ignore);
    connect(m_terminate, &QPushButton::released, this, &DmsErrorWindow::terminate);
    connect(m_reopen, &QPushButton::released, this, &DmsErrorWindow::reopen);
    m_reopen->setMaximumSize(75, 30);
    box_layout->addWidget(m_ignore);
    box_layout->addWidget(m_terminate);
    box_layout->addWidget(m_reopen);
    grid_layout->addLayout(box_layout, 14, 0, 1, 3);


    setWindowModality(Qt::ApplicationModal);
}

MainWindow::MainWindow(CmdLineSetttings& cmdLineSettings)
{ 
    assert(s_CurrMainWindow == nullptr);
    s_CurrMainWindow = this;

    m_file_changed_window = new DmsFileChangedWindow(this);
    m_error_window = new DmsErrorWindow(this);
    m_export_window = new DmsExportWindow(this);

    m_mdi_area = std::make_unique<QDmsMdiArea>(this);

    QFont dms_text_font(":/res/fonts/dmstext.ttf", 10);
    QApplication::setFont(dms_text_font);

    setCentralWidget(m_mdi_area.get());

    m_mdi_area->show();

    createStatusBar();
    createDmsHelperWindowDocks();
    createRightSideToolbar();

    // connect current item changed signal to appropriate slots
    connect(this, &MainWindow::currentItemChanged, m_detail_pages, &DmsDetailPages::newCurrentItem);

    setupDmsCallbacks();

    m_dms_model = std::make_unique<DmsModel>();
    m_treeview->setModel(m_dms_model.get());

    createActions();

    // read initial last config file
    if (!cmdLineSettings.m_NoConfig)
    {
        if (cmdLineSettings.m_ConfigFileName.empty())
            cmdLineSettings.m_ConfigFileName = GetGeoDmsRegKey("LastConfigFile");
        if (!cmdLineSettings.m_ConfigFileName.empty())
            LoadConfig(cmdLineSettings.m_ConfigFileName.c_str()); // TODO: return value unhandled
    }

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
    DMS_ReleaseMsgCallback(&geoDMSMessage, this);
    DMS_SetContextNotification(nullptr, nullptr);
    SHV_SetCreateViewActionFunc(nullptr);
}

void DmsCurrentItemBar::setDmsCompleter()
{
    auto dms_model = MainWindow::TheOne()->m_dms_model.get();
    TreeModelCompleter* completer = new TreeModelCompleter(this);
    completer->setModel(dms_model);
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
        auto treeView = MainWindow::TheOne()->m_treeview.get();
        treeView->scrollTo(treeView->currentIndex(), QAbstractItemView::ScrollHint::PositionAtBottom);
    }
}

bool MainWindow::IsExisting()
{
    return s_CurrMainWindow;
}

MainWindow* MainWindow::TheOne()
{
    assert(IsMainThread()); // or use a mutex to guard access to TheOne.
    assert(s_CurrMainWindow);
    return s_CurrMainWindow;
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

DmsRecentFileButtonAction::DmsRecentFileButtonAction(size_t index, std::string_view dms_file_full_path, QObject* parent)
    :QAction(QString(index < 10 ? QString("&") : "") + QString::number(index) + ". " + ConvertDosFileName(SharedStr(dms_file_full_path.data())).c_str())
{
    m_cfg_file_path = dms_file_full_path;
}

void DmsRecentFileButtonAction::onToolbuttonPressed()
{
    auto main_window = MainWindow::TheOne();
    main_window->LoadConfig(m_cfg_file_path.c_str());
}

DmsToolbuttonAction::DmsToolbuttonAction(const QIcon& icon, const QString& text, QObject* parent, ToolbarButtonData button_data, const ViewStyle vs)
    : QAction(icon, text, parent)
{
    assert(button_data.text.size()==2);

    if (vs == ViewStyle::tvsTableView)
        setStatusTip(button_data.text[0]);
    else
        setStatusTip(button_data.text[1]);

    if (button_data.ids.size() == 2) // toggle button
        setCheckable(true);

    m_data = std::move(button_data);
}

void DmsToolbuttonAction::onToolbuttonPressed()
{
    auto mdi_area = MainWindow::TheOne()->m_mdi_area.get();
    if (!mdi_area)
        return;

    auto subwindow_list = mdi_area->subWindowList(QMdiArea::WindowOrder::StackingOrder);
    auto subwindow_highest_in_z_order = subwindow_list.back();
    if (!subwindow_highest_in_z_order)
        return;

    auto dms_view_area = dynamic_cast<QDmsViewArea*>(subwindow_highest_in_z_order);
    if (!dms_view_area)
        return;

    auto number_of_button_states = getNumberOfStates();

    if (number_of_button_states - 1 == m_state) // state roll over
        m_state = 0;
    else
        m_state++;

    if (m_data.is_global) // ie. zoom-in or zoom-out can be active at a single time
    {
        dms_view_area->getDataView()->GetContents()->OnCommand(ToolButtonID::TB_Neutral);
        for (auto action : MainWindow::TheOne()->m_toolbar->actions())
        {
            auto dms_toolbar_action = dynamic_cast<DmsToolbuttonAction*>(action);
            if (!dms_toolbar_action)
                continue;

            if (dms_toolbar_action == this)
                continue;

            if (!dms_toolbar_action->m_data.is_global)
                continue;

            dms_toolbar_action->setChecked(false);
            dms_toolbar_action->m_state = 0;
        }
    }

    if (m_data.ids[0] == ToolButtonID::TB_Export)
    {
        auto dv = dms_view_area->getDataView();
        auto export_info = dv->GetExportInfo();
        if (dv->GetViewType() == tvsMapView)
            reportF(SeverityTypeID::ST_MajorTrace, "Exporting current viewport to bitmap in %s", export_info.m_FullFileNameBase);
        else
            reportF(SeverityTypeID::ST_MajorTrace, "Exporting current table to csv in %s", export_info.m_FullFileNameBase);
    }
    dms_view_area->getDataView()->GetContents()->OnCommand(m_data.ids[m_state]);
    
    if (m_data.icons.size()-1==m_state) // icon roll over
        setIcon(QIcon(m_data.icons[m_state]));
}

auto getToolbarButtonData(ToolButtonID button_id) -> ToolbarButtonData
{
    switch (button_id)
    {
    case TB_Export: return { {"Save to file as semicolon delimited text", "Export the viewport data to bitmaps file(s) using the export settings and the current ROI"}, {TB_Export}, {":/res/images/TB_save.bmp"}};
    case TB_TableCopy: return { {"Copy as semicolon delimited text to Clipboard",""}, {TB_TableCopy}, {":/res/images/TB_copy.bmp"}};
    case TB_Copy: return { {"Copy the visible contents as image to Clipboard","Copy the visible contents of the viewport to the Clipboard"}, {TB_Copy}, {":/res/images/TB_copy.bmp"}};
    case TB_CopyLC: return { {"","Copy the full contents of the LayerControlList to the Clipboard"}, {TB_CopyLC}, {":/res/images/TB_vcopy.bmp"}};
    case TB_ShowFirstSelectedRow: return { {"Show the first selected row","Make the extent of the selected elements fit in the ViewPort"}, {TB_ShowFirstSelectedRow}, {":/res/images/TB_table_show_first_selected.bmp"} };
    case TB_ZoomSelectedObj: return { {"Show the first selected row","Make the extent of the selected elements fit in the ViewPort"}, {TB_ZoomSelectedObj}, {":/res/images/TB_zoom_selected.bmp"}};
    case TB_SelectRows: return { {"Select row(s) by mouse-click (use Shift to add or Ctrl to deselect)",""}, {TB_SelectRows}, {":/res/images/TB_table_select_row.bmp"}};
    case TB_SelectAll: return { {"Select all rows","Select all elements in the active layer"}, {TB_SelectAll}, {":/res/images/TB_select_all.bmp"}};
    case TB_SelectNone: return { {"Deselect all rows","Deselect all elements in the active layer"}, {TB_SelectNone}, {":/res/images/TB_select_none.bmp"}};
    case TB_ShowSelOnlyOn: return { {"Show only selected rows","Show only selected elements"}, {TB_ShowSelOnlyOn, TB_ShowSelOnlyOff}, {":/res/images/TB_show_selected_features.bmp"}};
    case TB_TableGroupBy: return { {"Group by the highlighted columns",""}, {TB_TableGroupBy}, {":/res/images/TB_group_by.bmp"}};
    case TB_ZoomAllLayers: return { {"","Make the extents of all layers fit in the ViewPort"}, {TB_ZoomAllLayers}, {":/res/images/TB_zoom_all_layers.bmp"}};
    case TB_ZoomActiveLayer: return { {"","Make the extent of the active layer fit in the ViewPort"}, {TB_ZoomActiveLayer}, {":/res/images/TB_zoom_active_layer.bmp"}};
    case TB_ZoomIn2: return { {"","Zoom in by drawing a rectangle"}, {TB_Neutral, TB_ZoomIn2}, {":/res/images/TB_zoomin_button.bmp"}, true};
    case TB_ZoomOut2: return { {"","Zoom out by clicking on a ViewPort location"}, {TB_Neutral, TB_ZoomOut2}, {":/res/images/TB_zoomout_button.bmp"}, true};
    case TB_SelectObject: return { {"","Select elements in the active layer by mouse-click(use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectObject}, {":/res/images/TB_select_object.bmp"}, true};
    case TB_SelectRect: return { {"","Select elements in the active layer by drawing a rectangle(use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectRect}, {":/res/images/TB_select_rect.bmp"}, true};
    case TB_SelectCircle: return { {"","Select elements in the active layer by drawing a circle(use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectCircle}, {":/res/images/TB_select_circle.bmp"}, true};
    case TB_SelectPolygon: return { {"","Select elements in the active layer by drawing a polygon(use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectPolygon}, {":/res/images/TB_select_poly.bmp"}, true};
    case TB_SelectDistrict: return { {"","Select contiguous regions in the active layer by clicking on them (use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectDistrict}, {":/res/images/TB_select_district.bmp"}, true};
    case TB_Show_VP: return { {"","Toggle the layout of the ViewPort between{MapView only, with LayerControlList, with Overview and LayerControlList}"}, {TB_Show_VPLCOV,TB_Show_VP, TB_Show_VPLC}, {":/res/images/TB_toggle_layout_3.bmp", ":/res/images/TB_toggle_layout_1.bmp", ":/res/images/TB_toggle_layout_2.bmp"}};
    case TB_SP_All: return { {"","Toggle Palette Visibiliy between{All, Active Layer Only, None}"}, {TB_SP_All, TB_SP_Active, TB_SP_None}, {":/res/images/TB_toggle_palette.bmp"}};
    case TB_NeedleOn: return { {"","Show / Hide NeedleController"}, {TB_NeedleOff, TB_NeedleOn}, {":/res/images/TB_toggle_needle.bmp"}};
    case TB_ScaleBarOn: return { {"","Show / Hide ScaleBar"}, {TB_ScaleBarOff, TB_ScaleBarOn}, {":/res/images/TB_toggle_scalebar.bmp"}};
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

    static ToolButtonID available_table_buttons[] = { TB_Export, TB_TableCopy, TB_Copy, TB_Undefined, 
                                                      TB_ShowFirstSelectedRow, TB_SelectRows, TB_SelectAll, TB_SelectNone, TB_ShowSelOnlyOn, TB_Undefined, 
                                                      TB_TableGroupBy };

    static ToolButtonID available_map_buttons[] = { TB_Export , TB_Copy, TB_CopyLC, TB_Undefined,
                                                    TB_ZoomAllLayers, TB_ZoomActiveLayer, TB_ZoomIn2, TB_ZoomOut2, TB_Undefined,
                                                    TB_ZoomSelectedObj,TB_SelectObject,TB_SelectRect,TB_SelectCircle,TB_SelectPolygon,TB_SelectDistrict,TB_SelectAll,TB_SelectNone,TB_ShowSelOnlyOn, TB_Undefined,
                                                    TB_Show_VP,TB_SP_All,TB_NeedleOn,TB_ScaleBarOn };

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
        if (button_id == TB_Undefined)
        {
            m_toolbar->addSeparator();
            continue;
        }

        auto button_data = getToolbarButtonData(button_id);
        auto button_icon = QIcon(button_data.icons[0]);
        auto action = new DmsToolbuttonAction(button_icon, view_style==ViewStyle::tvsTableView ? button_data.text[0] : button_data.text[1], m_toolbar, button_data, view_style);
        auto is_command_enabled = dv->OnCommandEnable(button_id) == CommandStatus::ENABLED;
        if (!is_command_enabled)
            action->setDisabled(true);

        m_toolbar->addAction(action);

        // connections
        connect(action, &DmsToolbuttonAction::triggered, action, &DmsToolbuttonAction::onToolbuttonPressed);
    }
}

bool MainWindow::event(QEvent* event)
{
    if (event->type() == QEvent::WindowActivate)
    {
        auto vos = ReportChangedFiles(true); // TODO: report changed files not always returning if files are changed.
        if (vos.CurrPos())
        {
            auto changed_files = std::string(vos.GetData(), vos.GetDataEnd());
            m_file_changed_window->setFileChangedMessage(changed_files);
            m_file_changed_window->show();
        }
    }

    return QMainWindow::event(event);
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

void MainWindow::openConfigSourceDirectly(std::string_view filename, std::string_view line)
{
    std::string command = GetGeoDmsRegKey("DmsEditor").c_str(); // TODO: replace with Qt application persistent data 
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

void MainWindow::openConfigSource()
{
    std::string filename = getCurrentTreeItem()->GetConfigFileName().c_str();
    std::string line = std::to_string(getCurrentTreeItem()->GetConfigFileLineNr());
    openConfigSourceDirectly(filename, line);
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

void MainWindow::toggle_treeview()
{
    bool isVisible = m_treeview->isVisible();
    m_treeview->setVisible(!isVisible);
}

void MainWindow::toggle_detailpages()
{
    bool isVisible = m_right_side_toolbar->isVisible();
    m_detail_pages->setVisible(!isVisible);
    m_right_side_toolbar->setVisible(!isVisible);
}

void MainWindow::toggle_eventlog()
{
    bool isVisible = m_eventlog->isVisible();
    m_eventlog->setVisible(!isVisible);
}

void MainWindow::toggle_toolbar()
{
    if (!m_toolbar)
        return; // when there is no DataView opened, there is also no (in)visible toolbar

    bool isVisible = m_toolbar->isVisible();
    m_toolbar->setVisible(!isVisible);
}

void MainWindow::toggle_currentitembar()
{
    bool isVisible = m_current_item_bar_container->isVisible();
    m_current_item_bar_container->setVisible(!isVisible);
}

void MainWindow::gui_options()
{
    if (!m_gui_options_window)
        m_gui_options_window = new DmsGuiOptionsWindow(this);

    m_gui_options_window->show();
}

void MainWindow::advanced_options()
{
    if (!m_advanced_options_window)
        m_advanced_options_window = new DmsAdvancedOptionsWindow(this);

    m_advanced_options_window->show();
}

void MainWindow::config_options()
{
    if (!m_config_options_window)
        m_config_options_window = new DmsConfigOptionsWindow(this);

    m_config_options_window->show();
}

void MainWindow::code_analysis_set_source()
{
   TreeItem_SetAnalysisSource(getCurrentTreeItem());
}

void MainWindow::code_analysis_set_target()
{
    TreeItem_SetAnalysisTarget(getCurrentTreeItem(), true);
}

void MainWindow::code_analysis_add_target()
{
    TreeItem_SetAnalysisTarget(getCurrentTreeItem(), false);
}

void MainWindow::code_analysis_clr_targets()
{
    TreeItem_SetAnalysisSource(nullptr);
//    TreeItem_SetAnalysisTarget(nullptr, true);  // REMOVE, al gedaan door SetAnalysisSource
}


void MainWindow::error(ErrMsgPtr error_message_ptr)
{
    std::string error_message_markdown = "";
    auto error_message = std::string(error_message_ptr->GetAsText().c_str());
    
    std::size_t curr_pos = 0;
    while (true)
    {
        auto link = getLinkFromErrorMessage(std::string_view(&error_message[curr_pos]));
        if (!link.is_valid)
            break;
        auto full_link = link.filename + "(" + link.line + "," + link.col + ")";
        error_message_markdown += (error_message.substr(curr_pos, link.start) + "["+ full_link +"]("+ full_link +")");
        curr_pos = curr_pos + link.stop + 1;
    }
    error_message_markdown += error_message.substr(curr_pos);

    TheOne()->m_error_window->setErrorMessage(std::regex_replace(error_message_markdown, std::regex("\n"), "\n\n").c_str());
    TheOne()->m_error_window->exec();
}

void MainWindow::exportPrimaryData()
{
    if (!m_export_window)
        m_export_window = new DmsExportWindow(this);

    m_export_window->prepare();
    m_export_window->show();
}

void MainWindow::createView(ViewStyle viewStyle)
{
    try
    {
        //auto test = QScreen::devicePixelRatio();
        static UInt32 s_ViewCounter = 0;

        auto currItem = getCurrentTreeItem();
        if (!currItem)
            return;

        auto desktopItem = GetDefaultDesktopContainer(m_root); // rootItem->CreateItemFromPath("DesktopInfo");
        auto viewContextItem = desktopItem->CreateItemFromPath(mySSPrintF("View%d", s_ViewCounter++).c_str());

        //auto dataViewDockWidget = new ads::CDockWidget("DefaultView");
        SuspendTrigger::Resume();
        auto dms_mdi_subwindow = new QDmsViewArea(m_mdi_area.get(), viewContextItem, currItem, viewStyle);
        m_mdi_area->addSubWindow(dms_mdi_subwindow);
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
        error(errMsg);
        //MainWindow::EventLog(SeverityTypeID::ST_Error, errMsg->Why().c_str());
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
        m_detail_pages->leaveThisConfig(); // reset ValueInfo cached results
        m_dms_model->reset();
        m_treeview->reset();

        m_dms_model->setRoot(nullptr);
        m_root->EnableAutoDelete();
        m_root = nullptr;
    }
}

auto configIsInRecentFiles(std::string_view cfg, const std::vector<std::string>& files) -> Int32
{
    std::string dos_cfg_name = cfg.data();
    std::replace(dos_cfg_name.begin(), dos_cfg_name.end(), '/', '\\');
    auto it = std::find(files.begin(), files.end(), dos_cfg_name);
    if (it == files.end())
        return -1;
    return it - files.begin();
}

auto removeDuplicateStringsFromVector(std::vector<std::string>& strings)
{
    std::sort(strings.begin(), strings.end());
    strings.erase(std::unique(strings.begin(), strings.end()), strings.end());
}

void MainWindow::cleanRecentFilesThatDoNotExist()
{
    auto recent_files_from_registry = GetGeoDmsRegKeyMultiString("RecentFiles");
    auto it_rf = recent_files_from_registry.begin();

    for (auto it_rf = recent_files_from_registry.begin(); it_rf != recent_files_from_registry.end();)
    {
        if (!std::filesystem::exists(*it_rf) || it_rf->empty())
            it_rf = recent_files_from_registry.erase(it_rf);
        else
            ++it_rf;
    }
    //removeDuplicateStringsFromVector(recent_files_from_registry); TODO: causes reordering, remember correct order.
    SetGeoDmsRegKeyMultiString("RecentFiles", recent_files_from_registry);
}

void MainWindow::setRecentFiles()
{
    std::vector<std::string> recent_files_as_std_strings;
    for (auto* recent_file_action : m_recent_files_actions)
    {
        auto dos_string = recent_file_action->m_cfg_file_path;
        std::replace(dos_string.begin(), dos_string.end(), '/', '\\');
        recent_files_as_std_strings.push_back(dos_string); // TODO: string juggling
    }
    SetGeoDmsRegKeyMultiString("RecentFiles", recent_files_as_std_strings);
}

void MainWindow::insertCurrentConfigInRecentFiles(std::string_view cfg)
{
    auto cfg_index_in_recent_files = configIsInRecentFiles(cfg, GetGeoDmsRegKeyMultiString("RecentFiles"));
    if (cfg_index_in_recent_files == -1)
    {
        auto new_recent_file_action = new DmsRecentFileButtonAction(m_recent_files_actions.size() + 1, cfg, this);
        connect(new_recent_file_action, &DmsRecentFileButtonAction::triggered, new_recent_file_action, &DmsRecentFileButtonAction::onToolbuttonPressed);
        m_file_menu->addAction(new_recent_file_action);
        m_recent_files_actions.prepend(new_recent_file_action);

    }
    else
        m_recent_files_actions.move(cfg_index_in_recent_files, 0);

    setRecentFiles();
    updateFileMenu();
}

bool MainWindow::LoadConfig(CharPtr configFilePath)
{
    try
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
        m_root = newRoot;
        if (m_root)
        {
            SharedStr configFilePathStr = DelimitedConcat(ConvertDosFileName(GetCurrentDir()), ConvertDosFileName(SharedStr(configFilePath)));

//            std::string last_config_file_dos = configFilePathStr.c_str();
//            std::replace(last_config_file_dos.begin(), last_config_file_dos.end(), '/', '\\');
            insertCurrentConfigInRecentFiles(configFilePathStr.c_str());
            SetGeoDmsRegKeyString("LastConfigFile", configFilePathStr.c_str());

            m_treeview->setItemDelegate(new TreeItemDelegate());

            m_treeview->setModel(m_dms_model.get());
            m_treeview->setRootIndex(m_treeview->rootIndex().parent());// m_treeview->model()->index(0, 0));

            connect(m_treeview, &DmsTreeView::customContextMenuRequested, m_treeview, &DmsTreeView::showTreeviewContextMenu);
            m_treeview->scrollTo({}); // :/res/images/TV_branch_closed_selected.png

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
    }
    catch (...)
    {
        m_dms_model->setRoot(nullptr);
        setCurrentTreeItem(nullptr);
        error(catchException(false));
        return false;
    }
    m_dms_model->setRoot(m_root);
    setCurrentTreeItem(m_root); // as an example set current item to root, which emits signal currentItemChanged
    m_current_item_bar->setDmsCompleter();
    updateCaption();
    m_dms_model->reset();
    m_eventlog->scrollToBottomThrottled();
    return true;
}

void MainWindow::OnViewAction(const TreeItem* tiContext, CharPtr sAction, Int32 nCode, Int32 x, Int32 y, bool doAddHistory, bool isUrl, bool mustOpenDetailsPage)
{
    MainWindow::TheOne()->m_detail_pages->DoViewAction(const_cast<TreeItem*>(tiContext), sAction);
}

void MainWindow::ShowStatistics(const TreeItem* tiContext)
{
    auto* mdiSubWindow = new QMdiSubWindow(m_mdi_area.get()); // not a DmsViewArea
    auto* textWidget = new QTextBrowser(mdiSubWindow);
    mdiSubWindow->setWidget(textWidget);
    SharedStr title = "Statistics of " + tiContext->GetFullName();
    mdiSubWindow->setWindowTitle(title.c_str());
    m_mdi_area->addSubWindow(mdiSubWindow);
    mdiSubWindow->setAttribute(Qt::WA_DeleteOnClose);
    mdiSubWindow->show();

    InterestPtr<SharedPtr<const TreeItem>> tiHolder = tiContext;
    tiHolder->PrepareData();

    vos_buffer_type textBuffer;
    while (true)
    {
        bool done = NumericDataItem_GetStatistics(tiContext, textBuffer);
        textWidget->setText(begin_ptr(textBuffer));
        if (done)
            return;
        auto result = MessageBoxA((HWND)winId()
            , "Processing didn't complete yet; retry request?"
            , "Statistics"
            , MB_YESNO
        );
        if (result != IDYES)
            return;
    }
}

void AnyTreeItemStateHasChanged(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode)
{
    auto mainWindow = reinterpret_cast<MainWindow*>(clientHandle);
    switch (notificationCode) {
    case NC_Deleting:
        // TODO: remove self from any representation to avoid accessing it's dangling pointer
        break;
    case CC_CreateMdiChild:
    {
        auto* createStruct = const_cast<MdiCreateStruct*>(reinterpret_cast<const MdiCreateStruct*>(self));
        assert(createStruct);
        auto va = new QDmsViewArea(mainWindow->m_mdi_area.get(), createStruct);
        return;
    }
    case CC_Activate:
        mainWindow->setCurrentTreeItem(const_cast<TreeItem*>(self));
        return;

    case CC_ShowStatistics:
        mainWindow->ShowStatistics(self);
    }
    // MainWindow could have been destroyed
    if (s_CurrMainWindow)
    {
        assert(s_CurrMainWindow == mainWindow);
        mainWindow->m_treeview->update(); // this actually only invalidates any drawn area and causes repaint later
    }
}

void MainWindow::setupDmsCallbacks()
{
    DMS_RegisterMsgCallback(&geoDMSMessage, this);
    DMS_SetContextNotification(&geoDMSContextMessage, this);
    DMS_RegisterStateChangeNotification(AnyTreeItemStateHasChanged, this);
    SHV_SetCreateViewActionFunc(&OnViewAction);
}

void MainWindow::createActions()
{
    m_file_menu = std::make_unique<QMenu>(tr("&File"));
    menuBar()->addMenu(m_file_menu.get());
    m_current_item_bar_container = addToolBar(tr("Current item bar"));
    m_current_item_bar = std::make_unique<DmsCurrentItemBar>(this);
    
    m_current_item_bar_container->addWidget(m_current_item_bar.get());

    connect(m_current_item_bar.get(), &DmsCurrentItemBar::editingFinished, m_current_item_bar.get(), &DmsCurrentItemBar::onEditingFinished);

    addToolBarBreak();

    connect(m_mdi_area.get(), &QDmsMdiArea::subWindowActivated, this, &MainWindow::updateToolbar);
    //auto openIcon = QIcon::fromTheme("document-open", QIcon(":res/images/open.png"));
    auto fileOpenAct = new QAction(tr("&Open Configuration File"), this);
    fileOpenAct->setShortcuts(QKeySequence::Open);
    fileOpenAct->setStatusTip(tr("Open an existing configuration file"));
    connect(fileOpenAct, &QAction::triggered, this, &MainWindow::fileOpen);
    m_file_menu->addAction(fileOpenAct);

    auto reOpenAct = new QAction(tr("&Reopen current Configuration"), this);
    reOpenAct->setShortcuts(QKeySequence::Refresh);
    reOpenAct->setStatusTip(tr("Reopen the current configuration and reactivate the current active item"));
    connect(reOpenAct, &QAction::triggered, this, &MainWindow::reOpen);
    m_file_menu->addAction(reOpenAct);
    //fileToolBar->addAction(reOpenAct);

    m_file_menu->addSeparator();
    auto epdm = m_file_menu->addMenu(tr("Export Primary Data"));
    auto epdmBmp = new QAction(tr("Bitmap (*.tif or *.bmp)")); // TODO: memory leak, QAction will not transfer ownership from addAction
    auto epdmDbf = new QAction(tr("Table (*.dbf with a *.shp and *.shx if Feature data can be related)"));
    auto epdmCsv = new QAction(tr("Text Table (*.csv with semiColon Separated Values"));
    epdm->addAction(epdmBmp);
    epdm->addAction(epdmDbf);
    epdm->addAction(epdmCsv);

    m_file_menu->addSeparator();
    m_quit_action = std::make_unique<QAction>(tr("&Quit"));
    connect(m_quit_action.get(), &QAction::triggered, qApp, &QCoreApplication::quit);
    m_file_menu->addAction(m_quit_action.get());
    m_quit_action->setShortcuts(QKeySequence::Quit);
    m_quit_action->setStatusTip(tr("Quit the application"));
    connect(m_file_menu.get(), &QMenu::aboutToShow, this, &MainWindow::updateFileMenu);
    updateFileMenu();

    m_edit_menu = std::make_unique<QMenu>(tr("&Edit"));
    menuBar()->addMenu(m_edit_menu.get());

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
    m_edit_menu->addAction(m_edit_config_source_action.get());

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

    m_view_menu = std::make_unique<QMenu>(tr("&View"));
    menuBar()->addMenu(m_view_menu.get());

    m_defaultview_action = std::make_unique<QAction>(tr("Default View"));
    m_defaultview_action->setShortcut(QKeySequence(tr("Ctrl+Alt+D")));
    m_defaultview_action->setStatusTip(tr("Open current selected TreeItem's default view."));
    connect(m_defaultview_action.get(), &QAction::triggered, this, &MainWindow::defaultView);
    m_view_menu->addAction(m_defaultview_action.get());

    // table view
    m_tableview_action = std::make_unique<QAction>(tr("&Table View"));
    m_tableview_action->setShortcut(QKeySequence(tr("Ctrl+D")));
    m_tableview_action->setStatusTip(tr("Open current selected TreeItem's in a table view."));
    connect(m_tableview_action.get(), &QAction::triggered, this, &MainWindow::tableView);
    m_view_menu->addAction(m_tableview_action.get());

    // map view
    m_mapview_action = std::make_unique<QAction>(tr("&Map View"));
    m_mapview_action->setShortcut(QKeySequence(tr("Ctrl+M")));
    m_mapview_action->setStatusTip(tr("Open current selected TreeItem's in a map view."));
    connect(m_mapview_action.get(), &QAction::triggered, this, &MainWindow::mapView);
    m_view_menu->addAction(m_mapview_action.get());

    // statistics view
    m_statistics_action = std::make_unique<QAction>(tr("&Statistics View"));
//    m_statistics_action->setShortcut(QKeySequence(tr("Ctrl+H")));
    connect(m_statistics_action.get(), &QAction::triggered, this, &MainWindow::showStatistics);
    m_view_menu->addAction(m_statistics_action.get());

    // histogram view
    m_histogramview_action = std::make_unique<QAction>(tr("&Histogram View"));
    m_histogramview_action->setShortcut(QKeySequence(tr("Ctrl+H")));
    //connect(m_histogramview_action.get(), &QAction::triggered, this, & #TODO);
    
    // process schemes
    m_process_schemes_action = std::make_unique<QAction>(tr("&Process Schemes"));
    //connect(m_process_schemes_action.get(), &QAction::triggered, this, & #TODO);
    m_view_menu->addSeparator();
    m_toggle_treeview_action       = std::make_unique<QAction>(tr("Toggle TreeView"));
    m_toggle_detailpage_action     = std::make_unique<QAction>(tr("Toggle DetailPage"));
    m_toggle_eventlog_action       = std::make_unique<QAction>(tr("Toggle EventLog"));
    m_toggle_toolbar_action        = std::make_unique<QAction>(tr("Toggle Toolbar"));
    m_toggle_currentitembar_action = std::make_unique<QAction>(tr("Toggle CurrentItemBar"));

    m_toggle_treeview_action->setCheckable(true);
    m_toggle_detailpage_action->setCheckable(true);
    m_toggle_eventlog_action->setCheckable(true);
    m_toggle_toolbar_action->setCheckable(true);
    m_toggle_currentitembar_action->setCheckable(true);

    connect(m_toggle_treeview_action.get(), &QAction::triggered, this, &MainWindow::toggle_treeview);
    connect(m_toggle_detailpage_action.get(), &QAction::triggered, this, &MainWindow::toggle_detailpages);
    connect(m_toggle_eventlog_action.get(), &QAction::triggered, this, &MainWindow::toggle_eventlog);
    connect(m_toggle_toolbar_action.get(), &QAction::triggered, this, &MainWindow::toggle_toolbar);
    connect(m_toggle_currentitembar_action.get(), &QAction::triggered, this, &MainWindow::toggle_currentitembar);
    m_toggle_treeview_action->setShortcut(QKeySequence(tr("Alt+0")));
    m_toggle_detailpage_action->setShortcut(QKeySequence(tr("Alt+1")));
    m_toggle_eventlog_action->setShortcut(QKeySequence(tr("Alt+2")));
    m_toggle_toolbar_action->setShortcut(QKeySequence(tr("Alt+3")));
    m_toggle_currentitembar_action->setShortcut(QKeySequence(tr("Alt+4")));
    m_view_menu->addAction(m_toggle_treeview_action.get());
    m_view_menu->addAction(m_toggle_detailpage_action.get());
    m_view_menu->addAction(m_toggle_eventlog_action.get());
    m_view_menu->addAction(m_toggle_toolbar_action.get());
    m_view_menu->addAction(m_toggle_currentitembar_action.get());
    connect(m_view_menu.get(), &QMenu::aboutToShow, this, &MainWindow::updateViewMenu);


    // tools menu
    m_tools_menu = std::make_unique<QMenu>("&Tools");
    menuBar()->addMenu(m_tools_menu.get());
    m_gui_options_action = std::make_unique<QAction>(tr("&Gui Options"));
    connect(m_gui_options_action.get(), &QAction::triggered, this, &MainWindow::gui_options);
    m_tools_menu->addAction(m_gui_options_action.get());

    m_advanced_options_action = std::make_unique<QAction>(tr("&Advanced Options"));
    connect(m_advanced_options_action.get(), &QAction::triggered, this, &MainWindow::advanced_options);
    m_tools_menu->addAction(m_advanced_options_action.get());

    m_config_options_action = std::make_unique<QAction>(tr("&Config Options"));
    connect(m_config_options_action.get(), &QAction::triggered, this, &MainWindow::config_options);
    m_tools_menu->addAction(m_config_options_action.get());

    m_code_analysis_submenu = std::make_unique<QMenu>("&Code analysis ...");
    m_tools_menu->addMenu(m_code_analysis_submenu.get());

    m_code_analysis_set_source_action = std::make_unique<QAction>(tr("set source"));
    connect(m_code_analysis_set_source_action.get(), &QAction::triggered, this, &MainWindow::code_analysis_set_source);
    m_code_analysis_submenu->addAction(m_code_analysis_set_source_action.get());

    m_code_analysis_set_target_action = std::make_unique<QAction>(tr("set target"));
    connect(m_code_analysis_set_target_action.get(), &QAction::triggered, this, &MainWindow::code_analysis_set_target);
    m_code_analysis_submenu->addAction(m_code_analysis_set_target_action.get());

    m_code_analysis_add_target_action = std::make_unique<QAction>(tr("add target"));
    connect(m_code_analysis_add_target_action.get(), &QAction::triggered, this, &MainWindow::code_analysis_add_target);
    m_code_analysis_submenu->addAction(m_code_analysis_add_target_action.get());

    m_code_analysis_clr_targets_action = std::make_unique<QAction>(tr("clear target"));
    connect(m_code_analysis_clr_targets_action.get(), &QAction::triggered, this, &MainWindow::code_analysis_clr_targets);
    m_code_analysis_submenu->addAction(m_code_analysis_clr_targets_action.get());

    // window menu
    m_window_menu = std::make_unique<QMenu>(tr("&Window"));
    menuBar()->addMenu(m_window_menu.get());
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
    connect(win5_action, &QAction::triggered, m_mdi_area.get(), &QMdiArea::closeAllSubWindows);

    /*vauto win6_action = new QAction(tr("&Close All But This"), this);
    win6_action->setShortcut(QKeySequence(tr("Ctrl+B")));
    connect(win6_action, &QAction::triggered, m_mdi_area.get(), &QMdiArea::closeActiveSubWindow);*/
    m_window_menu->addActions({win1_action, win3_action, win4_action, win5_action});
    m_window_menu->addSeparator();
    connect(m_window_menu.get(), &QMenu::aboutToShow, this, &MainWindow::updateWindowMenu);

    // help menu
    m_help_menu = std::make_unique<QMenu>(tr("&Help"));
    menuBar()->addMenu(m_help_menu.get());
    QAction *aboutAct = m_help_menu->addAction(tr("&About GeoDms"), this, &MainWindow::aboutGeoDms);
    aboutAct->setStatusTip(tr("Show the application's About box"));
    QAction *aboutQtAct = m_help_menu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
}

void MainWindow::updateFileMenu()
{
    for (auto* recent_file_action : m_recent_files_actions)
    {
        m_window_menu->removeAction(recent_file_action);
        delete recent_file_action;
    }
    m_recent_files_actions.clear(); // delete old actions;

    // temporarily remove quit action
    removeAction(m_quit_action.get());

    // rebuild latest recent files from registry
    cleanRecentFilesThatDoNotExist();
    auto recent_files_from_registry = GetGeoDmsRegKeyMultiString("RecentFiles");
    size_t recent_file_index = 1;
    for (std::string_view recent_file : recent_files_from_registry)
    {
        auto qa = new DmsRecentFileButtonAction(recent_file_index, recent_file, this);
        connect(qa, &DmsRecentFileButtonAction::triggered, qa, &DmsRecentFileButtonAction::onToolbuttonPressed);
        m_file_menu->addAction(qa);
        m_recent_files_actions.append(qa);
        recent_file_index++;
    }

    // reinsert quit action
    m_file_menu->addAction(m_quit_action.get());
}

void MainWindow::updateViewMenu()
{
    m_toggle_treeview_action->setChecked(m_treeview->isVisible());
    m_toggle_detailpage_action->setChecked(m_right_side_toolbar->isVisible()); // 
    m_toggle_eventlog_action->setChecked(m_eventlog->isVisible());
    bool hasToolbar = !m_toolbar.isNull();
    m_toggle_toolbar_action->setEnabled(hasToolbar);
    if (hasToolbar)
        m_toggle_toolbar_action->setChecked(m_toolbar->isVisible());
    m_toggle_currentitembar_action->setChecked(m_current_item_bar_container->isVisible());
}

void MainWindow::updateWindowMenu() 
{
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
    {
        out << m_currConfigFileName;
        if (m_root)
        {
            auto name = m_root->GetName();
            CharPtr nameCStr = name.c_str();
            if (*nameCStr)
            {
                auto configNameLen = m_currConfigFileName.ssize(); // assume ending on ".dms"
                if ((configNameLen <= 4) || (StrLen(nameCStr) > configNameLen  - 4) || (strnicmp(nameCStr, m_currConfigFileName.c_str(), configNameLen - 4)))
                    out << "(" << nameCStr << ")";
            }
        }
        out << " in ";
    }
    out << GetCurrentDir();

    if (!(GetRegStatusFlags() & RSF_AdminMode)) out << "[Hiding]";
    if (!(GetRegStatusFlags() & RSF_ShowStateColors)) out << "[HSC]";
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

void MainWindow::createRightSideToolbar()
{
    m_right_side_toolbar = new QToolBar(tr("DetailPagesActions"), this);
    m_right_side_toolbar->setMovable(false);
    addToolBar(Qt::ToolBarArea::RightToolBarArea, m_right_side_toolbar);

    const QIcon general_icon = QIcon::fromTheme("detailpages-general", QIcon(":res/images/DP_properties.bmp"));
    m_general_page_action = std::make_unique<QAction>(general_icon, tr("&General"));
    m_general_page_action->setCheckable(true);
    m_general_page_action->setChecked(true);
    m_general_page_action->setStatusTip("Show property overview of the active item, including domain and value characteristics of attributes");
    m_right_side_toolbar->addAction(m_general_page_action.get());
    connect(m_general_page_action.get(), &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleGeneral);

    const QIcon explore_icon = QIcon::fromTheme("detailpages-explore", QIcon(":res/images/DP_explore.bmp"));
    m_explore_page_action = std::make_unique<QAction>(explore_icon, tr("&Explore"));
    m_explore_page_action->setCheckable(true);
    m_explore_page_action->setStatusTip("Show all item-names that can be found from the context of the active item in namespace search order, which are: 1. referred contexts, 2. template definition context and/or using contexts, and 3. parent context.");
    m_right_side_toolbar->addAction(m_explore_page_action.get());
    connect(m_explore_page_action.get(), &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleExplorer);

    const QIcon properties_icon = QIcon::fromTheme("detailpages-properties", QIcon(":res/images/DP_properties.bmp"));
    m_properties_page_action = std::make_unique<QAction>(properties_icon, tr("&Properties"));
    m_properties_page_action->setCheckable(true);
    m_properties_page_action->setStatusTip("Show all properties of the active item; some properties are itemtype-specific and the most specific properties are reported first");
    m_right_side_toolbar->addAction(m_properties_page_action.get());
    connect(m_properties_page_action.get(), &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleProperties);

    const QIcon configuration_icon = QIcon::fromTheme("detailpages-configuration", QIcon(":res/images/DP_configuration.bmp"));
    m_configuration_page_action = std::make_unique<QAction>(configuration_icon, tr("&Configuration"));
    m_configuration_page_action->setCheckable(true);
    m_configuration_page_action->setStatusTip("Show item configuration script of the active item in the detail-page; the script is generated from the internal representation of the item in the syntax of the read .dms file and is therefore similar to how it was defined there.");
    m_right_side_toolbar->addAction(m_configuration_page_action.get());
    connect(m_configuration_page_action.get(), &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleConfiguration);

    const QIcon sourcedescr_icon = QIcon::fromTheme("detailpages-sourcedescr", QIcon(":res/images/DP_SourceData.png"));
    m_sourcedescr_page_action = std::make_unique<QAction>(sourcedescr_icon, tr("&Source description"));
    m_sourcedescr_page_action->setCheckable(true);
    m_sourcedescr_page_action->setStatusTip("Show the first or next  of the 4 source descriptions of the items used to calculate the active item: 1. configured source descriptions; 2. read data specs; 3. to be written data specs; 4. both");
    m_right_side_toolbar->addAction(m_sourcedescr_page_action.get());
    connect(m_sourcedescr_page_action.get(), &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleSourceDescr);

    const QIcon metainfo_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/DP_MetaData.bmp"));
    m_metainfo_page_action = std::make_unique<QAction>(metainfo_icon, tr("&Meta information"));
    m_metainfo_page_action->setCheckable(true);
    m_metainfo_page_action->setStatusTip("Show the availability of meta-information, based on the URL property of the active item or it's anchestor");
    m_right_side_toolbar->addAction(m_metainfo_page_action.get());
    connect(m_metainfo_page_action.get(), &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleMetaInfo);

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_right_side_toolbar->addWidget(spacer);

    const QIcon event_text_filter_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/DP_properties.bmp"));
    m_eventlog_event_text_filter_toggle = std::make_unique<QAction>(event_text_filter_icon, tr("&Eventlog: text filter"));
    m_eventlog_event_text_filter_toggle->setCheckable(true);
    m_right_side_toolbar->addAction(m_eventlog_event_text_filter_toggle.get());
    connect(m_eventlog_event_text_filter_toggle.get(), &QAction::toggled, m_eventlog.get(), &DmsEventLog::toggleTextFilter);

    const QIcon eventlog_type_filter_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/TB_select_object.bmp"));
    m_eventlog_event_type_filter_toggle = std::make_unique<QAction>(eventlog_type_filter_icon, tr("&Eventlog: type filter"));
    m_eventlog_event_type_filter_toggle->setCheckable(true);
    m_right_side_toolbar->addAction(m_eventlog_event_type_filter_toggle.get());
    connect(m_eventlog_event_type_filter_toggle.get(), &QAction::toggled, m_eventlog.get(), &DmsEventLog::toggleTypeFilter);

    const QIcon eventlog_scroll_to_bottom_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/undo.png"));
    m_eventlog_scroll_to_bottom_toggle = std::make_unique<QAction>(eventlog_scroll_to_bottom_icon, tr("&Eventlog: scroll to bottom"));
    m_eventlog_scroll_to_bottom_toggle->setDisabled(true);
    m_right_side_toolbar->addAction(m_eventlog_scroll_to_bottom_toggle.get());
    connect(m_eventlog_scroll_to_bottom_toggle.get(), &QAction::triggered, m_eventlog.get(), &DmsEventLog::toggleScrollToBottomDirectly);

// value info should be dealt with differently, more similar to DataViews and statistics, but with forward/backward and clone functions
//    const QIcon value_info_icon = QIcon::fromTheme("detailpages-valueinfo", QIcon(":res/images/DP_ValueInfo.bmp"));
//    QAction* value_info_page_act = new QAction(value_info_icon, tr("&Value info"), this);
//    detail_pages_toolBar->addAction(value_info_page_act);
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
    connect(m_eventlog->m_text_filter.get(), &QLineEdit::returnPressed, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilter);
    connect(m_eventlog->m_minor_trace_filter.get(), &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
    connect(m_eventlog->m_major_trace_filter.get(), &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
    connect(m_eventlog->m_warning_filter.get(), &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
    connect(m_eventlog->m_error_filter.get(), &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
}