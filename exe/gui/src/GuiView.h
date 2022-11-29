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
		//auto test = dv->shared_from_this();
	}
	View(View&& other) noexcept;
	void operator=(View && other) noexcept;

	~View();

	void Reset();

	bool m_DoView = true;            // show the imgui window
	std::string m_Name;
	ViewStyle m_ViewStyle = tvsUndefined;;
	DataView* m_DataView = nullptr;
	HWND m_HWNDParent	 = nullptr;
	HWND m_HWND		     = nullptr;
	bool m_ShowWindow    = true;     // show or hide state of the m_HWND
};

class GuiView : GuiBaseComponent 
{
public:
	~GuiView();
	void CloseAll();
	void AddView(GuiState& state, TreeItem* currentItem, ViewStyle vs, std::string name);
	HWND GetHWND();
	void UpdateAll(GuiState& state);

	std::vector<View> m_Views;
	View EditPaletteWindow = View("Edit Palette Window");
	std::_Vector_iterator<std::_Vector_val<std::_Simple_types<View>>> m_ViewIt = m_Views.begin();

private:
	bool Update(GuiState& state, View& view);
	WindowState InitWindow(TreeItem* currentItem);
	WindowState UpdateParentWindow(View& view);
	bool CloseWindowOnMimimumSize(View& view);
	ImVec2 GetRootParentCurrentWindowOffset();
	void UpdateWindowPosition(View& view);
	void ShowOrHideWindow(View& view, bool show);
	void RegisterViewAreaWindowClass(HINSTANCE instance);
	bool IsDocked();
	void ProcessEvent(GuiEvents event, TreeItem* currentItem);
	bool		m_AddCurrentItem = false;
};