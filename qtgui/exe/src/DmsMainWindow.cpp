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
#include <QPixmap>


#include "DmsMainWindow.h"
#include "TestScript.h"

#include "DmsEventLog.h"
#include "DmsViewArea.h"
#include "DmsTreeView.h"
#include "DmsDetailPages.h"
#include "DmsOptions.h"
#include "DmsExport.h"
#include "DataView.h"
#include "StateChangeNotification.h"
#include "stg/AbstrStorageManager.h"
#include <regex>

#include "dataview.h"



static MainWindow* s_CurrMainWindow = nullptr;

void DmsFileChangedWindow::ignore()
{
    done(QDialog::Rejected);
}

void DmsFileChangedWindow::reopen()
{
    done(QDialog::Accepted);
    MainWindow::TheOne()->reopen();
}

void DmsFileChangedWindow::onAnchorClicked(const QUrl& link)
{
    auto clicked_file_link = link.toString().toStdString();
    MainWindow::TheOne()->openConfigSourceDirectly(clicked_file_link, "0");
}

DmsFileChangedWindow::DmsFileChangedWindow(QWidget* parent)
    : QDialog(parent)
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
    m_ignore = new QPushButton(tr("&Ignore"), this);
    m_ignore->setMaximumSize(75, 30);

    m_reopen = new QPushButton(tr("&Reopen"), this);
    connect(m_ignore, &QPushButton::released, this, &DmsFileChangedWindow::ignore);
    connect(m_reopen, &QPushButton::released, this, &DmsFileChangedWindow::reopen);
    m_reopen->setAutoDefault(true);
    m_reopen->setDefault(true);
    m_reopen->setMaximumSize(75, 30);
    box_layout->addWidget(m_reopen);
    box_layout->addWidget(m_ignore);
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
        file_changed_message_markdown += "[" + link + "](" + link + ")\n\n";
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
    done(QDialog::Rejected);
    std::terminate();
}

void DmsErrorWindow::reopen()
{
    done(QDialog::Accepted);
    // don't call now: MainWindow::TheOne()->reOpen();
    // but let the execution caller call it, after modal message pumping ended
}

void DmsErrorWindow::onAnchorClicked(const QUrl& link)
{
    MainWindow::TheOne()->m_detail_pages->onAnchorClicked(link);
}

DmsErrorWindow::DmsErrorWindow(QWidget* parent)
    : QDialog(parent)
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
    m_ignore = new QPushButton("Ignore", this);
    m_ignore->setMaximumSize(75, 30);
    m_terminate = new QPushButton("Terminate", this);
    m_terminate->setMaximumSize(75, 30);

    m_reopen = new QPushButton("Reopen", this);
    m_reopen->setAutoDefault(true);
    m_reopen->setDefault(true);

    connect(m_ignore, &QPushButton::released, this, &DmsErrorWindow::ignore);
    connect(m_terminate, &QPushButton::released, this, &DmsErrorWindow::terminate);
    connect(m_reopen, &QPushButton::released, this, &DmsErrorWindow::reopen);
    m_reopen->setMaximumSize(75, 30);
    box_layout->addWidget(m_reopen);
    box_layout->addWidget(m_terminate);
    box_layout->addWidget(m_ignore);
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

    m_mdi_area = new QDmsMdiArea(this);

    QFont dms_text_font(":/res/fonts/dmstext.ttf", 10);
    QApplication::setFont(dms_text_font);

    setCentralWidget(m_mdi_area.get());

    m_mdi_area->show();

    createStatusBar();
    createDmsHelperWindowDocks();
    createDetailPagesActions();

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
            QTimer::singleShot(0, this, [=]() { LoadConfig(cmdLineSettings.m_ConfigFileName.c_str()); });
    }

    updateCaption();
    setUnifiedTitleAndToolBarOnMac(true);
    if (!cmdLineSettings.m_CurrItemFullNames.empty())
        m_current_item_bar->setPath(cmdLineSettings.m_CurrItemFullNames.back().c_str());

    scheduleUpdateToolbar();

    LoadColors();
}

