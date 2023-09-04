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

#include "ShvDllPch.h"

#include <format>
#include <ppltasks.h>
#include "ShvUtils.h"

#include "dbg/debug.h"
#include "dbg/DebugContext.h"
#include "geo/BaseBounds.h"
#include "geo/Conversions.h"
#include "geo/Pair.h"
#include "mci/Class.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "utl/Environment.h"
#include "utl/IncrementalLock.h"
#include "utl/mySPrintF.h"
#include "utl/SplitPath.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "Projection.h"
#include "PropFuncs.h"
#include "SessionData.h"
#include "TicInterface.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "StgBase.h"

#include "GeoTypes.h"

#include "Aspect.h"
#include "CalcClassBreaks.h"
#include "DataView.h"
#include "DcHandle.h"
#include "GraphicObject.h"
#include "LayerClass.h"
#include "Theme.h"

#include "shellscalingapi.h"

//----------------------------------------------------------------------
// section : StatusText
//----------------------------------------------------------------------


void StatusTextCaller::operator() (SeverityTypeID st, CharPtr msg) const
{
	if (m_Func)
		m_Func(m_ClientHandle, st, msg);
}


//----------------------------------------------------------------------
// section : Instance
//----------------------------------------------------------------------

HINSTANCE GetInstance(HWND hWnd)
{
	return reinterpret_cast<HINSTANCE>( GetWindowLongPtr(hWnd, GWLP_HINSTANCE) );
}

//----------------------------------------------------------------------
// section : CreateViewAction
//----------------------------------------------------------------------

CreateViewActionFunc g_ViewActionFunc = nullptr;

void CreateViewAction(
	const TreeItem* tiContext,
	CharPtr         sAction,
	Int32           nCode,
	Int32           x,
	Int32           y,
	bool            doAddHistory,
	bool            isUrl,
	bool			mustOpenDetailsPage
)
{
	if (g_ViewActionFunc)
		g_ViewActionFunc(tiContext, sAction, nCode, x, y, doAddHistory, isUrl, mustOpenDetailsPage);
}

void CreateViewValueAction(const TreeItem* tiContext, SizeT index, bool mustOpenDetailsPage)
{
	if (IsDefined(index))
		CreateViewAction(tiContext, mySSPrintF("dp.vi.attr!%d", index).c_str(), -1, -1, -1, true, false, mustOpenDetailsPage);
}

void CreateGotoAction(const TreeItem* tiContext)
{
	CreateViewAction(tiContext, "goto", 0, 0, 0, false, false, false);
}

//----------------------------------------------------------------------
// section : TPoint & TRect
//----------------------------------------------------------------------

GPoint GPoint::ScreenToClient(HWND hWnd) const 
{
	GPoint result = *this;
	CheckedGdiCall( ::ScreenToClient(hWnd, &result),
		"ScreenToClient"
	);
	return result;
}		

//----------------------------------------------------------------------
#include "ser/RangeStream.h"

FormattedOutStream& operator <<(FormattedOutStream& os, const GRect& rect)
{
	return os << g2dms_order<TType>(rect);
}

FormattedOutStream& operator <<(FormattedOutStream& os, const GPoint& point)
{
	return os << g2dms_order<TType>(point);
}

FormattedOutStream& operator <<(FormattedOutStream& os, const TPoint& point)
{
	os
		<<	"{" 
		<<	point.first
		<<	", "
		<<	point.second
		<<	"}"
	;
	return os;
}

//----------------------------------------------------------------------
// section : Transform to projection
//----------------------------------------------------------------------

const AbstrUnit* GetWorldCrdUnitFromGeoUnit(const AbstrUnit* geoUnit)
{
	if (!geoUnit)
		return nullptr;

	const UnitProjection* proj = geoUnit->GetProjection();
	if (proj)
	{
		geoUnit = proj->GetCompositeBase();
		dms_assert(geoUnit);                   // projection always has a BaseUnit, guaranteed by constructors of UnitProjection!
	}
	if (geoUnit->IsCacheItem() && geoUnit->m_BackRef)
		geoUnit = AsUnit(geoUnit->m_BackRef);
	return AsUnit(geoUnit->GetUltimateSourceItem());
}

CrdTransformation GetGeoTransformation(const AbstrUnit* geoUnit)
{
	dms_assert(geoUnit);
	dms_assert(geoUnit->GetNrDimensions() == 2);

	return UnitProjection::GetCompositeTransform(geoUnit->GetProjection()); // grid to world transformer
}

grid_coord_key GetGridCoordKey(const AbstrUnit* geoUnit) 
{
	return grid_coord_key(GetGeoTransformation(geoUnit), geoUnit->GetRangeAsIRect());
}

CrdRect AsWorldExtents(const CrdRect& geoExtents, const UnitProjection* proj)
{
	if (!proj)
		return geoExtents;
	if (!IsDefined(geoExtents) || geoExtents.inverted() )
		return CrdRect();
	return UnitProjection::GetCompositeTransform(proj).Apply(geoExtents);
}

CrdRect AsWorldExtents(const CrdRect& geoExtents, const AbstrUnit* geoUnit)
{
	return AsWorldExtents(geoExtents, geoUnit->GetProjection()); 
}

//----------------------------------------------------------------------
// section : TreeItem Properties
//----------------------------------------------------------------------

SharedStr GetViewData(const TreeItem* item)         // look  at DialogData of subItem "ViewData"
{
	dms_assert(item);
	item = item->GetConstSubTreeItemByID(GetTokenID_mt("ViewData"));
	if (item)
		return TreeItem_GetDialogData(item );
	return SharedStr();
}

void SetViewData(TreeItem* item, CharPtr data) // store at DialogData of subItem "ViewData"
{
	dms_assert(item);
	TreeItem_SetDialogData(item->CreateItem(GetTokenID_mt("ViewData")), data);
}

