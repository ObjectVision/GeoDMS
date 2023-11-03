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

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "dbg/debug.h"
#include "mci/CompositeCast.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "TreeItemClass.h"
#include "CheckedDomain.h"

#include "TreeBuilder.h"

namespace {
	static CommonOperGroup cogTB("trace_back");
	static CommonOperGroup cogSAO("service_area");
}

// *****************************************************************************
//									TraceBack
// *****************************************************************************

template <typename FlowType, typename NodeType, typename LinkType>
struct TraceBackOperator : QuaternaryOperator
{
	typedef DataArray<NodeType> Arg1Type;   // E->V: graph.F1
	typedef DataArray<NodeType> Arg2Type;   // E->V: graph.F2
	typedef DataArray<LinkType> Arg3Type;   // V->E: TraceBack
	typedef DataArray<FlowType> Arg4Type;   // V->FlowSize

	typedef DataArray<FlowType> ResultType; // E->FlowSize: resulting aggregated flow

	TraceBackOperator():
		QuaternaryOperator(&cogTB, ResultType::GetStaticClass(), 
			Arg1Type::GetStaticClass(),
			Arg2Type::GetStaticClass(),
			Arg3Type::GetStaticClass(),
			Arg4Type::GetStaticClass()
		) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 4);

		const AbstrDataItem* arg1A= AsDataItem(args[0]);
		const AbstrDataItem* arg2A= AsDataItem(args[1]);
		const AbstrDataItem* arg3A= AsDataItem(args[2]);
		const AbstrDataItem* arg4A= AsDataItem(args[3]);

		dms_assert(arg1A && arg2A && arg3A && arg4A);

		auto du = arg1A->GetAbstrDomainUnit();
		auto vu = arg1A->GetAbstrValuesUnit();

		auto flowUnit = arg4A->GetAbstrValuesUnit();

		dms_assert(flowUnit);

		du->UnifyDomain( checked_domain<LinkType>(arg2A, "a2"), "e1", "e2", UM_Throw);
		du->UnifyDomain( arg3A->GetAbstrValuesUnit(), "e1", "e3", UM_Throw);
		vu->UnifyDomain( arg2A->GetAbstrValuesUnit(), "v1", "v2", UM_Throw);
		vu->UnifyDomain( checked_domain<LinkType>(arg3A, "a3"), "v1", "e3", UM_Throw);
		vu->UnifyDomain( checked_domain<LinkType>(arg4A, "a4"), "v1", "e4", UM_Throw);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(du, flowUnit);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg3Lock(arg3A);
			DataReadLock arg4Lock(arg4A);

			const Arg1Type* arg1 = const_array_cast<NodeType>(arg1A);
			const Arg2Type* arg2 = const_array_cast<NodeType>(arg2A);
			const Arg3Type* arg3 = const_array_cast<LinkType>(arg3A);
			const Arg4Type* arg4 = const_array_cast<FlowType>(arg4A);

			dms_assert(arg1 && arg2 && arg3 && arg4);

			auto e = arg1->m_TileRangeData;
			auto v = arg1->GetValueRangeData();
			dms_assert(e && v);

			auto node1Data = arg1->GetDataRead();
			auto node2Data = arg2->GetDataRead();
			auto tbData    = arg3->GetDataRead();
			auto flowData  = arg4->GetDataRead();

			SizeT nrV = v->GetRangeSize();
			SizeT nrE = e->GetRangeSize();

			dms_assert(tbData .size()   == nrV);
			dms_assert(node1Data.size() == nrE);
			dms_assert(node2Data.size() == nrE);
			dms_assert(flowData.size()  == nrV);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			ResultType* result = mutable_array_cast<FlowType>(resLock);
			auto resultData = result->GetDataWrite();

			dms_assert(resultData   .size() == nrE);

			DBG_START("Operator", "TraceBack", true);

			fast_fill(resultData.begin(), resultData.end(), FlowType() );

			// TODO: REMOVE BY Keeping results per link, 
			// directly in resultData and assign node value + aggregate bottom up there and not per Node; TEST: compare before and after
			// thus 
			//	resultData[tbLink] += flow[currNode]; 
			//	if (resultData[tbLink])
			//		NodeType parentNode = tr.NrOfNode(currNodePtr->m_ParentNode)
			//		if (parentNode) // is there an upstream link?
			//			parentLink = tbData[parentNode];
			//			dms_assert(parentLink < nrE); 
			//			resultData[parentLink] += resultData[tbLink];

			typename sequence_traits<FlowType>::container_type
				flowDataCopy(flowData.begin(), flowData.end()); 

			TreeRelations tr(tbData.begin(), node1Data.begin(), node2Data.begin(), nrV, nrE);

			TreeNode* currNodePtr = nullptr;
			while(currNodePtr = tr.WalkDepthFirst_BottomUp_all(currNodePtr))
			{
				if (! currNodePtr->m_ParentNode ) continue; // nothing to flow

				NodeType currNode = tr.NrOfNode(currNodePtr);
				dms_assert(currNode < nrV);
				FlowType flow =  flowDataCopy[currNode];

				LinkType tbLink = tbData[currNode];
				dms_assert(tbLink < nrE); // guaranteed by value of m_ParentNode

				resultData[tbLink] = flow;

				if (flow == FlowType()) continue;
				flowDataCopy[tr.NrOfNode(currNodePtr->m_ParentNode)] += flow;
			}
			
			resLock.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//									ServiceArea
