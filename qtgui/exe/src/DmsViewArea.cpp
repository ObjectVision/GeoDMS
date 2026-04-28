#include "RtcBase.h"
#include "act/TriggerOperator.h"
#include "DmsViewArea.h"

#include "DmsMainWindow.h"
#include "DmsTreeView.h"
#include "QtDrawContext.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPaintEvent>
#include <QFileInfo>
#include <QLabel>
#include <QMdiArea>
#include <QMimeData>
#include <QTimer>
#include <QCursor>
#include <QMenu>
#include <QPainter>
#include <QToolTip>
#include <QScreen>
#include <QGuiApplication>
#include <QClipboard>
#include <QApplication>

#ifdef _WIN32
#include <ShellScalingApi.h>
#endif

#include "dbg/SeverityType.h"
#include "Region.h"
#include "GdiRegionUtil.h"

#include "ShvDllInterface.h"
#include "DataView.h"
#include "ScrollPort.h"
#include "act/MainThread.h"
#include "KeyFlags.h"
#include "MenuData.h"
#include "AbstrController.h"
#include "ShvCompat.h"

// Portable ShowWindow constants (defined in windows.h on Win32)
#ifndef SW_HIDE
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#endif

#ifdef _WIN32
LRESULT CALLBACK DataViewWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    DBG_START("DataViewWndProc", "", MG_DEBUG_WNDPROC);
    DBG_TRACE(("msg: %x(%x, %x)", uMsg, wParam, lParam));

    if (uMsg == WM_CREATE) {
        LPVOID lpCreateParams = ((LPCREATESTRUCT)lParam)->lpCreateParams;
        DataView* view = reinterpret_cast<DataView*>(lpCreateParams);
        SetWindowLongPtr(hWnd, 0, (LONG_PTR)view);
    }

    DataView* view = reinterpret_cast<DataView*>(GetWindowLongPtr(hWnd, 0)); // retrieve pointer to DataView obj.

    if (view)
    {
        auto r = SHV_DataView_DispatchMessage(view, hWnd, uMsg, wParam, lParam);
        if (r.handled)
            return r.result;
    }

    if (uMsg == WM_DESTROY) {
        SHV_DataView_Destroy(view); // delete view; 
        view = nullptr;
        SetWindowLongPtr(hWnd, 0, (LONG_PTR)view);
    }
    if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST) {
        HWND parent = (HWND)GetWindowLongPtr(hWnd, GWLP_HWNDPARENT);
        if (parent)
            return SendMessage(parent, uMsg, wParam, lParam);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LPCWSTR RegisterViewAreaWindowClass(HINSTANCE instance) {
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
#endif // _WIN32

void DMS_CONV OnStatusText(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg) {
    auto* dva = reinterpret_cast<QDmsViewArea*>(clientHandle);
    assert(dva);
    if (st == SeverityTypeID::ST_MajorTrace) {
        dva->setWindowTitle(msg);
        MainWindow::TheOne()->m_mdi_area->updateTabTooltips();
    }
    else {
        MainWindow::TheOne()->m_statusbar_coordinates->setText(QString(msg));
    }
}

QDmsMdiArea::QDmsMdiArea(QWidget* parent)
    : QMdiArea(parent) {
    setTabbedViewModeStyle();
    setTabsClosable(true);
    setAcceptDrops(true);

    // set tabbar properties: elide mode, selection behavior on remove
    QTabBar* mdi_tabbar = getTabBar();
    if (!mdi_tabbar)
        return;

    mdi_tabbar->setElideMode(Qt::ElideMiddle);
    mdi_tabbar->setSelectionBehaviorOnRemove(QTabBar::SelectionBehavior::SelectLeftTab);
    mdi_tabbar->setUsesScrollButtons(true);
}

void QDmsMdiArea::updateTabTooltips()
{
    QTabBar* mdi_tabbar = getTabBar();
    if (!mdi_tabbar)
        return;

    auto windows = subWindowList();
    for (int i = 0; i < mdi_tabbar->count() && i < windows.size(); ++i)
    {
        mdi_tabbar->setTabToolTip(i, windows[i]->windowTitle());
    }
}

QMdiSubWindow* QDmsMdiArea::addDmsSubWindow(QWidget* widget)
{
    auto* subWindow = addSubWindow(widget);
    updateTabTooltips();
    return subWindow;
}

void QDmsMdiArea::dragEnterEvent(QDragEnterEvent* event) {
    event->acceptProposedAction(); // TODO: further specify that only treenodes dragged from treeview can be dropped here.
}

bool processUrlOfDropEvent(QList<QUrl> urls)
{
    for (auto& url : urls) {
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
    if (mimeData->hasUrls()) {
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
    QTabBar* mdi_tabbar = getTabBar();
    if (mdi_tabbar) {
        mdi_tabbar->setExpanding(false);
        mdi_tabbar->setStyleSheet("QTabBar::tab:selected{"
                                    "background-color: rgb(137, 207, 240);"
                                    "}");
    }
}

auto QDmsMdiArea::getTabBar() -> QTabBar*
{
	return findChild<QTabBar*>();
}

QSize QDmsMdiArea::sizeHint() const
{
    return QSize(500, 0);
}

void QDmsMdiArea::testCloseSubWindow()
{
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

void QDmsMdiArea::closeActiveDmsSubWindow()
{
	activatePreviousSubWindow();
	closeActiveSubWindow();
}

QDmsViewArea::QDmsViewArea(QMdiArea* parent, TreeItem* viewContext, const TreeItem* currItem, ViewStyle viewStyle)
	: QMdiSubWindow(parent)  // pass parent so Qt auto-registers via childEvent; we do NOT call addSubWindow() separately
{
	assert(currItem); // Precondition
	setAcceptDrops(true);

	m_DataView = SHV_DataView_Create(viewContext, viewStyle, ShvSyncMode::SM_Load);
	auto dv = m_DataView.lock();
	if (!dv)
		throwErrorF("CreateView", "Cannot create view with style %s with context '%s'"
			, GetViewStyleName(viewStyle)
			, viewContext->GetFullName().c_str()
		);

	ObjectMsgGenerator thisMsgGenerator(currItem, "CreateDmsView");
	Waiter showWaitingStatus(&thisMsgGenerator);

	CreateDmsView(parent, viewStyle);
	// SHV_DataView_AddItem can call ClassifyJenksFisher, which requires DataView with a m_hWnd, so this must be after CreateWindowEx
	// or PostMessage(UM_PROCESS_QUEUE, ...) directly here to trigger DataView::ProcessGuiOpers()
	try {
		auto current_item = MainWindow::TheOne()->getCurrentTreeItem();
		dv->AddLayer(currItem, false);
		if (dv->GetViewType()== ViewStyle::tvsMapView)
			reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "Add layer for item %s in %s", current_item->GetFullName(), dv->GetCaption());
		else
			reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "Add column for item %s in %s('%s')"
				, current_item->GetFullName()
				, GetViewStyleName(viewStyle)
				, dv->GetCaption()
			);
	}
	catch (...) {
#ifdef _WIN32
		CloseWindow((HWND)m_DataViewHWnd); // calls SHV_DataView_Destroy
#else
		// On Linux, DataView cleanup is handled differently
		auto dv = m_DataView.lock();
		if (dv)
			SHV_DataView_Destroy(dv.get());
#endif
		throw;
	}
}

QDmsViewArea::QDmsViewArea(QMdiArea* parent, MdiCreateStruct* createStruct)
	:   QMdiSubWindow(parent)  // pass parent so Qt auto-registers via childEvent; we do NOT call addSubWindow() separately
	,   m_DataView(createStruct->dataView->shared_from_this())
{
	//setUpdatesEnabled(false);
	setWindowTitle(createStruct->caption);
	CreateDmsView(parent, createStruct->ct);
#ifdef _WIN32
	createStruct->hWnd = (HWND)m_DataViewHWnd;
#endif
}

void QDmsViewArea::CreateDmsView(QMdiArea* parent, ViewStyle viewStyle)
{
	setAttribute(Qt::WA_DeleteOnClose);
#ifndef _WIN32
    setMouseTracking(true); // receive mouseMoveEvent even without button pressed (for SETCURSOR/hover)
#endif
    //    setAttribute(Qt::WA_Mapped);
    //    setAttribute(Qt::WA_PaintOnScreen);
    //    setAttribute(Qt::WA_NoSystemBackground);
    //    setAttribute(Qt::WA_OpaquePaintEvent);

    auto dv = m_DataView.lock(); MG_CHECK(dv);

#ifdef _WIN32
    // Win32: Create native HWND child window for GDI rendering
    HWND hWndMain = (HWND)MainWindow::TheOne()->winId();
    HINSTANCE instance = GetInstance(hWndMain);
    auto parent_hwnd = (HWND)this->winId();
    auto rect = contentsRectInPixelUnits();
    if (rect.width() < 200) rect.setWidth(200);
    if (rect.height() < 100) rect.setHeight(100);

    static LPCWSTR dmsViewAreaClassName = RegisterViewAreaWindowClass(instance); // I say this only once
    auto vs = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
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
        dv.get()
    );
    m_DataViewHWnd = dv_hWnd;
#endif

    // Use QDmsViewArea as ViewHost (implements ViewHost interface with Qt)
    SHV_DataView_SetViewHost(dv.get(), this);
    SHV_DataView_SetStatusTextFunc(dv.get(), this, OnStatusText);

#ifdef _WIN32
    SetWindowPos(dv_hWnd, HWND_TOP
        , rect.x(), rect.y()
        , rect.width(), rect.height()
        , SWP_SHOWWINDOW
    );
#endif

    // parent->addSubWindow(this) is intentionally omitted: the QMdiSubWindow(parent) constructor
    // already registered this window via QMdiArea::childEvent. Calling addSubWindow() again
    // triggers "window is already added" and heap corruption in glibc.
    this->setWindowIcon(MainWindow::TheOne()->getIconFromViewstyle(viewStyle));
    if (parent->subWindowList().size() == 1)
        showMaximized();

    setMinimumSize(200, 150);
    show();

#ifdef _WIN32
    auto parent_hwnd_for_scale = (HWND)this->winId();
    RegisterScaleChangeNotifications(DEVICE_PRIMARY, parent_hwnd_for_scale, UM_SCALECHANGE, &m_cookie);
#endif

    setProperty("viewstyle", viewStyle);

#ifdef _WIN32
    QTimer::singleShot(0, this, 
        [dv_hWnd]()
        { 
            SetFocus(dv_hWnd); 
        }
    );
#else
    // On Linux, set focus to this Qt widget
    QTimer::singleShot(0, this, [this]() { setFocus(); });
#endif
}

QDmsViewArea::~QDmsViewArea()
{
    // Stop all timers before any virtual dispatch can occur during teardown.
    for (auto& [id, timer] : m_Timers) {
        timer->stop();
        delete timer;
    }
    m_Timers.clear();

    // Disconnect ViewHost before destroying DataView.
    // DataView's child objects (MapControl, LayerControl, WmsLayer, etc.) call
    // back into the ViewHost from their own destructors or from async callbacks
    // (WMS tile loaders, queued Qt lambdas that may fire after the QDmsViewArea
    // is fully destroyed).  Clearing m_ViewHost to nullptr here ensures that all
    // `if (m_ViewHost)` guards in dataview.cpp are false, preventing any virtual
    // dispatch into an already-destroyed or partially-destroyed QDmsViewArea.
    auto dv = m_DataView.lock();
    if (dv)
        SHV_DataView_SetViewHost(dv.get(), nullptr);

#ifdef _WIN32
    RevokeScaleChangeNotifications(DEVICE_PRIMARY, m_cookie);
    CloseWindow((HWND)m_DataViewHWnd); // calls SHV_DataView_Destroy
#else
    if (dv)
        SHV_DataView_Destroy(dv.get());
#endif

    if (!MainWindow::IsExisting())
        return;

    auto main_window = MainWindow::TheOne();

    auto active_subwindow = main_window->m_mdi_area->activeSubWindow();
    if (!active_subwindow) {
        main_window->scheduleUpdateToolbar();
        main_window->m_current_toolbar_style = ViewStyle::tvsUndefined;
    }
}

#ifdef _WIN32
bool QDmsViewArea::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) 
{
    MSG* msg = static_cast<MSG*>(message);
    UInt32 received_message_type = msg->message;
    if (received_message_type == WM_QT_ACTIVATENOTIFIERS)
        if (auto main_window = MainWindow::TheOne())
            if (auto mdi_area = main_window->m_mdi_area.get())
                while (true) {
                    auto current_active_subwindow = mdi_area->activeSubWindow();
                    if (!current_active_subwindow)
                        break;

                    if (this == current_active_subwindow)
                        break;

                    mdi_area->activateNextSubWindow();
                }
    return false;
}
#endif // _WIN32

void QDmsViewArea::dragEnterEvent(QDragEnterEvent* event) {
    event->acceptProposedAction(); // TODO: further specify that only treenodes dragged from treeview can be dropped here.
}

void QDmsViewArea::closeEvent(QCloseEvent* event) {
    auto main_window = MainWindow::TheOne();
    auto number_of_active_subwindows = main_window->m_mdi_area->subWindowList().size();
    if (number_of_active_subwindows==1) { // the window that is about to be closed is the only window, set coordinate widget to disabled
        main_window->m_statusbar_coordinates->setVisible(false);
    }
    QMdiSubWindow::closeEvent(event);
}

void QDmsViewArea::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        if (processUrlOfDropEvent(urls))
            return;
    }
    auto tree_view = MainWindow::TheOne()->m_treeview;
    auto source = qobject_cast<DmsTreeView*>(event->source());
    if (tree_view != source)
        return;
    auto dv = getDataView();
    if (!dv)
        return;

    auto current_item = MainWindow::TheOne()->getCurrentTreeItem();
    if (dv->CanContain(current_item))
        SHV_DataView_AddItem(dv.get(), MainWindow::TheOne()->getCurrentTreeItem(), false);
    else
        reportF(MsgCategory::commands, SeverityTypeID::ST_Error, "Item %s is incompatible with view: %s", current_item->GetFullName(), dv->GetCaption());
}

