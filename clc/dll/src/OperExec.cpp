// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "RtcInterface.h"
#include "dbg/SeverityType.h"
#include "vt/StringArray.h"
#include "mci/CompositeCast.h"
#include "ser/AsString.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"

#include "Param.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "TreeItemClass.h"
#include "DataArrayValue.h"

namespace {
	using ERRORLEVEL = UInt32;

	template <bool returnValue>
	constexpr const Class* ReturnClass() {
		if (returnValue)
			return DataArray<ERRORLEVEL>::GetStaticClass();
		return TreeItem::GetStaticClass();
	}

	template <bool returnValue>
	void CreateResultHolder(TreeItemDualRef& resultHolder)
	{
		if (!resultHolder)
		{
			if constexpr (returnValue)
				resultHolder = CreateCacheDataItem(Unit<Void>::GetStaticClass()->CreateDefault(), Unit<ERRORLEVEL>::GetStaticClass()->CreateDefault());
			else
			{
				reportD(SeverityTypeID::ST_Warning, "Deprecated function called, use EXEC_EC instead that return an errorcode as parameter<UInt32>");
				resultHolder = TreeItem::CreateCacheRoot();
			}
		}
	}

	template <bool returnValue>
	void Execute(TreeItemDualRef& resultHolder, CharPtr moduleName, SharedStr& cmdLine)
	{
//		DMS_ReduceResources();
		Wait(100);
		reportF(SeverityTypeID::ST_MinorTrace, "exec_ec: {} {}", moduleName ? moduleName : "", cmdLine.c_str());
		auto errorCode = ExecuteChildProcess(moduleName, cmdLine.begin());

		if constexpr (returnValue)
		{
			DataWriteLock dwl(AsDataItem(resultHolder.GetNew()));
			auto resultItem = mutable_array_cast<ERRORLEVEL>(dwl)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
			resultItem[0] = errorCode;
			dwl.Commit();
		}
		else
			resultHolder.GetNew()->SetIsInstantiated();
	}
}
// *****************************************************************************
//											OperExec (1 param with command line)
// *****************************************************************************


template<bool returnValue>
struct OperExec : UnaryOperator
{
	typedef DataArray<SharedStr> ArgType;

	OperExec(AbstrOperGroup* gr) 
		:	UnaryOperator(gr, ReturnClass<returnValue>(), ArgType::GetStaticClass())
	{}

	// Override class Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		CreateResultHolder<returnValue>(resultHolder);
		if (mustCalc)
		{
			// Construct commandline from args
			checked_domain<Void>(args[0], "a1");
			SharedStr cmd = GetValue<SharedStr>(debug_cast<const AbstrDataItem*>(args[0]), 0);

			// Execute 
			Execute<returnValue>(resultHolder, nullptr, cmd);
		}
		return true;
	}
};

// *****************************************************************************
//											OperExecInDir (1 param with command line, 1 parameter in temp curr dir)
// *****************************************************************************

struct CurrentDirSelector
{
	CurrentDirSelector(CharPtr dir)
		:	m_PrevDir(GetCurrentDir())
	{
		SetCurrentDir(dir);
	}
	~CurrentDirSelector()
	{
		SetCurrentDir(m_PrevDir.c_str());
	}
private:
	SharedStr m_PrevDir;
};

template<bool returnValue>
struct OperExecInDir : BinaryOperator
{
	typedef DataArray<SharedStr> ArgType;

	OperExecInDir(AbstrOperGroup* gr) 
		:	BinaryOperator(gr, ReturnClass<returnValue>(), ArgType::GetStaticClass(), ArgType::GetStaticClass())
	{}

	// Override class Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		CreateResultHolder<returnValue>(resultHolder);
		if (mustCalc)
		{
			// Construct commandline from args
			checked_domain<Void>(args[0], "a1");
			checked_domain<Void>(args[1], "a2");
			SharedStr cmd = GetTheCurrValue<SharedStr>(debug_cast<const AbstrDataItem*>(args[0]));
			SharedStr dir = GetTheCurrValue<SharedStr>(debug_cast<const AbstrDataItem*>(args[1]));

			// Execute 
			CurrentDirSelector cds(dir.c_str());
			Execute<returnValue>(resultHolder, nullptr, cmd);
		}
		return true;
	}
};

template <bool returnValue>
struct OperCmdInDir : TernaryOperator
{
	typedef DataArray<SharedStr> ArgType;

	OperCmdInDir(AbstrOperGroup* gr) 
		:	TernaryOperator(gr, ReturnClass<returnValue>(), ArgType::GetStaticClass(), ArgType::GetStaticClass(), ArgType::GetStaticClass())
	{}

