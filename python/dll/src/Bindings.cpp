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
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/cast.h>
#include <pybind11/stl.h>

static_assert(PY_MAJOR_VERSION == GEODMS_PYTHON_MAJOR && PY_MINOR_VERSION == GEODMS_PYTHON_MINOR,
	"Python.h does not match python/PythonVersions.txt; point the ABI-specific Python root at the requested CPython minor");



namespace py = pybind11;

namespace py_geodms
{
	namespace
	{
		int binding_module_anchor;

		std::filesystem::path GetLoadedModulePath(HMODULE module)
		{
			std::vector<wchar_t> buffer(32768);
			const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
			if (!length || length == buffer.size())
				throw py::import_error("Cannot determine a loaded DLL path while checking the GeoDMS GDAL runtime");
			return std::filesystem::path(std::wstring(buffer.data(), length));
		}

#ifdef GEODMS_GLOBIO
		bool FilesHaveEqualContent(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
		{
			std::error_code fileError;
			const auto lhsSize = std::filesystem::file_size(lhs, fileError);
			if (fileError)
				return false;
			const auto rhsSize = std::filesystem::file_size(rhs, fileError);
			if (fileError || lhsSize != rhsSize)
				return false;

			std::ifstream lhsStream(lhs, std::ios::binary);
			std::ifstream rhsStream(rhs, std::ios::binary);
			if (!lhsStream || !rhsStream)
				return false;

			std::array<char, 64 * 1024> lhsBuffer;
			std::array<char, 64 * 1024> rhsBuffer;
			do {
				lhsStream.read(lhsBuffer.data(), lhsBuffer.size());
				rhsStream.read(rhsBuffer.data(), rhsBuffer.size());
				const auto lhsCount = lhsStream.gcount();
				const auto rhsCount = rhsStream.gcount();
				if (lhsCount != rhsCount || !std::equal(lhsBuffer.begin(), lhsBuffer.begin() + lhsCount, rhsBuffer.begin()))
					return false;
			} while (lhsStream);
			return true;
		}
#endif

		void CheckBundledGdalIsActive()
		{
			HMODULE bindingModule = nullptr;
			const auto anchorAddress = reinterpret_cast<LPCWSTR>(&binding_module_anchor);
			if (!GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				anchorAddress, &bindingModule))
			{
				throw py::import_error("Cannot locate the loaded geodms extension while checking its GDAL runtime");
			}

#ifdef GEODMS_GLOBIO
			constexpr wchar_t gdalDllName[] = L"gdal301.dll";
#else
			constexpr wchar_t gdalDllName[] = L"gdal.dll";
#endif
			const HMODULE gdalModule = GetModuleHandleW(gdalDllName);
			if (!gdalModule)
				return; // DmGeo may be delay-loaded by a future build.

			const auto bindingPath = GetLoadedModulePath(bindingModule);

			// In a setup and in the build output, GeoDMS's own GDAL is the file of that name next to
			// the binding, and a copy of the same name from elsewhere that is already loaded is what
			// the engine's DLLs would bind to. In a wheel (#1355), delvewheel renamed GeoDMS's GDAL
			// (geodms.libs\gdal-<hash>.dll) and patched the imports to match, so a gdal.dll that
			// another package loaded is not the engine's and needs no check.
			std::error_code existsError;
			if (!std::filesystem::exists(bindingPath.parent_path() / gdalDllName, existsError))
				return;

			const auto gdalPath = GetLoadedModulePath(gdalModule);
			std::error_code pathError;
			const bool sameDirectory = std::filesystem::equivalent(
				bindingPath.parent_path(), gdalPath.parent_path(), pathError);
			if (!pathError && sameDirectory)
				return;

#ifdef GEODMS_GLOBIO
			// `osgeo` may have loaded the GLOBIO copy before geodms. Permit that
			// only when it is byte-for-byte the same locked gdal301.dll that the
			// G setup bundles; a matching version string alone is not an ABI proof.
			const auto bundledGdalPath = bindingPath.parent_path() / gdalDllName;
			if (FilesHaveEqualContent(gdalPath, bundledGdalPath))
				return;

			throw py::import_error(
				"The GeoDMS G flavour refused an already-loaded gdal301.dll from '" + gdalPath.string() +
				"' because it differs from the GLOBIO-locked copy in '" + bundledGdalPath.string() +
				"'. Recreate the environment from the pinned GLOBIO environment.yml or use separate processes.");
#else
			throw py::import_error(
				"GeoDMS refused to use an already-loaded GDAL from '" + gdalPath.string() +
				"'. The geodms extension and its bundled gdal.dll must be loaded from '" +
				bindingPath.parent_path().string() +
				"'. A different QGIS/OSGeo4W/GDAL build cannot safely share this Python process; "
				"use matching builds or separate processes.");
#endif
		}
	}

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
	// The lifetime of a configuration and of its items (#1370)
	//
	// Every Python item object (ConstTreeItem, MutableTreeItem, UnitItem, MutableUnitItem, DataItem,
	// MutableDataItem) holds an ItemOwner, a shared owner of the ConfigState of the configuration that
	// the item belongs to. A configuration therefore stays loaded for as long as Python holds its Config
	// or any of its items, so engine.load_config(p).const_root().find("/x").asDataItem() works without a
	// variable for the Config. The TreeItem itself is held by a std::weak_ptr, as every stored pointer to
	// a TreeItem is (ptr/SharedTreePtr.h): the root owns the tree, and the ConfigState owns the root.
	//
	// Closing a configuration ends its session and releases its tree. That happens on Config.close(), at
	// the end of its with-block, when load_config or create_config_root finds it held by loose items only,
	// when the last owner goes, and at interpreter exit (ShutDown). After it, every method of its items
	// raises instead of reading a torn-down tree. A ConfigState holds the Engine, so the engine outlives
	// every configuration and every item.
	//
	// ItemRef has no default constructor and takes its owner as its first argument, so the compiler finds
	// every place where an item wrapper is made.
	//----------------------------------------------------------------------

