// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_ACT_ACTORVISITOR_H)
#define __RTC_ACT_ACTORVISITOR_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "act/ActorEnums.h"
#include "act/Actor.h"

//  -----------------------------------------------------------------------
//  struct ActorVisitor interface
//  -----------------------------------------------------------------------

struct ActorVisitor
{
	virtual ActorVisitState operator ()(const Actor* supplier) const= 0;

	RTC_CALL ActorVisitState Visit(const Actor* supplier) const;
};


//  -----------------------------------------------------------------------
//  helper to call with lambda expression
//  -----------------------------------------------------------------------

template <typename BoolLambda>
struct DerivedBoolVisitor : ActorVisitor
{
	DerivedBoolVisitor(BoolLambda&& rhs) : lfunc(std::move(rhs)) {}
	ActorVisitState operator() (const Actor* self) const override
	{
		assert(self);
		return lfunc(self) ? AVS_Ready : AVS_SuspendedOrFailed;
	}
	BoolLambda lfunc;
};

template <typename ProcLambda>
struct DerivedProcVisitor : ActorVisitor
{
	DerivedProcVisitor(ProcLambda&& rhs) : lfunc(std::forward<ProcLambda>(rhs)) {}

	ActorVisitState operator() (const Actor* self) const override
	{
		assert(self);
		lfunc(self);
		return AVS_Ready;
	}
	ProcLambda lfunc;
};

template <typename BoolLambda>
auto MakeDerivedBoolVisitor(BoolLambda&& func)
{
	return DerivedBoolVisitor<BoolLambda>(std::forward<BoolLambda>(func));
}

template <typename ProcLambda>
auto MakeDerivedProcVisitor(ProcLambda&& func)
{
	return DerivedProcVisitor<ProcLambda>(std::forward<ProcLambda>(func));
}


template <typename BoolLambda>
ActorVisitState VisitSupplBoolImpl(const Actor* self, SupplierVisitFlag svf, BoolLambda&& lfunc)
{
	return self->VisitSuppliers(svf, MakeDerivedBoolVisitor(std::forward<BoolLambda>(lfunc)));
}

template <typename ProcLambda>
ActorVisitState VisitSupplProcImpl(const Actor* self, SupplierVisitFlag svf, ProcLambda&& lfunc)
{
	return self->VisitSuppliers(svf, MakeDerivedProcVisitor(std::forward<ProcLambda>(lfunc)));
}

#endif // __RTC_ACT_ACTORVISITOR_H
