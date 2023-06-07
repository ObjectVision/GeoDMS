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

#include "ClcPCH.h"
#pragma hdrstop

#include "dbg/DebugContext.h"
#include "dbg/SeverityType.h"
#include "mci/CompositeCast.h"
#include "geo/Conversions.h"

#include "TileArrayImpl.h"
#include "DataCheckMode.h"
#include "OperationContext.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"
#include "TreeItemClass.h"

/*
overlay takes the following arguments:

	partitioningNames: P->RegioSetName;
	domainUnit
	partitionings
	{
		for each P:
			Partitioning: domainUnit->(RegioSet[P]: ArgID)
	}

and results in:
	unit<ResID> AtomicRegionUnit
	{
		GridData: gridDomain -> AtomicRegionUnit
		for each P:
			Partitioning: AtomicRegionUnit->(RegioSet[P]: ArgID)
	}
with ResID = UInt16; ArgID = UInt8
*/

// *****************************************************************************
//									Helping structures
// *****************************************************************************

typedef UInt32 ProdID;

struct overlay_partitioning_info_t
{
	overlay_partitioning_info_t(
		const AbstrDataItem* adi,
		AbstrDataItem* resPartRel)
			:	m_DataItem(adi)
			,	m_ResPartRel(resPartRel)
	{}

	TokenStr GetName() const { return m_DataItem->GetName(); }

	const AbstrDataItem*          m_DataItem;
	AbstrDataItem*                m_ResPartRel;
};

using overlay_partitionong_info_array = std::vector<overlay_partitioning_info_t>;

struct overlay_recode_info_t
{
	UInt32 m_NrRegions;
	ProdID m_AtomicRegionFactor;
};

// *****************************************************************************
//									Overlay function
// *****************************************************************************

struct OverlayLayerVisitorBase : UnitProcessor
{
	template <typename E>
	void VisitImpl(const Unit<E>* regionUnit) const
	{
		const DataArray<E>* dataArray = const_array_cast<E>(m_PartitioningInfo->m_DataItem);
		MG_CHECK(dataArray);

		if (regionUnit->GetRange().first)
			throwErrorF("Overlay", "ValuesUnit of %s is not zero based", m_PartitioningInfo->GetName());

		UInt32 nrRegions = regionUnit->GetCount();
		m_RecodeInfo->m_NrRegions          = nrRegions;
		m_RecodeInfo->m_AtomicRegionFactor = m_AtomicRegionFactor;

		DataReadLock partitionLock(m_PartitioningInfo->m_DataItem); 
		dms_assert(partitionLock.IsLocked());

		if (m_PartitioningInfo->m_DataItem->GetRawCheckMode() != DCM_None)
			m_CanContainNulls = true;

		for (tile_id t = 0; t != m_NrTiles; ++t)
		{
			auto partitioningData = dataArray->GetLockedDataRead(t);
			auto prodIdMapData    = m_ProdIdMapDO->GetLockedDataWrite(t);

			dbg_assert(partitioningData.size() == prodIdMapData.size());

			MG_CHECK(!nrRegions || m_AtomicRegionFactor <= MAX_VALUE(ProdID) / nrRegions);
			auto pd = partitioningData.begin();

			auto
				prodIdMapIter = prodIdMapData.begin(), 
				prodIdMapEnd  = prodIdMapData.end();

			if (m_AtomicRegionFactor == 1)
			{
				if (m_CanContainNulls)
					for (; prodIdMapIter != prodIdMapEnd; ++prodIdMapIter, ++pd)
					{
						ProdID regionId = Convert<ProdID>(*pd);
						if (regionId < nrRegions)
							*prodIdMapIter = regionId;
						else
							*prodIdMapIter = UNDEFINED_VALUE(ProdID);
					}
				else
					for (; prodIdMapIter != prodIdMapEnd; ++prodIdMapIter, ++pd)
					{
						ProdID regionId = Convert<ProdID>(*pd);
						dms_assert(regionId < nrRegions);
						*prodIdMapIter = regionId;
					}
			}
			else
			{
				if (m_CanContainNulls)
					for (; prodIdMapIter != prodIdMapEnd; ++prodIdMapIter, ++pd)
					{
						ProdID regionId = Convert<ProdID>(*pd);
						if (regionId < nrRegions)
						{
							if (IsDefined(*prodIdMapIter))
							{
								dms_assert(*prodIdMapIter < m_AtomicRegionFactor);
								*prodIdMapIter += (regionId * m_AtomicRegionFactor);
							}
						}
						else
							*prodIdMapIter = UNDEFINED_VALUE(ProdID);
					}
				else
					for (; prodIdMapIter != prodIdMapEnd; ++prodIdMapIter, ++pd)
					{
						ProdID regionId = Convert<ProdID>(*pd);
						dms_assert(regionId < nrRegions);
						dms_assert(*prodIdMapIter < m_AtomicRegionFactor);
						*prodIdMapIter += (regionId * m_AtomicRegionFactor);
					}
			}
		}
		m_AtomicRegionFactor *= nrRegions;
	}

