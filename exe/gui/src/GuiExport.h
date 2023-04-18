#pragma once
#include "GuiBase.h"

enum class export_types
{
	//GPKG,
	// tabel
	// lijst tabellen
	// raster
	// lijst met rasters
};

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