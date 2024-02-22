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
#if !defined(__TIC_DATASTOREMANAGERCALLER_H)
#define __TIC_DATASTOREMANAGERCALLER_H

#include "SessionData.h"

struct SafeFileWriterArray;
struct SessionData;

//----------------------------------------------------------------------
// struct DataStoreManager
//----------------------------------------------------------------------

namespace DSM 
{

	inline std::shared_ptr<SessionData> Curr() { return SessionData::Curr(); }
	inline bool IsCancelling() { auto curr = Curr();  return curr && curr->IsCancelling(); }
	TIC_CALL void CancelIfOutOfInterest(const TreeItem* item = nullptr);
	[[noreturn]] void CancelOrThrow(const TreeItem* item);

	inline auto GetSafeFileWriterArray() -> std::shared_ptr< SafeFileWriterArray>
	{
		auto curr = Curr();
		if (!curr)
			return {};
		return curr->GetSafeFileWriterArray();
	}
}; // namespace DSM 

#endif // __TIC_DATASTOREMANAGERCALLER_H
