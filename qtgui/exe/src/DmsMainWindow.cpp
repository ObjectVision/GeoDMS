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
#include "DmsFileChangedWindow.h"
#include "DmsActions.h"
#include "DmsToolbar.h"
#include "DmsErrorWindow.h"
#include "DmsAddressBar.h"
#include "StatisticsBrowser.h"

#include "DataView.h"
#include "StateChangeNotification.h"
#include "stg/AbstrStorageManager.h"
#include <regex>

#include "dataview.h"

static MainWindow* s_CurrMainWindow = nullptr;
UInt32 s_errorWindowActivationCount = 0;


bool MainWindow::ShowInDetailPage(SharedStr x) {
    auto realm = Realm(x);
    if (realm.size() == 3 && !strncmp(realm.begin(), "dms", 3))
        return true;
    if (!realm.empty() && realm.size() > 1)
        return false;
    CharPtrRange knownSuffix(".adms");
    return std::search(x.begin(), x.end(), knownSuffix.begin(), knownSuffix.end()) != x.end();
}

void MainWindow::SaveValueInfoImpl(CharPtr filename) {
    auto dmsFileName = ConvertDosFileName(SharedStr(filename));
    auto expandedFilename = AbstrStorageManager::Expand(m_current_item, dmsFileName);
    FileOutStreamBuff buff(SharedStr(expandedFilename), nullptr, true, false);
    for (auto& child_object : children()) {
        auto value_info_window_candidate = dynamic_cast<ValueInfoWindow*>(child_object);
        if (!value_info_window_candidate)
            continue;

        // TODO: implement this for qwebengineview
        //auto htmlSource = value_info_window_candidate->m_browser->toHtml();
        //auto htmlsourceAsUtf8 = htmlSource.toUtf8();
        //buff.WriteBytes(htmlsourceAsUtf8.data(), htmlsourceAsUtf8.size());
    }
}

void MainWindow::PostAppOper(std::function<void()>&& func)
{
    m_AppOperQueue.Post(std::move(func));
    RequestMainThreadOperProcessing();
}

void MainWindow::ProcessAppOpers()
{
    assert(IsMainThread());
    ConfirmMainThreadOperProcessing();
    m_AppOperQueue.Process();
}

void MainWindow::saveValueInfo() {
    SaveValueInfoImpl("C:/LocalData/test/test_value_info.txt");
}

CalculationTimesWindow::CalculationTimesWindow()
:   QMdiSubWindow::QMdiSubWindow() {
    setAttribute(Qt::WA_DeleteOnClose, false);
    setProperty("viewstyle", ViewStyle::tvsCalculationTimes);
    installEventFilter(this);
}

CalculationTimesWindow::~CalculationTimesWindow() { }

bool CalculationTimesWindow::eventFilter(QObject* obj, QEvent* e) {
    switch (e->type()) {
    case QEvent::Close: {
        MainWindow::TheOne()->m_mdi_area->removeSubWindow(MainWindow::TheOne()->m_calculation_times_window.get());
        auto dms_mdi_subwindow_list = MainWindow::TheOne()->m_mdi_area->subWindowList();
        if (!dms_mdi_subwindow_list.empty()) {
            MainWindow::TheOne()->m_mdi_area->setActiveSubWindow(dms_mdi_subwindow_list[0]);
            dms_mdi_subwindow_list[0]->showMaximized();
        }
        break;
    }
    }
    return QObject::eventFilter(obj, e);
}

