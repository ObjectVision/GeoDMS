#include "StoragePCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "GridStorageManager.h"
#include "TreeItemProps.h"
#include "AbstrDataItem.h"

#include "act/ActorVisitor.h"
#include "act/MainThread.h"

#include "Unit.h"
#include "Projection.h"

// ------------------------------------------------------------------------
// Implementation of IsGridDomain, HasGridDomain, GetGridDataDomain & GetPaletteData
// ------------------------------------------------------------------------

template <typename Item>
Item* InheritMutability(const TreeItem* storageHolder, const Item* curr)
{
	if (storageHolder->DoesContain(curr))
		return const_cast<Item*>(curr);
	return nullptr;
}

const AbstrUnit* GetGridDataDomainBase(const TreeItem * storageHolder)
{
	if (IsUnit(storageHolder))
	{
		const AbstrUnit* result = AsUnit(storageHolder);
		if (IsGridDomain(result))
			return result;
	}
	return nullptr;
}

const AbstrUnit* GetGridDataDomainRO(const TreeItem * storageHolder)
{
	const AbstrUnit* gdd = GetGridDataDomainBase(storageHolder);
	if (gdd)
		return gdd;

	const AbstrDataItem* gridData = AsDynamicDataItem(const_cast<TreeItem*>(storageHolder)->GetSubTreeItemByID(GRID_DATA_ID));

	if (gridData)
		return CheckedGridDomain(gridData);
	gdd = AsDynamicUnit(const_cast<TreeItem*>(storageHolder)->GetSubTreeItemByID(GRID_DOMAIN_ID));
	return gdd;
}

AbstrUnit* GetGridDataDomainRW(TreeItem * storageHolder)
{
	AbstrUnit* gdd = InheritMutability(storageHolder, GetGridDataDomainBase(storageHolder));
	if (gdd)
		return gdd;

	AbstrDataItem* gridData = AsDynamicDataItem(storageHolder->GetSubTreeItemByID(GRID_DATA_ID));
	if (gridData)
		return InheritMutability(storageHolder, CheckedGridDomain(gridData).get());
	gdd = AsDynamicUnit(storageHolder->GetSubTreeItemByID(GRID_DOMAIN_ID));
	if (gdd)
		return gdd;

	if (auto sm = storageHolder->GetStorageManager())
		if (auto nmsm = dynamic_cast<NonmappableStorageManager*>(sm))
			return nmsm->CreateGridDataDomain(storageHolder);
	return nullptr;
}

SharedDataItem GetPaletteData(const TreeItem * storageHolder)
{
	dms_assert(storageHolder);

	return AsDynamicDataItem(storageHolder->GetConstSubTreeItemByID(PALETTE_DATA_ID).get());
}

// *****************************************************************************
// 
// AbstrGridStorageManager
//
// *****************************************************************************

#include "Unit.h"
#include "UnitClass.h"

AbstrUnit* AbstrGridStorageManager::CreateGridDataDomain(const TreeItem* storageHolder)
{
	if (!m_GridDomainUnit)
	{
		m_GridDomainUnit = Unit<IPoint>::GetStaticClass()->CreateResultUnit(nullptr);
		try {
			StorageReadHandle storageHandle(this, storageHolder, m_GridDomainUnit, StorageAction::read, false);
			ReadUnitRange(*storageHandle.MetaInfo());
		}
		catch (...)
		{
			m_GridDomainUnit = nullptr;
		}
	}
	return m_GridDomainUnit;
}

ActorVisitState AbstrGridStorageManager::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor, const TreeItem* storageHolder, const TreeItem* self) const
{
	if (self != storageHolder && IsDataItem(self) && HasGridDomain(AsDataItem(self)))
	{
		const TreeItem* gridData = GetGridData(storageHolder);
		if (gridData && gridData != self && gridData != storageHolder)
		{
			dms_assert(!self->DoesContain(gridData)); // gridData is storageHolder or direct subItem thereof
// FIX: The following lines caused reading big GridData 
//			if (visitor(gridData) == AVS_SuspendedOrFailed) // self might be readData or readCount that requires the Projection Info of GridData
//				return AVS_SuspendedOrFailed;
		}
		const AbstrUnit* gridDomain = GetGridDataDomainRO(storageHolder);
		if (gridDomain && visitor(gridDomain) == AVS_SuspendedOrFailed) // self might be readData or readCount that requires the Projection Info of GridData
			return AVS_SuspendedOrFailed;
	}
	return NonmappableStorageManager::VisitSuppliers(svf, visitor, storageHolder, self);
}

