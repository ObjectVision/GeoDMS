// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef DMSVIEWAREA_H
#define DMSVIEWAREA_H

#include "ShvUtils.h"
#include "ViewHost.h"

#include <QMdiArea>
#include <QMdiSubWindow>
#include <QAbstractNativeEventFilter>
#include <QScrollBar>
#include <QTimer>
#include <QImage>
#include <QRegion>
#include <map>
#include <memory>

struct TreeItem;
class DataView;
struct MdiCreateStruct;

class QDmsMdiArea : public QMdiArea
{
    Q_OBJECT
public:
    QDmsMdiArea(QWidget* parent);
    QMdiSubWindow* addDmsSubWindow(QWidget* widget); 
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeAllButActiveSubWindow();
    void setTabbedViewModeStyle();
    QSize sizeHint() const override;
    auto getTabBar() -> QTabBar*;
    void updateTabTooltips();

public slots:
    void testCloseSubWindow();
    void onCascadeSubWindows();
    void onTileSubWindows();
    void closeActiveDmsSubWindow();
};

//----------------------------------------------------------------------
// QDmsViewArea: Qt-based ViewHost implementation
// Replaces Win32ViewHost for portable windowing operations.
// DataView uses ViewHost interface; this class provides Qt implementation.
//----------------------------------------------------------------------

class QDmsViewArea : public QMdiSubWindow, public ViewHost
{
    Q_OBJECT
    using base_class = QMdiSubWindow;

public:
    QDmsViewArea(QMdiArea* parent, TreeItem* viewContext, const TreeItem* currItem, ViewStyle viewStyle);
    QDmsViewArea(QMdiArea* parent, MdiCreateStruct* createStruct);
    ~QDmsViewArea();

    // QMdiSubWindow overrides
#ifdef _WIN32
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif
    void dragEnterEvent(QDragEnterEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void dropEvent(QDropEvent* event) override;

#ifndef _WIN32
    // Mouse/wheel event dispatch to DataView (Linux: Qt events → DispatchMouseEvent)
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;
#endif

    // Keyboard: forward to DataView::OnKeyDown on both platforms. Without this,
    // QMdiSubWindow's default arrow-key handling enters a move/resize mode that also
    // breaks the [x] close button until Esc is pressed.
    void keyPressEvent(QKeyEvent* event) override;

    auto getDataView() -> std::shared_ptr<DataView> { return m_DataView.lock(); }
#ifdef _WIN32
    auto getDataViewHwnd() -> void* { return m_DataViewHWnd; }
#endif
    void UpdatePosAndSize();
    void on_rescale();
    DPoint getScaleFactors() const;

    //------------------------------------------------------------------
    // ViewHost interface implementation (Qt-based, portable)
    //------------------------------------------------------------------

    // Timer management
    void VH_SetTimer(UInt32 id, UInt32 elapseMs) override;
    void VH_KillTimer(UInt32 id) override;

    // Mouse capture
    void VH_SetCapture() override;
    void VH_ReleaseCapture() override;

    // Focus
    void VH_SetFocus() override;

    // Cursor position
    bool VH_GetCursorScreenPos(GPoint& pos) const override;
    GPoint VH_ScreenToClient(GPoint screenPt) const override;
    GPoint VH_ClientToScreen(GPoint clientPt) const override;
    void VH_SetGlobalCursorPos(GPoint screenPt) override;

    // Cursor shape
    void VH_SetCursorArrow() override;
    void VH_SetCursorWait() override;
    void VH_SetCursor(DmsCursor cursor) override;

    // Invalidation and validation
    void VH_InvalidateRect(const GRect& rect, bool erase) override;
    void VH_InvalidateRgn(const Region& rgn, bool erase) override;
    void VH_ValidateRect(const GRect& rect) override;

    // Force synchronous paint processing
    void VH_UpdateWindow() override;

    // Async operations
    void VH_PostMessage(UInt32 msg, UInt64 wParam, Int64 lParam) override;
    void VH_SendClose() override;

    // Visibility and window state
    bool VH_IsVisible() const override;
    UInt32 VH_GetShowCmd() const override;

    // Text caret (blinking cursor in text editors)
    void VH_CreateTextCaret(int width, int height) override;
    void VH_DestroyTextCaret() override;
    void VH_SetTextCaretPos(GPoint pos) override;
    void VH_ShowTextCaret() override;
    void VH_HideTextCaret() override;

    // Mouse tracking
    void VH_TrackMouseLeave() override;

    // Parent notification
    void VH_NotifyParentActivation() override;

    // Scroll
    void VH_ScrollWindow(GPoint delta, const GRect& scrollRect, const GRect& clipRect,
        Region& updateRgn, const GRect& validRect) override;

    // Drawing
    void VH_DrawInContext(const Region& clipRgn, std::function<void(DrawContext&)> callback) override;

    // Tooltip
    void VH_ShowTooltip(GPoint screenPoint, CharPtr utf8Text) override;
    void VH_HideTooltip() override;

    // Context menu
    void VH_ShowPopupMenu(GPoint clientPoint, const MenuData& menuData) override;

    // Caret overlay
    void VH_SetCaretOverlay(const Region& rgn, bool visible) override;

    // Clipboard copy: copies backing store region to clipboard and saves to /tmp/geodms_copy.png
    void VH_CopyToClipboard(const GRect& rect) override;

#ifndef _WIN32
    // Scroll bars (Linux/Qt: implemented as QScrollBar child widgets)
    void VH_SetHScrollBar(bool visible, int pos, int page, int max, int tick) override;
    void VH_SetVScrollBar(bool visible, int pos, int page, int max, int tick) override;
#endif

#ifdef _WIN32
    HWND VH_GetHWnd() const override { return reinterpret_cast<HWND>(m_DataViewHWnd); }
#endif

public slots:
    void onWindowStateChanged(Qt::WindowStates oldState, Qt::WindowStates newState);
    void onTimerTimeout();
    void onHScrollValueChanged(int value);
    void onVScrollValueChanged(int value);

private:
    void CreateDmsView(QMdiArea* parent, ViewStyle viewStyle);
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    auto contentsRectInPixelUnits() -> QRect;

    // Backing store management
    void ensureBackingStore(int width, int height);
    void scrollBackingStore(int dx, int dy, const QRect& scrollRect);

    std::weak_ptr<DataView> m_DataView;

#ifdef _WIN32
    void* m_DataViewHWnd = nullptr;
#endif

    // Timer management for VH_SetTimer/VH_KillTimer
    std::map<UInt32, QTimer*> m_Timers;

    // Backing store for off-paint drawing (model data only, no carets)
    std::unique_ptr<QImage> m_BackingStore;

    // Caret overlay state (drawn on top of backing store in paintEvent)
    // This keeps the backing store clean with only model data
    QRegion m_CaretOverlayRegion;
    bool m_CaretOverlayVisible = false;

    // Text caret state (separate from graphical carets)
    bool m_TextCaretVisible = false;
    GPoint m_TextCaretPos = {0, 0};
    int m_TextCaretWidth = 2;
    int m_TextCaretHeight = 16;

#ifdef _WIN32
    DWORD m_cookie = 0; // used for RegisterScaleChangeNotifications
#endif
    DPoint m_LastScaleFactors = {1.0, 1.0};

#ifndef _WIN32
    QScrollBar* m_hScrollBar = nullptr;
    QScrollBar* m_vScrollBar = nullptr;
    int m_hScrollTick = 1;
    int m_vScrollTick = 1;
    void repositionScrollBars();
#endif
};

#endif // DMSVIEWAREA_H
