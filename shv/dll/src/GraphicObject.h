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
#pragma once

#ifndef __SHV_GRAPHICOBJECT_H
#define __SHV_GRAPHICOBJECT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ShvUtils.h"
#include "stg/AbstrStorageManager.h"

//----------------------------------------------------------------------
// const : GraphicClass flags
//----------------------------------------------------------------------

enum class GraphicClassFlags {
	None           = 0,
	PushVisibility = 0x0001,
	ChildCovered   = 0x0002, // Row and Col Containers are filled by children (thus don't require fill)
	ClipExtents    = 0x0004,
};
inline bool operator &(GraphicClassFlags lhs, GraphicClassFlags rhs) { return UInt32(lhs) & UInt32(rhs); }
inline GraphicClassFlags operator |(GraphicClassFlags lhs, GraphicClassFlags rhs) { return GraphicClassFlags(UInt32(lhs) | UInt32(rhs)); }

//----------------------------------------------------------------------
// const : GraphicObject flags
//----------------------------------------------------------------------

const UInt32 GOF_IsVisible           = actor_flag_set::AF_Next * 0x0001;
const UInt32 GOF_AllVisible          = actor_flag_set::AF_Next * 0x0002; // implies Visible and owner->AllVisible
const UInt32 GOF_Exclusive           = actor_flag_set::AF_Next * 0x0004;

const UInt32 GOF_Active              = actor_flag_set::AF_Next * 0x0008;
const UInt32 GOF_IgnoreActivation    = actor_flag_set::AF_Next * 0x0010; // cannot be activated (such as layers in an overlay and the id DataItemColumn)
const UInt32 GOF_Transparent         = actor_flag_set::AF_Next * 0x0020; // layers are transparent

const UInt32 GOF_IsDrawn             = actor_flag_set::AF_Next * 0x0040; 

const UInt32 GOF_IsUpdated           = actor_flag_set::AF_Next * 0x0080; 
const UInt32 GOF_AllUpdated          = actor_flag_set::AF_Next * 0x0100;
//REMOVE const UInt32 GOF_AllDataReady        = actor_flag_set::AF_Next * 0x0200; // Used only by TableControl and Control
const UInt32 GOF_ShowSelectedOnly    = actor_flag_set::AF_Next * 0x0400; // Used only by TableControl and Control

const UInt32 GOF_Next                = actor_flag_set::AF_Next * 0x0800;

const UInt32 SOF_DetailsVisible      = GOF_Next * 0x0001; // ScalableObject Only
const UInt32 SOF_DetailsTooLong      = GOF_Next * 0x0002; // ScalableObject Only
const UInt32 SOF_Topographic         = GOF_Next * 0x0004; // ScalableObject Only
const UInt32 GLF_EntityIndexReady    = GOF_Next * 0x0008; // GraphicLayer ONLY
const UInt32 GLF_FeatureIndexReady   = GOF_Next * 0x0010; // GraphicLayer ONLY

const UInt32 MOF_HasBorder           = GOF_Next * 0x0001; // MovableObject only
const UInt32 MOF_RevBorder           = GOF_Next * 0x0002; // MovableObject only

const UInt32 MOF_Next                = GOF_Next * 0x0004;

const UInt32 VPF_NeedleVisible       = MOF_Next * 0x01; // ViewPort ONLY:

const UInt32 SPF_NoScrollBars        = MOF_Next * 0x01; // ScrollPort ONLY:

const UInt32 TCF_HideSortOptions     = MOF_Next * 0x01; // TableControl ONLY ( PaletteControl is derived from TableControl)
const UInt32 TCF_FlipSortOrder       = MOF_Next * 0x02; // TableControl ONLY ( PaletteControl is derived from TableControl)
const UInt32 TCF_HideCount           = MOF_Next * 0x04; // TableControl ONLY ( PaletteControl is derived from TableControl)

const UInt32 PCF_CountsUpdated       = MOF_Next * 0x08; // PaletteControl ONLY:

