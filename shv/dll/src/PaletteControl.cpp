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

#include "PaletteControl.h"

#include "mci/ValueClass.h"
#include "utl/mySPrintF.h"

#include "DataItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "DataItemColumn.h"
#include "DataView.h"
#include "FeatureLayer.h"
#include "GridLayer.h"
#include "LayerClass.h"
#include "LayerControl.h"
#include "Theme.h"


//----------------------------------------------------------------------
// class  : PaletteControl
//----------------------------------------------------------------------

PaletteControl::PaletteControl(MovableObject* owner, GraphicLayer* layer, bool hideCount)
	:	base_type(owner)
	,	m_Layer(layer->shared_from_base<GraphicLayer>())
{
	HideSortOptions();
	if (hideCount)
		m_State.Set(TCF_HideCount);
	MG_DEBUGCODE( CheckState(); )

	auto activeTheme = GetActiveTheme();
	if (activeTheme) {
		m_BreakAttr = activeTheme->GetClassification();
		m_PaletteDomain = activeTheme->GetPaletteDomain(); dms_assert(m_PaletteDomain);

		m_ThemeAttr = activeTheme->GetThemeAttr();
		m_PaletteAttr = activeTheme->GetPaletteAttr();
	}
	if (!m_ThemeAttr)
		m_ThemeAttr = m_PaletteAttr;
//	dms_assert(m_ThemeAttr);
}

PaletteControl::PaletteControl(MovableObject* owner, AbstrDataItem* classAttr, const AbstrDataItem* themeAttr, const AbstrUnit* themeUnit, DataView* dv)
	:	base_type(owner)
	,	m_Layer(nullptr)
	,	m_ThemeAttr(themeAttr)
	,	m_ThemeUnit(themeUnit)
	,	m_BreakAttr(classAttr)
{
}

void PaletteControl::Init()
{
	if (!m_BreakAttr && !m_Layer)
	{
		if (m_ThemeAttr)
		{
			auto dv = GetDataView().lock();
			auto [paletteDomain, breakAttr] = CreateBreakAttr(dv.get(), m_ThemeAttr->GetAbstrValuesUnit(), m_ThemeAttr.get_ptr(), 0);
			m_BreakAttr = breakAttr.get_ptr();
			m_ThemeAttr->PrepareDataUsage(DrlType::Certain);
			CreateNonzeroJenksFisherBreakAttr(GetDataView(), m_ThemeAttr, ItemWriteLock(paletteDomain.get_ptr()), breakAttr, ItemWriteLock(breakAttr.get_ptr()));
		}
		else if (m_ThemeUnit)
			m_BreakAttr = CreateEqualIntervalBreakAttr(GetDataView(), m_ThemeUnit);
	}
	if (m_BreakAttr)
	{
		m_BreakAttr->UpdateMetaInfo();
		if (!m_PaletteDomain)
			m_PaletteDomain = m_BreakAttr->GetAbstrDomainUnit();

		dms_assert(m_PaletteDomain);
	}
	CreateColumns();
}

std::shared_ptr<Theme> PaletteControl::GetActiveTheme() const
{
	dms_assert(m_Layer);
	return m_Layer->GetActiveTheme();
}


const AbstrDataItem* PaletteControl::GetLabelTextAttr() const
{
	return m_LabelTextAttr;
}

const AbstrDataItem* PaletteControl::GetBreakAttr() const
{
	return m_BreakAttr;
}
const AbstrDataItem* PaletteControl::GetThemeAttr() const
{
	return m_ThemeAttr;
}

const AbstrUnit* PaletteControl::GetPaletteDomain() const
{
	return m_PaletteDomain;
}

