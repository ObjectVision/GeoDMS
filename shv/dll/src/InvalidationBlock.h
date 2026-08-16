// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  InvalidationBlock: RAII guard that, for the duration of a local change to
 *  an Actor (a view component changing its own state), suppresses the actor's
 *  invalidation propagation and restores the previous block state on
 *  destruction; ProcessChange() requests invalidation of the local changes.
 *  Moved here from rtc/act (2026-08): shv was its only consumer.
 */

#if !defined(__SHV_INVALIDATIONBLOCK_H)
#define __SHV_INVALIDATIONBLOCK_H

#include "act/ActorEnums.h"
#include "act/Actor.h"

struct InvalidationBlock
{
	InvalidationBlock(const Actor* self);

	void ProcessChange(); // request to Invalidate Local Changes

	~InvalidationBlock();

	const Actor* GetChangingObj() const { return m_ChangingObj; };

private:
	const Actor* m_ChangingObj;
	bool         m_OldBlockState;
};

#endif // __SHV_INVALIDATIONBLOCK_H
