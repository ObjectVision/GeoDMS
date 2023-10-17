// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QLineEdit>
#include <QCompleter>
#include <QToolBar>
#include <QTextEdit>
#include <QDialog>
#include <QCheckbox>
#include <QSlider>
#include <QFileDialog>
#include <QTextBrowser>
#include <QListView>
#include <QMenu>
#include <QWidgetAction>
#include <QMdiSubWindow>

#include "ptr/SharedPtr.h"
#include "ShvUtils.h"
#include "dataview.h"

#include "DockManager.h"
#include "DockAreaWidget.h"
#include "DockWidget.h"

#include "ui_DmsDetailPageProperties.h"
#include "ui_DmsDetailPageSourceDescription.h"

QT_BEGIN_NAMESPACE
class QAction;
class QWidgetAction;
class QComboBox;
class QDialog;
class QLabel;
class QMdiSubWindow;
class QMenu;
class QTextBrowser;
class QTextEdit;
class QTreeView;
QT_END_NAMESPACE

struct TreeItem;
class QDmsMdiArea;
class DmsDetailPages;
class DmsTreeView;
class DmsEventLog;
class DmsModel;
class DmsExportWindow;
class EventLogModel;

class DmsCurrentItemBar : public QLineEdit
{
Q_OBJECT
public:
    DmsCurrentItemBar(QWidget* parent = nullptr);
    void setDmsCompleter();
    void setPath(CharPtr itemPath);

public slots:
    void setPathDirectly(QString path);
    void onEditingFinished();

private:
    void findItem(const TreeItem* context, QString path, bool updateHistory);
};

enum class ButtonType
{
    SINGLE,
    SINGLE_EXCLUSIVE,
    MULTISTATE,
    MODAL
};

struct ToolbarButtonData
{
    std::vector<QString> text;
    std::vector<ToolButtonID> ids;
    std::vector<QString> icons;
    bool is_global = false;
};

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

class DmsToolbuttonAction : public QAction
{
    Q_OBJECT
public:
    DmsToolbuttonAction(const QIcon& icon, const QString& text, QObject* parent = nullptr, ToolbarButtonData button_data = {}, ViewStyle vs=ViewStyle::tvsUndefined);

public slots:
    void onToolbuttonPressed();

private:
    auto getNumberOfStates() -> UInt8 { return m_data.ids.size(); } ;

    ToolbarButtonData m_data;
    UInt8 m_state = 0;
};

class DmsConfigTextButton : public QPushButton
{
    Q_OBJECT

public:
    DmsConfigTextButton(const QString& text, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event);
};

class DmsRecentFileEntry : public QAction
{
    Q_OBJECT

public:
    DmsRecentFileEntry(size_t index, std::string_view dms_file_full_path, QObject* parent = nullptr);
    std::string m_cfg_file_path;
    size_t m_index = 0;
    DmsConfigTextButton* m_config_text;
    std::unique_ptr<QMenu> m_context_menu;
    bool eventFilter(QObject* obj, QEvent* ev) override;
    bool event(QEvent* e) override;

public slots:
    void onDeleteRecentFileEntry();
    void onFileEntryPressed();

private:
    void showRecentFileContextMenu(QPoint pos);
};

struct CmdLineSetttings {
    bool m_NoConfig = false;
    SharedStr m_ConfigFileName;
    std::vector<SharedStr> m_CurrItemFullNames;
    SharedStr m_TestScriptName;
};

class DmsFileChangedWindow : public QDialog
{
    Q_OBJECT

public:
    DmsFileChangedWindow(QWidget* parent = nullptr);
    void setFileChangedMessage(std::string_view changed_files);

private slots:
    void ignore();
    void reopen();
    void onAnchorClicked(const QUrl& link);

private:
    QPointer<QPushButton> m_ignore;
    QPointer<QPushButton> m_reopen;
    QPointer<QTextBrowser> m_message;
};


class DmsErrorWindow : public QDialog
{
    Q_OBJECT

public:
    DmsErrorWindow(QWidget* parent = nullptr);
    void setErrorMessageHtml(QString message) { m_message->setHtml(message); };

private slots:
    void ignore();
    void terminate();
    void reopen();
    void onAnchorClicked(const QUrl& link);

private:
    QPointer<QPushButton> m_ignore;
    QPointer<QPushButton> m_terminate;
    QPointer<QPushButton> m_reopen;
    QPointer<QTextBrowser> m_message;
};

class CalculationTimesWindow : public QMdiSubWindow
{
public:
    CalculationTimesWindow();
    ~CalculationTimesWindow();
    bool eventFilter(QObject* obj, QEvent* e) override;
};