const TreeItem* GetNextDialogDataRef(const TreeItem* item, CharPtr& i, CharPtr e)
{
	CharPtr orgI = i;
	CharPtr newI = std::find(i, e, ';');
	i = newI;
	if (newI != e) ++i;
	return item->FindItem( CharPtrRange(orgI, newI) );
}

const TreeItem* GetDialogDataRef(const TreeItem* item)
{
	dms_assert(item);

	SharedStr dd = TreeItem_GetDialogData(item);
	if (dd.empty())
		return nullptr;
	CharPtr i = dd.begin();
	const TreeItem* result = GetNextDialogDataRef(item, i, dd.send() );
	if (!result)
		item->throwItemErrorF("Cannot find DialogData reference '%s'", dd.c_str());
	return result;
}

void SetDialogDataRef(TreeItem* item, const TreeItem* ref)
{
	if (!item)
		return;
	if (ref)
		TreeItem_SetDialogData(item, ref->GetFullName());
	else
		TreeItem_SetDialogData(item, "");
}

//----------------------------------------------------------------------
// section : Sync & Save
//----------------------------------------------------------------------


template <typename A, typename T>
void SyncRefImpl(T& ptr, TreeItem* context, TokenID id, ShvSyncMode sm)
{
	dms_assert(context);
	ObjectIdContextHandle contextHandle(context, id, (sm == SM_Load) ? "LoadRef" : "SaveRef");

	const TreeItem* subItem = FindTreeItemByID(context, id);
	if (sm == SM_Load)
	{
		if (subItem)
		{
			const TreeItem* refItem = GetDialogDataRef(subItem);
			if (refItem)
			{
				ptr = checked_valcast<const A*>( refItem );
				return;
			}
		}
		ptr = nullptr;
		return;
	}

	if(sm == SM_Save && (ptr || subItem) ) 
	{
		if (!subItem)
			subItem = context->CreateItem(id);
		SetDialogDataRef(const_cast<TreeItem*>(subItem), ptr.get_ptr());
	}
}

void SyncRef(SharedPtr<const TreeItem>& ptr, TreeItem* context, TokenID id, ShvSyncMode sm) { SyncRefImpl<TreeItem>(ptr, context, id, sm); }
void SyncRef(SharedDataItemInterestPtr& ptr, TreeItem* context, TokenID id, ShvSyncMode sm) { SyncRefImpl<AbstrDataItem>(ptr, context, id, sm); }
void SyncRef(SharedPtr<const AbstrUnit>& ptr, TreeItem* context, TokenID id, ShvSyncMode sm) { SyncRefImpl<AbstrUnit>(ptr, context, id, sm); }

template <typename V>
void SaveValue(TreeItem* context, TokenID nameID, typename param_type<V>::type value)
{
	if (!context)
		return;

	SharedPtr<AbstrDataItem> adi = const_cast<AbstrDataItem*>(AsDataItem(FindTreeItemByID(context, nameID)));
	if (!adi)
		adi = CreateDataItem(
			context 
		,	nameID
		,	Unit<Void>::GetStaticClass()->CreateDefault()
		,	Unit<V   >::GetStaticClass()->CreateDefault()
		);

	adi->DisableStorage();
	adi->SetKeepDataState(true);
	adi->UpdateMetaInfo();
	::SetTheValue<V>(adi, value);
}

template <typename T>
T LoadValue(const TreeItem* context, TokenID nameID, typename param_type<T>::type defaultValue)
{
	if (context)
	{
		const TreeItem* refItem = FindTreeItemByID(context, nameID);
		if (IsDataItem(refItem))
		{
			refItem->UpdateMetaInfo();
			const AbstrDataItem* refAdi = AsDataItem(refItem);
			if (refAdi->GetDynamicObjClass()->GetValuesType() == ValueWrap<T>::GetStaticClass())
			{
				InterestRetainContextBase keepInterest;
				keepInterest.Add(refAdi);

				return refAdi->LockAndGetValue<T>(0);
			}
		}
	}
	return defaultValue;
}

template DPoint  LoadValue<DPoint >(const TreeItem* context, TokenID nameID, const DPoint& defaultValue);
template IPoint  LoadValue<IPoint >(const TreeItem* context, TokenID nameID, const IPoint  defaultValue);
template Float64 LoadValue<Float64>(const TreeItem* context, TokenID nameID, const Float64 defaultValue);

template <typename T>
void SyncValue(TreeItem* context, TokenID nameID, T& value, typename param_type<T>::type defaultValue, ShvSyncMode sm)
{
	dms_assert(context);
	if (!context) // REMOVE?
		return;

	if (sm == SM_Load)
		value = LoadValue<T>(context, nameID, defaultValue);
	else
	{	dms_assert(sm == SM_Save);
		if ((value  != defaultValue || FindTreeItemByID(context, nameID)) )
			SaveValue<T>(context, nameID, value);
	}
}
template void SyncValue<UInt32   >(TreeItem* context, TokenID nameID, UInt32& value, UInt32 defaultValue, ShvSyncMode sm);
template void SyncValue<SharedStr>(TreeItem* context, TokenID nameID, SharedStr& value, WeakStr defaultValue, ShvSyncMode sm);
template void SyncValue<SPoint   >(TreeItem* context, TokenID nameID, SPoint& value, SPoint defaultValue, ShvSyncMode sm);
template void SyncValue<Bool     >(TreeItem* context, TokenID nameID, Bool  & value, Bool   defaultValue, ShvSyncMode sm);


void SyncValue(TreeItem* context, TokenID nameID, SharedStr& value, ShvSyncMode sm)
{
	if (!context)
		return;

	const TreeItem* refItem = FindTreeItemByID(context, nameID);
	if (sm == SM_Load)
	{
		if (refItem)
		{
			InterestRetainContextBase keepInterest;
			keepInterest.Add(refItem);

			value = AsDataItem(refItem)->LockAndGetValue<SharedStr>(0).c_str();
		}
		else
			value = SharedStr();
	}
	else if (sm == SM_Save && (!value.empty() || refItem) )
		SaveValue<SharedStr>(context, nameID, value);
}


