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
#include "DataArray.h"

#include "libloaderapi.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataLocks.h"


#include <algorithm>
#include <filesystem>
#include <iostream>

#include <pybind11/pybind11.h>
#include <pybind11/cast.h>

namespace py = pybind11;

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace py_geodms
{
	/*struct Item
	{
		Item(const TreeItem* item)
			: m_item(item)
		{}

		//bool IsNull() const { return m_item.is_null(); }

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
			return IsDataItem(m_item.get_ptr());
		}
		bool is_unit_item() const
		{
			return IsUnit(m_item.get_ptr());
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

		SharedTreeItemInterestPtr m_item;
	};*/
	
	struct UnitItem
	{
		UnitItem(const AbstrUnit* au)
			: m_au(au)
		{}

		SharedPtr<const AbstrUnit> m_au;
	};

	struct FutureTile
	{

	};

	struct TileFunctor
	{
		TileFunctor(const AbstrDataObject* ado)
			: m_ado(ado)
		{}

		auto GetTile() -> FutureTile
		{
			// get tile
		}

		void data(tile_id tile)
		{

		}

		const AbstrDataObject* m_ado = nullptr;

	};

	// etc..

	/*struct DataItem
	{
		DataItem(const AbstrDataItem* adi)
			: m_adi(adi)
		{}

		auto GetAbstrDomainUnit() -> UnitItem
		{
			return UnitItem(m_adi->GetAbstrDomainUnit());
		}

		template <typename T>
		auto getDataObject() -> FutureData
		{
			using prepare_data = SharedPtr<typename TileFunctor<T>::future_tile>;

			auto tileRange_data = AsUnit(m_adi->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
			auto valuesUnit = debug_cast<const Unit<field_of_t<T>>*>(m_adi->GetAbstrValuesUnit());
			auto* ado = make_unique_FutureTileFunctor<T, prepare_data, false>(m_adi, false , tileRange_data, get_range_ptr_of_valuesunit(valuesUnit));
			return FutureData(ado.release());

			// covariant return typing, argumenten contravariant
			// get abstr tile, get tile
		}

		auto calculate() -> pybind11::memoryview
		{
			PreparedDataReadLock lck2(m_adi, "python::calculate");
			// data future, implemented as interest counter data item, type future data, tile functor
			// prepare data - data is scheduled for calculation
			// get van future data, per tile
			// interest count future op tile functor, tile functor 
			//auto count = adu->GetCount();
			//auto sz = adu->GetTileSizeAsI64Rect(no_tile);
		}


		SharedPtr<const AbstrDataItem> m_adi;
	};*/

	/*DataItem AsDataItem(const SharedDataItem& item)
	{
		MG_USERCHECK(item.is_data_item());
		return DataItem(::AsDataItem(item.m_item.get_ptr()));
	}*/

	/*UnitItem AsUnitItem(const Item& item)
	{
		MG_USERCHECK(item.is_unit_item());
		return UnitItem(::AsUnit(item.m_item.get_ptr()));
	}*/

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
				m_root.get()->EnableAutoDelete();
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

		auto get_root() const -> SharedTreeItemInterestPtr
		{
			return m_root.get_ptr();
		}

	private:
		SharedMutableTreeItem m_root;
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



void treeitem_CheckNonNull(const TreeItem* self)
{
	MG_USERCHECK2(self, "invalid dereference of item nullptr");
}

auto treeitem_find(SharedTreeItemInterestPtr self , CharPtr itemPath) -> SharedTreeItemInterestPtr // const TreeItem* self
{
	treeitem_CheckNonNull(self);
	auto foundItem = self->FindItem(CharPtrRange(itemPath));
	return foundItem;
}

auto treeitem_name(const TreeItem* self)
{
	treeitem_CheckNonNull(self);
	return self->GetID().AsStdString();
}

std::string treeitem_expr(const TreeItem* self)
{
	treeitem_CheckNonNull(self);
	return self->GetExpr().AsStdString();
}

auto treeitem_GetFirstSubItem(const TreeItem* self) -> SharedTreeItem
{
	treeitem_CheckNonNull(self);
	return self->GetFirstSubItem();

}

auto treeitem_GetNextItem(const TreeItem* self) -> SharedTreeItem
{
	treeitem_CheckNonNull(self);
	return self->GetNextItem();
}

auto treeitem_AsItem(const SharedMutableTreeItem& self) -> SharedTreeItem
{
	return self.get_ptr();
}

PYBIND11_MODULE(geodms, m) {
	py::class_<py_geodms::Engine>(m, "Engine")
		.def(py::init())
		.def("loadConfig", &py_geodms::Engine::load_config)
		.def("createRoot", &py_geodms::Engine::create_config_root)
		;

	py::class_<py_geodms::Config>(m, "Config")
		.def("getRoot", &py_geodms::Config::get_root);
	
	py::class_<SharedTreeItemInterestPtr>(m, "InterestItem")
		.def("update", [](SharedTreeItemInterestPtr self) 
			{
				DMS_TreeItem_Update(self.get_ptr());
				return DMS_NumericDataItem_GetStatistics(self.get_ptr(), nullptr);
			})//DMS_TreeItem_Update(self.get_ptr()); })// &DMS_TreeItem_Update)
		.def("find", &treeitem_find)
		.def("set_expr", [](SharedTreeItemInterestPtr self, const std::string& str) {return (const_cast<TreeItem*>(self.get_ptr())->SetExpr(SharedStr(str))); });

	py::class_<SharedMutableTreeItem>(m, "MutableItem")
		.def("asItem", &treeitem_AsItem);

	py::class_<SharedTreeItem>(m, "Item")
		.def("isNull", &SharedTreeItem::is_null)
		.def("find", &treeitem_find)
		.def("name", &treeitem_name)
		.def("expr", &treeitem_expr)
		/*.def("isDataItem", [](auto self) { return IsDataItem(self.get()); })
		.def("asDataItem", [](auto self) { return AsDataItem(self.get()); } )
		.def("isUnitItem", [](auto self) { return IsUnit(self.get()); } )
		.def("asUnitItem", [](auto self) { return AsUnit(self.get()); } )*/
		.def("firstSubItem", &treeitem_GetFirstSubItem)
		.def("nextItem", &treeitem_GetNextItem)
		;
	


	py::class_<SharedDataItem>(m, "DataItem")
		.def("getDomainUnit", [](SharedDataItem self) { return self->GetAbstrDomainUnit(); })
		//.def("calculate", );
		;

	/*py::class_<py_geodms::FutureData>(m, "FutureData")
		.def(GetTile);//<uint32_t>)
		//.def(GetTile<int32_t>)
	;*/

	py::class_<SharedUnit>(m, "UnitItem");





	m.def("version", DMS_GetVersion);
}