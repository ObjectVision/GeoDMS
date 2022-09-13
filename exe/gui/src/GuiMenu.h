#pragma once
#include <memory>

#include "GuiBase.h"
#include "GuiView.h"
#include "imfilebrowser.h"


class GuiMenuFileComponent : GuiBaseComponent
{
public:
	GuiMenuFileComponent();
	~GuiMenuFileComponent();
	void Update();
	ImGui::FileBrowser m_fileDialog; // https://github.com/AirGuanZ/imgui-filebrowser
private:
	void GetRecentAndPinnedFiles();
	void SetRecentAndPinnedFiles();
	void CleanRecentAndPinnedFiles();
	void UpdateRecentFilesByCurrentConfiguration();
	Int32 ConfigIsInRecentFiles(std::string cfg);
	void AddPinnedFile(std::string PinnedFile);
	void RemovePinnedFile(std::string PinnedFile);
	GuiState			     m_State;
	std::vector<std::string> m_PinnedFiles;
	std::vector<std::string> m_RecentFiles;
};

class GuiMenuEditComponent : GuiBaseComponent
{
public:
	void Update();

private:
	GuiState			      m_State;
};

class GuiMenuViewComponent : GuiBaseComponent
{
public:
	void Update();

private:
	GuiState			      m_State;
};

class GuiMenuToolsComponent : GuiBaseComponent
{
public:
	void Update();

private:
	GuiState			      m_State;
};

class GuiMenuWindowComponent : GuiBaseComponent
{
public:
	void Update(std::vector<GuiView>& TableViewsPtr, std::vector<GuiView>& MapViewsPtr);

private:
	GuiState			      m_State;

};

class GuiMenuHelpComponent : GuiBaseComponent
{
public:
	void Update();

private:
	GuiState			      m_State;

};

class GuiMenuComponent : GuiBaseComponent
{
public:
	void Update(std::vector<GuiView> & TableViewsPtr, std::vector<GuiView>& MapViewsPtr);
	GuiMenuFileComponent	  m_FileComponent;

private:
	GuiState			      m_State;
	GuiMenuEditComponent	  m_EditComponent;
	GuiMenuViewComponent	  m_ViewComponent;
	GuiMenuToolsComponent	  m_ToolsComponent;
	GuiMenuWindowComponent	  m_WindowComponent;
	GuiMenuHelpComponent	  m_HelpComponent;
};