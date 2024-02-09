// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_LAYERINFO_H)
#define __SHV_LAYERINFO_H

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

	const LayerClass*    m_LayerClass = nullptr;

	SharedStr m_Descr;

	LayerInfo(State state
	,	const AbstrDataItem* diAspectOrFeature
	,	const AbstrDataItem* diExtKey        = nullptr
	,	const AbstrDataItem* diThemeOrGeoRel = nullptr
	,	const LayerClass*    layerClass      = nullptr
	);

	LayerInfo(WeakStr infoTxt);
	LayerInfo(WeakStr infoTxt, State state
	,	const AbstrDataItem* diAspectOrFeature
	,	const AbstrDataItem* diClassBreaksOrExtKey
	,	const AbstrDataItem* diThemeOrGeoRel
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
