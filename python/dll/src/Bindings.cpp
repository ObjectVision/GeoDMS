#include "ShvDllInterface.h"
#include "TicInterface.h"
#include "ClcInterface.h"
#include "GeoInterface.h"
#include "StxInterface.h"
#include "RtcInterface.h"
#include "ShvUtils.h"

#include "dbg/Debug.h"
#include "dbg/DebugLog.h"
#include "dbg/DmsCatch.h"
#include "ptr/AutoDeletePtr.h"
#include "utl/Encodes.h"
#include "utl/Environment.h"
#include "utl/scoped_exit.h"
#include "utl/splitPath.h"

#include "libloaderapi.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataLocks.h"


#include <algorithm>
#include <filesystem>
#include <iostream>

#include <pybind11/pybind11.h>
namespace py = pybind11;

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

struct Item
{
	auto find(CharPtr itemPath) const
	{
		auto foundItem = m_item->FindItem(CharPtrRange(itemPath));
		return Item{ foundItem };
	}
	auto name() const
	{
		std::string name = m_item->GetName().c_str();
		return name;
	}

	SharedPtr<const TreeItem> m_item;
};

struct Engine
{
	Engine() 
	{
		TCHAR szPath[MAX_PATH];
		::GetModuleFileName((HINSTANCE)&__ImageBase, szPath, _MAX_PATH);
		auto dll_path = std::filesystem::path(szPath).remove_filename();

		DMS_Shv_Load();
		SHV_SetAdminMode(true);
		DMS_Appl_SetExeType(exe_type::unknown_run_or_dephi);
		DMS_Appl_SetExeDir(dll_path.string().c_str());
	}
	~Engine() 
	{
		if (m_root)
			m_root->EnableAutoDelete();
	}

	void init()
	{
	}

	void load_config(const std::string& config_name)
	{
		MG_CHECK(!m_root); // only init once
		m_root = CreateTreeFromConfiguration(config_name.c_str());
	}
	void create_config_root(CharPtr akaName)
	{
		MG_CHECK(!m_root); // only init once
		TokenID configName = GetTokenID_mt(akaName);
		m_root = TreeItem::CreateConfigRoot(configName);
	}

	Item get_root()
	{
		return Item{ m_root.get()};
	}

private:
	SharedPtr<TreeItem> m_root;
};



PYBIND11_MODULE(GeoDmsPython, m) {
	py::class_<Item>(m, "Item")
		.def("find", &Item::find)
		.def("name", &Item::name)
		;

	py::class_<Engine>(m, "Engine")
		.def(py::init())
		.def("loadConfig", &Engine::load_config)
		.def("createRoot", &Engine::create_config_root)
		.def("getRoot", &Engine::get_root)
	;

	m.def("version", DMS_GetVersion);
}