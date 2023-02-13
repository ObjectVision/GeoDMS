#pragma once
#include "GuiBase.h"
#include <vector>
#include <string>
#include <windows.h>
#include "dataview.h"

//TODO:		
// define active viewre
// window naming sprintf(buf, "Animated title %c %d###AnimatedTitle", "|/-\\"[(int)(ImGui::GetTime() / 0.25f) & 3], ImGui::GetFrameCount());
// programatically docking of new node DockBuilderDockWindow DockBuilderGetCentralNode


enum WindowState
{
	UNINITIALIZED = -1,
	UNCHANGED     = 0,
	CHANGED       = 1
};

class View
{
public:
	View(std::string n)
	{
		m_Name = n;
	}

	View(std::string n, ViewStyle vs, DataView* dv)
	{
		m_Name = n;
		m_ViewStyle = vs;
		m_DataView = dv; //use SharedFromThis on DataView
	}
	View(View&& other) noexcept;
	auto operator=(View && other) noexcept -> void;
	~View();
	auto Reset() -> void;

	bool m_DoView = true;            // show the imgui window
	std::string m_Name;
	ViewStyle m_ViewStyle = tvsUndefined;;
	DataView* m_DataView = nullptr;
	HWND m_HWNDParent	 = nullptr;
	HWND m_HWND		     = nullptr;
	bool m_ShowWindow    = true;     // show or hide state of the m_HWND
	bool has_been_docking_initialized = false;
};

class GuiView : GuiBaseComponent 
{
public:
	~GuiView();
	auto CloseAll() -> void;
	auto AddView(GuiState& state, TreeItem* currentItem, ViewStyle vs, std::string name) -> void;
	auto GetHWND() -> HWND;
	auto UpdateAll(GuiState& state) -> void;

	std::vector<View> m_Views;
	std::unique_ptr<View> EditPaletteWindow; // name, vs, SHV_DataView_Create(viewContextItem, vs, ShvSyncMode::SM_Load)
	std::_Vector_iterator<std::_Vector_val<std::_Simple_types<View>>> m_ViewIt = m_Views.begin();

private:
	auto Update(GuiState& state, View& view) -> bool;
	auto InitWindow(TreeItem* currentItem) -> WindowState;
	auto UpdateParentWindow(View& view) -> WindowState;
	auto CloseWindowOnMimimumSize(View& view) -> bool;
	auto GetRootParentCurrentWindowOffset() -> ImVec2;
	auto UpdateWindowPosition(View& view) -> void;
	auto ShowOrHideWindow(View& view, bool show) -> void;
	auto RegisterViewAreaWindowClass(HINSTANCE instance) -> void;
	auto IsDocked() -> bool;
	auto ProcessEvent(GuiEvents event, TreeItem* currentItem) -> void;

	bool		m_AddCurrentItem = false;
};