bool IsPostRequest(const QUrl& /*link*/);
auto Realm(const auto& x) -> CharPtrRange;
auto getLinkFromErrorMessage(std::string_view error_message, unsigned int lineNumber = 0) -> link_info;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(CmdLineSetttings& cmdLineSettings);
    ~MainWindow();

    auto getRootTreeItem() -> TreeItem* { return m_root; }
    auto getCurrentTreeItem() -> TreeItem* { return m_current_item; }
    auto getCurrentTreeItemOrRoot() -> TreeItem* { return m_current_item ? m_current_item : m_root; }
    void setCurrentTreeItem(TreeItem* new_current_item, bool update_history=true);
    void LoadConfig(CharPtr configFilePath, CharPtr currentItemPath = "");
    bool LoadConfigImpl(CharPtr configFilePath);
    void updateToolbar();
    void openConfigSourceDirectly(std::string_view filename, std::string_view line);
    void cleanRecentFilesThatDoNotExist();
    void insertCurrentConfigInRecentFiles(std::string_view cfg);
    void removeRecentFileAtIndex(size_t index);
    void saveRecentFileActionToRegistry();
    auto CreateCodeAnalysisSubMenu(QMenu* menu) -> std::unique_ptr<QMenu>;
    auto getIconFromViewstyle(ViewStyle vs) -> QIcon;
    void hideDetailPagesRadioButtonWidgets(bool hide_properties_buttons, bool hide_source_descr_buttons);
    void addRecentFilesEntry(std::string_view recent_file);
    void resizeDocksToNaturalSize();
    void onInternalLinkClick(const QUrl& link, QWidget* origin = nullptr);
    void doViewAction(TreeItem* tiContext, CharPtrRange sAction, QWidget* origin = nullptr);
    bool ShowInDetailPage(SharedStr x);

    static auto TheOne() -> MainWindow*;
    static bool IsExisting();
signals:
    void currentItemChanged();

public slots:
    void defaultViewOrAddItemToCurrentView();
    void defaultView();
    void tableView();
    void mapView();
    void openConfigSource();
    void openConfigRootSource();
    void exportPrimaryData();

    void gui_options();
    void advanced_options();
    void config_options();

    void code_analysis_set_source();
    void code_analysis_set_target();
    void code_analysis_add_target();
    void code_analysis_clr_targets();

    static bool reportErrorAndAskToReload(ErrMsgPtr error_message_ptr);
    static void reportErrorAndTryReload(ErrMsgPtr error_message_ptr);
    void stepToFailReason();
    void runToFailReason();

    void toggle_treeview();
    void toggle_detailpages();
    void toggle_eventlog();
    void toggle_toolbar();
    void toggle_currentitembar();
    void toggle_valueinfo();

    void view_calculation_times();
    void view_current_config_filelist();
    void update_calculation_times_report();

    void expandAll();
    void debugTreeItemWithMemoryReport();
    void expandActiveNode(bool doExpand);
    void expandRecursiveFromCurrentItem();

public slots:
    void fileOpen();
    void reopen();

    void aboutGeoDms();
    void wiki();
    void createView(ViewStyle viewStyle);

    void back();
    void forward();

    void scheduleUpdateToolbar();
    //void showTreeviewContextMenu(const QPoint& pos);
    void showStatisticsDirectly(const TreeItem* tiContext);
    void showValueInfo(const AbstrDataItem* studyObject, SizeT index);
    void showStatistics() { showStatisticsDirectly(getCurrentTreeItem()); }
    void setStatusMessage(CharPtr msg);

    void visual_update_treeitem();
    void visual_update_subtree();

protected:
    bool event(QEvent* event) override;

public:
    void updateToolsMenu();
    void updateTracelogHandle();

private:
    void openConfigSourceFor(const TreeItem* context);
    //auto createRecentFilesWidgetAction(int index, std::string_view cfg, QWidget* parent) -> QWidgetAction*;
    void reconnectToolbarActionsForSameStyleView();
    void clearToolbarUpToDetailPagesTools();
    bool openErrorOnFailedCurrentItem();
    void clearActionsForEmptyCurrentItem();
    void updateActionsForNewCurrentItem();
    bool CloseConfig(); // returns true when mdiSubWindows were closed
    void setupDmsCallbacks();
    void cleanupDmsCallbacks();
    void createActions();
    void createStatusBar();
    void createDetailPagesDock();
    void createValueInfoDock();
    void createDmsHelperWindowDocks();
    void updateFileMenu();
    void updateViewMenu();
    void updateSettingsMenu();
    void updateWindowMenu();
    void updateCaption();
    void updateTreeItemVisitHistory();
    void createDetailPagesActions();
    void updateDetailPagesToolbar();
    void on_status_msg_changed(const QString& msg);
    void updateStatusMessage();

    void begin_timing(AbstrMsgGenerator* ach); friend void OnStartWaiting(void* clientHandle, AbstrMsgGenerator* ach);
    void end_timing(AbstrMsgGenerator* ach);   friend void OnEndWaiting  (void* clientHandle, AbstrMsgGenerator* ach);


    static void OnViewAction(const TreeItem* tiContext, CharPtr sAction, Int32 nCode, Int32 x, Int32 y, bool doAddHistory, bool isUrl, bool mustOpenDetailsPage);

    SharedStr m_currConfigFileName;
    SharedPtr<TreeItem> m_root;
    SharedPtr<TreeItem> m_current_item;

