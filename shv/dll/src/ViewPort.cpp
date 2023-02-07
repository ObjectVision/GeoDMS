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

#include "ViewPort.h"
#include "../res/Resource.h"

#include "act/InvalidationBlock.h"
#include "act/ActorVisitor.h"
#include "act/TriggerOperator.h"
#include "act/UpdateMark.h"
#include "dbg/debug.h"
#include "dbg/DebugCast.h"
#include "geo/Conversions.h"
#include "mci/Class.h"
#include "mci/ValueComposition.h"
#include "set/Token.h"
#include "utl/mySPrintF.h"
#include "utl/SplitPath.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataLocks.h"
#include "DataStoreManagerCaller.h"
#include "Unit.h"
#include "UnitClass.h"

#include "StgBase.h"
#include "gdal/gdal_base.h"

#include "CaretOperators.h"
#include "Carets.h"
#include "ClipBoard.h"
#include "Controllers.h"
#include "DataView.h"
#include "GraphicContainer.h"
#include "GraphicRect.h"
#include "GridCoord.h"
#include "IndexCollector.h"
#include "KeyFlags.h"
#include "LayerSet.h"
#include "MouseEventDispatcher.h"
#include "ScaleBar.h"
#include "SelCaret.h"
#include "WmsLayer.h"

//----------------------------------------------------------------------
// class  : ViewPort
//----------------------------------------------------------------------

ViewPort::ViewPort(MovableObject* owner, DataView* dv, CharPtr caption, CrdType subPixelFactor) 
	:	Wrapper(owner, dv, caption)
	,	m_Orientation(OrientationType::NegateY)
	,	m_NeedleCaret(nullptr)
	,	m_ScaleBarCaret(nullptr)
	,	m_BrushOrg(0, 0)
	,	m_BkColor(DmsColor2COLORREF( STG_Bmp_GetDefaultColor(CI_BACKGROUND) )) // White, adjustable by tools->options->Current configuration
	,	m_SubPixelFactor(subPixelFactor)
{
	SetViewPortCursor(LoadCursor(NULL, IDC_ARROW));
	m_State.Set(GOF_IgnoreActivation);
}

ViewPort::~ViewPort()
{
	auto dv = GetDataView().lock();
	if (m_NeedleCaret && dv)
	{
		dbg_assert( DataViewOK() );
		dv->RemoveCaret(m_NeedleCaret);
	}
	if (m_ScaleBarCaret && dv)
	{
		dbg_assert( DataViewOK() );
		dv->RemoveCaret(m_ScaleBarCaret);
	}
	ClearContents(); // make sure all SelCarets and GridCoords (indirect property) are destroyed
}

#define ROI_TL_NAME "RoiTopLeft"
#define ROI_BR_NAME "RoiBottomRight"
static TokenID t_RoiTL = GetTokenID_st(ROI_TL_NAME);
static TokenID t_RoiBR = GetTokenID_st(ROI_BR_NAME);
static TokenID t_WCU = GetTokenID_st("WorldCrdUnit");
static TokenID vpminsID = GetTokenID_st("ViewPortMinSize");
static TokenID vpmaxsID = GetTokenID_st("ViewPortMaxSize");

void ViewPort::Sync(TreeItem* context, ShvSyncMode sm) 
{
	ObjectContextHandle contextHandle(context, "ViewPort::Sync");

	base_type::Sync(context, sm);
	if (sm == SM_Save)
		return;
	const AbstrDataItem* roiTL = dynamic_cast<const AbstrDataItem*>(
		FindTreeItemByID(context, t_RoiTL )
	);
	if (roiTL)
		InitWorldCrdUnit(roiTL->GetAbstrValuesUnit());
}


void ViewPort::InitWorldCrdUnit(const AbstrUnit* worldCrdUnit) 
{
	dms_assert(!SuspendTrigger::DidSuspend());
	worldCrdUnit = GetWorldCrdUnitFromGeoUnit(worldCrdUnit);
	if (m_WorldCrdUnit)
		if (m_WorldCrdUnit == worldCrdUnit || !worldCrdUnit)
			return;

	TreeItem* context = GetContext();
	if (worldCrdUnit)
	{
		if (m_WorldCrdUnit)
			m_WorldCrdUnit->UnifyValues(worldCrdUnit, "Already set WorldCrdUnit", "new WorldCrdUnit", UnifyMode(UM_AllowTypeDiff | UM_Throw | UM_AllowDefaultLeft)); // worldCrdUnit must not have less metrc/projection
		m_WorldCrdUnit = worldCrdUnit;
	}
	else
	{
		dms_assert(!m_WorldCrdUnit); // follows from not returning prematurely
		m_WorldCrdUnit = 
				Unit<CrdPoint>::GetStaticClass()->CreateUnit(
					context 
				,	t_WCU
				);
	}
	Invalidate();
}

CrdTransformation ViewPort::CalcWorldToClientTransformation() const
{
	UpdateView();
	return GetCurrWorldToClientTransformation();
}

CrdTransformation ViewPort::CalcCurrWorldToClientTransformation() const
{
	return CrdTransformation(
		const_cast<ViewPort*>(this)->GetROI(), 
		Convert<CrdRect>( TRect(TPoint(0, 0), GetCurrClientSize() ) ), 
		m_Orientation
	);
}

