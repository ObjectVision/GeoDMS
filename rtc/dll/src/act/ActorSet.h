// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__ACTORSET_H)
#define __ACTORSET_H

#if defined(MG_DEBUG_UPDATESOURCE)

// *****************************************************************************
// Section:     ActorSet
// *****************************************************************************

struct Actor;

struct ActorVectorType : std::vector<const Actor*> {};

// *****************************************************************************
// Section:     MG_DEBUG_UPDATESOURCE
// *****************************************************************************

struct SupplInclusionTester : private ActorVectorType
{
	RTC_CALL SupplInclusionTester(const Actor* actor);
	RTC_CALL ~SupplInclusionTester();

	static bool ActiveDoesContain(const Actor* actor);

private:
   SupplInclusionTester* m_Prev;
};

#endif // defined(MG_DEBUG_UPDATESOURCE)

#endif // __ACTORSET_H