public: 
    // helper window docks
    QPointer<QDockWidget> m_detailpages_dock, m_treeview_dock, m_eventlog_dock, m_value_info_dock;
    QPointer<QDmsMdiArea> m_value_info_mdi_area;

    std::unique_ptr<QMenu> m_file_menu, m_edit_menu, m_view_menu, m_tools_menu, m_window_menu, m_settings_menu, m_help_menu, m_code_analysis_submenu;

    // shared actions
    std::unique_ptr<QAction> m_export_primary_data_action
        , m_step_to_failreason_action, m_go_to_causa_prima_action, m_edit_config_source_action
        , m_update_treeitem_action, m_update_subtree_action, m_invalidate_action
        , m_defaultview_action, m_tableview_action, m_mapview_action, m_statistics_action
        //    , m_histogramview_action
        , m_process_schemes_action, m_view_calculation_times_action, m_view_current_config_filelist, m_open_root_config_file_action, m_expand_all_action
        , m_toggle_treeview_action, m_toggle_detailpage_action, m_toggle_eventlog_action, m_toggle_toolbar_action, m_toggle_currentitembar_action, m_toggle_valueinfo_action
        , m_gui_options_action, m_advanced_options_action, m_config_options_action
        , m_code_analysis_set_source_action, m_code_analysis_set_target_action, m_code_analysis_add_target_action, m_code_analysis_clr_targets_action
        , m_win_tile_action, m_win_cascade_action, m_win_close_action, m_win_close_all_action, m_win_close_but_this_action
        , m_quit_action
        , m_back_action, m_forward_action, m_general_page_action, m_explore_page_action, m_properties_page_action, m_configuration_page_action, m_sourcedescr_page_action, m_metainfo_page_action
        , m_eventlog_filter_toggle, m_debug_treeitem_with_mem_report;


    // unique application objects
    QPointer<QDmsMdiArea> m_mdi_area;
    std::unique_ptr<CalculationTimesWindow> m_calculation_times_window;
    std::unique_ptr<QTextBrowser> m_calculation_times_browser;
    std::unique_ptr<DmsModel> m_dms_model;
    std::unique_ptr<EventLogModel> m_eventlog_model;
    std::unique_ptr<DmsCurrentItemBar> m_current_item_bar;
    std::unique_ptr<QComboBox> m_treeitem_visit_history;

    // helper windows; TODO: destroy these before the above model objects
    QPointer<QLabel> m_statusbar_coordinate_label;
    QPointer<QLineEdit> m_statusbar_coordinates;
    std::unique_ptr<Ui::dp_properties> m_detail_page_properties_buttons;
    std::unique_ptr<Ui::dp_sourcedescription> m_detail_page_source_description_buttons;
    
    QPointer<DmsDetailPages> m_detail_pages;
    std::unique_ptr<DmsEventLog> m_eventlog;
    DmsTreeView* m_treeview;
    QPointer<QToolBar> m_toolbar, m_current_item_bar_container;
    ViewStyle m_current_toolbar_style = ViewStyle::tvsUndefined;
    std::unique_ptr<QAction> m_dms_toolbar_spacer_action;
    std::vector<QAction*> m_current_dms_view_actions;

    QPointer<QMdiSubWindow> m_tooled_mdi_subwindow;
    QPointer<DmsExportWindow> m_export_window;
    QPointer<DmsErrorWindow> m_error_window;
    QPointer<DmsFileChangedWindow> m_file_changed_window;

    using processing_record = std::tuple<std::time_t, std::time_t, SharedStr>;
    //QList<QWidgetAction*> m_recent_files_actions;
    QList<DmsRecentFileEntry*> m_recent_file_entries;

private:
    std::vector<processing_record> m_processing_records;
    SharedStr m_StatusMsg, m_LongestProcessingRecordTxt;
    bool m_UpdateToolbarRequestPending = false;
    std::unique_ptr<CDebugLog> m_TraceLogHandle;
};

#endif
