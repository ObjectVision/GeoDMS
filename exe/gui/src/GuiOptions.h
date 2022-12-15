#pragma once
#include "GuiBase.h"

class GuiOptions : GuiBaseComponent
{
public:
	GuiOptions();
	void Update(bool* p_open, GuiState& state);

private:
	//GuiState			      m_State;
	// CODE REVIEW: waarom niet SharedStr of std::string ?
	std::vector<char>		  m_LocalDataDirPath;
	std::vector<char>		  m_SourceDataDirPath;
	std::vector<char>		  m_DmsEditorPath;
};