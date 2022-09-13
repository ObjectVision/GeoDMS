#pragma once
#include "GuiBase.h"
#include <vector>
#include <string>
#include <windows.h>
#include "dataview.h"

enum WindowState
{
	UNINITIALIZED = -1,
	UNCHANGED     = 0,
	CHANGED       = 1
};

class GuiView : GuiBaseComponent 
{
public:
	//GuiView(GuiView&&) noexcept {}
	//GuiView(TreeItem*& currentItem, ViewStyle style, std::string name);
	GuiView() {}
	GuiView(GuiView&&) noexcept {}
	~GuiView();
	void ResetView(ViewStyle vs, std::string vn);
	void Update();
	void Close(bool keepDataView);
	void SetDoView(bool doView);
	bool DoView();
	bool IsPopulated();
	void SetViewStyle(ViewStyle vs);
	void SetViewName(std::string vn);
	std::string GetViewName();

private:
	void InitDataView(TreeItem* currentItem);
	WindowState InitWindow(TreeItem* currentItem);
	WindowState UpdateParentWindow();
	bool CloseWindowOnMimimumSize();
	ImVec2 GetRootParentCurrentWindowOffset();
	void UpdateWindowPosition(bool show);
	void RegisterMapViewAreaWindowClass(HINSTANCE instance);
	bool IsDocked();
	void ProcessEvent(GuiEvents event, TreeItem* currentItem);
	GuiTreeItemsHolder m_ActiveItems;
	GuiState	m_State;
	bool m_DoView = true;
	bool		m_IsPopulated = true;
	HWND		m_HWND       = nullptr; // Should be a childwindow of the window the MapView is currently in.
	HWND		m_HWNDParent = nullptr;
	mutable ViewStyle	m_ViewStyle = tvsUndefined;
	mutable std::string m_ViewName = "";
	DataView*	m_DataView   = nullptr;
};