	constexpr CharPtr closedItemMsg =
		"this item belongs to a configuration that has been closed, by Config.close(), at the end of its with-block, "
		"or by a later load_config() or create_config_root(); load the configuration again to use its items";
	constexpr CharPtr closedConfigMsg =
		"this Config has been closed; load the configuration again with Engine.load_config()";
	constexpr CharPtr goneItemMsg =
		"this item no longer exists in its configuration";
	constexpr CharPtr configStillHeldMsg =
		"only one configuration can be loaded at a time in a process, and the Config of the loaded one still exists: "
		"call its close(), or load it with 'with engine.load_config(...) as config:', before loading another configuration";
	constexpr CharPtr shutDownMsg =
		"the Python interpreter is exiting: the geodms engine has been shut down";

	struct Engine;

	static Engine* s_currSingleEngine = nullptr;
	static tg_maintainer* s_taskGroup = nullptr; // created by the first Engine, deleted by ShutDown(); see Engine()
	static bool s_isShutDown = false;

	struct ConfigState
	{
		explicit ConfigState(std::shared_ptr<Engine> engine)
			: m_engine(std::move(engine))
		{}
		ConfigState(const ConfigState&) = delete;
		ConfigState& operator =(const ConfigState&) = delete;

		~ConfigState()
		{
			try { Close(); }
			catch (...) { catchAndReportException(); }
		}

		bool IsOpen() const { return m_root != nullptr; }

		// End the session of this configuration and release its tree; does nothing when it is closed already.
		void Close();

		std::shared_ptr<Engine>      m_engine;  // the first member, so it is released last
		SharedMutableTreeItem        m_root;    // owns the tree; null once the configuration is closed
		std::shared_ptr<SessionData> m_session; // set for a configuration made by create_config_root
		bool                         m_hasConfigObject = false; // a Python Config object refers to this configuration
	};

	static std::weak_ptr<ConfigState> s_currConfig; // the configuration that was loaded last

	void ConfigState::Close()
	{
		if (!m_root)
			return;
		auto root = std::move(m_root); // closed from here on, also when the teardown below throws
		auto session = std::move(m_session);
		auto curr = s_currConfig.lock();
		if (!curr || curr.get() == this)
			s_currConfig.reset();
		root->EnableAutoDelete(); // cancels and drains the workers, resets interest and configuration data, and lets SessionData release the root
		session.reset();
		root.reset(); // the items hold the tree weakly, so it is destroyed here
	}

	// What keeps the item of a Python item object usable: the configuration it belongs to, or for a
	// default unit of the engine, which belongs to no configuration, the engine itself.
	class ItemOwner
	{
	public:
		explicit ItemOwner(std::shared_ptr<ConfigState> config)
			: m_config(std::move(config))
		{
			MG_CHECK(m_config);
		}
		explicit ItemOwner(std::shared_ptr<Engine> engine)
			: m_engine(std::move(engine))
		{
			MG_CHECK(m_engine);
		}

		void CheckOpen() const
		{
			if (m_config && !m_config->IsOpen())
				throwDmsErrD(closedItemMsg);
		}

	private:
		std::shared_ptr<ConfigState> m_config; // the configuration of the item, which holds the engine
		std::shared_ptr<Engine>      m_engine; // set instead of m_config for a default unit
	};

	template <typename T>
	class ItemRef
	{
	public:
		ItemRef(ItemOwner owner, T* item)
			: m_owner(std::move(owner))
			, m_item(make_weak_tree(item))
			, m_isNull(item == nullptr)
		{}

		// the read-only view of the same item, with the same owner
		template <typename U> requires (std::is_convertible_v<U*, T*> && !std::is_same_v<U, T>)
		explicit ItemRef(const ItemRef<U>& rhs)
			: m_owner(rhs.m_owner)
			, m_item(rhs.m_item)
			, m_isNull(rhs.m_isNull)
		{
			m_owner.CheckOpen(); // asConst() and asDataItem() raise after a close, as every other method does
		}

		auto Owner() const -> const ItemOwner& { return m_owner; }

		bool IsNull() const
		{
			m_owner.CheckOpen();
			return m_isNull;
		}

		// The item, held for the duration of a call; empty for the null item that find() gives for a
		// name that does not exist.
		auto GetOrNull() const -> std::shared_ptr<T>
		{
			m_owner.CheckOpen();
			if (m_isNull)
				return {};
			auto item = m_item.lock();
			MG_USERCHECK2(item, goneItemMsg);
			return item;
		}

		auto Get() const -> std::shared_ptr<T>
		{
			auto item = GetOrNull();
			MG_USERCHECK2(item, NullItemMsg());
			return item;
		}

	private:
		static constexpr CharPtr NullItemMsg()
		{
			if constexpr (std::is_base_of_v<AbstrDataItem, std::remove_const_t<T>>)
				return "invalid dereference of a null data item";
			else if constexpr (std::is_base_of_v<AbstrUnit, std::remove_const_t<T>>)
				return "invalid dereference of a null unit";
			else
				return "invalid dereference of item nullptr";
		}

		template <typename U> friend class ItemRef;

