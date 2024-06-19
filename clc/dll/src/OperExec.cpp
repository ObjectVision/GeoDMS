// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "RtcInterface.h"
#include "dbg/SeverityType.h"
#include "geo/StringArray.h"
#include "mci/CompositeCast.h"
#include "ser/AsString.h"
#include "utl/Environment.h"

#include "Param.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "TreeItemClass.h"

//#define OPER_EXECDLL

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
				reportD(SeverityTypeID::ST_Warning, "Depreciated function called, use EXEC_EC instead that return an errorcode as parameter<UInt32>");
				resultHolder = TreeItem::CreateCacheRoot();
			}
		}
	}

	template <bool returnValue>
	void Execute(TreeItemDualRef& resultHolder, CharPtr moduleName, SharedStr& cmdLine)
	{
//		DMS_ReduceResources();
		Wait(100);
		reportF(SeverityTypeID::ST_MinorTrace, "exec_ec: %s %s", moduleName ? moduleName : "", cmdLine.c_str());
		auto errorCode = ExecuteChildProcess(moduleName, cmdLine.begin());

		if constexpr (returnValue)
		{
			DataWriteLock dwl(AsDataItem(resultHolder.GetNew()));
			auto resultItem = mutable_array_cast<ERRORLEVEL>(dwl)->GetLockedDataWrite();
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
//											OperExecDll (4 params)
// *****************************************************************************

#if defined(OPER_EXECDLL)
#include "dllimp/RunDllProc.h"

UInt32 execDll_func(CharPtr dllName, CharPtr procName, SizeT nrArgs, std::vector<SharedStr>::const_iterator args)
{
	DMS_ReduceResources(); 
	Wait(100);
	switch (nrArgs) {
		case 0: return RunDllProc0(dllName, procName);
		case 1: return RunDllProc1(dllName, procName, args[0].c_str());
		case 2: return RunDllProc2(dllName, procName, args[0].c_str(), args[1].c_str());
		case 3: return RunDllProc3(dllName, procName, args[0].c_str(), args[1].c_str(), args[2].c_str());
	}
	throwErrorF("Exec", "Proc %s in dll %s called with %d arguments: Not supported", procName, dllName, nrArgs);
	return 0;
}


struct OperExecDllN : public VariadicOperator
{
	typedef DataArray<SharedStr> ArgType;

	OperExecDllN(AbstrOperGroup* gr, UInt32 nrArgs)
		:	VariadicOperator(gr, TreeItem::GetStaticClass(), nrArgs)
	{
		fast_fill(
			m_ArgClasses.begin(), 
			m_ArgClasses.begin() + nrArgs, 
			ArgType::GetStaticClass()
		);
	}
		
	// Override class Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		if (!resultHolder)
			resultHolder = TreeItem::CreateCacheRoot();

		if (mustCalc)
		{
			// Construct commandline from args
			// Execute 
			std::vector<SharedStr> argDataStr(args.size());
			for (UInt32 n = args.size(), i=0; i!=n; ++i)
			{
				checked_domain<Void>(args[i], "one of the args");
				argDataStr[i] = GetValue<SharedStr>(debug_cast<const AbstrDataItem*>(args[i]), 0);
			}
			dms_assert(args.size() >= 2);
			execDll_func(argDataStr[0].c_str(), argDataStr[1].c_str(), argDataStr.size()-2, argDataStr.begin()+2);
			resultHolder.GetNew()->SetIsInstantiated();
		}
		return true;
	}
};

#endif //defined(OPER_EXECDLL)

// *****************************************************************************
//											OperExecDll (4 params)
// *****************************************************************************

#include "odbc/OdbcStorageManager.h"
#include "Unit.h"
#include "UnitClass.h"
#include "stg/StorageClass.h"

SharedStr GetStorageManagerName(const TreeItem* ti)
{
	dms_assert(ti);
	const TreeItem* storageHolder = ti->GetStorageParent(false);
	if (!storageHolder)
		return SharedStr();

	AbstrStorageManager* storageManager = storageHolder->GetStorageManager();
	if (!storageManager)
		return SharedStr();

	if (storageManager->GetDynamicClass() == ODBCStorageManager::GetStaticClass())
	{
		return debug_cast<ODBCStorageManager*>(storageManager)->GetDatabaseFilename(storageHolder);
	}
	else
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

			auto resData = mutable_array_cast<SharedStr>(resLock)->GetDataWrite();
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
			auto resData = mutable_array_cast<SharedStr>(resLock)->GetDataWrite();

			UInt32 n = resData.size();
			dms_assert(n == argData.size());

			for (UInt32  i=0; i!=n; ++i)
				Assign(
					resData[i]
				,	AbstrStorageManager::Expand(
						args[0]
					,	SharedStr(
							begin_ptr(argData[i])
						,	end_ptr(argData[i])
						)
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

	Obsolete<ExecOperGroup> cog_EXEC("Use EXEC_EC and check the resulting UIn32 values as errorcode", "EXEC", oper_policy::depreciated);
	ExecOperGroup cog_EXEC_V("EXEC_EC", oper_policy());

	OperExec<false>      exec1(&cog_EXEC);
	OperExecInDir<false> exec2(&cog_EXEC);
	OperCmdInDir<false>  exec3(&cog_EXEC);

	OperExec<true>      exec1V(&cog_EXEC_V);
	OperExecInDir<true> exec2V(&cog_EXEC_V);
	OperCmdInDir<true>  exec3V(&cog_EXEC_V);


#if defined(OPER_EXECDLL)
	ExecOperGroup cog_EXECDLL("EXECDLL");
	ExecOperGroup cog_EXECDLL_V("EXECDLL_RV");
	OperExecDllN<false>
		execDll0(&cog_EXECDLL, 2),
		execDll1(&cog_EXECDLL, 3),
		execDll2(&cog_EXECDLL, 4),
		execDll3(&cog_EXECDLL, 5);

	OperExecDllN<true>
		execDll0V(&cog_EXECDLL_V, 2),
		execDll1V(&cog_EXECDLL_V, 3),
		execDll2V(&cog_EXECDLL_V, 4),
		execDll3V(&cog_EXECDLL_V, 5);
#endif //defined(OPER_EXECDLL)

	SpecialOperGroup sop_SN("storage_name", 1, &oap_sn[0]);
	OperGetCurrentStorage getCurrentStorage(&sop_SN);

	SpecialOperGroup cop_EXP("expand", 2, &oap_sn[0], oper_policy::calc_requires_metainfo);
	OperExpand expand(&cop_EXP);

	CommonOperGroup cop_do("do", oper_policy::calc_requires_metainfo);
	OperExpand operDo(&cop_do);
}

