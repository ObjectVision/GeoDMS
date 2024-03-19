#include "RtcBase.h"
#include "DmsViewArea.h"

#include "DmsMainWindow.h"
#include "DmsTreeView.h"

#include "QEvent.h"
#include <QLabel>
#include <QMdiArea>
#include <QMimeData>
#include <QTimer>

//#include <windows.h>
#include <ShellScalingApi.h>

#include "dbg/SeverityType.h"

#include "ShvDllInterface.h"
#include "DataView.h"
#include "KeyFlags.h"

LRESULT CALLBACK DataViewWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    DBG_START("DataViewWndProc", "", MG_DEBUG_WNDPROC);
    DBG_TRACE(("msg: %x(%x, %x)", uMsg, wParam, lParam));

    if (uMsg == WM_CREATE)
    {
        LPVOID lpCreateParams = ((LPCREATESTRUCT)lParam)->lpCreateParams;
        DataView* view = reinterpret_cast<DataView*>(lpCreateParams);
        SetWindowLongPtr(hWnd, 0, (LONG_PTR)view);
    }

    DataView* view = reinterpret_cast<DataView*>(GetWindowLongPtr(hWnd, 0)); // retrieve pointer to DataView obj.

    LRESULT result = 0;
    if (view && SHV_DataView_DispatchMessage(view, hWnd, uMsg, wParam, lParam, &result))
        return result;

    if (uMsg == WM_DESTROY)
    {
        SHV_DataView_Destroy(view); // delete view; 
        view = 0;
        SetWindowLongPtr(hWnd, 0, (LONG_PTR)view);
    }
    if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST)
    {
        if (uMsg == WM_KEYDOWN && wParam == 'W')
            if (GetKeyState(VK_CONTROL) & 0x8000) 
                if (not (GetKeyState(VK_SHIFT) & 0x8000))
                    if (not (GetKeyState(VK_MENU) & 0x8000))
                        wParam = VK_F4;

        HWND parent = (HWND)GetWindowLongPtr(hWnd, GWLP_HWNDPARENT);
        if (parent)
            return SendMessage(parent, uMsg, wParam, lParam);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LPCWSTR RegisterViewAreaWindowClass(HINSTANCE instance)
{
    LPCWSTR className = L"DmsView";
    WNDCLASSEX wndClassData;
    wndClassData.cbSize = sizeof(WNDCLASSEX);
    wndClassData.style = CS_DBLCLKS; // CS_OWNDC
    wndClassData.lpfnWndProc = &DataViewWndProc;
    wndClassData.cbClsExtra = 0;
    wndClassData.cbWndExtra = sizeof(DataView*);
    wndClassData.hInstance = instance;
    wndClassData.hIcon = NULL;
    wndClassData.hCursor = NULL;
    wndClassData.hbrBackground = HBRUSH(COLOR_WINDOW + 1);
    wndClassData.lpszMenuName = NULL;
    wndClassData.lpszClassName = className;
    wndClassData.hIconSm = NULL;

    RegisterClassEx(&wndClassData);
    return className;
}

void DMS_CONV OnStatusText(void* clientHandle, SeverityTypeID st, CharPtr msg)
{
    auto* dva = reinterpret_cast<QDmsViewArea*>(clientHandle);
    assert(dva);
    if (st == SeverityTypeID::ST_MajorTrace)
    {
        dva->setWindowTitle(msg);
    }
    else
    {
        MainWindow::TheOne()->m_statusbar_coordinates->setText(QString(msg));
    }
}

QDmsMdiArea::QDmsMdiArea(QWidget* parent)
    : QMdiArea(parent)
{
    setTabbedViewModeStyle();
    setTabsClosable(true);
    setAcceptDrops(true);

    auto mdi_children = this->children();
    for (QObjectList::Iterator i = mdi_children.begin(); i != mdi_children.end(); ++i)
    {
        if (QString((*i)->metaObject()->className()) == "QTabBar")
        {
            auto mdi_area_tabbar = dynamic_cast<QTabBar*>(*i);
            mdi_area_tabbar->setElideMode(Qt::ElideMiddle);
            mdi_area_tabbar->setSelectionBehaviorOnRemove(QTabBar::SelectionBehavior::SelectPreviousTab);
            break;
        }
    }

}

void QDmsMdiArea::dragEnterEvent(QDragEnterEvent* event)
{
    event->acceptProposedAction(); // TODO: further specify that only treenodes dragged from treeview can be dropped here.
}

bool processUrlOfDropEvent(QList<QUrl> urls)
{
    for (auto& url : urls)
    {
        auto local_file = url.toLocalFile();
        if (QFileInfo(local_file).suffix() != "dms")
            continue;

        MainWindow::TheOne()->LoadConfig(local_file.toUtf8());
        return true;
    }
    return false;
}

void QDmsMdiArea::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls())
    {
        QList<QUrl> urls = mimeData->urls();
        if (processUrlOfDropEvent(urls))
            return;
    }

    auto tree_view = MainWindow::TheOne()->m_treeview;
    auto source = qobject_cast<DmsTreeView*>(event->source());
    if (tree_view != source)
        return;

    MainWindow::TheOne()->defaultView();
}

