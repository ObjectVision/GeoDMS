#pragma once
#include "GuiBase.h"

class GuiExport
{
public:
	GuiExport();
	void Update(bool* p_open, GuiState& state);

private:
	void SetStorageLocation();

	std::string m_folder_name;
	std::string m_file_name;
};