void ViewPort::DoUpdateView()
{
	dms_assert(GetContents());

	if (!GetContents())
		return;
	if (dynamic_cast<LayerSet*>(GetContents()) && !GetContents()->NrEntries())
		return;

	if (m_Tracker)
		m_Tracker->AdjustTargetVieport();

	if (GetROI().empty() || GetCurrClientSize().IsSingular())
		return;

	CrdTransformation w2vTr = CalcCurrWorldToClientTransformation();

	if (m_w2vTr == w2vTr)
		return;

	m_w2vTr = w2vTr;

	InvalidateDraw();
	UpdateScaleBar();

	for (auto& gc: m_GridCoordMap)
		gc.second.lock()->Init(TPoint2GPoint(GetCurrClientSize()), m_w2vTr);

	for (auto& sc: m_SelCaretMap)
		sc.second.lock()->OnZoom();
}

bool ViewPort::Draw(GraphDrawer& d) const
{
	if (!d.IsInOnPaint())	// when drawing from OnPaint, ReverseCarets undoes this
		for (const auto& sc: m_SelCaretMap)
			sc.second.lock()->UpdateRgn(d.GetAbsClipRegion());

	return false;
}

void ViewPort::SetClientSize(TPoint newSize)
{
	if (GetCurrClientSize() == newSize)
		return;

	base_type::SetClientSize(newSize);

	InvalidateView();
	InvalidateDraw();
}

void ViewPort::UpdateScaleBar()
{
	auto dv = GetDataView().lock(); if (!dv) return;
	if (m_ScaleBarCaret)
		dv->MoveCaret(m_ScaleBarCaret, ScaleBarCaretOperator() );
}

void ViewPort::InvalidateOverlapped()
{
	auto dv = GetDataView().lock(); if (!dv) return;
	if (m_ScaleBarCaret)
		dv->InvalidateRect(m_ScaleBarCaret->GetCurrBoundingBox());
}

void ViewPort::OnChildSizeChanged()
{
}


void ViewPort::ToggleNeedleController()
{
	m_State.Toggle(VPF_NeedleVisible);
}

void ViewPort::SetNeedle(CrdTransformation* tr, GPoint trackPoint)
{
	auto dv = GetDataView().lock(); if (!dv) return;
	dms_assert(tr);
	dbg_assert(DataViewOK());

	if (IsNeedleVisible())
	{
		if (!m_NeedleCaret)
			dv->InsertCaret(m_NeedleCaret = new NeedleCaret);

		NeedleCaretOperator co(
			trackPoint,
			TRect2GRect(GetCurrClientAbsRect()),
			this
		);
		dv->MoveCaret(m_NeedleCaret, co);
	}
	else
	{
		if (m_NeedleCaret)
		{
			dv->RemoveCaret(m_NeedleCaret);
			m_NeedleCaret = nullptr;
		}
	}
}

ScalableObject* ViewPort::GetContents()
{
	return debug_cast<ScalableObject*>(base_type::GetContents()); 
}

const ScalableObject* ViewPort::GetContents() const
{
	return debug_cast<const ScalableObject*>(base_type::GetContents()); 
}

const AbstrUnit* ViewPort::GetWorldCrdUnit() const
{
	return m_WorldCrdUnit;
}

CrdRect WorldRect_AddBorderPixels(CrdRect result, const TRect& viewPort, const ScalableObject* go, OrientationType orientation, CrdType subPixelFactor)
{
	if (!result.empty())
	{
		GRect border = go->GetBorderPixelExtents(subPixelFactor);

		if (	border != GRect(GPoint(0,0), GPoint(0,0))
			&&	IsStrictlyLower(border.Size(), TPoint2GPoint(viewPort.Size())))
		{
			CrdRect crdBorder = Convert<CrdRect>(border);
			result +=
				CrdTransformation(result, Convert<CrdRect>(viewPort) - crdBorder, orientation).WorldScale( crdBorder );
		}
	}
	return result;
}

static ViewPort* g_CurrVpZoom = nullptr;

CrdRect calcWorldFullRect(const TRect& viewPort, const ScalableObject* go, OrientationType orientation, CrdType subPixelFactor)
{
	auto wcr = go->CalcWorldClientRect();
	return WorldRect_AddBorderPixels(wcr, viewPort, go, orientation, subPixelFactor);
}

CrdRect calcSelectedWorldFullRect(const TRect& viewPort, const GraphicLayer* gl, OrientationType orientation, CrdType subPixelFactor)
{
	CrdRect selectRect = gl->CalcSelectedFullWorldRect();
	if (! selectRect.inverted()) 
		return WorldRect_AddBorderPixels(selectRect, viewPort, gl, orientation, subPixelFactor);
	return selectRect;
}

CrdRect getCurrWorldFullRect(const TRect& viewPort, const ScalableObject* go, OrientationType orientation, CrdType subPixelFactor)
{
	return WorldRect_AddBorderPixels(go->GetCurrWorldClientRect(), viewPort, go, orientation, subPixelFactor);
}

const LayerSet* ViewPort::GetLayerSet() const
{
	return debug_cast<const LayerSet*>(GetContents());
}


LayerSet* ViewPort::GetLayerSet()
{
	return debug_cast<LayerSet*>(GetContents());
}

GraphicLayer* ViewPort::GetActiveLayer()
{
	LayerSet* ls = GetLayerSet();
	if (!ls) return 0;
	return ls->GetActiveLayer();
}

