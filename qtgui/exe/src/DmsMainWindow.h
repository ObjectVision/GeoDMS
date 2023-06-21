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
class QMenu;
class QTextEdit;
class QTextBrowser;
class QTreeView;
class QDialog;
class QLabel;
class QMdiSubWindow;
QT_END_NAMESPACE

struct TreeItem;
class QDmsMdiArea;
class DmsDetailPages;
class DmsTreeView;
class DmsEventLog;
class DmsModel;
class EventLogModel;

class DmsCurrentItemBar : public QLineEdit
{
Q_OBJECT
public:
    using QLineEdit::QLineEdit;
    void setDmsCompleter();
    void setPath(CharPtr itemPath);

public slots:
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

// BEGIN EXPORT TODO: move to more appropriate location
enum class driver_characteristics : UInt32
{
    none = 0,
    is_raster = 0x01,
    native_is_default = 0x02,
    tableset_is_folder = 0x04,
};
inline bool operator &(driver_characteristics lhs, driver_characteristics rhs) { return UInt32(lhs) & UInt32(rhs); }
inline driver_characteristics operator |(driver_characteristics lhs, driver_characteristics rhs) { return driver_characteristics(UInt32(lhs) | UInt32(rhs)); }

struct gdal_driver_id
{
    CharPtr shortname = nullptr;
    CharPtr name = nullptr;
    CharPtr nativeName = nullptr;
    driver_characteristics drChars = driver_characteristics::none;

    CharPtr Caption()
    {
        if (name)
            return name;
        return shortname;
    }

    bool HasNativeVersion() { return nativeName; }

    bool IsEmpty()
    {
        return shortname == nullptr;
    }
};


// END EXPORT

class DmsOptionsWindow : public QDialog
{
    Q_OBJECT

private slots:
    void onFlushTresholdValueChange(int value);
    void restoreOptions();
    void ok();
    void apply();
    void undo();
    void onStateChange(int state);
    void onTextChange(const QString& text);
    void setLocalDataDirThroughDialog();
    void setSourceDataDirThroughDialog();

    /*void onLocalDataDirChange();
    void onSourceDataDirChange();
    void onDmsEditorChange();
    void onPP0Change();
    void onPP1Change();
    void onPP2Change();
    void onPP3Change();
    void onTracelogFileChange();*/

public:
    DmsOptionsWindow(QWidget* parent = nullptr);
private:
    void setInitialLocalDataDirValue();
    void setInitialSourceDatDirValue();
    void setInitialEditorValue();
    void setInitialMemoryFlushTresholdValue();

    bool m_changed = false;

    QPointer<QFileDialog> m_folder_dialog;
    QPointer<QLabel> m_flush_treshold_text;
    QPointer<QCheckBox> m_pp0;
    QPointer<QCheckBox> m_pp1;
    QPointer<QCheckBox> m_pp2;
    QPointer<QCheckBox> m_pp3;
    QPointer<QCheckBox> m_tracelog;
    QPointer<QLineEdit> m_ld_input;
    QPointer<QLineEdit> m_sd_input;
    QPointer<QLineEdit> m_editor_input;
    QPointer<QSlider>   m_flush_treshold;
    QPointer<QPushButton> m_ok;
    QPointer<QPushButton> m_apply;
    QPointer<QPushButton> m_undo;
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
    void setErrorMessage(QString message) { m_message->setMarkdown(message); };

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
    void setCurrentTreeItem(TreeItem* new_current_item);
    bool LoadConfig(CharPtr configFilePath);

    void openConfigSourceDirectly(std::string_view filename, std::string_view line);
    void cleanRecentFilesThatDoNotExist();
    void insertCurrentConfigInRecentFiles(std::string_view cfg);
    void setRecentFiles();

    static auto TheOne() -> MainWindow*;
    static bool IsExisting();

signals:
    void currentItemChanged();

public slots:
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

    static void error(ErrMsgPtr error_message_ptr);
    void exportOkButton();
    void stepToFailReason();
    void runToFailReason();

public slots:
    void fileOpen();
    void reOpen();
    void aboutGeoDms();
    void createView(ViewStyle viewStyle);