void SyncState(GraphicObject* obj, TreeItem* context, TokenID stateID, UInt32 state, bool defaultValue, ShvSyncMode sm)
{
	dms_assert(obj);
	dms_assert(context);
	const TreeItem* refItem = FindTreeItemByID(context, stateID);
	if (refItem && sm == SM_Load)
	{
		InterestRetainContextBase keepInterest;
		keepInterest.Add(refItem);

		obj->m_State.Set(state, AsDataItem(refItem)->LockAndGetValue<Bool>(0));
	}
	else if (sm == SM_Save && (obj->m_State.Get(state) != defaultValue || refItem) )
//		SaveState(obj, context, stateID, state);
		SaveValue<Bool>(context, stateID, obj->m_State.Get(state));
}

void ChangePoint(AbstrDataItem* pointItem, const CrdPoint& point, bool isNew)
{
	if (!pointItem)
		return;

	dms_assert(pointItem->GetAbstrDomainUnit()->GetCount() == 1);

	StaticMtIncrementalLock<TreeItem::s_NotifyChangeLockCount> dontNotify;

	pointItem->UpdateMetaInfo();

	if (!isNew && pointItem->HasDataObj())
	{
		auto ado = pointItem->GetDataObj();
		if (ado->GetValueAsDPoint(0) == point)
			return;
	}
	DataWriteLock dataHolder(pointItem);
	MG_CHECK(dataHolder->GetTiledRangeData()->GetRangeSize() == 1);
	dms_assert(pointItem->m_DataLockCount < 0);

	dataHolder->SetValueAsDPoint(0, point);
	dataHolder.Commit();
}

//----------------------------------------------------------------------
// section : ViewContext
//----------------------------------------------------------------------

TokenID UniqueName(TreeItem* context, CharPtr nameBase)
{
	assert(context);
	UInt32 i = 0;
	while (true) {
		SharedStr nameStr = mySSPrintF("%s%d", nameBase, i++);
		TokenID result = GetTokenID_mt(nameStr.c_str());
		if (!context->GetConstSubTreeItemByID(result))
			return result;
	}
}

TokenID UniqueName(TreeItem* context, TokenID nameBaseID)
{
	assert(context);
	TokenID result = nameBaseID;
	if (context->GetConstSubTreeItemByID(nameBaseID))
	{
		UInt32 i = 0;
		do {
			SharedStr nameStr = mySSPrintF("%s%d", nameBaseID, ++i);
			result = GetTokenID_mt(nameStr.c_str());
		} while (context->GetConstSubTreeItemByID(result));
	}
	return result;
}

TokenID UniqueName(TreeItem* context, const Class* cls)
{
	return UniqueName(context, cls->GetID());
}	

TokenID CopyName(TreeItem* context, TokenID orgNameID)
{
	if (!context->GetConstSubTreeItemByID(orgNameID)) 
		return orgNameID;
	SharedStr orgName =  SharedStr(orgNameID);
	while (orgName.ssize() && isdigit(UChar(orgName.sback())))
		orgName.GetAsMutableCharArray()->erase(orgName.ssize()-1);
	if (orgName.ssize() > 4 && substr(orgName, orgName.ssize()-4, 4) == "Copy")
		orgName = substr(orgName, 0, orgName.ssize()-4);

	return UniqueName(context, mySSPrintF("%sCopy", orgName.c_str()).c_str());
}

std::shared_ptr<GraphicObject> CreateFromContext(TreeItem* context, GraphicObject* owner)
{
	TokenID className = TreeItem_GetDialogType(context);
	if (className.empty())
		return 0;

	ObjectContextHandle contextHandle(context, "CreateFromContext");

	const Class* cls = Class::Find(className);
	if (cls && cls->IsDerivedFrom(GraphicObject::GetStaticClass()))
	{
		const ShvClass* shvCls = debug_cast<const ShvClass*>(cls);
		std::shared_ptr<GraphicObject> result = shvCls->CreateShvObj(owner);
		if (result)
			result->Sync(context, SM_Load);
		return result;
	}
	return std::shared_ptr<GraphicObject>();
}

//----------------------------------------------------------------------
// section : CheckedGdiCall
//----------------------------------------------------------------------

void CheckedGdiCall(bool result, CharPtr context)
{
	if (result)
		return;
	DWORD lastErr = GetLastError();
	if (lastErr)
		throwSystemError(lastErr, context);
}

//----------------------------------------------------------------------
// section : Colors
//----------------------------------------------------------------------

COLORREF GetFocusClr() { return ::GetSysColor(COLOR_HIGHLIGHT); }
COLORREF GetDefaultClr(UInt32 i) { return DmsColor2COLORREF(STG_Bmp_GetDefaultColor(i)); }
COLORREF GetSelectedClr(SelectionID i) { return GetDefaultClr(i); }

//----------------------------------------------------------------------
// section : DrawBorder
//----------------------------------------------------------------------

void FillRectWithBrush(HDC dc, const GRect& rect, HBRUSH br)
{
	GdiHandle<HPEN>           invisiblePen(CreatePen(PS_NULL, 0, RGB(0, 0, 0)));
	GdiObjectSelector<HPEN>   penSelector(dc, invisiblePen);

	GdiObjectSelector<HBRUSH> brushSelector(dc, br);
	Rectangle(dc, rect.left, rect.top, rect.right+1, rect.bottom+1);
}

