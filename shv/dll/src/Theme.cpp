
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

#include "Theme.h"

#include "act/ActorVisitor.h"
#include "dbg/debug.h"
#include "mci/ValueClass.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "OperationContext.ipp"
#include "Unit.h"
#include "UnitClass.h"

#include "LayerInfo.h"
#include "DataView.h"
#include "ShvUtils.h"
#include "ThemeValueGetter.h"

Float64 GetMaxValue(const Theme* theme, Float64 defaultMax)
{
	if (theme)
		defaultMax = theme->GetMaxValue();
	return defaultMax;
}

Float64 Theme::GetMaxValue() const
{
	if (!IsDefined(m_MaxValue))
	{
		const AbstrDataItem* paletteAttr = GetValueGetter()->GetPaletteAttr();

		Float64 result = 0;
		UInt32  n      = paletteAttr->GetAbstrDomainUnit()->GetCount();

		PreparedDataReadLock lock(paletteAttr);
		const AbstrDataObject* paletteObj = paletteAttr->GetRefObj();

		for (UInt32 i = 0; i!=n; ++i)
		{
			MakeMax(result, paletteObj->GetValueAsFloat64(i));
		}
		m_MaxValue = result;
	}
	return m_MaxValue;
}

//----------------------------------------------------------------------
// class  : Theme
//----------------------------------------------------------------------

Theme::Theme(AspectNr aNr, const AbstrDataItem* themeAttr, const AbstrDataItem* classBreaks, const AbstrDataItem* palette)
	:	m_AspectNr(aNr)
	,	m_ThemeAttr(themeAttr)
	,	m_Classification(classBreaks)
	,	m_PaletteAttr(palette)
	,	m_MaxValue(UNDEFINED_VALUE(Float64))
{
	if (classBreaks)
		classBreaks->SetTSF(TSF_KeepData);
	if (palette)
		palette->SetTSF(TSF_KeepData);
}

Theme::~Theme()
{}

//	delayed construction
void Theme::SetThemeAttr(const AbstrDataItem* attr)
{
	m_ThemeAttr =attr;
}

void Theme::SetClassification(const AbstrDataItem* adi)
{
	m_Classification = adi;
}

void Theme::SetPaletteAttr(const AbstrDataItem* palAttr)
{
	m_PaletteAttr = palAttr;
}

//	extend contract
const AbstrDataItem*  Theme::GetThemeAttr() const
{
	return m_ThemeAttr;
}

const AbstrDataItem*  Theme::GetActiveAttr() const
{
	return
		(GetAspectNr() == AN_Feature)
	?	m_PaletteAttr
	:	m_ThemeAttr;
}

const AbstrDataItem* Theme::GetThemeAttrSource() const
{
	dms_assert(!SuspendTrigger::DidSuspend());
	if (!m_ThemeAttr)
		return 0;
	const AbstrDataItem* result = m_ThemeAttr;
	while (true)
	{
		const AbstrDataItem* result2 = debug_cast<const AbstrDataItem*>(result->GetSourceItem());
		if (!result2)
			return result;
		result = result2;
	}
}

const AbstrDataItem* Theme::GetClassification() const
{
	return m_Classification.get_ptr();
}

const AbstrDataItem* Theme::GetPaletteAttr() const 
{ 
	return m_PaletteAttr; 
}

const AbstrDataItem* Theme::GetPaletteOrThemeAttr() const
{
	if (m_PaletteAttr)
		return m_PaletteAttr;
	if (m_Classification)
		return nullptr;
	return m_ThemeAttr;
}

const AbstrDataItem* Theme::GetThemeOrPaletteAttr() const
{
	if (m_ThemeAttr)
		return m_ThemeAttr;
	if (m_Classification)
		return nullptr;
	return m_PaletteAttr;
}

CharPtr Theme::GetAspectName() const
{
	return ::GetAspectName(m_AspectNr);
}

const AbstrUnit* Theme::GetThemeEntityUnit() const
{
	if (GetThemeAttr())
		return GetThemeAttr()->GetAbstrDomainUnit();
	if (GetClassification())
		return GetClassification()->GetAbstrValuesUnit();
	if (GetPaletteAttr())
		return GetPaletteAttr()->GetAbstrDomainUnit();
	return Unit<Void>::GetStaticClass()->CreateDefault();
}

