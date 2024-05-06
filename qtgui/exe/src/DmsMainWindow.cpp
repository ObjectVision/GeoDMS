// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcInterface.h"
#include "StxInterface.h"
#include "TicInterface.h"

#include "act/MainThread.h"
#include "dbg/Check.h"
#include "dbg/debug.h"
#include "dbg/DebugLog.h"
#include "dbg/DmsCatch.h"
#include "dbg/SeverityType.h"
#include "dbg/Timer.h"

#include "utl/Encodes.h"
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
#include <QWidgetAction>
#include <QObject>


#include "DmsMainWindow.h"
#include "TestScript.h"

#include "DmsEventLog.h"
#include "DmsViewArea.h"
#include "DmsTreeView.h"
#include "DmsDetailPages.h"
#include "DmsOptions.h"
#include "DmsExport.h"
#include "DmsValueInfo.h"
#include "StatisticsBrowser.h"

#include "DataView.h"
#include "StateChangeNotification.h"
#include "stg/AbstrStorageManager.h"
#include <regex>

#include "dataview.h"

static MainWindow* s_CurrMainWindow = nullptr;
UInt32 s_errorWindowActivationCount = 0;

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

bool MainWindow::ShowInDetailPage(SharedStr x)
{
    auto realm = Realm(x);
    if (realm.size() == 3 && !strncmp(realm.begin(), "dms", 3))
        return true;
    if (!realm.empty() && realm.size() > 1)
        return false;
    CharPtrRange knownSuffix(".adms");
    return std::search(x.begin(), x.end(), knownSuffix.begin(), knownSuffix.end()) != x.end();
}

void MainWindow::SaveValueInfoImpl(CharPtr filename)
{
    auto dmsFileName = ConvertDosFileName(SharedStr(filename));
    auto expandedFilename = AbstrStorageManager::Expand(m_current_item, dmsFileName);
    FileOutStreamBuff buff(SharedStr(expandedFilename), nullptr, true, false);
    for (auto& child_object : children())
    {
        auto value_info_window_candidate = dynamic_cast<ValueInfoWindow*>(child_object);
        if (!value_info_window_candidate)
            continue;

        // TODO: implement this for qwebengineview
        //auto htmlSource = value_info_window_candidate->m_browser->toHtml();
        //auto htmlsourceAsUtf8 = htmlSource.toUtf8();
        //buff.WriteBytes(htmlsourceAsUtf8.data(), htmlsourceAsUtf8.size());
    }
}

void MainWindow::saveValueInfo()
{
    SaveValueInfoImpl("C:/LocalData/test/test_value_info.txt");
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
    m_ignore = new QPushButton(tr("&Ignore"), this);
    m_ignore->setMaximumSize(75, 30);
    m_terminate = new QPushButton(tr("&Terminate"), this);
    m_terminate->setMaximumSize(75, 30);

    m_reopen = new QPushButton(tr("&Reopen"), this);
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

CalculationTimesWindow::CalculationTimesWindow()
:   QMdiSubWindow::QMdiSubWindow()
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setProperty("viewstyle", ViewStyle::tvsCalculationTimes);
    installEventFilter(this);
}

CalculationTimesWindow::~CalculationTimesWindow()
{

}

bool CalculationTimesWindow::eventFilter(QObject* obj, QEvent* e)
{
    switch (e->type())
    {
    case QEvent::Close:
    {
        MainWindow::TheOne()->m_mdi_area->removeSubWindow(MainWindow::TheOne()->m_calculation_times_window.get());
        auto dms_mdi_subwindow_list = MainWindow::TheOne()->m_mdi_area->subWindowList();
        if (!dms_mdi_subwindow_list.empty())
        {
            MainWindow::TheOne()->m_mdi_area->setActiveSubWindow(dms_mdi_subwindow_list[0]);
            dms_mdi_subwindow_list[0]->showMaximized();
        }
        
        break;
    }
    }
    return QObject::eventFilter(obj, e);
}

MainWindow::MainWindow(CmdLineSetttings& cmdLineSettings)
{ 
    assert(s_CurrMainWindow == nullptr);
    s_CurrMainWindow = this;

    m_mdi_area = new QDmsMdiArea(this);

    // fonts
    int id = QFontDatabase::addApplicationFont(":/res/fonts/dmstext.ttf");
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    QFont dms_text_font(family, 10);
    QApplication::setFont(dms_text_font);
    QFontDatabase::addApplicationFont(":/res/fonts/remixicon.ttf");

    // helper dialogues
    m_file_changed_window = new DmsFileChangedWindow(this);
    m_error_window = new DmsErrorWindow(this);
    m_export_window = new DmsExportWindow(this);

    setCentralWidget(m_mdi_area.get());
    m_mdi_area->show();

    // calculation times
    m_calculation_times_window = std::make_unique<CalculationTimesWindow>();
    m_calculation_times_browser = std::make_unique<QTextBrowser>();
    m_calculation_times_window->setWidget(m_calculation_times_browser.get());

    createStatusBar();
    createDmsHelperWindowDocks();
    createDetailPagesActions();

    // connect current item changed signal to appropriate slots
    connect(this, &MainWindow::currentItemChanged, m_detail_pages, &DmsDetailPages::newCurrentItem);

    setupDmsCallbacks();

    m_dms_model = std::make_unique<DmsModel>();
    m_dms_model->updateChachedDisplayFlags();

    m_treeview->setModel(m_dms_model.get());

    createActions();

    // read initial last config file
    if (!cmdLineSettings.m_NoConfig)
    {
        CharPtr currentItemPath = "";
        if (cmdLineSettings.m_ConfigFileName.empty())
            cmdLineSettings.m_ConfigFileName = GetGeoDmsRegKey("LastConfigFile");
        else
        {
            if (cmdLineSettings.m_CurrItemFullNames.size())
                currentItemPath = cmdLineSettings.m_CurrItemFullNames.back().c_str();
        }
        if (!cmdLineSettings.m_ConfigFileName.empty())
           LoadConfig(cmdLineSettings.m_ConfigFileName.c_str(), currentItemPath);
    }

    updateCaption();
    updateTracelogHandle();

    setUnifiedTitleAndToolBarOnMac(true);
    scheduleUpdateToolbar();
    LoadColors();

    // set drawing size in pixels for polygons and arcs
    Float32 drawing_size_in_pixels = GetDrawingSizeInPixels(); // from registry
    SetDrawingSizeTresholdValue(drawing_size_in_pixels);

    setStyleSheet("QMainWindow::separator{ width: 5px; height: 5px; }");
    setFont(QApplication::font());
    auto menu_bar = menuBar();
    menu_bar->setFont(QApplication::font());
}

MainWindow::~MainWindow()
{
    g_IsTerminating = true;

    assert(s_CurrMainWindow == this);
    s_CurrMainWindow = nullptr;

    cleanupDmsCallbacks();
}