void ShadowRect(HDC dc, GRect rect, HBRUSH lightBrush, HBRUSH darkBrush)
{
	if (rect.top >= rect.bottom || rect.left >= rect.right)
		return;

	Int32 nextLeft   = rect.left+1;
	Int32 nextTop    = rect.top+1;
	Int32 prevRight  = rect.right - 1;
	Int32 prevBottom = rect.bottom - 1;

	FillRectWithBrush(dc, GRect(prevRight, rect.top,   rect.right, rect.bottom), darkBrush );  // right vertical line
	FillRectWithBrush(dc, GRect(rect.left, prevBottom, prevRight,  rect.bottom), darkBrush );  // bottom horizontal line

	if (rect.top >= prevBottom || rect.left >= prevRight)
		return;

	FillRectWithBrush(dc, GRect(rect.left, rect.top, prevRight,  nextTop ), lightBrush );  // top horizontal line
	FillRectWithBrush(dc, GRect(rect.left, nextTop,  nextLeft, prevBottom), lightBrush );  // left vertical line
}

void DrawButtonBorder(HDC dc, GRect& clientDeviceRect)
{
	HBRUSH lightBrush = GetSysColorBrush(COLOR_3DLIGHT);
	HBRUSH blackBrush = GetSysColorBrush(COLOR_3DDKSHADOW);

	ShadowRect(dc, clientDeviceRect, lightBrush, blackBrush);
	clientDeviceRect.Shrink(1);

	HBRUSH whiteBrush = GetSysColorBrush(COLOR_3DHIGHLIGHT);
	HBRUSH shadowBrush= GetSysColorBrush(COLOR_3DSHADOW);

	ShadowRect(dc, clientDeviceRect, whiteBrush, shadowBrush);
	clientDeviceRect.Shrink(1);
}

void DrawReversedBorder(HDC dc, GRect& clientDeviceRect)
{
	HBRUSH lightBrush = GetSysColorBrush(COLOR_3DLIGHT);
	HBRUSH blackBrush = GetSysColorBrush(COLOR_3DDKSHADOW);

	ShadowRect(dc, clientDeviceRect, blackBrush, lightBrush);
	clientDeviceRect.Shrink(1);

	HBRUSH whiteBrush = GetSysColorBrush(COLOR_3DHIGHLIGHT);
	HBRUSH shadowBrush= GetSysColorBrush(COLOR_3DSHADOW);

	ShadowRect(dc, clientDeviceRect, shadowBrush, whiteBrush);
	clientDeviceRect.Shrink(1);
}

void DrawRectDmsColor(HDC dc, const GRect& rect, DmsColor color)
{
	GdiHandle<HBRUSH> brush(
		CreateSolidBrush( DmsColor2COLORREF( color ) ),
		"DrawRectDmsColor::CreateSolidBrush"
	);

	ShadowRect(dc, rect, brush, brush);
}

void FillRectDmsColor(HDC dc, const GRect& rect, DmsColor color)
{
	GdiHandle<HBRUSH> brushHandle(
		CreateSolidBrush( DmsColor2COLORREF( color ) ),
		"FillRectDmsColor::CreateSolidBrush"
	);

	FillRect(dc, &rect, brushHandle );
}


//----------------------------------------------------------------------
// enum class FontSizeCategory
//----------------------------------------------------------------------

static const UInt32 g_DefaultFontHDIP[static_cast<int>(FontSizeCategory::COUNT)] = { 12, 16, 20 };
static CharPtr      g_DefaultFontName[static_cast<int>(FontSizeCategory::COUNT)] = { "Small", "Medium", "Large" };

CharPtr GetDefaultFontName(FontSizeCategory fid)
{
	assert(fid >= FontSizeCategory::SMALL && fid <= FontSizeCategory::COUNT);
	if (fid < FontSizeCategory::SMALL || fid > FontSizeCategory::COUNT)
		fid = FontSizeCategory::SMALL;

	return g_DefaultFontName[static_cast<int>(fid)];
}

UInt32 GetDefaultFontHeightDIP(FontSizeCategory fid)
{
	assert(fid >= FontSizeCategory::SMALL && fid <= FontSizeCategory::COUNT);
	if (fid < FontSizeCategory::SMALL || fid > FontSizeCategory::COUNT)
		fid = FontSizeCategory::SMALL;

	return g_DefaultFontHDIP[static_cast<int>(fid)];
}

Point<UINT> GetWindowEffectiveDPI(HWND hWnd)
{
	assert(hWnd);
	HWND hTopWnd = GetAncestor(hWnd, GA_ROOT);
	assert(hTopWnd);
	HMONITOR hMonitor = MonitorFromWindow(hTopWnd, MONITOR_DEFAULTTONEAREST);
	assert(hMonitor);
	UINT dpiX, dpiY;

	auto result = GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
	assert(result == S_OK);
	return shp2dms_order<UINT>( dpiX, dpiY );
}

Float64 GetWindowDip2PixFactorX(HWND hWnd)
{
	auto dpi = GetWindowEffectiveDPI(hWnd);
	return dpi.X() / 96.0;
}

Float64 GetWindowDip2PixFactorY(HWND hWnd)
{
	auto dpi = GetWindowEffectiveDPI(hWnd);
	return dpi.Y() / 96.0;
}

Point<Float64> GetWindowDip2PixFactors(HWND hWnd)
{
	auto dpi = GetWindowEffectiveDPI(hWnd);
	return { dpi.first / 96.0, dpi.second / 96.0 };
}

Float64 GetWindowDip2PixFactor(HWND hWnd)
{
	auto dpi = GetWindowEffectiveDPI(hWnd);
	return (dpi.first + dpi.second) / (2.0*96.0);
}

Point<Float64> GetWindowPix2DipFactors(HWND hWnd)
{
	auto dpi = GetWindowEffectiveDPI(hWnd);
	return shp2dms_order<Float64>(96.0 / dpi.first, 96.0 / dpi.second);
}

//----------------------------------------------------------------------
// desktop data section
//----------------------------------------------------------------------