MainWindow::MainWindow(CmdLineSetttings& cmdLineSettings) { 
    assert(s_CurrMainWindow == nullptr);
    s_CurrMainWindow = this;

    m_mdi_area = new QDmsMdiArea(this);
    m_mdi_area->setFocusPolicy(Qt::FocusPolicy::ClickFocus);
    connect(qApp, &QApplication::focusChanged, this, &MainWindow::onFocusChanged);

    // fonts
    int id = QFontDatabase::addApplicationFont(dms_params::dms_font_resource);
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    QFont dms_text_font(family, dms_params::default_font_size);
    QApplication::setFont(dms_text_font);
    QFontDatabase::addApplicationFont(dms_params::remix_icon_font_resource);

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

    // file menu
    m_file_menu = std::make_unique<QMenu>(tr("&File"));
    menuBar()->addMenu(m_file_menu.get());
    m_address_bar_container = addToolBar(tr("Current item bar"));
    m_treeitem_visit_history = std::make_unique<QComboBox>();
    m_treeitem_visit_history->setFixedSize(dms_params::treeitem_visit_history_fixed_size);
    m_treeitem_visit_history->setFrame(false);
    m_treeitem_visit_history->setStyleSheet(dms_params::stylesheet_treeitem_visit_history);
    m_treeitem_visit_history->setFocusPolicy(Qt::FocusPolicy::ClickFocus);
    connect(m_treeitem_visit_history.get(), &QComboBox::currentTextChanged, m_address_bar.get(), &DmsAddressBar::setPathDirectly);

    // address bar
    m_address_bar_container->addWidget(m_treeitem_visit_history.get());
    m_address_bar = std::make_unique<DmsAddressBar>(this);
    m_address_bar_container->addWidget(m_address_bar.get());
    m_address_bar_container->addAction(m_back_action.get());
    m_address_bar_container->addAction(m_forward_action.get());
    connect(m_address_bar.get(), &DmsAddressBar::editingFinished, m_address_bar.get(), &DmsAddressBar::onEditingFinished);
    addToolBarBreak();

    // schedule update toolbar
    connect(m_mdi_area.get(), &QDmsMdiArea::subWindowActivated, this, &MainWindow::scheduleUpdateToolbar);
    
    // actions
    createDmsActions();

    // read initial last config file
    if (!cmdLineSettings.m_NoConfig) {
        CharPtr currentItemPath = "";
        if (cmdLineSettings.m_ConfigFileName.empty())
            cmdLineSettings.m_ConfigFileName = GetGeoDmsRegKey(dms_params::reg_key_LastConfigFile);
        else {
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

    setStyleSheet(dms_params::stylesheet_main_window);
    setFont(QApplication::font());
    auto menu_bar = menuBar();
    menu_bar->setFont(QApplication::font());

    setTabOrder({ m_address_bar.get(), m_treeview, m_eventlog.get() });
}

MainWindow::~MainWindow() {
    g_IsTerminating = true;

    assert(s_CurrMainWindow == this);
    s_CurrMainWindow = nullptr;

    cleanupDmsCallbacks();
}

bool MainWindow::IsExisting() {
    return s_CurrMainWindow;
}

auto MainWindow::CreateCodeAnalysisSubMenu(QMenu* menu) const -> std::unique_ptr<QMenu> {
    auto code_analysis_submenu = std::make_unique<QMenu>("&Code analysis...");
    menu->addMenu(code_analysis_submenu.get());

    code_analysis_submenu->addAction(m_code_analysis_set_source_action.get());
    code_analysis_submenu->addAction(m_code_analysis_set_target_action.get());
    code_analysis_submenu->addAction(m_code_analysis_add_target_action.get());
    code_analysis_submenu->addAction(m_code_analysis_clr_targets_action.get());
    return code_analysis_submenu;
}

MainWindow* MainWindow::TheOne() {
    return s_CurrMainWindow;
}

bool MainWindow::openErrorOnFailedCurrentItem() {
    auto currItem = getCurrentTreeItem();
    if (currItem->WasFailed() || currItem->IsFailed()) {
        auto fail_reason = currItem->GetFailReason();
        reportErrorAndTryReload(fail_reason);
        return true;
    }
    return false;
}

void MainWindow::clearActionsForEmptyCurrentItem() const {
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

void MainWindow::updateActionsForNewCurrentItem() {
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
    
    try {
        if (ci && !FindURL(ci).empty())
            m_metainfo_page_action->setEnabled(true);
        else
            m_metainfo_page_action->setDisabled(true);
    }
    catch (...) {
        m_metainfo_page_action->setEnabled(true); 
    }
}

void MainWindow::updateTreeItemVisitHistory() const {
    auto current_index = m_treeitem_visit_history->currentIndex();
    if (current_index < m_treeitem_visit_history->count()-1) { // current index is not at the end, remove all forward items 
        for (int i=m_treeitem_visit_history->count() - 1; i > current_index; i--)
            m_treeitem_visit_history->removeItem(i);
    }

    m_treeitem_visit_history->addItem(m_current_item->GetFullName().c_str());
    m_treeitem_visit_history->setCurrentIndex(m_treeitem_visit_history->count()-1);
}

void MainWindow::setCurrentTreeItem(TreeItem* target_item, bool update_history) {
    if (m_current_item == target_item)
        return;

    if (!target_item)
        return;

    MG_CHECK(!m_root || !target_item || isAncestor(m_root, target_item));
    if (target_item && !m_dms_model->show_hidden_items) {
        if (target_item->GetTSF(TSF_InHidden)) {
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

    if (m_address_bar) {
        if (m_current_item)
            m_address_bar->setText(m_current_item->GetFullName().c_str());
        else
            m_address_bar->setText("");
    }

    if (update_history)
        updateTreeItemVisitHistory();

    m_treeview->setNewCurrentItem(target_item);
    m_treeview->scrollTo(m_treeview->currentIndex(), QAbstractItemView::ScrollHint::EnsureVisible);
    emit currentItemChanged();
    reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "ActivateItem %s", m_current_item->GetFullName());
}

#include <QFileDialog>

void MainWindow::fileOpen() {
    //m_recent_files_actions
    auto proj_dir = SharedStr("");
    if (m_root)
        proj_dir = AbstrStorageManager::Expand(m_root.get(), SharedStr("%projDir%"));
    else {
        if (!m_recent_file_entries.empty()) {
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

void MainWindow::reopen() {
    auto cip = m_address_bar->text();
    
    reportF(MsgCategory::commands, SeverityTypeID::ST_MajorTrace, "Reopen configuration");

    if (GetRegStatusFlags() & RSF_EventLog_ClearOnReLoad)
        m_eventlog_model->clear();

    LoadConfig(m_currConfigFileName.c_str(), cip.toUtf8());
}

void OnVersionComponentVisit(ClientHandle clientHandle, UInt32 componentLevel, CharPtr componentName) {
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

auto getGeoDMSAboutText() -> std::string {
    VectorOutStreamBuff buff;
    FormattedOutStream stream(&buff, FormattingFlags::None);

    stream << DMS_GetVersion();
    stream << ", Copyright (c) Object Vision b.v.\n";
    DMS_VisitVersionComponents(&stream, OnVersionComponentVisit);
    return { buff.GetData(), buff.GetDataEnd() };
}

void MainWindow::aboutGeoDms() {
    auto dms_about_text = getGeoDMSAboutText();
    dms_about_text += "- GeoDms icon obtained from: World icons created by turkkub-Flaticon https://www.flaticon.com/free-icons/world";
    QMessageBox::about(this, tr("About GeoDms"),
            tr(dms_about_text.c_str()));
}

void MainWindow::wiki() {
    QDesktopServices::openUrl(QUrl("https://geodms.nl", QUrl::TolerantMode));
}

DmsConfigTextButton::DmsConfigTextButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent) {}

void DmsConfigTextButton::paintEvent(QPaintEvent* event) {
    QStylePainter p(this);
    QStyleOptionButton option;
    initStyleOption(&option);
    option.state |= QStyle::State_MouseOver;
    p.drawControl(QStyle::CE_PushButton, option);
}

DmsRecentFileEntry::DmsRecentFileEntry(size_t index, std::string_view dms_file_full_path, QObject* parent)
    : QAction(parent) {
    m_cfg_file_path = dms_file_full_path;
    m_index = index;

    std::string preprending_spaces = index < 9 ? "   &" : "  ";
    std::string menu_text = preprending_spaces + std::to_string(index + 1) + ". " + std::string(ConvertDosFileName(SharedStr(dms_file_full_path.data())).c_str());
    setText(menu_text.c_str());
}

void DmsRecentFileEntry::showRecentFileContextMenu(QPoint pos) {
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

bool DmsRecentFileEntry::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto mouse_event = dynamic_cast<QMouseEvent*>(event);
        if (mouse_event && mouse_event->button()==Qt::MouseButton::RightButton) {
            showRecentFileContextMenu(mouse_event->globalPos());
            return true;
        }
    } else if (event->type() == QEvent::KeyPress) {
        auto key_event = dynamic_cast<QKeyEvent*>(event);
        if (key_event->key() == Qt::Key_Tab) {// handle tab event in case treeview is currently active

        }
    }

    return QAction::eventFilter(obj, event);
}

bool DmsRecentFileEntry::event(QEvent* e) {
    return QAction::event(e);
}

void DmsRecentFileEntry::onDeleteRecentFileEntry() const {
    auto main_window = MainWindow::TheOne();
    main_window->m_file_menu->close();
    main_window->removeRecentFileAtIndex(m_index);
}

void DmsRecentFileEntry::onFileEntryPressed() const {
    auto main_window = MainWindow::TheOne();

    if (GetRegStatusFlags() & RSF_EventLog_ClearOnLoad)
        main_window->m_eventlog_model->clear();
    main_window->m_file_menu->close();
    main_window->LoadConfig(m_cfg_file_path.c_str());
    main_window->saveRecentFileActionToRegistry();
}

void MainWindow::onFocusChanged(QWidget* old, QWidget* now) { }

void MainWindow::scheduleUpdateToolbar() {
    //ViewStyle current_toolbar_style = ViewStyle::tvsUndefined, requested_toolbar_viewstyle = ViewStyle::tvsUndefined;
    if (m_UpdateToolbarRequestPending || g_IsTerminating)
        return;

    m_UpdateToolbarRequestPending = true;
    QTimer::singleShot(0, [this]()
        {
            m_UpdateToolbarRequestPending = false;
            this->updateToolbar();
        }
    );
}

void MainWindow::updateDetailPagesToolbar() {
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
    spacer->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    //m_toolbar->addWidget(spacer);
    m_dms_toolbar_spacer_action.reset( m_toolbar->insertWidget(m_general_page_action.get(), spacer) );
}

void MainWindow::updateToolbar() {
    updateDmsToolbar();
}

bool MainWindow::event(QEvent* event) {
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

std::string fillOpenConfigSourceCommand(const std::string command, const std::string filename, const std::string line) {
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

void MainWindow::openConfigSourceDirectly(std::string_view filename, std::string_view line) {
    std::string filename_dos_style = ConvertDmsFileNameAlways(SharedStr(filename.data())).c_str();
    std::string command = GetGeoDmsRegKey("DmsEditor").c_str();
    std::string unexpanded_open_config_source_command = "";
    std::string open_config_source_command = "";
    if (!filename.empty() && !line.empty() && !command.empty()) {
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

void MainWindow::openConfigSourceFor(const TreeItem* context) {
    if (!context)
        return;
    auto filename = ConvertDmsFileNameAlways(context->GetConfigFileName());
    std::string line = std::to_string(context->GetConfigFileLineNr());
    openConfigSourceDirectly(filename.c_str(), line);
}

void MainWindow::openConfigSource() {
    openConfigSourceFor( getCurrentTreeItemOrRoot() );
}

void MainWindow::openConfigRootSource() {
    openConfigSourceFor( getRootTreeItem() );
}

TIC_CALL BestItemRef TreeItem_GetErrorSourceCaller(const TreeItem* src);

void MainWindow::focusAddressBar() const {
    m_address_bar->setFocus();
    m_address_bar->selectAll();
}

void MainWindow::stepToFailReason() {
    auto ti = getCurrentTreeItem();
    if (!ti)
        return;

    if (!WasInFailed(ti))
        return;

    SuspendTrigger::Resume();

    BestItemRef result = TreeItem_GetErrorSourceCaller(ti);
    if (result.first) {
        if (result.first->IsCacheItem())
            return;

        setCurrentTreeItem(const_cast<TreeItem*>(result.first.get()));
    }

    if (!result.second.empty()) {
        auto textWithUnfoundPart = m_address_bar->text() + " " + result.second.c_str();
        m_address_bar->setText(textWithUnfoundPart);
    }
}

void MainWindow::runToFailReason() {
    auto current_ti = getCurrentTreeItem();
    while (current_ti->WasFailed()) {
        stepToFailReason();
        auto new_ti = getCurrentTreeItem();
        if (current_ti == new_ti)
            break;
        current_ti = new_ti;
    }   
}

void MainWindow::visual_update_treeitem() {
    createView(ViewStyle::tvsUpdateItem);
}

void MainWindow::visual_update_subtree() {
    createView(ViewStyle::tvsUpdateTree);
}

void MainWindow::toggle_treeview() const {
    bool isVisible = m_treeview_dock->isVisible();
    m_treeview_dock->setVisible(!isVisible);
}

void MainWindow::toggle_detailpages() const {
    m_detail_pages->toggle(m_detail_pages->m_last_active_detail_page);
}

void MainWindow::toggle_eventlog() const {
    bool isVisible = m_eventlog_dock->isVisible();
    m_eventlog_dock->setVisible(!isVisible);
}

void MainWindow::toggle_toolbar() const {
    if (!m_toolbar)
        return; // when there is no DataView opened, there is also no (in)visible toolbar

    bool isVisible = m_toolbar->isVisible();
    m_toolbar->setVisible(!isVisible);
}

void MainWindow::toggle_currentitembar() const {
    bool isVisible = m_address_bar_container->isVisible();
    m_address_bar_container->setVisible(!isVisible);
}

void MainWindow::gui_options() {
    // Modal
    auto optionsWindow = new DmsGuiOptionsWindow(this);
    optionsWindow->show();
}

void MainWindow::advanced_options() {
    // Modal
    auto optionsWindow = new DmsLocalMachineOptionsWindow(this);
    optionsWindow->show();
}

void MainWindow::config_options() {
    auto optionsWindow = new DmsConfigOptionsWindow(this);
    optionsWindow->show();
}

void MainWindow::code_analysis_set_source() {
   try {
       TreeItem_SetAnalysisSource(getCurrentTreeItem());
   }
   catch (...) {
       auto errMsg = catchException(false);
       MainWindow::TheOne()->reportErrorAndTryReload(errMsg);
   }
}

void MainWindow::code_analysis_set_target() {
    try {
        TreeItem_SetAnalysisTarget(getCurrentTreeItem(), true);
    }
    catch (...) {
        auto errMsg = catchException(false);
        reportErrorAndTryReload(errMsg);
    }
}

void MainWindow::code_analysis_add_target() {
    try {
        TreeItem_SetAnalysisTarget(getCurrentTreeItem(), false);
    }
    catch (...) {
        auto errMsg = catchException(false);
        reportErrorAndTryReload(errMsg);
    }
}

void MainWindow::code_analysis_clr_targets() {
    TreeItem_SetAnalysisSource(nullptr);
}

bool MainWindow::reportErrorAndAskToReload(ErrMsgPtr error_message_ptr) {
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

void MainWindow::reportErrorAndTryReload(ErrMsgPtr error_message_ptr) {
    bool doReload = reportErrorAndAskToReload(std::move(error_message_ptr));
    if (doReload)
        TheOne()->reopen();
}

void MainWindow::exportPrimaryData() {
    if (!m_export_window)
        m_export_window = new DmsExportWindow(this);

    m_export_window->prepare();
    m_export_window->show();
}

void MainWindow::createView(ViewStyle viewStyle) {
    if (openErrorOnFailedCurrentItem())
        return;

    try {
        auto currItem = getCurrentTreeItem();
        static UInt32 s_ViewCounter = 0;
        if (!currItem)
            return;

        auto desktopItem = GetDefaultDesktopContainer(m_root); // rootItem->CreateItemFromPath("DesktopInfo");
        auto viewContextItem = desktopItem->CreateItemFromPath(mySSPrintF("View%d", s_ViewCounter++).c_str());

        SuspendTrigger::Resume();

        auto dms_mdi_subwindow = std::make_unique<QDmsViewArea>(m_mdi_area.get(), viewContextItem, currItem, viewStyle);
        dms_mdi_subwindow->setFocusPolicy(Qt::FocusPolicy::ClickFocus);
        connect(dms_mdi_subwindow.get(), &QDmsViewArea::windowStateChanged, dms_mdi_subwindow.get(), &QDmsViewArea::onWindowStateChanged);

        auto dms_view_window_icon = getIconFromViewstyle(viewStyle);
        dms_mdi_subwindow->setWindowIcon(dms_view_window_icon);
        m_mdi_area->addSubWindow(dms_mdi_subwindow.get());
        dms_mdi_subwindow.release();
    }
    catch (...) {
        auto errMsg = catchException(false);
        reportErrorAndTryReload(errMsg);
    }
}

void MainWindow::defaultViewOrAddItemToCurrentView() {
    try {
        if (auto active_mdi_subwindow = dynamic_cast<QDmsViewArea*>(m_mdi_area->activeSubWindow()))
        {
            if (active_mdi_subwindow->getDataView()->CanContain(m_current_item))
            {
                SHV_DataView_AddItem(active_mdi_subwindow->getDataView(), m_current_item, false);
                return;
            }
        }
        defaultView();
    }
    catch (...) {
        auto errMsg = catchException(false);
        reportErrorAndTryReload(errMsg);
    }
}

void MainWindow::defaultView() {
    reportF(MsgCategory::commands, SeverityTypeID::ST_MajorTrace, "defaultView // for item %s", m_current_item->GetFullName());
    auto default_view_style = SHV_GetDefaultViewStyle(m_current_item);
    if (default_view_style == ViewStyle::tvsPaletteEdit)
        default_view_style = ViewStyle::tvsTableView;
    if (default_view_style == ViewStyle::tvsDefault) {
        reportF(MsgCategory::other, SeverityTypeID::ST_Error, "Unable to deduce viewstyle for item %s, no view created.", m_current_item->GetFullName());
        return;
    }
    createView(default_view_style);
}

void MainWindow::mapView() {
    reportF(MsgCategory::commands, SeverityTypeID::ST_MajorTrace, "mapview // for item %s", m_current_item->GetFullName());
    createView(ViewStyle::tvsMapView);
}

void MainWindow::tableView() {
    reportF(MsgCategory::commands, SeverityTypeID::ST_MajorTrace, "tableView // for item %s", m_current_item->GetFullName());
    createView(ViewStyle::tvsTableView);
}

void geoDMSContextMessage(ClientHandle clientHandle, CharPtr msg) {
    assert(IsMainThread());
    auto mw = MainWindow::TheOne();
    if (mw)
        mw->setStatusMessage(msg);
    return;
}

bool MainWindow::CloseConfig() {
    TreeItem_SetAnalysisSource(nullptr); // clears all code-analysis coding

    // close all dms views
    bool has_active_dms_views = false;
    if (m_mdi_area) {
        has_active_dms_views = m_mdi_area->subWindowList().size();
        m_mdi_area->closeAllSubWindows();
    }

    // reset all dms tree data
    if (m_root) {
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
    for (auto* widget : value_info_windows) {
        if (dynamic_cast<ValueInfoWindow*>(widget))
            widget->close();
    }
    SessionData::ReleaseCurr();
    scheduleUpdateToolbar();
    return has_active_dms_views;
}

auto configIsInRecentFiles(std::string_view cfg, const std::vector<std::string>& files) -> Int32 {
    std::string cfg_name = cfg.data();
    UInt32 pos = 0;
    for (auto& recent_file : files) {
        if (recent_file.compare(cfg_name) == 0) {
            return pos;
        }
        pos++;
    }
    
    return -1;
}

void MainWindow::cleanRecentFilesThatDoNotExist() {
    auto recent_files_from_registry = GetGeoDmsRegKeyMultiString(dms_params::reg_key_RecentFiles);
    for (auto it_rf = recent_files_from_registry.begin(); it_rf != recent_files_from_registry.end();) {
        if ((strnicmp(it_rf->c_str(), "file:", 5) != 0)) {
            auto dosFileName = ConvertDmsFileName(SharedStr(it_rf->c_str()));
            if (!std::filesystem::exists(dosFileName.c_str()) || it_rf->empty()) {
                it_rf = recent_files_from_registry.erase(it_rf);
                continue;
            }
        }
        ++it_rf;
    }
    SetGeoDmsRegKeyMultiString("RecentFiles", recent_files_from_registry);
}

void MainWindow::removeRecentFileAtIndex(size_t index) {
    assert(m_recent_file_entries.size() >= 0);
    if (size_t(m_recent_file_entries.size()) <= index)
        return;

    auto menu_to_be_removed = m_recent_file_entries.at(index);
    auto rf_action = dynamic_cast<DmsRecentFileEntry*>(menu_to_be_removed);
    if (!rf_action)
        return;

    auto msgTxt = mySSPrintF("Remove %s from the list of recent files ?", rf_action->m_cfg_file_path);
    if (MessageBoxA((HWND)winId(), msgTxt.c_str(), "Confirmation Request", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        m_file_menu->removeAction(menu_to_be_removed);
        m_recent_file_entries.removeAt(index);
        saveRecentFileActionToRegistry();
    }
}

void MainWindow::saveRecentFileActionToRegistry() {
    std::vector<std::string> recent_files_as_std_strings;
    for (auto* recent_file_entry_candidate : m_recent_file_entries) {
        auto recent_file_entry = dynamic_cast<DmsRecentFileEntry*>(recent_file_entry_candidate);
        if (!recent_file_entry)
            continue;
        recent_files_as_std_strings.emplace_back(recent_file_entry->m_cfg_file_path);
    }
    SetGeoDmsRegKeyMultiString("RecentFiles", recent_files_as_std_strings);
}

void MainWindow::insertCurrentConfigInRecentFiles(std::string_view cfg) {
    auto cfg_index_in_recent_files = configIsInRecentFiles(cfg, GetGeoDmsRegKeyMultiString("RecentFiles"));
    if (cfg_index_in_recent_files == -1)
        addRecentFilesEntry(cfg);
    else
        m_recent_file_entries.move(cfg_index_in_recent_files, 0);

    saveRecentFileActionToRegistry();
    updateFileMenu();
}

void MainWindow::LoadConfig(CharPtr configFilePath, CharPtr currentItemPath) {
    bool hadSubWindows = CloseConfig();
    SharedStr configFilePathStr(configFilePath);
    SharedStr currentItemPathStr(currentItemPath);

    QTimer::singleShot(hadSubWindows ? 100 : 0, this, [=]()
        { 
            if (LoadConfigImpl(configFilePathStr.c_str()))
                QTimer::singleShot(0, this, [=]()
                    {
                        m_address_bar->setPath(currentItemPathStr.c_str());
                    }
                );
        }
    );
}

bool MainWindow::LoadConfigImpl(CharPtr configFilePath) {
  retry:
    try {
        auto orgConfigFilePath = SharedStr(configFilePath);
        m_currConfigFileName = ConvertDosFileName(orgConfigFilePath); // replace back-slashes to linux/dms style separators and prefix //SYSTEM/path with 'file:'
        
        auto folderName = splitFullPath(m_currConfigFileName.c_str());
        if (strnicmp(folderName.c_str(), "file:",5) != 0)
            SetCurrentDir(ConvertDmsFileNameAlways(std::move(folderName)).c_str());

        auto newRoot = CreateTreeFromConfiguration(m_currConfigFileName.c_str());

        m_root = newRoot;
        if (m_root) {
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
    catch (...) {
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

void MainWindow::showStatisticsDirectly(const TreeItem* tiContext) {
    if (openErrorOnFailedCurrentItem())
        return;

    if (!IsDataItem(tiContext)) {
        reportF(MsgCategory::commands, SeverityTypeID::ST_Warning, "Cannot show statistics window for items that are not of type dataitem: [[%s]]", tiContext->GetFullName());
        return;
    }

    auto* mdiSubWindow = new QMdiSubWindow(m_mdi_area.get());
    auto* statistics_browser = new StatisticsBrowser(mdiSubWindow);
    SuspendTrigger::Resume();
    statistics_browser->m_Context = tiContext;
    tiContext->PrepareData();
    mdiSubWindow->setWidget(statistics_browser);

    SharedStr title = "Statistics of " + SharedStr(tiContext->GetName());
    mdiSubWindow->setWindowTitle(title.c_str());
    mdiSubWindow->setWindowIcon(getIconFromViewstyle(ViewStyle::tvsStatistics));
    m_mdi_area->addSubWindow(mdiSubWindow);
    mdiSubWindow->setAttribute(Qt::WA_DeleteOnClose);
    mdiSubWindow->show();

    statistics_browser->restart_updating();
}

void MainWindow::showValueInfo(const AbstrDataItem* studyObject, SizeT index, SharedStr extraInfo) {
    assert(studyObject);
    new ValueInfoWindow(studyObject, index, extraInfo, this);
}

void MainWindow::setStatusMessage(CharPtr msg) {
    m_StatusMsg = msg;
    updateStatusMessage();
}

static bool s_IsTiming = false;
static std::time_t s_BeginTime = 0;

void MainWindow::begin_timing(AbstrMsgGenerator* /*ach*/) {
    if (s_IsTiming)
        return;
    s_IsTiming = true;
    s_BeginTime = std::time(nullptr);
}

std::time_t passed_time(const MainWindow::processing_record& pr) {
    return std::get<1>(pr) - std::get<0>(pr);
}

SharedStr passed_time_str(CharPtr preFix, time_t passedTime) {
    SharedStr result;
    time_t seconds_in_minute = 60;
    time_t seconds_in_hour = seconds_in_minute * 60;
    time_t hours_in_day = 24;
    time_t seconds_in_a_day = hours_in_day * seconds_in_hour;
    if (passedTime >= seconds_in_a_day) {
        result = AsString(passedTime / (seconds_in_a_day)) + " days and ";
        passedTime %= seconds_in_a_day;
    }
    assert(passedTime <= (seconds_in_a_day));
    result = mySSPrintF("%s%s%02d:%02d:%02d "
        , preFix
        , result.c_str()
        , passedTime / seconds_in_hour
        , (passedTime / seconds_in_minute) % seconds_in_minute
        , passedTime % seconds_in_minute
    );
    return result;
}

RTC_CALL auto GetMemoryStatus() -> SharedStr;

void MainWindow::end_timing(AbstrMsgGenerator* ach) {
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

    if (m_calculation_times_window->isVisible())
        update_calculation_times_report();

    assert(passedTime <= passed_time(m_processing_records.back()));
    if (!top_of_records_is_changing)
        return;

    assert(passedTime == passed_time(m_processing_records.back()));
    m_LongestProcessingRecordTxt = passed_time_str(" - max processing time: ", passedTime);

    updateStatusMessage();
}

RTC_CALL auto GetMemoryStatus() -> SharedStr;

static UInt32 s_ReentrancyCount = 0;
void MainWindow::updateStatusMessage() {
    if (s_ReentrancyCount)
        return;

    auto fullMsg = m_StatusMsg + m_LongestProcessingRecordTxt + " - " + GetMemoryStatus();
    DynamicIncrementalLock<> incremental_lock(s_ReentrancyCount);
    statusBar()->showMessage(fullMsg.c_str());
}

auto MainWindow::getIconFromViewstyle(ViewStyle viewstyle) const -> QIcon {
    switch (viewstyle) {
    case ViewStyle::tvsMapView: { return QPixmap(":/res/images/TV_globe.bmp"); }
    case ViewStyle::tvsStatistics: { return QPixmap(":/res/images/DP_statistics.bmp"); }
    case ViewStyle::tvsCalculationTimes: { return QPixmap(":/res/images/IconCalculationTimeOverview.png"); }
    case ViewStyle::tvsPaletteEdit: { return QPixmap(":/res/images/TV_palette.bmp");}
    case ViewStyle::tvsCurrentConfigFileList: { return QPixmap(":/res/images/ConfigFileList.png"); }
    default: { return QPixmap(":/res/images/TV_table.bmp");}
    }
}

void MainWindow::hideDetailPagesRadioButtonWidgets(bool hide_properties_buttons, bool hide_source_descr_buttons) const {
    m_detail_page_properties_buttons->gridLayoutWidget->setHidden(hide_properties_buttons);
    m_detail_page_source_description_buttons->gridLayoutWidget->setHidden(hide_source_descr_buttons);
}

void MainWindow::addRecentFilesEntry(std::string_view recent_file) {
    auto index = m_recent_file_entries.size();
    std::string preprending_spaces = index < 9 ? "   &" : "  ";
    std::string pushbutton_text = preprending_spaces + std::to_string(index + 1) + ". " + std::string(ConvertDosFileName(SharedStr(recent_file.data())).c_str());
    DmsRecentFileEntry* new_recent_file_entry = new DmsRecentFileEntry(index, recent_file.data(), this);

    m_file_menu->addAction(new_recent_file_entry);

    for (auto action_object_pointer : new_recent_file_entry->associatedObjects()) {
       action_object_pointer->installEventFilter(new_recent_file_entry);
    }
    m_recent_file_entries.push_back(new_recent_file_entry);

    // connections
    connect(new_recent_file_entry, &DmsRecentFileEntry::triggered, new_recent_file_entry, &DmsRecentFileEntry::onFileEntryPressed);
    connect(new_recent_file_entry, &DmsRecentFileEntry::toggled, new_recent_file_entry, &DmsRecentFileEntry::onFileEntryPressed);
}

auto Realm(const auto& x) -> CharPtrRange {
    auto b = begin_ptr(x);
    auto e = end_ptr(x);
    auto colonPos = std::find(b, e, ':');
    if (colonPos == e)
        return {};
    return { b, colonPos };
}

auto getLinkFromErrorMessage(std::string_view error_message, unsigned int lineNumber) -> link_info {
    std::string html_error_message = "";
    //auto error_message_text = std::string(error_message->Why().c_str());
    std::size_t currPos = 0, currLineNumber = 0;
    link_info lastFoundLink;
    while (currPos < error_message.size()) {
        auto currLineEnd = error_message.find_first_of('\n', currPos);
        if (currLineEnd == std::string::npos)
            currLineEnd = error_message.size();

        auto lineView = std::string_view(&error_message[currPos], currLineEnd - currPos);
        auto round_bracked_open_pos = lineView.find_first_of('(');
        auto comma_pos = lineView.find_first_of(',');
        auto round_bracked_close_pos = lineView.find_first_of(')');

        if (round_bracked_open_pos < comma_pos && comma_pos < round_bracked_close_pos && round_bracked_close_pos != std::string::npos) {
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

bool IsPostRequest(const QUrl& /*link*/) {
    return false;
}

void MainWindow::onInternalLinkClick(const QUrl& link, QWidget* origin) {
    try {
        auto linkStr = link.toString().toStdString();

        // log link action
#if defined(_DEBUG)
        MsgData data{ SeverityTypeID::ST_MajorTrace, MsgCategory::other, false, GetThreadID(), StreamableDateTime(), SharedStr(linkStr.data()) };
        MainWindow::TheOne()->m_eventlog_model->addText(std::move(data), false);
#endif

        auto* current_item = MainWindow::TheOne()->getCurrentTreeItem();

        auto realm = Realm(linkStr);
        if (realm.size() == 16 && !strnicmp(realm.begin(), "editConfigSource", 16)) {
            auto clicked_error_link = linkStr.substr(17);
            auto parsed_clicked_error_link = getLinkFromErrorMessage(clicked_error_link);
            MainWindow::TheOne()->openConfigSourceDirectly(parsed_clicked_error_link.filename, parsed_clicked_error_link.line);
            return;
        }
        if (realm.size() == 9 && !strnicmp(realm.begin(), "clipboard", 9)) {
            auto dataStr = linkStr.substr(10);
            auto decodedStr = UrlDecode(SharedStr(dataStr.c_str()));
            auto qStr = QString(decodedStr.c_str());
            QGuiApplication::clipboard()->clear();
            QGuiApplication::clipboard()->setText(qStr, QClipboard::Clipboard);
            return;
        }

        if (IsPostRequest(link)) {
            auto queryStr = link.query().toUtf8();
            DMS_ProcessPostData(const_cast<TreeItem*>(current_item), queryStr.data(), queryStr.size());
            return;
        }
        if (!ShowInDetailPage(SharedStr(linkStr.c_str()))) {
            auto raw_string = SharedStr(begin_ptr(linkStr), end_ptr(linkStr));
            ReplaceSpecificDelimiters(raw_string.GetAsMutableRange(), '\\');
            auto linkCStr = ConvertDosFileName(raw_string); // obtain zero-termination and non-const access
            QDesktopServices::openUrl(QUrl(linkCStr.c_str(), QUrl::TolerantMode));
            return;
        }

        if (linkStr.contains(".adms")) {
            auto queryResult = ProcessADMS(current_item, linkStr.data());
            m_detail_pages->setHtml(queryResult.c_str());
            return;
        }
        auto sPrefix = Realm(linkStr);
        if (!strncmp(sPrefix.begin(), "dms", sPrefix.size())) {
            auto sAction = CharPtrRange(begin_ptr(linkStr) + 4, end_ptr(linkStr));
            doViewAction(m_current_item, sAction, origin ? origin : nullptr);
            return;
        }
    }
    catch (...) {
        auto errMsg = catchAndReportException();
    }
}

void EditPropValue(TreeItem* /*tiContext*/, CharPtrRange /*url*/, SizeT /*recNo*/) {}
void PopupTable(SizeT /*recNo*/) {}
void MainWindow::doViewAction(TreeItem* tiContext, CharPtrRange sAction, QWidget* origin) {
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

    if (!strncmp(sMenu.begin(), "popuptable", sMenu.size())) {
        PopupTable(recNo);
        return;
    }

    if (!strncmp(sMenu.begin(), "edit", sMenu.size())) {
        EditPropValue(tiContext, sPathWithSub, recNo);
        return;
    }

    if (!strncmp(sMenu.begin(), "goto", sMenu.size())) {
        tiContext = const_cast<TreeItem*>(tiContext->FindBestItem(sPath).first.get()); // TODO: make result FindBestItem non-const
        if (tiContext)
            MainWindow::TheOne()->setCurrentTreeItem(tiContext);
        return;
    }

    // Value info link clicked from value info window
    tiContext = const_cast<TreeItem*>(tiContext->FindBestItem(sPath).first.get()); // TODO: make result FindBestItem non-const
    if (origin && IsDefined(recNo)) {
        auto value_info_browser = dynamic_cast<ValueInfoBrowser*>(origin);
        if (value_info_browser) {
            value_info_browser->addStudyObject(AsDataItem(tiContext), recNo, SharedStr(sSub));
            return;
        }
    }

    // Detail- or new ValueinfoWindow requested
    if (sMenu.size() >= 3 && !strncmp(sMenu.begin(), "dp.", 3)) {
        sMenu.first += 3;

        auto detail_page_type = m_detail_pages->activeDetailPageFromName(sMenu);
        switch (detail_page_type) {
        case ActiveDetailPage::STATISTICS: {
            MainWindow::TheOne()->showStatisticsDirectly(tiContext);
            return;
        }
        case ActiveDetailPage::VALUE_INFO: {
            if (!IsDataItem(tiContext))
                return;

            return MainWindow::TheOne()->showValueInfo(AsDataItem(tiContext), recNo, SharedStr(sSub));
        }
        default: {
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
void AnyTreeItemStateHasChanged(ClientHandle /*clientHandle*/, const TreeItem* self, NotificationCode notificationCode) {
    auto mainWindow = MainWindow::TheOne();
    if (!mainWindow)
        return;
    switch (notificationCode) {
    case NC_Deleting: break; // TODO: remove self from any representation to avoid accessing it's dangling pointer
    case NC_Creating: break;
    case CC_CreateMdiChild: {
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
    if (!s_TreeViewRefreshPending) {
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

void OnStartWaiting(ClientHandle /*clientHandle*/, AbstrMsgGenerator* ach) {
    assert(IsMainThread());
    QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    auto mw = MainWindow::TheOne();
    if (mw)
        mw->begin_timing(ach);
}

void OnEndWaiting(ClientHandle /*clientHandle*/, AbstrMsgGenerator* ach) {
    QGuiApplication::restoreOverrideCursor();
    auto mw = MainWindow::TheOne();
    if (mw)
        mw->end_timing(ach);
}

void MainWindow::setupDmsCallbacks() {
    DMS_RegisterMsgCallback(&geoDMSMessage, this);
    DMS_SetContextNotification(&geoDMSContextMessage, this);
    SHV_SetCreateViewActionFunc(&OnViewAction);
    DMS_RegisterStateChangeNotification(AnyTreeItemStateHasChanged, this);
    register_overlapping_periods_callback(OnStartWaiting, OnEndWaiting, this);
}

void MainWindow::cleanupDmsCallbacks() {
    unregister_overlapping_periods_callback(OnStartWaiting, OnEndWaiting, this);
    DMS_ReleaseStateChangeNotification(AnyTreeItemStateHasChanged, this);
    DMS_SetContextNotification(nullptr, nullptr);
    SHV_SetCreateViewActionFunc(nullptr);
    DMS_ReleaseMsgCallback(&geoDMSMessage, this);
}

void MainWindow::updateFileMenu() {
    for (auto recent_file_entry : m_recent_file_entries) {
        m_file_menu->removeAction(recent_file_entry);
        delete recent_file_entry;
    }
    m_recent_file_entries.clear();
    cleanRecentFilesThatDoNotExist();
    auto recent_files_from_registry = GetGeoDmsRegKeyMultiString("RecentFiles"); 

    for (std::string_view recent_file : recent_files_from_registry)
        addRecentFilesEntry(recent_file);
}

void MainWindow::updateViewMenu() const {
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
    m_toggle_currentitembar_action->setChecked(m_address_bar_container->isVisible());
    m_processing_records.empty() ? m_view_calculation_times_action->setDisabled(true) : m_view_calculation_times_action->setEnabled(true);
}

void MainWindow::updateToolsMenu() const {
    m_code_analysis_add_target_action->setEnabled(m_current_item);
    m_code_analysis_clr_targets_action->setEnabled(m_current_item);
    m_code_analysis_set_source_action->setEnabled(m_current_item);
    m_code_analysis_set_target_action->setEnabled(m_current_item);
}

void MainWindow::updateSettingsMenu() const {
    m_config_options_action->setEnabled(DmsConfigOptionsWindow::hasOverridableConfigOptions());
}

void MainWindow::updateWindowMenu() const {
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
    for (auto* sw : m_mdi_area->subWindowList()) {
        auto qa = new QAction(sw->windowTitle(), m_window_menu.get());
        ViewStyle viewstyle = static_cast<ViewStyle>(sw->property("viewstyle").value<QVariant>().toInt());
        qa->setIcon(getIconFromViewstyle(viewstyle));               
        connect(qa, &QAction::triggered, sw, [this, sw] { this->m_mdi_area->setActiveSubWindow(sw); });
        subWindowCount++;
        if (sw == asw) {
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

bool is_filenameBase_eq_rootname(CharPtrRange fileName, CharPtrRange rootName) {
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

void MainWindow::updateCaption() {
    VectorOutStreamBuff buff;
    FormattedOutStream out(&buff, FormattingFlags());
    if (!m_currConfigFileName.empty()) {
        auto fileName = getFileName(m_currConfigFileName.c_str());
        out << fileName;
        if (m_root) {
            auto rootName = m_root->GetName();
            CharPtr rootNameCStr = rootName.c_str();
            if (rootNameCStr && *rootNameCStr) {
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

void MainWindow::updateTracelogHandle() {
    if (GetRegStatusFlags() & RSF_TraceLogFile) {
        if (m_TraceLogHandle)
            return;
        auto fileName = AbstrStorageManager::Expand("", "%LocalDataDir%/trace.log");
        m_TraceLogHandle = std::make_unique<CDebugLog>(fileName);
    }
    else
        m_TraceLogHandle.reset();
}

void MainWindow::createStatusBar() {
    auto status_bar = statusBar();
    status_bar->showMessage(tr("Ready"));
    status_bar->setFocusPolicy(Qt::FocusPolicy::ClickFocus);
    
    connect(statusBar(), &QStatusBar::messageChanged, this, &MainWindow::on_status_msg_changed);
    m_statusbar_coordinates = new QLineEdit(this);
    m_statusbar_coordinates->setReadOnly(true);
    m_statusbar_coordinates->setFixedWidth(dms_params::coordinates_bar_width);
    m_statusbar_coordinates->setAlignment(Qt::AlignmentFlag::AlignLeft);
    m_statusbar_coordinates->setFocusPolicy(Qt::FocusPolicy::ClickFocus);

    statusBar()->insertPermanentWidget(0, m_statusbar_coordinates);
    m_statusbar_coordinates->setVisible(false);
}

void MainWindow::on_status_msg_changed(const QString& msg) {
    if (msg.isEmpty())
        updateStatusMessage();
}

CharPtrRange myAscTime(const struct tm* timeptr) {
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

void MainWindow::update_calculation_times_report() {
    VectorOutStreamBuff vosb;
    FormattedOutStream os(&vosb, FormattingFlags());
    os << "Top " << m_processing_records.size() << " calculation times:\n";
    for (const auto& pr : m_processing_records) {
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

void MainWindow::view_calculation_times() {
    update_calculation_times_report();
    //m_calculation_times_window->setAttribute(Qt::WA_DeleteOnClose);

    m_calculation_times_browser->show();
    m_calculation_times_window->setWindowTitle("Calculation time overview");
    m_calculation_times_window->setWindowIcon(QPixmap(":/res/images/IconCalculationTimeOverview.png"));
    m_calculation_times_window->show();
}

void MainWindow::view_current_config_filelist() const {
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

void MainWindow::expandAll() {
    FixedContextHandle context("expandAll");
    Waiter waitReporter(&context);
    reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "expandAll");
    m_treeview->expandAll();
}

#include "dbg/DebugReporter.h"
void MainWindow::debugReports() {

#if defined(MG_DEBUGREPORTER)
    DebugReporter::ReportAll();
#endif defined(MG_DEBUGREPORTER)
}

void MainWindow::expandActiveNode(bool doExpand) {
    FixedContextHandle context("expandActiveNode");
    Waiter waitReporter(&context);
    reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "expand");

    m_treeview->expandActiveNode(doExpand);
}

void MainWindow::expandRecursiveFromCurrentItem() {
    FixedContextHandle context("expandRecursiveFromCurrentItem");
    Waiter waitReporter(&context);
    reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "expandRecursiveFromCurrentItem");

    m_treeview->expandRecursiveFromCurrentItem();
}

void MainWindow::createDetailPagesDock() {
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
}

void MainWindow::createDmsHelperWindowDocks() {
    createDetailPagesDock();

    m_treeview = createTreeview(this);
    m_eventlog = createEventLog(this);

    // resize docks
    resizeDocks({ m_treeview_dock, m_detailpages_dock }, { 300, 300 }, Qt::Horizontal);
    resizeDocks({ m_eventlog_dock }, { 150 }, Qt::Vertical);
}

void MainWindow::back() {
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

void MainWindow::forward() {
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