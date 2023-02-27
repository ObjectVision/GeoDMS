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

class AbstractView
{
public:
	virtual bool Update(GuiState& state) = 0;
};

class View : AbstractView
{
public:
	View(std::string n)
	{
		m_Name = n;
	}

	bool Update(GuiState& state) override;

	View(std::string n, ViewStyle vs, DataView* dv)
	{
		m_Name = n;
		m_ViewStyle = vs;
		m_DataView = dv; //use SharedFromThis on DataView
	}
	View(View&& other) noexcept;
	void operator=(View && other) noexcept;
	~View();
	void Reset();

	// window helper functions
	bool CloseWindowOnMimimumSize();
	void ShowOrHideWindow(bool show);
	bool IsDocked();
	auto UpdateParentWindow() -> WindowState;
	void UpdateWindowPosition();
	auto GetRootParentCurrentWindowOffset() -> ImVec2;
	auto InitWindow(TreeItem* currentItem) -> WindowState;
	void RegisterViewAreaWindowClass(HINSTANCE instance);

	bool m_DoView = true;            // show the imgui window
	std::string m_Name;
	ViewStyle m_ViewStyle = tvsUndefined;;
	DataView* m_DataView = nullptr;
	HWND m_HWNDParent	 = nullptr;
	HWND m_HWND		     = nullptr;
	bool m_ShowWindow    = true;     // show or hide state of the child m_HWND
	bool has_been_docking_initialized = false;
};

class GuiViews 
{
public:
	~GuiViews();
	void CloseAll();
	void AddView(GuiState& state, TreeItem* currentItem, ViewStyle vs, std::string name);
	auto GetHWND() -> HWND; //TODO: move, not the right place
	void UpdateAll(GuiState& state);
	std::vector<View> m_Views;
	std::vector<View> m_EditPaletteWindows; // name, vs, SHV_DataView_Create(viewContextItem, vs, ShvSyncMode::SM_Load)
	std::_Vector_iterator<std::_Vector_val<std::_Simple_types<View>>> m_ViewIt = m_Views.begin();

	void ResetEditPaletteWindow(ClientHandle clientHandle, const TreeItem* self);
	static auto OnOpenEditPaletteWindow(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode) -> void;

private:
	//bool Update(GuiState& state, View& view);

	//auto InitWindowParameterized(std::string caption, DataView* dv, ViewStyle vs, HWND parent_hwnd, UInt32 min = 300, UInt32 max = 600) -> HWND;
	void ProcessEvent(GuiEvents event, TreeItem* currentItem);

	bool		m_AddCurrentItem = false;
};