void QDmsMdiArea::closeAllButActiveSubWindow()
{
    auto asw = activeSubWindow();
    if (!asw)
        return;

    for (auto swPtr : subWindowList())
        if (swPtr != asw)
            swPtr->close();
}

void QDmsMdiArea::setTabbedViewModeStyle()
{
    setViewMode(QMdiArea::ViewMode::TabbedView);
    QTabBar* mdi_tabbar = findChild<QTabBar*>();
    if (mdi_tabbar)
    {
        mdi_tabbar->setExpanding(false);
        mdi_tabbar->setStyleSheet("QTabBar::tab:selected{"
                                    "background-color: rgb(137, 207, 240);"
                                    "}");
    }
}

QSize QDmsMdiArea::sizeHint() const
{
    return QSize(500, 0);
}

void QDmsMdiArea::onCascadeSubWindows()
{
    setViewMode(QMdiArea::ViewMode::SubWindowView);
    cascadeSubWindows();
}

void QDmsMdiArea::onTileSubWindows()
{
    setViewMode(QMdiArea::ViewMode::SubWindowView);
    tileSubWindows();
}

QDmsViewArea::QDmsViewArea(QMdiArea* parent, TreeItem* viewContext, const TreeItem* currItem, ViewStyle viewStyle)
    : QMdiSubWindow(parent)
{
    assert(currItem); // Precondition
    setAcceptDrops(true);
    //setUpdatesEnabled(false);
    //setAttribute(Qt::WA_OpaquePaintEvent, true);
    //setAttribute(Qt::WA_NoSystemBackground, true);

    m_DataView = SHV_DataView_Create(viewContext, viewStyle, ShvSyncMode::SM_Load);
    if (!m_DataView)
        throwErrorF("CreateView", "Cannot create view with style %s with context '%s'"
            , GetViewStyleName(viewStyle)
            , viewContext->GetFullName().c_str()
        );

    ObjectMsgGenerator thisMsgGenerator(currItem, "CreateDmsView");
    Waiter showWaitingStatus(&thisMsgGenerator);

    CreateDmsView(parent, viewStyle);
    // SHV_DataView_AddItem can call ClassifyJenksFisher, which requires DataView with a m_hWnd, so this must be after CreateWindowEx
    // or PostMessage(WM_PROCESS_QUEUE, ...) directly here to trigger DataView::ProcessGuiOpers()
    try 
    {
        auto current_item = MainWindow::TheOne()->getCurrentTreeItem();
        m_DataView->AddLayer(currItem, false);
        if (m_DataView->GetViewType()== ViewStyle::tvsMapView)
            reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "Add layer for item %s in %s", current_item->GetFullName(), m_DataView->GetCaption());
        else
            reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "Add column for item %s in %s", current_item->GetFullName(), m_DataView->GetCaption());
    }
    catch (...) {
        CloseWindow((HWND)m_DataViewHWnd); // calls SHV_DataView_Destroy
        throw;
    }
}

QDmsViewArea::QDmsViewArea(QMdiArea* parent, MdiCreateStruct* createStruct)
    :   QMdiSubWindow(parent)
    ,   m_DataView(createStruct->dataView)
{
    //setUpdatesEnabled(false);
    setWindowTitle(createStruct->caption);
    CreateDmsView(parent, createStruct->ct);
    createStruct->hWnd = (HWND)m_DataViewHWnd;
}

void QDmsViewArea::CreateDmsView(QMdiArea* parent, ViewStyle viewStyle)
{
    setAttribute(Qt::WA_DeleteOnClose);
    //    setAttribute(Qt::WA_Mapped);
    //    setAttribute(Qt::WA_PaintOnScreen);
    //    setAttribute(Qt::WA_NoSystemBackground);
    //    setAttribute(Qt::WA_OpaquePaintEvent);

    HWND hWndMain = (HWND)MainWindow::TheOne()->winId();

    HINSTANCE instance = GetInstance(hWndMain);
    auto parent_hwnd = (HWND)winId();
    auto rect = contentsRectInPixelUnits();
    if (rect.width() < 200) rect.setWidth(200);
    if (rect.height() < 100) rect.setHeight(100);

    static LPCWSTR dmsViewAreaClassName = RegisterViewAreaWindowClass(instance); // I say this only once
    auto vs = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN; //  viewStyle == tvsMapView ? WS_DLGFRAME | WS_CHILD : WS_CHILD;
    auto dv_hWnd = CreateWindowEx(
        WS_EX_OVERLAPPEDWINDOW,          // extended style
        dmsViewAreaClassName,            // DmsDataView control class 
        nullptr,                         // text for window title bar 
        vs,                              // styles
        rect.x(), rect.y(),              // position
        rect.width(), rect.height(),     // size
        parent_hwnd,                     // handle to parent (MainWindow)
        nullptr,                         // no menu
        instance,                        // instance owning this window 
        m_DataView
    );
    m_DataViewHWnd = dv_hWnd;

    SHV_DataView_SetStatusTextFunc(m_DataView, this, OnStatusText); // to communicate title etc.
    SetWindowPos(dv_hWnd, HWND_TOP
        , rect.x(), rect.y()
        , rect.width(), rect.height()
        , SWP_SHOWWINDOW
    );
    parent->addSubWindow(this);
    this->setWindowIcon(MainWindow::TheOne()->getIconFromViewstyle(viewStyle));
    if (parent->subWindowList().size() == 1)
        showMaximized();

    setMinimumSize(200, 150);
    show();

    RegisterScaleChangeNotifications(DEVICE_PRIMARY, parent_hwnd, WM_APP + 2, &m_cookie);

    setProperty("viewstyle", viewStyle);

    QTimer::singleShot(0, this, [dv_hWnd] { SetFocus(dv_hWnd); });
}

