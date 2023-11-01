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

#if !defined(__TIC_EXPLAIN_H)
#define __TIC_EXPLAIN_H

#include "TicBase.h"

using AbstrValueRef = OwningPtr<AbstrValue>;

namespace Explain {
	struct CalcExplImpl;
	typedef std::pair<SizeT, AbstrValueRef> CoordinateType;
	struct Context
	{
		CalcExplImpl*    m_CalcExpl;
		const AbstrUnit* m_Domain;
		CoordinateType*  m_Coordinate;
	};

	// magic constants
	const UInt32 MaxNrEntries = 6;
	const UInt32 MaxLevel     = 3;

	struct CalcExplImpl;
	struct CalcExplanations;

	struct NonStaticCalcExplanations
	{
		TIC_CALL NonStaticCalcExplanations(OutStreamBase& xmlOutStr, const AbstrDataItem* studyObject, SizeT index, CharPtr extraInfo);
		TIC_CALL bool ProcessQueue();
		TIC_CALL void WriteDescr();

		std::unique_ptr<CalcExplImpl>     m_Impl;
		std::unique_ptr<CalcExplanations> m_Interface;
		SharedDataItem   m_StudyObject;
	};


	using context_handle = std::unique_ptr<Explain::CalcExplImpl, void (*)(Explain::CalcExplImpl*)>;
	TIC_CALL context_handle CreateContext();
	TIC_CALL void AddQueueEntry(Explain::CalcExplImpl* explImpl, const AbstrUnit* domain, SizeT index);
	TIC_CALL bool AttrValueToXML(Explain::CalcExplImpl* context, const AbstrDataItem* studyObject, OutStreamBase* xmlOutStrPtr, SizeT index, CharPtr extraInfo, bool bShowHidden);
}


#endif //!defined(__TIC_EXPLAIN_H)