const GraphicLayer* ViewPort::GetActiveLayer() const
{
	const LayerSet* ls = GetLayerSet();
	if (!ls) return 0;
	return ls->GetActiveLayer();
}

void ViewPort::ZoomWorldFullRect(const TRect& relClientRect)
{
	const ScalableObject* gc = GetContents();
	dms_assert(gc);

	CrdRect roi;
	
	for (gr_elem_index n = gc->NrEntries(); n--; )
	{
		const ScalableObject* go = gc->GetConstEntry(n);
		if (!go->HasNoExtent())
		{
			auto cwfr= calcWorldFullRect(relClientRect, go, m_Orientation, m_SubPixelFactor);
			roi |= cwfr;
		}
	}

	if (roi.empty())
		roi = Convert<CrdRect>(relClientRect);
	SetROI(roi);
}

CrdRect ViewPort::GetCurrWorldFullRect() const
{
	const ScalableObject* gc = GetContents();
	dms_assert(gc);

	gr_elem_index n = gc->NrEntries();
	dms_assert(n);

	TRect relClientRect = GetCurrClientRelRect();
	CrdRect roi;
	for (gr_elem_index i = 0; i != n; ++i)
	{
		const ScalableObject* go = gc->GetConstEntry(i);
		if (!go->HasNoExtent())
			roi |= getCurrWorldFullRect(relClientRect, go, m_Orientation, m_SubPixelFactor);
	}
	return roi;
}


CrdRect ViewPort::GetCurrWorldClientRect() const
{
	return m_w2vTr.Reverse( Convert<CrdRect>( TRect(TPoint(0, 0), GetCurrClientSize()) ) ); 
}

CrdRect ViewPort::CalcCurrWorldClientRect() const
{
	return CalcCurrWorldToClientTransformation().Reverse( Convert<CrdRect>( TRect(TPoint(0, 0), GetCurrClientSize()) ) ); 
}

CrdRect ViewPort::CalcWorldClientRect() const
{
	UpdateView();
	return GetCurrWorldClientRect();
}

CrdType ViewPort::GetCurrZoomLevel() const
{
	return m_w2vTr.ZoomLevel();
}

void ViewPort::ZoomAll()
{
	ScalableObject* gc = GetContents();
	if (!gc || !gc->NrEntries()) 
		return;
	ZoomWorldFullRect(GetCurrClientRelRect());
}

ViewPort* ViewPort::g_CurrZoom = nullptr;

void ViewPort::AL_ZoomAll()
{
	tmp_swapper<ViewPort*> currVpSetter(g_CurrZoom, this);

	ScalableObject* go = GetActiveLayer();
	if (go)
		SetROI(calcWorldFullRect(CalcClientRelRect(), go, m_Orientation, m_SubPixelFactor));
}

void ViewPort::AL_ZoomSel()
{
	GraphicLayer* al = GetActiveLayer();
	if (al)
	{
		CrdRect selectRect = calcSelectedWorldFullRect(CalcClientRelRect(), al, m_Orientation, m_SubPixelFactor);
		if (! selectRect.empty())
			SetROI(selectRect);
	}
}

void ViewPort::AL_SelectAllObjects(bool select)
{
	GraphicLayer* al = GetActiveLayer();
	if (al)
		al->SelectAll(select);
}

auto FindWmsLayer(LayerSet* ls) -> WmsLayer*
{
	for (gr_elem_index i = 0; i != ls->NrEntries(); ++i)
	{
		auto* layerBase = ls->GetEntry(i);
		auto asWmsLayer = dynamic_cast<WmsLayer*>(layerBase);
		if (asWmsLayer)
			return asWmsLayer;
		auto asSubset = dynamic_cast<LayerSet*>(layerBase);
		if (!asSubset)
			continue;
		asWmsLayer = FindWmsLayer(asSubset);
		if (asWmsLayer)
			return asWmsLayer;
	}
}

auto ViewPort::FindBackgroundWmsLayer() -> WmsLayer*
{
	auto ls = GetLayerSet();
	return FindWmsLayer(ls);
}

void ViewPort::ZoomIn1()
{
	auto layer = FindBackgroundWmsLayer();
	if (layer->ZoomIn())
		return;
	ZoomFactor(0.5); // zoom in on half of org ROI
}

void ViewPort::ZoomFactor(CrdType factor)
{
	if (!m_WorldCrdUnit)
		return;

	CrdPoint p = Center(GetROI());
	CrdPoint s = Size(GetROI()) * (factor * 0.5);

	SetROI(CrdRect(p-s, p+s)); // zoom in on half of org ROI
}


void ViewPort::ZoomOut1()
{
	auto layer = FindBackgroundWmsLayer();
	if (layer->ZoomOut())
		return;
	ZoomFactor(2.0); // zoom in on twice the org ROI
}

void ViewPort::PanTo(CrdPoint newCenter)
{
	CrdPoint s = Size(GetROI()) * 0.5;

	SetROI(CrdRect(newCenter - s, newCenter + s));
}

void ViewPort::PanToClipboardLocation()
{
	SharedStr clipboardText = ClipBoard().GetText();

	MemoInpStreamBuff buff(clipboardText.begin(), clipboardText.send());
	FormattedInpStream fin(&buff);

	CrdPoint worldCrd;
	dms_assert(worldCrd.first == 0.0 && worldCrd.second == 0.0);
	fin >> "[ X=" >> worldCrd.Col() >> "; Y=" >> worldCrd.Row() >> "]";
	if (worldCrd.first != 0.0 || worldCrd.second != 0.0)
		PanTo(worldCrd);
}