inline TreeItem* SafeCreateItem(TreeItem* context, TokenID pathID)
{
	MG_DEBUGCODE(StaticMtIncrementalLock<gd_TokenCreationBlockCount> tokenCreationLock; )
		auto result = context->CreateItem(pathID);
	dms_assert(result);
	return result;
}

inline TreeItem* SafeCreateItemFromPath(TreeItem* context, CharPtr path)
{
	MG_DEBUGCODE(StaticMtIncrementalLock<gd_TokenCreationBlockCount> tokenCreationLock; )
	auto result = context->CreateItemFromPath(path);
	dms_assert(result);
	return result;
}

static TokenID desktopsID = GetTokenID_st("Desktops");
static TokenID defaultID  = GetTokenID_st("Default");
static TokenID viewDataID = GetTokenID_st("ViewData");
static TokenID exportsID  = GetTokenID_st("Exports");

TreeItem* GetDefaultDesktopContainer(const TreeItem* ti)
{
	assert(ti);
	assert(!ti->IsCacheItem());
	const TreeItem* pi = nullptr;
	while (pi = ti->GetTreeParent())
		ti = pi;
	auto desktops = const_cast<TreeItem*>(ti)->CreateItem(desktopsID);
	return desktops->CreateItem(defaultID);
}

TreeItem* GetExportsContainer(TreeItem* desktopItem)
{
	assert(desktopItem && !desktopItem->IsCacheItem());
	auto result = desktopItem->CreateItem(exportsID);
	assert(result && !result->IsCacheItem());
	return result;
}

TreeItem* GetViewDataContainer(TreeItem* desktopItem)
{
	assert(desktopItem && !desktopItem->IsCacheItem());
	auto result = desktopItem->CreateItem(viewDataID);
	assert(result && !result->IsCacheItem());
	return result;
}

TreeItem* CreateContainer_impl(TreeItem* container, const TreeItem* item)
{
	dms_assert(item);
	if (item->IsCacheItem())
	{
		item = item->GetUltimateItem();
		dms_assert(item);
		auto name = std::format("I{:x}", std::size_t(item));
		return container->CreateItem(GetTokenID(name.c_str()));
	}

	if (IsUnit(item) && AsUnit(item)->IsDefaultUnit())
	{
		TreeItem* result = SafeCreateItem(container, AsUnit(item)->GetValueType()->GetID());
		dms_assert(result);
		return result;
	}

	if (container->DoesContain(item))
		return const_cast<TreeItem*>(item);
	TreeItem* result = SafeCreateItemFromPath(container, item->GetRelativeName(SessionData::Curr()->GetConfigRoot()).c_str());
	dms_assert(result);
	return result;
}

TreeItem* CreateContainer(TreeItem* container, const TreeItem* item)
{
	auto result = CreateContainer_impl(container, item);
	result->UpdateMetaInfo();
	return result;
}

TreeItem* CreateDesktopContainer(TreeItem* desktopItem, const TreeItem* item)
{
	return CreateContainer(GetViewDataContainer(desktopItem), item);
}

static TokenID paletteDomainID = GetTokenID_st("PaletteDomain");

SharedMutableUnitInterestPtr CreatePaletteDomain(TreeItem* themeContainer, SizeT n)
{
	SharedMutableUnit paletteDomain = Unit<UInt8>::GetStaticClass()->CreateUnit(themeContainer, paletteDomainID);
	ItemWriteLock  xx(paletteDomain.get_ptr());
	if (!paletteDomain->GetTSF(USF_HasConfigRange))
	{
		paletteDomain->SetTSF(USF_HasConfigRange);
		paletteDomain->SetCount(n);
	}
	return paletteDomain;
}

UInt32 InterpolateColor(UInt32 firstColor, UInt32 lastColor, SizeT n, SizeT i)
{
	return CombineRGB(
		InterpolateValue<UInt8>(GetRed  (firstColor), GetRed  (lastColor), n, i),
		InterpolateValue<UInt8>(GetGreen(firstColor), GetGreen(lastColor), n, i),
		InterpolateValue<UInt8>(GetBlue (firstColor), GetBlue (lastColor), n, i)
	);
}

TreeItem* CreatePaletteContainer(DataView* dv, const AbstrUnit* paletteDomain)
{
	if (!paletteDomain->IsEndogenous())
		return const_cast<AbstrUnit*>(paletteDomain);

	return CreateDesktopContainer(dv->GetDesktopContext(), paletteDomain);
}

