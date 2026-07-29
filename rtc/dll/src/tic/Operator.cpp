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
#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Operator.h"

// *****************************************************************************
// Section:     Operator
// *****************************************************************************

// Operator Constructors

Operator::Operator(AbstrOperGroup* group, ClassCPtr resultCls)
	:	m_Group(group)
	,	m_ResultClass(resultCls)
{
	dms_assert(resultCls);

	group->Register(this);
}

Operator::Operator(AbstrOperGroup* group, ClassCPtr resultCls, const ClassCPtr* argClsList, UInt32 nrArgs)
	:	m_ArgClassesBegin(argClsList)
	,	m_ArgClassesEnd  (argClsList+nrArgs)
	,	m_Group      (group)
	,	m_ResultClass(resultCls)
	,	m_NrOptionalArgs(0)
{
	assert(resultCls);
	assert(group);
	group->Register(this);
}

Operator::~Operator()
{}

bool Operator::CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const
{
	throwIllegalAbstract(MG_POS, "Operator::CreateResult");
}

auto Operator::GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const -> oper_arg_policy
{
	return GetGroup()->GetArgPolicy(argNr, firstArgValue);
}

#include "AbstrDataItem.h"
#include "AbstrUnit.h"

// A domain's element count, with how much that number can be trusted. Robust by construction: at
// schedule time a cache result's domain is often not computed yet, and asking an uncomputed unit
// for its count throws -- which must degrade this one field, never the whole estimate.
static auto EstimateDomainCount(const AbstrUnit* domain) -> std::pair<SizeT, estimate_confidence>
{
	try {
		if (IsDataReady(domain))
			return { domain->GetCount(), estimate_confidence::derived };
		auto declared = domain->HasSizeEstimator();
		return { domain->GetEstimatedCount()
			, declared ? estimate_confidence::declared : estimate_confidence::assumed }; // else ASSUMED_SIZE
	}
	catch (...) {
		return { 0, estimate_confidence::assumed };
	}
}

TIC_CALL auto Operator::EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData
{
	CreateResultCaller(resultHolder, args);

	auto result = PerformanceEstimationData();
	if (!IsDataItem(resultHolder.GetNew()))
	{
		result.confidence = estimate_confidence::derived; // a unit or container result: zero data cost, exactly
		return result;
	}

	auto adi = AsDataItem(resultHolder.GetNew());
	auto domain = adi->GetAbstrDomainUnit();

	std::tie(result.resultingNrElements, result.confidence) = EstimateDomainCount(domain);
	result.resultingMemory = EstimateDataBytes(adi, result.resultingNrElements);
	try { result.extraTasks = domain->GetNrTiles(); } catch (...) {}

	// Work scales with the widest domain involved, not with the result's own: an aggregation
	// visits every input element to produce one. Per-family refinements sharpen this (§4.3 of
	// doc/development/schedule-with-lookahead.md); this is the honest default.
	SizeT nrElemOps = result.resultingNrElements;
	for (const auto& argRef : args)
		if (auto argItem = GetItem(argRef); argItem && IsDataItem(argItem))
		{
			auto argAdi = AsDataItem(argItem);
			auto [argCount, argConfidence] = EstimateDomainCount(argAdi->GetAbstrDomainUnit());
			result.inputSize += EstimateDataBytes(argAdi, argCount);
			MakeMax(nrElemOps, argCount);
			MakeMax(result.confidence, argConfidence); // the estimate is only as good as its worst input
		}
	result.expectedCalcTime = calc_time_t(nrElemOps) * GetGroup()->GetCalcFactor();

	return result;
}