const AbstrUnit* Theme::GetThemeValuesUnit() const
{
	if (GetThemeAttr())
		return GetThemeAttr()->GetAbstrValuesUnit();
	if (GetClassification())
		return GetClassification()->GetAbstrValuesUnit();
	if (!GetPaletteAttr())
		return nullptr;
	return GetPaletteAttr()->GetAbstrDomainUnit();
}

const AbstrUnit* Theme::GetClassIdUnit() const
{
	if (GetClassification())
		return GetClassification()->GetAbstrDomainUnit();
	if (GetPaletteAttr())
		return GetPaletteAttr()->GetAbstrDomainUnit();
	dms_assert(GetThemeAttr());
	return GetThemeAttr()->GetAbstrValuesUnit();
}

const AbstrUnit* Theme::GetPaletteValuesUnit()  const
{
	if (GetPaletteAttr())
		return GetPaletteAttr()->GetAbstrValuesUnit();
	return GetClassIdUnit();
}

const AbstrUnit* Theme::GetPaletteDomain() const
{
	if (GetPaletteAttr())
		return GetPaletteAttr()->GetAbstrDomainUnit();
	if (GetClassification())
		return GetClassification()->GetAbstrDomainUnit();
	dms_assert(GetThemeAttr());
		return GetThemeAttr()->GetAbstrDomainUnit();
//	dms_assert(GetPaletteOrThemeAttr());
//	return GetPaletteOrThemeAttr()->GetAbstrDomainUnit();
}

bool Theme::IsAspectParameter() const
{
//	dms_assert(GetAspectNr() != AN_Feature);
	const AbstrDataItem* activeAttr = GetActiveAttr();
	return activeAttr && activeAttr->HasVoidDomainGuarantee() || m_ValueGetterPtr && m_ValueGetterPtr->IsParameterGetter();
}

bool Theme::IsAspectAttr() const
{
	//	dms_assert(GetAspectNr() != AN_Feature);
	const AbstrDataItem* activeAttr = GetActiveAttr();
	return activeAttr && !activeAttr->HasVoidDomainGuarantee();
}

bool Theme::IsCompatibleTheme(const Theme* othTheme) const
{
	dms_assert(othTheme);
	return 
		m_ThemeAttr      == othTheme->m_ThemeAttr
	&&	m_Classification == othTheme->m_Classification;
}

bool Theme::HasCompatibleDomain(const AbstrUnit* entity) const
{
	dms_assert(entity);

	const AbstrUnit* themeDomain = GetThemeEntityUnit();
	if (themeDomain->IsKindOf(Unit<Void>::GetStaticClass()))
		return true;
	return entity->UnifyDomain(themeDomain);
}

Float64 Theme::GetNumericAspectValue() const
{
	dms_assert(IsAspectParameter());
	dms_assert(AspectArray[m_AspectNr].aspectType == AT_Numeric);

	dms_assert(GetThemeAttr()->GetDataRefLockCount() > 0);

	return GetThemeAttr()->GetRefObj()->GetValueAsFloat64(0);
}

Int32   Theme::GetOrdinalAspectValue() const
{
	dms_assert(IsAspectParameter());
	dms_assert(AspectArray[m_AspectNr].aspectType == AT_Ordinal);

	dms_assert(GetThemeAttr()->GetDataRefLockCount() > 0);

	return GetThemeAttr()->GetRefObj()->GetValueAsInt32(0);
}

UInt32   Theme::GetCardinalAspectValue() const
{
	dms_assert(IsAspectParameter());
	dms_assert(AspectArray[m_AspectNr].aspectType == AT_Cardinal);

	dms_assert(GetThemeAttr()->GetDataRefLockCount() > 0);

	return GetThemeAttr()->GetRefObj()->GetValueAsUInt32(0);
}


DmsColor Theme::GetColorAspectValue() const
{
	dms_assert(IsAspectParameter());
	dms_assert(AspectArray[m_AspectNr].aspectType == AT_Color);

	dms_assert(GetThemeAttr() && GetThemeAttr()->GetDataRefLockCount() > 0 || m_ValueGetterPtr && m_ValueGetterPtr->IsParameterGetter());
	if (GetThemeAttr())
	{
//		GetThemeAttr()->PrepareDataUsage(DrlType::CertainOrThrow);
//		DataReadLock lock(GetThemeAttr());
		dms_assert(GetThemeAttr()->GetDataRefLockCount() > 0);
		return GetThemeAttr()->GetValue<DmsColor>(0);
	}
	dms_assert(m_ValueGetterPtr && m_ValueGetterPtr->IsParameterGetter());
	return m_ValueGetterPtr->GetColorValue(0);
}