// *****************************************************************************

class ServiceAreaOperator : public TernaryOperator
{
	typedef DataArray<NodeType> Arg1Type;   // E->V: graph.F1
	typedef DataArray<NodeType> Arg2Type;   // E->V: graph.F2
	typedef DataArray<LinkType> Arg3Type;   // V->E: TraceBack

	typedef DataArray<NodeType> ResultType; // V->V: resulting sevice per node

public:
	ServiceAreaOperator(AbstrOperGroup* gr):
		TernaryOperator(gr, ResultType::GetStaticClass(), 
			Arg1Type::GetStaticClass(),
			Arg2Type::GetStaticClass(),
			Arg3Type::GetStaticClass()
		) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{

		dms_assert(args.size() == 3);

		const AbstrDataItem* arg1A = debug_valcast<const AbstrDataItem*>(args[0]);
		const AbstrDataItem* arg2A = debug_valcast<const AbstrDataItem*>(args[1]);
		const AbstrDataItem* arg3A = debug_valcast<const AbstrDataItem*>(args[2]);

		const Unit<LinkType>* e = checked_domain<LinkType>(arg1A, "a1");
		const Unit<NodeType>* v = const_unit_cast<NodeType>(arg1A->GetAbstrValuesUnit());

		dms_assert(e && v);
		e->UnifyDomain( checked_domain<LinkType>(arg2A, "a2"), "e1", "e2", UM_Throw);
		e->UnifyDomain( arg3A->GetAbstrValuesUnit(),     "e1", "v3", UM_Throw);
		v->UnifyDomain( arg2A->GetAbstrValuesUnit(),     "v1", "v2", UM_Throw);
		v->UnifyDomain( checked_domain<LinkType>(arg3A, "a3"), "v1", "e3", UM_Throw);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(v, v);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg3Lock(arg3A);

			const Arg1Type* arg1 = const_array_cast<NodeType>(arg1A);
			const Arg2Type* arg2 = const_array_cast<NodeType>(arg2A);
			const Arg3Type* arg3 = const_array_cast<LinkType>(arg3A);

			dms_assert(arg1 && arg2 && arg3);

			auto node1Data = arg1->GetDataRead();
			auto node2Data = arg2->GetDataRead();
			auto tbData    = arg3->GetDataRead();

			auto nrV = v->GetCount();
			auto nrE = e->GetCount();

			dms_assert(tbData .size()   == nrV);
			dms_assert(node1Data.size() == nrE);
			dms_assert(node2Data.size() == nrE);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero);

			ResultType* result = mutable_array_cast<NodeType>(resLock);
			auto resultData = result->GetDataWrite();

			dms_assert(resultData   .size() == nrV);

			DBG_START("Operator", "ServiceArea", true);

			fast_fill(resultData.begin(), resultData.end(), UNDEFINED_VALUE(NodeType) );

			TreeRelations tr(tbData.begin(), node1Data.begin(), node2Data.begin(), nrV, nrE);

			NodeType currRoot = UNDEFINED_VALUE(NodeType); // INIT VALUE IS NOT USED, SEE ASSERTION BELOW

			TreeNode* currNodePtr = nullptr;
			while(currNodePtr = tr.WalkDepthFirst_TopDown_all(currNodePtr))
			{
				NodeType currNode = tr.NrOfNode(currNodePtr);

				if (! currNodePtr->m_ParentNode ) 
					currRoot = currNode;

				dms_assert(IsDefined( currRoot  ));

				resultData[currNode] = currRoot;
			}
			
			resLock.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace 
{
	tl_oper::inst_tuple<
		typelists::num_objects,
		TraceBackOperator<_, UInt32, UInt32>
	>
	traceBackOperSet;

	ServiceAreaOperator saop(&cogSAO);
}