DmsCurrentItemBar::DmsCurrentItemBar(QWidget* parent)
    : QLineEdit(parent)
{
    setFont(QApplication::font());
    QRegularExpression rx("^[^0-9=+\\-|&!?><,.{}();\\]\\[][^=+\\-|&!?><,.{}();\\]\\[]+$");
    auto rx_validator = new QRegularExpressionValidator(rx, this);
    setValidator(rx_validator);
    setDmsCompleter();
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

void DmsCurrentItemBar::findItem(const TreeItem * context, QString path, bool updateHistory)
{
    if (!context)
        return;

    auto best_item_ref = TreeItem_GetBestItemAndUnfoundPart(context, path.toUtf8());
    auto found_treeitem = best_item_ref.first.get();
    if (!found_treeitem)
        return;
    MainWindow::TheOne()->setCurrentTreeItem(const_cast<TreeItem*>(found_treeitem), updateHistory);
}

void DmsCurrentItemBar::setPathDirectly(QString path)
{
    setText(path);
    findItem(MainWindow::TheOne()->getRootTreeItem(), path, false);
}

void DmsCurrentItemBar::onEditingFinished()
{
    findItem(MainWindow::TheOne()->getCurrentTreeItemOrRoot(), text().toUtf8(), true);
}

bool MainWindow::IsExisting()
{
    return s_CurrMainWindow;
}

auto MainWindow::CreateCodeAnalysisSubMenu(QMenu* menu) -> std::unique_ptr<QMenu>
{
    auto code_analysis_submenu = std::make_unique<QMenu>("&Code analysis...");
    menu->addMenu(code_analysis_submenu.get());

    code_analysis_submenu->addAction(m_code_analysis_set_source_action.get());
    code_analysis_submenu->addAction(m_code_analysis_set_target_action.get());
    code_analysis_submenu->addAction(m_code_analysis_add_target_action.get());
    code_analysis_submenu->addAction(m_code_analysis_clr_targets_action.get());
    return code_analysis_submenu;
}

MainWindow* MainWindow::TheOne()
{
//    assert(IsMainThread()); // or use a mutex to guard access to TheOne.
//    assert(s_CurrMainWindow);// main window destructor might already be in session, such as when called from the destructor of ValueInfoPanel
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
    auto ci = m_current_item.get();
    auto viewstyle_flags = ci ? SHV_GetViewStyleFlags(ci) : ViewStyleFlags::vsfNone;
    m_defaultview_action->setEnabled(viewstyle_flags & (ViewStyleFlags::vsfDefault | ViewStyleFlags::vsfTableView | ViewStyleFlags::vsfTableContainer | ViewStyleFlags::vsfMapView)); // TODO: vsfDefault appears to never be set
    m_tableview_action->setEnabled(viewstyle_flags & (ViewStyleFlags::vsfTableView | ViewStyleFlags::vsfTableContainer));
    m_mapview_action->setEnabled(viewstyle_flags & ViewStyleFlags::vsfMapView);
    m_statistics_action->setEnabled(ci ? IsDataItem(ci) : false);
    m_process_schemes_action->setEnabled(true);
    m_update_treeitem_action->setEnabled(true);
    m_invalidate_action->setEnabled(true);
    m_update_subtree_action->setEnabled(true);
    m_edit_config_source_action->setEnabled(true);
    
    try 
    {
        if (ci && !FindURL(ci).empty())
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

void MainWindow::setCurrentTreeItem(TreeItem* target_item, bool update_history)
{
    if (m_current_item == target_item)
        return;

    if (!target_item)
        return;

    MG_CHECK(!m_root || !target_item || isAncestor(m_root, target_item));
    if (target_item && !m_dms_model->show_hidden_items)
    {
        if (target_item->GetTSF(TSF_InHidden))
        {
            const TreeItem* visible_parent = target_item;
            while (visible_parent && visible_parent->GetTSF(TSF_InHidden))
                visible_parent = visible_parent->GetTreeParent();
            reportF(MsgCategory::other, SeverityTypeID::ST_Warning, "cannnot set '%1%' as Current Item, as it is a hidden sub-item of '%2%'"
                "\nHint: you can make hidden items visible in the Settings->GUI Options Dialog"
                , target_item->GetFullName().c_str()
                , visible_parent ? visible_parent->GetFullName().c_str() : "a hidden root"
            );
            return;
        }
    }


    m_current_item = target_item;

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

    m_treeview->setNewCurrentItem(target_item);
    m_treeview->scrollTo(m_treeview->currentIndex(), QAbstractItemView::ScrollHint::EnsureVisible);
    emit currentItemChanged();
    reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "ActivateItem %s", m_current_item->GetFullName());
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
        if (!m_recent_file_entries.empty())
        {
            auto recent_file_widget = dynamic_cast<DmsRecentFileEntry*>(m_recent_file_entries.at(0));
            proj_dir = recent_file_widget->m_cfg_file_path.c_str();
        }
    }

    auto configFileName = QFileDialog::getOpenFileName(this, "Open configuration", proj_dir.c_str(), "*.dms").toLower();

    if (configFileName.isEmpty())
        return;

    if (GetRegStatusFlags() & RSF_EventLog_ClearOnLoad)
        m_eventlog_model->clear();

    LoadConfig(configFileName.toUtf8().data());
}

void MainWindow::reopen()
{
    auto cip = m_current_item_bar->text();
    
    reportF(MsgCategory::commands, SeverityTypeID::ST_MajorTrace, "Reopen configuration");

    if (GetRegStatusFlags() & RSF_EventLog_ClearOnReLoad)
        m_eventlog_model->clear();

    LoadConfig(m_currConfigFileName.c_str(), cip.toUtf8());
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

DmsConfigTextButton::DmsConfigTextButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent)
{

}

void DmsConfigTextButton::paintEvent(QPaintEvent* event)
{
    QStylePainter p(this);
    QStyleOptionButton option;
    initStyleOption(&option);
    option.state |= QStyle::State_MouseOver;
    p.drawControl(QStyle::CE_PushButton, option);
}

DmsRecentFileEntry::DmsRecentFileEntry(size_t index, std::string_view dms_file_full_path, QObject* parent)
    : QAction(parent)
{
    m_cfg_file_path = dms_file_full_path;
    m_index = index;

    std::string preprending_spaces = index < 9 ? "   &" : "  ";
    std::string menu_text = preprending_spaces + std::to_string(index + 1) + ". " + std::string(ConvertDosFileName(SharedStr(dms_file_full_path.data())).c_str());
    setText(menu_text.c_str());
}

void DmsRecentFileEntry::showRecentFileContextMenu(QPoint pos)
{
    auto main_window = MainWindow::TheOne();
    
    std::unique_ptr<QMenu> recent_file_context_menu = std::make_unique<QMenu>();
    std::unique_ptr<QAction> pin_action = std::make_unique<QAction>(QPixmap(":/res/images/TB_toggle_palette.bmp"), "pin", this);
    std::unique_ptr<QAction> remove_action = std::make_unique<QAction>(QPixmap(":/res/images/EL_clear.bmp"), "remove", this);
    pin_action->setDisabled(true);
    recent_file_context_menu->addAction(pin_action.get());
    recent_file_context_menu->addAction(remove_action.get());


    auto active_action = main_window->m_file_menu->activeAction();
    auto dms_recent_file_entry = dynamic_cast<DmsRecentFileEntry*>(active_action);
    if (!dms_recent_file_entry)
        return;

    connect(remove_action.get(), &QAction::triggered, dms_recent_file_entry, &DmsRecentFileEntry::onDeleteRecentFileEntry);
    recent_file_context_menu->exec(pos);
}

bool DmsRecentFileEntry::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) 
    {
        auto mouse_event = dynamic_cast<QMouseEvent*>(event);
        if (mouse_event && mouse_event->button()==Qt::MouseButton::RightButton)
        {
            showRecentFileContextMenu(mouse_event->globalPos());
            return true;
        }
    }

    return QAction::eventFilter(obj, event);
}

bool DmsRecentFileEntry::event(QEvent* e)
{
    return QAction::event(e);
}

void DmsRecentFileEntry::onDeleteRecentFileEntry()
{
    auto main_window = MainWindow::TheOne();
    main_window->m_file_menu->close();
    main_window->removeRecentFileAtIndex(m_index);
}

void DmsRecentFileEntry::onFileEntryPressed()
{
    auto main_window = MainWindow::TheOne();

    if (GetRegStatusFlags() & RSF_EventLog_ClearOnLoad)
        main_window->m_eventlog_model->clear();
    main_window->m_file_menu->close();
    main_window->LoadConfig(m_cfg_file_path.c_str());
    main_window->saveRecentFileActionToRegistry();
}

DmsToolbuttonAction::DmsToolbuttonAction(const ToolButtonID id, const QIcon& icon, const QString& text, QObject* parent, ToolbarButtonData button_data, const ViewStyle vs)
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

    if (id == ToolButtonID::TB_Pan)
        setChecked(true);

    m_data = std::move(button_data);
}