void QDmsViewArea::moveEvent(QMoveEvent* event) {
    base_class::moveEvent(event);
    UpdatePosAndSize();
}

void QDmsViewArea::resizeEvent(QResizeEvent* event) {
    base_class::resizeEvent(event);
    UpdatePosAndSize();
#ifndef _WIN32
    repositionScrollBars();
#endif
}

auto QDmsViewArea::contentsRectInPixelUnits()->QRect {
    auto rect = contentsRect();
#ifdef _WIN32
    auto wId = winId();
    assert(wId);
    auto scaleFactors = GetWindowDip2PixFactors(reinterpret_cast<HWND>(wId));
    QPoint topLeft (rect.left () * scaleFactors.first, rect.top   () * scaleFactors.second);
    QPoint botRight(rect.right() * scaleFactors.first, rect.bottom() * scaleFactors.second);
    return QRect(topLeft, botRight);
#else
    // On Linux, use Qt's device pixel ratio
    qreal dpr = devicePixelRatioF();
    QPoint topLeft (rect.left () * dpr, rect.top   () * dpr);
    QPoint botRight(rect.right() * dpr, rect.bottom() * dpr);
    return QRect(topLeft, botRight);
#endif
}

void QDmsViewArea::UpdatePosAndSize() {
#ifdef _WIN32
    auto rect = contentsRectInPixelUnits();
    MoveWindow((HWND)m_DataViewHWnd
        , rect.x(), rect.y()
        , rect.width (), rect.height()
        , true
    );
#else
    // On Linux, the Qt widget handles its own geometry
    // Notify DataView of the new size via invalidation
    auto rect = contentsRect();
    auto dv = getDataView();
    if (dv) {
        GPoint deviceSize(rect.width(), rect.height());
        auto sf = devicePixelRatioF();
        dv->OnResize(deviceSize, CrdPoint(sf, sf));
        // On Win32, WM_PAINT → OnPaint() seeds m_DoneGraphics so UpdateView() draws.
        // On Linux, OnPaint() never fires, so seed it explicitly here.
        dv->SeedDrawRegion();
        dv->RequestUpdate(); // kick off the render cycle (OnPaint not called on Linux)
    }
#endif
}