	const overlay_partitioning_info_t* m_PartitioningInfo = nullptr;
	overlay_recode_info_t*             m_RecodeInfo = nullptr;

	tile_id                            m_NrTiles = 0;
	SharedPtr<DataArray<ProdID>>       m_ProdIdMapDO;
	mutable ProdID                     m_AtomicRegionFactor = 1;
	mutable bool                       m_CanContainNulls = false;
};

struct OverlayLayerVisitor :  boost::mpl::fold<typelists::domain_ints, OverlayLayerVisitorBase, VisitorImpl<Unit<_2>, _1> >::type
{};


template <typename ResID>
void DoOverlay(AbstrDataItem* resAtomicRegionGrid, IterRange<const overlay_partitioning_info_t*> partitioningInfo, AbstrUnit* resAtomicRegions)
{
	//	generate unit atomicRegion with { atomicRegionMap->atomicRegion and for each partitioning atomicRegion->input region }

	std::vector<overlay_recode_info_t>  recodeFactors; //m_PartitionInfo;   // 1 per partitioning  (==  p )

	const AbstrUnit* domain = resAtomicRegionGrid->GetAbstrDomainUnit();
	auto trd = domain->GetTiledRangeData();
	OverlayLayerVisitor visitor;
	visitor.m_NrTiles     = domain->GetNrTiles();
	visitor.m_ProdIdMapDO = CreateHeapTileArrayV<ProdID>(trd, nullptr, false MG_DEBUG_ALLOCATOR_SRC("Overlay: m_ProdIdMapDO")).release();
//		std::make_unique<HeapTileArray<ProdID>>(trd, false);

//	REMOVE DataWriteLock prodIdMapLock(visitor.m_ProdIdMap);

	UInt32 p = partitioningInfo.size();
	recodeFactors.resize(p);

	// make atomicRegionMap with atomicRegionID's = prodNr := p1*1 + p2*#p1 + p3*#p1*#p2 + ... = p1+#p1*(p2+#p2*(p3+...)))

	if (p) for (UInt32 j = 0; j!=p; ++j)
	{
		visitor.m_PartitioningInfo = &( partitioningInfo[j] );
		visitor.m_RecodeInfo       = &( recodeFactors   [j] );
		visitor.m_PartitioningInfo->m_DataItem->GetAbstrValuesUnit()->InviteUnitProcessor(visitor);
	}
	else
	{
		for (tile_id t = 0; t != visitor.m_NrTiles; ++t)
		{
			DataArray<ProdID>::locked_seq_t prodIdMap = visitor.m_ProdIdMapDO->GetLockedDataWrite(t);
			fast_undefine(prodIdMap.begin(), prodIdMap.end());
		}
	}
	// count atomicRegionID's in the atomicMap and count the unique nr of observed combinations
	UInt32 nrAtomicRegions = 0;                                    // the unique nr of observed combinations
	std::vector<UInt32> atomicRegionCounts(visitor.m_AtomicRegionFactor, 0); // # of observed combinations in the atomic map

	for (tile_id t = 0; t != visitor.m_NrTiles; ++t)
	{
		auto prodIdMap = visitor.m_ProdIdMapDO->GetLockedDataRead(t);

		for (typename sequence_traits<ProdID>::const_pointer
			prodIdMapIter = prodIdMap.begin(),
			prodIdMapEnd = prodIdMap.end()
			;	prodIdMapIter != prodIdMapEnd
			;	++prodIdMapIter
			)
		{
			if (IsDefined(*prodIdMapIter))
			{
				dms_assert(*prodIdMapIter < visitor.m_AtomicRegionFactor);
				if (atomicRegionCounts[*prodIdMapIter]++ == 0)
					++nrAtomicRegions;
			}
		}
	}

	MG_CHECK(nrAtomicRegions <= MAX_VALUE(ResID));
	resAtomicRegions->SetCount(nrAtomicRegions);

	// collect atomicRegionIds and replace atomicRegionCounts by atomicRegionIds
	typedef std::pair<UInt32, ProdID> atomic_region_count_t;
	std::vector<atomic_region_count_t> atomicRegions;   // 1 per atomic region (== ar )
	atomicRegions.reserve(nrAtomicRegions);

	std::vector<UInt32>::iterator
		arcBeg = atomicRegionCounts.begin(),
		arcPtr = arcBeg,
		arcEnd = atomicRegionCounts.end();
	while(arcPtr != arcEnd)
	{
		if (*arcPtr)
		{
			*arcPtr = atomicRegions.size(); // id[prodNr] == curr atomic region ID
			atomicRegions.push_back(atomic_region_count_t(*arcPtr, arcPtr - arcBeg));  // (aantal, prodNr)
		}	
		++arcPtr;
	}
	dms_assert(atomicRegions.size() == nrAtomicRegions);

	// number atomicRegionMap according to (id+1) in atomicRegionCounts
	DataWriteLock resAtomicRegionGridLock(resAtomicRegionGrid);
	for (tile_id t = 0; t != visitor.m_NrTiles; ++t)
	{
		typename DataArray<ResID>::locked_seq_t atomicRegionMap = mutable_array_cast<ResID>(resAtomicRegionGridLock)->GetLockedDataWrite(t);
		ResID* arm_iter = atomicRegionMap.begin();
		ResID* arm_end  = atomicRegionMap.end();

		auto prodIdMap = visitor.m_ProdIdMapDO->GetLockedDataRead(t);
		for (sequence_traits<ProdID>::const_pointer prodIdMapIter = prodIdMap.begin(); 
			arm_iter != arm_end; 
			++prodIdMapIter, ++arm_iter)
		{
			if (IsDefined(*prodIdMapIter))
			{
				dms_assert(*prodIdMapIter < visitor.m_AtomicRegionFactor);
				*arm_iter = atomicRegionCounts[*prodIdMapIter];  // map[curr elem] := id[ prodNr[curr elem] ]
			}
			else
				*arm_iter = UNDEFINED_VALUE(ResID);

		}
	}
	resAtomicRegionGridLock.Commit();

	// save per partitioning: AR->region relation
	const overlay_partitioning_info_t* pPtr = partitioningInfo.begin();
	const overlay_partitioning_info_t* pEnd = partitioningInfo.end();

	std::vector<overlay_recode_info_t>::const_iterator rPtr = recodeFactors.begin();
	for (; pPtr != pEnd; ++rPtr, ++pPtr)
	{
		UInt32
			regionFactor = rPtr->m_AtomicRegionFactor,
			nrRegions    = rPtr->m_NrRegions;

		DataWriteLock regionLock(pPtr->m_ResPartRel);

//		AbstrDataObject* resPartRelObj = pPtr->m_ResPartRel->GetDataObj();

		std::vector<atomic_region_count_t>::const_iterator
			arPtr = atomicRegions.begin(),
			arEnd = atomicRegions.end();
		UInt32 c = 0;
		for (; arPtr != arEnd; ++arPtr, ++c)
		{
			UInt32 atomicCode = arPtr->second;
			UInt32 r = (atomicCode / regionFactor) % nrRegions;
			regionLock->SetValueAsUInt32(c, r);
		}
		regionLock.Commit();
	}
}

