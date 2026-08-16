// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// *****************************************************************************
//										CheckOperator
// *****************************************************************************

#include "dbg/SeverityType.h"
#include "utl/StrFormat.h"
#include "LispTreeType.h"

#include "MoreDataControllers.h"
#include "OperGroups.h"
#include "TicPropDefConst.h"
#include "TreeItemClass.h"

CommonOperGroup sog_Check(token::integrity_check, oper_policy::existing|oper_policy::dynamic_result_class);

#if defined(MG_DEBUG)
// #1182 observability: counts distinct integrity_check DC instantiations; the trace line makes
// per-condition totals grep-able from a /L log to compare guard-dedup effectiveness.
static std::atomic<UInt32> gd_NrIntegrityCheckDCs = 0;
#endif

struct CheckOperator : public BinaryOperator
{
	using Arg2Type = DataArray<Bool>;

	CheckOperator()
		: BinaryOperator(&sog_Check,
			TreeItem::GetStaticClass(),
			TreeItem::GetStaticClass(), Arg2Type::GetStaticClass())
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);
		const TreeItem* arg1 = args[0];
		assert(arg1);
//		dms_assert(arg1->IsCacheItem());
		if (!resultHolder) {
			assert(!mustCalc);
			resultHolder = arg1;
			resultHolder->m_State.Set(actor_flag_set::AF_IntegrityChecked);
			resultHolder.m_State.Set(actor_flag_set::AF_IntegrityChecked);
#if defined(MG_DEBUG)
			if (auto funcDC = dynamic_cast<FuncDC*>(&resultHolder))
				reportF(SeverityTypeID::ST_MinorTrace, "integrity_check dc #{}: {}"
				,	++gd_NrIntegrityCheckDCs
				,	AsFLispSharedStr(funcDC->GetArgDC(1)->GetLispRef(), FormattingFlags::None));
#endif
		}
		assert(resultHolder);

		if (mustCalc)
		{
			auto curr = resultHolder.GetCurr(); // owning snapshot; weak arm (config item) can expire
			MG_CHECK(curr);
			auto arg2A = AsDataItem(args[1]);
			assert(CheckDataReady(arg2A));
			assert(CheckDataReady(curr.get()));
			DataReadLock arg2Lock(arg2A);

			IntegrityCheckFailure(curr.get(), arg2A, [&resultHolder]() -> SharedStr
				{
					auto funcDC = dynamic_cast<FuncDC*>(&resultHolder);
					MG_CHECK(funcDC);
					auto condDC = funcDC->GetArgDC(1);
					assert(condDC);

					return AsFLispSharedStr(condDC->GetLispRef(), FormattingFlags::None);
				}
			);
			if (curr->WasFailed())
				resultHolder.Fail(curr.get());
			resultHolder.SetProgress(ProgressState::Validated);
		}
		return true;
	}
};


namespace {

	CheckOperator checkOperator;

}