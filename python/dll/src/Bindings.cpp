// Copyright (C) 1998-2023 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
#define PYBIND11_DETAILED_ERROR_MESSAGES
//#include "ShvDllInterface.h"
#include "TicInterface.h"
#include "ClcInterface.h"
#include "GeoInterface.h"
#include "StxInterface.h"
#include "RtcInterface.h"
#include "PropDefInterface.h"
#include "ShvUtils.h"

#include "dbg/debug.h"
#include "dbg/DebugLog.h"
#include "dbg/DmsCatch.h"
#include "ptr/AutoDeletePtr.h"
#include "utl/Encodes.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "utl/scoped_exit.h"
#include "act/MainThread.h" // SetMainThreadID
#include "utl/splitPath.h"
#include "DataArray.h"

#include "libloaderapi.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataLocks.h"
#include "DbgInterface.h"     // DMS_RegisterMsgCallback, MsgData
#include "OperationContext.h" // tg_maintainer: manages the global operation-context task group
#include "SessionData.h"      // SessionData: per-configuration session context
#include "TreeItemFlags.h"    // TSF_HasConfigData
#include "UnitClass.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "mci/ValueComposition.h"


#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/cast.h>
#include <pybind11/stl.h>



namespace py = pybind11;

namespace py_geodms
{
	//----------------------------------------------------------------------
	// helper: resolve a script value-type name (e.g. "float64", "uint32",
	//         "spoint", "string") to its UnitClass.
	//----------------------------------------------------------------------
	static const UnitClass* UnitClassFromValueTypeName(CharPtr valueTypeName)
	{
		const ValueClass* vc = ValueClass::FindByScriptName(GetTokenID_mt(valueTypeName));
		MG_USERCHECK2(vc, "unknown value type name; expected a basic type such as 'float64', 'uint32', 'spoint' or 'string'");
		return DMS_UnitClass_Find(vc);
	}

	//----------------------------------------------------------------------
	// Unit wrappers
	//----------------------------------------------------------------------
	struct MutableUnitItem;

	struct UnitItem
	{
		UnitItem(const AbstrUnit* au)
			: m_au(make_shared_tree(au, existing_obj{}))
		{}
		UnitItem(const MutableUnitItem& rhs);

		std::shared_ptr<const AbstrUnit> m_au;
	};

	struct MutableUnitItem
	{
		MutableUnitItem(AbstrUnit* au)
			: m_au(make_shared_tree(au, existing_obj{}))
		{}

		UnitItem asConst() const { return UnitItem(m_au.get()); }

		std::shared_ptr<AbstrUnit> m_au;
	};

	inline UnitItem::UnitItem(const MutableUnitItem& rhs)
		: m_au(make_shared_tree(rhs.m_au.get(), existing_obj{}))
	{}

	//----------------------------------------------------------------------
	// DataItem wrappers
	//----------------------------------------------------------------------
	struct DataItem
	{
		DataItem(const AbstrDataItem* adi)
			: m_adi(make_shared_tree(adi, existing_obj{}))
		{}

		auto GetAbstrDomainUnit() -> UnitItem
		{
			return UnitItem(m_adi->GetAbstrDomainUnit());
		}

		auto GetAbstrValuesUnit() -> UnitItem
		{
			return UnitItem(m_adi->GetAbstrValuesUnit());
		}

		auto LockAndGetStringValue(SizeT i) -> std::string
		{
			MG_USERCHECK2(m_adi.get(), "invalid dereference of a null data item");
			SharedTreeItemInterestPtr ip(m_adi.get()); // hold interest so calculated items compute
			return m_adi->LockAndGetValue<SharedStr>(i).c_str();
		}

		std::shared_ptr<const AbstrDataItem> m_adi;
	};

	struct MutableDataItem
	{
		MutableDataItem(AbstrDataItem* adi)
			: m_adi(make_shared_tree(adi, existing_obj{}))
		{}

		DataItem asDataItem()
		{
			return DataItem(m_adi.get());
		}

		std::shared_ptr<AbstrDataItem> m_adi;
	};

	//----------------------------------------------------------------------
	// TreeItem wrappers
	//----------------------------------------------------------------------
	struct ConstTreeItem
	{
		SharedTreeItem item;
	};

	struct MutableTreeItem
	{
		SharedMutableTreeItem item;
	};

	struct Engine;
	static Engine* s_currSingleEngine = nullptr;

	struct Config
	{
		static Config* currSingleConfig;

