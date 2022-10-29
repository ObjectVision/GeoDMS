#pragma once
#include "GuiBase.h"

void ShowEventLogOptionsWindow(bool* p_open);

class GuiOptions : GuiBaseComponent
{
public:
	GuiOptions();
	void Update(bool* p_open);

private:
	GuiState			      m_State;
	// CODE REVIEW: waarom niet SharedStr of std::string ?
	std::vector<char>		  m_LocalDataDirPath;
	std::vector<char>		  m_SourceDataDirPath;
	std::vector<char>		  m_DmsEditorPath;
};