void QDmsViewArea::on_rescale() {
    auto rect = contentsRectInPixelUnits();
    auto dv = getDataView();
    if (dv)
        dv->InvalidateDeviceRect(GRect(rect.left(), rect.top(), rect.right(), rect.bottom()));
}

#ifndef _WIN32

void QDmsViewArea::repositionScrollBars()
{
    auto cr = contentsRect();
    int W = cr.width(), H = cr.height();
    const int SW = 16; // scroll bar width/height in logical pixels
    bool hasH = m_hScrollBar != nullptr;
    bool hasV = m_vScrollBar != nullptr;
    if (m_hScrollBar)
        m_hScrollBar->setGeometry(cr.left(), cr.top() + H - SW, W - (hasV ? SW : 0), SW);
    if (m_vScrollBar)
        m_vScrollBar->setGeometry(cr.left() + W - SW, cr.top(), SW, H - (hasH ? SW : 0));
}

void QDmsViewArea::VH_SetHScrollBar(bool visible, int pos, int page, int max, int tick)
{
    if (visible) {
        if (!m_hScrollBar) {
            m_hScrollBar = new QScrollBar(Qt::Horizontal, this);
            connect(m_hScrollBar, &QScrollBar::valueChanged, this, &QDmsViewArea::onHScrollValueChanged);
            repositionScrollBars();
            m_hScrollBar->show();
        }
        m_hScrollTick = tick > 0 ? tick : 1;
        m_hScrollBar->blockSignals(true);
        m_hScrollBar->setRange(0, max);
        m_hScrollBar->setPageStep(page);
        m_hScrollBar->setValue(pos);
        m_hScrollBar->blockSignals(false);
    } else {
        if (m_hScrollBar) {
            delete m_hScrollBar;
            m_hScrollBar = nullptr;
            repositionScrollBars();
        }
    }
}