bool ViewPort::MouseEvent(MouseEventDispatcher& med)
{
	auto medOwner = med.GetOwner().lock(); if (!medOwner) return true;
	const EventInfo& eventInfo = med.GetEventInfo();
	EventID          eventID = eventInfo.GetID();
	if (eventID & EID_MOUSEWHEEL )
	{
		int wheelDelta = GET_WHEEL_DELTA_WPARAM(eventInfo.m_wParam);
		if (wheelDelta)
		{
			CrdType factor = pow(0.5, wheelDelta / WHEEL_DELTA);
			ZoomFactor(factor);
			return true;
		}
	}
	if (eventID & (EID_LBUTTONDOWN | EID_LBUTTONUP | EID_LBUTTONDBLCLK) )
	{
		AddClientOffset  viewportOffset(&med, GetCurrClientRelPos());
		Transformation tr = (CalcWorldToClientTransformation() + Convert<CrdPoint>(med.GetClientOffset()));

		InfoController::SelectFocusElem(GetLayerSet(), tr.Reverse(Convert<CrdPoint>(eventInfo.m_Point)), eventID);

		if (eventID & EID_LBUTTONDOWN) switch (medOwner->m_ControllerID)
		{
			case TB_ZoomIn2:
			{
				medOwner->InsertController(
					new ZoomInController(medOwner.get(), this, tr,  eventInfo.m_Point)
				);
				return true;
			}
			case TB_ZoomOut2:
				medOwner->InsertController(
					new ZoomOutController(medOwner.get(), this, tr)
				);
				return true;

			case TB_SelectObject:
				medOwner->InsertController(
					new SelectObjectController(medOwner.get(), this, tr)
				);
				return true;
			case TB_SelectRect:
				medOwner->InsertController(
					new SelectRectController(medOwner.get(), this, tr,  eventInfo.m_Point)
				);
				return true;

			case TB_SelectCircle:
				medOwner->InsertController(
					new SelectCircleController(medOwner.get(), this, tr, eventInfo.m_Point)
				);
				return true;

			case TB_SelectPolygon:
				medOwner->InsertController(
					new DrawPolygonController(medOwner.get(), this, tr, eventInfo.m_Point)
				);
				return true;

			case TB_SelectDistrict:
				medOwner->InsertController(
					new SelectDistrictController(medOwner.get(), this, CalcWorldToClientTransformation() + Convert<CrdPoint>(med.GetClientOffset() ) 
					)
				);
				return true;

			case TB_Neutral:
				if (med.GetEventInfo().m_EventID & EID_CTRLKEY)
				{
					if (med.GetEventInfo().m_EventID & EID_SHIFTKEY)
						ClipBoard::ClearBuff();
					med.GetEventInfo().m_EventID |= EID_COPYCOORD;
				}
				else
					medOwner->InsertController(
						new PanController(medOwner.get(), this, eventInfo.m_Point)
					);
		}
	}
	return Wrapper::MouseEvent(med); // returns false
}

#include "DataArray.h" 

CrdType interpolate(CrdType min, CrdType max, UInt32 i, UInt32 n)
{
	return (i*max + (n-i) *min) / n;
}

#include "FilePtrHandle.h"

void SaveBitmap(WeakStr filename, HBITMAP hBitmap)
{
	BITMAPINFO        bmpInfo;
	BITMAPFILEHEADER  bmpFileHeader; 

	DcHandleBase hdc(NULL);
	ZeroMemory(&bmpInfo,sizeof(BITMAPINFO));
	bmpInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	GetDIBits(hdc,hBitmap,0,0,nullptr,&bmpInfo,DIB_RGB_COLORS); 
	bmpInfo.bmiHeader.biBitCount = 3*8;
//	if(bmpInfo.bmiHeader.biSizeImage<=0)
		bmpInfo.bmiHeader.biSizeImage
			=	abs(bmpInfo.bmiHeader.biHeight) * ((bmpInfo.bmiHeader.biWidth * (bmpInfo.bmiHeader.biBitCount+7)/8 + 0x03) & ~0x03);

	OwningPtrSizedArray<BYTE> pBuf( bmpInfo.bmiHeader.biSizeImage MG_DEBUG_ALLOCATOR_SRC("SaveBitmap"));

	bmpInfo.bmiHeader.biCompression=BI_RGB;
	GetDIBits(hdc,hBitmap,0,bmpInfo.bmiHeader.biHeight,pBuf.begin(), &bmpInfo, DIB_RGB_COLORS);

	FilePtrHandle fh;
	if(!fh.OpenFH(filename, 0, FCM_CreateAlways, false, NR_PAGES_DATFILE))
		throwDmsErrF("Cannot Create %s" , filename.c_str());
	bmpFileHeader.bfReserved1=0;
	bmpFileHeader.bfReserved2=0;
	bmpFileHeader.bfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+bmpInfo.bmiHeader.biSizeImage;
	bmpFileHeader.bfType='MB';
	bmpFileHeader.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
	fwrite(&bmpFileHeader,sizeof(BITMAPFILEHEADER),1,fh);
	fwrite(&bmpInfo.bmiHeader,sizeof(BITMAPINFOHEADER),1,fh);
	fwrite(pBuf.begin(),bmpInfo.bmiHeader.biSizeImage,1,fh); 
}