void PaletteControl::CreateSymbolColumnFromLayer()
{
	auto activeTheme = GetActiveTheme();

	AspectNrSet as = m_Layer->GetLayerClass()->GetPossibleAspects();

	auto column = make_shared_gr<DataItemColumn>(this, 
		nullptr, 
		AspectNrSet(as|ASE_LabelTextColor | ASE_LabelBackColor|ASE_LabelText),
		AN_LabelTextColor //m_Layer->GetLayerClass()->GetMainAspect()
	)();
	column->SetElemSize(GPoint( COLOR_PIX_WIDTH * 2, DEF_TEXT_PIX_HEIGHT));

	as = AspectNrSet(as & column->GetPossibleAspects());

	m_LabelTextAttr = nullptr;

	for (AspectNr a = AspectNr(AN_Feature+1); 
		a !=AN_AspectCount;
		++reinterpret_cast<UInt32&>(a))
	{
		if ((1<<a) & as)
		{
			std::shared_ptr<Theme> theme = m_Layer->GetTheme(a); 

			if (!theme) {
				FeatureLayer* fLayer = dynamic_cast<FeatureLayer*>(m_Layer.get()); if (!fLayer) continue;
				DmsColor* value;
				switch (a) {
					case AN_BrushColor : value = &fLayer->GetDefaultBrushColor(); break;
					case AN_PenColor   : value = &fLayer->GetDefaultArcColor(); break;
					case AN_SymbolColor: value = &fLayer->GetDefaultPointColor(); break;
					default: continue;
				}
				if (a == AN_BrushColor)
					column->SetTheme(Theme::CreateVar<DmsColor>(AN_LabelBackColor, value, m_Layer.get()).get(), nullptr);
				else
				{
					column->SetTheme(Theme::CreateVar<DmsColor>(AN_LabelTextColor, value, m_Layer.get()).get(), nullptr);
				}
			}
			else if (theme->HasCompatibleDomain(m_PaletteDomain)) // this theme is void or active theme is attr-only domain
				if (!((1<<a) & ASE_AnyColor & ~ASE_LabelTextColor))
					if  (a == AN_LabelText)
						m_LabelTextAttr = theme->GetThemeAttr();
					else
						column->SetTheme(theme.get(), nullptr);
				else
				{
					column->SetTheme(
						Theme::Create((a == AN_BrushColor) ? AN_LabelBackColor : AN_LabelTextColor
							, theme->GetThemeAttr()
							, theme->GetClassification()
							, theme->GetPaletteAttr()
						).get()
						, nullptr
					);
				}
			else if (activeTheme->IsCompatibleTheme(theme.get()) && m_PaletteDomain->UnifyDomain(theme->GetPaletteDomain()))
			{
				if  (a == AN_LabelText)
					m_LabelTextAttr = theme->GetPaletteOrThemeAttr();
				else
				{
					column->SetTheme(
						Theme::Create(((1 << a) & ASE_AnyColor) ? AN_LabelBackColor : a
							, theme->GetPaletteOrThemeAttr()
							, nullptr
							, nullptr
						).get()
						, nullptr
					);
				}
			}
		}
	}
	if (!column->GetTheme(AN_LabelText))
	{
		SharedStr labelTxt;
		if (m_Layer->GetLayerClass() == GraphicArcLayer::GetStaticClass())
			labelTxt = CharPtr(u8"⚊ ⚊");
		else if (m_Layer->GetLayerClass() == GraphicPolygonLayer::GetStaticClass())
			labelTxt = CharPtr(u8"⚪ ⚪");
		else if (m_Layer->GetLayerClass() == GridLayer::GetStaticClass())
			labelTxt = CharPtr(u8"⬜ ⬜");
		if (!labelTxt.empty())
			column->SetTheme(Theme::CreateValue(AN_LabelText, labelTxt).get(), nullptr);
	}
//	dms_assert(!column->GetTheme(AN_LabelText));
	if (!column->GetTheme(AN_SymbolIndex))
		column->SetElemBorder(true);

	auto columnActiveTheme = column->GetActiveTheme();
	if (columnActiveTheme && !columnActiveTheme->IsAspectParameter())
	{
		dms_assert(!column->GetActiveEntity() || column->GetActiveEntity()->UnifyDomain(m_PaletteDomain) );
		// further provider logic
	}
	InsertColumn(column.get());
}

