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

#include "ShvDllPch.h"

#include "dbg/DebugCast.h"
#include "dbg/DebugContext.h"
#include "mci/ValueWrap.h"

#include "AbstrUnit.h"
#include "DataArray.h"

#include "GraphicLayer.h"
#include "GraphVisitor.h"
#include "LayerClass.h"
#include "LayerControl.h"
#include "LayerInfo.h"
#include "LayerSet.h"
#include "Theme.h"
#include "ThemeSet.h"

// DEBUG, HACK
#include "GraphicRect.h" 
#include "ViewPort.h" 

//----------------------------------------------------------------------
// class  : LayerSet
//----------------------------------------------------------------------

LayerSet::LayerSet(GraphicObject* owner, CharPtr caption)
	:	base_type(owner)
	,	m_Caption(caption)
{
	m_State.Set(GOF_IgnoreActivation|SOF_DetailsVisible);
}

GraphVisitState LayerSet::InviteGraphVistor(AbstrVisitor& v)
{
	CalcWorldClientRect();
	return v.DoLayerSet(this);
}

void LayerSet::ShowPalettes(bool visible, const GraphicLayer* activeLayer)
{
	SizeT n = NrEntries();
	while (n)
	{
		GraphicObject* go = GetEntry(--n);
		if (!go)
			break;
		GraphicLayer* gl = dynamic_cast<GraphicLayer*>(go);
		if (gl)
		{
			if ((visible || activeLayer == gl) != gl->DetailsVisible())
				gl->ToggleDetailsVisibility();
		}
		else
		{
			LayerSet* ls = dynamic_cast<LayerSet*>(go);
			if (ls && ls->DetailsVisible())
				ls->ShowPalettes(visible, activeLayer);
		}
	}
}

bool LayerSet::OnCommand(ToolButtonID id)
{
	switch (id)
	{
		case TB_SP_All:   ShowPalettes(true , nullptr);          return true;
		case TB_SP_None:  ShowPalettes(false, nullptr);          return true;
		case TB_SP_Active:ShowPalettes(false, GetActiveLayer()); return true;
	}
	return base_type::OnCommand(id);
}

void LayerSet::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	ObjectContextHandle contextHandle(viewContext, "LayerSet::Sync");
	auto dv = GetDataView().lock(); if (!dv) return;

	SizeT nrLayers = NrEntries();
	base_type::Sync(viewContext, sm);
	while (nrLayers != NrEntries())
	{
		dms_assert(sm == SM_Load);
		GraphicLayer* layer = dynamic_cast<GraphicLayer*>( GetEntry(nrLayers) );
		if (layer && !layer->IsTopographic())
			layer->ConnectSelectionsTheme( dv.get() );
		++nrLayers;
	}
	SyncValue(viewContext, GetTokenID_mt("Caption"), m_Caption, sm);
}

void LayerSet::FillLcMenu(MenuData& menuData)
{
	ScalableObject::FillLcMenu(menuData);
}

//----------------------------------------------------------------------
// class  : LayerSet
//----------------------------------------------------------------------

void LayerSet::ProcessCollectionChange()
{
	base_type::ProcessCollectionChange();
	InvalidateView();
}

void LayerSet::DoUpdateView()
{
	CrdRect resRect;
	if (!HasNoExtent()) 
	{
		SizeT n = NrEntries();
		while (n)
		{
			const auto* lo = GetConstEntry(--n);
			dms_assert(lo);
			if (!lo->HasNoExtent())
			{
				const auto* gr = dynamic_cast<const GraphicRect*>(lo); // HACK, DEBUG
				if (gr)
				{
					gr->UpdateView();
					auto targetVP = const_cast<GraphicRect*>(gr)->GetTargetVP().lock();
					if (targetVP)
						resRect |= targetVP->GetCurrWorldClientRect();
				}
				else
					resRect |= lo->CalcWorldClientRect();
			}
		}
	}
	if (resRect.inverted())
		resRect = GetDefaultCrdRect();
	SetWorldClientRect( resRect );
}

TRect LayerSet::GetBorderLogicalExtents() const
{
	SizeT n = NrEntries();
	if (!n) 
		return TRect(0, 0, 0, 0);

	TRect resRect = GetConstEntry(--n)->GetBorderLogicalExtents();
	while (n)
		resRect |= GetConstEntry(--n)->GetBorderLogicalExtents();

	return resRect	+ base_type::GetBorderLogicalExtents();
}