void QDmsViewArea::VH_SetVScrollBar(bool visible, int pos, int page, int max, int tick)
{
    if (visible) {
        if (!m_vScrollBar) {
            m_vScrollBar = new QScrollBar(Qt::Vertical, this);
            connect(m_vScrollBar, &QScrollBar::valueChanged, this, &QDmsViewArea::onVScrollValueChanged);
            repositionScrollBars();
            m_vScrollBar->show();
        }
        m_vScrollTick = tick > 0 ? tick : 1;
        m_vScrollBar->blockSignals(true);
        m_vScrollBar->setRange(0, max);
        m_vScrollBar->setPageStep(page);
        m_vScrollBar->setValue(pos);
        m_vScrollBar->blockSignals(false);
    } else {
        if (m_vScrollBar) {
            delete m_vScrollBar;
            m_vScrollBar = nullptr;
            repositionScrollBars();
        }
    }
}

void QDmsViewArea::onHScrollValueChanged(int value)
{
    auto dv = getDataView(); if (!dv) return;
    auto sp = dv->GetScrollEventsReceiver(); if (!sp || !sp->GetContents()) return;
    auto currY = sp->GetContents()->GetCurrClientRelPos().Y();
    sp->ScrollLogicalTo(shp2dms_order<CrdType>(-CrdType(value) * m_hScrollTick, currY));
}

void QDmsViewArea::onVScrollValueChanged(int value)
{
    auto dv = getDataView(); if (!dv) return;
    auto sp = dv->GetScrollEventsReceiver(); if (!sp || !sp->GetContents()) return;
    auto currX = sp->GetContents()->GetCurrClientRelPos().X();
    sp->ScrollLogicalTo(shp2dms_order<CrdType>(currX, -CrdType(value) * m_vScrollTick));
}

#endif // !_WIN32

void QDmsViewArea::ensureBackingStore(int width, int height)
{
    if (!m_BackingStore || m_BackingStore->width() != width || m_BackingStore->height() != height)
    {
        m_BackingStore = std::make_unique<QImage>(width, height, QImage::Format_ARGB32_Premultiplied);
        m_BackingStore->fill(Qt::white);
    }
}

void QDmsViewArea::scrollBackingStore(int dx, int dy, const QRect& scrollRect)
{
    if (!m_BackingStore)
        return;

    // Create a copy of the region to scroll
    QImage scrolledContent = m_BackingStore->copy(scrollRect);

    // Draw the scrolled content at the new position
    QPainter painter(m_BackingStore.get());
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(scrollRect.translated(dx, dy), scrolledContent);
}

DPoint QDmsViewArea::getScaleFactors() const
{
#ifdef _WIN32
    auto wId = winId();
    if (wId)
        return GetWindowDip2PixFactors(reinterpret_cast<HWND>(wId));
    return DPoint(1.0, 1.0);
#else
    qreal dpr = devicePixelRatioF();
    return DPoint(dpr, dpr);
#endif
}

void QDmsViewArea::paintEvent(QPaintEvent* event) {
    auto currScaleFactors = getScaleFactors();
    if (currScaleFactors != m_LastScaleFactors) {
        m_LastScaleFactors = currScaleFactors;
        on_rescale();
    }

    // On Windows, the native HWND child handles its own painting
    // On Linux, we would paint the backing store here
#ifndef _WIN32
    if (m_BackingStore)
    {
        QPainter painter(this);
        // contentsRect() is in widget coordinates; the backing store uses content-area
        // coordinates (0,0 = top-left of contentsRect).  Translate before drawing so that
        // backing-store pixel (0,0) lands at widget pixel contentsRect().topLeft().
        QRect cr = contentsRect();
        QRect widgetDirty = event->rect().intersected(cr);
        if (!widgetDirty.isEmpty())
        {
            // Convert dirty region to backing-store coordinates for the source rect
            QRect bsRect = widgetDirty.translated(-cr.left(), -cr.top())
                                      .intersected(m_BackingStore->rect());
            if (!bsRect.isEmpty())
                painter.drawImage(widgetDirty.topLeft(), *m_BackingStore, bsRect);
        }

        // Draw caret overlay on top using XOR composition (overlay stored in content-area coords)
        if (m_CaretOverlayVisible && !m_CaretOverlayRegion.isEmpty())
        {
            QRegion widgetOverlay = m_CaretOverlayRegion.translated(cr.topLeft());
            painter.setCompositionMode(QPainter::RasterOp_SourceXorDestination);
            painter.setClipRegion(widgetOverlay);
            painter.fillRect(widgetOverlay.boundingRect(), Qt::white);
        }

        // Draw text caret if visible (position stored in content-area coords)
        if (m_TextCaretVisible)
        {
            painter.setCompositionMode(QPainter::RasterOp_SourceXorDestination);
            painter.fillRect(m_TextCaretPos.x + cr.left(), m_TextCaretPos.y + cr.top(),
                           m_TextCaretWidth, m_TextCaretHeight, Qt::white);
        }
    }
#endif

    return QMdiSubWindow::paintEvent(event);
}