void PaletteControl::CreateSymbolColumnFromAttr()
{
	auto column = make_shared_gr<DataItemColumn>(this, 
		nullptr, 
		AspectNrSet((ASE_AllDrawAspects & ~ASE_AnyColor)|ASE_LabelTextColor),
		AN_LabelTextColor
	)();

	m_LabelTextAttr = nullptr;
	dms_assert(m_PaletteDomain);
	const TreeItem* firstContext = m_BreakAttr.has_ptr() ? m_BreakAttr.get_ptr() : m_ThemeAttr.get_ptr();
	dms_assert(firstContext);

	for (AspectNr a = AspectNr(AN_Feature+1); 
		a !=AN_AspectCount;
		++reinterpret_cast<UInt32&>(a))
	{
		if ((1<<a) & ASE_AllDrawAspects)
		{

			const AbstrDataItem* paletteData = FindAspectAttr(a, firstContext, m_PaletteDomain, 0);
			if (!paletteData)
				paletteData = FindAspectAttr(a, m_PaletteDomain, m_PaletteDomain, 0);
			if (!paletteData)
				continue;

			if  (a == AN_LabelText)
				m_LabelTextAttr = paletteData;
			else
				column->SetTheme(
					Theme::Create(((1<<a) & (ASE_SymbolColor|ASE_BrushColor)) ?	AN_LabelTextColor : a
					,	paletteData
					,	nullptr
					,	nullptr
					).get()
				,	nullptr
				);
		}
	}

	dms_assert(!column->GetTheme(AN_LabelText));
	if (column->GetActiveTheme() && !column->GetActiveTheme()->IsAspectParameter())
	{
		dms_assert(column->GetActiveEntity()->UnifyDomain(m_PaletteDomain) );
		// further provider logic
		if ( ! column->GetTheme(AN_SymbolIndex) )
			column->SetElemBorder(true);
		InsertColumn(column.get());
	}
}

void PaletteControl::CreateColorColumn()
{
	auto dv = GetDataView().lock(); if (!dv) return;
	auto column = make_shared_gr<DataItemColumn>(this, nullptr, ASE_LabelTextColor, AN_LabelTextColor)();
	column->SetTheme(
		Theme::Create(AN_LabelTextColor
			, CreateSystemColorPalette(dv.get(), m_PaletteDomain, AN_LabelTextColor, m_BreakAttr.has_ptr(), false, true, nullptr, nullptr)
		,	nullptr
		,	nullptr
		).get()
	,	nullptr
	);
}

void PaletteControl::CreateLabelTextColumn()
{
	if (m_LabelTextAttr)
		InsertColumn(make_shared_gr<DataItemColumn>(this, m_LabelTextAttr)().get() );
}

void PaletteControl::CreateColumns()
{
	try {
		CreateColumnsImpl();
	}
	catch (...)
	{	}
}