MainWindow::~MainWindow()
{
    m_UpdateToolbarRequestPending = true; // avoid going into scheduleUpdateToolbar while destructing subWindows
    CloseConfig();

    assert(s_CurrMainWindow == this);
    s_CurrMainWindow = nullptr;

    cleanupDmsCallbacks();
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

void DmsCurrentItemBar::setPathDirectly(QString path)
{
    setText(path);
    auto root = MainWindow::TheOne()->getRootTreeItem();
    if (!root)
        return;

    auto best_item_ref = TreeItem_GetBestItemAndUnfoundPart(root, path.toUtf8());
    auto found_treeitem = best_item_ref.first;
    if (!found_treeitem)
        return;
    MainWindow::TheOne()->setCurrentTreeItem(const_cast<TreeItem*>(found_treeitem), false);
}

void DmsCurrentItemBar::onEditingFinished()
{
    auto root = MainWindow::TheOne()->getRootTreeItem();
    if (!root)
        return;

    auto best_item_ref = TreeItem_GetBestItemAndUnfoundPart(root, text().toUtf8());
    auto found_treeitem = best_item_ref.first;
    if (!found_treeitem)
        return;
    MainWindow::TheOne()->setCurrentTreeItem(const_cast<TreeItem*>(found_treeitem));
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

bool MainWindow::openErrorOnFailedCurrentItem()
{
    auto currItem = getCurrentTreeItem();
    if (currItem->WasFailed() || currItem->IsFailed())
    {
        auto fail_reason = currItem->GetFailReason();
        reportErrorAndTryReload(fail_reason);
        return true;
    }
    return false;
}

void MainWindow::clearActionsForEmptyCurrentItem()
{
    m_defaultview_action->setDisabled(true);
    m_tableview_action->setDisabled(true);
    m_mapview_action->setDisabled(true);
    m_statistics_action->setDisabled(true);
    m_process_schemes_action->setDisabled(true);
    m_update_treeitem_action->setDisabled(true);
    m_invalidate_action->setDisabled(true);
    m_update_subtree_action->setDisabled(true);
    m_edit_config_source_action->setDisabled(true);
    m_metainfo_page_action->setDisabled(true);
}

void MainWindow::updateActionsForNewCurrentItem()
{
    auto viewstyle_flags = SHV_GetViewStyleFlags(m_current_item.get());
    m_defaultview_action->setEnabled(viewstyle_flags & (ViewStyleFlags::vsfDefault | ViewStyleFlags::vsfTableView | ViewStyleFlags::vsfTableContainer | ViewStyleFlags::vsfMapView)); // TODO: vsfDefault appears to never be set
    m_tableview_action->setEnabled(viewstyle_flags & (ViewStyleFlags::vsfTableView | ViewStyleFlags::vsfTableContainer));
    m_mapview_action->setEnabled(viewstyle_flags & ViewStyleFlags::vsfMapView);
    m_statistics_action->setEnabled(IsDataItem(m_current_item.get()));
    m_process_schemes_action->setEnabled(true);
    m_update_treeitem_action->setEnabled(true);
    m_invalidate_action->setEnabled(true);
    m_update_subtree_action->setEnabled(true);
    m_edit_config_source_action->setEnabled(true);
    
    try 
    {
        if (!FindURL(m_current_item.get()).empty())
            m_metainfo_page_action->setEnabled(true);
        else
            m_metainfo_page_action->setDisabled(true);
    }
    catch (...)
    {
        m_metainfo_page_action->setEnabled(true); 
    }
}

void MainWindow::updateTreeItemVisitHistory()
{
    auto current_index = m_treeitem_visit_history->currentIndex();
    if (current_index < m_treeitem_visit_history->count()-1) // current index is not at the end, remove all forward items
    {
        for (int i=m_treeitem_visit_history->count() - 1; i > current_index; i--)
            m_treeitem_visit_history->removeItem(i);
    }

    m_treeitem_visit_history->addItem(m_current_item->GetFullName().c_str());
    m_treeitem_visit_history->setCurrentIndex(m_treeitem_visit_history->count()-1);
}

void MainWindow::setCurrentTreeItem(TreeItem* new_current_item, bool update_history)
{
    if (m_current_item == new_current_item)
        return;

    m_current_item = new_current_item;

    // update actions based on new current item
    updateActionsForNewCurrentItem();

    if (m_current_item_bar)
    {
        if (m_current_item)
            m_current_item_bar->setText(m_current_item->GetFullName().c_str());
        else
            m_current_item_bar->setText("");
    }

    if (update_history)
        updateTreeItemVisitHistory();

    m_treeview->setNewCurrentItem(new_current_item);
    m_treeview->scrollTo(m_treeview->currentIndex(), QAbstractItemView::ScrollHint::EnsureVisible);
    emit currentItemChanged();
    reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "Selected new current item %s", m_current_item->GetFullName());
}

#include <QFileDialog>

void MainWindow::fileOpen() 
{
    //m_recent_files_actions
    auto proj_dir = SharedStr("");
    if (m_root)
        proj_dir = AbstrStorageManager::Expand(m_root.get(), SharedStr("%projDir%"));
    else
    {
        if (!m_recent_files_actions.empty())
            proj_dir = m_recent_files_actions.at(0)->m_cfg_file_path.c_str();
    }

    auto configFileName = QFileDialog::getOpenFileName(this, "Open configuration", proj_dir.c_str(), "*.dms").toLower();

    if (configFileName.isEmpty())
        return;

    if (GetRegStatusFlags() & RSF_EventLog_ClearOnLoad)
        m_eventlog_model->clear();

    bool result = LoadConfig(configFileName.toUtf8().data());
}

bool MainWindow::reopen()
{
    auto cip = m_current_item_bar->text();

    if (GetRegStatusFlags() & RSF_EventLog_ClearOnReLoad)
        m_eventlog_model->clear();

    bool result = LoadConfig(m_currConfigFileName.c_str());
    if (!result)
        return false;

    if (cip.isEmpty())
        return true;

    m_current_item_bar->setPath(cip.toUtf8());
    return true;
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
    dms_about_text += "- GeoDms icon obtained from: World icons created by turkkub-Flaticon https://www.flaticon.com/free-icons/world";
    QMessageBox::about(this, tr("About GeoDms"),
            tr(dms_about_text.c_str()));
}

void MainWindow::wiki()
{
    QDesktopServices::openUrl(QUrl("https://github.com/ObjectVision/GeoDMS/wiki", QUrl::TolerantMode));
}

DmsRecentFileButtonAction::DmsRecentFileButtonAction(size_t index, std::string_view dms_file_full_path, QObject* parent)
    :QAction(QString(index < 10 ? QString("&") : "") + QString::number(index) + ". " + ConvertDosFileName(SharedStr(dms_file_full_path.data())).c_str(), parent)
{
    m_cfg_file_path = dms_file_full_path;
}

void DmsRecentFileButtonAction::onToolbuttonPressed()
{
    auto main_window = MainWindow::TheOne();

    if (GetRegStatusFlags() & RSF_EventLog_ClearOnLoad)
        main_window->m_eventlog_model->clear();

    main_window->LoadConfig(m_cfg_file_path.c_str());
}

DmsToolbuttonAction::DmsToolbuttonAction(const QIcon& icon, const QString& text, QObject* parent, ToolbarButtonData button_data, const ViewStyle vs)
    : QAction(icon, text, parent)
{
    assert(button_data.text.size()==2);
    assert(parent);

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
    try
    {
        SuspendTrigger::Resume();
        dms_view_area->getDataView()->GetContents()->OnCommand(m_data.ids[m_state]);
        if (m_data.icons.size() - 1 == m_state) // icon roll over
            setIcon(QIcon(m_data.icons[m_state]));
    }
    catch (...)
    {
        auto errMsg = catchException(false);
        MainWindow::TheOne()->reportErrorAndTryReload(errMsg);
    }    
}

auto getToolbarButtonData(ToolButtonID button_id) -> ToolbarButtonData
{
    switch (button_id)
    {
    case TB_Export: return { {"Save to file as semicolon delimited text", "Export the viewport data to bitmaps file(s) using the export settings and the current ROI"}, {TB_Export}, {":/res/images/TB_save.bmp"}};
    case TB_TableCopy: return { {"Copy as semicolon delimited text to Clipboard",""}, {TB_TableCopy}, {":/res/images/TB_copy.bmp"}};
    case TB_Copy: return { {"Copy the visible contents as image to Clipboard","Copy the visible contents of the viewport to the Clipboard"}, {TB_Copy}, {":/res/images/TB_vcopy.bmp"}};
    case TB_CopyLC: return { {"","Copy the full contents of the LayerControlList to the Clipboard"}, {TB_CopyLC}, {":/res/images/TB_copy.bmp"}};
    case TB_ShowFirstSelectedRow: return { {"Show the first selected row","Make the extent of the selected elements fit in the ViewPort"}, {TB_ShowFirstSelectedRow}, {":/res/images/TB_table_show_first_selected.bmp"} };
    case TB_ZoomSelectedObj: return { {"Show the first selected row","Make the extent of the selected elements fit in the ViewPort"}, {TB_ZoomSelectedObj}, {":/res/images/TB_zoom_selected.bmp"}};
    case TB_SelectRows: return { {"Select row(s) by mouse-click (use Shift to add or Ctrl to deselect)",""}, {TB_SelectRows}, {":/res/images/TB_table_select_row.bmp"}};
    case TB_SelectAll: return { {"Select all rows","Select all elements in the active layer"}, {TB_SelectAll}, {":/res/images/TB_select_all.bmp"}};
    case TB_SelectNone: return { {"Deselect all rows","Deselect all elements in the active layer"}, {TB_SelectNone}, {":/res/images/TB_select_none.bmp"}};
    case TB_ShowSelOnlyOn: return { {"Show only selected rows","Show only selected elements"}, {TB_ShowSelOnlyOff, TB_ShowSelOnlyOn}, {":/res/images/TB_show_selected_features.bmp"}};
    case TB_TableGroupBy: return { {"Group by the highlighted columns",""}, {TB_Neutral, TB_TableGroupBy}, {":/res/images/TB_group_by.bmp"}};
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

void MainWindow::scheduleUpdateToolbar()
{
    //ViewStyle current_toolbar_style = ViewStyle::tvsUndefined, requested_toolbar_viewstyle = ViewStyle::tvsUndefined;
    if (m_UpdateToolbarRequestPending)
        return;

    // update requested toolbar style
    QMdiSubWindow* active_mdi_subwindow = m_mdi_area->activeSubWindow();
    QDmsViewArea* dms_active_mdi_subwindow = dynamic_cast<QDmsViewArea*>(active_mdi_subwindow);
    !dms_active_mdi_subwindow ? m_current_toolbar_style = ViewStyle::tvsUndefined : dms_active_mdi_subwindow->getDataView()->GetViewType();
    
    m_UpdateToolbarRequestPending = true;
    QTimer::singleShot(0, [this]()
        {
            m_UpdateToolbarRequestPending = false;
            this->updateToolbar();
        }
    );

}

void MainWindow::createDetailPagesActions()
{
    const QIcon backward_icon = QIcon::fromTheme("backward", QIcon(":/res/images/DP_back.bmp"));
    m_back_action = std::make_unique<QAction>(backward_icon, tr("&Back"));
    //m_back_action->setShortcut(QKeySequence(tr("Shift+Ctrl+W")));
    //m_back_action->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_back_action.get(), &QAction::triggered, this, &MainWindow::back);

    const QIcon forward_icon = QIcon::fromTheme("detailpages-general", QIcon(":/res/images/DP_forward.bmp"));
    m_forward_action = std::make_unique<QAction>(forward_icon, tr("&Forward"));
    //m_forward_action->setShortcut(QKeySequence(tr("Shift+Ctrl+W")));
    //m_forward_action->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_forward_action.get(), &QAction::triggered, this, &MainWindow::forward);

    const QIcon general_icon = QIcon::fromTheme("detailpages-general", QIcon(":/res/images/DP_properties_general.bmp"));
    m_general_page_action = std::make_unique<QAction>(general_icon, tr("&General"));
    m_general_page_action->setCheckable(true);
    m_general_page_action->setChecked(true);
    m_general_page_action->setStatusTip("Show property overview of the active item, including domain and value characteristics of attributes");
    connect(m_general_page_action.get(), &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleGeneral);

    const QIcon explore_icon = QIcon::fromTheme("detailpages-explore", QIcon(":res/images/DP_explore.bmp"));
    m_explore_page_action = std::make_unique<QAction>(explore_icon, tr("&Explore"));
    m_explore_page_action->setCheckable(true);
    m_explore_page_action->setStatusTip("Show all item-names that can be found from the context of the active item in namespace search order, which are: 1. referred contexts, 2. template definition context and/or using contexts, and 3. parent context.");
    connect(m_explore_page_action.get(), &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleExplorer);

    const QIcon properties_icon = QIcon::fromTheme("detailpages-properties", QIcon(":/res/images/DP_properties_properties.bmp"));
    m_properties_page_action = std::make_unique<QAction>(properties_icon, tr("&Properties"));
    m_properties_page_action->setCheckable(true);
    m_properties_page_action->setStatusTip("Show all properties of the active item; some properties are itemtype-specific and the most specific properties are reported first");
    connect(m_properties_page_action.get(), &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleProperties);

    const QIcon configuration_icon = QIcon::fromTheme("detailpages-configuration", QIcon(":res/images/DP_configuration.bmp"));
    m_configuration_page_action = std::make_unique<QAction>(configuration_icon, tr("&Configuration"));
    m_configuration_page_action->setCheckable(true);
    m_configuration_page_action->setStatusTip("Show item configuration script of the active item in the detail-page; the script is generated from the internal representation of the item in the syntax of the read .dms file and is therefore similar to how it was defined there.");
    connect(m_configuration_page_action.get(), &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleConfiguration);

    const QIcon sourcedescr_icon = QIcon::fromTheme("detailpages-sourcedescr", QIcon(":res/images/DP_SourceData.png"));
    m_sourcedescr_page_action = std::make_unique<QAction>(sourcedescr_icon, tr("&Source description"));
    m_sourcedescr_page_action->setCheckable(true);
    m_sourcedescr_page_action->setStatusTip("Show the first or next  of the 4 source descriptions of the items used to calculate the active item: 1. configured source descriptions; 2. read data specs; 3. to be written data specs; 4. both");
    connect(m_sourcedescr_page_action.get(), &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleSourceDescr);

    const QIcon metainfo_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/DP_MetaData.bmp"));
    m_metainfo_page_action = std::make_unique<QAction>(metainfo_icon, tr("&Meta information"));
    m_metainfo_page_action->setCheckable(true);
    m_metainfo_page_action->setStatusTip("Show the availability of meta-information, based on the URL property of the active item or it's anchestor");
    connect(m_metainfo_page_action.get(), &QAction::triggered, m_detail_pages, &DmsDetailPages::toggleMetaInfo);
}

auto getAvailableTableviewButtonIds() -> std::vector<ToolButtonID>
{
    return { TB_Export, TB_TableCopy, TB_Copy, TB_Undefined,
             TB_ShowFirstSelectedRow, TB_SelectRows, TB_SelectAll, TB_SelectNone, TB_ShowSelOnlyOn, TB_Undefined,
             TB_TableGroupBy };
}

auto getAvailableMapviewButtonIds() -> std::vector<ToolButtonID>
{
    return { TB_Export , TB_CopyLC, TB_Copy, TB_Undefined,
             TB_ZoomAllLayers, TB_ZoomActiveLayer, TB_ZoomIn2, TB_ZoomOut2, TB_Undefined,
             TB_ZoomSelectedObj,TB_SelectObject,TB_SelectRect,TB_SelectCircle,TB_SelectPolygon,TB_SelectDistrict,TB_SelectAll,TB_SelectNone,TB_ShowSelOnlyOn, TB_Undefined,
             TB_Show_VP,TB_SP_All,TB_NeedleOn,TB_ScaleBarOn };
}

void MainWindow::updateDetailPagesToolbar()
{
    if (m_detail_pages->isHidden())
        return;
    
    m_toolbar->addAction(m_general_page_action.get());
    m_toolbar->addAction(m_explore_page_action.get());
    m_toolbar->addAction(m_properties_page_action.get());
    m_toolbar->addAction(m_configuration_page_action.get());
    m_toolbar->addAction(m_sourcedescr_page_action.get());
    m_toolbar->addAction(m_metainfo_page_action.get());
    // detail pages buttons
    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    //m_toolbar->addWidget(spacer);
    m_dms_toolbar_spacer_action.reset(m_toolbar->insertWidget(m_general_page_action.get(), spacer));
}

void MainWindow::clearToolbarUpToDetailPagesTools()
{
    for (auto action : m_current_dms_view_actions)
        m_toolbar->removeAction(action);
    m_current_dms_view_actions.clear();

}

void MainWindow::updateToolbar()
{
    // TODO: #387 separate variable for wanted toolbar state: mapview, tableview
    if (!m_toolbar)
    {
        addToolBarBreak();
        m_toolbar = addToolBar(tr("dmstoolbar"));
        m_toolbar->setStyleSheet("QToolBar { background: rgb(117, 117, 138);\n padding : 0px; }\n"
                                 "QToolButton {padding: 0px;}\n"
                                 "QToolButton:checked {background-color: rgba(255, 255, 255, 150);}\n"
                                 "QToolButton:checked {selection-color: rgba(255, 255, 255, 150);}\n"
                                 "QToolButton:checked {selection-background-color: rgba(255, 255, 255, 150);}\n");
        
        m_toolbar->setIconSize(QSize(32, 32));
        m_toolbar->setMinimumSize(QSize(38, 38));

        updateDetailPagesToolbar(); // detail pages toolbar is created once
    }

    QMdiSubWindow* active_mdi_subwindow = m_mdi_area->activeSubWindow();
    if (!active_mdi_subwindow)
    {
        clearToolbarUpToDetailPagesTools();
        return;
    }

    if (m_tooled_mdi_subwindow == active_mdi_subwindow)
        return;

    m_tooled_mdi_subwindow = active_mdi_subwindow;
    auto active_dms_view_area = dynamic_cast<QDmsViewArea*>(active_mdi_subwindow);
    if (!active_dms_view_area)
    {
        clearToolbarUpToDetailPagesTools();
        return;
    }

    // create new actions
    auto* dv = active_dms_view_area->getDataView();
    auto view_style = dv->GetViewType();
    if (view_style==m_current_toolbar_style) // Do nothing
        return;

    // disable/enable coordinate tool
    auto is_mapview = view_style == ViewStyle::tvsMapView;
    m_statusbar_coordinate_label->setVisible(is_mapview);
    m_statusbar_coordinates->setVisible(is_mapview);

    clearToolbarUpToDetailPagesTools();
    static std::vector<ToolButtonID> available_table_buttons = getAvailableTableviewButtonIds();
    static std::vector<ToolButtonID> available_map_buttons = getAvailableMapviewButtonIds();
    auto& available_buttons = view_style == ViewStyle::tvsTableView ? available_table_buttons : available_map_buttons;
    
    auto first_toolbar_detail_pages_action = m_dms_toolbar_spacer_action.get();
    for (auto button_id : available_buttons)
    {
        if (button_id == TB_Undefined)
        {
            QWidget* spacer = new QWidget(this);
            spacer->setMinimumSize(30,0);
            m_current_dms_view_actions.push_back(m_toolbar->insertWidget(first_toolbar_detail_pages_action, spacer));
            continue;
        }

        auto button_data = getToolbarButtonData(button_id);
        auto button_icon = QIcon(button_data.icons[0]);
        auto action = new DmsToolbuttonAction(button_icon, view_style==ViewStyle::tvsTableView ? button_data.text[0] : button_data.text[1], m_toolbar, button_data, view_style);
        m_current_dms_view_actions.push_back(action);
        auto is_command_enabled = dv->OnCommandEnable(button_id) == CommandStatus::ENABLED;
        if (!is_command_enabled)
            action->setDisabled(true);
        
        m_toolbar->insertAction(first_toolbar_detail_pages_action, action);
        //m_toolbar->addAction(action); // TODO: Possible memory leak, ownership of action not transferred to m_toolbar

        // connections
        connect(action, &DmsToolbuttonAction::triggered, action, &DmsToolbuttonAction::onToolbuttonPressed);
    }
    m_current_toolbar_style = view_style;
}

bool MainWindow::event(QEvent* event)
{
    if (event->type() == QEvent::WindowActivate)
    {
        QTimer::singleShot(0, this, [=]() 
            { 
                auto vos = ReportChangedFiles(true); // TODO: report changed files not always returning if files are changed.
                if (vos.CurrPos())
                {
                    auto changed_files = std::string(vos.GetData(), vos.GetDataEnd());
                    m_file_changed_window->setFileChangedMessage(changed_files);
                    m_file_changed_window->show();
                }
            });
    }

    return QMainWindow::event(event);
}

std::string fillOpenConfigSourceCommand(const std::string command, const std::string filename, const std::string line)
{
    //tested for "%env:ProgramFiles%\Notepad++\Notepad++.exe" "%F" -n%L and "%ProgramFiles32%\Crimson Editor\cedt.exe" /L:%L "%F"
    std::string result = command;
    auto fnp_pos = result.find("%F");
    if (fnp_pos == std::string::npos)
        return result;
    result.replace(fnp_pos, 2, "");
    result.insert(fnp_pos, filename);

    auto lnp_pos = result.find("%L");
    if (lnp_pos == std::string::npos)
        return result;
    result.replace(lnp_pos, 2, "");
    result.insert(lnp_pos, line);
    return result;
}

void MainWindow::openConfigSourceDirectly(std::string_view filename, std::string_view line)
{
    std::string filename_dos_style = ConvertDmsFileNameAlways(SharedStr(filename.data())).c_str();
    std::string command = GetGeoDmsRegKey("DmsEditor").c_str();
    std::string unexpanded_open_config_source_command = "";
    std::string open_config_source_command = "";
    if (!filename.empty() && !line.empty() && !command.empty())
    {
        unexpanded_open_config_source_command = fillOpenConfigSourceCommand(command, std::string(filename), std::string(line));
        const TreeItem* ti = getCurrentTreeItem();
        if (!ti)
            ti = getRootTreeItem();

        if (!ti)
            open_config_source_command = AbstrStorageManager::Expand("", unexpanded_open_config_source_command.c_str()).c_str();// AbstrStorageManager::GetFullStorageName("", unexpanded_open_config_source_command.c_str()).c_str();
        else
            open_config_source_command = AbstrStorageManager::Expand(ti, SharedStr(unexpanded_open_config_source_command.c_str())).c_str();//AbstrStorageManager::GetFullStorageName(ti, SharedStr(unexpanded_open_config_source_command.c_str())).c_str();

        assert(!open_config_source_command.empty());

        QProcess process;
        QStringList args = QProcess::splitCommand(QString(open_config_source_command.c_str()));
        process.setProgram(args.takeFirst());
        process.setArguments(args);
        if (process.startDetached())
            reportF(MsgCategory::commands, SeverityTypeID::ST_MajorTrace, open_config_source_command.c_str());
        else
            reportF(MsgCategory::commands, SeverityTypeID::ST_Warning, "Unable to start open config source command %s.", open_config_source_command.c_str());
    }
}

void MainWindow::openConfigSource()
{
    auto filename =  ConvertDmsFileNameAlways(getCurrentTreeItem()->GetConfigFileName());
    std::string line = std::to_string(getCurrentTreeItem()->GetConfigFileLineNr());
    openConfigSourceDirectly(filename.c_str(), line);
}

TIC_CALL BestItemRef TreeItem_GetErrorSourceCaller(const TreeItem* src);

void MainWindow::stepToFailReason()
{
    auto ti = getCurrentTreeItem();
    if (!ti)
        return;

    if (!ti->WasFailed())
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

void MainWindow::visual_update_treeitem()
{
    createView(ViewStyle::tvsUpdateItem);
}

void MainWindow::visual_update_subtree()
{
    createView(ViewStyle::tvsUpdateTree);
}

void MainWindow::toggle_treeview()
{
    bool isVisible = m_treeview_dock->isVisible();
    m_treeview_dock->setVisible(!isVisible);
}

void MainWindow::toggle_detailpages()
{
    //bool isVisible = m_detail_pages->isVisible();
    //m_detail_pages->setVisible(!isVisible);
    bool isVisible = m_detailpages_dock->isVisible();
    m_detailpages_dock->setVisible(!isVisible);

    m_general_page_action->setVisible(!isVisible);
    m_explore_page_action->setVisible(!isVisible);
    m_properties_page_action->setVisible(!isVisible);
    m_configuration_page_action->setVisible(!isVisible);
    m_sourcedescr_page_action->setVisible(!isVisible);
    m_metainfo_page_action->setVisible(!isVisible);
}

void MainWindow::toggle_eventlog()
{
    bool isVisible = m_eventlog_dock->isVisible();
    m_eventlog_dock->setVisible(!isVisible);
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
    // Modal
    auto optionsWindow = new DmsGuiOptionsWindow(this);
    optionsWindow->show();
}

void MainWindow::advanced_options()
{
    // Modal
    auto optionsWindow = new DmsAdvancedOptionsWindow(this);
    optionsWindow->show();
}

void MainWindow::config_options()
{
    auto optionsWindow = new DmsConfigOptionsWindow(this);
    optionsWindow->show();
}

void MainWindow::code_analysis_set_source()
{
   try 
   {
       TreeItem_SetAnalysisSource(getCurrentTreeItem());
   }
   catch (...)
   {
       auto errMsg = catchException(false);
   }
}

void MainWindow::code_analysis_set_target()
{
    try
    {
        TreeItem_SetAnalysisTarget(getCurrentTreeItem(), true);
    }
    catch (...)
    {
        auto errMsg = catchException(false);
    }
}

void MainWindow::code_analysis_add_target()
{
    try
    {
        TreeItem_SetAnalysisTarget(getCurrentTreeItem(), false);
    }
    catch (...)
    {
        auto errMsg = catchException(false);
    }
}

void MainWindow::code_analysis_clr_targets()
{
    TreeItem_SetAnalysisSource(nullptr);
//    TreeItem_SetAnalysisTarget(nullptr, true);  // REMOVE, al gedaan door SetAnalysisSource
}



bool MainWindow::reportErrorAndTryReload(ErrMsgPtr error_message_ptr)
{
    VectorOutStreamBuff buffer;
    auto streamType = OutStreamBase::ST_HTM;
    auto msgOut = std::unique_ptr<OutStreamBase>(XML_OutStream_Create(&buffer, streamType, "", calcRulePropDefPtr));

    *msgOut << *error_message_ptr;
    buffer.WriteByte(0);

    TheOne()->m_error_window->setErrorMessageHtml(buffer.GetData());
    auto dialogResult = TheOne()->m_error_window->exec();
    if (dialogResult == QDialog::DialogCode::Rejected)
        return false;
    assert(dialogResult == QDialog::DialogCode::Accepted);
    bool reloadResult = TheOne()->reopen();
    return reloadResult;
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
    if (openErrorOnFailedCurrentItem())
        return;

    try
    {
        auto currItem = getCurrentTreeItem();
        static UInt32 s_ViewCounter = 0;
        if (!currItem)
            return;

        auto desktopItem = GetDefaultDesktopContainer(m_root); // rootItem->CreateItemFromPath("DesktopInfo");
        auto viewContextItem = desktopItem->CreateItemFromPath(mySSPrintF("View%d", s_ViewCounter++).c_str());

        SuspendTrigger::Resume();
        auto dms_mdi_subwindow = std::make_unique<QDmsViewArea>(m_mdi_area.get(), viewContextItem, currItem, viewStyle);
        connect(dms_mdi_subwindow.get(), &QDmsViewArea::windowStateChanged, dms_mdi_subwindow.get(), &QDmsViewArea::onWindowStateChanged);
        auto dms_view_window_icon = QIcon();
        switch (viewStyle)
        {
        case (ViewStyle::tvsTableView): {dms_view_window_icon.addFile(":/res/images/TV_table.bmp"); break; }
        case (ViewStyle::tvsMapView): {dms_view_window_icon.addFile(":/res/images/TV_globe.bmp"); break; }
        default: {dms_view_window_icon.addFile(":/res/images/TV_table.bmp"); break; }
        }
        dms_mdi_subwindow->setWindowIcon(dms_view_window_icon);
        m_mdi_area->addSubWindow(dms_mdi_subwindow.get());
        dms_mdi_subwindow.release();
    }
    catch (...)
    {
        auto errMsg = catchException(false);
        reportErrorAndTryReload(errMsg);
    }
}

void MainWindow::defaultViewOrAddItemToCurrentView()
{
    auto active_mdi_subwindow = dynamic_cast<QDmsViewArea*>(m_mdi_area->activeSubWindow());
    if (!active_mdi_subwindow || !active_mdi_subwindow->getDataView()->CanContain(m_current_item))
    {
        defaultView();
        return;
    }

    SHV_DataView_AddItem(active_mdi_subwindow->getDataView(), m_current_item, false);
}

void MainWindow::defaultView()
{
    //reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "Command: Defaultview for item %s", m_current_item->GetFullName());
    createView(SHV_GetDefaultViewStyle(m_current_item));
}

void MainWindow::mapView()
{
    reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "Command: create new Mapview for item %s", m_current_item->GetFullName());
    createView(ViewStyle::tvsMapView);
}

void MainWindow::tableView()
{
    reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "Command: create new Tableview for item %s", m_current_item->GetFullName());
    createView(ViewStyle::tvsTableView);
}

void geoDMSContextMessage(ClientHandle clientHandle, CharPtr msg)
{
    assert(IsMainThread());
    auto dms_main_window = reinterpret_cast<MainWindow*>(clientHandle);
    dms_main_window->setStatusMessage(msg);
    return;
}

void MainWindow::CloseConfig()
{
    if (m_mdi_area)
    {
        bool has_active_dms_views = m_mdi_area->subWindowList().size();
        m_mdi_area->closeAllSubWindows();
        if (has_active_dms_views)
            m_mdi_area->repaint();
    }

    if (m_root)
    {
        m_detail_pages->leaveThisConfig(); // reset ValueInfo cached results
        m_dms_model->reset();
        m_treeview->reset();

        m_dms_model->setRoot(nullptr);
        m_root->EnableAutoDelete();
        m_root = nullptr;
        m_current_item.reset();
        m_current_item = nullptr;
    }
    scheduleUpdateToolbar();
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

    for (auto it_rf = recent_files_from_registry.begin(); it_rf != recent_files_from_registry.end();)
    {
        if ((strnicmp(it_rf->c_str(), "file:", 5) != 0) && !std::filesystem::exists(*it_rf) || it_rf->empty())
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
        auto new_recent_file_action = new DmsRecentFileButtonAction(m_recent_files_actions.size() + 1, cfg, m_file_menu.get());
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

        auto orgConfigFilePath = SharedStr(configFilePath);
        m_currConfigFileName = ConvertDosFileName(orgConfigFilePath); // replace back-slashes to linux/dms style separators and prefix //SYSTEM/path with 'file:'
        
        auto folderName = splitFullPath(m_currConfigFileName.c_str());
        if (strnicmp(folderName.c_str(), "file:",5) == 0)
            SetCurrentDir(ConvertDmsFileNameAlways(std::move(folderName)).c_str());

        auto newRoot = CreateTreeFromConfiguration(m_currConfigFileName.c_str());

        m_root = newRoot;
        if (m_root)
        {
            SharedStr configFilePathStr = DelimitedConcat(ConvertDosFileName(GetCurrentDir()), ConvertDosFileName(m_currConfigFileName));

            insertCurrentConfigInRecentFiles(configFilePathStr.c_str());
            SetGeoDmsRegKeyString("LastConfigFile", configFilePathStr.c_str());

            m_treeview->setItemDelegate(new TreeItemDelegate(m_treeview));
            m_treeview->setModel(m_dms_model.get());
            m_treeview->setRootIndex(m_treeview->rootIndex().parent());
            m_treeview->scrollTo({});
        }
    }
    catch (...)
    {
        m_dms_model->setRoot(nullptr);
        setCurrentTreeItem(nullptr);
        auto x = catchException(false);
        bool reloadSucceeded = reportErrorAndTryReload(x);
        return reloadSucceeded;
    }
    m_dms_model->setRoot(m_root);
    clearActionsForEmptyCurrentItem();
    //setCurrentTreeItem(m_root); // as an example set current item to root, which emits signal currentItemChanged
    m_current_item_bar->setDmsCompleter();
    updateCaption();
    m_dms_model->reset();
    m_eventlog->scrollToBottomThrottled();
    return true;
}

// TODO: clean-up specification of unused parameters at the calling sites, or do use them.
void MainWindow::OnViewAction(const TreeItem* tiContext, CharPtr sAction, Int32 /*nCode*/, Int32 /*x*/, Int32 /*y*/, bool /*doAddHistory*/, bool /*isUrl*/, bool /*mustOpenDetailsPage*/)
{
    assert(IsMainThread());
    MainWindow::TheOne()->m_detail_pages->DoViewAction(const_cast<TreeItem*>(tiContext), sAction);
}

// TODO: BEGIN move to separate file UpdatableTextBrowser.h

struct QUpdatableTextBrowser : QTextBrowser, MsgGenerator
{
    using QTextBrowser::QTextBrowser;

    void restart_updating()
    {
        m_Waiter.start(this);
        QTimer::singleShot(0, [this]()
            {
                if (!this->update())
                    this->restart_updating();
                else
                    m_Waiter.end();
            }
        );
    }
    void GenerateDescription() override
    {
        auto pw = dynamic_cast<QMdiSubWindow*>(parentWidget());
        if (!pw)
            return;
        SetText(SharedStr(pw->windowTitle().toStdString().c_str()));
    }

protected:
    Waiter m_Waiter;

    virtual bool update() = 0;
};

// TODO: move to separate file StatisticsBrowser.h

struct StatisticsBrowser : QUpdatableTextBrowser
{
    using QUpdatableTextBrowser::QUpdatableTextBrowser;

    bool update() override
    {
        vos_buffer_type textBuffer;
        SuspendTrigger::Resume();
        bool done = NumericDataItem_GetStatistics(m_Context, textBuffer);
        textBuffer.emplace_back(char(0));
        setText(begin_ptr(textBuffer));
        return done;
    }
    SharedTreeItemInterestPtr m_Context;
};

// TODO: END move to separate file

void MainWindow::showStatisticsDirectly(const TreeItem* tiContext)
{
    if (openErrorOnFailedCurrentItem())
        return;

    auto* mdiSubWindow = new QMdiSubWindow(m_mdi_area.get()); // not a DmsViewArea //TODO: memory leak, no parent
    auto* textWidget = new StatisticsBrowser(mdiSubWindow);
    textWidget->m_Context = tiContext;
    tiContext->PrepareData();
    mdiSubWindow->setWidget(textWidget);

    SharedStr title = "Statistics View of " + tiContext->GetFullName();
    mdiSubWindow->setWindowTitle(title.c_str());
    mdiSubWindow->setWindowIcon(QPixmap(":/res/images/DP_statistics.bmp"));
    m_mdi_area->addSubWindow(mdiSubWindow);
    mdiSubWindow->setAttribute(Qt::WA_DeleteOnClose);
    mdiSubWindow->show();

    textWidget->restart_updating();
}


// TODO: BEGIN move to separate file ValueInfoBrowser.h

#include "TicBase.h"
#include "Explain.h"

struct ValueInfoBrowser : QUpdatableTextBrowser
{
    ValueInfoBrowser(auto parent)
        : QUpdatableTextBrowser(parent)
        , m_Context(Explain::CreateContext())
    {
        setOpenLinks(false);
        setOpenExternalLinks(false);
        connect(this, &QTextBrowser::anchorClicked, MainWindow::TheOne()->m_detail_pages, &DmsDetailPages::onAnchorClicked);
    }

    bool update() override;

    Explain::context_handle m_Context;
    SharedDataItemInterestPtr m_StudyObject;
    SizeT m_Index;

    vos_buffer_type m_VOS_buffer;
};

// TODO: move to separate file ValueInfoBrowser.cpp

bool ValueInfoBrowser::update()
{
    m_VOS_buffer.clear();
    ExternalVectorOutStreamBuff outStreamBuff(m_VOS_buffer);

    auto xmlOut = OutStream_HTM(&outStreamBuff, "html", nullptr);
    SuspendTrigger::Resume();


    bool done = Explain::AttrValueToXML(m_Context.get(), m_StudyObject, &xmlOut, m_Index, "", true);
    outStreamBuff.WriteByte(char(0));

    setText(outStreamBuff.GetData());

    // clean-up;
    if (done)
        m_VOS_buffer = vos_buffer_type();
    return done;
}

// TODO: END move to separate file

void MainWindow::showValueInfo(const AbstrDataItem* studyObject, SizeT index)
{
    assert(studyObject);
 
    auto* mdiSubWindow = new QMdiSubWindow(m_mdi_area.get()); // not a DmsViewArea //TODO: no parent, memory leak
    auto* textWidget = new ValueInfoBrowser(mdiSubWindow);
    textWidget->m_Context = Explain::CreateContext();
    textWidget->m_StudyObject = studyObject;
    textWidget->m_Index = index;

    mdiSubWindow->setWidget(textWidget);
    auto title = mySSPrintF("ValueInfo for row %d of %s", index, studyObject->GetFullName());
    mdiSubWindow->setWindowTitle(title.c_str());
    mdiSubWindow->setWindowIcon(QPixmap(":/res/images/DP_ValueInfo.bmp"));
    m_mdi_area->addSubWindow(mdiSubWindow);
    mdiSubWindow->setAttribute(Qt::WA_DeleteOnClose);
    mdiSubWindow->show();

    textWidget->restart_updating();
}

void MainWindow::setStatusMessage(CharPtr msg)
{
    m_StatusMsg = msg;
    updateStatusMessage();
}

static bool s_IsTiming = false;
static std::time_t s_BeginTime = 0;

void MainWindow::begin_timing(AbstrMsgGenerator* /*ach*/)
{
    if (s_IsTiming)
        return;

    s_IsTiming = true;
    s_BeginTime = std::time(nullptr);
}

std::time_t passed_time(const MainWindow::processing_record& pr)
{
    return std::get<1>(pr) - std::get<0>(pr);
}

SharedStr passed_time_str(CharPtr preFix, time_t passedTime)
{
    SharedStr result;
    if (passedTime >= 24 * 3600)
    {
        result = AsString(passedTime / (24 * 3600)) + " days and ";
        passedTime %= 24 * 3600;
    }
    assert(passedTime <= (24 * 3600));
    result = mySSPrintF("%s%s%02d:%02d:%02d"
        , preFix
        , result.c_str()
        , passedTime / 3600, (passedTime / 60) % 60, passedTime % 60
    );
    return result;
}

void MainWindow::end_timing(AbstrMsgGenerator* ach)
{
    if (!s_IsTiming)
        return;

    s_IsTiming = false;
    auto current_processing_record = processing_record(s_BeginTime, std::time(nullptr), SharedStr());
    auto passedTime = passed_time(current_processing_record);
    if (passedTime < 2)
        return;

    if (passedTime > 5)
        MessageBeep(MB_OK); // Beep after 5 sec of continuous work

    if (m_processing_records.size() >= 10 && passedTime < passed_time(m_processing_records.front()))
        return;

    if (ach)
        std::get<SharedStr>(current_processing_record) = SharedStr(ach->GetDescription());

    auto comparator = [](std::time_t passedTime, const processing_record& rhs) { return passedTime <= passed_time(rhs);  };
    auto insertionPoint = std::upper_bound(m_processing_records.begin(), m_processing_records.end(), passedTime, comparator);
    bool top_of_records_is_changing =( insertionPoint == m_processing_records.end());
    m_processing_records.insert(insertionPoint, current_processing_record);
    if (m_processing_records.size() > 10)
        m_processing_records.erase(m_processing_records.begin());
    if (!top_of_records_is_changing)
        return;
    assert(passedTime == passed_time(m_processing_records.back()));
   

//    SHV_SetBusyMode(false);
//    ChangeCursor(g_OldCursor);

    m_LongestProcessingRecordTxt = passed_time_str(" - max processing time: ", passedTime);

    updateStatusMessage();
}

static UInt32 s_ReentrancyCount = 0;
void MainWindow::updateStatusMessage()
{
    if (s_ReentrancyCount)
        return;

    auto fullMsg = m_StatusMsg + m_LongestProcessingRecordTxt;

    DynamicIncrementalLock<> incremental_lock(s_ReentrancyCount);
    
    statusBar()->showMessage(fullMsg.c_str());
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
        assert(IsMainThread());
        auto* createStruct = const_cast<MdiCreateStruct*>(reinterpret_cast<const MdiCreateStruct*>(self));
        assert(createStruct);
        new QDmsViewArea(mainWindow->m_mdi_area.get(), createStruct); // TODO: no parent, memory leak
        return;
    }
    case CC_Activate:
        assert(IsMainThread());
        mainWindow->setCurrentTreeItem(const_cast<TreeItem*>(self));
        return;

    case CC_ShowStatistics:
        assert(IsMainThread());
        mainWindow->showStatisticsDirectly(self);
    }
    // MainWindow could have been destroyed
    if (s_CurrMainWindow)
    {
        assert(s_CurrMainWindow == mainWindow);
        auto tv = mainWindow->m_treeview;
        if (IsMainThread())
            tv->update(); // this actually only invalidates any drawn area and causes repaint later
        else
            QTimer::singleShot(0, tv, [tv]() { tv->update(); });
        mainWindow->m_detail_pages->onTreeItemStateChange();
    }
}