SharedDataItemInterestPtr CreateColorPalette(DataView* dv, const AbstrUnit* paletteDomain, AspectNr aNr, DmsColor clr)
{
	TreeItem* paletteContainer = CreatePaletteContainer(dv, paletteDomain);
	TokenID name = GetAspectNameID(aNr);
	SharedMutableDataItem result = AsDynamicDataItem(paletteContainer->GetSubTreeItemByID(name));
	if (!result)
		result = CreateDataItem(paletteContainer, name, paletteDomain, Unit<UInt32>::GetStaticClass()->CreateDefault());
	dms_assert(result);

	TreeItem_SetDialogType(result, GetAspectNameID(aNr));

	result->DisableStorage();
	result->UpdateMetaInfo();

	paletteDomain->PrepareDataUsage(DrlType::Certain);
	auto n = paletteDomain->GetCount();
	DataWriteLock lock(result);
	for (row_id i = 0; i != n; ++i)
		lock->SetValue<UInt32>(i, clr);
	lock.Commit();
	dms_assert(result->HasConfigData());

	return result.get_ptr();
}
SharedDataItemInterestPtr CreateSystemColorPalette(DataView* dv, const AbstrUnit* paletteDomain, AspectNr aNr, bool ramp, bool always, bool unique, const Float64* first, const Float64* last)
{
	bool hasZero = false;
	bool bothSigns = false;
	SizeT nrNegative = 0, nrBreaks = last - first;
	if (nrBreaks)
	{
		dms_assert(ramp);
		bothSigns = *first * last[-1] <= 0;
		if (bothSigns)
		{
			while (nrNegative < nrBreaks && first[nrNegative] < 0)
				++nrNegative;
			if (nrNegative < nrBreaks && first[nrNegative] == 0)
				hasZero = true;
		}
	}
	TreeItem* paletteContainer = CreatePaletteContainer(dv, paletteDomain);
	SharedMutableDataItem result = AsDynamicDataItem(paletteContainer->GetSubTreeItemByID(GetAspectNameID(aNr)));
	TokenID name = GetAspectNameID(aNr);
	if (always || !result)
	{
		if (unique)
		{
			auto uniqueNameStr = SharedStr(name);
			name = UniqueName(paletteContainer, uniqueNameStr.c_str());
		}
		result = CreateDataItem(paletteContainer, name, paletteDomain, Unit<UInt32>::GetStaticClass()->CreateDefault() );
		TreeItem_SetDialogType(result, GetAspectNameID(aNr) );

		result->DisableStorage();
		result->UpdateMetaInfo();
		paletteDomain->PrepareDataUsage(DrlType::Certain);
		SizeT n = paletteDomain->GetCount();
		DataWriteLock lock(result);
		if (n == 2)
		{
			lock->SetValue<UInt32>(0, DmsWhite);
			lock->SetValue<UInt32>(1, unique ? dv->GetNextDmsColor() : DmsRed);
		}
		else if (ramp)
		{
			DmsColor firstColor = STG_Bmp_GetDefaultColor(CI_RAMPSTART);
			DmsColor lastColor  = STG_Bmp_GetDefaultColor(CI_RAMPEND);
			if (bothSigns)
			{
				DmsColor white = STG_Bmp_GetDefaultColor(CI_BACKGROUND);
				SizeT i = 0;
				for (; i != nrNegative; ++i)
					lock->SetValue<UInt32>(i, InterpolateColor(firstColor, white, nrNegative, i));
				if (hasZero)
					lock->SetValue<UInt32>(i++, white);
				SizeT nrNonPositive = i;
				SizeT nrPositive = n - nrNonPositive;
				for (; i != n; ++i)
					lock->SetValue<UInt32>(i, InterpolateColor(white, lastColor, nrPositive, i - nrNonPositive + 1));
			}
			else
			{
				SizeT denominator = (n > 1) ? n - 1 : 1;
				for (SizeT i = 0; i != n; ++i)
					lock->SetValue<UInt32>(i, InterpolateColor(firstColor, lastColor, denominator, i));
			}
		}
		else 
		{
			for (SizeT i = 0; i != n; ++i)
				lock->SetValue<UInt32>(i, STG_Bmp_GetDefaultColor(i % CI_NRCOLORS) );
		}
		lock.Commit();
		dms_assert(result->HasConfigData());
	}
	return result.get_ptr();
}

SharedDataItemInterestPtr CreateSystemLabelPalette(DataView* dv, const AbstrUnit* paletteDomain, AspectNr aNr)
{
	dms_assert(!paletteDomain->WasFailed(FR_Data));
	TreeItem* paletteContainer = CreatePaletteContainer(dv, paletteDomain);
	SharedDataItemInterestPtr result = AsDynamicDataItem( paletteContainer->GetSubTreeItemByID(GetAspectNameID(aNr)) );
	if (!result)
	{
		SizeT n = paletteDomain->GetPreparedCount();
		SharedMutableDataItem newResult = CreateDataItem(paletteContainer, GetAspectNameID(aNr), paletteDomain, Unit<SharedStr>::GetStaticClass()->CreateDefault() );
		TreeItem_SetDialogType(newResult, GetAspectNameID(aNr) );

		newResult->DisableStorage();
//			newResult->SetConfigData();
		newResult->UpdateMetaInfo();
		result = newResult.get_ptr();
		DataWriteLock lock(newResult);

		auto resultData = mutable_array_cast<SharedStr>(lock)->GetDataWrite();
		for (SizeT i = 0; i != n; ++i)
			resultData[i] = AsString(i);

		lock.Commit();
	}
	return result;
}

NewBreakAttrItems CreateBreakAttr(DataView* dv, const AbstrUnit* thematicUnit, const TreeItem* themeIndicator, SizeT n)
{
	TreeItem* themeContainer = CreateDesktopContainer(dv->GetDesktopContext(), GetUltimateSourceItem(themeIndicator));
	NewBreakAttrItems result;
	result.paletteDomain = CreatePaletteDomain(themeContainer, n);
	result.breakAttr = CreateDataItem(
		result.paletteDomain.get_ptr()
	, GetTokenID_mt("ClassBreaks")
	, result.paletteDomain
	,	thematicUnit
	);

	MakeClassBreakAttr(result.breakAttr);
	return result;
}

SharedDataItemInterestPtr CreateEqualIntervalBreakAttr(std::weak_ptr<DataView> dv_wptr, const AbstrUnit* themeUnit)
{
	auto dv = dv_wptr.lock(); if (!dv) return nullptr;

	auto [paletteDomain, breakAttr] = CreateBreakAttr(dv.get(), themeUnit, themeUnit, DEFAULT_MAX_NR_BREAKS);

	Range<Float64> range = themeUnit->GetRangeAsFloat64();
	MakeRange(range.first, range.second);
	ValueCountPairContainer sortedUniqueValueCache;
	sortedUniqueValueCache.reserve(2);
	sortedUniqueValueCache.push_back(ValueCountPair(range.first,  1) );
	sortedUniqueValueCache.push_back(ValueCountPair(range.second, 1) );
	sortedUniqueValueCache.m_Total = 2;

	ClassifyEqualInterval(breakAttr, sortedUniqueValueCache, themeUnit->GetTiledRangeData());

	return breakAttr.get_ptr();
}