//===================================== ViewPort::Export()
ExportInfo ViewPort::GetExportInfo()
{
	return ExportInfo(this);
}

void ViewPort::Export()
{
	auto dv = GetDataView().lock(); if (!dv) return;

	FixedContextHandle context("while Exporting full ViewPort contents");
	FencedInterestRetainContext resultHolder;

	dms_assert(!SuspendTrigger::DidSuspend()); // DEBUG.
	dbg_assert(DataViewOK());

	ExportInfo info(this);

	bool isBmp = true; // false would indicate tiff

	auto tmpVP = std::make_shared<ViewPort>(nullptr, nullptr, "", info.m_SubPixelFactor);
	tmpVP->SetContents(GetContents()->shared_from_this());
	tmpVP->InitWorldCrdUnit(GetWorldCrdUnit());
	tmpVP->SetClientSize(Convert<TPoint>(info.GetNrSubDotsPerTile()));

	std::shared_ptr<ScaleBarObj> optionalScaleBar;
	if (m_ScaleBarCaret)
		optionalScaleBar = std::make_shared<ScaleBarObj>(tmpVP.get(), tmpVP.get());

	// and go
	UInt32 row, col;
	CrdRect tileRoi; 
	
	for (	tileRoi.first .Row() = info.m_ROI.first.Row(), row = 0
		;	tileRoi.second.Row() = interpolate(info.m_ROI.first.Row(), info.m_ROI.second.Row(), row+1, info.m_NrTiles.Row()), row != info.m_NrTiles.Row()
		;	tileRoi.first .Row() = tileRoi.second.Row(), ++row)
	{
		UInt32 rowFromTop = info.m_NrTiles.Row() - row - 1;
		for (	tileRoi.first .Col() = info.m_ROI.first.Col(), col = 0
			;	tileRoi.second.Col() = interpolate(info.m_ROI.first.Col(), info.m_ROI.second.Col(), col+1, info.m_NrTiles.Col()), col != info.m_NrTiles.Col()
			;	tileRoi.first. Col() = tileRoi.second.Col(), ++col)
		{
			tmpVP->SetROI( tileRoi );
			if (optionalScaleBar)
				optionalScaleBar->DetermineAndSetBoundingBox(
					Point2TPoint(info.GetNrSubDotsPerTile()) * TPoint(col, rowFromTop), 
					Point2TPoint(info.GetNrSubDotsPerPage()), 
					info.m_SubPixelFactor
				);

			GdiHandle<HBITMAP> bitmap( tmpVP->GetAsDDBitmap(dv.get(), info.m_SubPixelFactor, optionalScaleBar.get()) );

			dms_assert(!SuspendTrigger::DidSuspend());

			if (isBmp)
			{
				SharedStr fileName = mySSPrintF("%s_%d_%d.bmp", info.m_FullFileNameBase.c_str(), rowFromTop, col);
				SaveBitmap(fileName, bitmap);
			}
			else
			{
	//				TifImp fh; fh.Open(mySSPrintF("%s_%d_%d.tif", fullFileNameBase.c_str(), row, col).c_str(), TIF_WRITE);  // NYI
			}
		}
	}
}

#if(WINVER < 0x0500)
#define IDC_HAND            MAKEINTRESOURCE(32649)
#endif /* WINVER >= 0x0500 */

int ScrollStepSize()
{
	return (GetKeyState(VK_SHIFT)& 0x8000)
		?	PAGE_SCROLL_STEP
		:	LINE_SCROLL_STEP; 
}

bool ViewPort::OnKeyDown(UInt32 virtKey)
{
	if (KeyInfo::IsSpec(virtKey)) 
	{
		switch (KeyInfo::CharOf(virtKey)) {
			case VK_ADD:      return OnCommand(TB_ZoomIn1);
			case VK_SUBTRACT: return OnCommand(TB_ZoomOut1);			

			case VK_RIGHT:    Scroll(GPoint(-ScrollStepSize(), 0)); return true;
			case VK_LEFT:     Scroll(GPoint( ScrollStepSize(), 0)); return true;
			case VK_UP:       Scroll(GPoint(0,  ScrollStepSize())); return true;
			case VK_DOWN:     Scroll(GPoint(0, -ScrollStepSize())); return true;
		}
	} else if (KeyInfo::IsCtrl(virtKey)) {
		switch (KeyInfo::CharOf(virtKey)) {
			case 'L': return OnCommand(TB_ZoomAllLayers);
			case 'Y': return OnCommand(TB_ZoomActiveLayer);
			case 'F': return OnCommand(TB_ZoomSelectedObj);
			case 'N': return OnCommand(TB_Export);
			case 'G': return OnCommand(TB_GotoClipboardLocation);
		}
	} else if (KeyInfo::IsCtrlAlt(virtKey)) {
		switch (KeyInfo::CharOf(virtKey)) {
			case 'X': return OnCommand(TB_CutSel);
			case 'C': return OnCommand(TB_CopySel);
			case 'P': return OnCommand(TB_PasteSelDirect);
			case 'D': return OnCommand(TB_DeleteSel);
		}
	}
	return base_type::OnKeyDown(virtKey);
}