void QDmsViewArea::onWindowStateChanged(Qt::WindowStates oldState, Qt::WindowStates newState) {
    if (!(newState & Qt::WindowState::WindowMaximized))
        return;
    auto mainWindow = MainWindow::TheOne();
    if (!mainWindow)
        return;
    auto mdi_area = mainWindow->m_mdi_area.get();
    if (!mdi_area)
        return;
    mdi_area->setTabbedViewModeStyle();
}

//----------------------------------------------------------------------
// ViewHost implementation (Qt-based, portable)
//----------------------------------------------------------------------

void QDmsViewArea::VH_SetTimer(UInt32 id, UInt32 elapseMs)
{
    VH_KillTimer(id); // Remove any existing timer with this ID

    auto* timer = new QTimer(this);
    timer->setProperty("timerId", id);
    connect(timer, &QTimer::timeout, this, &QDmsViewArea::onTimerTimeout);
    timer->start(elapseMs);
    m_Timers[id] = timer;
}

void QDmsViewArea::VH_KillTimer(UInt32 id)
{
    auto it = m_Timers.find(id);
    if (it != m_Timers.end()) {
        it->second->stop();
        delete it->second;
        m_Timers.erase(it);
    }
}

void QDmsViewArea::onTimerTimeout()
{
    auto* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;

    UInt32 timerId = timer->property("timerId").toUInt();
    auto dv = m_DataView.lock();
    if (dv) {
        // Post timer message to DataView for processing
        // On Win32 this would be WM_TIMER; here we call directly
        dv->OnTimer(timerId);
    }
}

void QDmsViewArea::VH_SetCapture()
{
    grabMouse();
}

void QDmsViewArea::VH_ReleaseCapture()
{
    releaseMouse();
}

void QDmsViewArea::VH_SetFocus()
{
    setFocus(Qt::OtherFocusReason);
}

bool QDmsViewArea::VH_GetCursorScreenPos(GPoint& pos) const
{
    QPoint globalPos = QCursor::pos();
    pos.x = globalPos.x();
    pos.y = globalPos.y();
    return true;
}

GPoint QDmsViewArea::VH_ScreenToClient(GPoint screenPt) const
{
    QPoint local = mapFromGlobal(QPoint(screenPt.x, screenPt.y));
    return GPoint(local.x(), local.y());
}

GPoint QDmsViewArea::VH_ClientToScreen(GPoint clientPt) const
{
    QPoint global = mapToGlobal(QPoint(clientPt.x, clientPt.y));
    return GPoint(global.x(), global.y());
}

void QDmsViewArea::VH_SetGlobalCursorPos(GPoint screenPt)
{
    QCursor::setPos(screenPt.x, screenPt.y);
}

void QDmsViewArea::VH_SetCursorArrow()
{
    setCursor(Qt::ArrowCursor);
}

void QDmsViewArea::VH_SetCursorWait()
{
    setCursor(Qt::WaitCursor);
}

void QDmsViewArea::VH_SetCursor(DmsCursor cursor)
{
    switch (cursor) {
    case DmsCursor::Arrow:         setCursor(Qt::ArrowCursor); break;
    case DmsCursor::IBeam:         setCursor(Qt::IBeamCursor); break;
    case DmsCursor::Cross:         setCursor(Qt::CrossCursor); break;
    case DmsCursor::Hand:          setCursor(Qt::PointingHandCursor); break;
    case DmsCursor::Wait:          setCursor(Qt::WaitCursor); break;
    case DmsCursor::ZoomIn:        setCursor(QCursor(QPixmap(":/res/images/TB_zoom_in.bmp").scaled(24, 24))); break;
    case DmsCursor::ZoomOut:       setCursor(QCursor(QPixmap(":/res/images/TB_zoom_out.bmp").scaled(24, 24))); break;
    case DmsCursor::Pan:           setCursor(Qt::OpenHandCursor); break;
    case DmsCursor::SelectDiamond: setCursor(Qt::CrossCursor); break;
    default:                       setCursor(Qt::ArrowCursor); break;
    }
}

void QDmsViewArea::VH_InvalidateRect(const GRect& rect, bool erase)
{
#ifdef _WIN32
    if (m_DataViewHWnd)
        ::InvalidateRect(reinterpret_cast<HWND>(m_DataViewHWnd), &AsRECT(rect), erase);
    else
#endif
    {
        // rect is in content-area coordinates; translate to widget coordinates for Qt
        QRect cr = contentsRect();
        update(QRect(rect.left + cr.left(), rect.top + cr.top(), rect.Width(), rect.Height()));
    }
}

