#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QLineEdit>
#include <QCompleter>
#include <QToolBar>
#include <QTextEdit>

#include "ptr/SharedPtr.h"
#include "ShvUtils.h"
#include "dataview.h"

#include "DockManager.h"
#include "DockAreaWidget.h"
#include "DockWidget.h"

QT_BEGIN_NAMESPACE
class QAction;
class QListWidget;
class QTreeWidget;
class QMenu;
class QTextEdit;
class QTextBrowser;
class QTreeView;
class QTableView;


class QMdiSubWindow;
QT_END_NAMESPACE

struct TreeItem;
class QDmsMdiArea;
class DmsDetailPages;
class DmsTreeView;
class DmsModel;

class DmsCurrentItemBar : public QLineEdit
{
Q_OBJECT
public:
    using QLineEdit::QLineEdit;
    void setDmsCompleter();

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
    ButtonType type;
    QString text;
    std::vector<ToolButtonID> ids;
    std::vector<QString> icons;
};

class DmsToolbuttonAction : public QAction
{
    Q_OBJECT
public:
    DmsToolbuttonAction(const QIcon& icon, const QString& text, QObject* parent = nullptr, ToolbarButtonData button_data = {});

public slots:
    void onToolbuttonPressed();

private:
    ToolbarButtonData m_data;
};

struct CmdLineSetttings {
    bool m_NoConfig = false;
    SharedStr m_ConfigFileName;
    std::vector<SharedStr> m_CurrItemFullNames;
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
    auto getDmsTreeViewPtr() -> QPointer<DmsTreeView> { return m_treeview; }
    auto getDmsMdiAreaPtr() -> QDmsMdiArea* { return m_mdi_area.get(); }
    
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

    static MainWindow* TheOne();
    static void EventLog(SeverityTypeID st, CharPtr msg);

signals:
    void currentItemChanged();

public slots:
    void defaultView();
    void tableView();
    void mapView();
    void openConfigSource();
    void exportPrimaryData();
    void exportOkButton();

private slots:
    void fileOpen();
    void reOpen();
    void aboutGeoDms();
    void createView(ViewStyle viewStyle);

    void updateToolbar(QMdiSubWindow* active_mdi_subwindow);
    //void showTreeviewContextMenu(const QPoint& pos);

private:
    void LoadConfig(CharPtr filename);
    void setupDmsCallbacks();
    void createActions();
    void createStatusBar();
    void createDetailPagesDock();
    void createDetailPagesToolbar();
    void createDmsHelperWindowDocks();
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

    // unique application objects
    std::unique_ptr<QDmsMdiArea> m_mdi_area;
    std::unique_ptr<DmsModel> m_dms_model;
    std::unique_ptr<DmsCurrentItemBar> m_current_item_bar;

    // helper windows
    QPointer<DmsDetailPages> m_detail_pages;
    QPointer<QListWidget> m_eventlog;
    QPointer<DmsTreeView> m_treeview;
    QPointer<QToolBar> m_toolbar;
    QPointer<QMdiSubWindow> m_tooled_mdi_subwindow;
};

#endif
