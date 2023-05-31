#include "RtcBase.h"
#include "DmsViewArea.h"
#include <windows.h>

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

QDmsViewArea::QDmsViewArea(QWidget* parent, HANDLE hWndMain, TreeItem* viewContext, TreeItem* currItem)
    : QWidget(parent)
{
    HINSTANCE instance = GetInstance((HWND)hWndMain);//m_Views.at(m_ViewIndex).m_HWNDParent);
    static LPCWSTR dmsViewAreaClassName = RegisterViewAreaWindowClass(instance); // I say this only wonce

    ViewStyle viewStyle = SHV_GetDefaultViewStyle(currItem);

    dv = SHV_DataView_Create(viewContext, viewStyle, ShvSyncMode::SM_Load);
       
    auto vs = WS_CHILD; //  viewStyle == tvsMapView ? WS_DLGFRAME | WS_CHILD : WS_CHILD;
    wnd = CreateWindowEx(
        0L,                            // no extended styles
        dmsViewAreaClassName,          // DmsDataView control class 
        nullptr,                       // text for window title bar 
        vs,                            // styles
        CW_USEDEFAULT,                 // horizontal position 
        CW_USEDEFAULT,                 // vertical position
        size.width(), size.height(),   // width, height, to be reset by parent Widget
        (HWND)hWndMain,                // handle to parent (MainWindow)
        nullptr,                       // no menu
        instance,                      // instance owning this window 
        dv                             // dataView
    );

    SHV_DataView_AddItem(dv, currItem, false);
}

QDmsViewArea::~QDmsViewArea()
{
    SHV_DataView_Destroy(dv);
}

void QDmsViewArea::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);
    pos = event->pos();
    QWidget* w = this->parentWidget();
    while (w)
    {
        pos += w->pos();
        w = w->parentWidget();
    }
    pos.ry() += 32; // TODO: ???
    UpdatePos();
}

void QDmsViewArea::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    size = event->size();
    UpdatePos();
}

void QDmsViewArea::UpdatePos()
{
    SetWindowPos((HWND)wnd, HWND_TOP, pos.x(), pos.y(), size.width(), size.height(), SWP_SHOWWINDOW|SWP_ASYNCWINDOWPOS);
}