// *****************************************************************************
//									Overlay operator
// *****************************************************************************

static TokenID t_UnionData = GetTokenID_st("UnionData");

template <typename ResID>
class OverlayOperator : public TernaryOperator
{
	typedef DataArray<SharedStr> Arg1Type;          // partitions->name
	typedef AbstrUnit            Arg2Type;          // domain of cells
//	typedef TreeItem             Arg3Type;          // container with named partitionings
	typedef Unit<ResID>          ResultType;

public:
	OverlayOperator(AbstrOperGroup* gr)
		:	TernaryOperator(gr, ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass(), // ggPartitionName array
				Arg2Type::GetStaticClass(), // gridDomain
				TreeItem::GetStaticClass()  // container met attributes<p_u8> partitioningName(domain) per p (verschillende indelingen) 
			)
	{}

	// Override Operator
	virtual void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext* fc, LispPtr) const
	{
		dms_assert(args.size() == 3);

		const AbstrDataItem* ggPartNamesA = AsDataItem(args[0]);
		const AbstrUnit*     ggPartSet    = ggPartNamesA->GetAbstrDomainUnit();

		const AbstrUnit* gridDomain = debug_cast<const AbstrUnit*>(GetItem(args[1]));

		UInt32 p = ggPartSet->GetCount();
		MG_CHECK(p < 256);

		AbstrUnit* resAtomicRegions = Unit<ResID>::GetStaticClass()->CreateResultUnit(resultHolder);
		assert(resAtomicRegions);
		resAtomicRegions->SetTSF(TSF_Categorical);

		resultHolder = resAtomicRegions;

		AbstrDataItem* resAtomicRegionGrid = CreateDataItem(resAtomicRegions, t_UnionData, gridDomain, resAtomicRegions);

		overlay_partitionong_info_array partitionInfo;

		// make AtomicRegionsSet, gridDomain->AtomicRegionSet and UniqeRegions -> (AtomicRegionsSet -> Region)
		DataReadLock ggPartNamesLock(ggPartNamesA);

		const DataArray<SharedStr>* ggPartNames = const_array_cast<SharedStr>(ggPartNamesA);
		assert(ggPartNames);

		for (UInt32 j = 0; j != p; ++j)
		{
			SharedStr partName = ggPartNames->GetIndexedValue(j);
			TokenID partNameID = GetTokenID_mt( partName.c_str() );

			CDebugContextHandle context("overlay", partName.c_str(), true);

			const TreeItem* partitioningTI = GetItem(args[2])->GetConstSubTreeItemByID(partNameID);
			if (!partitioningTI)
				throwErrorF("Overlay", "%s not found in %s", partNameID.GetStr().c_str(), GetItem(args[2])->GetSourceName().c_str());
			const AbstrDataItem* partitioningDI = AsCheckedDataItem(partitioningTI);
			assert(partitioningDI);

			AbstrDataItem* resPartitionRel = 
				CreateDataItem(
					resAtomicRegions, 
					partNameID, 
					resAtomicRegions, partitioningDI->GetAbstrValuesUnit()
				);

			partitionInfo.push_back( 
				overlay_partitioning_info_t(
					partitioningDI,
					resPartitionRel
				) 
			);
			fc->AddDependency(partitioningDI->GetCheckedDC()); // requires Meta info.
//			fc->AddDependency(partitioningDI->GetAbstrValuesUnit()); // and of valuesunit, or is that included?
		}
		fc->m_MetaInfo.emplace<overlay_partitionong_info_array>(std::move(partitionInfo));
	}

	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, OperationContext* fc, Explain::Context* context) const override
	{
		dms_assert(resultHolder);
		const overlay_partitionong_info_array& partitionInfo = *any_cast<overlay_partitionong_info_array>(&fc->m_MetaInfo);

		AbstrUnit* resAtomicRegions = AsUnit(resultHolder.GetNew());
		dbg_assert(resAtomicRegions);

		AbstrDataItem* resAtomicRegionGrid = AsDataItem(resAtomicRegions->GetSubTreeItemByID(t_UnionData));

		reportD(SeverityTypeID::ST_MajorTrace, "overlay started");
		DoOverlay<ResID>(resAtomicRegionGrid, IterRange<const overlay_partitioning_info_t*>(&partitionInfo), resAtomicRegions);

		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	oper_arg_policy overlay_oap[3] = { oper_arg_policy::calc_always, oper_arg_policy::calc_as_result, oper_arg_policy::subst_with_subitems };

	SpecialOperGroup overlay16Group("overlay", 3, overlay_oap);
	SpecialOperGroup overlay32Group("overlay32", 3, overlay_oap);
//	OverlayOperator<UInt16,  UInt8>  overlay8 (overlayGroup);
	OverlayOperator<UInt16> overlay16(&overlay16Group);
	OverlayOperator<UInt32> overlay32(&overlay32Group);
}

