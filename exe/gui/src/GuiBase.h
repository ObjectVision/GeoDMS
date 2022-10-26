#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>

#include "TreeItem.h"
#include "AbstrDataItem.h"
#include "dbg/check.h"
#include "dbg/SeverityType.h"
#include "IconsFontRemixIcon.h"

class GuiTreeNode
{
private:
	TreeItem* m_TreeItem;
	ProgressState m_State = PS_None; // synchronized using TreeItem update callback
	std::vector<GuiTreeNode> m_Branches;
};

static void OnTreeItemChanged(ClientHandle clientHandle, const TreeItem* ti, NotificationCode state)
{
	int i = 0;
}

struct GuiSparseTree
{
	static std::mutex m_Lock; // synchronization primitive
	GuiTreeNode m_Root;
	std::map<TreeItem*, GuiTreeNode*> m_TreeItemToTreeNode;
};

enum GuiWindowOpenFlags
{
	GWOF_TreeView = 1,
	GWOF_DetailPages = 2,
	GWOF_EventLog = 4,
	GWOF_Options = 8,
	GWOF_ToolBar = 16,
	GWOF_CurrentItemBar = 32,
};

enum GuiEvents
{
	UpdateCurrentItem = 0,
	JumpToCurrentItem = 1,
	UpdateCurrentAndCompatibleSubItems = 2,
	ReopenCurrentConfiguration = 3,
	OpenNewMapViewWindow = 4,
	OpenNewTableViewWindow = 5,
	OpenNewConfiguration = 6,
	OpenInMemoryDataView = 7,
	OpenNewDefaultViewWindow = 8
};

class GuiTreeItemsHolder
{
public:
	bool contains(TreeItem* item);
	void add(TreeItem* item);
	void add(const AbstrDataItem* adi);
	void clear();
	SizeT size();
	std::vector<InterestPtr<TreeItem*>>& get();
private:
	std::vector<InterestPtr<TreeItem*>> m_TreeItems;
	std::vector<InterestPtr<const AbstrDataItem*>> m_DataHolder;
};

class EventQueue
{
public:
	bool HasEvents()
	{
		return not m_Queue.empty();
	}

	void Add(GuiEvents event)
	{
		m_Queue.push_back(event);
	}

	GuiEvents Pop()
	{
		auto event = m_Queue.back();
		m_Queue.pop_back();
		return event;
	}
private:
	std::vector<GuiEvents> m_Queue;
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
		UInt32 cap = 25;
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

struct OptionsEventLog
{
	bool ShowMessageTypeMinorTrace  = true;
	bool ShowMessageTypeMajorTrace  = true;
	bool ShowMessageTypeWarning     = true;
	bool ShowMessageTypeError       = true;
	bool ShowMessageTypeNothing     = true;
};

class GuiState
{
public:
	GuiState() {}
	GuiState(const GuiState&) = delete;
	void clear();

	// option window flags
	static bool ShowOptionsWindow;
	static bool ShowDetailPagesOptionsWindow;
	static bool ShowEventLogOptionsWindow;

	static bool ShowOpenFileWindow;
	static bool ShowConfigSource;
	static bool ShowTreeviewWindow;
	static bool ShowMapviewWindow;
	static bool ShowTableviewWindow;
	static bool ShowDetailPagesWindow;
	static bool ShowDemoWindow;
	static bool ShowEventLogWindow;
	static bool ShowToolbar;
	static bool ShowCurrentItemBar;
	static bool MapViewIsActive;
	static bool TableViewIsActive;

	static StringStateManager configFilenameManager;
	static StringStateManager errorDialogMessage;

	// jump to letter in TreeView
	static std::pair<std::string, std::string> m_JumpLetter;

	// sparse dms tree
	static GuiSparseTree m_SparseTree;

	// option structs
	static OptionsEventLog m_OptionsEventLog;

	// event queues
	static EventQueue MainEvents;
	static EventQueue CurrentItemBarEvents;
	static EventQueue TreeViewEvents;
	static EventQueue TableViewEvents;
	static EventQueue MapViewEvents;
	static EventQueue DetailPagesEvents;
	static EventQueue GuiMenuFileComponentEvents;

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
	static TreeItem* m_Root;
	static TreeItem* m_CurrentItem;
};

class GuiBaseComponent
{
public:
	GuiBaseComponent();
	~GuiBaseComponent();
	virtual void Update();
private:
};

// Helper functions
std::vector<std::string> DivideTreeItemFullNameIntoTreeItemNames(std::string fullname, std::string separator = "/");
TreeItem* SetJumpItemFullNameToOpenInTreeView(TreeItem* root, std::vector<std::string> SeparatedTreeItemFullName);
std::string GetExeFilePath();
ImVec2 SetCursorPosToOptionsIconInWindowHeader();
void   SetClipRectToIncludeOptionsIconInWindowHeader();
bool   MouseHooversOptionsIconInWindowHeader();
void   SetKeyboardFocusToThisHwnd();
void   LoadIniFromRegistry();
void   SaveIniToRegistry();