bool LayerSet::HasDefinedExtent() const
{
	if (!base_type::HasDefinedExtent())
		return false;
	SizeT n = NrEntries();
	while (n)
		if (!GetConstEntry(--n)->HasDefinedExtent())
			return false;
	return true;
}

std::shared_ptr<LayerControlBase> LayerSet::CreateControl(MovableObject* owner)
{
	return make_shared_gr<LayerControlGroup>(owner, this)(this, m_Caption.c_str());
}

SharedStr LayerSet::GetCaption() const
{
	return m_Caption;
}


void AddFoundAspects(GraphicLayer* layer, const AbstrDataItem* viewItem, const LayerInfo& featureInfo, AspectNrSet aspectSet)
{
	static_assert(AN_Feature == 0);  // skip Feature aspect

	for (AspectNr aNr = AspectNr(1); aNr != AN_AspectCount; ++reinterpret_cast<UInt32&>(aNr) )
	{
		if ((AspectNrSet(1<<aNr) & aspectSet)==0) continue;

		LayerInfo aspectInfo = GetAspectInfo(aNr, viewItem, featureInfo, true);
		if (aspectInfo.m_State == LayerInfo::ConfigError)
			viewItem->throwItemError(aspectInfo.m_Descr.c_str());
		if (!aspectInfo.IsComplete()) continue;

		auto theme = Theme::Create(aNr,
			aspectInfo.m_diThemeOrGeoRel,
			aspectInfo.m_diClassBreaksOrExtKey,
			aspectInfo.m_diAspectOrFeature
		);
		layer->SetThemeAndActivate(theme.get(), viewItem);
	}
}

void AddIndicatedAspects(GraphicLayer* layer, const AbstrDataItem* viewItem, const LayerInfo& featureInfo)
{
	AspectNrSet aspectSet = featureInfo.m_LayerClass->GetPossibleAspects();

	AddFoundAspects(layer, viewItem, featureInfo, aspectSet);

	auto labelTextTheme = layer->GetTheme(AN_LabelText);
	bool isTextSubLayer =
		(	labelTextTheme
		&&	labelTextTheme->GetThemeAttr() == viewItem
		&&	labelTextTheme->GetClassification() == 0
		&&	labelTextTheme->GetPaletteAttr() == 0
		&&  viewItem->GetAbstrValuesUnit()->GetValueType() == ValueWrap<SharedStr>::GetStaticClass() 
		);
#if defined(MG_DEBUG)
	if (isTextSubLayer)
	{
		dms_assert(aspectSet & ASE_LabelText);
		LayerInfo aspectInfo = GetAspectInfo(AN_LabelText, viewItem, featureInfo, true);
		dms_assert(aspectInfo.IsDirectAspect() && aspectInfo.m_diThemeOrGeoRel == viewItem);
	}
#endif

	if (isTextSubLayer)
	{
		layer->DisableAspectGroup(AG_Symbol);
		layer->DisableAspectGroup(AG_Brush);
	}
	else
		layer->DisableAspectGroup(AG_Label);
	if (!layer->HasAttrTheme(ASE_Brush))
		layer->DisableAspectGroup(AG_Brush);
}