/******************************************************************************/

/* MOVE TO SubSet variant operator
TODO, TEMP, XXX

UInt32 count(DataArray<bool>::cseq_t filter)
{
	UInt32 c = 0;
	DataArray<bool>::const_iterator
		fi = filter.begin(),
		fe = filter.end();
	while (fi != fe)
		if(*fi++) ++c;
	return c;
}

template <typename V>
void Compact(
	std::vector<V>& result, 
	DataArray<V>   ::cseq_t source,
	DataArray<bool>::cseq_t filter
)
{
	DataArray<V>::const_iterator
		si = source.begin(),
		se = source.end();
	DataArray<bool>::const_iterator
		fi = filter.begin();
	while (si != se)
	{
		if (*fi++)
			result.push_back(*si);
		++si;
	}
	dms_assert(fi == filter.end());

}

template <typename V>
void Expand(
	DataArray<V>::seq_t     result,
	const std::vector<V>&    compact, 
	DataArray<bool>::cseq_t filter
)
{
	std::vector<V>::const_iterator
		ci = compact.begin();

	DataArray<bool>::const_iterator
		fi = filter.begin();

	vector_fill_n(result, filter.size(), UNDEFINED_VALUE(V));
	DataArray<V>::iterator
		ri = result.begin(),
		re = result.end();

	while (ri != re)
	{
		if (*fi++)
			*ri = *ci++
		++ri;
	}
	dms_assert(ci == compact.end());
	dms_assert(fi == filter .end());

}
END MOVE */

