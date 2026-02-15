// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_ACT_INVALIDATIONBLOCK_H)
#define __RTC_ACT_INVALIDATIONBLOCK_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "act/ActorEnums.h"
#include "act/Actor.h"

//----------------------------------------------------------------------
// class  : InvalidationBlock
//----------------------------------------------------------------------

struct InvalidationBlock
{
	RTC_CALL InvalidationBlock(const Actor* self);

	RTC_CALL void ProcessChange(); // request to Invalidate Local Changes 

	RTC_CALL ~InvalidationBlock();

	const Actor* GetChangingObj() const { return m_ChangingObj; };

private:
	const Actor* m_ChangingObj;
	bool         m_OldBlockState;
};


#endif // __RTC_ACT_INVALIDATIONBLOCK_H
