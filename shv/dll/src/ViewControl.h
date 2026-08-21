// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if !defined(__MG_VIEWCONTROL_H)
#define __MG_VIEWCONTROL_H

#include "ShvBase.h"

#include "MovableContainer.h"
class DataView;

//----------------------------------------------------------------------
// class  : MapControl
//----------------------------------------------------------------------

class ViewControl : public MovableContainer
{
	typedef MovableContainer base_type;
public:
	ViewControl(DataView* dv);
	void Init(DataView* dv);

//	override GraphicObject virtuals
	std::weak_ptr<DataView> GetDataView() const      override;
	bool OnCommand(ToolButtonID id)    override;
	bool OnKeyDown(UInt32 virtKey)     override;
	void SetClientSize(CrdPoint newSize) override;

protected: // new callbacks
	virtual void ProcessSize(CrdPoint newSize) =0;

private:
	std::weak_ptr<DataView> m_DataView;

	DECL_ABSTR(, Class)
};

#endif // __MG_VIEWCONTROL_H