std::shared_ptr<GraphicLayer> LayerSet::CreateLayer(const AbstrDataItem* viewItem, const LayerInfo& featureInfo, DataView* dv, bool foreground)
{
	dms_assert(viewItem);
	dms_assert(featureInfo.m_LayerClass);
	dms_assert(featureInfo.m_LayerClass->IsDerivedFrom(GraphicLayer::GetStaticClass()));

	// TODO: maybe theme fits into existing Layer
	std::shared_ptr<GraphicLayer> result = std::static_pointer_cast<GraphicLayer>(featureInfo.m_LayerClass->CreateShvObj(this));
	dms_assert(result);

	if (featureInfo.IsGrid())
	{
		auto theme = Theme::Create(AN_BrushColor, viewItem, featureInfo, dv, true);
		dms_assert(theme);
		result->SetThemeAndActivate(theme.get(), viewItem);
	}
	AddIndicatedAspects(result.get(), viewItem, featureInfo);

	auto activeTheme = result->GetActiveTheme();
	if (activeTheme && activeTheme->IsAspectParameter())
		activeTheme = nullptr;

	MG_CHECK(activeTheme || ! featureInfo.IsGrid());

	if (!featureInfo.IsDirectGrid())
	{
		result->SetThemeAndActivate(
			Theme::Create(AN_Feature, 
				activeTheme ? featureInfo.m_diThemeOrGeoRel.get_ptr()       : nullptr,
				activeTheme ? featureInfo.m_diClassBreaksOrExtKey.get_ptr() : nullptr, 
				featureInfo.m_diAspectOrFeature
			).get()
		,	viewItem
		);
	}

	if (foreground)
		result->ConnectSelectionsTheme( dv );
	else
	{
		result->SetTopographic();
		dms_assert(result->DetailsVisible());
		result->ToggleDetailsVisibility(); // make palette invisible
		dms_assert(result->IsVisible());

		const TreeItem* bdvp = viewItem->GetConstSubTreeItemByID(GetTokenID_mt("BackgroundDefaultVisibility"));

		FencedInterestRetainContext allowInterestIncrease;

		if (bdvp && IsDataItem(bdvp) && AsDataItem(bdvp)->HasVoidDomainGuarantee() && !AsDataItem(bdvp)->LockAndGetValue<Bool>(0))
		{
			result->SetIsVisible(false);
			dms_assert(! result->IsVisible());
		}
	}
	// moved from AddLayerCmd::DoLayerSet
	activeTheme = result->GetActiveTheme();
	if	(	!activeTheme
		||	activeTheme->GetAspectNr() == AN_Feature
		||	activeTheme->IsAspectParameter()
		||	activeTheme->IsDisabled()
		||	result->IsDisabledAspectGroup(AspectArray[activeTheme->GetAspectNr()].aspectGroup)
		)
	{
		AspectNr an = featureInfo.m_LayerClass->GetMainAspect();
		auto mainTheme = Theme::Create(an, viewItem, featureInfo, dv, false );
		if (mainTheme)
			result->SetThemeAndActivate( mainTheme.get(), viewItem );
		result->ThemeSet::EnableAspectGroup(AspectArray[an].aspectGroup);
	}

	return result;
}


std::shared_ptr<LayerSet> LayerSet::CreateSubset(CharPtr caption)
{
	return std::make_shared<LayerSet>(this, caption);
}

const GraphicLayer* LayerSet::GetActiveLayer() const
{
	return const_cast<LayerSet*>(this)->GetActiveLayer();
}

GraphicLayer* LayerSet::GetActiveLayer()
{
	LayerSet* ls = this;
	dms_assert(ls);
	
	do {
		ScalableObject* go = ls->GetActiveEntry();
		if (!go)
			break;
		GraphicLayer* gl = dynamic_cast<GraphicLayer*>(go);
		if (gl)
			return gl;
		ls = dynamic_cast<LayerSet*>(go);
	} while (ls);

	return nullptr;
}

void LayerSet::ShuffleEntry(ScalableObject* focalEntry, ElemShuffleCmd esc)
{
	if (esc == ElemShuffleCmd::Remove)
	{
		bool wasActive = focalEntry->IsActive();
		RemoveEntry(focalEntry); 
		if (wasActive)
		{
			SizeT n = NrEntries();
			if (n)
				GetEntry(n-1)->SetActive(true);
			else
			{
				auto owner = std::dynamic_pointer_cast<LayerSet>(GetOwner().lock());
				if (owner)
					owner->ShuffleEntry(this, ElemShuffleCmd::Remove);
			}
		}
		return;
	}
	gr_elem_index pos = GetEntryPos(focalEntry);

	if (!IsDefined(pos))
		return;

	dms_assert(pos < m_Array.size());
	// note that back() is drawn last and semms to be on the front
	switch(esc) {
		case ElemShuffleCmd::ToBack:
			std::rotate(m_Array.begin(), m_Array.begin()+pos, m_Array.begin()+pos+1); 
			break;
		case ElemShuffleCmd::ToFront:
			std::rotate(m_Array.begin()+pos, m_Array.begin()+pos+1, m_Array.end()); 
			break;
		case ElemShuffleCmd::Backward:
			if (pos == 0)
				return;
			omni::swap(m_Array[pos-1],  m_Array[pos]); 
			break;
		case ElemShuffleCmd::Forward:
			if (pos >= m_Array.size()-1)
				return;
			omni::swap(m_Array[pos+1],  m_Array[pos]); 
			break;
		default:
			return;
	}
	SaveOrder();
	ProcessCollectionChange();
}

#include "LayerClass.h"

IMPL_DYNC_SHVCLASS(LayerSet,GraphicObject)



