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

#include "CalcDestroyer.h"

#include "AbstrCalculator.h"
#include "AbstrDataItem.h"
/* REMOVE
CalcDestroyer::CalcDestroyer(const TreeItem* item, bool canDestroyCalc)
	:	m_OldChecker   (std::move(item->mc_IntegrityChecker))
	,	m_OldRefItem   (std::move(item->mc_RefItem))
	,	m_HasInterest  (item->GetInterestCount())
{
	if (canDestroyCalc)
		m_OldCalculator = std::move(item->mc_Calculator);
	dms_assert(item->mc_Calculator == nullptr || !canDestroyCalc);
	dms_assert(item->mc_RefItem    == nullptr);
	if (IsDataItem(item)) // REMOVE, Move to overridden 
	{
		item->m_State.Clear(ASF_DataReadableDefined);
		item->ClearTSF(DSF_ValuesChecked);
	}
	
}

CalcDestroyer::~CalcDestroyer()
{
	if (m_HasInterest)
	{
		if (m_OldChecker)
			m_OldChecker->DecInterestCount();
		if (m_OldCalculator)
			m_OldCalculator->DecInterestCount();

		if (m_OldRefItem)
			m_OldRefItem->DecInterestCount();
	}
}

*/