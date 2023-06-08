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
    {
        dva->setWindowTitle(msg);

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
    }
    else
    {
        // dva->lblCoord->SetCaption( msg ); // mouse info in world-coordinates
    }
}


QDmsViewArea::QDmsViewArea(QWidget* parent, void* hWndMain, TreeItem* viewContext, const TreeItem* currItem, ViewStyle viewStyle)
    : QMdiSubWindow(parent)
{
    assert(currItem); // Precondition

    HINSTANCE instance = GetInstance((HWND)hWndMain);//m_Views.at(m_ViewIndex).m_HWNDParent);
    static LPCWSTR dmsViewAreaClassName = RegisterViewAreaWindowClass(instance); // I say this only wonce    

    m_DataView = SHV_DataView_Create(viewContext, viewStyle, ShvSyncMode::SM_Load);
    if (!m_DataView)
        throwErrorF("CreateView", "Cannot create view with style %d with context '%s'"
            , viewStyle
            , viewContext->GetFullName().c_str()
        );
    bool result = SHV_DataView_AddItem(m_DataView, currItem, false);
    if (!result)
    {
        SHV_DataView_Destroy(m_DataView);
        throwErrorF("CreateView", "Cannot add '%s' to a view with style %d"
            , currItem->GetFullName().c_str()
            , viewStyle
        );

    }

    auto windowStyle = WS_CHILD | WS_CLIPSIBLINGS
        | WS_CAPTION   // enables TitleBar
        | WS_THICKFRAME // enables sizing
        | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU;

    auto exStyle = WS_EX_OVERLAPPEDWINDOW; // | WS_EX_MDICHILD;

    m_HWnd = CreateWindowEx(
        exStyle,                       // extended styles
        dmsViewAreaClassName,          // DmsDataView control class 
        nullptr,                       // text for window title bar 
        windowStyle,                   // styles
        CW_USEDEFAULT,                 // horizontal position 
        CW_USEDEFAULT,                 // vertical position
        m_Size.width(), m_Size.height(),   // width, height, to be reset by parent Widget
        (HWND)hWndMain,                // handle to parent (MainWindow)
        nullptr,                       // no menu
        instance,                      // instance owning this window 
        m_DataView                     // dataView
    );

    SHV_DataView_SetStatusTextFunc(m_DataView, this, OnStatusText); // to communicate title etc.

}

QDmsViewArea::~QDmsViewArea()
{
    SHV_DataView_Destroy(m_DataView);
    CloseWindow((HWND)m_HWnd);
}

void QDmsViewArea::moveEvent(QMoveEvent* event)
{
    base_class::moveEvent(event);

    m_Pos = event->pos();
    QWidget* w = this->parentWidget();
    if (w)
    {
        auto p = w->parentWidget();
        while (p)
        {
            m_Pos += w->pos();
            w = p; p = p->parentWidget();
        }
    }
    UpdatePos();
}

void QDmsViewArea::resizeEvent(QResizeEvent* event)
{
    base_class::resizeEvent(event);
    m_Size = event->size();
    UpdatePos();
}

void QDmsViewArea::UpdatePos()
{
    SetWindowPos((HWND)m_HWnd, HWND_TOP
        , m_Pos.x(), m_Pos.y()
        , m_Size.width(), m_Size.height()
        , SWP_SHOWWINDOW|SWP_ASYNCWINDOWPOS
    );
}

