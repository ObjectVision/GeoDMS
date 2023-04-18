#pragma once
#include "GuiBase.h"

struct gdal_driver_id
{
	std::string shortname;
	std::string name;
	bool is_raster = false;
};

class GuiExport
{
public:
	GuiExport();
	void Update(bool* p_open, GuiState& state);

private:
	void SetDefaultNativeDriverUsage();
	void SelectDriver(bool is_raster);
	void SetStorageLocation();

	std::vector<gdal_driver_id> m_available_drivers;
	gdal_driver_id m_selected_driver;
	bool           m_use_native_driver;
	std::string    m_folder_name;
	std::string    m_file_name;
};