void QDmsViewArea::VH_InvalidateRgn(const Region& rgn, bool erase)
{
#ifdef _WIN32
    if (m_DataViewHWnd) {
        auto hrgn = RegionToHRGN(rgn); // GdiHandle<HRGN>, auto-deletes on scope exit
        ::InvalidateRgn(reinterpret_cast<HWND>(m_DataViewHWnd), hrgn, erase);
    }
    else
#endif
    {
        // rgn is in content-area coordinates; translate to widget coordinates for Qt
        QRect cr = contentsRect();
        if (rgn.Empty())
            update(cr); // invalidate content area only
        else {
            GRect bbox = rgn.BoundingBox();
            update(QRect(bbox.left + cr.left(), bbox.top + cr.top(), bbox.Width(), bbox.Height()));
        }
    }
}

void QDmsViewArea::VH_ValidateRect(const GRect& rect)
{
#ifdef _WIN32
    if (m_DataViewHWnd)
        ::ValidateRect(reinterpret_cast<HWND>(m_DataViewHWnd), &AsRECT(rect));
#endif
    // No-op in Qt - validation happens after paint
}

void QDmsViewArea::VH_UpdateWindow()
{
#ifdef _WIN32
    if (m_DataViewHWnd)
        ::UpdateWindow(reinterpret_cast<HWND>(m_DataViewHWnd));
    else
#endif
    repaint(); // Synchronous paint, unlike update() which is async
}

void QDmsViewArea::VH_PostMessage(UInt32 msg, UInt64 wParam, Int64 lParam)
{
#ifdef _WIN32
    // On Win32, we can still post to the HWND for transitional code
    if (m_DataViewHWnd) {
        PostMessage(reinterpret_cast<HWND>(m_DataViewHWnd), msg, wParam, lParam);
    }
#else
    if (msg == UM_PROCESS_QUEUE) {
        // Queue a call to ProcessGuiOpers() on the Qt event loop so that
        // the DataView's GUI oper queue (which holds write locks on items
        // like classBreaks) gets drained promptly.
        QMetaObject::invokeMethod(this, [this]() {
            SuspendTrigger::Resume(); // match Win32 nativeEventFilter which Resumes on every event
            auto dv = getDataView();
            if (dv)
                dv->ProcessGuiOpers();
        }, Qt::QueuedConnection);
    }
#endif
}

void QDmsViewArea::VH_SendClose()
{
    close();
}

bool QDmsViewArea::VH_IsVisible() const
{
    return isVisible();
}

UInt32 QDmsViewArea::VH_GetShowCmd() const
{
    if (isMinimized()) return SW_SHOWMINIMIZED;
    if (isMaximized()) return SW_SHOWMAXIMIZED;
    if (!isVisible()) return SW_HIDE;
    return SW_SHOWNORMAL;
}

void QDmsViewArea::VH_CreateTextCaret(int width, int height)
{
    m_TextCaretWidth = width;
    m_TextCaretHeight = height;
    // Qt doesn't have a native caret; we draw it ourselves in paintEvent
}

void QDmsViewArea::VH_DestroyTextCaret()
{
    m_TextCaretVisible = false;
    update(); // Repaint to remove caret
}

void QDmsViewArea::VH_SetTextCaretPos(GPoint pos)
{
    m_TextCaretPos = pos;
    if (m_TextCaretVisible)
        update(); // Repaint to show caret at new position
}

void QDmsViewArea::VH_ShowTextCaret()
{
    m_TextCaretVisible = true;
    update();
}

void QDmsViewArea::VH_HideTextCaret()
{
    m_TextCaretVisible = false;
    update();
}

void QDmsViewArea::VH_TrackMouseLeave()
{
    // Qt handles mouse leave automatically via leaveEvent
    setMouseTracking(true);
}

void QDmsViewArea::VH_NotifyParentActivation()
{
    auto mainWindow = MainWindow::TheOne();
    if (mainWindow) {
        mainWindow->activateWindow();
    }
}

#ifndef _WIN32

static UINT qtModsToMkFlags(Qt::MouseButtons buttons, Qt::KeyboardModifiers mods)
{
    UINT flags = 0;
    if (buttons & Qt::LeftButton)  flags |= MK_LBUTTON;
    if (buttons & Qt::RightButton) flags |= MK_RBUTTON;
    if (buttons & Qt::MiddleButton)flags |= MK_MBUTTON;
    if (mods & Qt::ControlModifier) flags |= MK_CONTROL;
    if (mods & Qt::ShiftModifier)   flags |= MK_SHIFT;
    return flags;
}

// Convert a position relative to the QMdiSubWindow frame to content-area coordinates.
// QMdiSubWindow draws its own title bar and resize frame as part of the widget, so
// event->pos() includes the title bar offset.  DataView expects coordinates relative to
// the content area only (i.e. contentsRect().topLeft() is the origin).
static GPoint toClientPoint(const QPoint& widgetPos, const QRect& contentsRect)
{
    return GPoint(widgetPos.x() - contentsRect.left(),
                  widgetPos.y() - contentsRect.top());
}

void QDmsViewArea::mousePressEvent(QMouseEvent* event)
{
    QRect cr = contentsRect();
    if (!cr.contains(event->pos())) { QMdiSubWindow::mousePressEvent(event); return; }
    auto dv = getDataView(); if (!dv) { QMdiSubWindow::mousePressEvent(event); return; }
    GPoint pt = toClientPoint(event->pos(), cr);
    UINT flags = qtModsToMkFlags(event->buttons(), event->modifiers());
    if (event->button() == Qt::LeftButton)
    {
        VH_NotifyParentActivation();
        dv->DispatchMouseEvent(EventID::LBUTTONDOWN, flags, pt);
    }
    else if (event->button() == Qt::RightButton)
        dv->DispatchMouseEvent(EventID::RBUTTONDOWN, flags, pt);
    event->accept();
}