		ItemOwner        m_owner;
		std::weak_ptr<T> m_item;
		bool             m_isNull;
	};

	//----------------------------------------------------------------------
	// TreeItem, unit and data item wrappers
	//----------------------------------------------------------------------
	struct ConstTreeItem : ItemRef<const TreeItem>
	{
		using ItemRef<const TreeItem>::ItemRef;
	};

	struct MutableTreeItem : ItemRef<TreeItem>
	{
		using ItemRef<TreeItem>::ItemRef;
		auto asConst() const -> ConstTreeItem { return ConstTreeItem(*this); }
	};

	struct UnitItem : ItemRef<const AbstrUnit>
	{
		using ItemRef<const AbstrUnit>::ItemRef;
	};

	struct MutableUnitItem : ItemRef<AbstrUnit>
	{
		using ItemRef<AbstrUnit>::ItemRef;
		auto asConst() const -> UnitItem { return UnitItem(*this); }
	};

	struct DataItem : ItemRef<const AbstrDataItem>
	{
		using ItemRef<const AbstrDataItem>::ItemRef;

		auto GetAbstrDomainUnit() const -> UnitItem
		{
			auto adi = Get();
			return UnitItem(Owner(), adi->GetAbstrDomainUnit());
		}

		auto GetAbstrValuesUnit() const -> UnitItem
		{
			auto adi = Get();
			return UnitItem(Owner(), adi->GetAbstrValuesUnit());
		}

		auto LockAndGetStringValue(SizeT i) const -> std::string
		{
			auto adi = Get();
			SharedTreeItemInterestPtr ip(adi.get()); // hold interest so calculated items compute
			return adi->LockAndGetValue<SharedStr>(i).c_str();
		}
	};

	struct MutableDataItem : ItemRef<AbstrDataItem>
	{
		using ItemRef<AbstrDataItem>::ItemRef;
		auto asDataItem() const -> DataItem { return DataItem(*this); }
	};

	//----------------------------------------------------------------------
	// Config: the Python object that load_config and create_config_root return. Its ConfigState can
	// outlive it, held by the items of the configuration.
	//----------------------------------------------------------------------
	struct Config
	{
		explicit Config(std::shared_ptr<ConfigState> state)
			: m_state(std::move(state))
		{
			m_state->m_hasConfigObject = true;
		}
		Config(Config&& rhs) noexcept = default;
		Config(const Config&) = delete;
		Config& operator =(const Config&) = delete;
		Config& operator =(Config&&) = delete;

		~Config()
		{
			if (m_state) // null when moved from
				m_state->m_hasConfigObject = false;
		}

		void close() { m_state->Close(); }

		void CheckOpen() const { MG_USERCHECK2(m_state->IsOpen(), closedConfigMsg); }

		auto get_root() const -> MutableTreeItem
		{
			CheckOpen();
			return MutableTreeItem(ItemOwner(m_state), m_state->m_root.get());
		}

		auto get_root_non_mutable() const -> ConstTreeItem
		{
			CheckOpen();
			return ConstTreeItem(ItemOwner(m_state), m_state->m_root.get());
		}

	private:
		std::shared_ptr<ConfigState> m_state;
	};

	static void DMS_CONV py_geodms_msg_callback(ClientHandle, const MsgData* msgData, bool)
	{
		if (msgData)
			std::cerr << "[geodms] " << msgData->m_Txt.c_str() << std::endl;
	}

	struct Engine : std::enable_shared_from_this<Engine>
	{
		Engine()
		{
			MG_USERCHECK2(!s_isShutDown, shutDownMsg);
			MG_USERCHECK2(s_currSingleEngine == nullptr, "only one geodms.Engine can exist at a time in a process");

			// Identify the main thread here, on the thread that constructs the
			// single Engine (formerly done via DMS_Appl_SetExeDir). The exe-root
			// dir is now self-determined by GetExeDir() from the Rtc module's
			// own location, so it no longer needs to be conveyed.
			SetMainThreadID();

			DMS_RegisterMsgCallback(py_geodms_msg_callback, nullptr);
			try
			{
				DMS_Clc_Load();
				DMS_Geo_Load();
				DMS_Stx_Load(); // initialize the expression parser so runtime set_expr() can be compiled

				// Initialize the global operation-context task group (the worker pool used
				// by data calculation / Primary Data Access). The exe entry points create a
				// stack-scoped tg_maintainer in main(); without one, any data access dereferences
				// a null task-group singleton. The first Engine creates it and ShutDown() ends it
				// at interpreter exit, after the open configuration is closed, as main() ends.
				// It does not end with an Engine: ~tg_maintainer leaves s_OcTaskGroupIsCanceling
				// set, which would cancel every operation of a later Engine.
				if (!s_taskGroup)
					s_taskGroup = new tg_maintainer;
			}
			catch (...)
			{
				DMS_ReleaseMsgCallback(py_geodms_msg_callback, nullptr);
				throw;
			}
			s_currSingleEngine = this;
		}
		Engine(const Engine&) = delete;
		Engine& operator =(const Engine&) = delete;

		// Runs when the Python Engine object and every configuration and item of this engine are gone,
		// so that a later Engine() can start again.
		~Engine()
		{
			DMS_ReleaseMsgCallback(py_geodms_msg_callback, nullptr);
			s_currSingleEngine = nullptr;
		}

		Config load_config(CharPtr config_file_name)
		{
			auto state = NewConfigState();
			state->m_root = make_shared_tree(CreateTreeFromConfiguration(config_file_name), existing_obj{});
			MG_USERCHECK2(state->m_root, "load_config: the file holds no configuration");
			s_currConfig = state;
			return Config(std::move(state));
		}

