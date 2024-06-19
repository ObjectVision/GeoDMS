// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "CheckedDomain.h"
#include "TileChannel.h"
#include "Unit.h"
#include "UnitClass.h"

template <typename NodeType, typename LinkType>
void InvertIntoLinkedList(LinkType edgeCount, NodeType nodeCount,
	auto nodeIdArray,
	LinkType* backPtr, LinkType* nextPtr)
{
	for (LinkType edgeNr = 0; edgeNr != edgeCount; ++edgeNr, ++nodeIdArray)
	{
		NodeType nodeId = *nodeIdArray;
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
static TokenID s_PartLink = GetTokenID_st("PartLink");
static TokenID s_PartFromRel = GetTokenID_st("from_rel");
static TokenID s_PartToRel = GetTokenID_st("to_rel");

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
				link1    (nrV, UNDEFINED_VALUE(LinkType)),
				link2    (nrV, UNDEFINED_VALUE(LinkType)),
				nextLink1(nrE, UNDEFINED_VALUE(LinkType)),
				nextLink2(nrE, UNDEFINED_VALUE(LinkType));

			InvertIntoLinkedList<NodeType, LinkType>(nrE, nrV, node1Data.begin(), begin_ptr(link1), begin_ptr(nextLink1));
			InvertIntoLinkedList<NodeType, LinkType>(nrE, nrV, node2Data.begin(), begin_ptr(link2), begin_ptr(nextLink2));

			NodeStackType nodeStack;
			
			PartType nrParts = 0;

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
			assert(nrParts <= nrV);
			resultUnit->SetRange(Range<UInt32>(0, nrParts));

			auto resultSubChannel = locked_tile_write_channel<PartType>(resSub);
			resultSubChannel.Write(begin_ptr(resultData), end_ptr(resultData));
			resultSubChannel.Commit();
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

#include "ptr/OwningPtrSizedArray.h"

template<typename NodeType, typename LinkType, typename PartType, typename PartLinkType>
auto stronglyConnectedComponentsIterativeWithInvertedLinks(NodeType nrV, LinkType nrE, const LinkType* link1, const LinkType* nextLink1, const NodeType* node2Data, PartType* resSubData)
{
	sequence_traits<Bool>::container_type onStackFlag(nrV, false);
	std::vector<NodeType> nodeStack;
	std::vector<Couple<PartType>> partLinks;

	std::vector<std::tuple<NodeType, LinkType, bool>> stack; // Node, CurrentLink, didVisitLink
	OwningPtrSizedArray< Couple < PartType >> timedPartLinkLinkedList; // avoid using a std::set for the partLinks from currentPart
	OwningPtrSizedArray<Couple<NodeType>> indices(nrV, dont_initialize MG_DEBUG_ALLOCATOR_SRC("stronglyConnectedComponents: indices"));
	for (auto& index : indices)
		index.first = UNDEFINED_VALUE(NodeType);
	assert(indices.size() == nrV);

	PartType nrParts = 0;
	PartLinkType nrPartLinks = 0;
	NodeType currIndex = 0;

	auto pushNode = [&](NodeType v) {
			// Initial visit actions (similar to the start of the recursive function)
			indices[v] = { currIndex, currIndex }; // Set the depth index for v to the smallest unused index
			++currIndex;
			nodeStack.emplace_back(v);
			assert(!onStackFlag[v]);
			onStackFlag[v] = true;
			stack.emplace_back(v, link1[v], false); // Start with first visit of v
		};

	for (NodeType newV = 0; newV != nrV; ++newV)
	{
		if (IsDefined(indices[newV].first))
			continue;
		pushNode(newV);
		while (!stack.empty()) {
			auto& [v, currentLink, didVisitLink] = stack.back();
			// Processing neighbors or finishing up
		retry:
			if (IsDefined(currentLink)) {
				NodeType w = node2Data[currentLink];

				if (didVisitLink)
				{
					MakeMin(indices[v].second, indices[w].second);
					didVisitLink = false;
				}
				else
				{
					if (!IsDefined(indices[w].first)) {
						// Neighbor w has not yet been visited, push to stack for processing
						didVisitLink = true;
						pushNode(w);
						continue; // recurse by iteration
					}
					else if (onStackFlag[w].value()) {
						// Successor w is in stack S and hence in the current SCC
						// If w is not on stack, then (v, w) is an edge pointing to an SCC already found and must be ignored
						// The next line may look odd - but is correct.
						// It says w.index not w.lowlink; that is deliberate and from the original paper
						MakeMin(indices[v].second, indices[w].first);
					}
					// Move to the next link
				}
				currentLink = nextLink1[currentLink];
				goto retry; // continue processing neighbors, no do while loop to allow continue to jump to processing next or previous stack frame
			}
			// Final actions (similar to the end of the recursive function)
			// If v is a root node, pop the stack and generate an SCC
			if (indices[v].first == indices[v].second)
			{
				// start a new strongly connected component
				// use nrParts (initially zero) as chrono-number for accessing timedPartLinkLinkedList
				if (nrParts >= timedPartLinkLinkedList.size())
				{
					timedPartLinkLinkedList = OwningPtrSizedArray< Couple < PartType> >(nrParts * 2, dont_initialize MG_DEBUG_ALLOCATOR_SRC("timedPartLinkLinkedList"));
					for (PartType i = 0; i != nrParts; ++i)
						timedPartLinkLinkedList[i].first = UNDEFINED_VALUE(NodeType);
				}
				else
				{
					timedPartLinkLinkedList[nrParts - 1].first = UNDEFINED_VALUE(NodeType);
				}
				PartType lastSubPart = UNDEFINED_VALUE(PartType);

				NodeType w;
				do {
					w = nodeStack.back(); nodeStack.pop_back();
					assert(onStackFlag[w]);
					onStackFlag[w] = false;
					resSubData[w] = nrParts; // add w to current strongly connected component

					for (LinkType e = link1[w]; IsDefined(e); e = nextLink1[e])
					{
						NodeType ww = node2Data[e];
						PartType pp = resSubData[ww];
						if (IsDefined(pp))
						{
							assert(pp <= nrParts);
							if (pp < nrParts)
							{
								// set the chronos of pp to the current part to avoid adding pp to the partLinks of the current part more than once
								// aka Lazy Initialization
								if (timedPartLinkLinkedList[pp].first != nrParts)
								{
									timedPartLinkLinkedList[pp].second = lastSubPart;
									lastSubPart = pp;
									timedPartLinkLinkedList[pp].first = nrParts;
								}
							}
						}
					}
				} while (w != v);
				while (IsDefined(lastSubPart))
				{
					partLinks.emplace_back(Couple<PartType>(nrParts, lastSubPart));
					assert(timedPartLinkLinkedList[lastSubPart].first == nrParts);
					lastSubPart = timedPartLinkLinkedList[lastSubPart].second;
				}
				++nrParts;
			}
			stack.pop_back();
		}
	}
	return std::make_pair(nrParts, std::move(partLinks));
}

template<typename NodeType, typename LinkType, typename PartType, typename PartLinkType>
auto stronglyConnectedComponentsIterative(NodeType nrV, LinkType nrE, const DataArray<NodeType>* arg1, const NodeType* node2Data, PartType* resSubData)
{
	std::vector<LinkType>
		link1(nrV, UNDEFINED_VALUE(LinkType)),
		nextLink1(nrE, UNDEFINED_VALUE(LinkType));
	{
//		auto node1Data = arg1->GetDataRead();
//		assert(node1Data.size() == nrE);
		InvertIntoLinkedList<NodeType, LinkType>(nrE, nrV, tile_read_channel(arg1), begin_ptr(link1), begin_ptr(nextLink1));
	}
	return stronglyConnectedComponentsIterativeWithInvertedLinks<NodeType, LinkType, PartType, PartLinkType>(nrV, nrE, begin_ptr(link1), begin_ptr(nextLink1), node2Data, resSubData);
}

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

		AbstrUnit* resSub2 = ResultSub2Type::GetStaticClass()->CreateUnit(res, s_PartLink); // PartLinks
		AbstrDataItem* resPartFrom = CreateDataItem(resSub2, s_PartFromRel, resSub2, res);
		AbstrDataItem* resPartTo   = CreateDataItem(resSub2, s_PartToRel, resSub2, res);

		if (!mustCalc)
			return true;

		DataReadLock arg1Lock(arg1A);
		DataReadLock arg2Lock(arg2A);

		const Arg1Type* arg1 = const_array_cast<NodeType>(arg1Lock); assert(arg1);
		const Arg2Type* arg2 = const_array_cast<NodeType>(arg2Lock); assert(arg2);

		auto linkDomainData = arg1->GetTiledRangeData(); assert(linkDomainData);
		auto nodeDomainData = arg1->GetValueRangeData(); assert(nodeDomainData);

		auto node2Data = arg2->GetDataRead();

		NodeType nrV = nodeDomainData->GetElemCount();
		LinkType nrE = linkDomainData->GetElemCount();

		assert(node2Data.size() == nrE);

		OwningPtrSizedArray<PartType> resSubData(nrV, dont_initialize MG_DEBUG_ALLOCATOR_SRC("StronglyConnectedComponentsOperator: resSubData"));
		fast_undefine(resSubData.begin(), resSubData.end());

		auto [nrParts, partLinks] = stronglyConnectedComponentsIterative<NodeType, LinkType, PartType, PartLinkType>(nrV, nrE
			, arg1
			, node2Data.begin()
			, resSubData.begin());

		ResultUnitType* resultUnit = debug_cast<ResultUnitType*>(res);
		assert(resultUnit);
		resultUnit->SetRange(Range<PartType>(0, nrParts));

		auto resultSubChannel = locked_tile_write_channel<PartType>(resSub);
		resultSubChannel.Write(resSubData.begin(), resSubData.end() );
		resultSubChannel.Commit();
		resSubData.reset();

		ResultSub2Type* partLinkUnit = debug_cast<ResultSub2Type*>(resSub2);
		partLinkUnit->SetRange(Range<PartLinkType>(0, partLinks.size()));

		auto partFromChannel = locked_tile_write_channel<PartType>(resPartFrom);
		auto partToChannel = locked_tile_write_channel<PartType>(resPartTo);
		for (const auto& partLink : partLinks)
		{
			partFromChannel.Write(partLink.first);
			partToChannel.Write(partLink.second);
		}
		partFromChannel.Commit();
		partToChannel.Commit();

		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************


namespace 
{	
	ConnectedPartsOperator connPartsOper;
	StronglyConnectedComponentsOperator sccOper;
}

/******************************************************************************/

