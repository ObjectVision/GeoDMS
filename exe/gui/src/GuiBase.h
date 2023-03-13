#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <list>
#include <queue>
#include <chrono>

#include "TreeItem.h"
#include "AbstrDataItem.h"
#include "dbg/check.h"
#include "dbg/SeverityType.h"
#include "TreeItemProps.h"
#include "IconsFontRemixIcon.h"

struct ImGuiWindow;

enum GuiWindowOpenFlags
{
	GWOF_TreeView = 1,
	GWOF_DetailPages = 2,
	GWOF_EventLog = 4,
	GWOF_Options = 8,
	GWOF_ToolBar = 16,
	GWOF_CurrentItemBar = 32,
	GWOF_StatusBar = 64
};

enum class GuiEvents
{
	UpdateCurrentItem,
	UpdateCurrentItemDirectly,
	JumpToCurrentItem,
	UpdateCurrentAndCompatibleSubItems,
	ReopenCurrentConfiguration,
	OpenNewMapViewWindow,
	OpenNewTableViewWindow,
	OpenNewImGuiTableViewWIndow,
	OpenNewStatisticsViewWindow,
	OpenNewConfiguration,
	OpenInMemoryDataView,
	OpenNewDefaultViewWindow,
	OpenConfigSource,
	OpenEditPaletteWindow,
	OpenErrorDialog,
	OpenOptionsWindow,
	OpenExportWindow,
	ToggleShowTreeViewWindow,
	ToggleShowEventLogWindow,
	ToggleShowDetailPagesWindow,
	ToggleShowCurrentItemBar,
	ToggleShowToolbar,
	StepToErrorSource,
	StepToRootErrorSource,
	ShowLocalSourceDataChangedModalWindow,
	ShowAboutTextModalWindow,
	FocusValueInfoTab,
	AscendVisibleTree,
	TreeViewJumpKeyPress,
	DescendVisibleTree,
	CollapseTreeNode,
	ExpandTreeNode,
	MenuOpenFile,
	MenuOpenEdit,
	MenuOpenView,
	MenuOpenTools,
	MenuOpenWindow,
	MenuOpenHelp,
	CloseAllViews,
	Close,
};

enum class GeoDMSWindowTypes
{
	GeoDMSGui,
	TreeView,
	DetailPages,
	EventLog,
	MapView,
	TableView,
	Options
};

auto GeoDMSWindowTypeToName(GeoDMSWindowTypes wt) -> std::string;

static auto InputTextCallback(ImGuiInputTextCallbackData* data) -> int;

struct InputTextCallback_UserData
{
	std::string* Str;
	ImGuiInputTextCallback ChainCallback;
	void* ChainCallbackUserData;
};

auto InputTextCallback(ImGuiInputTextCallbackData* data) -> int
{
	InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
	{
		// Resize string callback
		// If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
		std::string* str = user_data->Str;
		IM_ASSERT(data->Buf == str->c_str());
		str->resize(data->BufTextLen);
		data->Buf = (char*)str->c_str();
	}
	else if (user_data->ChainCallback)
	{
		// Forward to user callback, if any
		data->UserData = user_data->ChainCallbackUserData;
		return user_data->ChainCallback(data);
	}
	return 0;
}

namespace ImGui
{
	// ImGui::InputText() with std::string
	// Because text input needs dynamic resizing, we need to setup a callback to grow the capacity
	IMGUI_API bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
}

class EventQueue
{
public:
	bool HasEvents()
	{
		return not m_Queue.empty();
	}

	void Add(GuiEvents event)
	{
		m_Queue.push(event);
	}

	GuiEvents Pop()
	{
		auto event = m_Queue.front();
		m_Queue.pop();
		return event;
	}

	auto Size() -> size_t
	{
		return m_Queue.size();
	}

private:
	std::queue<GuiEvents> m_Queue;
};

class FlagStateManager
{
public:
	bool HasNew()
	{
		return m_hasNew;
	}

	bool Get()
	{
		m_hasNew = false;
		return m_flag;
	}

	void Set(bool newFlagState)
	{
		m_hasNew = true;
		m_flag = newFlagState;
	}
private:
	bool m_flag   = false;
	bool m_hasNew = false;
};

class TreeItemStateManager
{
public:

	bool HasNew()
	{
		return m_hasNew;
	}

	TreeItem* Get()
	{
		m_hasNew = false;
		return m_item;
	}

	void Set(TreeItem* newitem)
	{
		m_hasNew = true;
		m_item = newitem;
	}
private:
	TreeItem* m_item;
	bool m_hasNew = false;
};

class StringStateManager
{
public:
	StringStateManager()
	{
		UInt32 cap = 25; //TODO: remove ugly code
		m_PreviousStrings.resize(cap);
		m_NextStrings.resize(cap);
	}

	bool HasNew()
	{
		return m_hasNew;
	}

	std::string Get()
	{
		m_hasNew = false;
		return m_StringState;
	}

	std::string _Get()
	{
		return m_StringState;
	}

	void Set(std::string newStringState)
	{
		m_hasNew = true;
		m_StringState = newStringState;
	}

	void SetPrevious()
	{
		// TODO: implement
	}

	void SetNext()
	{
		// TODO: implement
	}

private:
	void RotateStringsLeft(std::vector<std::string> &strings)
	{
		std::rotate(strings.begin(), strings.begin() + 1, strings.end());
	}

	void RotateStringsRight(std::vector<std::string>& strings)
	{
		std::rotate(strings.rbegin(), strings.rbegin() + 1, strings.rend());
	}

	bool		m_hasNew = false;
	std::string m_StringState;
	std::vector<std::string> m_PreviousStrings;
	std::vector<std::string> m_NextStrings;
};