bool ViewPort::OnCommand(ToolButtonID id)
{
	auto dv = GetDataView().lock(); if (!dv) return false;
	switch (id)
	{
		case TB_ZoomIn1:  ZoomIn1 (); return true;
		case TB_ZoomOut1: ZoomOut1(); return true;

		case TB_ZoomAllLayers:    ZoomAll();   return true;
		case TB_ZoomActiveLayer:  AL_ZoomAll(); return true;
		case TB_ZoomSelectedObj:  AL_ZoomSel(); return true;

		case TB_Neutral:
			SetViewPortCursor( LoadCursor(NULL, IDC_ARROW) );
			goto setControllerID;

		case TB_ZoomIn2:
			SetViewPortCursor( LoadCursor(g_ShvDllInstance, MAKEINTRESOURCE(IDC_ZOOMIN) ) );
			goto setControllerID;

		case TB_ZoomOut2:
			SetViewPortCursor( LoadCursor(g_ShvDllInstance, MAKEINTRESOURCE(IDC_ZOOMOUT) ) );
			goto setControllerID;

		case TB_SelectObject:
		case TB_SelectRect:
		case TB_SelectCircle:
		case TB_SelectPolygon:
		case TB_SelectDistrict:
			SetViewPortCursor( LoadCursor(NULL, MAKEINTRESOURCE(IDC_SELECTDIAMOND) ) );
			goto setControllerID;

		case TB_SelectAll : AL_SelectAllObjects( true); return true;
		case TB_SelectNone: AL_SelectAllObjects(false); return true;

		case TB_NeedleOn:
		case TB_NeedleOff:
			if (IsNeedleVisible() != (id == TB_NeedleOn))
				ToggleNeedleController();
			return true;

		case TB_ScaleBarOn:
			if (!m_ScaleBarCaret)
				dv->InsertCaret(m_ScaleBarCaret = new ScaleBarCaret(this) );
			UpdateScaleBar();
			return true;

		case TB_ScaleBarOff:
			InvalidateOverlapped();
			if (m_ScaleBarCaret)
			{
				dv->RemoveCaret(m_ScaleBarCaret); 
				m_ScaleBarCaret = nullptr;
			}
			return true;

		case TB_SP_All:
		case TB_SP_Active:
		case TB_SP_None:
			return GetContents()->OnCommand(id);

		case TB_CutSel:
		case TB_CopySel:
		case TB_PasteSelDirect:
		case TB_PasteSel:
		case TB_DeleteSel:
		case TB_ShowSelOnlyOn:
		case TB_ShowSelOnlyOff:
			{
				GraphicLayer* al = GetActiveLayer();
				if (!al)
					throwItemError("No layer is activated");
				return al->OnCommand(id);
			}
		case TB_GotoClipboardLocation:
			PanToClipboardLocation();
	}
	return base_type::OnCommand(id);

setControllerID:
	dms_assert(dv);
	dv->m_ControllerID = id;
	return true;
}

void ViewPort::FillMenu(MouseEventDispatcher& med)
{
	//	Goto & Find
	med.m_MenuData.push_back(MenuItem(SharedStr("Goto (Ctrl-G): take Clipboard contents such as [ X=999; y=999 ] pan there"), new MembFuncCmd<ViewPort>(&ViewPort::PanToClipboardLocation), this));

	base_type::FillMenu(med);
}


CommandStatus ViewPort::OnCommandEnable(ToolButtonID id) const
{
	switch (id)
	{
		case TB_ShowSelOnlyOn:
		case TB_ShowSelOnlyOff:
			if (GetUserMode() < UM_Select) return CommandStatus::HIDDEN;
			{
				const GraphicLayer* al = GetActiveLayer();
				if (!al)
					return CommandStatus::DISABLED;
				return al->OnCommandEnable(id);
			}
	}
	return base_type::OnCommandEnable(id);
}

//============================
// PasteGridController
//============================

#include "GridLayer.h"
#include "GridDrawer.h"

class PasteGridController : public AbstrController
{
	typedef AbstrController base_type;
public:
	PasteGridController(
		DataView*      owner, 
		ViewPort*      vp,
		GridLayer*     target, 
		GridCoordPtr    gridCoords,
		SelValuesData* selValues
	)	:	AbstrController(owner, target, 
				EID_LBUTTONDOWN|EID_MOUSEMOVE|EID_MOUSEDRAG,
				EID_LBUTTONUP,
				EID_LBUTTONUP|EID_RBUTTONDOWN|EID_RBUTTONUP|EID_CAPTURECHANGED|EID_SCROLLED
			)
		,	m_ViewPort(vp)
		,	m_GridLayer(target)
		,	m_GridCoords(gridCoords)
		,	m_SelValues(selValues)
	{
		dms_assert(target);
		dms_assert(selValues);
	}
	~PasteGridController()
	{
		dms_assert(!m_OrgCursor);
	}

protected:
	bool Move (EventInfo& eventInfo) override
	{
		m_GridCoords->Update(1.0);
		IPoint gridNewBase = (IsDefined(eventInfo.m_Point))			
			?	m_GridCoords->GetExtGridCoordFromAbs(eventInfo.m_Point)
			:	UNDEFINED_VALUE(IPoint); 

		if (gridNewBase == m_SelValues->m_Rect.first)
			return false;

		m_GridLayer->InvalidatePasteArea();
		m_SelValues->MoveTo(gridNewBase);
		m_GridLayer->InvalidatePasteArea();
		return true;
	}
	bool Exec(EventInfo& eventInfo) override
	{
		m_GridLayer->PasteNow();
		return true;
	}