void DmsToolbuttonAction::onToolbuttonPressed()
{
    auto mdi_area = MainWindow::TheOne()->m_mdi_area.get();
    if (!mdi_area)
        return;

    auto subwindow_list = mdi_area->subWindowList(QMdiArea::WindowOrder::StackingOrder);
    if (!subwindow_list.size())
        return;

    auto subwindow_highest_in_z_order = subwindow_list.back();
    if (!subwindow_highest_in_z_order)
        return;

    auto dms_view_area = dynamic_cast<QDmsViewArea*>(subwindow_highest_in_z_order);
    if (!dms_view_area)
        return;

    auto number_of_button_states = getNumberOfStates();
    bool button_is_toggle_type = number_of_button_states == 2;
    bool global_toggle_button_is_currently_active = button_is_toggle_type and m_state and m_data.is_global;

    if (number_of_button_states - 1 == m_state) // state roll over
        m_state = 0;
    else
        m_state++;

    if (m_data.is_global) // ie. zoom-in and zoom-out are mutually exclusive
    {
        auto was_checked = !isChecked();
        if (was_checked && m_data.id == TB_Pan)
        {
            setChecked(true);
            return;
        }

        bool activate_pan_tool = was_checked ? true : false;

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

            if (activate_pan_tool && dms_toolbar_action->m_data.id == TB_Pan)
                dms_toolbar_action->setChecked(true);
            else
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
        if (m_state < m_data.icons.size())
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
    case TB_Export: return { TB_Export, {"Save to file as semicolon delimited text", "Export the viewport data to bitmaps file(s) using the export settings and the current ROI"}, {TB_Export}, {":/res/images/TB_save.bmp"}};
    case TB_TableCopy: return { TB_TableCopy, {"Copy as semicolon delimited text to Clipboard",""}, {TB_TableCopy}, {":/res/images/TB_copy.bmp"}};
    case TB_Copy: return { TB_Copy, {"Copy the visible contents as image to Clipboard","Copy the visible contents of the viewport to the Clipboard"}, {TB_Copy}, {":/res/images/TB_vcopy.bmp"}};
    case TB_CopyLC: return { TB_CopyLC, {"","Copy the full contents of the LayerControlList to the Clipboard"}, {TB_CopyLC}, {":/res/images/TB_copy.bmp"}};
    case TB_ShowFirstSelectedRow: return { TB_ShowFirstSelectedRow, {"Show the first selected row","Make the extent of the selected elements fit in the ViewPort"}, {TB_ShowFirstSelectedRow}, {":/res/images/TB_table_show_first_selected.bmp"} };
    case TB_ZoomSelectedObj: return { TB_ZoomSelectedObj, {"Show the first selected row","Make the extent of the selected elements fit in the ViewPort"}, {TB_ZoomSelectedObj}, {":/res/images/TB_zoom_selected.bmp"}};
    case TB_SelectRows: return { TB_SelectRows, {"Select row(s) by mouse-click (use Shift to add or Ctrl to deselect)",""}, {TB_SelectRows}, {":/res/images/TB_table_select_row.bmp"}};
    case TB_SelectAll: return { TB_SelectAll, {"Select all rows","Select all elements in the active layer"}, {TB_SelectAll}, {":/res/images/TB_select_all.bmp"}};
    case TB_SelectNone: return { TB_SelectNone, {"Deselect all rows","Deselect all elements in the active layer"}, {TB_SelectNone}, {":/res/images/TB_select_none.bmp"}};
    case TB_ShowSelOnlyOn: return { TB_ShowSelOnlyOn, {"Show only selected rows","Show only selected elements"}, {TB_ShowSelOnlyOff, TB_ShowSelOnlyOn}, {":/res/images/TB_show_selected_features.bmp"}};
    case TB_TableGroupBy: return { TB_TableGroupBy, {"Group by the highlighted columns",""}, {TB_Neutral, TB_TableGroupBy}, {":/res/images/TB_group_by.bmp"}};
    case TB_ZoomAllLayers: return { TB_ZoomAllLayers, {"","Make the extents of all layers fit in the ViewPort"}, {TB_ZoomAllLayers}, {":/res/images/TB_zoom_all_layers.bmp"}};
    case TB_ZoomActiveLayer: return { TB_ZoomActiveLayer, {"","Make the extent of the active layer fit in the ViewPort"}, {TB_ZoomActiveLayer}, {":/res/images/TB_zoom_active_layer.bmp"}};
    case TB_ZoomIn2: return { TB_ZoomIn2, {"","Zoom in by drawing a rectangle"}, {TB_Neutral, TB_ZoomIn2}, {":/res/images/TB_zoomin_button.bmp"}, true};
    case TB_ZoomOut2: return { TB_ZoomOut2, {"","Zoom out by clicking on a ViewPort location"}, {TB_Neutral, TB_ZoomOut2}, {":/res/images/TB_zoomout_button.bmp"}, true};
    case TB_Pan: return { TB_Pan, {"","Pan by holding left mouse button down in the ViewPort and dragging"}, {TB_Neutral, TB_Pan}, {":/res/images/TB_pan_button.bmp"}, true };
    case TB_SelectObject: return { TB_SelectObject, {"","Select elements in the active layer by mouse-click(use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectObject}, {":/res/images/TB_select_object.bmp"}, true};
    case TB_SelectRect: return { TB_SelectRect, {"","Select elements in the active layer by drawing a rectangle(use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectRect}, {":/res/images/TB_select_rect.bmp"}, true};
    case TB_SelectCircle: return { TB_SelectCircle, {"","Select elements in the active layer by drawing a circle(use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectCircle}, {":/res/images/TB_select_circle.bmp"}, true};
    case TB_SelectPolygon: return { TB_SelectPolygon, {"","Select elements in the active layer by drawing a polygon(use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectPolygon}, {":/res/images/TB_select_poly.bmp"}, true};
    case TB_SelectDistrict: return { TB_SelectDistrict, {"","Select contiguous regions in the active layer by clicking on them (use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectDistrict}, {":/res/images/TB_select_district.bmp"}, true};
    case TB_Show_VP: return { TB_Show_VP, {"","Toggle the layout of the ViewPort between{MapView only, with LayerControlList, with Overview and LayerControlList}"}, {TB_Show_VPLCOV,TB_Show_VP, TB_Show_VPLC}, {":/res/images/TB_toggle_layout_3.bmp", ":/res/images/TB_toggle_layout_1.bmp", ":/res/images/TB_toggle_layout_2.bmp"}};
    case TB_SP_All: return { TB_SP_All, {"","Toggle Palette Visibiliy between{All, Active Layer Only, None}"}, {TB_SP_All, TB_SP_Active, TB_SP_None}, {":/res/images/TB_toggle_palette.bmp"}};
    case TB_NeedleOn: return { TB_NeedleOn, {"","Show / Hide NeedleController"}, {TB_NeedleOff, TB_NeedleOn}, {":/res/images/TB_toggle_needle.bmp"}};
    case TB_ScaleBarOn: return { TB_ScaleBarOn, {"","Show / Hide ScaleBar"}, {TB_ScaleBarOff, TB_ScaleBarOn}, {":/res/images/TB_toggle_scalebar.bmp"}};
    }

    return {};
}

void MainWindow::scheduleUpdateToolbar()
{
    //ViewStyle current_toolbar_style = ViewStyle::tvsUndefined, requested_toolbar_viewstyle = ViewStyle::tvsUndefined;
    if (m_UpdateToolbarRequestPending || g_IsTerminating)
        return;
/*
    // update requested toolbar style
    QMdiSubWindow* active_mdi_subwindow = m_mdi_area->activeSubWindow();
    QDmsViewArea* dms_active_mdi_subwindow = dynamic_cast<QDmsViewArea*>(active_mdi_subwindow);
    if (!dms_active_mdi_subwindow)
        m_current_toolbar_style = ViewStyle::tvsUndefined;
*/

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
             TB_ZoomAllLayers, TB_ZoomActiveLayer, TB_Pan, TB_ZoomIn2, TB_ZoomOut2, TB_Undefined,
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

/*auto MainWindow::createRecentFilesWidgetAction(int index, std::string_view cfg, QWidget* parent) -> QWidgetAction*
{
    auto new_recent_file_action_widget = new QWidgetAction(parent);
    auto new_recent_file_widget = new DmsRecentFileEntry(index, cfg, parent);
    connect(new_recent_file_action_widget, &QWidgetAction::hovered, new_recent_file_widget, &DmsRecentFileEntry::onHoover);
    new_recent_file_action_widget->setDefaultWidget(new_recent_file_widget);
    return new_recent_file_action_widget;
}*/

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
        clearToolbarUpToDetailPagesTools();

    if (m_tooled_mdi_subwindow == active_mdi_subwindow) // Do nothing
        return;

    auto view_style = ViewStyle::tvsUndefined;

    m_tooled_mdi_subwindow = active_mdi_subwindow;
    auto active_dms_view_area = dynamic_cast<QDmsViewArea*>(active_mdi_subwindow);

    DataView* dv = nullptr;
    if (active_dms_view_area)
    {
        // get viewstyle from dataview
        dv = active_dms_view_area->getDataView();
        if (dv)
            view_style = dv->GetViewType();
    }

    // get viewstyle from property
    if (m_tooled_mdi_subwindow && view_style == ViewStyle::tvsUndefined)
        ViewStyle view_style = static_cast<ViewStyle>(m_tooled_mdi_subwindow->property("viewstyle").value<QVariant>().toInt());

    if (view_style == ViewStyle::tvsUndefined) // No tools for undefined viewstyle
        return;

    if (view_style==m_current_toolbar_style) // Do nothing
        return;

    m_current_toolbar_style = view_style;

    // disable/enable coordinate tool
    auto is_mapview = view_style == ViewStyle::tvsMapView;
    auto is_tableview = view_style == ViewStyle::tvsTableView;
    m_statusbar_coordinates->setVisible(is_mapview || is_tableview);
    clearToolbarUpToDetailPagesTools();

    static std::vector<ToolButtonID> available_table_buttons = getAvailableTableviewButtonIds();
    static std::vector<ToolButtonID> available_map_buttons = getAvailableMapviewButtonIds();

    std::vector < ToolButtonID>* buttons_array_ptr = nullptr;

    switch(view_style)
    {
    case ViewStyle::tvsTableView: buttons_array_ptr = &available_table_buttons; break;
    case ViewStyle::tvsMapView:   buttons_array_ptr = &available_map_buttons; break;
    }
    if (buttons_array_ptr == nullptr)
        return;

    assert(dv);
    auto first_toolbar_detail_pages_action = m_dms_toolbar_spacer_action.get();
    for (auto button_id : *buttons_array_ptr)
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
        auto action = new DmsToolbuttonAction(button_id, button_icon, view_style==ViewStyle::tvsTableView ? button_data.text[0] : button_data.text[1], m_toolbar, button_data, view_style);
        m_current_dms_view_actions.push_back(action);
        auto is_command_enabled = dv->OnCommandEnable(button_id) == CommandStatus::ENABLED;
        if (!is_command_enabled)
            action->setDisabled(true);
        
        m_toolbar->insertAction(first_toolbar_detail_pages_action, action);
        //m_toolbar->addAction(action); // TODO: Possible memory leak, ownership of action not transferred to m_toolbar

        // connections
        connect(action, &DmsToolbuttonAction::triggered, action, &DmsToolbuttonAction::onToolbuttonPressed);
    }
}

bool MainWindow::event(QEvent* event)
{
    if (event->type() == QEvent::WindowActivate && !s_errorWindowActivationCount)
    {
        QTimer::singleShot(0, this, [=]() 
            { 
                auto vos = ReportChangedFiles(); 
                if (vos.CurrPos())
                {
                    auto changed_files = std::string(vos.GetData(), vos.GetDataEnd());
                    m_file_changed_window->setFileChangedMessage(changed_files);

                    // now allow MsgQueue to process GUI events, such TreeView item activation, 
                    // that initiated the WindowActivate, before showing the FileChangedWindow
                    QTimer::singleShot(0, this, [=]()
                        {
                            m_file_changed_window->show();
                        });
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
        const TreeItem* ti = getCurrentTreeItemOrRoot();

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

void MainWindow::openConfigSourceFor(const TreeItem* context)
{
    if (!context)
        return;
    auto filename = ConvertDmsFileNameAlways(context->GetConfigFileName());
    std::string line = std::to_string(context->GetConfigFileLineNr());
    openConfigSourceDirectly(filename.c_str(), line);
}

void MainWindow::openConfigSource()
{
    openConfigSourceFor( getCurrentTreeItemOrRoot() );
}

void MainWindow::openConfigRootSource()
{
    openConfigSourceFor( getRootTreeItem() );
}

TIC_CALL BestItemRef TreeItem_GetErrorSourceCaller(const TreeItem* src);

void MainWindow::stepToFailReason()
{
    auto ti = getCurrentTreeItem();
    if (!ti)
        return;

    if (!WasInFailed(ti))
        return;

    BestItemRef result = TreeItem_GetErrorSourceCaller(ti);
    if (result.first)
    {
        if (result.first->IsCacheItem())
            return;

        setCurrentTreeItem(const_cast<TreeItem*>(result.first.get()));
    }

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
    m_detail_pages->toggle(m_detail_pages->m_last_active_detail_page);
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
    auto optionsWindow = new DmsLocalMachineOptionsWindow(this);
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
       MainWindow::TheOne()->reportErrorAndTryReload(errMsg);
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
        reportErrorAndTryReload(errMsg);
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
        reportErrorAndTryReload(errMsg);
    }
}

void MainWindow::code_analysis_clr_targets()
{
    TreeItem_SetAnalysisSource(nullptr);
//    TreeItem_SetAnalysisTarget(nullptr, true);  // REMOVE, al gedaan door SetAnalysisSource
}

bool MainWindow::reportErrorAndAskToReload(ErrMsgPtr error_message_ptr)
{
    VectorOutStreamBuff buffer;
    auto streamType = OutStreamBase::ST_HTM;
    auto msgOut = std::unique_ptr<OutStreamBase>(XML_OutStream_Create(&buffer, streamType, "", calcRulePropDefPtr));

    *msgOut << *error_message_ptr;
    buffer.WriteByte(0);

    if (s_errorWindowActivationCount)
        return false;
    DynamicIncrementalLock lock(s_errorWindowActivationCount);

    TheOne()->m_error_window->setErrorMessageHtml(buffer.GetData());
    auto dialogResult = TheOne()->m_error_window->exec();
    if (dialogResult == -1) // this appears to be the result when exec is called recursively.
        return false;
    if (dialogResult == QDialog::DialogCode::Rejected)
        return false;
    assert(dialogResult == QDialog::DialogCode::Accepted);
    return true;
}

void MainWindow::reportErrorAndTryReload(ErrMsgPtr error_message_ptr)
{
    bool doReload = reportErrorAndAskToReload(std::move(error_message_ptr));
    if (doReload)
        TheOne()->reopen();
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

        auto dms_view_window_icon = getIconFromViewstyle(viewStyle);
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
    reportF(MsgCategory::commands, SeverityTypeID::ST_MajorTrace, "defaultView // for item %s", m_current_item->GetFullName());
    auto default_view_style = SHV_GetDefaultViewStyle(m_current_item);
    if (default_view_style == ViewStyle::tvsPaletteEdit)
        default_view_style = ViewStyle::tvsTableView;
    if (default_view_style == ViewStyle::tvsDefault)
    {
        reportF(MsgCategory::other, SeverityTypeID::ST_Error, "Unable to deduce viewstyle for item %s, no view created.", m_current_item->GetFullName());
        return;
    }
    createView(default_view_style);
}

void MainWindow::mapView()
{
    reportF(MsgCategory::commands, SeverityTypeID::ST_MajorTrace, "mapview // for item %s", m_current_item->GetFullName());
    createView(ViewStyle::tvsMapView);
}

void MainWindow::tableView()
{
    reportF(MsgCategory::commands, SeverityTypeID::ST_MajorTrace, "tableView // for item %s", m_current_item->GetFullName());
    createView(ViewStyle::tvsTableView);
}

void geoDMSContextMessage(ClientHandle clientHandle, CharPtr msg)
{
    assert(IsMainThread());
    auto dms_main_window = reinterpret_cast<MainWindow*>(clientHandle);
    dms_main_window->setStatusMessage(msg);
    return;
}

bool MainWindow::CloseConfig()
{
    TreeItem_SetAnalysisSource(nullptr); // clears all code-analysis coding

    // close all dms views
    bool has_active_dms_views = false;
    if (m_mdi_area)
    {
        has_active_dms_views = m_mdi_area->subWindowList().size();
        m_mdi_area->closeAllSubWindows();
    }

    // reset all dms tree data
    if (m_root)
    {
        m_detail_pages->leaveThisConfig(); // reset ValueInfo cached results
        m_dms_model->reset();
        m_treeview->reset();

        m_dms_model->setRoot(nullptr);
        m_root->EnableAutoDelete(); // calls SessionData::ReleaseIt(m_root)
        m_root = nullptr;
        m_current_item.reset();
        m_current_item = nullptr;
    }

    // close all active value info windows
    QList<QWidget*> value_info_windows = this->findChildren<QWidget*>(Qt::FindDirectChildrenOnly);
    for (auto* widget : value_info_windows)
    {
        if (dynamic_cast<ValueInfoWindow*>(widget))
            widget->close();
    }

    SessionData::ReleaseCurr();

    scheduleUpdateToolbar();
    return has_active_dms_views;
}

auto configIsInRecentFiles(std::string_view cfg, const std::vector<std::string>& files) -> Int32
{
    std::string cfg_name = cfg.data();
    //auto pos = std::find(files.begin(), files.end(), cfg_name) - files.begin();
    UInt32 pos = 0;
    for (auto& recent_file : files)
    {
        if (recent_file.compare(cfg_name) == 0)
        {
            return pos;
        }
        pos++;
    }
    
    return -1;
    //if (pos >= files.size())
    //    return -1;
    //return pos;
}

void MainWindow::cleanRecentFilesThatDoNotExist()
{
    auto recent_files_from_registry = GetGeoDmsRegKeyMultiString("RecentFiles");

    for (auto it_rf = recent_files_from_registry.begin(); it_rf != recent_files_from_registry.end();)
    {
        if ((strnicmp(it_rf->c_str(), "file:", 5) != 0))
        {
            auto dosFileName = ConvertDmsFileName(SharedStr(it_rf->c_str()));
            if (!std::filesystem::exists(dosFileName.c_str()) || it_rf->empty())
            {
                it_rf = recent_files_from_registry.erase(it_rf);
                continue;
            }
        }
        ++it_rf;
    }
    SetGeoDmsRegKeyMultiString("RecentFiles", recent_files_from_registry);
}

void MainWindow::removeRecentFileAtIndex(size_t index)
{
    if (m_recent_file_entries.size() <= index)
        return;

    auto menu_to_be_removed = m_recent_file_entries.at(index);
    auto rf_action = dynamic_cast<DmsRecentFileEntry*>(menu_to_be_removed);
    if (!rf_action)
        return;

    auto msgTxt = mySSPrintF("Remove %s from the list of recent files ?", rf_action->m_cfg_file_path);
    if (MessageBoxA((HWND)winId(), msgTxt.c_str(), "Confirmation Request", MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        m_file_menu->removeAction(menu_to_be_removed);
        m_recent_file_entries.removeAt(index);
        saveRecentFileActionToRegistry();
    }
}

void MainWindow::saveRecentFileActionToRegistry()
{
    std::vector<std::string> recent_files_as_std_strings;
    for (auto* recent_file_entry_candidate : m_recent_file_entries)
    {
        auto recent_file_entry = dynamic_cast<DmsRecentFileEntry*>(recent_file_entry_candidate);
        if (!recent_file_entry)
            continue;
        auto dms_string = recent_file_entry->m_cfg_file_path;

        recent_files_as_std_strings.push_back(dms_string);
    }
    SetGeoDmsRegKeyMultiString("RecentFiles", recent_files_as_std_strings);
}

void MainWindow::insertCurrentConfigInRecentFiles(std::string_view cfg)
{
    auto cfg_index_in_recent_files = configIsInRecentFiles(cfg, GetGeoDmsRegKeyMultiString("RecentFiles"));
    if (cfg_index_in_recent_files == -1)
        addRecentFilesEntry(cfg);
    else
        m_recent_file_entries.move(cfg_index_in_recent_files, 0);

    saveRecentFileActionToRegistry();
    updateFileMenu();
}

void MainWindow::LoadConfig(CharPtr configFilePath, CharPtr currentItemPath)
{
    bool hadSubWindows = CloseConfig();
    SharedStr configFilePathStr(configFilePath);
    SharedStr currentItemPathStr(currentItemPath);

    QTimer::singleShot(hadSubWindows ? 100 : 0, this, [=]()
        { 
            if (LoadConfigImpl(configFilePathStr.c_str()))
                QTimer::singleShot(0, this, [=]()
                    {
                        m_current_item_bar->setPath(currentItemPathStr.c_str());
                    }
                );
        }
    );
}

bool MainWindow::LoadConfigImpl(CharPtr configFilePath)
{
  retry:
    try {
        auto orgConfigFilePath = SharedStr(configFilePath);
        m_currConfigFileName = ConvertDosFileName(orgConfigFilePath); // replace back-slashes to linux/dms style separators and prefix //SYSTEM/path with 'file:'
        
        auto folderName = splitFullPath(m_currConfigFileName.c_str());
        if (strnicmp(folderName.c_str(), "file:",5) != 0)
            SetCurrentDir(ConvertDmsFileNameAlways(std::move(folderName)).c_str());

        auto newRoot = CreateTreeFromConfiguration(m_currConfigFileName.c_str());

        m_root = newRoot;
        if (m_root)
        {
            SharedStr configFilePathStr = DelimitedConcat(ConvertDosFileName(GetCurrentDir()), ConvertDosFileName(m_currConfigFileName));
            for (auto& ch : configFilePathStr)
                ch = std::tolower(ch);

            insertCurrentConfigInRecentFiles(configFilePathStr.c_str());
            SetGeoDmsRegKeyString("LastConfigFile", configFilePathStr.c_str());

            m_treeview->setItemDelegate(new TreeItemDelegate(m_treeview));
            m_treeview->setModel(m_dms_model.get());
            m_treeview->setRootIndex(m_treeview->rootIndex().parent());
            m_treeview->scrollTo({});
        }
        else
            SessionData::ReleaseCurr();
    }
    catch (...)
    {
        m_dms_model->setRoot(nullptr);
        setCurrentTreeItem(nullptr);
        auto x = catchException(false);
        bool reloadSucceeded = reportErrorAndAskToReload(x);
        if (!reloadSucceeded)
            return false;
        goto retry;
    }
    m_dms_model->setRoot(m_root);
    clearActionsForEmptyCurrentItem();
    //setCurrentTreeItem(m_root); // as an example set current item to root, which emits signal currentItemChanged
    
    updateCaption();
    m_dms_model->reset();
    m_eventlog->scrollToBottomThrottled();
    return true;
}

// TODO: clean-up specification of unused parameters at the calling sites, or do use them.
void MainWindow::OnViewAction(const TreeItem* tiContext, CharPtr sAction, Int32 /*nCode*/, Int32 /*x*/, Int32 /*y*/, bool /*doAddHistory*/, bool /*isUrl*/, bool /*mustOpenDetailsPage*/)
{
    assert(IsMainThread());
    MainWindow::TheOne()->doViewAction(const_cast<TreeItem*>(tiContext), sAction);
}

void MainWindow::showStatisticsDirectly(const TreeItem* tiContext)
{
    if (openErrorOnFailedCurrentItem())
        return;

    if (!IsDataItem(tiContext))
    {
        reportF(MsgCategory::commands, SeverityTypeID::ST_Warning, "Cannot show statistics window for items that are not of type dataitem: [[%s]]", tiContext->GetFullName());
        return;
    }

    auto* mdiSubWindow = new QMdiSubWindow(m_mdi_area.get());
    
    //mdiSubWindow->setWindowFlag(Qt::Window, true);// type windowType
    auto* statistics_browser = new StatisticsBrowser(mdiSubWindow);
    SuspendTrigger::Resume();
    statistics_browser->m_Context = tiContext;
    tiContext->PrepareData();

    /*auto* statistics_browser = new QTextEdit(mdiSubWindow);
    statistics_browser->setText("Test text for DEBUGGING.");*/
    mdiSubWindow->setWidget(statistics_browser);

    SharedStr title = "Statistics of " + SharedStr(tiContext->GetName());
    mdiSubWindow->setWindowTitle(title.c_str());
    mdiSubWindow->setWindowIcon(getIconFromViewstyle(ViewStyle::tvsStatistics));
    m_mdi_area->addSubWindow(mdiSubWindow);
    mdiSubWindow->setAttribute(Qt::WA_DeleteOnClose);
    mdiSubWindow->show();

    statistics_browser->restart_updating();
}

void MainWindow::showValueInfo(const AbstrDataItem* studyObject, SizeT index, SharedStr extraInfo)
{
    assert(studyObject);

    auto* value_info_window = new ValueInfoWindow(studyObject, index, extraInfo, this);
    return;
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
    result = mySSPrintF("%s%s%02d:%02d:%02d "
        , preFix
        , result.c_str()
        , passedTime / 3600, (passedTime / 60) % 60, passedTime % 60
    );
    return result;
}

RTC_CALL auto GetMemoryStatus() -> SharedStr;

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

    auto msgStr = GetMemoryStatus();
    if (ach)
        msgStr = SharedStr(ach->GetDescription()) + " " + msgStr;
    std::get<SharedStr>(current_processing_record) = msgStr;

    auto comparator = [](std::time_t passedTime, const processing_record& rhs) { return passedTime <= passed_time(rhs);  };
    auto insertionPoint = std::upper_bound(m_processing_records.begin(), m_processing_records.end(), passedTime, comparator);
    bool top_of_records_is_changing =( insertionPoint == m_processing_records.end());
    m_processing_records.insert(insertionPoint, current_processing_record);
    if (m_processing_records.size() > 10)
        m_processing_records.erase(m_processing_records.begin());

    if (!top_of_records_is_changing)
        return;

    if (m_calculation_times_window->isVisible())
        update_calculation_times_report();

    assert(passedTime == passed_time(m_processing_records.back()));
 
    m_LongestProcessingRecordTxt = passed_time_str(" - max processing time: ", passedTime);

    updateStatusMessage();
}

RTC_CALL auto GetMemoryStatus() -> SharedStr;

static UInt32 s_ReentrancyCount = 0;
void MainWindow::updateStatusMessage()
{
    if (s_ReentrancyCount)
        return;

    auto fullMsg = m_StatusMsg + m_LongestProcessingRecordTxt + " - " + GetMemoryStatus();

    DynamicIncrementalLock<> incremental_lock(s_ReentrancyCount);
    
    statusBar()->showMessage(fullMsg.c_str());
    //statusBar()->repaint();
}

auto MainWindow::getIconFromViewstyle(ViewStyle viewstyle) -> QIcon
{
    switch (viewstyle)
    {
    case ViewStyle::tvsMapView: { return QPixmap(":/res/images/TV_globe.bmp"); }
    case ViewStyle::tvsStatistics: { return QPixmap(":/res/images/DP_statistics.bmp"); }
    case ViewStyle::tvsCalculationTimes: { return QPixmap(":/res/images/IconCalculationTimeOverview.png"); }
    case ViewStyle::tvsPaletteEdit: { return QPixmap(":/res/images/TV_palette.bmp");}
    case ViewStyle::tvsCurrentConfigFileList: { return QPixmap(":/res/images/ConfigFileList.png"); }
    default: { return QPixmap(":/res/images/TV_table.bmp");}
    }
}

void MainWindow::hideDetailPagesRadioButtonWidgets(bool hide_properties_buttons, bool hide_source_descr_buttons)
{
    m_detail_page_properties_buttons->gridLayoutWidget->setHidden(hide_properties_buttons);
    m_detail_page_source_description_buttons->gridLayoutWidget->setHidden(hide_source_descr_buttons);
}

void MainWindow::addRecentFilesEntry(std::string_view recent_file)
{
    auto index = m_recent_file_entries.size();
    std::string preprending_spaces = index < 9 ? "   &" : "  ";
    std::string pushbutton_text = preprending_spaces + std::to_string(index + 1) + ". " + std::string(ConvertDosFileName(SharedStr(recent_file.data())).c_str());
    DmsRecentFileEntry* new_recent_file_entry = new DmsRecentFileEntry(index, recent_file.data(), this);

    m_file_menu->addAction(new_recent_file_entry);

    for (auto action_object_pointer : new_recent_file_entry->associatedObjects())
    {
       action_object_pointer->installEventFilter(new_recent_file_entry);
    }

    m_recent_file_entries.push_back(new_recent_file_entry);

    // connections
    connect(new_recent_file_entry, &DmsRecentFileEntry::triggered, new_recent_file_entry, &DmsRecentFileEntry::onFileEntryPressed);
    connect(new_recent_file_entry, &DmsRecentFileEntry::toggled, new_recent_file_entry, &DmsRecentFileEntry::onFileEntryPressed);
}

auto Realm(const auto& x) -> CharPtrRange
{
    auto b = begin_ptr(x);
    auto e = end_ptr(x);
    auto colonPos = std::find(b, e, ':');
    if (colonPos == e)
        return {};
    return { b, colonPos };
}

auto getLinkFromErrorMessage(std::string_view error_message, unsigned int lineNumber) -> link_info
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

bool IsPostRequest(const QUrl& /*link*/)
{
    return false;
}

void MainWindow::onInternalLinkClick(const QUrl& link, QWidget* origin)
{
    try {
        auto linkStr = link.toString().toStdString();

        // log link action
#if defined(_DEBUG)
        MsgData data{ SeverityTypeID::ST_MajorTrace, MsgCategory::other, false, GetThreadID(), StreamableDateTime(), SharedStr(linkStr.data()) };
        MainWindow::TheOne()->m_eventlog_model->addText(std::move(data), false);
#endif

        auto* current_item = MainWindow::TheOne()->getCurrentTreeItem();

        auto realm = Realm(linkStr);
        if (realm.size() == 16 && !strnicmp(realm.begin(), "editConfigSource", 16))
        {
            auto clicked_error_link = linkStr.substr(17);
            auto parsed_clicked_error_link = getLinkFromErrorMessage(clicked_error_link);
            MainWindow::TheOne()->openConfigSourceDirectly(parsed_clicked_error_link.filename, parsed_clicked_error_link.line);
            return;
        }
        if (realm.size() == 9 && !strnicmp(realm.begin(), "clipboard", 9))
        {
            auto dataStr = linkStr.substr(10);
            auto decodedStr = UrlDecode(SharedStr(dataStr.c_str()));
            auto qStr = QString(decodedStr.c_str());
            QGuiApplication::clipboard()->clear();
            QGuiApplication::clipboard()->setText(qStr, QClipboard::Clipboard);
            return;
        }

        if (IsPostRequest(link))
        {
            auto queryStr = link.query().toUtf8();
            DMS_ProcessPostData(const_cast<TreeItem*>(current_item), queryStr.data(), queryStr.size());
            return;
        }
        if (!ShowInDetailPage(SharedStr(linkStr.c_str())))
        {
            auto raw_string = SharedStr(begin_ptr(linkStr), end_ptr(linkStr));
            ReplaceSpecificDelimiters(raw_string.GetAsMutableRange(), '\\');
            auto linkCStr = ConvertDosFileName(raw_string); // obtain zero-termination and non-const access
            QDesktopServices::openUrl(QUrl(linkCStr.c_str(), QUrl::TolerantMode));
            return;
        }

        if (linkStr.contains(".adms"))
        {
            auto queryResult = ProcessADMS(current_item, linkStr.data());
            m_detail_pages->setHtml(queryResult.c_str());
            return;
        }
        auto sPrefix = Realm(linkStr);
        if (!strncmp(sPrefix.begin(), "dms", sPrefix.size()))
        {
            auto sAction = CharPtrRange(begin_ptr(linkStr) + 4, end_ptr(linkStr));
            doViewAction(m_current_item, sAction, origin ? origin : nullptr);
            return;
        }
    }
    catch (...)
    {
        auto errMsg = catchAndReportException();
    }
}

void EditPropValue(TreeItem* /*tiContext*/, CharPtrRange /*url*/, SizeT /*recNo*/)
{

}

void PopupTable(SizeT /*recNo*/)
{

}

void MainWindow::doViewAction(TreeItem* tiContext, CharPtrRange sAction, QWidget* origin)
{
    assert(tiContext);
    SuspendTrigger::Resume();

    auto colonPos = std::find(sAction.begin(), sAction.end(), ':');
    auto sMenu = CharPtrRange(sAction.begin(), colonPos);
    auto sPathWithSub = CharPtrRange(colonPos == sAction.end() ? colonPos : colonPos + 1, sAction.end());

    auto queryPos = std::find(sPathWithSub.begin(), sPathWithSub.end(), '?');
    auto sPath = CharPtrRange(sPathWithSub.begin(), queryPos);
    auto sSub = CharPtrRange(queryPos == sPathWithSub.end() ? queryPos : queryPos + 1, sPathWithSub.end());

    //    DMS_TreeItem_RegisterStateChangeNotification(OnTreeItemChanged, m_tiFocus, TClientHandle(self)); // Resource aquisition which must be matched by a call to LooseFocus

    auto separatorPos = std::find(sMenu.begin(), sMenu.end(), '!');
    //    MakeMin(separatorPos, std::find(sMenu.begin(), sMenu.end(), '#')); // TODO: unify syntax to '#' or '!'
    auto sRecNr = CharPtrRange(separatorPos < sMenu.end() ? separatorPos + 1 : separatorPos, sMenu.end());
    sMenu.second = separatorPos;

    SizeT recNo = UNDEFINED_VALUE(SizeT);
    if (!sRecNr.empty())
        AssignValueFromCharPtrs(recNo, sRecNr.begin(), sRecNr.end());

    if (!strncmp(sMenu.begin(), "popuptable", sMenu.size()))
    {
        PopupTable(recNo);
        return;
    }

    if (!strncmp(sMenu.begin(), "edit", sMenu.size()))
    {
        EditPropValue(tiContext, sPathWithSub, recNo);
        return;
    }

    if (!strncmp(sMenu.begin(), "goto", sMenu.size()))
    {
        tiContext = const_cast<TreeItem*>(tiContext->FindBestItem(sPath).first.get()); // TODO: make result FindBestItem non-const
        if (tiContext)
            MainWindow::TheOne()->setCurrentTreeItem(tiContext);
        return;
    }

    // Value info link clicked from value info window
    tiContext = const_cast<TreeItem*>(tiContext->FindBestItem(sPath).first.get()); // TODO: make result FindBestItem non-const
    if (origin)
    {
        auto value_info_browser = dynamic_cast<ValueInfoBrowser*>(origin);
        if (value_info_browser)
        {
            value_info_browser->addStudyObject(AsDataItem(tiContext), recNo, SharedStr(sSub));
            return;
        }
    }

    // Detail- or new ValueinfoWindow requested
    if (sMenu.size() >= 3 && !strncmp(sMenu.begin(), "dp.", 3))
    {
        sMenu.first += 3;

        auto detail_page_type = m_detail_pages->activeDetailPageFromName(sMenu);
        switch (detail_page_type)
        {
        case ActiveDetailPage::STATISTICS:
        {
            MainWindow::TheOne()->showStatisticsDirectly(tiContext);
            return;
        }
        case ActiveDetailPage::VALUE_INFO:
        {
            if (!IsDataItem(tiContext))
                return;

            return MainWindow::TheOne()->showValueInfo(AsDataItem(tiContext), recNo, SharedStr(sSub));
        }
        default:
        {
            m_detail_pages->m_active_detail_page = detail_page_type;
            if (tiContext)
                MainWindow::TheOne()->setCurrentTreeItem(tiContext);
            m_detail_pages->scheduleDrawPageImpl(100);
            return;
        }
        }
    }
}

static bool s_TreeViewRefreshPending = false;

void AnyTreeItemStateHasChanged(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode)
{
    auto mainWindow = reinterpret_cast<MainWindow*>(clientHandle);
    switch (notificationCode) {
    case NC_Deleting:
        // TODO: remove self from any representation to avoid accessing it's dangling pointer
        break;
    case NC_Creating:
        break;
    case CC_CreateMdiChild:
    {
        assert(IsMainThread());
        auto* createStruct = const_cast<MdiCreateStruct*>(reinterpret_cast<const MdiCreateStruct*>(self));
        assert(createStruct);
        new QDmsViewArea(mainWindow->m_mdi_area.get(), createStruct);
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

    // this actually only invalidates any drawn area and causes repaint later, but time anyway to avoid too many repaints
    if (!s_TreeViewRefreshPending)
    {
        s_TreeViewRefreshPending = true;
        QTimer::singleShot(1000, []() 
            { 
                // MainWindow could have been destroyed
                if (!s_CurrMainWindow)
                    return;

                s_TreeViewRefreshPending = false;
                auto tv = s_CurrMainWindow->m_treeview;
                if (tv)
                    tv->update();
            }
        );
    }

    if (s_CurrMainWindow && self == s_CurrMainWindow->getCurrentTreeItem())
        s_CurrMainWindow->m_detail_pages->onTreeItemStateChange();
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
    m_treeitem_visit_history->setStyleSheet("QComboBox QAbstractItemView {\n"
                                                "min-width:400px;"
                                            "}\n"
                                            "QComboBox::drop-down:button{\n"
                                                "background-color: transparant;\n"
                                            "}\n"
                                            "QComboBox::down-arrow {\n"
                                                "image: url(:/res/images/arrow_down.png);\n"
                                            "}\n");
   

    m_current_item_bar_container->addWidget(m_treeitem_visit_history.get());
    m_current_item_bar = std::make_unique<DmsCurrentItemBar>(this);
    m_current_item_bar_container->addWidget(m_current_item_bar.get());
    m_current_item_bar_container->addAction(m_back_action.get());
    m_current_item_bar_container->addAction(m_forward_action.get());

    connect(m_current_item_bar.get(), &DmsCurrentItemBar::editingFinished, m_current_item_bar.get(), &DmsCurrentItemBar::onEditingFinished);
    connect(m_treeitem_visit_history.get(), &QComboBox::currentTextChanged, m_current_item_bar.get(), &DmsCurrentItemBar::setPathDirectly);

    addToolBarBreak();

    connect(m_mdi_area.get(), &QDmsMdiArea::subWindowActivated, this, &MainWindow::scheduleUpdateToolbar);
    //auto openIcon = QIcon::fromTheme("document-open", QIcon(":res/images/open.png"));
    auto fileOpenAct = new QAction(tr("&Open Configuration"), this);
    fileOpenAct->setShortcuts(QKeySequence::Open);
    fileOpenAct->setStatusTip(tr("Open an existing configuration file"));
    connect(fileOpenAct, &QAction::triggered, this, &MainWindow::fileOpen);
    m_file_menu->addAction(fileOpenAct);

    auto reOpenAct = new QAction(tr("&Reopen current Configuration"), this);
    reOpenAct->setShortcut(QKeySequence(tr("Alt+R")));
    reOpenAct->setStatusTip(tr("Reopen the current configuration and reactivate the current active item"));
    connect(reOpenAct, &QAction::triggered, this, &MainWindow::reopen);
    m_file_menu->addAction(reOpenAct);

    m_quit_action = std::make_unique<QAction>(tr("&Quit"));
    connect(m_quit_action.get(), &QAction::triggered, qApp, &QCoreApplication::quit);
    m_file_menu->addAction(m_quit_action.get());
    m_quit_action->setShortcuts(QKeySequence::Quit);
    m_quit_action->setStatusTip(tr("Quit the application"));

    m_file_menu->addSeparator();

    connect(m_file_menu.get(), &QMenu::aboutToShow, this, &MainWindow::updateFileMenu);
    updateFileMenu(); //this will be done again when the it's about showtime, but we already need the list of recent_file_actions fn case we insert or refresh one

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
    m_edit_config_source_action = std::make_unique<QAction>(tr("&Open in Editor"));
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

    m_defaultview_action = std::make_unique<QAction>(tr("Default"));
    m_defaultview_action->setIcon(QIcon::fromTheme("backward", QIcon(":/res/images/TV_default_view.bmp")));
    m_defaultview_action->setStatusTip(tr("Open current selected TreeItem's default view."));
    connect(m_defaultview_action.get(), &QAction::triggered, this, &MainWindow::defaultView);
    m_view_menu->addAction(m_defaultview_action.get());
    m_defaultview_action->setShortcut(QKeySequence(tr("Ctrl+Alt+D")));

    // table view
    m_tableview_action = std::make_unique<QAction>(tr("&Table"));
    m_tableview_action->setStatusTip(tr("Open current selected TreeItem's in a table view."));
    m_tableview_action->setIcon(QIcon::fromTheme("backward", QIcon(":/res/images/TV_table.bmp")));
    connect(m_tableview_action.get(), &QAction::triggered, this, &MainWindow::tableView);
    m_view_menu->addAction(m_tableview_action.get());
    m_tableview_action->setShortcut(QKeySequence(tr("Ctrl+D")));

    // map view
    m_mapview_action = std::make_unique<QAction>(tr("&Map"));
    m_mapview_action->setStatusTip(tr("Open current selected TreeItem's in a map view."));
    m_mapview_action->setIcon(QIcon::fromTheme("backward", QIcon(":/res/images/TV_globe.bmp")));
    connect(m_mapview_action.get(), &QAction::triggered, this, &MainWindow::mapView);
    m_view_menu->addAction(m_mapview_action.get());
    m_mapview_action->setShortcut(QKeySequence(tr("CTRL+M")));

    // statistics view
    m_statistics_action = std::make_unique<QAction>(tr("&Statistics"));
    m_statistics_action->setIcon(QIcon::fromTheme("backward", QIcon(":/res/images/DP_statistics.bmp")));
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
    m_view_calculation_times_action->setIcon(getIconFromViewstyle(ViewStyle::tvsCalculationTimes));
    connect(m_view_calculation_times_action.get(), &QAction::triggered, this, &MainWindow::view_calculation_times);
    m_view_menu->addAction(m_view_calculation_times_action.get());

    m_view_current_config_filelist = std::make_unique<QAction>(tr("List of loaded Configuration Files"));
    m_view_current_config_filelist->setIcon(getIconFromViewstyle(ViewStyle::tvsCurrentConfigFileList));// QPixmap(":/res/images/IconCalculationTimeOverview.png"));
    connect(m_view_current_config_filelist.get(), &QAction::triggered, this, &MainWindow::view_current_config_filelist);
    m_view_menu->addAction(m_view_current_config_filelist.get());

    m_view_menu->addSeparator();
    m_toggle_treeview_action       = std::make_unique<QAction>(tr("Toggle TreeView"));
    m_toggle_detailpage_action     = std::make_unique<QAction>(tr("Toggle DetailPages area"));
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

    m_code_analysis_set_source_action = std::make_unique<QAction>(tr("set source"));
    connect(m_code_analysis_set_source_action.get(), &QAction::triggered, this, &MainWindow::code_analysis_set_source);
    m_code_analysis_set_source_action->setShortcut(QKeySequence(tr("Alt+K")));
    m_code_analysis_set_source_action->setShortcutContext(Qt::ApplicationShortcut);
//    this->addAction(m_code_analysis_set_source_action.get());

    m_code_analysis_set_target_action = std::make_unique<QAction>(tr("set target"));
    connect(m_code_analysis_set_target_action.get(), &QAction::triggered, this, &MainWindow::code_analysis_set_target);
    m_code_analysis_set_target_action->setShortcut(QKeySequence(tr("Alt+B")));
    m_code_analysis_set_target_action->setShortcutContext(Qt::ApplicationShortcut);

    m_code_analysis_add_target_action = std::make_unique<QAction>(tr("add target"));
    connect(m_code_analysis_add_target_action.get(), &QAction::triggered, this, &MainWindow::code_analysis_add_target);
    m_code_analysis_add_target_action->setShortcut(QKeySequence(tr("Alt+N")));
    m_code_analysis_add_target_action->setShortcutContext(Qt::ApplicationShortcut);

    m_code_analysis_clr_targets_action = std::make_unique<QAction>(tr("clear target")); 
    connect(m_code_analysis_clr_targets_action.get(), &QAction::triggered, this, &MainWindow::code_analysis_clr_targets);
    m_code_analysis_submenu = CreateCodeAnalysisSubMenu(m_tools_menu.get());

    m_eventlog_filter_toggle = std::make_unique<QAction>(tr("Eventlog filter"));
    connect(m_eventlog_filter_toggle.get(), &QAction::triggered, m_eventlog->m_event_filter_toggle.get(), &QCheckBox::click);
    m_tools_menu->addAction(m_eventlog_filter_toggle.get());

    m_open_root_config_file_action = std::make_unique<QAction>(tr("Open the root configuration file"));
    connect(m_open_root_config_file_action.get(), &QAction::triggered, this, &MainWindow::openConfigRootSource);
    m_tools_menu->addAction(m_open_root_config_file_action.get());

    m_expand_all_action = std::make_unique<QAction>(tr("Expand all items in the TreeView"));
    connect(m_expand_all_action.get(), &QAction::triggered, this, &MainWindow::expandAll);
    m_tools_menu->addAction(m_expand_all_action.get());

    // debug tools
#ifdef MG_DEBUG
    m_save_value_info_pages = std::make_unique<QAction>(tr("Debug: save value info page(s)"));
    connect(m_save_value_info_pages.get(), &QAction::triggered, this, &MainWindow::saveValueInfo);
    m_tools_menu->addAction(m_save_value_info_pages.get());

    m_debug_reports = std::make_unique<QAction>(tr("Debug: produce internal report(s)"));
    connect(m_debug_reports.get(), &QAction::triggered, this, &MainWindow::debugReports);
    m_tools_menu->addAction(m_debug_reports.get());
#endif

    // settings menu
    m_settings_menu = std::make_unique<QMenu>(tr("&Settings"));
    menuBar()->addMenu(m_settings_menu.get());
    connect(m_tools_menu.get(), &QMenu::aboutToShow, this, &MainWindow::updateSettingsMenu);

    m_gui_options_action = std::make_unique<QAction>(tr("&Gui Options"));
    connect(m_gui_options_action.get(), &QAction::triggered, this, &MainWindow::gui_options);
    m_settings_menu->addAction(m_gui_options_action.get());

    m_advanced_options_action = std::make_unique<QAction>(tr("&Local machine Options"));
    connect(m_advanced_options_action.get(), &QAction::triggered, this, &MainWindow::advanced_options); //TODO: change advanced options refs in local machine options
    m_settings_menu->addAction(m_advanced_options_action.get());

    m_config_options_action = std::make_unique<QAction>(tr("&Config Options"));
    connect(m_config_options_action.get(), &QAction::triggered, this, &MainWindow::config_options);
    m_settings_menu->addAction(m_config_options_action.get());

    // window menu
    m_window_menu = std::make_unique<QMenu>(tr("&Window"));
    menuBar()->addMenu(m_window_menu.get());

    m_win_tile_action = std::make_unique<QAction>(tr("&Tile Windows"), m_window_menu.get());
    m_win_tile_action->setShortcut(QKeySequence(tr("Ctrl+Alt+W")));
    m_win_tile_action->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_win_tile_action.get(), &QAction::triggered, m_mdi_area.get(), &QDmsMdiArea::onTileSubWindows);

    m_win_cascade_action = std::make_unique<QAction>(tr("Ca&scade"), m_window_menu.get());
    m_win_cascade_action->setShortcut(QKeySequence(tr("Shift+Ctrl+W")));
    m_win_cascade_action->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_win_cascade_action.get(), &QAction::triggered, m_mdi_area.get(), &QDmsMdiArea::onCascadeSubWindows);

    m_win_close_action = std::make_unique<QAction>(tr("&Close"), m_window_menu.get());
    m_win_close_action->setShortcut(QKeySequence(tr("Ctrl+W")));
    connect(m_win_close_action.get(), &QAction::triggered, m_mdi_area.get(), &QDmsMdiArea::closeActiveSubWindow);
    m_mdi_area->getTabBar()->addAction(MainWindow::TheOne()->m_win_close_action.get());
    //connect(m_win_close_action.get(), &QAction::triggered, m_mdi_area.get(), &QDmsMdiArea::testCloseSubWindow);

    
    //connect(m_win_close_action.get(), &QAction::triggered, m_mdi_area, &QDmsMdiArea::testCloseSubWindow);

    m_win_close_all_action = std::make_unique<QAction>(tr("Close &All"), m_window_menu.get());
    m_win_close_all_action->setShortcut(QKeySequence(tr("Ctrl+L")));
    m_win_close_all_action->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_win_close_all_action.get(), &QAction::triggered, m_mdi_area.get(), &QDmsMdiArea::closeAllSubWindows);

    m_win_close_but_this_action = std::make_unique<QAction>(tr("Close All &But This"), m_window_menu.get());
    m_win_close_but_this_action->setShortcut(QKeySequence(tr("Ctrl+B")));
    m_win_close_but_this_action->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_win_close_but_this_action.get(), &QAction::triggered, m_mdi_area.get(), &QDmsMdiArea::closeAllButActiveSubWindow);
    m_window_menu->addAction(m_win_tile_action.get());
    m_window_menu->addAction(m_win_cascade_action.get());
    m_window_menu->addAction(m_win_close_action.get());
    m_window_menu->addAction(m_win_close_all_action.get());
    m_window_menu->addAction(m_win_close_but_this_action.get());
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
    for (auto recent_file_entry : m_recent_file_entries)
    {
        m_file_menu->removeAction(recent_file_entry);
        delete recent_file_entry;
    }
    m_recent_file_entries.clear();
    cleanRecentFilesThatDoNotExist();
    auto recent_files_from_registry = GetGeoDmsRegKeyMultiString("RecentFiles");
    for (std::string_view recent_file : recent_files_from_registry)
    {
        addRecentFilesEntry(recent_file);

    }
}

void MainWindow::updateViewMenu()
{
    // disable actions not applicable to current item
    auto ti_is_or_is_in_template = !m_current_item || (m_current_item->InTemplate() && m_current_item->IsTemplate());
    m_update_treeitem_action->setDisabled(ti_is_or_is_in_template);
    m_update_subtree_action->setDisabled(ti_is_or_is_in_template);
    m_invalidate_action->setDisabled(ti_is_or_is_in_template);
    m_defaultview_action->setDisabled(ti_is_or_is_in_template);
    m_tableview_action->setDisabled(ti_is_or_is_in_template);
    m_mapview_action->setDisabled(ti_is_or_is_in_template);
    m_statistics_action->setDisabled(ti_is_or_is_in_template);
    m_expand_all_action->setEnabled(m_root.has_ptr());
    m_open_root_config_file_action->setEnabled(m_root.has_ptr());

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
}

void MainWindow::updateSettingsMenu()
{
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

    m_window_menu->addSeparator();

    // reinsert window actions
    int subWindowCount = 0;
    auto asw = m_mdi_area->currentSubWindow();
    for (auto* sw : m_mdi_area->subWindowList())
    {
        auto qa = new QAction(sw->windowTitle(), m_window_menu.get());
        ViewStyle viewstyle = static_cast<ViewStyle>(sw->property("viewstyle").value<QVariant>().toInt());
        qa->setIcon(getIconFromViewstyle(viewstyle));               
        connect(qa, &QAction::triggered, sw, [this, sw] { this->m_mdi_area->setActiveSubWindow(sw); });
        subWindowCount++;
        if (sw == asw)
        {
            qa->setCheckable(true);
            qa->setChecked(true);
        }
        m_window_menu->addAction(qa);
    }

    m_win_tile_action->setEnabled(subWindowCount > 0);
    m_win_cascade_action->setEnabled(subWindowCount > 0); 
    m_win_close_action->setEnabled(subWindowCount > 0);
    m_win_close_all_action->setEnabled(subWindowCount > 0);
    m_win_close_but_this_action->setEnabled(subWindowCount > 1);
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

void MainWindow::updateTracelogHandle()
{
    if (GetRegStatusFlags() & RSF_TraceLogFile)
    {
        if (m_TraceLogHandle)
            return;
        auto fileName = AbstrStorageManager::Expand("", "%LocalDataDir%/trace.log");
        m_TraceLogHandle = std::make_unique<CDebugLog>(fileName);
    }
    else
        m_TraceLogHandle.reset();
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
    
    connect(statusBar(), &QStatusBar::messageChanged, this, &MainWindow::on_status_msg_changed);
    m_statusbar_coordinates = new QLineEdit(this);
    m_statusbar_coordinates->setReadOnly(true);
    m_statusbar_coordinates->setFixedWidth(300);
    m_statusbar_coordinates->setAlignment(Qt::AlignmentFlag::AlignLeft);
    statusBar()->insertPermanentWidget(0, m_statusbar_coordinates);
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

void MainWindow::update_calculation_times_report()
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
    vosb.WriteByte(char(0)); // ends

    if (!m_mdi_area->subWindowList().contains(m_calculation_times_window.get()))
        m_mdi_area->addSubWindow(m_calculation_times_window.get());

    m_calculation_times_browser->setText(vosb.GetData());
}

void MainWindow::view_calculation_times()
{
    update_calculation_times_report();
    //m_calculation_times_window->setAttribute(Qt::WA_DeleteOnClose);

    m_calculation_times_browser->show();
    m_calculation_times_window->setWindowTitle("Calculation time overview");
    m_calculation_times_window->setWindowIcon(QPixmap(":/res/images/IconCalculationTimeOverview.png"));
    m_calculation_times_window->show();
}

void MainWindow::view_current_config_filelist()
{
    VectorOutStreamBuff vosb;
    {
        auto xmlOut = OutStream_HTM(&vosb, "html", nullptr);
        //    outStreamBuff << "List of currently loaded configuration (*.dms) files\n";
        ReportCurrentConfigFileList(xmlOut);
    }
    vosb.WriteByte(char(0)); // ends

    auto viewstyle = ViewStyle::tvsCurrentConfigFileList;
    auto* mdiSubWindow = new QMdiSubWindow(m_mdi_area.get());
    mdiSubWindow->setProperty("viewstyle", viewstyle);
    auto* textWidget = new QTextBrowser(mdiSubWindow);
    mdiSubWindow->setWidget(textWidget);
    textWidget->setHtml(vosb.GetData());

    mdiSubWindow->setWindowTitle("List of currently loaded configuration (*.dms) files");
    mdiSubWindow->setWindowIcon(getIconFromViewstyle(viewstyle));
    m_mdi_area->addSubWindow(mdiSubWindow);
    mdiSubWindow->setAttribute(Qt::WA_DeleteOnClose);
    mdiSubWindow->show();
}

void MainWindow::expandAll()
{
    FixedContextHandle context("expandAll");
    Waiter waitReporter(&context);
    reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "expandAll");

    m_treeview->expandAll();
}

#include "dbg/DebugReporter.h"

void MainWindow::debugReports()
{

#if defined(MG_DEBUGREPORTER)
    DebugReporter::ReportAll();
#endif defined(MG_DEBUGREPORTER)

}



void MainWindow::expandActiveNode(bool doExpand)
{
    FixedContextHandle context("expandActiveNode");
    Waiter waitReporter(&context);
    reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "expand");

    m_treeview->expandActiveNode(doExpand);
}

void MainWindow::expandRecursiveFromCurrentItem()
{
    FixedContextHandle context("expandRecursiveFromCurrentItem");
    Waiter waitReporter(&context);
    reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "expandRecursiveFromCurrentItem");

    m_treeview->expandRecursiveFromCurrentItem();
}


void MainWindow::createDetailPagesDock()
{
    m_detailpages_dock = new QDockWidget(QObject::tr("DetailPages"), this);
    m_detailpages_dock->setTitleBarWidget(new QWidget(m_detailpages_dock));

    auto detail_pages_holder = new QWidget(this);
    auto vertical_layout = new QVBoxLayout(this);
    m_detail_page_properties_buttons = std::make_unique<Ui::dp_properties>();
    m_detail_page_properties_buttons->setupUi(this);
    m_detail_page_source_description_buttons = std::make_unique<Ui::dp_sourcedescription>();
    m_detail_page_source_description_buttons->setupUi(this);

    m_detail_pages = new DmsDetailPages(m_detailpages_dock);
    
    vertical_layout->addWidget(m_detail_page_properties_buttons->gridLayoutWidget);
    vertical_layout->addWidget(m_detail_page_source_description_buttons->gridLayoutWidget);

    // connect detail pages buttons
    connect(m_detail_page_properties_buttons->dp_properties_option, &QButtonGroup::buttonToggled, m_detail_pages, &DmsDetailPages::propertiesButtonToggled);
    connect(m_detail_page_source_description_buttons->dp_sourcedescription_option, &QButtonGroup::buttonToggled, m_detail_pages, &DmsDetailPages::sourceDescriptionButtonToggled);
    hideDetailPagesRadioButtonWidgets(true, true);

    vertical_layout->addWidget(m_detail_pages.get());
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    detail_pages_holder->setLayout(vertical_layout);
    m_detailpages_dock->setWidget(detail_pages_holder);

    addDockWidget(Qt::RightDockWidgetArea, m_detailpages_dock);
    //resizeDocks({ m_treeview_dock, m_detailpages_dock }, { 600, 300 }, Qt::Horizontal);
}

void MainWindow::createDmsHelperWindowDocks()
{
    createDetailPagesDock();

    m_treeview = createTreeview(this);
    m_eventlog = createEventLog(this);

    // resize docks
    resizeDocks({ m_treeview_dock, m_detailpages_dock }, { 300, 300 }, Qt::Horizontal);
    resizeDocks({ m_eventlog_dock }, { 150 }, Qt::Vertical);
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
    auto found_treeitem = best_item_ref.first.get();

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
    auto found_treeitem = best_item_ref.first.get();

    if (found_treeitem == m_current_item)
        return;

    MainWindow::TheOne()->setCurrentTreeItem(const_cast<TreeItem*>(found_treeitem), false);
}

#include "VersionComponent.h"

static VersionComponent notoSansFont("application font: (derived from) NotoSans-Medium.ttf (c) Noto project");