		Config create_config_root(CharPtr akaName)
		{
			auto state = NewConfigState();
			TokenID configName = GetTokenID_mt(akaName);
			// Mirror DMS_CreateTreeFromString: a SessionData must be created and opened on
			// the root, otherwise SessionData::Curr() is unset and expression evaluation /
			// Primary Data Access produce undefined results.
			state->m_session = SessionData::Create(GetCurrentDir().c_str(), "");
			state->m_root = TreeItem::CreateConfigRoot(configName);
			state->m_session->Open(state->m_root.get());
			s_currConfig = state;
			return Config(std::move(state));
		}

		// Resolve a basic value-type name to its shared, non-ranged default unit,
		// e.g. default_unit("float64") yields the values unit for floating-point attributes.
		UnitItem default_unit(CharPtr valueTypeName)
		{
			return UnitItem(ItemOwner(shared_from_this()), DMS_GetDefaultUnit(UnitClassFromValueTypeName(valueTypeName)));
		}

		// The Void unit; the domain of any parameter.
		UnitItem void_unit()
		{
			return UnitItem(ItemOwner(shared_from_this()), DMS_GetDefaultUnit(DMS_VoidUnit_GetStaticClass()));
		}

	private:
		// One configuration at a time: an earlier one that only loose items hold is closed, and those
		// items raise from then on; one whose Config object still exists is refused.
		auto NewConfigState() -> std::shared_ptr<ConfigState>
		{
			MG_USERCHECK2(!s_isShutDown, shutDownMsg);
			if (auto prev = s_currConfig.lock())
			{
				MG_USERCHECK2(!prev->IsOpen() || !prev->m_hasConfigObject, configStillHeldMsg);
				prev->Close();
			}
			return std::make_shared<ConfigState>(shared_from_this());
		}
	};

	// Registered with atexit, so it runs when the interpreter starts to exit, before Python releases the
	// objects that are left in an order of its own. It closes the configuration that is still open, as
	// the exe entry points close theirs before main() returns, and then ends the task group. Items and
	// Configs released after it only drop their owners.
	void ShutDown()
	{
		s_isShutDown = true;
		try
		{
			if (auto curr = s_currConfig.lock())
				curr->Close();
		}
		catch (...)
		{
			catchAndReportException();
		}
		delete std::exchange(s_taskGroup, nullptr);
	}

} // namespace py_geodms



//----------------------------------------------------------------------
// TreeItem free functions (querying + building)
//----------------------------------------------------------------------

using py_geodms::ConstTreeItem;
using py_geodms::MutableTreeItem;
using py_geodms::UnitItem;
using py_geodms::MutableUnitItem;
using py_geodms::DataItem;
using py_geodms::MutableDataItem;

// asDataItem / asUnitItem: AsDataItem and AsUnit are a static_cast in Release, so a container passed
// here was mis-cast, and the null that find() gives for a missing name crashed on the first method
// call of the wrapper. Both now raise a Python exception that says which of the two it was.
auto treeitem_AsDataItem_const(const ConstTreeItem& self) -> DataItem {
	auto item = self.Get();
	auto adi = AsDynamicDataItem(item.get());
	MG_USERCHECK2(adi, "asDataItem: this item is not a data item; isDataItem() tells");
	return DataItem(self.Owner(), adi);
}

auto treeitem_AsDataItem_mutable(const MutableTreeItem& self) -> MutableDataItem {
	auto item = self.Get();
	auto adi = AsDynamicDataItem(item.get());
	MG_USERCHECK2(adi, "asDataItem: this item is not a data item; isDataItem() tells");
	return MutableDataItem(self.Owner(), adi);
}

auto treeitem_AsUnit_const(const ConstTreeItem& self) -> UnitItem {
	auto item = self.Get();
	auto au = AsDynamicUnit(item.get());
	MG_USERCHECK2(au, "asUnitItem: this item is not a unit; isUnitItem() tells");
	return UnitItem(self.Owner(), au);
}

auto treeitem_AsUnit_mutable(const MutableTreeItem& self) -> MutableUnitItem {
	auto item = self.Get();
	auto au = AsDynamicUnit(item.get());
	MG_USERCHECK2(au, "asUnitItem: this item is not a unit; isUnitItem() tells");
	return MutableUnitItem(self.Owner(), au);
}

auto treeitem_find_const(const ConstTreeItem& self, CharPtr itemPath) -> ConstTreeItem {
	auto item = self.Get();
	auto foundItem = item->ResolveItemPath(CharPtrRange(itemPath));
	return ConstTreeItem(self.Owner(), foundItem.get());
}

auto treeitem_find_mutable(const MutableTreeItem& self, CharPtr itemPath) -> MutableTreeItem {
	auto item = self.Get();
	auto foundItem = item->ResolveItemPath(CharPtrRange(itemPath));
	return MutableTreeItem(self.Owner(), const_cast<TreeItem*>(foundItem.get())); // TODO: future improvement: use GetItem to stay non-const.
}

template <typename Wrapper>
auto treeitem_name(const Wrapper& self) -> std::string {
	return self.Get()->GetNameID().AsStdString();
}

template <typename Wrapper>
auto treeitem_fullname(const Wrapper& self) -> std::string {
	return self.Get()->GetFullName().AsStdString();
}

template <typename Wrapper>
auto treeitem_expr(const Wrapper& self) -> std::string {
	return self.Get()->GetExpr().AsStdString();
}

auto treeitem_descr_const(const ConstTreeItem& self) -> std::string {
	return self.Get()->GetDescr().AsStdString();
}