#include "waiter.h"

void OnStartWaiting(void* clientHandle, AbstrMsgGenerator* ach)
{
    assert(IsMainThread());
    QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    reinterpret_cast<MainWindow*>(clientHandle)->begin_timing(ach);
}

void OnEndWaiting(void* clientHandle, AbstrMsgGenerator* ach)
{
    QGuiApplication::restoreOverrideCursor();
    reinterpret_cast<MainWindow*>(clientHandle)->end_timing(ach);
}

void MainWindow::setupDmsCallbacks()
{
    DMS_RegisterMsgCallback(&geoDMSMessage, this);

    DMS_SetContextNotification(&geoDMSContextMessage, this);
    SHV_SetCreateViewActionFunc(&OnViewAction);

    DMS_RegisterStateChangeNotification(AnyTreeItemStateHasChanged, this);
    register_overlapping_periods_callback(OnStartWaiting, OnEndWaiting, this);
}

void MainWindow::cleanupDmsCallbacks()
{
    unregister_overlapping_periods_callback(OnStartWaiting, OnEndWaiting, this);
    DMS_ReleaseStateChangeNotification(AnyTreeItemStateHasChanged, this);

    DMS_SetContextNotification(nullptr, nullptr);
    SHV_SetCreateViewActionFunc(nullptr);

    DMS_ReleaseMsgCallback(&geoDMSMessage, this);
}

