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
#pragma once

#if !defined(_SHV_UTLS_H)
#define _SHV_UTLS_H


#include "ShvBase.h"
#include "GeoTypes.h"
#include "Aspect.h"
#include "CalcClassBreaks.h"

#include "ser/FileCreationMode.h"
#include "set/Token.h"
#include "DataLocks.h"

class GraphicObject;

//----------------------------------------------------------------------
// section : Tic extensions, TODO: move to TIC
//----------------------------------------------------------------------

const AbstrUnit* GetRealAbstrValuesUnit(const AbstrDataItem* adi);

//----------------------------------------------------------------------
// section : consts
//----------------------------------------------------------------------

const UInt32 DEFAULT_MAX_NR_BREAKS = 8;

//----------------------------------------------------------------------
// section : StatusText
//----------------------------------------------------------------------

struct StatusTextCaller
{
	void operator() (SeverityTypeID st, CharPtr msg) const;

	ClientHandle   m_ClientHandle = nullptr;
	StatusTextFunc m_Func = 0;
};

//----------------------------------------------------------------------
// section : Instance
//----------------------------------------------------------------------

SHV_CALL HINSTANCE GetInstance(HWND hWnd);

//----------------------------------------------------------------------
// section : CreateViewAction
//----------------------------------------------------------------------

void CreateViewValueAction(const TreeItem* tiContext, SizeT index, bool mustOpenDetailsPage);

//----------------------------------------------------------------------
// section : Transform to projection
//----------------------------------------------------------------------

const AbstrUnit* GetWorldCrdUnitFromGeoUnit(const AbstrUnit* geoUnit);
CrdTransformation GetGeoTransformation(const AbstrUnit* geoUnit);
CrdRect AsWorldExtents(const CrdRect& geoExtents, const UnitProjection* proj);
CrdRect AsWorldExtents(const CrdRect& geoExtents, const AbstrUnit* geoUnit);
grid_coord_key GetGridCoordKey(const AbstrUnit* geoUnit);

//----------------------------------------------------------------------
// section : TreeItem Properties
//----------------------------------------------------------------------

#include "DataLocks.h"

SharedStr       GetViewData  (const TreeItem* item);         // look  at DialogData of subItem "ViewData"
void            SetViewData  (TreeItem* item, CharPtr data); // store at DialogData of subItem "ViewData"

const TreeItem* GetNextDialogDataRef(const TreeItem* item, CharPtr& i, CharPtr e);

const TreeItem* GetDialogDataRef(const TreeItem* item);
void            SetDialogDataRef(      TreeItem* item, const TreeItem* ref);

SHV_CALL auto GetMappedData(const TreeItem* ti) -> const AbstrDataItem*;
SHV_CALL bool RefersToMappable(const TreeItem* ti);

//----------------------------------------------------------------------
// section : Sync & Save
//----------------------------------------------------------------------

void SyncRef(SharedPtr<const TreeItem>& ptr, TreeItem* context, TokenID id, ShvSyncMode sm);
void SyncRef(SharedDataItemInterestPtr& ptr, TreeItem* context, TokenID id, ShvSyncMode sm);
void SyncRef(SharedPtr<const AbstrUnit>& ptr, TreeItem* context, TokenID id, ShvSyncMode sm);

template <typename T>
T LoadValue(const TreeItem* context, TokenID nameID, typename param_type<T>::type defaultValue = UNDEFINED_VALUE(T) );

template <typename T>
void SaveValue(TreeItem* context, TokenID nameID, typename param_type<T>::type value);

template <typename T>
void SyncValue(TreeItem* context, TokenID nameID, T& value, typename param_type<T>::type defaultValue, ShvSyncMode sm);
void SyncValue(TreeItem* context, TokenID nameID, SharedStr& value, ShvSyncMode sm);


void SyncState(GraphicObject* obj, TreeItem* context, TokenID stateID, UInt32 state, bool defaultValue, ShvSyncMode sm);

void ChangePoint(AbstrDataItem* pointItem, const CrdPoint& point, bool isNew);

//----------------------------------------------------------------------
// section : ViewContext
//----------------------------------------------------------------------

SHV_CALL TokenID UniqueName(TreeItem* context, TokenID nameBaseID);
TokenID UniqueName(TreeItem* context, const Class* cls);
TokenID CopyName(TreeItem* context, TokenID orgNameID);

std::shared_ptr<GraphicObject> CreateFromContext(TreeItem* context, GraphicObject* owner);

//----------------------------------------------------------------------
// section : CheckedGdiCall
//----------------------------------------------------------------------

void CheckedGdiCall(bool result, CharPtr context);

const UInt32 MAX_COLOR = 0xFFFFFF;
void CheckColor(DmsColor clr); 

