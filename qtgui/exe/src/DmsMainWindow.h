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
    auto getDmsModel() -> DmsModel* { return m_dms_model.get(); }
    auto getRootTreeItem() -> TreeItem* { return m_root; }
    auto getCurrentTreeItem() -> TreeItem* { return m_current_item; }
    void setCurrentTreeItem(TreeItem* new_current_item);
    auto getDmsTreeViewPtr() -> DmsTreeView*;
    auto getEventLogViewPtr() -> QListView* { return m_eventlog;  }
    auto getDmsMdiAreaPtr() -> QDmsMdiArea* { return m_mdi_area.get(); }
    auto getDmsToolbarPtr() -> QToolBar* { return m_toolbar; }
    auto getExportPrimaryDataAction() -> QAction* { return m_export_primary_data_action.get(); };
    auto getStepToFailreasonAction() -> QAction* { return m_step_to_failreason_action.get(); };
    auto getGoToCausaPrimaAction() -> QAction* { return m_go_to_causa_prima_action.get(); };
    auto getEditConfigSourceAction() -> QAction* { return m_edit_config_source_action.get(); };
    auto getUpdateTreeItemAction() -> QAction* { return m_update_treeitem_action.get(); };
    auto getUpdateSubtreeAction() -> QAction* { return m_update_subtree_action.get(); };
    auto getInvalidateAction() -> QAction* { return m_invalidate_action.get(); };
    auto getDefaultviewAction() -> QAction* { return m_defaultview_action.get(); };
    auto getTableviewAction() -> QAction* { return m_tableview_action.get(); };
    auto getMapviewAction() -> QAction* { return m_mapview_action.get(); };
    auto getHistogramAction() -> QAction* { return m_histogramview_action.get(); };
    auto getProcessSchemeAction() -> QAction* { return m_process_schemes_action.get(); };
    auto getCodeAnalysisAction() -> QAction* { return m_code_analysis_action.get(); };
    void openConfigSourceDirectly(std::string_view filename, std::string_view line);

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
    void options();
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

protected:
    bool event(QEvent* event) override;

private:
    void CloseConfig();
    bool LoadConfig(CharPtr configFilePath);
    void setupDmsCallbacks();
    void createActions();
    void createStatusBar();
    void createDetailPagesDock();
    void createDetailPagesToolbar();
    void createDmsHelperWindowDocks();
    void updateWindowMenu();
    void updateCaption();

    static void OnViewAction(const TreeItem* tiContext, CharPtr sAction, Int32 nCode, Int32 x, Int32 y, bool doAddHistory, bool isUrl, bool mustOpenDetailsPage);

    SharedStr m_currConfigFileName;
    SharedPtr<TreeItem> m_root;
    SharedPtr<TreeItem> m_current_item;

    // helper window docks
    QPointer<QDockWidget> m_detailpages_dock;

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
    std::unique_ptr<QAction> m_histogramview_action;
    std::unique_ptr<QAction> m_process_schemes_action;
    std::unique_ptr<QAction> m_code_analysis_action;
    std::unique_ptr<QAction> m_options_action;

public:
    // unique application objects
    std::unique_ptr<QDmsMdiArea> m_mdi_area;
    std::unique_ptr<DmsModel> m_dms_model;
    std::unique_ptr<EventLogModel> m_eventlog_model;
    std::unique_ptr<DmsCurrentItemBar> m_current_item_bar;

    // helper windows; TODO: destroy these before the above model objects
    QPointer<DmsDetailPages> m_detail_pages;
    QPointer<QListView> m_eventlog;
    QPointer<DmsTreeView> m_treeview;
    QPointer<QToolBar> m_toolbar;
    QPointer<QMenu> m_window_menu;
    QPointer<QMdiSubWindow> m_tooled_mdi_subwindow;
    QPointer<DmsErrorWindow> m_error_window;
    QPointer<DmsOptionsWindow> m_options_window;
    QPointer<DmsFileChangedWindow> m_file_changed_window;
};

#endif