auto treeitem_GetFirstSubItem(const ConstTreeItem& self) -> ConstTreeItem {
	auto item = self.Get();
	return ConstTreeItem(self.Owner(), item->GetFirstSubItem());
}

auto treeitem_GetNextItem(const ConstTreeItem& self) -> ConstTreeItem {
	auto item = self.Get();
	return ConstTreeItem(self.Owner(), item->GetNextItem());
}

auto treeitem_subitems_const(const ConstTreeItem& self) -> std::vector<ConstTreeItem> {
	auto item = self.Get();
	std::vector<ConstTreeItem> result;
	for (auto si = item->GetFirstSubItem(); si; si = si->GetNextItem())
		result.push_back(ConstTreeItem(self.Owner(), si));
	return result;
}

auto treeitem_subitems_mutable(const MutableTreeItem& self) -> std::vector<MutableTreeItem> {
	auto item = self.Get();
	std::vector<MutableTreeItem> result;
	for (auto si = item->GetFirstSubItem(); si; si = si->GetNextItem())
		result.push_back(MutableTreeItem(self.Owner(), const_cast<TreeItem*>(si)));
	return result;
}

auto treeitem_parent_const(const ConstTreeItem& self) -> ConstTreeItem {
	auto item = self.Get();
	return ConstTreeItem(self.Owner(), DMS_TreeItem_GetParent(item.get()));
}

auto treeitem_parent_mutable(const MutableTreeItem& self) -> MutableTreeItem {
	auto item = self.Get();
	return MutableTreeItem(self.Owner(), const_cast<TreeItem*>(DMS_TreeItem_GetParent(item.get())));
}

auto treeitem_fail_reason_impl(const TreeItem* self) -> std::string {
	auto handle = DMS_TreeItem_GetFailReasonAsIString(self);
	if (!handle)
		return std::string();
	CharPtr str = DMS_IString_AsCharPtr(handle);
	std::string result = str ? str : "";
	DMS_IString_Release(handle);
	return result;
}

template <typename Wrapper>
auto treeitem_fail_reason(const Wrapper& self) -> std::string {
	auto item = self.Get();
	return treeitem_fail_reason_impl(item.get());
}

// Determine the meta info of an item without calculating it, which is what colours an item
// red in the GUI tree-view. update() would read the primary data of the item and of all its
// suppliers, which for a raster attribute is a full calculation; this resolves the
// calculation rule, the units, the storage and the implied sub-items only, so a walk over a
// whole configuration stays cheap. Returns false for a failed item, whose reason is then
// available from fail_reason().
template <typename Wrapper>
auto treeitem_update_metainfo(const Wrapper& self) -> bool {
	auto item = self.Get();
	return DMS_TreeItem_UpdateMetaInfo(item.get());
}

template <typename Wrapper>
auto treeitem_in_template(const Wrapper& self) -> bool {
	return self.Get()->InTemplate();
}

template <typename Wrapper>
auto treeitem_is_template(const Wrapper& self) -> bool {
	return self.Get()->IsTemplate();
}

template <typename Wrapper>
void treeitem_update(const Wrapper& self) {
	auto item = self.Get();
	DMS_TreeItem_Update(item.get());
}

template <typename Wrapper>
auto treeitem_is_data_item(const Wrapper& self) -> bool {
	auto item = self.GetOrNull();
	return IsDataItem(item.get());
}

template <typename Wrapper>
auto treeitem_is_unit_item(const Wrapper& self) -> bool {
	auto item = self.GetOrNull();
	return IsUnit(item.get());
}

//----------------------------------------------------------------------
// TreeItem building free functions (mutable only)
//----------------------------------------------------------------------

auto treeitem_add_container(const MutableTreeItem& self, const std::string& name) -> MutableTreeItem {
	auto parent = self.Get();
	TreeItem* ti = DMS_CreateTreeItem(parent.get(), name.c_str());
	return MutableTreeItem(self.Owner(), ti);
}

auto treeitem_create_unit(const MutableTreeItem& self, const std::string& name, const std::string& valueType) -> MutableUnitItem {
	auto parent = self.Get();
	const UnitClass* uc = py_geodms::UnitClassFromValueTypeName(valueType.c_str());
	AbstrUnit* au = DMS_CreateUnit(parent.get(), name.c_str(), uc);
	return MutableUnitItem(self.Owner(), au);
}

auto treeitem_add_data_item(const MutableTreeItem& self, const std::string& name,
	const UnitItem& domain, const UnitItem& values, ValueComposition vc) -> MutableDataItem {
	auto parent = self.Get();
	auto domainUnit = domain.Get();
	auto valuesUnit = values.Get();
	AbstrDataItem* adi = DMS_CreateDataItem(parent.get(), name.c_str(), domainUnit.get(), valuesUnit.get(), vc);
	return MutableDataItem(self.Owner(), adi);
}

auto treeitem_add_attribute(const MutableTreeItem& self, const std::string& name,
	const UnitItem& domain, const std::string& valuesValueType) -> MutableDataItem {
	auto parent = self.Get();
	auto domainUnit = domain.Get();
	const AbstrUnit* values = DMS_GetDefaultUnit(py_geodms::UnitClassFromValueTypeName(valuesValueType.c_str()));
	AbstrDataItem* adi = DMS_CreateDataItem(parent.get(), name.c_str(), domainUnit.get(), values, ValueComposition::Single);
	return MutableDataItem(self.Owner(), adi);
}