COLORREF GetFocusClr();
COLORREF GetDefaultClr(UInt32 i);
COLORREF GetSysPaletteColor(UInt8& counter);
COLORREF GetSelectedClr(SelectionID i);


//----------------------------------------------------------------------
// section : DrawBorder
//----------------------------------------------------------------------

void DrawButtonBorder(HDC dc, GRect& clientRect);
void DrawReversedBorder(HDC dc, GRect& clientRect);
inline void DrawBorder(HDC dc, GRect& clientRect, bool rev) 
{
	if (rev) 
		DrawReversedBorder(dc, clientRect);
	else
		DrawButtonBorder(dc, clientRect);
}

void DrawRectDmsColor (HDC dc, const GRect& rect, DmsColor color);
void FillRectDmsColor (HDC dc, const GRect& rect, DmsColor color);
void FillRectWithBrush(HDC dc, const GRect& rect, HBRUSH br);


//----------------------------------------------------------------------
// section : ToolButtonID
//----------------------------------------------------------------------

enum ToolButtonID {

	TB_ZoomAllLayers,               // Button Command
	TB_ZoomActiveLayer,             // Button Command
	TB_ShowFirstSelectedRow,        // 
	TB_ZoomSelectedObj,             // Button Command

	TB_ZoomIn1,                     // Key Command to zoom from center (triggered by '+' key)
	TB_ZoomOut1,                    // Key Command to zoom from center (triggered by '-' key)
//	===========
	TB_ZoomIn2,				        // DualPoint   Tool
	TB_ZoomOut2,			        // SinglePoint Tool
	OBSOLETE_TB_Pan,                // DualPoint   Tool
//	===========
	TB_Neutral,				        // zero-Tool, includes cooordinate copying when Ctrl (with Shift) key is pressed
//	===========
	TB_Info,                        // SinglePoint Tool
	TB_SelectObject,                // SinglePoint Tool
	TB_SelectRect,                  // DualPoint   Tool
	TB_SelectCircle,                // DualPoint   Tool
	TB_SelectPolygon,               // PolyPoint   Tool
	TB_SelectDistrict,              // SinglePoint Tool
//	===========
	TB_SelectAll,                   // Button Command
	TB_SelectNone,                  // Button Command
	TB_SelectRows,                  // Button Command
//	===========
	TB_Copy,                        // Button Command: Copy Vieport Contents to Clipboard
	TB_CopyLC,                      // Button Command: Copy LayerControl Contents to Clipboard
	TB_TableCopy,                   // Button Command: Copy table as text to Clipboard
	TB_Export,                      // Button Command: Copy MapControl contexts to .BMP files or TableControl contents to .csv files.
	TB_CutSel,                      // Button Command 
	TB_CopySel,                     // Button Command: Copy Vieport Contents to Clipboard
	TB_PasteSelDirect,              // Button Command: DROP on Location 
	TB_PasteSel,                    // SinglePoint Tool: MOVE->DRAG LBUTTONUP: DROP
	TB_DeleteSel,                   // Button Command
//	===========
	TB_Show_VP, TB_Show_VPLC, TB_Show_VPLCOV, // Tri-state Toggle Button for layout
	TB_SP_All, TB_SP_Active, TB_SP_None,      // Tri-state Toggle Button for showing palettes
	TB_NeedleOn,   TB_NeedleOff,              // Toggle-Button
	TB_ScaleBarOn, TB_ScaleBarOff,            // Toggle-Button
	TB_ShowSelOnlyOn, TB_ShowSelOnlyOff,      // Toggle-Button
//	===========
	TB_SyncScale, TB_SyncROI,
	TB_TableGroupBy,
	TB_GotoClipboardLocation,
	TB_FindClipboardLocation,
	TB_GotoClipboardZoomlevel,
	TB_GotoClipboardLocationAndZoomlevel,
	TB_CopyLocationAndZoomlevelToClipboard,
	//	TB_SetData,
	TB_Undefined
};

enum class PressStatus
{
	DontCare,
	Up,
	Dn,
	ThirdWay,
};

//----------------------------------------------------------------------
// enum class FontSizeCategory
//----------------------------------------------------------------------

enum class FontSizeCategory
{
	SMALL,
	MEDIUM,
	LARGE,
	COUNT,

	CARET = SMALL
};

CharPtr GetDefaultFontName(FontSizeCategory fid);
UInt32  GetDefaultFontHeightDIP(FontSizeCategory fid); // in Device Independent Pixels, which are assumed to be 1/96th of an inch.

const WCHAR UNDEFINED_WCHAR = 0xFFFF;

SHV_CALL Float64 GetDesktopDIP2pixFactorX();
SHV_CALL Float64 GetDesktopDIP2pixFactorY();
SHV_CALL Float64 GetDesktopDIP2pixFactor();

//----------------------------------------------------------------------
// desktop data section
//----------------------------------------------------------------------