	void Stop () override
	{
		m_ViewPort->SetViewPortCursor(m_OrgCursor);
		m_OrgCursor = nullptr;
		m_GridLayer->ClearPaste();
 		base_type::Stop(); // !!! destroys this
	}

private:
	std::shared_ptr<ViewPort>    m_ViewPort;
	std::shared_ptr<GridLayer>   m_GridLayer;
	GridCoordPtr           m_GridCoords;
	WeakPtr<SelValuesData> m_SelValues;
	HCURSOR                m_OrgCursor = nullptr; friend class ViewPort;
};

//============================

void ViewPort::PasteGrid(SelValuesData* svd, GridLayer* gl)
{
	auto dv = GetDataView().lock(); if (!dv) return;
	SharedPtr<PasteGridController> pasteController = new PasteGridController(dv.get(), this, gl, gl->GetGridCoordInfo(this), svd);
	dv->InsertController(pasteController);
	pasteController->m_OrgCursor = SetViewPortCursor(LoadCursor(g_ShvDllInstance, MAKEINTRESOURCE(IDC_PAN)));
}

void ViewPort::Scroll(const GPoint& delta)
{
	DBG_START("ViewPort", "Scroll", MG_DEBUG_SCROLL);

	auto dv = GetDataView().lock(); if (!dv) return;

	dbg_assert(DataViewOK());

	if (delta == GPoint(0, 0) )
		return;

	CrdTransformation w2v = CalcWorldToClientTransformation();
//	m_w2vTr = w2v + Convert<CrdPoint>(delta);
	{
		InvalidationBlock lock(this);
		SetROI(GetROI() - w2v.WorldScale(Convert<CrdPoint>(delta)) );
	}
	m_w2vTr = CalcCurrWorldToClientTransformation();
	GRect viewExtents = TRect2GRect( CalcClientRelRect() );

	InvalidateOverlapped();

	dv->Scroll(delta, viewExtents, viewExtents, this);

	m_BrushOrg += delta;

	DBG_TRACE(("delta %s",       AsString(delta).c_str()));
	DBG_TRACE(("viewExtents %s", AsString(viewExtents).c_str()));

	for (auto& gc: m_GridCoordMap)
		gc.second.lock()->OnScroll(delta);

	for (auto& sc: m_SelCaretMap)
		sc.second.lock()->OnScroll(delta);
}

void ViewPort::InvalidateWorldRect(const CrdRect& rect, const GRect& borderExtents) const
{
	if (IsDefined(rect) && !rect.inverted())
		InvalidateClientRect(
			DRect2TRect( GetCurrWorldToClientTransformation().Apply(rect) )
		+	TRect(borderExtents)
		);
}

bool CreatePointParam(SharedRwDataItemInterestPtr& param, ViewPort* vp, TokenID nameID)
{
	if (param || !vp->GetContext())
		return false;
	param = CreateDataItem(vp->GetContext(), nameID, 	
		Unit<Void>::GetStaticClass()->CreateDefault(), 
		vp->GetWorldCrdUnit(), ValueComposition::Single
	);
	return true;
}

AbstrDataItem* GetPointParam(SharedRwDataItemInterestPtr& param, ViewPort* vp, TokenID nameID)
{
	if (!param && vp->GetContext())
		param = AsDynamicDataItem( vp->GetContext()->GetSubTreeItemByID(nameID) );
	return param;
}

Float64 GetSubItemValue(const TreeItem* context, TokenID id, Float64 defaultVal)
{
	dms_assert(context);
	const TreeItem* si = context->GetConstSubTreeItemByID(id);
	if (!si || !IsDataItem(si))
		return defaultVal;

	InterestPtr<const TreeItem*> tii(si);
	PreparedDataReadLock drl(AsDataItem(si));
	return AsDataItem(si)->GetRefObj()->GetValueAsFloat64(0);
}

void LimitRange(Float64& start, Float64& end, Float64 minSize, Float64 maxSize)
{
	dms_assert(start <= end );
	dms_assert(minSize <= maxSize);
	Float64 size = end - start;
	if (size >= minSize && size <= maxSize)
		return;
	Float64 center = (start + end) * 0.5;
	Float64 radius = ((size < minSize) ? minSize : maxSize) * 0.5;
	start = center - radius;
	end   = center + radius;
}

void ViewPort::SetROI(const CrdRect& r) 
{ 
	DBG_START("ViewPort", "SetROI", MG_DEBUG_SCROLL);

	DBG_TRACE(("NewRect : %s", AsString(r).c_str() ));

	if (r.inverted() || m_ROI == r)
		return;
    
	InitWorldCrdUnit(0);
	dms_assert(m_WorldCrdUnit); // must be set before

	bool tlIsNew = CreatePointParam(m_ROI_TL, this, t_RoiTL);
	bool brIsNew = CreatePointParam(m_ROI_BR, this, t_RoiBR);

	DBG_TRACE(("IsNew : %d", tlIsNew || brIsNew ));
	DBG_TRACE(("TlChanged: %d", GetContext() ? m_ROI_TL->GetLastChangeTS() : 0 ));
	DBG_TRACE(("BrChanged: %d", GetContext() ? m_ROI_BR->GetLastChangeTS() : 0 ));

	CrdType minSize = GetSubItemValue(m_WorldCrdUnit,  vpminsID, -1.0);
	CrdType maxSize = GetSubItemValue(m_WorldCrdUnit,  vpmaxsID, 40000.0e+9);
	if (minSize == -1.0)
		minSize = 10.0 / GetUnitSizeInMeters(m_WorldCrdUnit);
	CrdRect rr = r;
	LimitRange(rr.first.Row(), rr.second.Row(), minSize, maxSize);
	LimitRange(rr.first.Col(), rr.second.Col(), minSize, maxSize);

	ChangePoint(m_ROI_TL, rr.first , tlIsNew);
	ChangePoint(m_ROI_BR, rr.second, brIsNew);
	CertainUpdate(PS_Committed);
	m_ROI = rr;

	DBG_TRACE(("TlChanged: %d", GetContext() ? m_ROI_TL->GetLastChangeTS() : 0 ));
	DBG_TRACE(("BrChanged: %d", GetContext() ? m_ROI_BR->GetLastChangeTS() : 0 ));
//	DoUpdateView();
	InvalidateView();
	UpdateScaleBar();
}