		Config(CharPtr fileName)
		{
			check_unique();
			m_root = make_shared_tree(CreateTreeFromConfiguration(fileName), existing_obj{});
			init();
		}
		Config(CharPtr akaName, int dummy)
		{
			check_unique();
			TokenID configName = GetTokenID_mt(akaName);
			// Mirror DMS_CreateTreeFromString: a SessionData must be created and opened on
			// the root, otherwise SessionData::Curr() is unset and expression evaluation /
			// Primary Data Access produce undefined results.
			m_session = SessionData::Create(GetCurrentDir().c_str(), "");
			m_root = TreeItem::CreateConfigRoot(configName);
			m_session->Open(m_root.get());
			init();
		}

		Config(Config&& rhs) noexcept
			: m_root(std::move(rhs.m_root))
			, m_session(std::move(rhs.m_session))
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

		auto get_root() -> MutableTreeItem
		{
			return MutableTreeItem(make_shared_tree(m_root.get(), existing_obj{}));
		}

		auto get_root_non_mutable() -> ConstTreeItem
		{
			return ConstTreeItem(make_shared_tree(m_root.get(), existing_obj{}));
		}

	private:
		SharedMutableTreeItem m_root = nullptr;
		std::shared_ptr<SessionData> m_session;
	};

	Config* Config::currSingleConfig = nullptr;

	static void DMS_CONV py_geodms_msg_callback(ClientHandle, const MsgData* msgData, bool)
	{
		if (msgData)
			std::cerr << "[geodms] " << msgData->m_Txt.c_str() << std::endl;
	}

