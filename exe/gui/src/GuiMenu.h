#pragma once
#include <memory>

#include "GuiBase.h"
#include "GuiView.h"
#include "imfilebrowser.h"


class GuiMenuFile : GuiBaseComponent
{
public:
	GuiMenuFile();
	~GuiMenuFile();
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

class GuiMenuEdit : GuiBaseComponent
{
public:
	void Update(GuiState& state);

private:
};

class GuiMenuView : GuiBaseComponent
{
public:
	void Update(GuiState& state);

private:
};

class GuiMenuTools : GuiBaseComponent
{
public:
	void Update(GuiState& state);

private:
};

class GuiMenuWindow : GuiBaseComponent
{
public:
	void Update(GuiView& ViewPtr);

private:
};

class GuiMenuHelp : GuiBaseComponent
{
public:
	void Update(GuiState& state);

private:
};

class GuiMenu : GuiBaseComponent
{
public:
	void Update(GuiState& state, GuiView& ViewPtr);
	GuiMenuFile	    m_File;

private:
	GuiMenuEdit		m_Edit;
	GuiMenuView		m_View;
	GuiMenuTools	m_Tools;
	GuiMenuWindow	m_Window;
	GuiMenuHelp		m_Help;
};