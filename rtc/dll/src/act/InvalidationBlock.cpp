// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "act/InvalidationBlock.h"
#include "act/Actor.h"

//----------------------------------------------------------------------
// class  : InvalidationBlock
//----------------------------------------------------------------------

InvalidationBlock::InvalidationBlock(const Actor* self)
	:	m_ChangingObj(self)
	,	m_OldBlockState(self->m_State.HasInvalidationBlock())
{
	dms_assert(self);
	self->DetermineState();  // let other changes of suppliers such as ROI invalidate the ViewPort if changes were made prior to the locked change
	self->m_State.SetInvalidationBlock(); // block invalidation for the current change
}

void InvalidationBlock::ProcessChange() // request to Invalidate Local Changes 
{
	assert(m_ChangingObj);
	m_ChangingObj->GetLastChangeTS(); // process the changes while actor_flag_set::AF_BlockChange is set
}

InvalidationBlock::~InvalidationBlock()
{
	dms_assert(m_ChangingObj);
	dms_assert(m_ChangingObj->m_State.HasInvalidationBlock());
	if (!m_OldBlockState)
		m_ChangingObj->m_State.ClearInvalidationBlock();// go to original state
}

