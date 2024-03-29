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

#if !defined(__STG_IMPL_H)
#define __STG_IMPL_H

#include "StgBase.h"

// ------------------------------------------------------------------------
//
// Helper functions
//
// ------------------------------------------------------------------------

STGDLL_CALL const ValueClass* GetStreamType(const AbstrDataObject* adi);
STGDLL_CALL const ValueClass* GetStreamType(const AbstrDataItem* adi);

const AbstrUnit*  StorageHolder_GetTableDomain(const TreeItem* storageHolder);
bool              TableDomain_IsAttr(const AbstrUnit* domain, const AbstrDataItem* adi);

STGDLL_CALL SharedUnit FindProjectionRef (const TreeItem* storageHolder, const AbstrUnit* uDomain);
STGDLL_CALL SharedUnit FindProjectionBase(const TreeItem* storageHolder, const AbstrUnit* uDomain);

bool WriteGeoRefFile(const AbstrDataItem* diGrid, WeakStr geoRefFileName);
void GetImageToWorldTransformFromFile(TreeItem* storageHolder, WeakStr geoRefFileName);


#endif __STG_IMPL_H
