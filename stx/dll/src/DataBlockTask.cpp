//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#include "StxPch.h"
#pragma hdrstop

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
	dms_assert(adiContext);
	adiContext->DisableStorage();
}

DataBlockTask::DataBlockTask(AbstrDataItem* adiContext, const DataBlockTask& src)
	:	AbstrCalculator(adiContext, CalcRole::Calculator)
	,	m_NrElems(src.GetNrElems())
	,	m_DataBlock(src.m_DataBlock)
{
	dms_assert(adiContext);
	adiContext->DisableStorage();
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
		AbstrDataItem* adi = AsDataItem(resultHolder.GetNew());

		DataBlockProd prod(adi, domain->GetCount());
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

			auto err = problem.descriptor;
			dms_assert(err);
			err->TellExtra(strAtProblemLoc.c_str());
			resultHolder->throwItemError(err->Why());
		}
	}
private:
	ValueComposition m_VC;
};

namespace {
	const CharPtr strDataArray = "DataArray";
	const CharPtr strSeqArray = "SequenceArray";
	CommonOperGroup ogDataArray(strDataArray, oper_policy::dynamic_result_class | oper_policy::calc_requires_metainfo);
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