SharedDataItemInterestPtr CreateEqualCountBreakAttr(DataView* dv, const AbstrDataItem* thematicAttr)
{
	SizeT count = thematicAttr->GetAbstrDomainUnit()->GetCount();

	CountsResultType sortedUniqueValueCache;
	if (count)
		sortedUniqueValueCache = PrepareCounts(thematicAttr, MAX_PAIR_COUNT);

	auto [paletteDomain, breakAttr] = CreateBreakAttr(dv, thematicAttr->GetAbstrValuesUnit(), thematicAttr, Min<SizeT>(sortedUniqueValueCache.first.size(), DEFAULT_MAX_NR_BREAKS));


	if (sortedUniqueValueCache.first.m_Total)
		ClassifyEqualCount(breakAttr, sortedUniqueValueCache.first, sortedUniqueValueCache.second);

	return breakAttr.get_ptr();
}

void CreateNonzeroJenksFisherBreakAttr(std::weak_ptr<DataView> dv_wptr, const AbstrDataItem* thematicAttr, ItemWriteLock&& iwlPaletteDomain, AbstrDataItem* breakAttr, ItemWriteLock&& iwlBreakAttr, AspectNr aNr)
{
	dms_assert(thematicAttr);
	SizeT count = thematicAttr->GetAbstrDomainUnit()->GetCount();

	CountsResultType sortedUniqueValueCache;
	SharedPtr<const SharedObj> thematicValuesRangeData;
	if (count)
	{
		ItemReadLock readLock(thematicAttr->GetCurrRangeItem());
		DataReadLock lck(thematicAttr);
		sortedUniqueValueCache = GetCounts(thematicAttr, MAX_PAIR_COUNT);
		thematicValuesRangeData = sortedUniqueValueCache.second;
	}

	SharedPtr<AbstrUnit> paletteDomain = const_cast<AbstrUnit*>(breakAttr->GetAbstrDomainUnit());
	SharedPtr<AbstrDataItem> breakAttrPtr = breakAttr;

	SizeT nrBreaks = Min<SizeT>(sortedUniqueValueCache.first.size(), DEFAULT_MAX_NR_BREAKS);
	auto result = ClassifyJenksFisher(sortedUniqueValueCache.first, nrBreaks, true); // callsClassifyUniqueValues if breakAttr.size() >= sortedUniqueValueCache.size()

	auto dv = dv_wptr.lock(); if (!dv) return;

	TimeStamp tsActive = UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("Obtaining active frame for JenksFisher job"));

	auto siwlPaletteDomain = std::make_shared<ItemWriteLock>(std::move(iwlPaletteDomain));
	auto siwlBreakAttr = std::make_shared<ItemWriteLock>(std::move(iwlBreakAttr)); // TODO G8: Can this be moved into a functor's data field directly? Requires no functor copy!
	dv->AddGuiOper([tsActive, paletteDomain
			, siwlMovedPaletteDomain = std::move(siwlPaletteDomain)
			, breakAttrPtr, siwlBreakAttr, nrBreaks
			, resultCopy = std::move(result)
			, thematicValuesRangeData, aNr, dv_wptr
			]() 
		{
			UpdateMarker::ChangeSourceLock tsLock(tsActive, "JenksFisher application");
			paletteDomain->SetCount(nrBreaks);

			// Alleviate restriction on breakAttr write-access to avoid dead-lock, which requires mutable=synchronized=unique access to the ItemWriteLock
			// Could the move fix the dangling writeLock on PaletteDomain issue ? No, since the spawning thread doesn't write, except for when the destructor could run, which has unique access, guaranteed by shared_ptr.
			*siwlMovedPaletteDomain = ItemWriteLock(); 
			auto paletteInterest = SharedTreeItemInterestPtr(paletteDomain.get());
			auto tryReadLock = ItemReadLock(std::move(paletteInterest), try_token);
			if (!tryReadLock.has_ptr())
				return; // no accces because of other classifying action, pray for the other action to fill this palette

			breakAttrPtr->MarkTS(tsActive);

			FillBreakAttrFromArray(breakAttrPtr, resultCopy, thematicValuesRangeData);
			if (aNr != AN_AspectCount)
			{
				auto dv = dv_wptr.lock(); if (!dv) return;
				CreatePaletteData(dv.get(), paletteDomain, aNr, true, true, begin_ptr( resultCopy ), end_ptr( resultCopy ));
			}
		}
	);
}

const AbstrDataItem* GetSystemPalette(const AbstrUnit* paletteDomain, AspectNr aNr)
{
	dms_assert(paletteDomain);
	return  AsDynamicDataItem( paletteDomain->GetConstSubTreeItemByID(GetAspectNameID(aNr)) );
}

//----------------------------------------------------------------------
// config section
//----------------------------------------------------------------------

MG_DEBUGCODE( static bool gd_AdminModeKnown = false; )
static bool g_AdminMode;
static bool g_BusyMode;

bool HasAdminMode()
{
	dbg_assert(gd_AdminModeKnown);
	return g_AdminMode;
}

bool IsBusy()
{
	return g_BusyMode;
}

extern "C" SHV_CALL void DMS_CONV SHV_SetAdminMode(bool v) 
{
	MG_DEBUGCODE( gd_AdminModeKnown = true; )
	g_AdminMode = v;
}

extern "C" SHV_CALL void DMS_CONV SHV_SetBusyMode(bool v) 
{
	g_BusyMode = v;
}

//----------------------------------------------------------------------
// DataContainer section
//----------------------------------------------------------------------

const AbstrUnit* SHV_DataContainer_GetDomain(const TreeItem* ti, UInt32 level, bool adminMode)
{
	if (!ti || !adminMode && ti->GetTSF(TSF_InHidden)) return nullptr;

	if (IsDataItem(ti))
	{
		try { 
			return AsDataItem(ti)->GetAbstrDomainUnit();
		} 
		catch (...) {}
		return nullptr;
	}

	if (level) 
	{
		--level;

		for (ti = ti->GetFirstSubItem(); ti; ti = ti->GetNextItem()) 
		{
			const AbstrUnit* result = SHV_DataContainer_GetDomain(ti, level, adminMode);
			if (result)
				return result;
		}
	}
	return nullptr;
}