void QDmsViewArea::mouseReleaseEvent(QMouseEvent* event)
{
    QRect cr = contentsRect();
    if (!cr.contains(event->pos()) && !(event->buttons())) {
        // Release can happen outside contents if mouse was grabbed; still forward if we have capture
    }
    auto dv = getDataView(); if (!dv) { QMdiSubWindow::mouseReleaseEvent(event); return; }
    GPoint pt = toClientPoint(event->pos(), cr);
    UINT flags = qtModsToMkFlags(event->buttons(), event->modifiers());
    if (event->button() == Qt::LeftButton)
    {
        dv->DispatchMouseEvent(EventID::LBUTTONUP, flags, pt);
        VH_ReleaseCapture();
    }
    else if (event->button() == Qt::RightButton)
        dv->DispatchMouseEvent(EventID::RBUTTONUP, flags, pt);
    event->accept();
}

void QDmsViewArea::mouseDoubleClickEvent(QMouseEvent* event)
{
    QRect cr = contentsRect();
    if (!cr.contains(event->pos())) { QMdiSubWindow::mouseDoubleClickEvent(event); return; }
    auto dv = getDataView(); if (!dv) { QMdiSubWindow::mouseDoubleClickEvent(event); return; }
    GPoint pt = toClientPoint(event->pos(), cr);
    UINT flags = qtModsToMkFlags(event->buttons(), event->modifiers());
    if (event->button() == Qt::LeftButton)
        dv->DispatchMouseEvent(EventID::LBUTTONDBLCLK, flags, pt);
    else if (event->button() == Qt::RightButton)
        dv->DispatchMouseEvent(EventID::RBUTTONDBLCLK, flags, pt);
    event->accept();
}

void QDmsViewArea::mouseMoveEvent(QMouseEvent* event)
{
    auto dv = getDataView(); if (!dv) { QMdiSubWindow::mouseMoveEvent(event); return; }
    QRect cr = contentsRect();
    GPoint pt = toClientPoint(event->pos(), cr);
    UINT flags = qtModsToMkFlags(event->buttons(), event->modifiers());
    // MOUSEDRAG when button held, MOUSEMOVE otherwise; SETCURSOR always
    EventID moveEvent = (flags & (MK_LBUTTON|MK_RBUTTON)) ? EventID::MOUSEDRAG : EventID::MOUSEMOVE;
    dv->DispatchMouseEvent(moveEvent, flags, pt);
    if (!dv->DispatchMouseEvent(EventID::SETCURSOR, flags, pt))
        VH_SetCursorArrow();
    event->accept();
}

void QDmsViewArea::wheelEvent(QWheelEvent* event)
{
    auto dv = getDataView(); if (!dv) { QMdiSubWindow::wheelEvent(event); return; }
    QRect cr = contentsRect();
    GPoint pt = toClientPoint(event->position().toPoint(), cr);
    // Pack delta (in 120-units) into HIWORD of flags, buttons in LOWORD — matches Win32 WM_MOUSEWHEEL wParam
    int delta = event->angleDelta().y(); // positive = scroll up = zoom in
    UINT flags = qtModsToMkFlags(event->buttons(), event->modifiers());
    UINT wParam = (UINT)(((short)delta) << 16) | (flags & 0xFFFF);
    dv->DispatchMouseEvent(EventID::MOUSEWHEEL, wParam, pt);
    event->accept();
}

void QDmsViewArea::leaveEvent(QEvent* event)
{
    auto dv = getDataView();
    if (dv)
        dv->DispatchMouseEvent(EventID::MOUSEMOVE, 0, UNDEFINED_VALUE(GPoint));
    QMdiSubWindow::leaveEvent(event);
}

#endif // !_WIN32

void QDmsViewArea::VH_ScrollWindow(GPoint delta, const GRect& scrollRect, const GRect& clipRect,
    Region& updateRgn, const GRect& validRect)
{
#ifdef _WIN32
    // Use Win32 ScrollWindowEx for efficiency on Windows
    if (m_DataViewHWnd) {
        RECT rcScroll = { scrollRect.left, scrollRect.top, scrollRect.right, scrollRect.bottom };
        RECT rcClip = { clipRect.left, clipRect.top, clipRect.right, clipRect.bottom };
        HRGN hrgnUpdate = CreateRectRgn(0, 0, 0, 0);
        ScrollWindowEx(
            reinterpret_cast<HWND>(m_DataViewHWnd),
            delta.x, delta.y,
            &rcScroll, &rcClip,
            hrgnUpdate, nullptr,
            SW_INVALIDATE
        );
        updateRgn = Region(HRGNToQRegion(hrgnUpdate));
        DeleteObject(hrgnUpdate);
    }
#else
    // Scroll the backing store content
    QRect qScrollRect(scrollRect.left, scrollRect.top, scrollRect.Width(), scrollRect.Height());
    scrollBackingStore(delta.x, delta.y, qScrollRect);

    // Calculate the exposed region that needs redrawing
    QRegion scrolledRegion(qScrollRect);
    QRegion destRegion = scrolledRegion.translated(delta.x, delta.y);
    QRegion exposedRegion = scrolledRegion - destRegion;

    updateRgn = Region(exposedRegion);
    // exposedRegion is in content-area coordinates; translate to widget coordinates for Qt
    QPoint cr_origin = contentsRect().topLeft();
    update(exposedRegion.translated(cr_origin));
#endif
}

