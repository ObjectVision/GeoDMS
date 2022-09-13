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
#include "TicPCH.h"
#pragma hdrstop

#include "LockLevels.h"

#include "TreeItemFlags.h"
#include "mci/ValueComposition.h"

leveled_critical_section sc_TreeItemFlags(item_level_type(0), ord_level_type::TreeItemFlags, "TreeItemFlags");


void treeitem_flag_set::SetValueComposition(ValueComposition vc)
{ 
	leveled_critical_section::scoped_lock lockSetting(sc_TreeItemFlags);

	dms_assert(UInt32(vc) <= VC_MASK);
	dms_assert(vc < ValueComposition::Count); // 3=VC_Unknown; valid values are ValueComposition::Single, ValueComposition::Polygon, ValueComposition::Sequence
	Clear(VC_MASK << VC2DSF_SHIFT);
	Set  (UInt32(vc) << VC2DSF_SHIFT); 
}

void treeitem_flag_set::SetDataCheckMode(DataCheckMode dcm)
{ 
	leveled_critical_section::scoped_lock lockSetting(sc_TreeItemFlags);

	dms_assert(!Get(DSF_ValuesChecked));
	dms_assert(UInt32(dcm) <= DCM_MASK);
	Clear(DCM_MASK << DCM2DSF_SHIFT);
	Set  ((dcm     << DCM2DSF_SHIFT)|DSF_ValuesChecked);

}
