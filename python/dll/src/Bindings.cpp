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

class Engine
{
public:
	Engine() {}
	~Engine() {}

	void init()
	{
		TCHAR szPath[MAX_PATH];
		::GetModuleFileName((HINSTANCE)&__ImageBase, szPath, _MAX_PATH);
		auto dll_path = std::filesystem::path(szPath).remove_filename();

		DMS_Shv_Load();
		SHV_SetAdminMode(true);
		DMS_Appl_SetExeType(exe_type::unknown_run_or_dephi);
		DMS_Appl_SetExeDir(dll_path.string().c_str());
	}

	void load(const std::string& config_name)
	{
		m_root = CreateTreeFromConfiguration(config_name.c_str());
	}

private:
	SharedPtr<TreeItem> m_root;
};

/*int add(int i, int j)
{
	return i + j;
}

PYBIND11_MODULE(example, m) {
	m.doc() = "pybind11 example plugin";

	m.def("add", &add, "A function that adds two numbers");
}*/


PYBIND11_MODULE(geodms, m) {
	py::class_<Engine>(m, "Engine")
		.def(py::init())
		.def("init", &Engine::init)
		.def("load", &Engine::load);
	m.def("version", DMS_GetVersion);
}