auto treeitem_add_param(const MutableTreeItem& self, const std::string& name, const std::string& valuesValueType) -> MutableDataItem {
	auto parent = self.Get();
	const AbstrUnit* voidUnit = DMS_GetDefaultUnit(DMS_VoidUnit_GetStaticClass());
	const AbstrUnit* values = DMS_GetDefaultUnit(py_geodms::UnitClassFromValueTypeName(valuesValueType.c_str()));
	AbstrDataItem* adi = DMS_CreateDataItem(parent.get(), name.c_str(), voidUnit, values, ValueComposition::Single);
	return MutableDataItem(self.Owner(), adi);
}

void treeitem_set_storage_manager(const MutableTreeItem& self, const std::string& storageName, const std::string& storageType, bool readOnly) {
	auto item = self.Get();
	item->SetStorageManager(storageName.c_str(), storageType.c_str(),
		readOnly ? StorageReadOnlySetting::ReadOnly : StorageReadOnlySetting::ReadWrite);
}

//----------------------------------------------------------------------
// DataItem primary data access free functions
//----------------------------------------------------------------------

// Bulk-read all values of an attribute. We address tiles through the data object's own
// GetTiledLocation, as the DMS_NumericAttr_Get*Array C functions do since #1311; the domain
// unit's GetTiledRangeData() is null for a domain whose range is defined by an expression
// (the range then lives in the unit it refers to).
auto dataitem_get_values_as_float_list(const DataItem& self) -> std::vector<Float64> {
	auto adiHolder = self.Get();
	const AbstrDataItem* adi = adiHolder.get();
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

auto dataitem_get_values_as_int_list(const DataItem& self) -> std::vector<Int32> {
	auto adiHolder = self.Get();
	const AbstrDataItem* adi = adiHolder.get();
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
// data object can be sized. The list must hold exactly one value per domain element, and it
// is written tile by tile, as the getters above read: the bulk setters write one tile and
// check their length only in a Debug build, so a longer list, or one spanning tiles, used to
// be written past the end of the first tile, and a shorter one left the rest undefined.
template <typename T>
void dataitem_set_values_from_list(AbstrDataItem* adi, const std::vector<T>& data, CharPtr funcName,
	void (AbstrDataObject::* setValues)(tile_loc, SizeT, const T*))
{
	MG_USERCHECK2(adi, "invalid dereference of a null data item");
	adi->SetTSF(TSF_HasConfigData); // mark as authoritative primary data so dependents don't recompute it
	SizeT n = DMS_Unit_GetCount(adi->GetAbstrDomainUnit());
	if (data.size() != n)
		throwErrorF(funcName, "the list has {} values, but the domain of {} has {} elements", data.size(), adi->GetFullName().c_str(), n);
	DataWriteLock lock(adi, dms_rw_mode::write_only_all);
	if (n)
	{
		auto trd = lock->GetTiledRangeData();
		SizeT index = 0;
		while (index != n)
		{
			tile_loc tl = lock->GetTiledLocation(index);
			if (!IsDefined(tl.first))
				throwErrorF(funcName, "element {} of the domain of {} lies in no tile", index, adi->GetFullName().c_str());
			SizeT len = trd->GetTileSize(tl.first) - tl.second;
			MakeMin(len, n - index);
			(lock.get()->*setValues)(tl, len, data.data() + index);
			index += len;
		}
	}
	lock.Commit();
}

void dataitem_set_values_from_float_list(const MutableDataItem& self, const std::vector<Float64>& data) {
	auto adi = self.Get();
	dataitem_set_values_from_list(adi.get(), data, "set_values_from_float_list", &AbstrDataObject::SetValuesAsFloat64Array);
}

void dataitem_set_values_from_int_list(const MutableDataItem& self, const std::vector<Int32>& data) {
	auto adi = self.Get();
	dataitem_set_values_from_list(adi.get(), data, "set_values_from_int_list", &AbstrDataObject::SetValuesAsInt32Array);
}

template <typename Wrapper>
auto dataitem_domain_unit(const Wrapper& self) -> UnitItem {
	auto adi = self.Get();
	return UnitItem(self.Owner(), adi->GetAbstrDomainUnit());
}

template <typename Wrapper>
auto dataitem_values_unit(const Wrapper& self) -> UnitItem {
	auto adi = self.Get();
	return UnitItem(self.Owner(), adi->GetAbstrValuesUnit());
}

template <typename Wrapper>
auto dataitem_value_composition(const Wrapper& self) -> ValueComposition {
	auto adi = self.Get();
	return DMS_DataItem_GetValueComposition(adi.get());
}

template <typename Wrapper>
auto dataitem_size(const Wrapper& self) -> SizeT {
	auto adi = self.Get();
	return DMS_Unit_GetCount(adi->GetAbstrDomainUnit());
}

template <typename Wrapper>
auto unit_get_range(const Wrapper& self) -> std::pair<Float64, Float64> {
	auto au = self.Get();
	Float64 b = 0, e = 0;
	DMS_NumericUnit_GetRangeAsFloat64(au.get(), &b, &e);
	return { b, e };
}

template <typename Wrapper>
auto unit_value_type_id(const Wrapper& self) -> ValueClassID {
	auto au = self.Get();
	return DMS_Unit_GetValueTypeID(au.get());
}

template <typename Wrapper>
auto unit_count(const Wrapper& self) -> SizeT {
	auto au = self.Get();
	return DMS_Unit_GetCount(au.get());
}

PYBIND11_MODULE(geodms, m) {
	py_geodms::CheckBundledGdalIsActive();

	m.doc() = "Python bindings for the GeoDMS Data & Model Server: read/query a configuration, "
	          "set parameter values, build an in-memory configuration without a model script, "
	          "and query results via Primary Data Access.";

	// Close the open configuration and end the task group before the interpreter releases what is left (#1370).
	py::module_::import("atexit").attr("register")(py::cpp_function(&py_geodms::ShutDown));

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

	// engine: held by a shared_ptr, because every Config and item holds it as well (#1370)
	py::class_<py_geodms::Engine, std::shared_ptr<py_geodms::Engine>>(m, "Engine")
		.def(py::init())
		.def("load_config", &py_geodms::Engine::load_config,
			"Load a configuration from a .dms file and return its Config. An earlier configuration that only "
			"loose items still hold is closed first; one whose Config still exists must be closed with close() first")
		.def("create_config_root", &py_geodms::Engine::create_config_root,
			"Create an empty in-memory configuration with the given root name; one configuration at a time, as load_config")
		.def("default_unit", &py_geodms::Engine::default_unit, "Return the shared default unit for a basic value type name (e.g. 'float64')")
		.def("void_unit", &py_geodms::Engine::void_unit, "Return the Void unit, used as the domain of parameters")
		;

	// config
	py::class_<py_geodms::Config>(m, "Config")
		.def("root", &py_geodms::Config::get_root, "Return the mutable root container of the configuration")
		.def("const_root", &py_geodms::Config::get_root_non_mutable, "Return the read-only root container of the configuration")
		.def("close", &py_geodms::Config::close,
			"Close the configuration: end its session and release its items, whose methods raise from then on. "
			"Closing a closed configuration does nothing")
		.def("__enter__", [](py::object self) -> py::object { self.cast<py_geodms::Config&>().CheckOpen(); return self; })
		.def("__exit__", [](py_geodms::Config& self, py::args) -> bool { self.close(); return false; },
			"Close the configuration at the end of a with-block")
		;

	// non-mutable treeitem
	py::class_<ConstTreeItem>(m, "ConstTreeItem")
		.def("is_null", &ConstTreeItem::IsNull)
		.def("find", &treeitem_find_const, "Find a sub-item by relative or absolute path")
		.def("name", &treeitem_name<ConstTreeItem>)
		.def("full_name", &treeitem_fullname<ConstTreeItem>)
		.def("expr", &treeitem_expr<ConstTreeItem>)
		.def("descr", &treeitem_descr_const)
		.def("first_subitem", &treeitem_GetFirstSubItem)
		.def("next", &treeitem_GetNextItem)
		.def("sub_items", &treeitem_subitems_const, "Return a list of all direct sub-items")
		.def("parent", &treeitem_parent_const)
		.def("fail_reason", &treeitem_fail_reason<ConstTreeItem>, "Failure reason string, or empty when the item is valid")
		.def("update_metainfo", &treeitem_update_metainfo<ConstTreeItem>,
			"Determine the meta info of this item without calculating it, as the GUI does before it colours an item red; "
			"returns False when the item failed, whose reason is then given by fail_reason()")
		.def("in_template", &treeitem_in_template<ConstTreeItem>,
			"True for a template and for every item inside one; such a body is inert and is not meant to be updated")
		.def("is_template", &treeitem_is_template<ConstTreeItem>,
			"True for the template item itself")
		.def("update", &treeitem_update<ConstTreeItem>, "Force (re)calculation of this item and its suppliers")
		.def("isDataItem", &treeitem_is_data_item<ConstTreeItem>)
		.def("asDataItem", &treeitem_AsDataItem_const)
		.def("isUnitItem", &treeitem_is_unit_item<ConstTreeItem>)
		.def("asUnitItem", &treeitem_AsUnit_const)
		;

	// mutable treeitem
	py::class_<MutableTreeItem>(m, "MutableTreeItem")
		.def("is_null", &MutableTreeItem::IsNull)
		.def("find", &treeitem_find_mutable, "Find a sub-item by relative or absolute path")
		.def("name", &treeitem_name<MutableTreeItem>)
		.def("full_name", &treeitem_fullname<MutableTreeItem>)
		.def("expr", &treeitem_expr<MutableTreeItem>)
		.def("asConst", &MutableTreeItem::asConst)
		.def("sub_items", &treeitem_subitems_mutable, "Return a list of all direct sub-items")
		.def("parent", &treeitem_parent_mutable)
		.def("fail_reason", &treeitem_fail_reason<MutableTreeItem>, "Failure reason string, or empty when the item is valid")
		.def("update_metainfo", &treeitem_update_metainfo<MutableTreeItem>,
			"Determine the meta info of this item without calculating it; returns False when the item failed")
		.def("in_template", &treeitem_in_template<MutableTreeItem>,
			"True for a template and for every item inside one")
		.def("is_template", &treeitem_is_template<MutableTreeItem>,
			"True for the template item itself")
		.def("update", &treeitem_update<MutableTreeItem>, "Force (re)calculation of this item and its suppliers")
		.def("set_expr", [](const MutableTreeItem& self, const std::string& str) { self.Get()->SetExpr(SharedStr(str)); }, "Set the calculation expression of this item")
		.def("set_descr", [](const MutableTreeItem& self, const std::string& str) { self.Get()->SetDescr(SharedStr(str)); }, "Set the description property of this item")
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
		.def("disable_storage", [](const MutableTreeItem& self) { self.Get()->DisableStorage(); }, "Force in-memory / calculator-only operation")
		.def("isDataItem", &treeitem_is_data_item<MutableTreeItem>)
		.def("asDataItem", &treeitem_AsDataItem_mutable)
		.def("isUnitItem", &treeitem_is_unit_item<MutableTreeItem>)
		.def("asUnitItem", &treeitem_AsUnit_mutable)
		;

	// const unit
	py::class_<UnitItem>(m, "UnitItem")
		.def("is_null", &UnitItem::IsNull)
		.def("name", &treeitem_name<UnitItem>)
		.def("full_name", &treeitem_fullname<UnitItem>)
		.def("value_type_id", &unit_value_type_id<UnitItem>, "The value type of this unit, as a ValueTypeId enumeration value")
		.def("count", &unit_count<UnitItem>, "Number of elements (entity count) of this domain unit")
		.def("get_range", &unit_get_range<UnitItem>)
		;

	// mutable unit
	py::class_<MutableUnitItem>(m, "MutableUnitItem")
		.def("is_null", &MutableUnitItem::IsNull)
		.def("name", &treeitem_name<MutableUnitItem>)
		.def("full_name", &treeitem_fullname<MutableUnitItem>)
		.def("value_type_id", &unit_value_type_id<MutableUnitItem>, "The value type of this unit, as a ValueTypeId enumeration value")
		.def("asConst", &MutableUnitItem::asConst)
		.def("count", &unit_count<MutableUnitItem>)
		.def("set_expr", [](const MutableUnitItem& self, const std::string& str) { self.Get()->SetExpr(SharedStr(str)); }, "Define this (domain) unit by an expression, e.g. 'range(uint32, 0, n)'")
		.def("set_count", [](const MutableUnitItem& self, SizeT count) { self.Get()->SetCount(count); }, "Set the entity count of an ordinal domain unit")
		.def("set_range", [](const MutableUnitItem& self, Float64 begin, Float64 end) { auto au = self.Get(); DMS_NumericUnit_SetRangeAsFloat64(au.get(), begin, end); }, "Set the numeric range [begin, end) of this unit")
		.def("get_range", &unit_get_range<MutableUnitItem>)
		;

	py::implicitly_convertible<MutableUnitItem, UnitItem>();

	// const data item
	py::class_<DataItem>(m, "DataItem")
		.def("is_null", &DataItem::IsNull)
		.def("name", &treeitem_name<DataItem>)
		.def("full_name", &treeitem_fullname<DataItem>)
		.def("domain_unit", &DataItem::GetAbstrDomainUnit)
		.def("values_unit", &DataItem::GetAbstrValuesUnit)
		.def("value_composition", &dataitem_value_composition<DataItem>)
		.def("size", &dataitem_size<DataItem>, "Number of values (= domain unit count)")
		.def("update", &treeitem_update<DataItem>, "Force (re)calculation before reading values")
		// Primary Data Access (scalar)
		.def("get_value_as_float", [](const DataItem& self, SizeT i) -> Float64 { auto adi = self.Get(); return dataitem_get_value_as_float(adi.get(), i); })
		.def("get_value_as_int", [](const DataItem& self, SizeT i) -> Int32 { auto adi = self.Get(); return dataitem_get_value_as_int(adi.get(), i); })
		.def("get_value_as_str", &DataItem::LockAndGetStringValue)
		.def("LockAndGetStringValue", &DataItem::LockAndGetStringValue) // backward-compatible alias
		// Primary Data Access (bulk)
		.def("get_values_as_float_list", &dataitem_get_values_as_float_list, "Read all values of this attribute as a list of floats")
		.def("get_values_as_int_list", &dataitem_get_values_as_int_list, "Read all values of this attribute as a list of ints")
		;

	// mutable data item
	py::class_<MutableDataItem>(m, "MutableDataItem")
		.def("is_null", &MutableDataItem::IsNull)
		.def("name", &treeitem_name<MutableDataItem>)
		.def("full_name", &treeitem_fullname<MutableDataItem>)
		.def("asDataItem", &MutableDataItem::asDataItem, "Return a read-only view of this data item")
		.def("domain_unit", &dataitem_domain_unit<MutableDataItem>)
		.def("values_unit", &dataitem_values_unit<MutableDataItem>)
		.def("value_composition", &dataitem_value_composition<MutableDataItem>)
		.def("size", &dataitem_size<MutableDataItem>)
		.def("update", &treeitem_update<MutableDataItem>)
		.def("set_expr", [](const MutableDataItem& self, const std::string& str) { self.Get()->SetExpr(SharedStr(str)); }, "Set the calculation expression of this data item")
		// Primary Data Modification (bulk; fresh in-memory allocation via write_only_all)
		.def("set_values_from_float_list", &dataitem_set_values_from_float_list, "Write all values of this attribute from a list of floats")
		.def("set_values_from_int_list", &dataitem_set_values_from_int_list, "Write all values of this attribute from a list of ints")
		// Parameter (Void domain) convenience accessors
		.def("set_param_float", [](const MutableDataItem& self, Float64 v) { auto adi = self.Get(); DMS_NumericParam_SetValueAsFloat64(adi.get(), v); }, "Set the value of a numeric parameter")
		.def("set_param_int", [](const MutableDataItem& self, Int32 v) { auto adi = self.Get(); DMS_NumericParam_SetValueAsFloat64(adi.get(), Float64(v)); }, "Set the value of an integer parameter")
		.def("set_param_str", [](const MutableDataItem& self, const std::string& v) { auto adi = self.Get(); DMS_StringParam_SetValue(adi.get(), v.c_str()); }, "Set the value of a string parameter")
		.def("get_param_float", [](const MutableDataItem& self) -> Float64 { auto adi = self.Get(); return dataitem_get_value_as_float(adi.get(), 0); }, "Read the value of a numeric parameter")
		;
}