    void updateToolbar(QMdiSubWindow* active_mdi_subwindow);
    //void showTreeviewContextMenu(const QPoint& pos);
    void showStatistics() { ShowStatistics(getCurrentTreeItem()); }
    void ShowStatistics(const TreeItem* tiContext);

protected:
    bool event(QEvent* event) override;

private:
    void CloseConfig();
    void setupDmsCallbacks();
    void createActions();
    void createStatusBar();
    void createDetailPagesDock();
    void createRightSideToolbar();
    void createDmsHelperWindowDocks();
    void updateFileMenu();
    void updateWindowMenu();
    void updateCaption();

    static void OnViewAction(const TreeItem* tiContext, CharPtr sAction, Int32 nCode, Int32 x, Int32 y, bool doAddHistory, bool isUrl, bool mustOpenDetailsPage);

    SharedStr m_currConfigFileName;
    SharedPtr<TreeItem> m_root;
    SharedPtr<TreeItem> m_current_item;

    // helper window docks
    QPointer<QDockWidget> m_detailpages_dock;

public:
    // shared actions
    std::unique_ptr<QAction> m_export_primary_data_action;
    std::unique_ptr<QAction> m_step_to_failreason_action;
    std::unique_ptr<QAction> m_go_to_causa_prima_action;
    std::unique_ptr<QAction> m_edit_config_source_action;
    std::unique_ptr<QAction> m_update_treeitem_action;
    std::unique_ptr<QAction> m_update_subtree_action;
    std::unique_ptr<QAction> m_invalidate_action;
    std::unique_ptr<QAction> m_defaultview_action;
    std::unique_ptr<QAction> m_tableview_action;
    std::unique_ptr<QAction> m_mapview_action;
    std::unique_ptr<QAction> m_statistics_action;
    std::unique_ptr<QAction> m_histogramview_action;
    std::unique_ptr<QAction> m_process_schemes_action;
    std::unique_ptr<QAction> m_gui_options_action;
    std::unique_ptr<QAction> m_advanced_options_action;
    std::unique_ptr<QAction> m_config_options_action;
    std::unique_ptr<QAction> m_code_analysis_set_source_action;
    std::unique_ptr<QAction> m_code_analysis_set_target_action;
    std::unique_ptr<QAction> m_code_analysis_add_target_action;
    std::unique_ptr<QAction> m_code_analysis_clr_targets_action;
    std::unique_ptr<QAction> m_quit_action;

    // unique application objects
    std::unique_ptr<QDmsMdiArea> m_mdi_area;
    std::unique_ptr<DmsModel> m_dms_model;
    std::unique_ptr<EventLogModel> m_eventlog_model;
    std::unique_ptr<DmsCurrentItemBar> m_current_item_bar;

    // helper windows; TODO: destroy these before the above model objects
    QPointer<DmsDetailPages> m_detail_pages;
    std::unique_ptr<DmsEventLog> m_eventlog;
    QPointer<DmsTreeView> m_treeview;
    QPointer<QToolBar> m_toolbar;
    std::unique_ptr<QMenu> m_file_menu;
    std::unique_ptr<QMenu> m_edit_menu;
    std::unique_ptr<QMenu> m_view_menu;
    std::unique_ptr<QMenu> m_tools_menu;
    std::unique_ptr<QMenu> m_window_menu;
    std::unique_ptr<QMenu> m_help_menu;
    std::unique_ptr<QMenu> m_code_analysis_submenu;
    QPointer<QToolBar> m_right_side_toolbar;
    std::unique_ptr<QAction> m_general_page_action;
    std::unique_ptr<QAction> m_explore_page_action;
    std::unique_ptr<QAction> m_properties_page_action;
    std::unique_ptr<QAction> m_configuration_page_action;
    std::unique_ptr<QAction> m_sourcedescr_page_action;
    std::unique_ptr<QAction> m_metainfo_page_action;
    std::unique_ptr<QAction> m_eventlog_event_text_filter_toggle;
    std::unique_ptr<QAction> m_eventlog_event_type_filter_toggle;
    QPointer<QMdiSubWindow> m_tooled_mdi_subwindow;
    QPointer<DmsErrorWindow> m_error_window;
    QPointer<DmsOptionsWindow> m_options_window;
    QPointer<DmsFileChangedWindow> m_file_changed_window;

private:
    QList<QAction*> m_CurrWindowActions;
    QList<DmsRecentFileButtonAction*> m_recent_files_actions;
};

#endif
