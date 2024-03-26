// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "CheckedDomain.h"
#include "Unit.h"
#include "UnitClass.h"

template <typename NodeType, typename LinkType>
void InvertIntoLinkedList(
	const NodeType* nodeIdArray,
	typename std::vector<LinkType>::iterator backPtr,
	typename std::vector<LinkType>::iterator nextPtr,
	LinkType edgeCount, NodeType nodeCount)
{
	for (LinkType edgeNr = 0; edgeNr != edgeCount; ++edgeNr)
	{
		NodeType nodeId = *nodeIdArray++;
		if (nodeId < nodeCount)
		{
			nextPtr[edgeNr] = backPtr[nodeId];
			backPtr[nodeId] = edgeNr;
		}
	}
}

// *****************************************************************************
//									ConnectedParts
// *****************************************************************************

namespace {
	CommonOperGroup cogCP("connected_parts", oper_policy::better_not_in_meta_scripting);
	CommonOperGroup cogSCC("strongly_connected_components", oper_policy::better_not_in_meta_scripting);
}

static TokenID s_PartNr = GetTokenID_st("PartNr");

class ConnectedPartsOperator : public BinaryOperator
{
	using NodeType = UInt32;
	using LinkType = UInt32;
	using PartType = UInt32;

	using Arg1Type = DataArray<NodeType>; // E->V
	using Arg2Type = DataArray<NodeType>; // E->V

	using ResultUnitType = Unit<PartType>;
	using ResultSubType = DataArray<PartType>; // V->P

	using NodeStackType = std::vector<NodeType>;

public:
	ConnectedPartsOperator():
		BinaryOperator(&cogCP, ResultUnitType::GetStaticClass(), 
			Arg1Type::GetStaticClass(),
			Arg2Type::GetStaticClass()
		) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);

		const AbstrDataItem* arg1A= AsDataItem(args[0]);
		const AbstrDataItem* arg2A= AsDataItem(args[1]);
		arg1A->GetAbstrDomainUnit()->UnifyDomain(arg2A->GetAbstrDomainUnit());
		arg1A->GetAbstrValuesUnit()->UnifyValues(arg2A->GetAbstrValuesUnit());

		MG_CHECK(checked_domain<LinkType>(arg1A, "a1") == checked_domain<LinkType>(arg2A, "a2"));
		MG_CHECK(arg1A->GetAbstrValuesUnit() == arg2A->GetAbstrValuesUnit());

		AbstrUnit* res = ResultUnitType::GetStaticClass()->CreateResultUnit(resultHolder);
		assert(res);
		resultHolder = res;

		AbstrDataItem* resSub = CreateDataItem(res, s_PartNr, arg1A->GetAbstrValuesUnit(), res);
		MG_CHECK(resSub);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
	
			const Arg1Type* arg1 = const_array_cast<NodeType>(arg1Lock);
			const Arg2Type* arg2 = const_array_cast<NodeType>(arg2Lock);

			assert(arg1 && arg2);

			auto e = arg1->GetTiledRangeData();
			auto v = arg1->GetValueRangeData();
			assert(e && v);

			auto node1Data = arg1->GetDataRead();
			auto node2Data = arg2->GetDataRead();

			NodeType nrV = v->GetElemCount();
			LinkType nrE = e->GetElemCount();

			assert(node1Data.size() == nrE);
			assert(node2Data.size() == nrE);

			// TODO G8: doe dit zoals in OperDistrict.cpp

			std::vector<PartType> resultData(nrV, UNDEFINED_VALUE(PartType));
			assert(resultData   .size() == nrV); 

			std::vector<LinkType>
				link1    (nrV, UNDEFINED_VALUE(UInt32)),
				link2    (nrV, UNDEFINED_VALUE(UInt32)),
				nextLink1(nrE, UNDEFINED_VALUE(UInt32)), 
				nextLink2(nrE, UNDEFINED_VALUE(UInt32));

			InvertIntoLinkedList<NodeType, LinkType>(node1Data.begin(), link1.begin(), nextLink1.begin(), nrE, nrV);
			InvertIntoLinkedList<NodeType, LinkType>(node2Data.begin(), link2.begin(), nextLink2.begin(), nrE, nrV);

			NodeStackType nodeStack;
			
			UInt32 nrParts = 0;

			for (auto rb = resultData.begin(), ri = resultData.begin(), re = resultData.end(); ri != re; ++ri)
			{
				if (!IsDefined(*ri))
				{
					nodeStack.push_back(ri - rb);
					do 
					{
						NodeType currNode =  nodeStack.back(); assert(currNode < nrV);
						nodeStack.pop_back();

						PartType currPart = rb[ currNode ];
						assert(currPart == nrParts || !IsDefined(currPart));
						if ( ! IsDefined( currPart ) )
						{
							rb[ currNode ] = nrParts;

							LinkType currLink = link1[currNode];
							while (currLink < nrE)
							{
								NodeType otherNode = node2Data[currLink]; 
								if (otherNode <  nrV)
									nodeStack.push_back(otherNode);			
								currLink = nextLink1[currLink];
							}
							currLink = link2[currNode];
							while (currLink < nrE)
							{
								NodeType otherNode = node1Data[currLink];
								if (otherNode < nrV)
									nodeStack.push_back(otherNode);
								currLink = nextLink2[currLink];
							}
						}
					}
					while (!nodeStack.empty());
					++nrParts;
				}
			}

			ResultUnitType* resultUnit = debug_cast<ResultUnitType*>(res);
			assert(resultUnit);
			resultUnit->SetRange(Range<UInt32>(0, nrParts));

			DataWriteLock resLock(resSub, dms_rw_mode::write_only_all);
			auto resultSub = mutable_array_cast<PartType>(resLock);
			auto realResultData = resultSub->GetDataWrite();
			assert(realResultData.size() == nrV);
			assert(nrParts <= nrV);
			fast_copy(begin_ptr(resultData), end_ptr(resultData), realResultData.begin());
			resLock.Commit();
		}
		return true;
	}
};

