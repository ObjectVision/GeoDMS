#pragma once
#include <vector>

#include "GuiBase.h"
#include "GuiMenu.h"
#include "GuiTreeview.h"
#include "GuiCurrentItem.h"
#include "GuiTableview.h"
#include "GuiView.h"
#include "GuiOptions.h"
#include "GuiDetailPages.h"
#include "GuiEventLog.h"
#include "GuiToolbar.h"
#include "GuiInput.h"
#include "GuiStatusBar.h"
#include "GuiUnitTest.h"

#include "AbstrDataItem.h"
#include "DataView.h"

class GuiMainComponent : GuiBaseComponent
{
public:
	GuiMainComponent();
	~GuiMainComponent();
	int MainLoop();
	int Init();
private:
	void Update();
	void ProcessEvent(GuiEvents e);
	void CloseCurrentConfig();
	bool ShowErrorDialogIfNecessary();
	bool ShowSourceFileChangeDialogIfNecessary();
	void TraverseTreeItemHistoryIfRequested();
	void InterpretCommandLineParameters();

	GLFWwindow*				m_Window = nullptr;
	GuiState				m_State;
	GuiInput			    m_Input;
	GuiMenuComponent		m_MenuComponent;
	GuiTreeViewComponent	m_TreeviewComponent;
	GuiCurrentItemComponent m_CurrentItemComponent;
	GuiToolbar              m_Toolbar;
	GuiEventLog		        m_EventLog;
	GuiView					m_View; // TODO: convert single view to multiview
	GuiOptions				m_Options;
	GuiStatusBar			m_StatusBar;
	GuiUnitTest				m_GuiUnitTest;
	GuiDetailPages			m_DetailPages;
	//GuiTreeItemsHolder		m_ItemsHolder;
	int 					m_FirstFrames = 1;
	bool                    m_NoConfig = false;

	std::unique_ptr<CDebugLog> m_DebugLog;
};