void MainWindow::createActions()
{
    m_file_menu = std::make_unique<QMenu>(tr("&File"));
    menuBar()->addMenu(m_file_menu.get());
    m_current_item_bar_container = addToolBar(tr("Current item bar"));

    m_treeitem_visit_history = std::make_unique<QComboBox>();
    m_treeitem_visit_history->setFixedWidth(18);
    m_treeitem_visit_history->setFixedHeight(18);
    m_treeitem_visit_history->setFrame(false);
    //m_treeitem_visit_history->setMinimumSize(QSize(0, 0));
    m_treeitem_visit_history->setStyleSheet("QComboBox QAbstractItemView {\n"
                                                "min-width:400px;"
                                            "}\n"
                                            "QComboBox::drop-down:button{\n"
                                                "background-color: transparant;\n"
                                                "padding: 5px;\n"
                                            "}\n"
                                            "QComboBox::down-arrow {\n"
                                                "image: url(:/res/images/arrow_down.png);\n"
                                            "}\n");
   

    m_current_item_bar_container->addWidget(m_treeitem_visit_history.get());

    m_current_item_bar = std::make_unique<DmsCurrentItemBar>(this);
    
    m_current_item_bar_container->addAction(m_back_action.get());
    m_current_item_bar_container->addAction(m_forward_action.get());
    m_current_item_bar_container->addWidget(m_current_item_bar.get());

    connect(m_current_item_bar.get(), &DmsCurrentItemBar::editingFinished, m_current_item_bar.get(), &DmsCurrentItemBar::onEditingFinished);
    connect(m_treeitem_visit_history.get(), &QComboBox::currentTextChanged, m_current_item_bar.get(), &DmsCurrentItemBar::setPathDirectly);

    addToolBarBreak();

    connect(m_mdi_area.get(), &QDmsMdiArea::subWindowActivated, this, &MainWindow::scheduleUpdateToolbar);
    //auto openIcon = QIcon::fromTheme("document-open", QIcon(":res/images/open.png"));
    auto fileOpenAct = new QAction(tr("&Open Configuration File"), this);
    fileOpenAct->setShortcuts(QKeySequence::Open);
    fileOpenAct->setStatusTip(tr("Open an existing configuration file"));
    connect(fileOpenAct, &QAction::triggered, this, &MainWindow::fileOpen);
    m_file_menu->addAction(fileOpenAct);

    auto reOpenAct = new QAction(tr("&Reopen current Configuration"), this);
    reOpenAct->setShortcut(QKeySequence(tr("Alt+R")));
    reOpenAct->setStatusTip(tr("Reopen the current configuration and reactivate the current active item"));
    connect(reOpenAct, &QAction::triggered, this, &MainWindow::reopen);
    m_file_menu->addAction(reOpenAct);
    //fileToolBar->addAction(reOpenAct);

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
    auto step_to_failreason_shortcut = new QShortcut(QKeySequence(tr("F2")), this);
    connect(step_to_failreason_shortcut, &QShortcut::activated, this, &MainWindow::stepToFailReason);
    connect(m_step_to_failreason_action.get(), &QAction::triggered, this, &MainWindow::stepToFailReason);

    // go to causa prima
    m_go_to_causa_prima_action = std::make_unique<QAction>(tr("&Run up to Causa Prima (i.e. repeated Step up)"));
    auto go_to_causa_prima_shortcut = new QShortcut(QKeySequence(tr("Shift+F2")), this);
    connect(go_to_causa_prima_shortcut, &QShortcut::activated, this, &MainWindow::runToFailReason);
    connect(m_go_to_causa_prima_action.get(), &QAction::triggered, this, &MainWindow::runToFailReason);
    
    // open config source
    m_edit_config_source_action = std::make_unique<QAction>(tr("&Edit Config Source"));
    auto edit_config_source_shortcut = new QShortcut(QKeySequence(tr("Ctrl+E")), this);
    connect(edit_config_source_shortcut, &QShortcut::activated, this, &MainWindow::openConfigSource);
    connect(m_edit_config_source_action.get(), &QAction::triggered, this, &MainWindow::openConfigSource);
    m_edit_menu->addAction(m_edit_config_source_action.get());

    // update treeitem
    m_update_treeitem_action = std::make_unique<QAction>(tr("&Update TreeItem"));
    auto update_treeitem_shortcut = new QShortcut(QKeySequence(tr("Ctrl+U")), this);
    connect(update_treeitem_shortcut, &QShortcut::activated, this, &MainWindow::visual_update_treeitem);
    connect(m_update_treeitem_action.get(), &QAction::triggered, this, &MainWindow::visual_update_treeitem);

    // update subtree
    m_update_subtree_action = std::make_unique<QAction>(tr("&Update Subtree"));
    auto update_subtree_shortcut = new QShortcut(QKeySequence(tr("Ctrl+T")), this);
    connect(update_subtree_shortcut, &QShortcut::activated, this, &MainWindow::visual_update_subtree);
    connect(m_update_subtree_action.get(), &QAction::triggered, this, &MainWindow::visual_update_subtree);
    
    // invalidate action
    m_invalidate_action = std::make_unique<QAction>(tr("&Invalidate"));
    m_invalidate_action->setShortcut(QKeySequence(tr("Ctrl+I")));
    m_invalidate_action->setShortcutContext(Qt::ApplicationShortcut);
    //connect(m_invalidate_action.get(), &QAction::triggered, this, & #TODO);

    m_view_menu = std::make_unique<QMenu>(tr("&View"));
    menuBar()->addMenu(m_view_menu.get());

    m_defaultview_action = std::make_unique<QAction>(tr("Default View"));
    m_defaultview_action->setStatusTip(tr("Open current selected TreeItem's default view."));
    auto defaultview_shortcut = new QShortcut(QKeySequence(tr("Ctrl+Alt+D")), this);
    connect(defaultview_shortcut, &QShortcut::activated, this, &MainWindow::defaultView);
    connect(m_defaultview_action.get(), &QAction::triggered, this, &MainWindow::defaultView);
    m_view_menu->addAction(m_defaultview_action.get());

    // table view
    m_tableview_action = std::make_unique<QAction>(tr("&Table View"));
    m_tableview_action->setStatusTip(tr("Open current selected TreeItem's in a table view."));
    auto tableview_shortcut = new QShortcut(QKeySequence(tr("Ctrl+D")), this);
    connect(tableview_shortcut, &QShortcut::activated, this, &MainWindow::tableView);
    connect(m_tableview_action.get(), &QAction::triggered, this, &MainWindow::tableView);
    m_view_menu->addAction(m_tableview_action.get());

    // map view
    m_mapview_action = std::make_unique<QAction>(tr("&Map View"));
    m_mapview_action->setStatusTip(tr("Open current selected TreeItem's in a map view."));
    auto mapview_shortcut = new QShortcut(QKeySequence(tr("Ctrl+M")), this);
    connect(mapview_shortcut, &QShortcut::activated, this, &MainWindow::mapView);
    connect(m_mapview_action.get(), &QAction::triggered, this, &MainWindow::mapView);
    m_view_menu->addAction(m_mapview_action.get());

    // statistics view
    m_statistics_action = std::make_unique<QAction>(tr("&Statistics View"));
//    m_statistics_action->setShortcut(QKeySequence(tr("Ctrl+H")));
    connect(m_statistics_action.get(), &QAction::triggered, this, &MainWindow::showStatistics);
    m_view_menu->addAction(m_statistics_action.get());

    // histogram view
//    m_histogramview_action = std::make_unique<QAction>(tr("&Histogram View"));
//    m_histogramview_action->setShortcut(QKeySequence(tr("Ctrl+H")));
    //connect(m_histogramview_action.get(), &QAction::triggered, this, & #TODO);
    
    // process schemes
    m_process_schemes_action = std::make_unique<QAction>(tr("&Process Schemes"));
    //connect(m_process_schemes_action.get(), &QAction::triggered, this, & #TODO);
    //m_view_menu->addAction(m_process_schemes_action.get()); // TODO: to be implemented or not..

    m_view_calculation_times_action = std::make_unique<QAction>(tr("Calculation times"));
    connect(m_view_calculation_times_action.get(), &QAction::triggered, this, &MainWindow::view_calculation_times);
    m_view_menu->addAction(m_view_calculation_times_action.get());

    m_view_menu->addSeparator();
    m_toggle_treeview_action       = std::make_unique<QAction>(tr("Toggle TreeView"));
    m_toggle_detailpage_action     = std::make_unique<QAction>(tr("Toggle DetailPages"));
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
    m_toggle_treeview_action->setShortcutContext(Qt::ApplicationShortcut);
    m_toggle_detailpage_action->setShortcut(QKeySequence(tr("Alt+1")));
    m_toggle_detailpage_action->setShortcutContext(Qt::ApplicationShortcut);
    m_toggle_eventlog_action->setShortcut(QKeySequence(tr("Alt+2")));
    m_toggle_eventlog_action->setShortcutContext(Qt::ApplicationShortcut);
    m_toggle_toolbar_action->setShortcut(QKeySequence(tr("Alt+3")));
    m_toggle_toolbar_action->setShortcutContext(Qt::ApplicationShortcut);
    m_toggle_currentitembar_action->setShortcut(QKeySequence(tr("Alt+4")));
    m_toggle_currentitembar_action->setShortcutContext(Qt::ApplicationShortcut);
    m_view_menu->addAction(m_toggle_treeview_action.get());
    m_view_menu->addAction(m_toggle_detailpage_action.get());
    m_view_menu->addAction(m_toggle_eventlog_action.get());
    m_view_menu->addAction(m_toggle_toolbar_action.get());
    m_view_menu->addAction(m_toggle_currentitembar_action.get());
    connect(m_view_menu.get(), &QMenu::aboutToShow, this, &MainWindow::updateViewMenu);

    // tools menu
    m_tools_menu = std::make_unique<QMenu>("&Tools");
    menuBar()->addMenu(m_tools_menu.get());
    connect(m_tools_menu.get(), &QMenu::aboutToShow, this, &MainWindow::updateToolsMenu);

    m_gui_options_action = std::make_unique<QAction>(tr("&Gui Options"));
    connect(m_gui_options_action.get(), &QAction::triggered, this, &MainWindow::gui_options);
    m_tools_menu->addAction(m_gui_options_action.get());

    m_advanced_options_action = std::make_unique<QAction>(tr("&Local machine Options"));
    connect(m_advanced_options_action.get(), &QAction::triggered, this, &MainWindow::advanced_options); //TODO: change advanced options refs in local machine options
    m_tools_menu->addAction(m_advanced_options_action.get());

    m_config_options_action = std::make_unique<QAction>(tr("&Config Options"));
    connect(m_config_options_action.get(), &QAction::triggered, this, &MainWindow::config_options);
    m_tools_menu->addAction(m_config_options_action.get());

    m_code_analysis_submenu = std::make_unique<QMenu>("&Code analysis...");
    m_tools_menu->addMenu(m_code_analysis_submenu.get());

    m_code_analysis_set_source_action = std::make_unique<QAction>(tr("set source"));
    connect(m_code_analysis_set_source_action.get(), &QAction::triggered, this, &MainWindow::code_analysis_set_source);
    m_code_analysis_submenu->addAction(m_code_analysis_set_source_action.get());
    m_code_analysis_set_source_action->setShortcut(QKeySequence(tr("Alt+K")));
    m_code_analysis_set_source_action->setShortcutContext(Qt::ApplicationShortcut);
    this->addAction(m_code_analysis_set_source_action.get());
    m_code_analysis_set_target_action = std::make_unique<QAction>(tr("set target"));
    m_code_analysis_set_target_action->setShortcut(QKeySequence(tr("Alt+B")));
    m_code_analysis_set_target_action->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_code_analysis_set_target_action.get(), &QAction::triggered, this, &MainWindow::code_analysis_set_target);
    m_code_analysis_submenu->addAction(m_code_analysis_set_target_action.get());

    m_code_analysis_add_target_action = std::make_unique<QAction>(tr("add target"));
    connect(m_code_analysis_add_target_action.get(), &QAction::triggered, this, &MainWindow::code_analysis_add_target);
    m_code_analysis_submenu->addAction(m_code_analysis_add_target_action.get());
    m_code_analysis_add_target_action->setShortcut(QKeySequence(tr("Alt+N")));
    m_code_analysis_add_target_action->setShortcutContext(Qt::ApplicationShortcut);

    m_code_analysis_clr_targets_action = std::make_unique<QAction>(tr("clear target")); 
    connect(m_code_analysis_clr_targets_action.get(), &QAction::triggered, this, &MainWindow::code_analysis_clr_targets);
    m_code_analysis_submenu->addAction(m_code_analysis_clr_targets_action.get());

    m_eventlog_filter_toggle = std::make_unique<QAction>(tr("Eventlog filter"));
    connect(m_eventlog_filter_toggle.get(), &QAction::triggered, m_eventlog->m_event_filter_toggle.get(), &QCheckBox::click);
    m_tools_menu->addAction(m_eventlog_filter_toggle.get());

    // window menu
    m_window_menu = std::make_unique<QMenu>(tr("&Window"));
    menuBar()->addMenu(m_window_menu.get());
    auto win1_action = new QAction(tr("&Tile Windows"), m_window_menu.get());
    win1_action->setShortcut(QKeySequence(tr("Ctrl+Alt+W")));
    win1_action->setShortcutContext(Qt::ApplicationShortcut);
    connect(win1_action, &QAction::triggered, m_mdi_area.get(), &QDmsMdiArea::onTileSubWindows);

    auto win2_action = new QAction(tr("&Cascade"), m_window_menu.get());
    win2_action->setShortcut(QKeySequence(tr("Shift+Ctrl+W")));
    win2_action->setShortcutContext(Qt::ApplicationShortcut);
    connect(win2_action, &QAction::triggered, m_mdi_area.get(), &QDmsMdiArea::onCascadeSubWindows);

    auto win3_action = new QAction(tr("&Close"), m_window_menu.get());
    win3_action->setShortcut(QKeySequence(tr("Ctrl+W")));
    connect(win3_action, &QAction::triggered, m_mdi_area.get(), &QDmsMdiArea::closeActiveSubWindow);

    auto win4_action = new QAction(tr("&Close All"), m_window_menu.get());
    win4_action->setShortcut(QKeySequence(tr("Ctrl+L")));
    win4_action->setShortcutContext(Qt::ApplicationShortcut);
    connect(win4_action, &QAction::triggered, m_mdi_area.get(), &QDmsMdiArea::closeAllSubWindows);

    auto win5_action = new QAction(tr("&Close All But This"), m_window_menu.get());
    win5_action->setShortcut(QKeySequence(tr("Ctrl+B")));
    win5_action->setShortcutContext(Qt::ApplicationShortcut);
    connect(win5_action, &QAction::triggered, m_mdi_area.get(), &QDmsMdiArea::closeAllButActiveSubWindow);

    m_window_menu->addActions({win1_action, win2_action, win3_action, win4_action, win5_action});
    m_window_menu->addSeparator();
    connect(m_window_menu.get(), &QMenu::aboutToShow, this, &MainWindow::updateWindowMenu);

    // help menu
    m_help_menu = std::make_unique<QMenu>(tr("&Help"));
    menuBar()->addMenu(m_help_menu.get());
    auto about_action = new QAction(tr("&About GeoDms"), m_help_menu.get());
    about_action->setStatusTip(tr("Show the application's About box"));
    connect(about_action, &QAction::triggered, this, &MainWindow::aboutGeoDms);
    m_help_menu->addAction(about_action);
    
    auto aboutQt_action = new QAction(tr("About &Qt"), m_help_menu.get());
    aboutQt_action->setStatusTip(tr("Show the Qt library's About box"));
    connect(aboutQt_action, &QAction::triggered, qApp, &QApplication::aboutQt);
    m_help_menu->addAction(aboutQt_action);

    auto wiki_action = new QAction(tr("&Wiki"), m_help_menu.get());
    wiki_action->setStatusTip(tr("Open the GeoDms wiki in a browser"));
    connect(wiki_action, &QAction::triggered, this, &MainWindow::wiki);
    m_help_menu->addAction(wiki_action);
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
        auto qa = new DmsRecentFileButtonAction(recent_file_index, recent_file, m_file_menu.get());
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
    // disable actions not applicable to current item
    auto ti_is_or_is_in_template = m_current_item->InTemplate() && m_current_item->IsTemplate();
    m_update_treeitem_action->setDisabled(ti_is_or_is_in_template);
    m_update_subtree_action->setDisabled(ti_is_or_is_in_template);
    m_invalidate_action->setDisabled(ti_is_or_is_in_template);
    m_defaultview_action->setDisabled(ti_is_or_is_in_template);
    m_tableview_action->setDisabled(ti_is_or_is_in_template);
    m_mapview_action->setDisabled(ti_is_or_is_in_template);
    m_statistics_action->setDisabled(ti_is_or_is_in_template);

    m_toggle_treeview_action->setChecked(m_treeview->isVisible());
    m_toggle_detailpage_action->setChecked(m_detail_pages->isVisible());
    m_toggle_eventlog_action->setChecked(m_eventlog->isVisible());
    bool hasToolbar = !m_toolbar.isNull();
    m_toggle_toolbar_action->setEnabled(hasToolbar);
    if (hasToolbar)
        m_toggle_toolbar_action->setChecked(m_toolbar->isVisible());
    m_toggle_currentitembar_action->setChecked(m_current_item_bar_container->isVisible());

    m_processing_records.empty() ? m_view_calculation_times_action->setDisabled(true) : m_view_calculation_times_action->setEnabled(true);

}