UInt32 SHV_DataContainer_GetItemCount(const TreeItem* ti, const AbstrUnit* domain, UInt32 level, bool adminMode)
{
	assert(domain);
	if (!ti || !adminMode && ti->GetTSF(TSF_InHidden)) return 0;

	UInt32 result =0;

	domain->UpdateMetaInfo();
	ti->UpdateMetaInfo();

	if (IsDataItem(ti) && AsDataItem(ti)->GetAbstrDomainUnit()->UnifyDomain(domain))
		++result;

	if (!level) return result; --level;

	for (ti = ti->GetFirstSubItem(); ti; ti = ti->GetNextItem()) 
		result += SHV_DataContainer_GetItemCount(ti, domain, level, adminMode);

	return result;
}

auto DataContainer_NextItem(const TreeItem* ti, const TreeItem* si, const AbstrUnit* domain, bool adminMode) -> const AbstrDataItem*
{
	assert(ti);
	while (si = ti->WalkConstSubTree(si))
	{
		// skip hidden items
		if (!adminMode)
			while (si->GetTSF(TSF_InHidden))
			{
				if (si == ti)
					return nullptr;
				const TreeItem* next;
				while ((next = si->GetNextItem()) == nullptr) // skip sub-tree
				{
					si = si->GetTreeParent();
					if (si == ti)
						return nullptr;
					assert(si);
				}
				si = next;
			}

		// return dataItem if compatible
		assert(si);
		if (IsDataItem(si))
		{
			auto adi = AsDataItem(si);
			if (adi->GetAbstrDomainUnit()->UnifyDomain(domain))
				return adi;
		}
	}
	return nullptr;
}

const AbstrDataItem* SHV_DataContainer_GetItem(const TreeItem* ti, const AbstrUnit* domain, UInt32 k, UInt32 level, bool adminMode)
{
	assert(domain);
	if (!ti || !adminMode && ti->GetTSF(TSF_InHidden)) return 0;

	UInt32 result =0;
	if (IsDataItem(ti) && AsDataItem(ti)->GetAbstrDomainUnit()->UnifyDomain(domain))
	{
		if (!k)
			return AsDataItem(ti);
		--k;
	}

	if (!level) return 0; --level;

	for (ti = ti->GetFirstSubItem(); ti; ti = ti->GetNextItem()) 
	{
		UInt32 n = SHV_DataContainer_GetItemCount(ti, domain, level, adminMode);
		if (k < n)
			return SHV_DataContainer_GetItem(ti, domain, k, level, adminMode);
		k -= n;
	}
	return nullptr;
}

//----------------------------------------------------------------------
// UpdateShowSelOnly section
//----------------------------------------------------------------------
#include "Theme.h"

static TokenID selID = GetTokenID_st("SelID");
static TokenID selSetID = GetTokenID_st("SelSet");

void UpdateShowSelOnlyImpl(
	GraphicObject* self
,	const AbstrUnit*       entity, const AbstrDataItem*       indexAttr
,	SharedUnitInterestPtr& selEntity, SharedDataItemInterestPtr& selIndexAttr
,	Theme* selTheme)
{

	if (self->ShowSelectedOnly())
	{
		dms_assert(selTheme);
		const AbstrDataItem* selAttr = selTheme->GetThemeAttr();
		dms_assert(selAttr);
		dms_assert(entity);
		entity->UnifyDomain(selAttr->GetAbstrDomainUnit(), "TableDomain", "Domain of selection attribute", UM_Throw);

		SharedStr expr = selAttr->GetFullName();
		if (indexAttr)
			expr = mySSPrintF("lookup(%s, %s)", indexAttr->GetFullName().c_str(), expr.c_str());
		expr = mySSPrintF("select_with_org_rel(%s)", expr.c_str());

		const ValueClass* vc           = entity->GetValueType();
		const UnitClass*  resDomainCls = UnitClass::Find(vc->GetCrdClass());


		AbstrUnit* newSelEntity = resDomainCls->CreateUnit(self->GetContext(), selSetID);
		selEntity = newSelEntity;
		newSelEntity->SetExpr(SharedStr(expr) );

		AbstrDataItem* newSelIndexAttr = CreateDataItem(newSelEntity,
					selID, 
					newSelEntity, 
					entity
		);
		selIndexAttr = newSelIndexAttr; 
		newSelIndexAttr->SetKeepDataState(false);
		newSelIndexAttr->DisableStorage(true);


		if (indexAttr)
			expr = mySSPrintF("lookup(org_rel, %s)", indexAttr->GetFullName().c_str());
		else
			expr = "org_rel";

		newSelIndexAttr->SetExpr( expr );
	}
	else
	{
		selEntity    = entity;
		selIndexAttr = indexAttr;

		TreeItem* oldSelEntity= self->GetContext()->GetSubTreeItemByID(selSetID);
		if (oldSelEntity)
			oldSelEntity->EnableAutoDelete();
	}
}

//----------------------------------------------------------------------
// GetUserMode section
//----------------------------------------------------------------------
#include "SessionData.h"

UserMode GetUserMode()
{
	static UserMode userMode = UM_Unknown;
	if (userMode == UM_Unknown)
	{
		Int32 userModeID = SessionData::Curr()->ReadConfigValue("Tools", "NrGroups", UM_Edit);
		userMode = UserMode(userModeID);
		MakeMax(userMode, UM_View);
		MakeMin(userMode, UM_Edit);
	}
	return userMode;
}

GraphVisitState GVS_BreakOnSuspended()
{
	return SuspendTrigger::DidSuspend() ? GVS_Break : GVS_Continue; 
}

