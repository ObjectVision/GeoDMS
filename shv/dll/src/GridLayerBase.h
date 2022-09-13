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

#ifndef __SHV_GRIDLAYERBASE_H
#define __SHV_GRIDLAYERBASE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "GraphicLayer.h"
#include "GridCoord.h"

//----------------------------------------------------------------------
// class  : GridLayerBase
//----------------------------------------------------------------------

struct GridLayerBase : public GraphicLayer
{
	using GraphicLayer::GraphicLayer;
	typedef GraphicLayer base_type;

protected:
	GridCoordPtr GetGridCoordInfo(ViewPort* vp) const; friend class ViewPort;

	void FillLcMenu(MenuData& menuData) override;

	virtual void Zoom1To1() = 0;
	void Zoom1To1Caller() { Zoom1To1(); }
	mutable GridCoordPtr m_GridCoord;
};


#endif // __SHV_GRIDLAYERBASE_H

