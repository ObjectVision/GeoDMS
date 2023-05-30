#pragma once
#include "GuiBase.h"

struct StyleOptions
{
	bool changed = false;
	bool dark_mode = false;
};

struct AdvancedOptions
{
	bool changed = false;
	std::string local_data_dir = {}; //!GetGeoDmsRegKey("LocalDataDir").empty() ? GetGeoDmsRegKey("LocalDataDir").c_str() : SetDefaultRegKey("LocalDataDir", "C:\\LocalData").c_str();;
	std::string source_data_dir = {};// !GetGeoDmsRegKey("SourceDataDir").empty() ? GetGeoDmsRegKey("SourceDataDir").c_str() : SetDefaultRegKey("SourceDataDir", "C:\\SourceData").c_str();
	std::string dms_editor_command = {}; // !GetGeoDmsRegKey("DmsEditor").empty() ? GetGeoDmsRegKey("DmsEditor").c_str() : SetDefaultRegKey("DmsEditor", """%env:ProgramFiles%\\Notepad++\\Notepad++.exe"" ""%F"" -n%L").c_str();
	bool pp0 = true;// IsMultiThreaded0();
	bool pp1 = true; //IsMultiThreaded1();
	bool pp2 = true;// IsMultiThreaded2();
	bool pp3 = true;// IsMultiThreaded3();
	DWORD flags = 0;// GetRegStatusFlags();
	int treshold_mem_flush = 100;
	bool tracelog_file = false;
};

struct Options
{
	StyleOptions style_options = {};
	AdvancedOptions advanced_options = {};
};

class GuiOptions
{
public:
	GuiOptions();
	void Update(bool* p_open, GuiState& state);

private:
	void ApplyChanges(GuiState& state);
	void RestoreAdvancedSettingsFromRegistry();
	void SetAdvancedOptionsInRegistry(GuiState& state);
	void SetAdvancedOptionsInCache();
	bool AdvancedOptionsStringValueHasBeenChanged(std::string key, std::string_view value);

	Options m_Options = {};
	//std::string m_LocalDataDirPath;
	//std::string m_SourceDataDirPath;
	//std::string m_DmsEditorPath;
};