const UInt32 DIC_HasElemBorder       = MOF_Next * 0x01; // DataItemColumn ONLY
const UInt32 DIC_RelativeDisplay     = MOF_Next * 0x02; // DataItemColumn ONLY
const UInt32 DIC_TotalReady          = MOF_Next * 0x04; // DataItemColumn ONLY

#if defined(MG_DEBUG)
const UInt32 GOFD_BlockInvalidateView = MOF_Next * 0x08; // Assume no invalidation between UpdateView and SetAllUpdated in UpdateViewProcessor::Visit
#endif

//----------------------------------------------------------------------
// class  : GraphicObject
//----------------------------------------------------------------------

typedef std::weak_ptr<GraphicObject> weakPtrGO;
typedef std::weak_ptr<const GraphicObject> weakPtrCGO;
typedef std::shared_ptr<GraphicObject> sharedPtrGO;
typedef std::shared_ptr<const GraphicObject> sharedPtrCGO;

template <typename Target>
struct shared_ptr_producer {

	template <typename ...Args>
	std::shared_ptr<Target> operator()(Args&& ...args)
	{
		m_Ptr->Init(std::forward<Args>(args)...);
		return std::move(m_Ptr);
	}
	std::shared_ptr<Target> m_Ptr;
};

template <typename Target, typename ...Args>
shared_ptr_producer<Target> make_shared_gr(Args&& ...args)
{
	return shared_ptr_producer<Target>{ std::make_shared<Target>(std::forward<Args>(args)...) };
}

class GraphicObject: public Actor, public enable_shared_from_this_base<GraphicObject>
{
	typedef Actor base_type;

public:
	GraphicObject(GraphicObject* owner);
	void Init() {}

	virtual GraphicClassFlags GetGraphicClassFlags() const { return GraphicClassFlags::None; }

	bool MustPushVisibility() const { return GetGraphicClassFlags() & GraphicClassFlags::PushVisibility; }
	bool MustClip          () const { return GetGraphicClassFlags() & GraphicClassFlags::ClipExtents;    }
	bool IsChildCovered    () const { return GetGraphicClassFlags() & GraphicClassFlags::ChildCovered;   }

	bool IsVisible          () const { return  m_State.Get(GOF_IsVisible);        }
	bool AllVisible         () const { return  m_State.Get(GOF_AllVisible);       }
	bool IsActive           () const { return  m_State.Get(GOF_Active);           }
	bool IsDrawn            () const { return  m_State.Get(GOF_IsDrawn);          }
	bool IsUpdated          () const { return  m_State.Get(GOF_IsUpdated);        }
	bool IgnoreActivation   () const { return  m_State.Get(GOF_IgnoreActivation); }
	bool IsTransparent      () const { return  m_State.Get(GOF_Transparent);      }
	bool MustFill           () const { return (!IsTransparent()) && (!IsChildCovered()); }
	bool ShowSelectedOnly   () const { return  m_State.Get(GOF_ShowSelectedOnly);  }

	bool AllUpdated         () const { MG_DEBUGCODE( CheckState(); ) return m_State.Get(GOF_AllUpdated); }

	void InvalidateDraw();
	void InvalidateView();
	void InvalidateViews();
	void UpdateView() const;

	void SetIsDrawn();
	virtual void SetDisconnected();

	void ClearAllUpdated();
	void SetAllUpdated();
	ToolButtonID GetControllerID() const;

#if defined(MG_DEBUG)
	virtual void CheckState() const;
	void CheckSubStates() const;
	void SetBlockInvalidateView(bool v) { dms_assert(m_State.Get(GOFD_BlockInvalidateView) != v); m_State.Set(GOFD_BlockInvalidateView, v); }
#endif

	weakPtrCGO GetOwner() const { return m_Owner; }
	weakPtrGO  GetOwner()       { return m_Owner; }

	bool IsOwnerOf(GraphicObject* obj) const;

