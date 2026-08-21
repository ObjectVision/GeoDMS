// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __SHV_WRAPPER_H
#define __SHV_WRAPPER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "GraphicContainer.h"
#include "MovableObject.h"

const int LINE_SCROLL_STEP =  20;
const int PAGE_SCROLL_STEP = 100;

//----------------------------------------------------------------------
// class  : Wrapper
//----------------------------------------------------------------------

class Wrapper: public MovableObject
{
	typedef MovableObject base_type;

protected:
	Wrapper(MovableObject* owner, DataView* dv, CharPtr caption);
	
	GraphicClassFlags GetGraphicClassFlags() const override { return GraphicClassFlags::PushVisibility| GraphicClassFlags::ClipExtents; }

public:

// props
	void SetContents(sharedPtrGO contents);
	void ClearContents();


	GraphicObject*       GetContents()       { return m_Contents.get(); }
	const GraphicObject* GetContents() const { return m_Contents.get(); }

	SharedStr GetCaption() const override;

//	overrid GraphicObject interface for composition of GraphicObjects (composition pattern)
	virtual SizeT  NrEntries() const override;
	GraphicObject* GetEntry(SizeT i) override;
	std::weak_ptr<DataView> GetDataView() const override { return m_DataView; }

//	override virtual methods of GraphicObject
	void Sync(TreeItem* context, ShvSyncMode sm) override;

	bool OnCommand(ToolButtonID id) override;

	virtual void Export() = 0;

protected:

#if defined(MG_DEBUG)
	bool DataViewOK() const;
#endif
private:
	std::weak_ptr<DataView> m_DataView;
	sharedPtrGO m_Contents;
	SharedStr   m_Caption;

	DECL_ABSTR(, Class)
};



#endif // __SHV_WRAPPER_H

