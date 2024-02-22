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

// *****************************************************************************
//									ConnectedParts
// *****************************************************************************

namespace {
	CommonOperGroup cogCP("connected_parts", oper_policy::better_not_in_meta_scripting);
}

static TokenID s_PartNr = GetTokenID_st("PartNr");

class ConnectedPartsOperator : public BinaryOperator
{
	typedef UInt32                 NodeType;
	typedef UInt32                 LinkType;
	typedef UInt32                 PartType;

	typedef DataArray<NodeType> Arg1Type; // E->V
	typedef DataArray<NodeType> Arg2Type; // E->V
	typedef Unit     <PartType> ResultUnitType;    
	typedef DataArray<PartType> ResultSubType; // V->P

	typedef std::vector<NodeType> NodeStackType;

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
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A= AsDataItem(args[0]);
		const AbstrDataItem* arg2A= AsDataItem(args[1]);
		arg1A->GetAbstrDomainUnit()->UnifyDomain(arg2A->GetAbstrDomainUnit());
		arg1A->GetAbstrValuesUnit()->UnifyValues(arg2A->GetAbstrValuesUnit());

		MG_CHECK(checked_domain<LinkType>(arg1A, "a1") == checked_domain<LinkType>(arg2A, "a2"));
		MG_CHECK(arg1A->GetAbstrValuesUnit() == arg2A->GetAbstrValuesUnit());

		AbstrUnit* res = ResultUnitType::GetStaticClass()->CreateResultUnit(resultHolder);
		dms_assert(res);
		resultHolder = res;

		AbstrDataItem* resSub = CreateDataItem(res, s_PartNr, arg1A->GetAbstrValuesUnit(), res);
		MG_CHECK(resSub);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
	
			const Arg1Type* arg1 = const_array_cast<NodeType>(arg1Lock);
			const Arg2Type* arg2 = const_array_cast<NodeType>(arg2Lock);

			dms_assert(arg1 && arg2);

			auto e = arg1->GetTiledRangeData();
			auto v = arg1->GetValueRangeData();
			dms_assert(e && v);

			auto node1Data = arg1->GetDataRead();
			auto node2Data = arg2->GetDataRead();

			NodeType nrV = v->GetElemCount();
			LinkType nrE = e->GetElemCount();

			dms_assert(node1Data.size() == nrE);
			dms_assert(node2Data.size() == nrE);

			// TODO G8: doe dit zoals in OperDistrict.cpp

			std::vector<PartType> resultData(nrV, UNDEFINED_VALUE(PartType));
			dms_assert(resultData   .size() == nrV); 

			std::vector<LinkType>
				link1    (nrV, UNDEFINED_VALUE(UInt32)),
				link2    (nrV, UNDEFINED_VALUE(UInt32)),
				nextLink1(nrE, UNDEFINED_VALUE(UInt32)), 
				nextLink2(nrE, UNDEFINED_VALUE(UInt32));

			Invert(node1Data.begin(), link1.begin(), nextLink1.begin(), nrE, nrV);
			Invert(node2Data.begin(), link2.begin(), nextLink2.begin(), nrE, nrV);

			NodeStackType nodeStack;
			
			UInt32 nrParts = 0;

			for (auto rb = resultData.begin(), ri = resultData.begin(), re = resultData.end(); ri != re; ++ri)
			{
				if (!IsDefined(*ri))
				{
					nodeStack.push_back(ri - rb);
					do 
					{
						NodeType currNode =  nodeStack.back(); dms_assert(currNode < nrV);
						nodeStack.pop_back();

						PartType currPart = rb[ currNode ];
						dms_assert(currPart == nrParts || !IsDefined(currPart));
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
			dms_assert(resultUnit);
			resultUnit->SetRange(Range<UInt32>(0, nrParts));

			DataWriteLock resLock(resSub, dms_rw_mode::write_only_all);
			auto resultSub = mutable_array_cast<PartType>(resLock);
			auto realResultData = resultSub->GetDataWrite();
			dms_assert(realResultData.size() == nrV);
			dms_assert(nrParts <= nrV);
			fast_copy(begin_ptr(resultData), end_ptr(resultData), realResultData.begin());
			resLock.Commit();
		}
		return true;
	}

	void Invert(
		const NodeType* nodeIdArray, 
		std::vector<LinkType>::iterator backPtr, 
		std::vector<LinkType>::iterator nextPtr, 
		UInt32 edgeCount, 
		UInt32 nodeCount
	) const
	{
		for (UInt32 edgeNr=0; edgeNr != edgeCount; ++edgeNr)
		{
			UInt32 nodeId = *nodeIdArray++;
			if (nodeId < nodeCount)
			{
				nextPtr[edgeNr] = backPtr[nodeId];
				backPtr[nodeId] = edgeNr;
			}
		}
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

