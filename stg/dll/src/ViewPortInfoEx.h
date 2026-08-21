// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__STG_VIEWPORTINFOEX_H)
#define __STG_VIEWPORTINFOEX_H

#include "StgBase.h"
#include "ViewPortInfo.h"

// *****************************************************************************

template<typename SignedInt>
struct ViewPortInfoEx : public ViewPortInfo<SignedInt>
{
	using rect_type = typename ViewPortInfo<SignedInt>::rect_type;

	STGDLL_CALL ViewPortInfoEx(const TreeItem* context, const AbstrUnit* currDomain, tile_id tc, const AbstrUnit* gridDomain, tile_id tg, StorageMetaInfoPtr smi=nullptr
		, bool correctGridOffset = false,  bool mustCheck = true, countcolor_t cc = -1, bool queryActualGridDomain = true);

	bool Is1to1() const 
	{ 
		return this->m_ViewPortExtents == m_GridExtents 
			&& CrdTransformation::IsIdentity() 
			&& this->m_CountColor == -1;
	}
	bool IsGridCurrVisible() const
	{
		return IsIntersecting(m_GridExtents, this->GetViewPortInGridAsIRect());
	}

	void SetWritability(AbstrDataItem* adi) const;

	rect_type GetGridExtents() const { return m_GridExtents; }

protected:
	rect_type  m_GridExtents; // [in Grid units]
};

struct ViewPortInfoProvider
{
	ViewPortInfoProvider(const TreeItem * storageHolder, const AbstrDataItem* adi, bool mayCreateDomain, bool queryActualRange);

	ViewPortInfoEx<Int32> GetViewportInfoEx(tile_id tc, StorageMetaInfoPtr smi, tile_id tg=no_tile) const;

	std::shared_ptr<const AbstrDataItem > m_ADI;
	SharedUnitInterestPtr m_CurrDomain; // target domain when reading from storage or viewport extent when drawing
	SharedUnitInterestPtr m_GridDomain; // source pixel range in case of storage or domain of dataitem when drawing
	countcolor_t m_CountColor;
	bool         m_QueryActualGridDomain;
};

#endif __STG_VIEWPORTINFOEX_H
