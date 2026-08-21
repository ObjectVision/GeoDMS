// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ShvDllPCH.h"
#include "act/UpdateMark.h" // UpdateMarker

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Assorted shv helpers (see ShvUtils.h): values-unit and classification
// lookups, ViewData properties, status text and selection utilities.

#include <format>
#include "ShvUtils.h"

#include "dbg/debug.h"
#include "dbg/DebugContext.h"
#include "vt/BaseBounds.h"
#include "vt/Conversions.h"
#include "vt/Pair.h"
#include "mci/Class.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "utl/Environment.h"
#include "utl/PlatformError.h"
#include "utl/IncrementalLock.h"
#include "utl/StrFormat.h"
#include "utl/splitPath.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataArrayValue.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "Projection.h"
#include "PropFuncs.h"
#include "SessionData.h"
#include "TicInterface.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

#include "StgBase.h"

#include "CalcClassBreaks.h"
#include "ValuesTable.h"

#include "GeoTypes.h"

#include "Aspect.h"

#include "DataView.h"
#include "DcHandle.h"
#include "GraphicObject.h"
#include "LayerClass.h"
#include "Theme.h"

#ifdef _WIN32
#include "shellscalingapi.h"
#endif

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

#ifdef _WIN32
HINSTANCE GetInstance(HWND hWnd)
{
	return reinterpret_cast<HINSTANCE>( GetWindowLongPtr(hWnd, GWLP_HINSTANCE) );
}
#endif

//----------------------------------------------------------------------
// section : CreateViewAction
//----------------------------------------------------------------------

CreateViewActionFunc g_ViewActionFunc = nullptr;
ChooseColorFunc      g_ChooseColorFunc = nullptr; // issue #859, set by the Qt GUI

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
		CreateViewAction(tiContext, mySSPrintF("dp.vi.attr!{}", index).c_str(), -1, -1, -1, true, false, mustOpenDetailsPage);
}

void CreateGotoAction(const TreeItem* tiContext)
{
	CreateViewAction(tiContext, "goto", 0, 0, 0, false, false, false);
}

//----------------------------------------------------------------------
// section : TPoint & TRect
//----------------------------------------------------------------------

#ifdef _WIN32
GPoint ScreenToClientGPoint(GPoint pt, HWND hWnd)
{
	CheckedGdiCall( ::ScreenToClient(hWnd, &AsPOINT(pt)),
		"ScreenToClient"
	);
	return pt;
}
#endif

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
// section : ViewContext
//----------------------------------------------------------------------

TokenID UniqueName(TreeItem* context, CharPtr nameBase)
{
	assert(context);
	UInt32 i = 0;
	while (true) {
		SharedStr nameStr = mySSPrintF("{}{}", nameBase, i++);
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
			SharedStr nameStr = mySSPrintF("{}{}", nameBaseID, ++i);
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

	return UniqueName(context, mySSPrintF("{}Copy", orgName.c_str()).c_str());
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
// UpdateShowSelOnly section
//----------------------------------------------------------------------

static StaticLateTokenID selID("SelID");
static StaticLateTokenID selSetID("SelSet");

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
			expr = mySSPrintF("lookup({}, {})", indexAttr->GetFullName().c_str(), expr.c_str());
		expr = mySSPrintF("select_with_org_rel({})", expr.c_str());

		const ValueClass* vc           = entity->GetValueType();
		const UnitClass*  resDomainCls = UnitClass::Find(vc->GetCrdClass());


		auto newSelEntity_owner = resDomainCls->CreateUnit(self->GetContext(), selSetID);
		AbstrUnit* newSelEntity = newSelEntity_owner.get();
		selEntity = newSelEntity;
		newSelEntity->SetExpr(SharedStr(expr) );

		AbstrDataItem* newSelIndexAttr = CreateDataItem(newSelEntity,
					selID,
					newSelEntity,
					entity
		).get(); // owned by newSelEntity (parent)
		selIndexAttr = newSelIndexAttr; 
		newSelIndexAttr->SetKeepDataState(false);
		newSelIndexAttr->DisableStorage(true);


		if (indexAttr)
			expr = mySSPrintF("lookup(org_rel, {})", indexAttr->GetFullName().c_str());
		else
			expr = "org_rel";

		newSelIndexAttr->SetExpr( expr );
	}
	else
	{
		selEntity    = entity;
		selIndexAttr = indexAttr;

		// GetContext() can be null for sub-controls built before their ViewContext is wired,
		// e.g. the TableControl created inside PaletteControl::CreateColumns. Skip the
		// cleanup in that case — there is no SelSet sub-item to dispose of yet.
		// (Release-only crash on Linux: optimizer changes the order so that this code
		//  runs with m_ViewContext still null; Debug initialization happens to work.)
		if (auto* ctx = self->GetContext())
		{
			TreeItem* oldSelEntity = ctx->GetSubTreeItemByID(selSetID);
			if (oldSelEntity)
				oldSelEntity->RemoveFromConfig(); // detach from owning parent -> releases (and destroys) it
		}
	}
}

GraphVisitState GVS_BreakOnSuspended()
{
	return SuspendTrigger::DidSuspend() ? GVS_Break : GVS_Continue; 
}

