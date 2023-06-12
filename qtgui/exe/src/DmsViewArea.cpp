#include "RtcBase.h"
#include "DmsViewArea.h"
#include "DmsMainWindow.h"

#include <QMdiArea>

#include <windows.h>

#include "dbg/SeverityType.h"

#include "DataView.h"
#include "ShvDllInterface.h"
#include "QEvent.h"

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
/*
        // keep caption of window in sync with widget, TODO: simplify or factor out the usage of MultiByteToWideChar everywhere
        auto hWnd = (HWND)dva->getHwnd();
        const UInt32 MAX_TEXTOUT_SIZE = 400;
        wchar_t uft16Buff[MAX_TEXTOUT_SIZE+1];
        SizeT textLen = Min<SizeT>(StrLen(msg), MAX_TEXTOUT_SIZE);
        textLen = MultiByteToWideChar(utf8CP, 0, msg, textLen, uft16Buff, MAX_TEXTOUT_SIZE);
        assert(textLen <= MAX_TEXTOUT_SIZE);
        if (textLen > 0 && textLen <= MAX_TEXTOUT_SIZE)
        {
            uft16Buff[textLen] = wchar_t(0);
            if (hWnd)
                SetWindowTextW(hWnd, uft16Buff);
        }
*/
    }
    else
    {
        // dva->lblCoord->SetCaption( msg ); // mouse info in world-coordinates
    }
}

QDmsMdiArea::QDmsMdiArea(QWidget* parent)
    : QMdiArea(parent) 
{
    setAcceptDrops(true);
}

void QDmsMdiArea::dragEnterEvent(QDragEnterEvent* event)
{
    event->acceptProposedAction(); // TODO: further specify that only treenodes dragged from treeview can be dropped here.
}

void QDmsMdiArea::dropEvent(QDropEvent* event)
{
    MainWindow::TheOne()->defaultView();
}

QDmsViewArea::QDmsViewArea(QWidget* parent, void* hWndMain, TreeItem* viewContext, const TreeItem* currItem, ViewStyle viewStyle)
    : QMdiSubWindow(parent)
{
    assert(currItem); // Precondition

    HINSTANCE instance = GetInstance((HWND)hWndMain);
    static LPCWSTR dmsViewAreaClassName = RegisterViewAreaWindowClass(instance); // I say this only once

    auto parent_hwnd = (HWND)winId();
    auto rect = contentsRect();

    m_DataView = SHV_DataView_Create(viewContext, viewStyle, ShvSyncMode::SM_Load);
    if (!m_DataView)
        throwErrorF("CreateView", "Cannot create view with style %d with context '%s'"
            , viewStyle
            , viewContext->GetFullName().c_str()
        );

    auto vs = WS_CHILD | WS_CLIPSIBLINGS|WS_CLIPCHILDREN; //  viewStyle == tvsMapView ? WS_DLGFRAME | WS_CHILD : WS_CHILD;
    m_HWnd = CreateWindowEx(
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
    SHV_DataView_SetStatusTextFunc(m_DataView, this, OnStatusText); // to communicate title etc.

    // SHV_DataView_AddItem can call ClassifyJenksFisher, which requires DataView with a m_hWnd, so this must be after CreateWindowEx
    // or PostMessage(WM_PROCESS_QUEUE, ...) directly here to trigger DataView::ProcessGuiOpers()
    bool result = SHV_DataView_AddItem(m_DataView, currItem, false); 
  
    if (!result)
    {
        CloseWindow((HWND)m_HWnd); // calls SHV_DataView_Destroy
        throwErrorF("CreateView", "Cannot add '%s' to a view with style %d"
            , currItem->GetFullName().c_str()
            , viewStyle
        );
    }
    SetWindowPos((HWND)m_HWnd, HWND_TOP, rect.x(), rect.y(), rect.width(), rect.height(), SWP_SHOWWINDOW);
}

QDmsViewArea::~QDmsViewArea()
{
//    SHV_DataView_Destroy(m_DataView);
    CloseWindow((HWND)m_HWnd); // calls SHV_DataView_Destroy
}

void QDmsViewArea::moveEvent(QMoveEvent* event)
{
    base_class::moveEvent(event);
//    UpdatePosAndSize();
}

void QDmsViewArea::resizeEvent(QResizeEvent* event)
{
    base_class::resizeEvent(event);
    UpdatePosAndSize();
}

void QDmsViewArea::UpdatePosAndSize()
{
    auto rect= contentsRect();

    MoveWindow((HWND)m_HWnd, rect.x(), rect.y(), rect.width(), rect.height(), true);
}