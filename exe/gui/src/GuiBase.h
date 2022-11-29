#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <list>
#include <queue>

#include "TreeItem.h"
#include "AbstrDataItem.h"
#include "dbg/check.h"
#include "dbg/SeverityType.h"
#include "IconsFontRemixIcon.h"

class GuiTreeNode
{
private:
	TreeItem* m_TreeItem = nullptr;
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
	GWOF_StatusBar = 64
};

enum class GuiEvents
{
	UpdateCurrentItem,
	JumpToCurrentItem,
	UpdateCurrentAndCompatibleSubItems,
	ReopenCurrentConfiguration,
	OpenNewMapViewWindow,
	OpenNewTableViewWindow,
	OpenNewConfiguration,
	OpenInMemoryDataView,
	OpenNewDefaultViewWindow,
	OpenConfigSource,
	ToggleShowTreeViewWindow,
	ToggleShowEventLogWindow,
	ToggleShowDetailPagesWindow,
	ToggleShowCurrentItemBar,
	ToggleShowToolbar,
	StepToErrorSource,
	StepToRootErrorSource,
	Close,
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
		m_Queue.push(event);
	}

	GuiEvents Pop()
	{
		auto event = m_Queue.front();
		m_Queue.pop();
		return event;
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

class TreeItemHistory
{
public:
	TreeItemHistory(); // TODO: TreeItem* alive state not guaranteed, replace with full item name lookup
	void Insert(TreeItem* new_item);
	TreeItem* GetNext();
	TreeItem* GetPrevious();
	std::list<TreeItem*>::iterator GetCurrentIterator();
	std::list<TreeItem*>::iterator GetBeginIterator();
	std::list<TreeItem*>::iterator GetEndIterator();
private:
	std::list<TreeItem*>::iterator m_Iterator;
	std::list<TreeItem*> m_History;
};

class GuiEventQueues
{
public:
	// singleton
	static GuiEventQueues* getInstance();
	~GuiEventQueues()
	{
		if (instance)
			delete instance;
	}

	// event queues
	EventQueue MainEvents;
	EventQueue CurrentItemBarEvents;
	EventQueue TreeViewEvents;
	EventQueue TableViewEvents;
	EventQueue MapViewEvents;
	EventQueue DetailPagesEvents;
	EventQueue GuiMenuFileComponentEvents;

private:
	GuiEventQueues() {}
	static GuiEventQueues* instance;
};



class GuiState
{
public:
	GuiState() {}
	~GuiState();
	GuiState(const GuiState&) = delete;
	auto clear() -> void;

	int return_value					= 0;

	// option window flags
	bool ShowOptionsWindow				= false;
	bool ShowDetailPagesOptionsWindow	= false;
	bool ShowEventLogOptionsWindow		= false;
	bool ShowOpenFileWindow				= false;
	bool ShowConfigSource				= false;
	bool ShowTreeviewWindow				= false;
	bool ShowDetailPagesWindow			= false;
	bool ShowDemoWindow					= false;
	bool ShowEventLogWindow				= false;
	bool ShowToolbar					= false;
	bool ShowStatusBar					= false;
	bool ShowCurrentItemBar				= false;
	bool MapViewIsActive				= false;
	bool TableViewIsActive				= false;

	StringStateManager configFilenameManager;
	StringStateManager errorDialogMessage;
	StringStateManager contextMessage;

	// history
	TreeItemHistory TreeItemHistoryList;

	// jump to letter in TreeView
	std::pair<std::string, std::string> m_JumpLetter; //TODO: on the fly lookup, synchronize evaluation with key press

	// option structs
	OptionsEventLog m_OptionsEventLog;



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

class GuiBaseComponent
{
public:
	GuiBaseComponent();
	~GuiBaseComponent();
	virtual void Update();
private:
};

// Helper functions
auto DivideTreeItemFullNameIntoTreeItemNames(std::string fullname, std::string separator = "/") -> std::vector<std::string>;
auto GetExeFilePath() -> std::string;
ImVec2 SetCursorPosToOptionsIconInWindowHeader();
void   SetClipRectToIncludeOptionsIconInWindowHeader();
bool   MouseHooversOptionsIconInWindowHeader();
void   SetKeyboardFocusToThisHwnd();
void   LoadIniFromRegistry();
void   SaveIniToRegistry();