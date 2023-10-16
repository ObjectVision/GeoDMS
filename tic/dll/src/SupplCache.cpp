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

#include "SupplCache.h"
#include "TreeItemProps.h"

#include "act/UpdateMark.h"
#include "set/VectorFunc.h"

#include <algorithm>

#include "AbstrCalculator.h"

//----------------------------------------------------------------------
// class  : SupplCache
//----------------------------------------------------------------------

SupplCache::SupplCache()
	:	m_NrConfigured(0)
	,	m_IsDirty   (false)        // default: no suppliers
{}

void SupplCache::Reset()
{
	m_SupplArray.reset();
	m_IsDirty = true;
}

void SupplCache::SetSupplString(WeakStr val)
{
	if (!m_IsDirty)
		Reset();
	m_strConfigured = val;
}

void SupplCache::BuildSet(const TreeItem* context) const
{
	dms_assert(context);
	dms_assert(m_IsDirty);

	SharedStr strConfigured = TreeItemPropertyValue(context, explicitSupplPropDefPtr);

	CharPtr
		iBegin = strConfigured.begin(), 
		iEnd   = strConfigured.send();

	m_NrConfigured = 0;

	while (true)
	{
		CharPtr
			iFirstEnd = std::find(iBegin, iEnd, ';');
		if (iFirstEnd != iBegin)
		{
			++m_NrConfigured;
			dms_assert(m_NrConfigured);
		}
		if (iFirstEnd == iEnd)
			break;
		iBegin = iFirstEnd + 1;
	}
	iBegin = strConfigured.begin();

	ActorCRefArray newSupplArray(new ActorCRef[m_NrConfigured]);
	if (!context->GetTreeParent())
		context->throwItemError("ExplicitSuppliers property cannot be set on root items");
	context = context->GetTreeParent();
	UInt32 i=0;
	while (true)
	{
		CharPtr iFirstEnd = std::find(iBegin, iEnd, ';');
		if (iFirstEnd != iBegin)
		{
			CharPtrRange explicitSupplierName(iBegin, iFirstEnd);
			Trim(explicitSupplierName);
			const TreeItem* suppl = context->FindItem(explicitSupplierName);
			if (!suppl)
				context->throwItemErrorF("ExplicitSupplier %s not found", SingleQuote(explicitSupplierName.first, explicitSupplierName.second));

			assert(i<m_NrConfigured);
			newSupplArray[i++] = ActorCRef(suppl);
		}
		if (iFirstEnd == iEnd)
			break;
		iBegin = iFirstEnd + 1;
	}
	assert(i == m_NrConfigured);
	newSupplArray.swap(m_SupplArray);

	m_IsDirty = false;
}
