#pragma once
#include "GuiBase.h"

class GuiOptions : GuiBaseComponent
{
public:
	GuiOptions();
	void Update(bool* p_open, GuiState& state);

private:
	std::string m_LocalDataDirPath;
	std::string m_SourceDataDirPath;
	std::string m_DmsEditorPath;
};