//  --CLASSES------------------------------------------------------------------
namespace Grid {

	TileCount::TileCount(Int32 start, UInt32 diff, UInt32 tileSize)
	{
		// nr of strips needed
		UInt32 shift = 0;
		if (start < 0)
		{
			shift = 1 + ((-start) / tileSize);
			start += shift * tileSize;
		}
		dms_assert(start >= 0);
		UInt32 end = start + diff - 1;

		t_min = start / tileSize - shift;
		UInt32 t_max = end / tileSize - shift;
		t_cnt = t_max - t_min + 1;
		mod_begin = start% tileSize;
		mod_end = end  % tileSize;
	}

} //namespace Grid

// *****************************************************************************
// 
// scoped GridStorageMetaInfo
//
// *****************************************************************************

GridStorageMetaInfo::GridStorageMetaInfo(const TreeItem* storageHolder, TreeItem* focusItem, StorageAction sa)
	: GdalMetaInfo(storageHolder, focusItem)
{
	dms_assert(IsMetaThread());

	const AbstrDataItem* adi = AsDynamicDataItem(focusItem);
	if (!adi || !GridDomain(adi) || (sa != StorageAction::read))
		return;

	if (sqlStringPropDefPtr->HasNonDefaultValue(focusItem))
		m_SqlString = TreeItemPropertyValue(focusItem, sqlStringPropDefPtr);

	m_VPIP.emplace(storageHolder, adi, true, true);
}

void GridStorageMetaInfo::OnPreLock()
{
	StorageMetaInfo::OnPreLock();
	if (m_VPIP)
		m_VPIP->m_GridDomain->GetCount();
}

StorageMetaInfoPtr AbstrGridStorageManager::GetMetaInfo(const TreeItem* storageHolder, TreeItem* curr, StorageAction sa) const
{
	return std::make_unique<GridStorageMetaInfo>(storageHolder, curr, sa);
}

bool AbstrGridStorageManager::DoCheckFactorSimilarity(StorageMetaInfoPtr smi) const
{
	const GridStorageMetaInfo* gbr = debug_cast<const GridStorageMetaInfo*>(smi.get());
	bool result = true;
	if (!gbr)
		return false;

	auto vpi = gbr->m_VPIP.value().GetViewportInfoEx(no_tile, smi);
	auto grid_projection = gbr->m_VPIP->m_GridDomain->GetCurrProjection();
	auto curr_projection = gbr->m_VPIP->m_CurrDomain->GetCurrProjection();

	if (!grid_projection) // no projection, assume similar factor
		return true;
	if (!curr_projection) // no projection, assume similar factor
		return true;

	// actual factor comparison
	auto grid_factor = grid_projection->Factor();
	auto curr_factor = curr_projection->Factor();
	if (grid_factor != curr_factor)
	{
		reportF(MsgCategory::storage_read, SeverityTypeID::ST_Warning, "Factor difference encountered between item %s: [%d,%d] and storage %s: [%d,%d]",
			GetSourceName().c_str()
			, curr_factor.X()
			, curr_factor.Y()
			, GetNameStr().c_str()
			, grid_factor.X()
			, grid_factor.Y()
		);
		result = false;
	}

	return result;
}

