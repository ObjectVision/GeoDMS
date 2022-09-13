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

#if !defined(__TIC_CHECKEDDOMAIN_H)
#define __TIC_CHECKEDDOMAIN_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------


#include "mci/CompositeCast.h"
#include "mci/ValueWrap.h"
#include "mci/ValueClass.h"

#include "AbstrDataItem.h"
#include "Unit.h"

//----------------------------------------------------------------------
// function checked_domain
//----------------------------------------------------------------------

template <typename D>
const Unit<D>* checked_domain(const TreeItem* ti)
{
	dms_assert(ti);
	const AbstrDataItem* adi = checked_cast<const AbstrDataItem*>(ti);
	const AbstrUnit*     au  = adi->GetAbstrDomainUnit();
	dms_assert(au);
	const Unit<D>* u = const_unit_dynacast<D>(au);
	if (!u) 
		ti->throwItemErrorF(
			"DataItem with DomainUnit of type %s expected, but Domain is of type %s", 
			ValueWrap<D>::GetStaticClass()->GetName().c_str(),
			au->GetClsName().c_str()
		);
	return u;
}


#endif // __TIC_CHECKEDDOMAIN_H