QDmsViewArea::~QDmsViewArea()
{
    RevokeScaleChangeNotifications(DEVICE_PRIMARY, m_cookie);
    CloseWindow((HWND)m_DataViewHWnd); // calls SHV_DataView_Destroy

    if (!MainWindow::IsExisting())
        return;

    auto main_window = MainWindow::TheOne();

    auto active_subwindow = main_window->m_mdi_area->activeSubWindow();
    if (!active_subwindow)
    {
        main_window->scheduleUpdateToolbar();
        main_window->m_current_toolbar_style = ViewStyle::tvsUndefined;
    }
}

void QDmsViewArea::dragEnterEvent(QDragEnterEvent* event)
{
    event->acceptProposedAction(); // TODO: further specify that only treenodes dragged from treeview can be dropped here.
}

void QDmsViewArea::closeEvent(QCloseEvent* event)
{
    auto main_window = MainWindow::TheOne();
    auto number_of_active_subwindows = main_window->m_mdi_area->subWindowList().size();
    if (number_of_active_subwindows==1) // the window that is about to be closed is the only window, set coordinate widget to disabled
    {
        main_window->m_statusbar_coordinates->setVisible(false);
    }
    QMdiSubWindow::closeEvent(event);
}

void QDmsViewArea::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls())
    {
        QList<QUrl> urls = mimeData->urls();
        if (processUrlOfDropEvent(urls))
            return;
    }
    auto tree_view = MainWindow::TheOne()->m_treeview;
    auto source = qobject_cast<DmsTreeView*>(event->source());
    if (tree_view != source)
        return;

    auto current_item = MainWindow::TheOne()->getCurrentTreeItem();
    if (m_DataView->CanContain(current_item))
        SHV_DataView_AddItem(m_DataView, MainWindow::TheOne()->getCurrentTreeItem(), false);
    else
        reportF(MsgCategory::commands, SeverityTypeID::ST_Error, "Item %s is incompatible with view: %s", current_item->GetFullName(), m_DataView->GetCaption());
}

void QDmsViewArea::moveEvent(QMoveEvent* event)
{
    base_class::moveEvent(event);
    UpdatePosAndSize();
}

void QDmsViewArea::resizeEvent(QResizeEvent* event)
{
    base_class::resizeEvent(event);
    UpdatePosAndSize();
}

/*bool QDmsViewArea::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::Paint) 
        return true;
    return false;
}*/

auto QDmsViewArea::contentsRectInPixelUnits()->QRect
{
    auto wId = winId();
    assert(wId);
    auto scaleFactors = GetWindowDip2PixFactors(reinterpret_cast<HWND>(wId));
    auto rect = contentsRect();
    QPoint topLeft (rect.left () * scaleFactors.first, rect.top   () * scaleFactors.second);
    QPoint botRight(rect.right() * scaleFactors.first, rect.bottom() * scaleFactors.second);
    return QRect(topLeft, botRight);
}

void QDmsViewArea::keyPressEvent(QKeyEvent* keyEvent)
{
    if (m_DataView->OnKeyDown(keyEvent->key() | KeyInfo::Flag::Char))
        return;
    QMdiSubWindow::keyPressEvent(keyEvent);
}

void QDmsViewArea::UpdatePosAndSize()
{
    auto rect = contentsRectInPixelUnits();

    MoveWindow((HWND)m_DataViewHWnd
        , rect.x(), rect.y()
        , rect.width (), rect.height()
        , true
    );
}

void QDmsViewArea::on_rescale()
{
    auto rect = contentsRectInPixelUnits();

    m_DataView->InvalidateDeviceRect(GRect(rect.left(), rect.top(), rect.right(), rect.bottom()));
}

void QDmsViewArea::paintEvent(QPaintEvent* event)
{
    auto wId = winId();
    assert(wId);
    auto currScaleFactors = GetWindowDip2PixFactors(reinterpret_cast<HWND>(wId));
    if (currScaleFactors != m_LastScaleFactors)
    { 
        m_LastScaleFactors = currScaleFactors;
        on_rescale();
    }
    return QMdiSubWindow::paintEvent(event);
}

void QDmsViewArea::onWindowStateChanged(Qt::WindowStates oldState, Qt::WindowStates newState)
{
    if (!(newState & Qt::WindowState::WindowMaximized))
        return;

    auto mdi_area = MainWindow::TheOne()->m_mdi_area.get();
    mdi_area->setTabbedViewModeStyle();
}