bool AbstrGridStorageManager::DoCheck50PercentExtentOverlap(StorageMetaInfoPtr smi) const
{
	const GridStorageMetaInfo* gbr = debug_cast<const GridStorageMetaInfo*>(smi.get());
	bool result = true;

	if (!gbr)
		return true;

	auto vpi = gbr->m_VPIP.value().GetViewportInfoEx(no_tile, smi);
	auto grid_projection = gbr->m_VPIP->m_GridDomain->GetCurrProjection();
	auto curr_projection = gbr->m_VPIP->m_CurrDomain->GetCurrProjection();
	if (!grid_projection) // no projection, assume extent overlap >= 50%
		return true;
	if (!curr_projection) // no projection, assume extent overlap >= 50%
		return true;

	// affine transformed grid extent
	auto grid_extent = vpi.GetGridExtents();
	auto grid_factor = grid_projection->Factor();
	auto grid_offset = grid_projection->Offset();
	auto grid_extent_in_world_coordinates = DRect(DPoint(grid_extent.first) * grid_factor + grid_offset, DPoint(grid_extent.second) * grid_factor + grid_offset);

	// affine transformed curr extent
	auto curr_extent = vpi.GetViewPortExtents();
	auto curr_factor = grid_projection->Factor();
	auto curr_offset = grid_projection->Offset();
	auto curr_extent_in_world_coordinates = DRect(DPoint(curr_extent.first) * curr_factor + curr_offset, DPoint(curr_extent.second) * curr_factor + curr_offset);

	// actual extent check
	auto intersect = grid_extent_in_world_coordinates & curr_extent_in_world_coordinates;
	if (intersect.empty())
	{
		reportF(MsgCategory::storage_read, SeverityTypeID::ST_Warning, "Extent of domain of item %s and storage %s overlap less than 50 percent",
			GetSourceName().c_str()
			, GetNameStr().c_str()
		);
		return false;
	}

	auto read_area         = (curr_extent_in_world_coordinates.second - curr_extent_in_world_coordinates.first).X() * (curr_extent_in_world_coordinates.second - curr_extent_in_world_coordinates.first).Y();
	auto intersection_area = (intersect.second-intersect.first).X() * (intersect.second - intersect.first).Y();
	
	auto intersection_faction = intersection_area / read_area;
	if (intersection_faction < 0.5)
	{
		reportF(MsgCategory::storage_read, SeverityTypeID::ST_Warning, "Extent of domain of item %s and storage %s overlap less than 50 percent: %d",
			GetSourceName().c_str()
			, GetNameStr().c_str()
			, intersection_faction
		);
		result = false;
	}

	return result;
}


// ------------------------------------------------------------------------
// Implementation of IsGridDomain, HasGridDomain, GetGridDataDomain & GetPaletteData
// ------------------------------------------------------------------------

#define GRID_DATA		"GridData"
#define GRID_DOMAIN 	"GridDomain"
#define PALETTE_DATA	"PaletteData"

TokenID GRID_DATA_ID    = GetTokenID_st(GRID_DATA);
TokenID GRID_DOMAIN_ID  = GetTokenID_st(GRID_DOMAIN);
TokenID PALETTE_DATA_ID = GetTokenID_st(PALETTE_DATA);

bool IsGridDomain(const AbstrUnit* au)
{
	dms_assert(au);
	return au->GetNrDimensions() == 2 && au->CanBeDomain();
}

bool HasGridDomain(const AbstrDataItem* adi)
{
	dms_assert(adi);
	return IsGridDomain( adi->GetAbstrDomainUnit() );
}

SharedDataItem GetGridData(const TreeItem* storageHolder) // Look up the 'GridData' item
{
	dms_assert(storageHolder);

	SharedDataItem pData = AsDynamicDataItem(storageHolder->GetConstSubTreeItemByID(GRID_DATA_ID));
	if (pData && !GridDomain(pData)) 
		return nullptr;

	return pData;
}

SharedDataItem GetGridData(const TreeItem* storageHolder, bool projectionSpecsAvailable)
{
	auto pData = GetGridData(storageHolder);

	// try storageHolder
	if (!pData)
	{
		if (!projectionSpecsAvailable || storageHolder->IsStorable() )
		{
			pData = AsDynamicDataItem(storageHolder);
			if (pData && !GridDomain(pData)) 
				pData = nullptr;
		}
	}
	return pData;
}

SharedUnit GridDomain(const AbstrDataItem* adi)
{
	assert(adi);
	auto gridDomain = adi->GetAbstrDomainUnit();
	assert(gridDomain);
	if (!IsGridDomain(gridDomain))
		return {};
	return gridDomain;
}

SharedUnit CheckedGridDomain(const AbstrDataItem* adi)
{
	assert(adi); // PRECONDITION
	auto gridDomain = GridDomain(adi);
	if (!gridDomain)
		adi->throwItemErrorF(
			"ViewPortInfo expected a GridDataItem but the domain of this DataItem is %s"
		,	adi->GetAbstrDomainUnit()->GetSourceName().c_str()
		);
	return gridDomain;
}

