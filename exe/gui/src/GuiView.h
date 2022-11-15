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

	std::string m_Name;
	ViewStyle m_ViewStyle = tvsUndefined;;
	//GuiTreeItemsHolder m_ActiveItems;
	DataView* m_DataView = nullptr;
	HWND m_HWNDParent	 = nullptr;
	HWND m_HWND		     = nullptr;
};

class GuiView : GuiBaseComponent 
{
public:
	GuiView() {}
	GuiView(GuiView&&) noexcept {}
	~GuiView();
	void Update();
	void Close(bool keepDataView);
	void CloseAll();
	void SetDoView(bool doView);
	bool DoView();
	bool IsPopulated();
	void InitDataView(TreeItem* currentItem, ViewStyle vs, std::string name);
	void SetViewIndex(int index);
	HWND GetHWND();


	std::vector<View> m_Views;
	View EditPaletteWindow = View("Edit Palette Window");
	int m_ViewIndex = -1;

private:
	WindowState InitWindow(TreeItem* currentItem);
	WindowState UpdateParentWindow();
	bool CloseWindowOnMimimumSize();
	ImVec2 GetRootParentCurrentWindowOffset();
	void UpdateWindowPosition(bool show);
	void RegisterViewAreaWindowClass(HINSTANCE instance);
	bool IsDocked();
	void ProcessEvent(GuiEvents event, TreeItem* currentItem);
	GuiState	m_State;
	bool m_DoView = true;
	bool		m_IsPopulated = false;
	bool		m_AddCurrentItem = false;
};