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

	STGDLL_CALL void SetWritability(AbstrDataItem* adi) const;

	rect_type GetGridExtents() const { return m_GridExtents; }

protected:
	rect_type  m_GridExtents; // [in Grid units]
};

struct ViewPortInfoProvider
{
	STGDLL_CALL ViewPortInfoProvider(const TreeItem * storageHolder, const AbstrDataItem* adi, bool mayCreateDomain, bool queryActualRange);

	STGDLL_CALL ViewPortInfoEx<Int32> GetViewportInfoEx(tile_id tc, StorageMetaInfoPtr smi, tile_id tg=no_tile) const;

	SharedPtr<const AbstrDataItem > m_ADI;
	SharedUnitInterestPtr m_CurrDomain; // target domain when reading from storage or viewport extent when drawing
	SharedUnitInterestPtr m_GridDomain; // source pixel range in case of storage or domain of dataitem when drawing
	countcolor_t m_CountColor;
	bool         m_QueryActualGridDomain;
};

#endif __STG_VIEWPORTINFOEX_H
