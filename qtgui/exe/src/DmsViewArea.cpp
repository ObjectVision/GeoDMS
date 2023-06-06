#include "RtcBase.h"
#include "DmsViewArea.h"
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
        dva->parentWidget()->setWindowTitle(msg);
//    else
//        dva->lblCoord->SetCaption( msg ); // mouse info in world-coordinates
}


QDmsViewArea::QDmsViewArea(QWidget* parent, void* hWndMain, TreeItem* viewContext, const TreeItem* currItem, ViewStyle viewStyle)
    : QWidget(parent)
{
    HINSTANCE instance = GetInstance((HWND)hWndMain);//m_Views.at(m_ViewIndex).m_HWNDParent);
    static LPCWSTR dmsViewAreaClassName = RegisterViewAreaWindowClass(instance); // I say this only wonce

    

    m_DataView = SHV_DataView_Create(viewContext, viewStyle, ShvSyncMode::SM_Load);
       
    auto vs = WS_CHILD; //  viewStyle == tvsMapView ? WS_DLGFRAME | WS_CHILD : WS_CHILD;
    m_HWnd = CreateWindowEx(
        0L,                            // no extended styles
        dmsViewAreaClassName,          // DmsDataView control class 
        nullptr,                       // text for window title bar 
        vs,                            // styles
        CW_USEDEFAULT,                 // horizontal position 
        CW_USEDEFAULT,                 // vertical position
        m_Size.width(), m_Size.height(),   // width, height, to be reset by parent Widget
        (HWND)hWndMain,                // handle to parent (MainWindow)
        nullptr,                       // no menu
        instance,                      // instance owning this window 
        m_DataView                     // dataView
    );

    SHV_DataView_SetStatusTextFunc(m_DataView, this, OnStatusText);

    SHV_DataView_AddItem(m_DataView, currItem, false);
}

QDmsViewArea::~QDmsViewArea()
{
    SHV_DataView_Destroy(m_DataView);
    CloseWindow((HWND)m_HWnd);
}

void QDmsViewArea::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);
    m_Pos = event->pos();
    QWidget* w = this->parentWidget();
    while (w)
    {
        m_Pos += w->pos();
        w = w->parentWidget();
    }
    m_Pos.ry() += 64; // TODO: ???
    UpdatePos();
}

void QDmsViewArea::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_Size = event->size();
    UpdatePos();
}

void QDmsViewArea::UpdatePos()
{
    SetWindowPos((HWND)m_HWnd, HWND_TOP
        , m_Pos.x(), m_Pos.y()
        , m_Size.width(), m_Size.height()-64
        , SWP_SHOWWINDOW|SWP_ASYNCWINDOWPOS
    );
}

