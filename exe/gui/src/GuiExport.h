#pragma once
#include "GuiBase.h"

enum class driver_characteristics : UInt32
{
	none = 0,
	is_raster = 0x01,
	native_is_default = 0x02,
	tableset_is_folder = 0x04,
};
inline bool operator &(driver_characteristics lhs, driver_characteristics rhs) { return UInt32(lhs) & UInt32(rhs); }
inline driver_characteristics operator |(driver_characteristics lhs, driver_characteristics rhs) { return driver_characteristics(UInt32(lhs) | UInt32(rhs)); }

struct gdal_driver_id
{
	CharPtr shortname = nullptr;
	CharPtr name = nullptr;
	CharPtr nativeName = nullptr;
	driver_characteristics drChars = driver_characteristics::none;

	CharPtr Caption()
	{
		if (name)
			return name;
		return shortname;
	}

	bool HasNativeVersion() { return nativeName; }

	bool IsEmpty()
	{
		return shortname == nullptr;
	}
};

class GuiExport
{
public:
	GuiExport();
	void Update(bool* p_open, GuiState& state);

	void CloseDialog(GuiState& state)
	{
		m_ExportRootItem = {};
		state.ShowExportWindow = false;
	}

private:
	void SetDefaultNativeDriverUsage();
	void SelectDriver(bool is_raster);
	void SetStorageLocation();
	bool DoExport();

	SharedPtr<const TreeItem> m_ExportRootItem;
	std::string m_Caption;

	std::vector<gdal_driver_id> m_available_drivers;
	bool           m_EnableVector = false, m_EnableRaster = false;
	gdal_driver_id m_selected_driver;
	bool           m_use_native_driver;
	std::string    m_folder_name;
	std::string    m_file_name;
};