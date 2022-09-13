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
// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(__SHV_LAYERINFO_H)
#define __SHV_LAYERINFO_H

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "ptr/InterestHolders.h"
#include "ptr/SharedStr.h"

#include "ShvBase.h"
#include "Aspect.h"

//----------------------------------------------------------------------
// section : LayerInfo
//----------------------------------------------------------------------

struct LayerInfo
{
	enum State {
		Undefined, 
		PaletteMissing, 
		ClassificationMissing, 
		NotNumeric,
		ConfigError, 
		                                                   Feature,  Grid,        DirFeat, DirGrid,     Aspect 
	} m_State;

	SharedUnitInterestPtr     m_uAspectOrFeature;      //  Crd       Crd          Crd       Crd         Aspect
	SharedDataItemInterestPtr m_diAspectOrFeature;     //  P ->Crd   nil          P->Crd    nil         C->Aspect
	SharedDataItemInterestPtr m_diClassBreaksOrExtKey; // (P ->EK)   SPoint->EK   nil       SPoint->V   C->V
	SharedDataItemInterestPtr m_diThemeOrGeoRel;       // (E<->EK)  (E<->EK)      nil       nil         E->V
	SharedUnitInterestPtr     m_uEntity;               //  E         E            P=E       V           E

	const LayerClass*    m_LayerClass;

	SharedStr m_Descr;

	LayerInfo(
		State state, 
		const AbstrDataItem* diAspectOrFeature, 
		const AbstrDataItem* diExtKey   = 0, 
		const AbstrDataItem* diThemeOrGeoRel   = 0,
		const LayerClass*    layerClass = 0
	);

	LayerInfo(WeakStr infoTxt);
	LayerInfo(WeakStr infoTxt,
		State state,
		const AbstrDataItem* diAspectOrFeature, 
		const AbstrDataItem* diClassBreaksOrExtKey, 
		const AbstrDataItem* diThemeOrGeoRel
	);

	bool IsComplete() const { return m_State >= Feature; }
	bool IsFeat  () const { return m_State == Feature || m_State == DirFeat; }
	bool IsGrid  () const { return m_State == Grid    || m_State == DirGrid; }
	bool IsDirectGrid() const { return m_State == DirGrid; }
	bool IsAspect() const { return m_State == Aspect; }
	bool IsDirectAspect() const { return IsAspect() && m_diThemeOrGeoRel && !m_diClassBreaksOrExtKey && !m_diAspectOrFeature; }
	bool IsDirect() const { return m_State == DirFeat || m_State == DirGrid; }
	const AbstrUnit* GetPaletteDomain() const;

	const AbstrUnit* GetWorldCrdUnit() const;

private:
	void InitUnits();
};

//----------------------------------------------------------------------
// section : ViewType classificaiton funcs
//----------------------------------------------------------------------

const TreeItem* GetMappingItem(const TreeItem* ti);
const AbstrDataItem* FindAspectAttr(AspectNr aNr, const TreeItem* context, const AbstrUnit* adu, const LayerClass* layerClass);

LayerInfo GetLayerInfo (const AbstrDataItem* adi);
LayerInfo GetAspectInfo(AspectNr aNr, const AbstrDataItem* adi, const LayerInfo& featureInfo, bool allowParameter);

#endif // !defined(__SHV_LAYERINFO_H)