void QDmsViewArea::VH_DrawInContext(const Region& clipRgn, std::function<void(DrawContext&)> callback)
{
#ifdef _WIN32
    // On Win32, use GDI drawing directly on the native HWND
    // This allows drawing outside of WM_PAINT (for carets, etc.)
    if (m_DataViewHWnd)
    {
        HWND hWnd = reinterpret_cast<HWND>(m_DataViewHWnd);
        HDC hdc = ::GetDC(hWnd);
        if (hdc)
        {
            SHV_DrawInHDC(hdc, clipRgn, callback);
            ::ReleaseDC(hWnd, hdc);
        }
    }
#else
    // On non-Windows platforms, draw to the backing store
    // This can be called anytime; the backing store is blitted in paintEvent()
    auto rect = contentsRect();
    ensureBackingStore(rect.width(), rect.height());

    if (!m_BackingStore)
        return;

    QPainter painter(m_BackingStore.get());
    if (!painter.isActive())
        return;

    painter.setBackgroundMode(Qt::TransparentMode);

    // Set clip region (clipRgn is in content-area / backing-store coordinates)
    if (!clipRgn.Empty())
        painter.setClipRegion(clipRgn.GetQRegion());

    QtDrawContext ctx(&painter);
    callback(ctx);

    // Schedule a repaint to show the updated backing store.
    // update() expects widget coordinates, so translate from content-area coordinates.
    if (!clipRgn.Empty())
    {
        GRect bbox = clipRgn.BoundingBox();
        update(QRect(bbox.left + rect.left(), bbox.top + rect.top(), bbox.Width(), bbox.Height()));
    }
    else
    {
        update(rect); // only repaint the content area, not the title bar
    }
#endif
}

void QDmsViewArea::VH_ShowTooltip(GPoint screenPoint, CharPtr utf8Text)
{
    QToolTip::showText(QPoint(screenPoint.x, screenPoint.y), QString::fromUtf8(utf8Text), this);
}

void QDmsViewArea::VH_HideTooltip()
{
    QToolTip::hideText();
}

void QDmsViewArea::VH_ShowPopupMenu(GPoint clientPoint, const MenuData& menuData)
{
    if (menuData.empty())
        return;

    // Build QMenu tree from flat MenuData (items have m_Level for nesting)
    QMenu rootMenu(this);
    std::vector<QMenu*> menuStack;
    menuStack.push_back(&rootMenu);

    for (SizeT i = 0; i < menuData.size(); ++i)
    {
        const auto& item = menuData[i];

        // Adjust stack to match item level
        while (menuStack.size() > item.m_Level + 1)
            menuStack.pop_back();

        QMenu* currentMenu = menuStack.back();

        if (item.m_Caption.empty())
        {
            currentMenu->addSeparator();
            continue;
        }

        // Check if next item is a deeper level (submenu)
        bool hasSubmenu = (i + 1 < menuData.size() && menuData[i + 1].m_Level > item.m_Level);

        if (hasSubmenu)
        {
            auto* subMenu = currentMenu->addMenu(QString::fromUtf8(item.m_Caption.c_str()));
            menuStack.push_back(subMenu);
        }
        else
        {
            auto* action = currentMenu->addAction(QString::fromUtf8(item.m_Caption.c_str()));
            action->setData(QVariant::fromValue(static_cast<quint64>(i)));
            if (item.m_Flags & 0x0003) // MFS_DISABLED | MFS_GRAYED
                action->setEnabled(false);
            if (item.m_Flags & 0x0008) // MFS_CHECKED
                action->setCheckable(true), action->setChecked(true);
        }
    }

    QPoint screenPoint = mapToGlobal(QPoint(clientPoint.x, clientPoint.y));
    QAction* selected = rootMenu.exec(screenPoint);
    if (selected)
    {
        SizeT index = selected->data().value<quint64>();
        if (index < menuData.size())
            menuData[index].Execute();
    }
}

void QDmsViewArea::VH_SetCaretOverlay(const Region& rgn, bool visible)
{
    QRegion oldRegion = m_CaretOverlayRegion;
    bool wasVisible = m_CaretOverlayVisible;

    m_CaretOverlayRegion = rgn.Empty() ? QRegion() : rgn.GetQRegion();
    m_CaretOverlayVisible = visible;

    // Invalidate both old and new caret regions to trigger repaint.
    // Regions are in content-area coordinates; translate to widget coordinates for Qt.
    QPoint cr_origin = contentsRect().topLeft();
    if (wasVisible && !oldRegion.isEmpty())
        update(oldRegion.translated(cr_origin));
    if (visible && !m_CaretOverlayRegion.isEmpty())
        update(m_CaretOverlayRegion.translated(cr_origin));
}

void QDmsViewArea::VH_CopyToClipboard(const GRect& rect)
{
    if (!m_BackingStore)
        return;

    QRect qRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    qRect = qRect.intersected(m_BackingStore->rect());
    if (qRect.isEmpty())
        qRect = m_BackingStore->rect(); // fall back to full backing store

    QImage cropped = m_BackingStore->copy(qRect);

    QApplication::clipboard()->setImage(cropped);

    // Also save to file so test scripts can inspect the output
    static int s_CopyCount = 0;
#ifdef _WIN32
    QString dir = QString::fromLocal8Bit(qgetenv("TEMP").constData());
    if (dir.isEmpty()) dir = "C:\\temp";
#else
    QString dir = "/tmp";
#endif
    QString path = QString("%1/geodms_copy_%2.png").arg(dir).arg(s_CopyCount++);
    cropped.save(path);
}