// see https://github.com/ObjectVision/GeoDMS/issues/574
// see https://en.wikipedia.org/wiki/Strongly_connected_component
// see https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
//
// Input: F1: E->V, F2: E->V
// Output: P { Part_rel: V->P; F { F1: F->P; F2: F->P } }


class StronglyConnectedComponentsOperator : public BinaryOperator
{
	using NodeType = UInt32;
	using LinkType = UInt32;
	using PartType = UInt32;
	using PartLinkType = UInt32;

	using Arg1Type = DataArray<NodeType>; // E->V
	using Arg2Type = DataArray<NodeType>; // E->V

	using ResultUnitType = Unit<PartType>; // P
	using ResultSub1Type = DataArray<PartType>; // V->P
	using ResultSub2Type = Unit<PartLinkType>; // F
	using ResultSub2SubType = DataArray<PartType>; // F->P

	using NodeStackType = std::vector<NodeType>;

public:
	StronglyConnectedComponentsOperator() :
		BinaryOperator(&cogSCC, ResultUnitType::GetStaticClass(),
			Arg1Type::GetStaticClass(),
			Arg2Type::GetStaticClass()
		)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);

		const AbstrDataItem* arg1A = AsDataItem(args[0]); // F1: E->V
		const AbstrDataItem* arg2A = AsDataItem(args[1]); // F2: E->V
		arg1A->GetAbstrDomainUnit()->UnifyDomain(arg2A->GetAbstrDomainUnit());
		arg1A->GetAbstrValuesUnit()->UnifyValues(arg2A->GetAbstrValuesUnit());

		MG_CHECK(checked_domain<LinkType>(arg1A, "a1") == checked_domain<LinkType>(arg2A, "a2"));
		MG_CHECK(arg1A->GetAbstrValuesUnit() == arg2A->GetAbstrValuesUnit());

		AbstrUnit* res = ResultUnitType::GetStaticClass()->CreateResultUnit(resultHolder);
		assert(res);
		resultHolder = res;

		AbstrDataItem* resSub = CreateDataItem(res, s_PartNr, arg1A->GetAbstrValuesUnit(), res);
		MG_CHECK(resSub);

		if (!mustCalc)
			return true;

		DataReadLock arg1Lock(arg1A);
		DataReadLock arg2Lock(arg2A);

		const Arg1Type* arg1 = const_array_cast<NodeType>(arg1Lock);
		const Arg2Type* arg2 = const_array_cast<NodeType>(arg2Lock);

		assert(arg1 && arg2);

		auto e = arg1->GetTiledRangeData();
		auto v = arg1->GetValueRangeData();
		assert(e && v);

		auto node1Data = arg1->GetDataRead();
		auto node2Data = arg2->GetDataRead();

		NodeType nrV = v->GetElemCount();
		LinkType nrE = e->GetElemCount();

		assert(node1Data.size() == nrE);
		assert(node2Data.size() == nrE);

		// TODO G8: doe dit zoals in OperDistrict.cpp

		std::vector<PartType> resultData(nrV, UNDEFINED_VALUE(PartType));
		assert(resultData.size() == nrV);

		std::vector<LinkType>
			link1(nrV, UNDEFINED_VALUE(UInt32)),
			link2(nrV, UNDEFINED_VALUE(UInt32)),
			nextLink1(nrE, UNDEFINED_VALUE(UInt32)),
			nextLink2(nrE, UNDEFINED_VALUE(UInt32));

		InvertIntoLinkedList<NodeType, LinkType>(node1Data.begin(), link1.begin(), nextLink1.begin(), nrE, nrV);
		InvertIntoLinkedList<NodeType, LinkType>(node2Data.begin(), link2.begin(), nextLink2.begin(), nrE, nrV);

		NodeStackType nodeStack;

		UInt32 nrParts = 0;

		for (auto rb = resultData.begin(), ri = resultData.begin(), re = resultData.end(); ri != re; ++ri)
		{
			if (!IsDefined(*ri))
			{
				nodeStack.push_back(ri - rb);
				do
				{
					NodeType currNode = nodeStack.back(); assert(currNode < nrV);
					nodeStack.pop_back();

					PartType currPart = rb[currNode];
					assert(currPart == nrParts || !IsDefined(currPart));
					if (!IsDefined(currPart))
					{
						rb[currNode] = nrParts;

						LinkType currLink = link1[currNode];
						while (currLink < nrE)
						{
							NodeType otherNode = node2Data[currLink];
							if (otherNode < nrV)
								nodeStack.push_back(otherNode);
							currLink = nextLink1[currLink];
						}
						currLink = link2[currNode];
						while (currLink < nrE)
						{
							NodeType otherNode = node1Data[currLink];
							if (otherNode < nrV)
								nodeStack.push_back(otherNode);
							currLink = nextLink2[currLink];
						}
					}
				} while (!nodeStack.empty());
				++nrParts;
			}
		}

		ResultUnitType* resultUnit = debug_cast<ResultUnitType*>(res);
		assert(resultUnit);
		resultUnit->SetRange(Range<UInt32>(0, nrParts));

		DataWriteLock resLock(resSub, dms_rw_mode::write_only_all);
		auto resultSub = mutable_array_cast<PartType>(resLock);
		auto realResultData = resultSub->GetDataWrite();
		assert(realResultData.size() == nrV);
		assert(nrParts <= nrV);
		fast_copy(begin_ptr(resultData), end_ptr(resultData), realResultData.begin());
		resLock.Commit();

		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************


namespace 
{	
	ConnectedPartsOperator connParts;
}

/******************************************************************************/

