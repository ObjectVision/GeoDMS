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

#if !defined(__TIC_FREEDATAMANAGER_H)
#define __TIC_FREEDATAMANAGER_H

#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"

#include "TreeItemSet.h"
#include "AbstrDataObject.h"
#include "AbstrDataItem.h"
#include "DataItemClass.h"

// ====================== Various Predicates

TIC_CALL UInt32 GetSwapFileMinSize();
#define KEEPMEM_MAX_NR_BYTES          128

TIC_CALL UInt32 ElemSize(const ValueClass* vc);
template <typename V>
inline UInt32 ElemSize()
{
	return ElemSize(ValueWrap<V>::GetStaticClass());
}


TIC_CALL UInt32 AbstrDataByteSize(const AbstrDataItem* adi);
TIC_CALL bool MustStorePersistent(const TreeItem* adi);
//TIC_CALL bool IsFileableSize(const AbstrDataItem* adi, SizeT nrBytes);
/* 
inline bool IsFileable(const AbstrDataItem* adi)
{
	return IsFileableSize(adi,  AbstrDataByteSize(adi) );
}

template <typename V>
inline bool IsFileable(const AbstrDataItem* adi, SizeT nrElem)
{
	dbg_assert(adi->CheckMetaInfoReadyOrPassor()); // PRECONDITION

	return IsFileableSize(adi, ElemSize<V>() * nrElem);
}
*/

#endif !defined(__TIC_FREEDATAMANAGER_H)
