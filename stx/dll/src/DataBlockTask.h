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

#if !defined( __STX_DATABLOCKTASK_H)
#define __STX_DATABLOCKTASK_H

#include "DataBlockProd.h"
#include "AbstrCalculator.h"
#include "AbstrDataItem.h"

struct DataBlockTask  : AbstrCalculator // TODO G8: RENAME TO DataBlockExprKey
{
	typedef AbstrCalculator base_type;
	SYNTAX_CALL DataBlockTask(AbstrDataItem* adiCurr, 
		CharPtr begin, CharPtr end, row_id nrElems
	);
	
	SYNTAX_CALL DataBlockTask(AbstrDataItem* tiCurr, const DataBlockTask& src);
	virtual ~DataBlockTask();

	MetaInfo GetMetaInfo() const override; // TODO G8: consider non polymorphic integration with AbstrCalculator
	bool IsSourceRef() const override { return false; }

	bool IsDataBlock() const override { return true; }
	SharedStr GetExpr () const override { return SharedStr(m_DataBlock->GetStrnBeg(), m_DataBlock->GetStrnEnd()); }

private: friend struct DataBlockProd;
	const AbstrDataItem* GetContext() const { return AsDataItem(GetHolder()); }
	row_id         GetNrElems() const { return m_NrElems; }

	LispRef m_DataBlock;
	row_id  m_NrElems;
};

#endif
