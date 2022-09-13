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

#ifndef __SHV_SELCARET_H
#define __SHV_SELCARET_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ShvBase.h"

#include "LockedIndexCollectorPtr.h"

//----------------------------------------------------------------------
// class  : SelCaret
//----------------------------------------------------------------------

#include "Region.h"

struct SelCaret : std::enable_shared_from_this<SelCaret>
{
	SelCaret(ViewPort* owner, const sel_caret_key& key, GridCoordPtr gridCoords);
	~SelCaret();

	void SetSelCaretRgn(      Region&& selCaret);
	void XOrSelCaretRgn(const Region& selCaret);
	const Region& GetSelCaretRgn() const { return m_SelCaretRgn; }

	void OnZoom();
	void OnScroll(const GPoint& delta);

	std::weak_ptr<ViewPort> GetOwner() const;

	void UpdateRgn(const Region& updateRgn);

private:
	Region UpdateRectImpl(const GRect& updateRect);
	void ForwardDiff(const Region& diff);

	std::weak_ptr<ViewPort>        m_Owner;	
	LockedIndexCollectorPtr        m_EntityIndexCollector;
	SharedPtr<const AbstrDataItem> m_SelAttr;
	GridCoordPtr                   m_GridCoords;
	Region                         m_SelCaretRgn;
	bool                           m_Ready;   // UpdateRgn was called after OnZoom of ctor, thus OnScroll should update incrementally

	friend class ViewPort;
};

#endif // __SHV_SELCARET_H

