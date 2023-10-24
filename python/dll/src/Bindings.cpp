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

namespace py_geodms
{
	struct Item
	{
		Item(const TreeItem* item)
			: m_item(item)
		{}
//		Item(Item&&) = default;
//		Item(const Item&) = default;

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
		bool is_data_item() const
		{
			return IsDataItem(m_item.get());
		}
//	private:
		SharedPtr<const TreeItem> m_item;
	};

	struct DataItem
	{
		DataItem(const AbstrDataItem* adi)
			: m_adi(adi)
		{}

		SharedPtr<const AbstrDataItem> m_adi;
	};


	DataItem AsDataItem(const Item& item)
	{
		return DataItem(::AsDataItem(item.m_item.get()));
	}

	struct Engine;
	static Engine* s_currSingleEngine = nullptr;

	struct Config
	{
		static Config* currSingleConfig;

		Config(CharPtr fileName)
		{
			check_unique();
			m_root = CreateTreeFromConfiguration(fileName);
			init();
		}
		Config(CharPtr akaName, int dummy)
		{
			check_unique();
			TokenID configName = GetTokenID_mt(akaName);
			m_root = TreeItem::CreateConfigRoot(configName);
			init();
		}

		Config(Config&& rhs)
			: m_root(std::move(rhs.m_root))
		{
			MG_CHECK(currSingleConfig == &rhs);
			currSingleConfig = this;
			MG_CHECK(!rhs.m_root);
		}

		~Config()
		{
			if (m_root)
			{
				m_root->EnableAutoDelete();
				MG_CHECK(currSingleConfig == this);
				currSingleConfig = nullptr;
			}
		}

		void check_unique()
		{
			MG_CHECK(s_currSingleEngine);
			MG_CHECK(!currSingleConfig);
		}

		void init()
		{
			currSingleConfig = this;
		}

		Item get_root() const
		{
			return Item( m_root.get() );
		}

	private:
		SharedPtr<TreeItem> m_root;
	};

	Config* Config::currSingleConfig = nullptr;


	struct Engine
	{
		Engine()
		{
			MG_CHECK(s_currSingleEngine == nullptr);
			s_currSingleEngine = this;

			TCHAR szPath[MAX_PATH];
			::GetModuleFileName((HINSTANCE)&__ImageBase, szPath, _MAX_PATH);
			auto dll_path = std::filesystem::path(szPath).remove_filename();

			DMS_Shv_Load();
			SHV_SetAdminMode(true);
			DMS_Appl_SetExeDir(dll_path.string().c_str()); // only call once
		}
		~Engine()
		{
		}

		Config load_config(CharPtr config_file_name)
		{
			return Config(config_file_name);
		}
		Config create_config_root(CharPtr akaName)
		{
			return Config(akaName, int(0));
		}
	};

} // namespace py_geodms

PYBIND11_MODULE(GeoDmsPython, m) {
	py::class_<py_geodms::Item>(m, "Item")
		.def("find", &py_geodms::Item::find)
		.def("name", &py_geodms::Item::name)
		.def("isDataItem", &py_geodms::Item::is_data_item)
		;

	py::class_<py_geodms::Config>(m, "Config")
		.def("getRoot", &py_geodms::Config::get_root)
		;
	py::class_<py_geodms::Engine>(m, "Engine")
		.def(py::init())
		.def("loadConfig", &py_geodms::Engine::load_config)
		.def("createRoot", &py_geodms::Engine::create_config_root)
	;

	m.def("version", DMS_GetVersion);
}