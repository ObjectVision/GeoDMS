// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StxPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "DataBlockTask.h"

#include "act/UpdateMark.h"

#include "DataLockContainers.h"
#include "DataStoreManagerCaller.h"
#include "LispRef.h"

// ============================= CLASS: DataBlockTask

DataBlockTask::DataBlockTask(AbstrDataItem* adiContext, 
	CharPtr begin, CharPtr end, row_id nrElems
)	:	AbstrCalculator(adiContext, CalcRole::Calculator)
	,	m_NrElems(nrElems)
	,	m_DataBlock(LispRef(begin, end))
{
	assert(adiContext);
}

DataBlockTask::DataBlockTask(AbstrDataItem* adiContext, const DataBlockTask& src)
	:	AbstrCalculator(adiContext, CalcRole::Calculator)
	,	m_NrElems(src.GetNrElems())
	,	m_DataBlock(src.m_DataBlock)
{
	dms_assert(adiContext);
}

DataBlockTask::~DataBlockTask()
{}

#include "DataBlockParse.h"

#include "AbstrUnit.h"
#include "DataArray.h"
#include "Operator.h"
#include "TreeItemClass.h"

struct DataArrayOperator : TernaryOperator
{
	DataArrayOperator(AbstrOperGroup* og, ValueComposition vc = ValueComposition::Single)
		:	TernaryOperator(og, AbstrDataItem::GetStaticClass(), AbstrUnit::GetStaticClass(), AbstrUnit::GetStaticClass(), DataArray<SharedStr>::GetStaticClass())
		,	m_VC(vc)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrUnit* domain = AsUnit(args[0]);
		const AbstrUnit* values = AsUnit(args[1]);
		const AbstrDataItem* arg3A = AsDataItem(args[2]);
		dms_assert(arg3A);

		if (!arg3A->HasVoidDomainGuarantee())
			arg3A->throwItemError("Should have a void domain");

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(domain, values, m_VC);
			domain->GetUltimateItem(); // gather required meta info in primary thread
		}
		if (!mustCalc)
			return true;

		DataReadLock readLock(arg3A);
		auto dataBlock = const_array_cast<SharedStr>(arg3A)->GetDataRead()[0];
		AbstrDataItem* resultAttr = AsDataItem(resultHolder.GetNew());

		assert(!IsDataReady(resultAttr)); // must be here only once.

		DataBlockProd prod(resultAttr, domain->GetCount());
		try {
			parse_info_t info
				=	boost::spirit::parse(
						iterator_t(dataBlock.begin(), dataBlock.end(), position_t())
					,	iterator_t() 
					,	datablock_grammar(prod) >> boost::spirit::end_p 
					,	comment_skipper()
					);
			CheckInfo(info);

			prod.Commit();
			return true;
		}
		catch (const parser_error_t& problem)
		{
			++s_AuthErrorDisplayLockCatchCount;

			SharedStr strAtProblemLoc = problemlocAsString(dataBlock.begin(), dataBlock.end(), &*problem.where);

			ErrMsgPtr descr = std::make_shared<ErrMsg>(problem.descriptor);
			dms_assert(descr);
			descr->TellWhere(resultHolder.GetOld());
			descr->TellExtra(strAtProblemLoc.c_str());
			throw DmsException(descr);
		}
	}
private:
	ValueComposition m_VC;
};

namespace {
	const CharPtr strDataArray = "DataArray";
	const CharPtr strSeqArray = "SequenceArray";
	CommonOperGroup ogDataArray(strDataArray, oper_policy::dynamic_result_class);
	CommonOperGroup ogSeqArray(strSeqArray, oper_policy::dynamic_result_class);

	DataArrayOperator theDataBlockOper(&ogDataArray, ValueComposition::Single);
	DataArrayOperator theSeqBlockOper(&ogSeqArray, ValueComposition::Sequence);

	TokenID t_dataArray = GetTokenID_st(strDataArray);
	TokenID t_seqArray  = GetTokenID_st(strSeqArray);
}

#include "LispList.h"


MetaInfo DataBlockTask::GetMetaInfo() const
{
	auto domainUnit = GetContext()->GetAbstrDomainUnit(); domainUnit->UpdateMetaInfo();
	auto valuesUnit = GetContext()->GetAbstrValuesUnit(); valuesUnit->UpdateMetaInfo();
	ValueComposition vc = GetContext()->GetValueComposition();
	return ExprList(IsAcceptableValuesComposition(vc) ? t_seqArray : t_dataArray
	,	domainUnit->GetCheckedKeyExpr()
	,	valuesUnit->GetCheckedKeyExpr()
	,	m_DataBlock
	);
}