void MainWindow::updateToolsMenu()
{
    m_code_analysis_add_target_action->setEnabled(m_current_item);
    m_code_analysis_clr_targets_action->setEnabled(m_current_item);
    m_code_analysis_set_source_action->setEnabled(m_current_item);
    m_code_analysis_set_target_action->setEnabled(m_current_item);
    m_config_options_action->setEnabled(DmsConfigOptionsWindow::hasOverridableConfigOptions());
}

void MainWindow::updateWindowMenu()
{
    // clear non persistent actions from window menu
    auto actions_to_remove = QList<QAction*>();
    int number_of_persistent_actions = 5;
    for (int i=number_of_persistent_actions; i<m_window_menu->actions().size(); i++)
        actions_to_remove.append(m_window_menu->actions().at(i));

    for (auto to_be_removed_action : actions_to_remove)
        m_window_menu->removeAction(to_be_removed_action);

    // reinsert window actions
    auto asw = m_mdi_area->currentSubWindow();
    for (auto* sw : m_mdi_area->subWindowList())
    {
        auto qa = new QAction(sw->windowTitle(), m_window_menu.get());
        connect(qa, &QAction::triggered, sw, [this, sw] { this->m_mdi_area->setActiveSubWindow(sw); });
        if (sw == asw)
        {
            qa->setCheckable(true);
            qa->setChecked(true);
        }
        m_window_menu->addAction(qa);
    }
}

