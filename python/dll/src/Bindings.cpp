// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

//#include "ShvDllInterface.h"
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

		bool IsNull() const { return m_item.is_null(); }

		auto find(CharPtr itemPath) const
		{
			CheckNonNull();
			auto foundItem = m_item->FindItem(CharPtrRange(itemPath));
			return Item{ foundItem };
		}
		auto name() const
		{
			CheckNonNull();
			return m_item->GetID().AsStdString();
		}
		bool is_data_item() const
		{
			return IsDataItem(m_item.get());
		}
		bool is_unit_item() const
		{
			return IsUnit(m_item.get());
		}
		std::string expr() const {
			CheckNonNull();
			return m_item->GetExpr().AsStdString();
		}

		Item GetFirstSubItem() const {
			CheckNonNull();
			return m_item->GetFirstSubItem();
		}
		Item GetNextItem() const {
			CheckNonNull();
			return m_item->GetNextItem();
		}

		void CheckNonNull() const
		{
			MG_USERCHECK2(m_item, "invalid dereference of item nullptr");
		}

		SharedPtr<const TreeItem> m_item;

	};

	struct DataItem
	{
		DataItem(const AbstrDataItem* adi)
			: m_adi(adi)
		{}

		SharedPtr<const AbstrDataItem> m_adi;
	};


	struct UnitItem
	{
		UnitItem(const AbstrUnit* au)
			: m_au(au)
		{}

		SharedPtr<const AbstrUnit> m_au;
	};


	DataItem AsDataItem(const Item& item)
	{
		MG_USERCHECK(item.is_data_item());
		return DataItem(::AsDataItem(item.m_item.get()));
	}

	UnitItem AsUnitItem(const Item& item)
	{
		MG_USERCHECK(item.is_unit_item());
		return UnitItem(::AsUnit(item.m_item.get()));
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

		Config(Config&& rhs) noexcept
			: m_root(std::move(rhs.m_root))
		{
			assert(currSingleConfig == &rhs);
			currSingleConfig = this;
			assert(!rhs.m_root);
		}

		~Config()
		{
			if (m_root)
			{
				m_root->EnableAutoDelete();
				MG_CHECK(currSingleConfig == this); // at any point of time, there is only one active Config
				currSingleConfig = nullptr;
			}
		}

		void check_unique()
		{
			MG_CHECK(s_currSingleEngine); // only Engines can create Configs.
			MG_USERCHECK2(!currSingleConfig, "Multiple simultaneous configuration not allowed within one process");
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
			MG_USERCHECK2(s_currSingleEngine == nullptr, "Engine should only be constructed once");
			s_currSingleEngine = this;

			TCHAR szPath[MAX_PATH];
			::GetModuleFileName((HINSTANCE)&__ImageBase, szPath, _MAX_PATH);
			auto dll_path = std::filesystem::path(szPath).remove_filename();

			DMS_Clc_Load();
			DMS_Geo_Load();
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

PYBIND11_MODULE(geodms, m) {
	py::class_<py_geodms::Item>(m, "Item")
		.def("isNull", &py_geodms::Item::IsNull)
		.def("find", &py_geodms::Item::find)
		.def("name", &py_geodms::Item::name)
		.def("expr", &py_geodms::Item::expr)
		.def("isDataItem", &py_geodms::Item::is_data_item)
		.def("isUnitItem", &py_geodms::Item::is_unit_item)
		.def("firstSubItem", &py_geodms::Item::GetFirstSubItem)
		.def("nextItem", &py_geodms::Item::GetNextItem)
		;

	//py::class_<py_geodms::DataItem>(m, "DataItem");
	//py::class_<py_geodms::DataItem>(m, "UnitItem");

	py::class_<py_geodms::Config>(m, "Config")
		.def("getRoot", &py_geodms::Config::get_root);

	py::class_<py_geodms::Engine>(m, "Engine")
		.def(py::init())
		.def("loadConfig", &py_geodms::Engine::load_config)
		.def("createRoot", &py_geodms::Engine::create_config_root)
	;

	m.def("AsDataItem", &py_geodms::AsDataItem);
	m.def("version", DMS_GetVersion);
}