	// Override class Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		CreateResultHolder<returnValue>(resultHolder);
		if (mustCalc)
		{
			// Construct commandline from args
			checked_domain<Void>(args[0], "a1");
			checked_domain<Void>(args[1], "a2");
			checked_domain<Void>(args[2], "a3");
			SharedStr module  = GetTheCurrValue<SharedStr>(args[0]);
			SharedStr cmdLine = GetTheCurrValue<SharedStr>(args[1]);
			SharedStr dirPath = GetTheCurrValue<SharedStr>(args[2]);

			// Execute 
			CurrentDirSelector cds(dirPath.c_str());
			Execute<returnValue>(resultHolder, module.c_str(), cmdLine);
		}
		return true;
	}
};


// *****************************************************************************
//											OperGetCurrentStorage, OperExpand
// *****************************************************************************

#include "odbc/OdbcStorageManager.h"
#include "stg/AbstrStorageManager.h"
#include "Unit.h"
#include "UnitClass.h"
#include "stg/StorageClass.h"

SharedStr GetStorageManagerName(const TreeItem* ti)
{
	dms_assert(ti);
	auto storageHolder = ti->GetStorageParent(false);
	if (!storageHolder)
		return SharedStr();

	AbstrStorageManager* storageManager = storageHolder->GetStorageManager();
	if (!storageManager)
		return SharedStr();

if (storageManager->GetDynamicClass() == ODBCStorageManager::GetStaticClass())
	return debug_cast<ODBCStorageManager*>(storageManager)->GetDatabaseFilename(storageHolder.get());

return storageManager->GetNameStr();
}

class OperGetCurrentStorage : public UnaryOperator
{
	typedef DataArray<SharedStr> ResultType;
			
public:

	OperGetCurrentStorage(AbstrOperGroup* gr) 
		: UnaryOperator(gr, ResultType::GetStaticClass(),  TreeItem::GetStaticClass()) {}

	// Override class Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		// Check number of parameters
		dms_assert(args.size() == 1);

		if (!resultHolder)
			resultHolder = CreateCacheParam<SharedStr>();

		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			auto resData = mutable_array_cast<SharedStr>(resLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
			dms_assert(resData.size() == 1);
			Assign(resData[0], GetStorageManagerName(args[0]) );

			resLock.Commit();
		}
		return true;
	}
};


class OperExpand: public BinaryOperator
{
	typedef DataArray<SharedStr> ResultType;
			
public:

	OperExpand(AbstrOperGroup* gr) 
		: BinaryOperator(gr, ResultType::GetStaticClass(), TreeItem::GetStaticClass(),  ResultType::GetStaticClass()) {}

	// Override class Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		// Check number of parameters
		dms_assert(args.size() == 2);

		if (!resultHolder)
			resultHolder = CreateCacheParam<SharedStr>();

		if (mustCalc)
		{
			DataReadLock argLock(AsDataItem(args[1]));

			auto argData = const_array_cast<SharedStr>(args[1])->GetDataRead();

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);
			auto resData = mutable_array_cast<SharedStr>(resLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero);

			auto n = resData.size();
			assert(n == argData.size());

			for (decltype(n) i=0; i!=n; ++i)
				Assign(
					resData[i]
				,	AbstrStorageManager::Expand(
						args[0]
					,	SharedStr(CharPtrRange(
							begin_ptr(argData[i])
						,	end_ptr(argData[i])
						))
					)
				);

			resLock.Commit();
		}
		return true;
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

struct ExecOperGroup: CommonOperGroup
{
	ExecOperGroup(CharPtr operName, oper_policy op) : CommonOperGroup(operName, op)
	{
		m_Policy = m_Policy | oper_policy::has_external_effects | oper_policy::calc_requires_metainfo;
	}
};

namespace 
{
	oper_arg_policy oap_sn[2] = { oper_arg_policy::calc_never, oper_arg_policy::calc_as_result };

	ExecOperGroup cog_EXEC_V("EXEC_EC", oper_policy());

	OperExec<true>      exec1V(&cog_EXEC_V);
	OperExecInDir<true> exec2V(&cog_EXEC_V);
	OperCmdInDir<true>  exec3V(&cog_EXEC_V);

	SpecialOperGroup sop_SN("storage_name", 1, &oap_sn[0]);
	OperGetCurrentStorage getCurrentStorage(&sop_SN);

	SpecialOperGroup cop_EXP("expand", 2, &oap_sn[0], oper_policy::calc_requires_metainfo);
	OperExpand expand(&cop_EXP);

	CommonOperGroup cop_do("do", oper_policy::calc_requires_metainfo);
	OperExpand operDo(&cop_do);
}