bool is_filenameBase_eq_rootname(CharPtrRange fileName, CharPtrRange rootName)
{
    if (fileName.size() < 4)
        return false;
    if (fileName.size() != rootName.size() + 4)
        return false;
    if (strnicmp(fileName.begin(), rootName.begin(), rootName.size()))
        return false;
    if (strnicmp(fileName.begin() + rootName.size(), ".dms", 4))
        return false;
    return true;
}

void MainWindow::updateCaption()
{
    VectorOutStreamBuff buff;
    FormattedOutStream out(&buff, FormattingFlags());
    if (!m_currConfigFileName.empty())
    {
        auto fileName = getFileName(m_currConfigFileName.c_str());
        out << fileName;
        if (m_root)
        {
            auto rootName = m_root->GetName();
            CharPtr rootNameCStr = rootName.c_str();
            if (rootNameCStr && *rootNameCStr)
            {
                if (!is_filenameBase_eq_rootname(fileName, rootNameCStr))
                    out << " (aka " << rootNameCStr << ")";
            }
        }
        out << " in ";
        auto folderName = splitFullPath(m_currConfigFileName.c_str());
        out << folderName;
    }

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
    
    connect(statusBar(), &QStatusBar::messageChanged, this, &MainWindow::on_status_msg_changed);
    m_statusbar_coordinate_label = new QLabel("Coordinate", this);
    m_statusbar_coordinates = new QLineEdit(this);
    m_statusbar_coordinates->setReadOnly(true);
    m_statusbar_coordinates->setFixedWidth(310);
    m_statusbar_coordinates->setAlignment(Qt::AlignmentFlag::AlignRight);
    statusBar()->insertPermanentWidget(0, m_statusbar_coordinate_label);
    statusBar()->insertPermanentWidget(1, m_statusbar_coordinates);
    m_statusbar_coordinate_label->setVisible(false);
    m_statusbar_coordinates->setVisible(false);
}