struct ViewAction
{
	TreeItem*       tiContext = nullptr;
	std::string     sAction   = "";
	Int32           nCode     = 0;
	Int32           x         = 0;
	Int32           y         = 0;

	bool operator==(ViewAction const& rhs) const 
	{ 
		return tiContext==rhs.tiContext && sAction==rhs.sAction && nCode==rhs.nCode && x==rhs.x && y==rhs.y;
	}
};

class ViewActionHistory
{
public:
	ViewActionHistory(); // TODO: TreeItem* alive state not guaranteed, replace with full item name lookup
	auto Insert(ViewAction) -> void;
	ViewAction GetNext();
	ViewAction GetPrevious();
	auto GetCurrentIterator() -> std::list<ViewAction>::iterator;
	auto  GetBeginIterator() -> std::list<ViewAction>::iterator;
	auto GetEndIterator() -> std::list<ViewAction>::iterator;
	void clear() { m_History.clear(); m_Iterator = m_History.begin(); };
private:
	std::list<ViewAction>::iterator m_Iterator;
	std::list<ViewAction> m_History; // TODO: replace std::list with std::vector
};

class GuiEventQueues
{
public:
	// singleton
	static GuiEventQueues* getInstance();
	static auto DeleteInstance() -> void;
	~GuiEventQueues()
	{
	}

	// event queues
	EventQueue MainEvents;
	EventQueue CurrentItemBarEvents;
	EventQueue TreeViewEvents;
	EventQueue TableViewEvents;
	EventQueue MapViewEvents;
	EventQueue DetailPagesEvents;
	EventQueue GuiMenuFileComponentEvents;
	EventQueue MenuBarEvents;

private:
	GuiEventQueues() {}
	static GuiEventQueues* instance;
};

struct GuiFonts
{
	ImFont* text_font = nullptr;
	ImFont* icon_font = nullptr;
	ImFont* math_font = nullptr;
};

class GuiState
{
public:
	GuiState() {}
	~GuiState();
	GuiState(const GuiState&) = delete;
	auto clear() -> void;

	int return_value					= 0;
	std::chrono::time_point<std::chrono::system_clock> m_last_update_time;


	// open window flags // TODO: move these flags to the specific windows?
	bool ShowOptionsWindow				= false;
	bool ShowDetailPagesOptionsWindow	= false;
	bool ShowEventLogOptionsWindow		= false;
	bool ShowOpenFileWindow				= false;
	bool ShowConfigSource				= false;
	bool ShowTreeviewWindow				= true;
	bool ShowDetailPagesWindow			= true;
	bool ShowDemoWindow					= false;
	bool ShowEventLogWindow				= true;
	bool ShowToolbar					= true;
	bool ShowStatusBar					= false;
	bool ShowCurrentItemBar				= true;
	bool ShowExportWindow				= false;

	SourceDescrMode SourceDescrMode		= SourceDescrMode::Configured;

	StringStateManager configFilenameManager;

	// docking
	ImGuiID dockspace_id = -1;

	// style
	GuiFonts fonts;

	// singletons //TODO: Remove singletons in light of good coding practice
	static StringStateManager errorDialogMessage;
	std::string aboutDialogMessage;
	static StringStateManager contextMessage;
	static ViewActionHistory TreeItemHistoryList;

	// jump to letter in TreeView
	static std::string m_JumpLetter;

	TreeItem* GetRoot() { return m_Root; }
	TreeItem* GetCurrentItem() { return m_CurrentItem; }
	void SetRoot(TreeItem* root) { m_Root = root; };
	void SetCurrentItem(TreeItem* current_item) { m_CurrentItem = current_item; };

	// load and save state
	void   SaveWindowOpenStatusFlags();
	void   LoadWindowOpenStatusFlags();

	// on first use
	void   SetWindowOpenStatusFlagsOnFirstUse();

private:
	SharedPtr<TreeItem> m_Root;
	SharedPtr<TreeItem> m_CurrentItem;
};

// for detail pages and gui views
enum PropertyEntryType
{
	PET_SEPARATOR,
	PET_TEXT,
	PET_LINK,
	PET_HEADING
};

struct PropertyEntry
{
	PropertyEntryType   type;
	bool background_is_red = false;
	std::string         text;
};

using RowData = std::vector<PropertyEntry>;
using TableData = std::vector<RowData>;

// Helper functions
auto DivideTreeItemFullNameIntoTreeItemNames(std::string fullname, std::string separator = "/") -> std::vector<std::string>;
auto GetExeFilePath() -> std::string;
auto SetCursorPosToOptionsIconInWindowHeader() -> ImVec2;
void SetClipRectToIncludeOptionsIconInWindowHeader();
bool MouseHooversOptionsIconInWindowHeader();
void SetKeyboardFocusToThisHwnd();
bool LoadIniFromRegistry(bool reload=false);
void SaveIniToRegistry();
void OnItemClickItemTextTextToClipboard(std::string_view text);
void SetTextBackgroundColor(ImVec2 background_rectangle_size, ImU32 col = IM_COL32(225, 6, 0, 200), ImDrawList* draw_list = nullptr, ImVec2* cursor_pos = nullptr);
void AutoHideWindowDocknodeTabBar(bool& is_docking_initialized);
bool TryDockViewInGeoDMSDataViewAreaNode(GuiState& state, ImGuiWindow* window);
auto StartWindowsFileDialog(std::string start_path, std::wstring file_dialog_text, std::wstring file_dialog_exts) -> std::string;
auto BrowseFolder(std::string saved_path) -> std::string;
void OpenUrlInWindowsDefaultBrowser(const std::string url);
void PostEmptyEventToGLFWContext();
void StringToTable(std::string& input, TableData& result, std::string separator = "");
void DrawProperties(GuiState& state, TableData& properties);
