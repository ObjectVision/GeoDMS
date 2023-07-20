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
#pragma hdrstop

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

TIC_CALL auto Operator::EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) -> PerformanceEstimationData
{
	CreateResultCaller(resultHolder, args, nullptr);

	if (!IsDataItem(resultHolder.GetNew()))
		return {0, 0};
	auto adi = AsDataItem(resultHolder.GetNew());
	auto nrElements = adi->GetAbstrDomainUnit()->GetEstimatedCount();
	auto resultingMemory = nrElements * ElementWeight(adi);
	return { calc_time_t(nrElements), resultingMemory };
}