void MainWindow::on_status_msg_changed(const QString& msg)
{
    if (msg.isEmpty())
        updateStatusMessage();
}

CharPtrRange myAscTime(const struct tm* timeptr)
{
    static char wday_name[7][4] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    static char mon_name[12][4] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    static char result[26];

    return myFixedBufferAsCharPtrRange(result, 26, "%.3s %.3s%3d %02d:%02d:%02d %d",
        wday_name[timeptr->tm_wday],
        mon_name[timeptr->tm_mon],
        timeptr->tm_mday, timeptr->tm_hour,
        timeptr->tm_min, timeptr->tm_sec,
        1900 + timeptr->tm_year);
}

void MainWindow::view_calculation_times()
{
    VectorOutStreamBuff vosb;
    FormattedOutStream os(&vosb, FormattingFlags());
    os << "Top " << m_processing_records.size() << " calculation times:\n";
    for (const auto& pr : m_processing_records)
    {
        os << passed_time_str("", passed_time(pr));
        os << " from " << myAscTime(std::localtime(& std::get<0>(pr)));
        os << " till " << myAscTime(std::localtime(& std::get<1>(pr)));
        os << ": " << std::get<SharedStr>(pr) << "\n";
    }
    os << char(0); // ends

    auto* mdiSubWindow = new QMdiSubWindow(m_mdi_area.get()); // not a DmsViewArea //TODO: memory leak, no parent
    auto* textWidget = new QTextBrowser(mdiSubWindow);
    mdiSubWindow->setWidget(textWidget);
    textWidget->setText(vosb.GetData());

    mdiSubWindow->setWindowTitle("Calculation time overview");
    mdiSubWindow->setWindowIcon(QPixmap(":/res/images/IconCalculationTimeOverview.bmp"));
    m_mdi_area->addSubWindow(mdiSubWindow);
    mdiSubWindow->setAttribute(Qt::WA_DeleteOnClose);
    mdiSubWindow->show();
}


