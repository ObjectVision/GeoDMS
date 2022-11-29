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
	void Update(GuiState& state);
	ImGui::FileBrowser m_fileDialog; // https://github.com/AirGuanZ/imgui-filebrowser
private:
	void GetRecentAndPinnedFiles();
	void SetRecentAndPinnedFiles();
	std::vector<std::string>::iterator CleanRecentOrPinnedFiles(std::vector<std::string>& m_Files);
	void UpdateRecentOrPinnedFilesByCurrentConfiguration(GuiState& state, std::vector<std::string>& m_Files);
	Int32 ConfigIsInRecentOrPinnedFiles(std::string cfg, std::vector<std::string>& m_Files);
	void AddPinnedOrRecentFile(std::string PinnedFile, std::vector<std::string>& m_Files);
	void RemovePinnedOrRecentFile(std::string PinnedFile, std::vector<std::string>& m_Files);
	//GuiState			     m_State;
	std::vector<std::string> m_PinnedFiles;
	std::vector<std::string> m_RecentFiles;
};

class GuiMenuEditComponent : GuiBaseComponent
{
public:
	void Update(GuiState& state);

private:
};

class GuiMenuViewComponent : GuiBaseComponent
{
public:
	void Update(GuiState& state);

private:
};

class GuiMenuToolsComponent : GuiBaseComponent
{
public:
	void Update(GuiState& state);

private:
};

class GuiMenuWindowComponent : GuiBaseComponent
{
public:
	void Update(GuiView& ViewPtr);

private:
};

class GuiMenuHelpComponent : GuiBaseComponent
{
public:
	void Update(GuiState& state);

private:
};

class GuiMenuComponent : GuiBaseComponent
{
public:
	void Update(GuiState& state, GuiView& ViewPtr);
	GuiMenuFileComponent	  m_FileComponent;

private:
	GuiMenuEditComponent	  m_EditComponent;
	GuiMenuViewComponent	  m_ViewComponent;
	GuiMenuToolsComponent	  m_ToolsComponent;
	GuiMenuWindowComponent	  m_WindowComponent;
	GuiMenuHelpComponent	  m_HelpComponent;
};