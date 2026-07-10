// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_DATASTOREMANAGERCALLER_H)
#define __TIC_DATASTOREMANAGERCALLER_H

#include "SessionData.h"

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

}; // namespace DSM 

#endif // __TIC_DATASTOREMANAGERCALLER_H
