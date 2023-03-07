#pragma once
#include "GuiBase.h"
#include <vector>
#include <string>
#include <windows.h>
#include "dataview.h"

// TODO: rename files to GuiViews

class GuiTableView;

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
	std::string m_Name;
	std::string m_Icon;
	bool m_DoView = true;
	bool has_been_docking_initialized = false;
};

class DMSView : public AbstractView
{
public:
	bool Update(GuiState& state) override;

	DMSView(std::string n, ViewStyle vs, DataView* dv)
	{
		//"\xE2\x88\x91 " + std::string("Statistics for ") + std::string(m_item->GetName().c_str()) + name;
		m_Icon = "";
		switch (vs)
		{
		case tvsMapView: { m_Icon = ICON_RI_GLOBE + std::string(" "); break; }
		case tvsTableView: { m_Icon = ICON_RI_TABLE + std::string(" ");	break; }
		case tvsTableContainer:	{ m_Icon = ICON_RI_TABLE + std::string(" ");	break; }
		default: break;
		}

		m_Name = n;
		m_ViewStyle = vs;
		m_DataView = dv; //use SharedFromThis on DataView
	}
	DMSView(DMSView&& other) noexcept;
	void operator=(DMSView&& other) noexcept;
	~DMSView();
	void Reset();

	// window helper functions
	bool CloseWindowOnMimimumSize();
	void ShowOrHideWindow(bool show);
	//bool IsDocked();
	auto UpdateParentWindow() -> WindowState;
	void UpdateWindowPosition();
	auto GetRootParentCurrentWindowOffset() -> ImVec2;
	auto InitWindow(TreeItem* currentItem) -> WindowState;
	void RegisterViewAreaWindowClass(HINSTANCE instance);

	//bool m_DoView = true;            // show the imgui window
	//std::string m_Name;
	ViewStyle m_ViewStyle = tvsUndefined;;
	DataView* m_DataView = nullptr;
	HWND m_HWNDParent	 = nullptr;
	HWND m_HWND		     = nullptr;
	bool m_ShowWindow    = true;     // show or hide state of the child m_HWND
};

class StatisticsView : public  AbstractView
{
public:
	StatisticsView(GuiState &state, std::string name);
	bool Update(GuiState& state) override; // TODO: hotkey using CTRL-I, change invalidate hotkey to ALT-I
	void UpdateData();

private:
	
	bool m_done = false;
	bool m_is_ready = false;
	TreeItem *m_item = nullptr; // to be released when statistics information is finalized
	TableData m_data;
};

class GuiViews 
{
	using dms_views = std::vector<DMSView>;
	using statistics_views = std::vector<StatisticsView>;
	using table_views = std::vector<GuiTableView>;
	using dms_view_it = std::_Vector_iterator<std::_Vector_val<std::_Simple_types<DMSView>>>;

public:
	~GuiViews();
	void CloseAll();
	void AddDMSView(GuiState& state, ViewStyle vs, std::string name);
	void AddTableView(GuiState& state);
	void AddStatisticsView(GuiState& state, std::string name);
	auto GetHWND() -> HWND; //TODO: move, not the right place
	void UpdateAll(GuiState& state);
	
	// view containers
	dms_views        m_dms_views; // tableviews and mapviews
	dms_views        m_edit_palette_windows;
	table_views		 m_table_views;
	statistics_views m_statistic_views;
	dms_view_it      m_dms_view_it = m_dms_views.begin();

	void ResetEditPaletteWindow(ClientHandle clientHandle, const TreeItem* self);
	static auto OnOpenEditPaletteWindow(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode) -> void;

private:
	//bool Update(GuiState& state, View& view);

	//auto InitWindowParameterized(std::string caption, DataView* dv, ViewStyle vs, HWND parent_hwnd, UInt32 min = 300, UInt32 max = 600) -> HWND;
	void ProcessEvent(GuiEvents event, TreeItem* currentItem);

	bool m_AddCurrentItem = false;
};