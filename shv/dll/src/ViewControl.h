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
	void SetClientSize(TPoint newSize) override;

protected: // new callbacks
	virtual void ProcessSize(TPoint newSize) =0;

private:
	std::weak_ptr<DataView> m_DataView;

	DECL_ABSTR(SHV_CALL, Class);
};

#endif // __MG_VIEWCONTROL_H