	TreeItem* GetContext();

	void ClearContext();

//	define new virtuals for accessing sub-entries (composition pattern)
	virtual gr_elem_index  NrEntries() const;
	virtual GraphicObject* GetEntry(gr_elem_index i);
	virtual SharedStr GetCaption() const;
	const GraphicObject* GetConstEntry(gr_elem_index i) const { return const_cast<GraphicObject*>(this)->GetEntry(i); }

//	define new virtuals for invitation of GraphicObjects (visitor pattern)
	void ToggleVisibility();
	virtual void SetIsVisible(bool value);
	virtual void OnVisibilityChanged();

  	virtual GraphVisitState InviteGraphVistor(AbstrVisitor&);
  	virtual void SetActive(bool newState);

//	ShowSelectedOny stuff
	virtual bool ShowSelectedOnlyEnabled() const;
	virtual void UpdateShowSelOnly();
	void SetShowSelectedOnly(bool on);

//	define new virtuals for display of GraphicObjects
	virtual void DrawBackground(const GraphDrawer& d) const;
	virtual COLORREF GetBkColor() const;
	virtual FontSizeCategory GetFontSizeCategory() const;

	virtual bool Draw(GraphDrawer& d) const;
	virtual bool MouseEvent(MouseEventDispatcher& med);
	virtual bool OnKeyDown(UInt32 nVirtKey);
	virtual bool OnCommand(ToolButtonID id);
	virtual CommandStatus OnCommandEnable(ToolButtonID id)  const;
	virtual std::weak_ptr<DataView> GetDataView() const;


	virtual void Sync(TreeItem* viewContext, ShvSyncMode sm);
	void SyncShowSelOnly(ShvSyncMode sm);

	// various EventHandlers
	virtual void FillMenu(MouseEventDispatcher& med);

//	Size and Position
	virtual TRect CalcFullAbsRect   (const GraphVisitor&) const=0;
	virtual TRect GetCurrFullAbsRect(const GraphVisitor&) const=0;

	GRect GetClippedCurrFullAbsRect(const GraphVisitor& v) const;
	GRect GetDrawnFullAbsRect  () const;

	virtual GRect GetDrawnNettAbsRect() const { return GetDrawnFullAbsRect(); }
	virtual bool HasDefinedExtent() const { return true; }

	void SetOwner(GraphicObject* owner);
	void ClearOwner();

	static DmsColor GetDefaultTextColor      () { return CombineRGB(0, 0, 0); }
	static DmsColor GetDefaultBackColor      () { return COLORREF2DmsColor(TRANSPARENT_COLORREF); }

	void TranslateDrawnRect(const GRect& clipRect, const GPoint& delta);
	void ClipDrawnRect     (const GRect& clipRect);
	void ResizeDrawnRect   (const GRect& clipRect, GPoint delta, GPoint invarantLimit);

	//  helper functions
	bool PrepareDataOrUpdateViewLater(const TreeItem* item);

protected:
	virtual void OnSizeChanged();

//	override Actor interface
	void DoInvalidate() const override;
	TokenID GetXmlClassID() const override;

//	new callback interface
	virtual void OnChildSizeChanged();
	virtual void DoUpdateView();

private:
	void ClearDrawFlag();
	void MakeAllVisible();
	void UpdateAllVisible();
	bool HasShvCreator();

	friend void SyncState(GraphicObject* obj, TreeItem* context, TokenID stateID, UInt32 state, bool defaultValue, ShvSyncMode sm);
	friend class DataView;

	MG_DEBUGCODE( bool IsShvObj() const override { return true; } )

private:
	SharedPtr<TreeItem> m_ViewContext;
	weakPtrGO m_Owner;
	GRect     m_DrawnFullAbsRect; friend GraphDrawer; friend MovableObject;

	DECL_ABSTR(SHV_CALL, Class);
};


#endif // __SHV_GRAPHICOBJECT_H