void SetRoiParam(TreeItem* context, SharedRwDataItemInterestPtr& param, TokenID id, const AbstrUnit* worldCrdUnit)
{
	dms_assert(context);
	if (!param)
	{
		const AbstrDataItem* res = AsDynamicDataItem( context->GetConstSubTreeItemByID(id) );

		if (res && res->GetAbstrValuesUnit()->UnifyValues(worldCrdUnit, res->GetFullName().c_str(), "WorldCrdUnit", UM_AllowDefault))
			param = const_cast<AbstrDataItem*>(res);
	}
}

bool ViewPort::HasROI() const
{
	if (!m_WorldCrdUnit)
		return false;
	TreeItem* context = const_cast<ViewPort*>(this)->GetContext();
	if (!context)
		return false;

	SetRoiParam(context, m_ROI_TL, t_RoiTL, m_WorldCrdUnit);
	SetRoiParam(context, m_ROI_BR, t_RoiBR, m_WorldCrdUnit);

	return m_ROI_TL && m_ROI_BR; 
}

ActorVisitState ViewPort::DoUpdate(ProgressState ps)
{
	dms_assert(ps >= PS_Validated);
	if ( HasROI() )
	{
		if (!m_ROI_TL->PrepareData() || !m_ROI_BR->PrepareData())
			return AVS_SuspendedOrFailed;

		DataReadLock l1(m_ROI_TL);
		DataReadLock l2(m_ROI_BR);

		CrdRect roi = CrdRect(
			m_ROI_TL->GetRefObj()->GetValueAsDPoint(0), 
			m_ROI_BR->GetRefObj()->GetValueAsDPoint(0)
		);
		if (m_ROI != roi)
		{
			m_ROI = roi;
			if (!m_State.HasInvalidationBlock())
				InvalidateDraw();
		}
	}
	return AVS_Ready;
}

CrdRect ViewPort::GetROI() const
{ 
	dms_assert(GetContents());
	CertainUpdate(PS_Committed);
// ISSUE 262, WIP
//	if (m_ROI.empty())
//		const_cast<ViewPort*>(this)->ZoomAll();
	return m_ROI;
}

COLORREF ViewPort::GetBkColor() const
{
	return m_BkColor;
}

GraphVisitState ViewPort::InviteGraphVistor(AbstrVisitor& v)
{
	dms_assert(!SuspendTrigger::DidSuspend());
	return v.DoViewPort(this);
}

ActorVisitState ViewPort::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (HasROI())
	{
		if (visitor.Visit(GetPointParam(m_ROI_TL, const_cast<ViewPort*>(this), t_RoiTL)) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
		if (visitor.Visit(GetPointParam(m_ROI_BR, const_cast<ViewPort*>(this), t_RoiBR)) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
	}
	return base_type::VisitSuppliers(svf, visitor);
}

GridCoordPtr ViewPort::GetOrCreateGridCoord(const grid_coord_key& key)
{
	grid_coord_map::iterator pos = m_GridCoordMap.find(key);
	if (pos != m_GridCoordMap.end() && pos->first == key)
		return pos->second.lock();

	auto result = std::make_shared<GridCoord>(this, key, TPoint2GPoint(GetCurrClientSize()), m_w2vTr);
	m_GridCoordMap.insert(pos, grid_coord_map::value_type(key, result) );
	return result;
}

GridCoordPtr ViewPort::GetOrCreateGridCoord(const AbstrUnit* gridCrdUnit)
{
	return GetOrCreateGridCoord(GetGridCoordKey(gridCrdUnit));
}

//----------------------------------------------------------------------
// class  : sel_caret_key
//----------------------------------------------------------------------

const AbstrUnit* GetGridUnit(const sel_caret_key& key)
{
	return key.second ? key.second->GetFeatureDomain() : key.first->GetAbstrDomainUnit();
}

SelCaretPtr  ViewPort::GetOrCreateSelCaret (const sel_caret_key& key)
{
	sel_caret_map::iterator pos = m_SelCaretMap.find(key);
	if (pos != m_SelCaretMap.end() && pos->first == key) 
		return pos->second.lock();

	auto result = std::make_shared<SelCaret>(this, key, GetOrCreateGridCoord( GetGridUnit(key) ) );
	m_SelCaretMap.insert(pos, sel_caret_map::value_type(key, result) );
	return result;
}

IMPL_RTTI_CLASS(ViewPort)

