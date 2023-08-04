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

#include "ptr/SharedPtr.h"
#include "ShvUtils.h"
#include "dataview.h"

#include "DockManager.h"
#include "DockAreaWidget.h"
#include "DockWidget.h"

QT_BEGIN_NAMESPACE
class QAction;
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
    using QLineEdit::QLineEdit;
    void setDmsCompleter();
    void setPath(CharPtr itemPath);

public slots:
    void setPathDirectly(QString path);
    void onEditingFinished();
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

class DmsRecentFileButtonAction : public QAction
{
    Q_OBJECT
public:
    DmsRecentFileButtonAction(size_t index, std::string_view dms_file_full_path, QObject* parent = nullptr);
    std::string m_cfg_file_path;
public slots:
    void onToolbuttonPressed();
};

struct CmdLineSetttings {
    bool m_NoConfig = false;
    SharedStr m_ConfigFileName;
    std::vector<SharedStr> m_CurrItemFullNames;
};

class DmsFileChangedWindow : public QDialog
{
    Q_OBJECT

public: //auto changed_files = DMS_ReportChangedFiles(true);
    DmsFileChangedWindow(QWidget* parent);
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
    DmsErrorWindow(QWidget* parent);
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


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(CmdLineSetttings& cmdLineSettings);
    ~MainWindow();

    auto getRootTreeItem() -> TreeItem* { return m_root; }
    auto getCurrentTreeItem() -> TreeItem* { return m_current_item; }
    void setCurrentTreeItem(TreeItem* new_current_item, bool update_history=true);
    bool LoadConfig(CharPtr configFilePath);
    void updateToolbar();
    void openConfigSourceDirectly(std::string_view filename, std::string_view line);
    void cleanRecentFilesThatDoNotExist();
    void insertCurrentConfigInRecentFiles(std::string_view cfg);
    void setRecentFiles();

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
    void exportPrimaryData();

    void gui_options();
    void advanced_options();
    void config_options();

    void code_analysis_set_source();
    void code_analysis_set_target();
    void code_analysis_add_target();
    void code_analysis_clr_targets();

    static bool reportErrorAndTryReload(ErrMsgPtr error_message_ptr);
    void stepToFailReason();
    void runToFailReason();

    void toggle_treeview();
    void toggle_detailpages();
    void toggle_eventlog();
    void toggle_toolbar();
    void toggle_currentitembar();

public slots:
    void fileOpen();
    void reopen();
    bool reOpen();

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

private:
    bool openErrorOnFailedCurrentItem();
    void clearActionsForEmptyCurrentItem();
    void updateActionsForNewCurrentItem();
    void CloseConfig();
    void setupDmsCallbacks();
    void cleanupDmsCallbacks();
    void createActions();
    void createStatusBar();
    void createDetailPagesDock();
    void createDmsHelperWindowDocks();
    void updateFileMenu();
    void updateViewMenu();
    void updateToolsMenu();
    void updateWindowMenu();
    void updateCaption();
    void updateTreeItemVisitHistory();
    void createDetailPagesActions();
    void updateDetailPagesToolbar();
    void on_status_msg_changed(const QString& msg);
    void updateStatusMessage();
    void view_calculation_times();
    void begin_timing(AbstrMsgGenerator* ach); friend void OnStartWaiting(void* clientHandle, AbstrMsgGenerator* ach);
    void end_timing(AbstrMsgGenerator* ach);   friend void OnEndWaiting  (void* clientHandle, AbstrMsgGenerator* ach);

    static void OnViewAction(const TreeItem* tiContext, CharPtr sAction, Int32 nCode, Int32 x, Int32 y, bool doAddHistory, bool isUrl, bool mustOpenDetailsPage);

    SharedStr m_currConfigFileName;
    SharedPtr<TreeItem> m_root;
    SharedPtr<TreeItem> m_current_item;



public:
    // helper window docks
    QPointer<QDockWidget> m_detailpages_dock, m_treeview_dock, m_eventlog_dock;

    std::unique_ptr<QMenu> m_file_menu, m_edit_menu, m_view_menu, m_tools_menu, m_window_menu, m_help_menu
        , m_code_analysis_submenu;

    // shared actions
    std::unique_ptr<QAction> m_export_primary_data_action
        , m_step_to_failreason_action, m_go_to_causa_prima_action, m_edit_config_source_action
        , m_update_treeitem_action, m_update_subtree_action, m_invalidate_action
        , m_defaultview_action, m_tableview_action, m_mapview_action, m_statistics_action
        //    , m_histogramview_action
        , m_process_schemes_action, m_view_calculation_times_action
        , m_toggle_treeview_action, m_toggle_detailpage_action, m_toggle_eventlog_action, m_toggle_toolbar_action, m_toggle_currentitembar_action
        , m_gui_options_action, m_advanced_options_action, m_config_options_action
        , m_code_analysis_set_source_action, m_code_analysis_set_target_action, m_code_analysis_add_target_action, m_code_analysis_clr_targets_action
        , m_quit_action
        , m_back_action, m_forward_action, m_general_page_action, m_explore_page_action, m_properties_page_action, m_configuration_page_action, m_sourcedescr_page_action, m_metainfo_page_action;

    // unique application objects
    std::unique_ptr<QDmsMdiArea> m_mdi_area;
    std::unique_ptr<DmsModel> m_dms_model;
    std::unique_ptr<EventLogModel> m_eventlog_model;
    std::unique_ptr<DmsCurrentItemBar> m_current_item_bar;
    std::unique_ptr<QComboBox> m_treeitem_visit_history;

    // helper windows; TODO: destroy these before the above model objects
    QPointer<QLineEdit> m_statusbar_coordinates;
    QPointer<DmsDetailPages> m_detail_pages;
    std::unique_ptr<DmsEventLog> m_eventlog;
    DmsTreeView* m_treeview;
    QPointer<QToolBar> m_toolbar, m_current_item_bar_container;
//    QPointer<QToolBar> m_right_side_toolbar;
//    QPointer<QLabel>   m_StatusWidget;

    QPointer<QMdiSubWindow> m_tooled_mdi_subwindow;
    QPointer<DmsExportWindow> m_export_window;
    QPointer<DmsErrorWindow> m_error_window;
//    QPointer<DmsGuiOptionsWindow> m_gui_options_window;
//    QPointer<DmsAdvancedOptionsWindow> m_advanced_options_window;
//    QPointer<DmsConfigOptionsWindow> m_config_options_window;
    QPointer<DmsFileChangedWindow> m_file_changed_window;

    using processing_record = std::tuple<std::time_t, std::time_t, SharedStr>;
private:
    std::vector<processing_record> m_processing_records;

    QList<QAction*> m_CurrWindowActions;
    QList<DmsRecentFileButtonAction*> m_recent_files_actions;
    SharedStr m_StatusMsg, m_LongestProcessingRecordTxt;
    bool m_UpdateToolbarResuestPending = false;
};

#endif
