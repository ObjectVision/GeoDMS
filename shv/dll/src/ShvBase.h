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

#if !defined(__SHV_BASE_H)
#define __SHV_BASE_H

#include "TicBase.h"

#include "dbg/Check.h"
#include "geo/Undefined.h"
#include "geo/Point.h"
#include "geo/Range.h"
#include "geo/Transform.h"


#include <vector>

#if defined(DM_SHV_EXPORTS)
#	define SHV_CALL __declspec(dllexport)
#else
#	if defined(DM_SHV_DLL)
#		define SHV_CALL __declspec(dllimport)
#	else
#		define SHV_CALL
#	endif
#endif

#define SHV_SUPPORT_OLDNAMES

//----------------------------------------------------------------------
// forward class references
//----------------------------------------------------------------------

class GraphicObject;
//template <typename ElemType> class GraphicContainer;
	class MovableObject;
		class MovableContainer;
			class GraphicVarRows;
				class LayerControlBase;
					class LayerControl;
					class LayerControlGroup;
				class LayerControlSet;
			class GraphicVarCols;
				class TableControl;
					class PaletteControl;
				class TableHeaderControl;
			class ViewControl;
				class TableViewControl;
				class EditPaletteControl;
				class MapControl;
		class DataItemColumn;
		class Wrapper;
			class ViewPort;
			class ScrollPort;

	class ScalableObject;
		class LayerSet;
		class GraphicLayer;
			class FeatureLayer;
			class GridLayer;
		class GraphicRect;

class  Theme;
struct ThemeSet;

class AbstrVisitor;
	class GraphUpdater;
	class GraphVisitor;
		class GraphDrawer;
		class GraphInvalidator;
		class MouseEventDispatcher;

class DataView;
class ShvClass;
	class LayerClass;

struct MenuItem;
struct MenuData;
struct Region;

struct CounterStacks;
struct EventInfo;

enum AspectNr;
enum DefaultFontID;
enum EventID;
enum ShvSyncMode { SM_Load, SM_Save };
enum ToolButtonID;
enum ViewStyle;
enum ViewStyleFlags;

struct GridCoord;
struct SelCaret;
struct IndexCollector;

//----------------------------------------------------------------------
// typedefs
//----------------------------------------------------------------------

typedef SizeT entity_id;
typedef SizeT feature_id;

typedef UInt8 ClassID;
typedef Bool  SelectionID;
typedef sequence_traits<SelectionID>::const_pointer SelectionIdCPtr;
typedef SizeT gr_elem_index;
typedef UInt32 resource_index_t;

typedef void (DMS_CONV *StatusTextFunc)(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg);

typedef void (DMS_CONV *CreateViewActionFunc)(
	const TreeItem* tiContext,
	CharPtr         sAction,
	Int32           nCode,
	Int32           x,
	Int32           y,
	bool            doAddHistory,
	bool            isUrl,
	bool			mustOpenDetailsPage
);

typedef std::pair<const AbstrDataItem*, const IndexCollector*> sel_caret_key;
typedef std::map<sel_caret_key, std::weak_ptr<SelCaret>>       sel_caret_map;
typedef Pair<CrdTransformation, IRect>                         grid_coord_key;
typedef std::map<grid_coord_key, std::weak_ptr<GridCoord>>     grid_coord_map;

typedef std::shared_ptr<GridCoord> GridCoordPtr;
typedef std::shared_ptr<SelCaret>  SelCaretPtr;
 
typedef std::shared_ptr<const GridCoord> GridCoordCPtr;
typedef std::shared_ptr<const SelCaret>  SelCaretCPtr;

extern CreateViewActionFunc g_ViewActionFunc;


#if defined(MG_DEBUG)
#	define MG_CHECK_PAST_BREAK
	const bool MG_DEBUG_REGION        = false;
	const bool MG_DEBUG_CARET         = false;
	const bool MG_DEBUG_SCROLL        = false  || MG_DEBUG_REGION;
	const bool MG_DEBUG_INVALIDATE    = false;
	const bool MG_DEBUG_COUNTERSTACKS = false;
	const bool MG_DEBUG_VALUEGETTER   = false;
	const bool MG_DEBUG_VIEWINVALIDATE= false;
	const bool MG_DEBUG_WNDPROC       = false;
	const bool MG_DEBUG_SIZE          = false;
	const bool MG_DEBUG_IDLE          = false;
#endif

//----------------------------------------------------------------------
// constants
//----------------------------------------------------------------------

constexpr int BORDERSIZE = 2;
constexpr int DOUBLE_BORDERSIZE = 2 * BORDERSIZE;

enum class CommandStatus {
	ENABLED  = 0,
	DISABLED = 1,
	HIDDEN   = 2,
	DOWN     = 3,
	UP       = 4,
};

enum GraphVisitState: unsigned char // see also the DIFFERENT used ActorVisitState
{
	GVS_UnHandled  = false, // processing not-completed, continue with next work (ready or component failed)
	GVS_Continue   = false, 
	GVS_Ready      = false,
	GVS_Handled    = true,  // task suspended or failed (at least for now), don't continue with next work or suspended (check DidSuspend() )
	GVS_Break      = true,
	GVS_Yield      = true,
};

GraphVisitState GVS_BreakOnSuspended();


#define MG_SKIP_WINDOWPOSCHANGED 1
#if defined(MG_SKIP_WINDOWPOSCHANGED)
#define MG_WM_SIZE WM_SIZE
#else
#define MG_WM_SIZE WM_WINDOWPOSCHANGED
#endif


#include <windows.h>

// somehow windows.h enabled warning 4200
#if defined(_MSC_VER)
#	pragma warning( disable : 4200) // nonstandard extension used : zero-sized array in struct/union
#endif

// mitigate the windows macro pollution
#undef CreateDC

extern HINSTANCE g_ShvDllInstance;

#endif // !defined(__SHV_BASE_H)