SharedStr Theme::GetTextAspectValue() const
{
	dms_assert(IsAspectParameter());
	dms_assert(AspectArray[m_AspectNr].aspectType == AT_Text);

	dms_assert(!GetThemeAttr() || GetThemeAttr()->GetDataRefLockCount() > 0);
	if (GetThemeAttr())
		return GetThemeAttr()->GetValue<SharedStr>(0);
	GuiReadLock dummy;
	return m_ValueGetterPtr->GetStringValue(0, dummy);
}

//	override virtuals of Actor
ActorVisitState Theme::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (visitor.Visit(m_PaletteAttr.get_ptr()   ) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
	if (visitor.Visit(m_Classification.get_ptr()) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
	if (visitor.Visit(m_ThemeAttr.get_ptr()) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
	return base_type::VisitSuppliers(svf, visitor);
}

void Theme::DoInvalidate () const
{
	base_type::DoInvalidate();

	if (m_ValueGetterPtr && m_ValueGetterPtr->MustRecalc())
		m_ValueGetterPtr.reset();

	Assign(m_MaxValue, Undefined());

	dms_assert(DoesHaveSupplInterest() || !GetInterestCount());
}

TokenID Theme::GetID() const
{
	return GetAspectNameID(m_AspectNr);
}

/* ============================= Theme::Sync =============================*/

void Theme::Sync(TreeItem* themeContext, ShvSyncMode sm)
{
	dms_assert(themeContext);

	SyncRef(m_ThemeAttr, themeContext, GetTokenID_mt("ThemeAttr"), sm);
	SyncRef(m_Classification, themeContext, GetTokenID_mt("ClassBreaks"), sm);
	SyncRef(m_PaletteAttr, themeContext, GetTokenID_mt("PaletteAttr"), sm);

	if (sm == SM_Load)
		CheckConsistency();
}

void Theme::CheckConsistency() const
{
	ObjectContextHandle och(this, GetAspectName());

	const AbstrUnit* vu = nullptr;
	if (m_ThemeAttr)
		vu = m_ThemeAttr->GetAbstrValuesUnit();
	if (m_Classification)
	{
		if (vu) 
			vu->UnifyValues(m_Classification->GetAbstrValuesUnit(), "Values of thematic attribute", "Values of class-break attribute", UnifyMode(UM_AllowDefault | UM_Throw));
		vu = m_Classification->GetAbstrDomainUnit();
	}
	if (m_PaletteAttr)
	{
		if (vu)
			vu->UnifyDomain(m_PaletteAttr->GetAbstrDomainUnit()
				, m_Classification ? "Domain of class-break attribute" : "Domain of thematic attribute"
				, "Domain of palette attribute"
				, UnifyMode(UM_AllowDefaultLeft | UM_Throw)
			);
	}
}

/* ============================= Theme::Create =============================*/

SharedDataItemInterestPtr CreatePaletteData(DataView* dv, const AbstrUnit* domain, AspectNr aNr, bool ramp, bool always, const Float64* first, const Float64* last)
{
	dms_assert(domain);
	dms_assert(!domain->IsCacheItem());
	AspectType at = AspectArray[aNr].aspectType;
	const UnitClass* uc = nullptr;
	switch (at)
	{
		case AT_Numeric:  uc = Unit<Float32>::GetStaticClass(); break;
		case AT_Ordinal:  uc = Unit<UInt16 >::GetStaticClass(); break;
		case AT_Cardinal: uc = Unit<UInt32 >::GetStaticClass(); break;
		case AT_Color:   return CreateSystemColorPalette(dv, domain, aNr, ramp, always, domain->GetValueType()->GetBitSize() == 1, first, last);
		case AT_Text:    return CreateSystemLabelPalette(dv, domain, aNr);
	}
	return nullptr;
}

std::shared_ptr<Theme> Theme::Create(AspectNr aNr, const AbstrDataItem* thematicAttr, const LayerInfo& featureLayerInfo, DataView* dv, bool doThrow)
{
	dms_assert(thematicAttr);

	LayerInfo layerInfo = GetAspectInfo(aNr, thematicAttr, featureLayerInfo, false);
	dms_assert(layerInfo.m_diThemeOrGeoRel == thematicAttr 
		|| (layerInfo.m_diThemeOrGeoRel 
				&& (layerInfo.m_diThemeOrGeoRel->GetAbstrDomainUnit()->GetUnitClass() 
						== Unit<Void>::GetStaticClass()
					|| thematicAttr
						&& layerInfo.m_diThemeOrGeoRel->GetAbstrDomainUnit()->GetCurrUltimateItem() 
							== thematicAttr->GetAbstrDomainUnit()->GetCurrUltimateItem()
					)
			)
	);
	dms_assert(dv);

	SharedDataItemInterestPtr thematicAttrHolder(thematicAttr);
	NewBreakAttrItems nbai;
	SharedUnit paletteDomain;
	std::shared_ptr<OperationContext> etc;

	if (!layerInfo.IsComplete())
	{
		paletteDomain = layerInfo.GetPaletteDomain();
		switch (layerInfo.m_State) {
			case LayerInfo::ClassificationMissing:
				nbai = CreateBreakAttr(dv, thematicAttr->GetAbstrValuesUnit(), thematicAttr, DEFAULT_MAX_NR_BREAKS);
				dms_assert(nbai.breakAttr);
				layerInfo.m_diClassBreaksOrExtKey = nbai.breakAttr.get_ptr();
				paletteDomain = nbai.paletteDomain.get_ptr();
				
				if (!thematicAttr->PrepareDataUsage(DrlType::Certain)) // async
					thematicAttr->ThrowFail();
				dms_assert(CheckCalculatingOrReady(thematicAttr->GetCurrRangeItem()));

				etc = std::make_shared<OperationContext>();

			[[fallthrough]];
			case LayerInfo::PaletteMissing:
				layerInfo.m_diAspectOrFeature = CreatePaletteData(dv, paletteDomain, aNr, layerInfo.m_diClassBreaksOrExtKey.get_ptr(), false, nullptr, nullptr);
				if (layerInfo.m_diAspectOrFeature) 
					break;

			[[fallthrough]];
			default:
				if (!doThrow)
					return std::shared_ptr<Theme>();
				thematicAttr->throwItemErrorF("Cannot create Theme with Aspect %s because\n%s",
					AspectArray[aNr].name, 
					layerInfo.m_Descr.c_str()
				);
		}
	}

	auto result = Create(aNr,
		layerInfo.m_diThemeOrGeoRel,
		layerInfo.m_diClassBreaksOrExtKey,
		layerInfo.m_diAspectOrFeature
	);
	dms_assert(result);
	if (etc)
	{
		dms_assert(nbai.breakAttr);
		dms_assert(layerInfo.m_diAspectOrFeature);
		std::weak_ptr<Theme> result_wptr = result;

		result->m_ClassTask.emplace<std::shared_ptr<OperationContext>>(etc);

		std::weak_ptr<DataView> dv_wptr = dv->shared_from_this();

		TimeStamp ts = thematicAttr->GetLastChangeTS();
		MakeMax<TimeStamp>(ts, nbai.breakAttr->GetLastChangeTS());
		MakeMax<TimeStamp>(ts, layerInfo.m_diAspectOrFeature->GetLastChangeTS());
		UpdateMarker::ChangeSourceLock changeStamp(ts, "CreateNonzeroJenksFisherBreakAttr");

//		breakAttr->GetAbstrDomainUnit()->PrepareDataUsage(DrlType::CertainOrThrow);

		FutureSuppliers emptyFutureSupplierSet;
//		etc->m_WriteLock = ItemWriteLock(breakAttr.get_ptr());
		etc->ScheduleItemWriter(MG_SOURCE_INFO_CODE("Theme::Create") nbai.breakAttr.get_ptr(),
				[dv_wptr, ts, thematicAttrHolder
					, iwlPaletteDomain = std::make_shared<ItemWriteLock>(nbai.paletteDomain.get_ptr())
					, breakAttr = nbai.breakAttr
					, result_wptr, aNr
					, etc_wptr = std::weak_ptr(etc)
					](Explain::Context* context) 
			{
				auto etc = etc_wptr.lock();
				if (!etc)
					return;
				UpdateMarker::ChangeSourceLock changeStamp(ts, "CreateNonzeroJenksFisherBreakAttr");
				CreateNonzeroJenksFisherBreakAttr(dv_wptr, thematicAttrHolder, std::move(*iwlPaletteDomain), breakAttr, std::move(etc->m_WriteLock), aNr); // async
				auto r = result_wptr.lock();
				if (r) r->m_ClassTask.Clear();
			}
		,	emptyFutureSupplierSet
		,	false
		,	nullptr
		);
	}

	return result;
}

std::shared_ptr<Theme> Theme::Create(AspectNr aNr,
	const AbstrDataItem* thematicAttr, 
	const AbstrDataItem* classification, 
	const AbstrDataItem* palette)
{
	auto newTheme = std::make_shared<Theme>(aNr, thematicAttr, classification, palette);
	newTheme->CheckConsistency();

	dms_assert(newTheme);
	return newTheme;
}

std::shared_ptr<Theme> Theme::Create(AspectNr aNr, TreeItem* themeContext)
{
	auto newTheme = std::make_shared<Theme>(aNr, nullptr, nullptr, nullptr);
	dms_assert(newTheme);
	newTheme->Sync(themeContext, SM_Load);

	return newTheme;
}

std::shared_ptr<Theme> Theme::Create(AspectNr aNr, AbstrThemeValueGetter* avg)
{
	dms_assert(avg);

	auto newTheme = std::make_shared<Theme>(aNr, nullptr, nullptr, nullptr);
	dms_assert(newTheme);

	newTheme->m_ValueGetterPtr.assign( avg );

	return newTheme;
}

template <typename V>
static std::shared_ptr<Theme> Theme::CreateValue(AspectNr aNr, V value)
{
	OwningPtr<AbstrThemeValueGetter> constValueGetter = new ConstValueGetter<V>(value);
	auto result =  Theme::Create(aNr, constValueGetter);
	constValueGetter.release();
	return result;
}

template <typename V>
static std::shared_ptr<Theme> Theme::CreateVar(AspectNr aNr, V* valuePtr, GraphicObject* go)
{
	OwningPtr<AbstrThemeValueGetter> constValueGetter = new ConstVarGetter<V>(valuePtr, go);
	auto result = Theme::Create(aNr, constValueGetter);
	constValueGetter.release();
	return result;
}

template std::shared_ptr<Theme> Theme::CreateValue(AspectNr aNr, UInt32  value);
template std::shared_ptr<Theme> Theme::CreateValue(AspectNr aNr, SharedStr  value);
template std::shared_ptr<Theme> Theme::CreateVar  (AspectNr aNr, DmsColor* valuePtr, GraphicObject* go);

std::shared_ptr<Theme> Theme::Create(Theme* src)
{
	dms_assert(src);
	return Create(src->GetAspectNr(), src->GetThemeAttr(), src->GetClassification(), src->GetPaletteAttr());
}


ActorVisitState PrepareThemeData(const AbstrDataItem* adi, const Actor* act)
{
	dms_assert(act);
	if (!adi || adi->IsPassor())
		return AVS_Ready;

	// first try to commit.
	if ((adi->SuspendibleUpdate(PS_Committed) == AVS_SuspendedOrFailed) && SuspendTrigger::DidSuspend())
		return AVS_SuspendedOrFailed;
	if (!adi->WasFailed(FR_Data))
	{

		if (adi->PrepareData())
			return AVS_Ready;
		if (SuspendTrigger::DidSuspend())
			return AVS_SuspendedOrFailed;
	}
	dms_assert(adi->WasFailed()); 
	act->Fail(adi);
	return act->WasFailed(FR_Data) ? AVS_SuspendedOrFailed : AVS_Ready;
}

ActorVisitState Theme::PrepareThemeData(const Actor* act) const
{
	if (::PrepareThemeData(m_ThemeAttr     , act) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;
	if (::PrepareThemeData(m_Classification, act) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;
	if (::PrepareThemeData(m_PaletteAttr   , act) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;

	return AVS_Ready;
}

void Theme::ResetDataLock() const
{
	if (m_ValueGetterPtr)
		m_ValueGetterPtr->ResetDataLock();
}

#include "LayerClass.h"

IMPL_RTTI_SHVCLASS(Theme)