void MainWindow::createDetailPagesDock()
{
    m_detailpages_dock = new QDockWidget(QObject::tr("DetailPages"), this);
    m_detailpages_dock->setTitleBarWidget(new QWidget(m_detailpages_dock));

    m_detail_pages = new DmsDetailPages(m_detailpages_dock);
    m_detailpages_dock->setWidget(m_detail_pages);
    //m_detail_pages->minimumSizeHint() = QSize(20,20);
    addDockWidget(Qt::RightDockWidgetArea, m_detailpages_dock);
    m_detail_pages->connectDetailPagesAnchorClicked();
}

void MainWindow::createDmsHelperWindowDocks()
{
    createDetailPagesDock();
//    m_detail_pages->setDummyText();

    m_treeview = createTreeview(this);
    m_eventlog = createEventLog(this);
    // connections below need constructed treeview and filters to work
    // TODO: refactor action/pushbutton logic

}

void MainWindow::back()
{
    if (!m_root)
        return;

    if (m_treeitem_visit_history->count() == 0) // empty
        return;

    if (m_treeitem_visit_history->currentIndex() == -1) // no current item set
        return;

    if (m_treeitem_visit_history->currentIndex() == 0) //already at beginning of history list
        return;

    int previous_item_index = m_treeitem_visit_history->currentIndex() - 1;
    m_treeitem_visit_history->setCurrentIndex(previous_item_index);

    auto best_item_ref = TreeItem_GetBestItemAndUnfoundPart(m_root, m_treeitem_visit_history->itemText(previous_item_index).toUtf8());
    auto found_treeitem = best_item_ref.first;

    if (found_treeitem == m_current_item)
        return;

    MainWindow::TheOne()->setCurrentTreeItem(const_cast<TreeItem*>(found_treeitem), false);
}

void MainWindow::forward()
{
    if (!m_root)
        return;

    if (m_treeitem_visit_history->count() == 0) // empty
        return;

    if (m_treeitem_visit_history->count() - 1 == m_treeitem_visit_history->currentIndex()) // already at end of history list
        return;

    int next_item_index = m_treeitem_visit_history->currentIndex() + 1;
    m_treeitem_visit_history->setCurrentIndex(next_item_index);
    
    auto best_item_ref = TreeItem_GetBestItemAndUnfoundPart(m_root, m_treeitem_visit_history->itemText(next_item_index).toUtf8());
    auto found_treeitem = best_item_ref.first;

    if (found_treeitem == m_current_item)
        return;

    MainWindow::TheOne()->setCurrentTreeItem(const_cast<TreeItem*>(found_treeitem), false);
}