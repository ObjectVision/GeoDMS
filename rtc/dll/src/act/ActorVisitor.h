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
		if (!self)
			return AVS_Ready;
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
		if (self)
			lfunc(self);
		return AVS_Ready;
	}
	ProcLambda lfunc;
};

template <typename BoolLambda>
auto MakeDerivedBoolVisitor(BoolLambda&& func)
{
	return DerivedProcVisitor<BoolLambda>(std::forward<BoolLambda>(func));
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