SHV_CALL TreeItem* GetDefaultDesktopContainer(const TreeItem* ti);
SHV_CALL TreeItem* GetExportsContainer   (TreeItem* desktopItem);
SHV_CALL TreeItem* GetViewDataContainer  (TreeItem* desktopItem);
SHV_CALL TreeItem* CreateContainer       (TreeItem* container,   const TreeItem* item);
SHV_CALL TreeItem* CreateDesktopContainer(TreeItem* desktopItem, const TreeItem* item);

template <typename T>
const T* GetUltimateSourceItem(const T* item)
{
	dms_assert(item);
	return debug_cast<const T*>(item->GetUltimateSourceItem());
}

template <typename T>
inline T InterpolateValue(T first, T last, SizeT n, SizeT i)
{
	return (first*(n-i)+last*i)/n;
}

UInt32 InterpolateColor(UInt32 firstColor, UInt32 lastColor, SizeT n, SizeT i);

struct NewBreakAttrItems {
	SharedMutableUnitInterestPtr paletteDomain;
	SharedMutableDataItemInterestPtr breakAttr;
};

NewBreakAttrItems CreateBreakAttr(DataView* dv, const AbstrUnit* thematicUnit, const TreeItem* themeIndicator, SizeT n);

SharedDataItemInterestPtr CreateSystemColorPalette(DataView*, const AbstrUnit* paletteDomain, AspectNr aNr, bool ramp, bool always, bool unique, const Float64* first, const Float64* last);
SharedDataItemInterestPtr CreateColorPalette(DataView*, const AbstrUnit* paletteDomain, AspectNr aNr, DmsColor clr);
const AbstrDataItem*      GetSystemPalette        (const AbstrUnit* paletteDomain, AspectNr aNr);
SharedDataItemInterestPtr CreateSystemLabelPalette    (DataView*, const AbstrUnit* paletteDomain, AspectNr aNr);
SharedDataItemInterestPtr CreateEqualCountBreakAttr   (std::weak_ptr<DataView>, const AbstrDataItem* thematicAttr);
//void CreateJenksFisherBreakAttr(std::weak_ptr<DataView> dv_wptr, const AbstrDataItem* thematicAttr, ItemWriteLock&& iwlPaletteDomain, AbstrDataItem* breakAttr, ItemWriteLock&& iwlBreakAttr, AspectNr aNr = AN_AspectCount);
void CreateNonzeroJenksFisherBreakAttr(std::weak_ptr<DataView> dv_wptr, const AbstrDataItem* thematicAttr, ItemWriteLock&& iwlPaletteDomain, AbstrDataItem* breakAttr, ItemWriteLock&& iwlBreakAttr, AspectNr aNr = AN_AspectCount);
SharedDataItemInterestPtr CreateEqualIntervalBreakAttr(std::weak_ptr<DataView>, const AbstrUnit*     thematicUnit);

//----------------------------------------------------------------------
// config section
//----------------------------------------------------------------------

bool    HasAdminMode();
bool    IsBusy();

//----------------------------------------------------------------------
// DataContainer section
//----------------------------------------------------------------------

extern "C" {
	extern "C" SHV_CALL void DMS_CONV SHV_SetAdminMode(bool v);
	SHV_CALL const AbstrUnit*     DMS_CONV SHV_DataContainer_GetDomain   (const TreeItem* ti, UInt32 level, bool adminMode);
	SHV_CALL UInt32               DMS_CONV SHV_DataContainer_GetItemCount(const TreeItem* ti, const AbstrUnit* domain, UInt32 level, bool adminMode);
	SHV_CALL const AbstrDataItem* DMS_CONV SHV_DataContainer_GetItem     (const TreeItem* ti, const AbstrUnit* domain, UInt32 k, UInt32 level, bool adminMode);
}

SHV_CALL auto DataContainer_NextItem(const TreeItem* ti, const TreeItem* si, const AbstrUnit* domain, bool adminMode) -> const AbstrDataItem*;

//----------------------------------------------------------------------
// UpdateShowSelOnly section
//----------------------------------------------------------------------

void UpdateShowSelOnlyImpl(GraphicObject* self, const AbstrUnit* entity, const AbstrDataItem* indexAttr
,	SharedUnitInterestPtr& selEntity, SharedDataItemInterestPtr& selIndexAttr
,	Theme* selTheme
);

//----------------------------------------------------------------------
// GetUserMode section
//----------------------------------------------------------------------

enum UserMode { UM_Unknown, UM_View, UM_Select, UM_Edit };
UserMode GetUserMode();

inline dms_rw_mode DmsRwChangeType(bool mustCreateNew)
{
	return mustCreateNew
		?	dms_rw_mode::write_only_mustzero
		:	dms_rw_mode::read_write;
}

#endif // !defined(_SHV_UTLS_H)