	struct Engine
	{
		Engine()
		{
			MG_USERCHECK2(s_currSingleEngine == nullptr, "Engine should only be constructed once");
			s_currSingleEngine = this;

			// Identify the main thread here, on the thread that constructs the
			// single Engine (formerly done via DMS_Appl_SetExeDir). The exe-root
			// dir is now self-determined by GetExeDir() from the Rtc module's
			// own location, so it no longer needs to be conveyed.
			SetMainThreadID();

			DMS_RegisterMsgCallback(py_geodms_msg_callback, nullptr);

			DMS_Clc_Load();
			DMS_Geo_Load();
			DMS_Stx_Load(); // initialize the expression parser so runtime set_expr() can be compiled

			// Initialize the global operation-context task group (the worker pool used
			// by data calculation / Primary Data Access). The exe entry points create a
			// stack-scoped tg_maintainer in main(); for the embedded engine we tie its
			// lifetime to the Engine. Without it, any data access dereferences a null
			// task-group singleton (release builds skip the assert -> access violation).
			m_taskGroup = std::make_unique<tg_maintainer>();
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

		// Resolve a basic value-type name to its shared, non-ranged default unit,
		// e.g. default_unit("float64") yields the values unit for floating-point attributes.
		UnitItem default_unit(CharPtr valueTypeName)
		{
			return UnitItem(DMS_GetDefaultUnit(UnitClassFromValueTypeName(valueTypeName)));
		}

		// The Void unit; the domain of any parameter.
		UnitItem void_unit()
		{
			return UnitItem(DMS_GetDefaultUnit(DMS_VoidUnit_GetStaticClass()));
		}

	private:
		std::unique_ptr<tg_maintainer> m_taskGroup;
	};

} // namespace py_geodms



//----------------------------------------------------------------------
// TreeItem free functions (querying + building)
//----------------------------------------------------------------------

void treeitem_CheckNonNull_const(py_geodms::ConstTreeItem self) {
	MG_USERCHECK2(self.item, "invalid dereference of item nullptr");
}

void treeitem_CheckNonNull_mutable(py_geodms::MutableTreeItem self) {
	MG_USERCHECK2(self.item, "invalid dereference of item nullptr");
}

auto treeitem_find_const(py_geodms::ConstTreeItem self, CharPtr itemPath) -> py_geodms::ConstTreeItem { // const TreeItem* self
	treeitem_CheckNonNull_const(self);
	auto foundItem = self.item->FindItem(CharPtrRange(itemPath));
	return py_geodms::ConstTreeItem(foundItem);
}

auto treeitem_find_mutable(py_geodms::MutableTreeItem self, CharPtr itemPath) -> py_geodms::MutableTreeItem { // const TreeItem* self
	treeitem_CheckNonNull_mutable(self);
	auto foundItem = self.item->FindItem(CharPtrRange(itemPath));

	return py_geodms::MutableTreeItem(make_shared_tree(const_cast<TreeItem*>(foundItem.get()), existing_obj{})); // TODO: future improvement: use GetItem to stay non-const.
}

auto treeitem_name_const(py_geodms::ConstTreeItem self) -> std::string {
	treeitem_CheckNonNull_const(self);
	return self.item->GetID().AsStdString();
}

auto treeitem_fullname_const(py_geodms::ConstTreeItem self) -> std::string {
	treeitem_CheckNonNull_const(self);
	return self.item->GetFullName().AsStdString();
}

auto treeitem_expr_const(py_geodms::ConstTreeItem self) -> std::string {
	treeitem_CheckNonNull_const(self);
	return self.item->GetExpr().AsStdString();
}

auto treeitem_descr_const(py_geodms::ConstTreeItem self) -> std::string {
	treeitem_CheckNonNull_const(self);
	return self.item->GetDescr().AsStdString();
}

auto treeitem_name_mutable(py_geodms::MutableTreeItem self) -> std::string {
	treeitem_CheckNonNull_mutable(self);
	return self.item->GetID().AsStdString();
}

auto treeitem_fullname_mutable(py_geodms::MutableTreeItem self) -> std::string {
	treeitem_CheckNonNull_mutable(self);
	return self.item->GetFullName().AsStdString();
}

auto treeitem_expr_mutable(py_geodms::MutableTreeItem self) -> std::string {
	treeitem_CheckNonNull_mutable(self);
	return self.item->GetExpr().AsStdString();
}

auto treeitem_GetFirstSubItem(py_geodms::ConstTreeItem self) -> py_geodms::ConstTreeItem {
	treeitem_CheckNonNull_const(self);
	return py_geodms::ConstTreeItem(make_shared_tree(self.item->GetFirstSubItem(), existing_obj{}));
}

auto treeitem_GetNextItem(py_geodms::ConstTreeItem self) -> py_geodms::ConstTreeItem {
	treeitem_CheckNonNull_const(self);
	return py_geodms::ConstTreeItem(make_shared_tree(self.item->GetNextItem(), existing_obj{}));
}

auto treeitem_subitems_const(py_geodms::ConstTreeItem self) -> std::vector<py_geodms::ConstTreeItem> {
	treeitem_CheckNonNull_const(self);
	std::vector<py_geodms::ConstTreeItem> result;
	for (auto si = self.item->GetFirstSubItem(); si; si = si->GetNextItem())
		result.push_back(py_geodms::ConstTreeItem(make_shared_tree(si, existing_obj{})));
	return result;
}

auto treeitem_subitems_mutable(py_geodms::MutableTreeItem self) -> std::vector<py_geodms::MutableTreeItem> {
	treeitem_CheckNonNull_mutable(self);
	std::vector<py_geodms::MutableTreeItem> result;
	for (auto si = self.item->GetFirstSubItem(); si; si = si->GetNextItem())
		result.push_back(py_geodms::MutableTreeItem(make_shared_tree(const_cast<TreeItem*>(si), existing_obj{})));
	return result;
}

auto treeitem_parent_const(py_geodms::ConstTreeItem self) -> py_geodms::ConstTreeItem {
	treeitem_CheckNonNull_const(self);
	return py_geodms::ConstTreeItem(make_shared_tree(DMS_TreeItem_GetParent(self.item.get()), existing_obj{}));
}

auto treeitem_parent_mutable(py_geodms::MutableTreeItem self) -> py_geodms::MutableTreeItem {
	treeitem_CheckNonNull_mutable(self);
	return py_geodms::MutableTreeItem(make_shared_tree(const_cast<TreeItem*>(DMS_TreeItem_GetParent(self.item.get())), existing_obj{}));
}

auto treeitem_fail_reason(py_geodms::ConstTreeItem self) -> std::string {
	treeitem_CheckNonNull_const(self);
	auto handle = DMS_TreeItem_GetFailReasonAsIString(self.item.get());
	if (!handle)
		return std::string();
	CharPtr str = DMS_IString_AsCharPtr(handle);
	std::string result = str ? str : "";
	DMS_IString_Release(handle);
	return result;
}

//----------------------------------------------------------------------
// TreeItem building free functions (mutable only)
//----------------------------------------------------------------------

auto treeitem_add_container(py_geodms::MutableTreeItem self, const std::string& name) -> py_geodms::MutableTreeItem {
	treeitem_CheckNonNull_mutable(self);
	TreeItem* ti = DMS_CreateTreeItem(self.item.get(), name.c_str());
	return py_geodms::MutableTreeItem(make_shared_tree(ti, existing_obj{}));
}

auto treeitem_create_unit(py_geodms::MutableTreeItem self, const std::string& name, const std::string& valueType) -> py_geodms::MutableUnitItem {
	treeitem_CheckNonNull_mutable(self);
	const UnitClass* uc = py_geodms::UnitClassFromValueTypeName(valueType.c_str());
	AbstrUnit* au = DMS_CreateUnit(self.item.get(), name.c_str(), uc);
	return py_geodms::MutableUnitItem(au);
}

auto treeitem_add_data_item(py_geodms::MutableTreeItem self, const std::string& name,
	py_geodms::UnitItem domain, py_geodms::UnitItem values, ValueComposition vc) -> py_geodms::MutableDataItem {
	treeitem_CheckNonNull_mutable(self);
	AbstrDataItem* adi = DMS_CreateDataItem(self.item.get(), name.c_str(), domain.m_au.get(), values.m_au.get(), vc);
	return py_geodms::MutableDataItem(adi);
}

auto treeitem_add_attribute(py_geodms::MutableTreeItem self, const std::string& name,
	py_geodms::UnitItem domain, const std::string& valuesValueType) -> py_geodms::MutableDataItem {
	treeitem_CheckNonNull_mutable(self);
	const AbstrUnit* values = DMS_GetDefaultUnit(py_geodms::UnitClassFromValueTypeName(valuesValueType.c_str()));
	AbstrDataItem* adi = DMS_CreateDataItem(self.item.get(), name.c_str(), domain.m_au.get(), values, ValueComposition::Single);
	return py_geodms::MutableDataItem(adi);
}

auto treeitem_add_param(py_geodms::MutableTreeItem self, const std::string& name, const std::string& valuesValueType) -> py_geodms::MutableDataItem {
	treeitem_CheckNonNull_mutable(self);
	const AbstrUnit* voidUnit = DMS_GetDefaultUnit(DMS_VoidUnit_GetStaticClass());
	const AbstrUnit* values = DMS_GetDefaultUnit(py_geodms::UnitClassFromValueTypeName(valuesValueType.c_str()));
	AbstrDataItem* adi = DMS_CreateDataItem(self.item.get(), name.c_str(), voidUnit, values, ValueComposition::Single);
	return py_geodms::MutableDataItem(adi);
}

void treeitem_set_storage_manager(py_geodms::MutableTreeItem self, const std::string& storageName, const std::string& storageType, bool readOnly) {
	treeitem_CheckNonNull_mutable(self);
	self.item->SetStorageManager(storageName.c_str(), storageType.c_str(),
		readOnly ? StorageReadOnlySetting::ReadOnly : StorageReadOnlySetting::ReadWrite);
}

//----------------------------------------------------------------------
// DataItem primary data access free functions
//----------------------------------------------------------------------

// Bulk-read all values of an attribute. We address tiles through the data object's own
// GetTiledLocation rather than the DMS_NumericAttr_Get*Array C functions, which resolve
// the tile location via GetAbstrDomainUnit()->GetTiledRangeData() — null for a domain
// whose range is defined by an expression (the range then lives in its current range item).
auto dataitem_get_values_as_float_list(py_geodms::DataItem self) -> std::vector<Float64> {
	const AbstrDataItem* adi = self.m_adi.get();
	MG_USERCHECK2(adi, "invalid dereference of a null data item");
	SharedTreeItemInterestPtr ip(adi); // hold interest so a calculated item is computed and kept alive
	SizeT n = DMS_Unit_GetCount(adi->GetAbstrDomainUnit());
	std::vector<Float64> result(n);
	if (n) {
		PreparedDataReadLock dlr(adi, "geodms::get_values_as_float_list");
		auto ado = adi->GetRefObj();
		SizeT index = 0, len = n; Float64* ptr = result.data();
		while (len) {
			SizeT nrRead = ado->GetValuesAsFloat64Array(ado->GetTiledLocation(index), len, ptr);
			if (!nrRead) break;
			len -= nrRead; ptr += nrRead; index += nrRead;
		}
	}
	return result;
}

auto dataitem_get_values_as_int_list(py_geodms::DataItem self) -> std::vector<Int32> {
	const AbstrDataItem* adi = self.m_adi.get();
	MG_USERCHECK2(adi, "invalid dereference of a null data item");
	SharedTreeItemInterestPtr ip(adi); // hold interest so a calculated item is computed and kept alive
	SizeT n = DMS_Unit_GetCount(adi->GetAbstrDomainUnit());
	std::vector<Int32> result(n);
	if (n) {
		PreparedDataReadLock dlr(adi, "geodms::get_values_as_int_list");
		auto ado = adi->GetRefObj();
		SizeT index = 0, len = n; Int32* ptr = result.data();
		while (len) {
			SizeT nrRead = ado->GetValuesAsInt32Array(ado->GetTiledLocation(index), len, ptr);
			if (!nrRead) break;
			len -= nrRead; ptr += nrRead; index += nrRead;
		}
	}
	return result;
}

// Read a single value, holding interest so calculated items compute. Used by the scalar
// get_value_as_* and parameter accessors.
auto dataitem_get_value_as_float(const AbstrDataItem* adi, SizeT i) -> Float64 {
	MG_USERCHECK2(adi, "invalid dereference of a null data item");
	SharedTreeItemInterestPtr ip(adi);
	PreparedDataReadLock dlr(adi, "geodms::get_value_as_float");
	return adi->GetRefObj()->GetValueAsFloat64(i);
}

auto dataitem_get_value_as_int(const AbstrDataItem* adi, SizeT i) -> Int32 {
	MG_USERCHECK2(adi, "invalid dereference of a null data item");
	SharedTreeItemInterestPtr ip(adi);
	PreparedDataReadLock dlr(adi, "geodms::get_value_as_int");
	return adi->GetRefObj()->GetValueAsInt32(i);
}

// Write primary data into a fresh in-memory attribute. We open the DataWriteLock in
// write_only_all mode (fresh allocation) rather than the read_write mode used by the
// DMS_NumericAttr_Set*Array C functions, which clone the previous data object and so
// crash on an attribute that has no data yet. The domain range is prepared first so the
// data object can be sized; the values list length should equal the domain element count.
void dataitem_set_values_from_float_list(py_geodms::MutableDataItem self, const std::vector<Float64>& data) {
	AbstrDataItem* adi = self.m_adi.get();
	MG_USERCHECK2(adi, "invalid dereference of a null data item");
	adi->SetTSF(TSF_HasConfigData); // mark as authoritative primary data so dependents don't recompute it
	DMS_Unit_GetCount(adi->GetAbstrDomainUnit());
	DataWriteLock lock(adi, dms_rw_mode::write_only_all);
	if (!data.empty())
		lock->SetValuesAsFloat64Array(lock->GetTiledLocation(0), data.size(), data.data());
	lock.Commit();
}

void dataitem_set_values_from_int_list(py_geodms::MutableDataItem self, const std::vector<Int32>& data) {
	AbstrDataItem* adi = self.m_adi.get();
	MG_USERCHECK2(adi, "invalid dereference of a null data item");
	adi->SetTSF(TSF_HasConfigData); // mark as authoritative primary data so dependents don't recompute it
	DMS_Unit_GetCount(adi->GetAbstrDomainUnit());
	DataWriteLock lock(adi, dms_rw_mode::write_only_all);
	if (!data.empty())
		lock->SetValuesAsInt32Array(lock->GetTiledLocation(0), data.size(), data.data());
	lock.Commit();
}

PYBIND11_MODULE(geodms, m) {
	m.doc() = "Python bindings for the GeoDMS Data & Model Server: read/query a configuration, "
	          "set parameter values, build an in-memory configuration without a model script, "
	          "and query results via Primary Data Access.";

	// meta data
	m.def("version", DMS_GetVersion, "GeoDMS version string");

	// value composition of an attribute / values unit
	py::enum_<ValueComposition>(m, "ValueComposition")
		.value("Single", ValueComposition::Single)
		.value("Polygon", ValueComposition::Polygon)
		.value("Sequence", ValueComposition::Sequence)
		.value("MultiPoint", ValueComposition::MultiPoint)
		;

	// the value type of a unit, as an enumeration (no type-system structure is exposed)
	py::enum_<ValueClassID>(m, "ValueTypeId")
		.value("UInt32", ValueClassID::VT_UInt32)
		.value("Int32", ValueClassID::VT_Int32)
		.value("UInt16", ValueClassID::VT_UInt16)
		.value("Int16", ValueClassID::VT_Int16)
		.value("UInt8", ValueClassID::VT_UInt8)
		.value("Int8", ValueClassID::VT_Int8)
		.value("UInt64", ValueClassID::VT_UInt64)
		.value("Int64", ValueClassID::VT_Int64)
		.value("Float64", ValueClassID::VT_Float64)
		.value("Float32", ValueClassID::VT_Float32)
		.value("Bool", ValueClassID::VT_Bool)
		.value("UInt4", ValueClassID::VT_UInt4)
		.value("SPoint", ValueClassID::VT_SPoint)
		.value("WPoint", ValueClassID::VT_WPoint)
		.value("IPoint", ValueClassID::VT_IPoint)
		.value("UPoint", ValueClassID::VT_UPoint)
		.value("FPoint", ValueClassID::VT_FPoint)
		.value("DPoint", ValueClassID::VT_DPoint)
		.value("String", ValueClassID::VT_SharedStr)
		.value("Void", ValueClassID::VT_Void)
		.value("Unknown", ValueClassID::VT_Unknown)
		;

	// engine
	py::class_<py_geodms::Engine>(m, "Engine")
		.def(py::init())
		.def("load_config", &py_geodms::Engine::load_config, "Load a configuration from a .dms file and return its Config")
		.def("create_config_root", &py_geodms::Engine::create_config_root, "Create an empty in-memory configuration with the given root name")
		.def("default_unit", &py_geodms::Engine::default_unit, "Return the shared default unit for a basic value-type name (e.g. 'float64')")
		.def("void_unit", &py_geodms::Engine::void_unit, "Return the Void unit, used as the domain of parameters")
		;

	// config
	py::class_<py_geodms::Config>(m, "Config")
		.def("root", &py_geodms::Config::get_root, "Return the mutable root container of the configuration")
		.def("const_root", &py_geodms::Config::get_root_non_mutable, "Return the read-only root container of the configuration")
		;

	// non-mutable treeitem
	py::class_<py_geodms::ConstTreeItem>(m, "ConstTreeItem")
		.def("is_null", [](py_geodms::ConstTreeItem self) {return self.item== nullptr; })
		.def("find", &treeitem_find_const, "Find a sub-item by relative or absolute path")
		.def("name", &treeitem_name_const)
		.def("full_name", &treeitem_fullname_const)
		.def("expr", &treeitem_expr_const)
		.def("descr", &treeitem_descr_const)
		.def("first_subitem", &treeitem_GetFirstSubItem)
		.def("next", &treeitem_GetNextItem)
		.def("sub_items", &treeitem_subitems_const, "Return a list of all direct sub-items")
		.def("parent", &treeitem_parent_const)
		.def("fail_reason", &treeitem_fail_reason, "Failure reason string, or empty when the item is valid")
		.def("update", [](py_geodms::ConstTreeItem self) { treeitem_CheckNonNull_const(self); DMS_TreeItem_Update(self.item.get()); }, "Force (re)calculation of this item and its suppliers")
		.def("isDataItem", [](py_geodms::ConstTreeItem self) -> bool { return IsDataItem(self.item.get()); })
		.def("asDataItem", [](py_geodms::ConstTreeItem self) -> py_geodms::DataItem { return AsDataItem(self.item.get()); })
		.def("isUnitItem", [](py_geodms::ConstTreeItem self) -> bool { return IsUnit(self.item.get()); })
		.def("asUnitItem", [](py_geodms::ConstTreeItem self) -> py_geodms::UnitItem{ return AsUnit(self.item.get()); })
		;

	// mutable treeitem
	py::class_<py_geodms::MutableTreeItem>(m, "MutableTreeItem")
		.def("is_null", [](py_geodms::MutableTreeItem self) {return self.item== nullptr; })
		.def("find", &treeitem_find_mutable, "Find a sub-item by relative or absolute path")
		.def("name", &treeitem_name_mutable)
		.def("full_name", &treeitem_fullname_mutable)
		.def("expr", &treeitem_expr_mutable)
		.def("asConst", [](py_geodms::MutableTreeItem self) -> py_geodms::ConstTreeItem { return { make_shared_tree(self.item.get(), existing_obj{}) }; })
		.def("sub_items", &treeitem_subitems_mutable, "Return a list of all direct sub-items")
		.def("parent", &treeitem_parent_mutable)
		.def("update", [](py_geodms::MutableTreeItem self) { treeitem_CheckNonNull_mutable(self); DMS_TreeItem_Update(self.item.get()); }, "Force (re)calculation of this item and its suppliers")
		.def("set_expr", [](py_geodms::MutableTreeItem self, const std::string& str) { treeitem_CheckNonNull_mutable(self); self.item->SetExpr(SharedStr(str)); }, "Set the calculation expression of this item")
		.def("set_descr", [](py_geodms::MutableTreeItem self, const std::string& str) { treeitem_CheckNonNull_mutable(self); self.item->SetDescr(SharedStr(str)); }, "Set the description property of this item")
		// configuration building
		.def("add_container", &treeitem_add_container, "Create and return a new sub-container with the given name")
		.def("create_unit", &treeitem_create_unit, "Create a named unit of the given basic value type (e.g. 'uint32', 'spoint')")
		.def("add_data_item", &treeitem_add_data_item,
			py::arg("name"), py::arg("domain"), py::arg("values"), py::arg("vc") = ValueComposition::Single,
			"Create an attribute with the given domain unit, values unit and value composition")
		.def("add_attribute", &treeitem_add_attribute,
			py::arg("name"), py::arg("domain"), py::arg("values_value_type"),
			"Create an attribute over the given domain whose values unit is the default unit of a basic value type")
		.def("add_param", &treeitem_add_param,
			py::arg("name"), py::arg("values_value_type"),
			"Create a parameter (Void domain) whose values unit is the default unit of a basic value type")
		.def("set_storage_manager", &treeitem_set_storage_manager,
			py::arg("storage_name"), py::arg("storage_type"), py::arg("read_only") = true,
			"Attach a storage manager (e.g. type 'gdal.vect', 'gdal.grid') to this item")
		.def("disable_storage", [](py_geodms::MutableTreeItem self) { self.item->DisableStorage(); }, "Force in-memory / calculator-only operation")
		.def("isDataItem", [](py_geodms::MutableTreeItem self) -> bool { return IsDataItem(self.item.get()); })
		.def("asDataItem", [](py_geodms::MutableTreeItem self) -> py_geodms::MutableDataItem { return AsDataItem(self.item.get()); })
		.def("isUnitItem", [](py_geodms::MutableTreeItem self) -> bool { return IsUnit(self.item.get()); })
		.def("asUnitItem", [](py_geodms::MutableTreeItem self) -> py_geodms::MutableUnitItem { return AsUnit(self.item.get()); })
		;

	// const unit
	py::class_<py_geodms::UnitItem>(m, "UnitItem")
		.def("is_null", [](py_geodms::UnitItem self) {return self.m_au== nullptr; })
		.def("name", [](py_geodms::UnitItem self) -> std::string { return self.m_au->GetID().AsStdString(); })
		.def("full_name", [](py_geodms::UnitItem self) -> std::string { return self.m_au->GetFullName().AsStdString(); })
		.def("value_type_id", [](py_geodms::UnitItem self) -> ValueClassID { return DMS_Unit_GetValueTypeID(self.m_au.get()); }, "The value type of this unit, as a ValueTypeId enumeration value")
		.def("count", [](py_geodms::UnitItem self) -> SizeT { return DMS_Unit_GetCount(self.m_au.get()); }, "Number of elements (entity count) of this domain unit")
		.def("get_range", [](py_geodms::UnitItem self) -> std::pair<Float64, Float64> {
				Float64 b = 0, e = 0;
				DMS_NumericUnit_GetRangeAsFloat64(self.m_au.get(), &b, &e);
				return { b, e };
			})
		;

	// mutable unit
	py::class_<py_geodms::MutableUnitItem>(m, "MutableUnitItem")
		.def("is_null", [](py_geodms::MutableUnitItem self) {return self.m_au== nullptr; })
		.def("name", [](py_geodms::MutableUnitItem self) -> std::string { return self.m_au->GetID().AsStdString(); })
		.def("full_name", [](py_geodms::MutableUnitItem self) -> std::string { return self.m_au->GetFullName().AsStdString(); })
		.def("value_type_id", [](py_geodms::MutableUnitItem self) -> ValueClassID { return DMS_Unit_GetValueTypeID(self.m_au.get()); }, "The value type of this unit, as a ValueTypeId enumeration value")
		.def("asConst", &py_geodms::MutableUnitItem::asConst)
		.def("count", [](py_geodms::MutableUnitItem self) -> SizeT { return DMS_Unit_GetCount(self.m_au.get()); })
		.def("set_expr", [](py_geodms::MutableUnitItem self, const std::string& str) { self.m_au->SetExpr(SharedStr(str)); }, "Define this (domain) unit by an expression, e.g. 'range(uint32, 0, n)'")
		.def("set_count", [](py_geodms::MutableUnitItem self, SizeT count) { self.m_au->SetCount(count); }, "Set the entity count of an ordinal domain unit")
		.def("set_range", [](py_geodms::MutableUnitItem self, Float64 begin, Float64 end) { DMS_NumericUnit_SetRangeAsFloat64(self.m_au.get(), begin, end); }, "Set the numeric range [begin, end) of this unit")
		.def("get_range", [](py_geodms::MutableUnitItem self) -> std::pair<Float64, Float64> {
				Float64 b = 0, e = 0;
				DMS_NumericUnit_GetRangeAsFloat64(self.m_au.get(), &b, &e);
				return { b, e };
			})
		;

	py::implicitly_convertible<py_geodms::MutableUnitItem, py_geodms::UnitItem>();

	// const data item
	py::class_<py_geodms::DataItem>(m, "DataItem")
		.def("is_null", [](py_geodms::DataItem self) {return self.m_adi== nullptr; })
		.def("name", [](py_geodms::DataItem self) -> std::string { return self.m_adi->GetID().AsStdString(); })
		.def("full_name", [](py_geodms::DataItem self) -> std::string { return self.m_adi->GetFullName().AsStdString(); })
		.def("domain_unit", &py_geodms::DataItem::GetAbstrDomainUnit)
		.def("values_unit", &py_geodms::DataItem::GetAbstrValuesUnit)
		.def("value_composition", [](py_geodms::DataItem self) -> ValueComposition { return DMS_DataItem_GetValueComposition(self.m_adi.get()); })
		.def("size", [](py_geodms::DataItem self) -> SizeT { return DMS_Unit_GetCount(self.m_adi->GetAbstrDomainUnit()); }, "Number of values (= domain unit count)")
		.def("update", [](py_geodms::DataItem self) { DMS_TreeItem_Update(self.m_adi.get()); }, "Force (re)calculation before reading values")
		// Primary Data Access (scalar)
		.def("get_value_as_float", [](py_geodms::DataItem self, SizeT i) -> Float64 { return dataitem_get_value_as_float(self.m_adi.get(), i); })
		.def("get_value_as_int", [](py_geodms::DataItem self, SizeT i) -> Int32 { return dataitem_get_value_as_int(self.m_adi.get(), i); })
		.def("get_value_as_str", &py_geodms::DataItem::LockAndGetStringValue)
		.def("LockAndGetStringValue", &py_geodms::DataItem::LockAndGetStringValue) // backward-compatible alias
		// Primary Data Access (bulk)
		.def("get_values_as_float_list", &dataitem_get_values_as_float_list, "Read all values of this attribute as a list of floats")
		.def("get_values_as_int_list", &dataitem_get_values_as_int_list, "Read all values of this attribute as a list of ints")
		;

	// mutable data item
	py::class_<py_geodms::MutableDataItem>(m, "MutableDataItem")
		.def("is_null", [](py_geodms::MutableDataItem self) {return self.m_adi== nullptr; })
		.def("name", [](py_geodms::MutableDataItem self) -> std::string { return self.m_adi->GetID().AsStdString(); })
		.def("full_name", [](py_geodms::MutableDataItem self) -> std::string { return self.m_adi->GetFullName().AsStdString(); })
		.def("asDataItem", &py_geodms::MutableDataItem::asDataItem, "Return a read-only view of this data item")
		.def("domain_unit", [](py_geodms::MutableDataItem self) -> py_geodms::UnitItem { return py_geodms::UnitItem(self.m_adi->GetAbstrDomainUnit()); })
		.def("values_unit", [](py_geodms::MutableDataItem self) -> py_geodms::UnitItem { return py_geodms::UnitItem(self.m_adi->GetAbstrValuesUnit()); })
		.def("value_composition", [](py_geodms::MutableDataItem self) -> ValueComposition { return DMS_DataItem_GetValueComposition(self.m_adi.get()); })
		.def("size", [](py_geodms::MutableDataItem self) -> SizeT { return DMS_Unit_GetCount(self.m_adi->GetAbstrDomainUnit()); })
		.def("update", [](py_geodms::MutableDataItem self) { DMS_TreeItem_Update(self.m_adi.get()); })
		.def("set_expr", [](py_geodms::MutableDataItem self, const std::string& str) { self.m_adi->SetExpr(SharedStr(str)); }, "Set the calculation expression of this data item")
		// Primary Data Modification (bulk; fresh in-memory allocation via write_only_all)
		.def("set_values_from_float_list", &dataitem_set_values_from_float_list, "Write all values of this attribute from a list of floats")
		.def("set_values_from_int_list", &dataitem_set_values_from_int_list, "Write all values of this attribute from a list of ints")
		// Parameter (Void domain) convenience accessors
		.def("set_param_float", [](py_geodms::MutableDataItem self, Float64 v) { MG_USERCHECK2(self.m_adi.get(), "invalid dereference of a null data item"); DMS_NumericParam_SetValueAsFloat64(self.m_adi.get(), v); }, "Set the value of a numeric parameter")
		.def("set_param_int", [](py_geodms::MutableDataItem self, Int32 v) { MG_USERCHECK2(self.m_adi.get(), "invalid dereference of a null data item"); DMS_NumericParam_SetValueAsFloat64(self.m_adi.get(), Float64(v)); }, "Set the value of an integer parameter")
		.def("set_param_str", [](py_geodms::MutableDataItem self, const std::string& v) { MG_USERCHECK2(self.m_adi.get(), "invalid dereference of a null data item"); DMS_StringParam_SetValue(self.m_adi.get(), v.c_str()); }, "Set the value of a string parameter")
		.def("get_param_float", [](py_geodms::MutableDataItem self) -> Float64 { return dataitem_get_value_as_float(self.m_adi.get(), 0); }, "Read the value of a numeric parameter")
		;
}