void PaletteControl::CreateColumnsImpl()
{
	auto dv = GetDataView().lock(); if (!dv) return;

	if (m_PaletteDomain)
		m_PaletteDomain->PrepareDataUsage(DrlType::CertainOrThrow);

	if (m_Layer)
		CreateSymbolColumnFromLayer();
	else
		CreateSymbolColumnFromAttr();

	//	=========================================	find additional LabelAttr
	if (!m_LabelTextAttr && m_PaletteDomain)
	{
		m_LabelTextAttr = m_PaletteDomain->GetLabelAttr();
		if (!m_LabelTextAttr && !m_PaletteDomain->IsFailed(FR_Data) && m_PaletteDomain->GetValueType()->GetSize() < 4)
			m_LabelTextAttr = CreateSystemLabelPalette(dv.get(), m_PaletteDomain, AN_LabelText);
	}
	if (m_LabelTextAttr)
		CreateLabelTextColumn();

	SharedStr exprStr = m_ThemeAttr ? m_ThemeAttr->GetFullName() : SharedStr();

	SharedPtr<const AbstrUnit> classIds = m_ThemeAttr ? GetRealAbstrValuesUnit(m_ThemeAttr) : nullptr;

	//	=========================================	add Class boundaries
	if (m_BreakAttr && (!classIds || classIds->UnifyValues(m_BreakAttr->GetAbstrValuesUnit(), UM_AllowDefaultRight)))
	{
		dms_assert(m_BreakAttr->GetAbstrDomainUnit() == m_PaletteDomain);
		if (m_ThemeAttr)
			exprStr = mySSPrintF("classify(%s, %s)", exprStr.c_str(), m_BreakAttr->GetFullName().c_str() );

		auto column = make_shared_gr<DataItemColumn>(this, m_BreakAttr)();
		InsertColumn(column.get());
		classIds = m_BreakAttr->GetAbstrDomainUnit();
	}

	//	=========================================	add Count
	// DEBUG, TMP, OPTIMIZE, GEEN COUNT VOOR LANDS DEMO
	dms_assert(classIds || ! m_ThemeAttr); // Follows from definitioin, assumed in next if.

	if	(	m_ThemeAttr 
		&&	classIds->UnifyDomain(m_PaletteDomain)
		&&	classIds->CanBeDomain()  && classIds->GetCount() <= 256
		&&	(!m_Layer || (!m_Layer->IsTopographic() && !m_Layer->HasEditAttr()))
		)
	{
		dms_assert(m_PaletteDomain);

		TreeItem* container = CreateDesktopContainer(dv->GetDesktopContext(), GetUltimateSourceItem(m_ThemeAttr.get_ptr()));
		// there could be different counts for the same ThemeAttr due to different classifications; assume one classification per class entity
		if (m_BreakAttr) 
			container = CreateContainer(container, GetUltimateSourceItem(m_BreakAttr.get_ptr()) );

		SharedPtr<AbstrDataItem> countAttr = CreateDataItem(
			container
		,	GetTokenID_mt("Count")
		,	m_PaletteDomain
		,	UnitClass::Find(m_ThemeAttr->GetAbstrDomainUnit()->GetValueType()->GetCrdClass())->CreateDefault()
		);
		countAttr->SetKeepDataState(true);
		countAttr->DisableStorage(true);
		countAttr->SetExpr( mySSPrintF("pcount(%s)", exprStr.c_str() ) );
		m_CountAttr = countAttr.get_ptr();
		InsertColumn(make_shared_gr<DataItemColumn>(this, m_CountAttr)().get());
	}
	else
		m_CountAttr = nullptr;
	if (!GetEntity())
		SetEntity( Unit<Void>::GetStaticClass()->CreateDefault() ); 
}

void PaletteControl::DoUpdateView()
{
	SetRowHeight(GetDefaultFontHeightDIP( GetDefaultFontID() ) );
	base_type::DoUpdateView();
}

static TokenID paletteCtrlID = GetTokenID_st("PaletteControl");

void PaletteControl::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	base_type::Sync(viewContext, sm);


	TreeItem* viewContext2 = const_cast<TreeItem*>(FindTreeItemByID(viewContext, paletteCtrlID));
	if (viewContext2 || sm == SM_Save)
	{
		if (!viewContext2)
			viewContext2 = viewContext->CreateItem(paletteCtrlID);
		SyncRef(m_ThemeAttr, viewContext2, GetTokenID_mt("ThemeAttr"), sm);
		SyncRef(m_BreakAttr, viewContext2, GetTokenID_mt("ClassBreaks"), sm);
		SyncRef(m_PaletteAttr, viewContext2, GetTokenID_mt("PaletteAttr"), sm);
		SyncRef(m_CountAttr, viewContext2, GetTokenID_mt("Count"), sm);
	}
	if (sm == SM_Load)
		m_PaletteDomain = GetEntity();
}

IMPL_RTTI_CLASS(PaletteControl)
