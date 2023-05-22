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

/*
 *  Description : Actor serves as basis class for the Update Mechanism
 */

#if !defined(__RTC_ACT_ACTOR_H)
#define __RTC_ACT_ACTOR_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "act/ActorEnums.h"
#include "ptr/InterestHolders.h"
#include "ptr/PersistentSharedObj.h"
#include "ptr/SharedStr.h"

#include <any>
struct ActorVisitor;
struct SupplInterestListPtr;

struct ErrMsg;
using ErrMsgPtr = std::shared_ptr<ErrMsg>;

//  -----------------------------------------------------------------------
//  struct Actor interface
//  -----------------------------------------------------------------------

struct Actor : PersistentSharedObj
{
	RTC_CALL Actor();
	RTC_CALL virtual ~Actor();

	//	Update the object if Invalidated (inplies a SuppliersUpdate())
	//	SuspendibleUpdate the object if it is changed
	RTC_CALL virtual ActorVisitState SuspendibleUpdate(ProgressState ps) const;

	//	Calls UpdateMetaInfo, Locks the MustSuspend() interrupter and then calls Update
	RTC_CALL void CertainUpdate(ProgressState ps) const;

	bool Update(ProgressState ps, bool mustUpdate) const
	{
		if (!mustUpdate)
			return SuspendibleUpdate(ps) != AVS_SuspendedOrFailed;
		CertainUpdate(ps);
		return true;
	}

	//	Purpose: Mark this and its consumers as Invalidated
	RTC_CALL void Invalidate ();

	// Purpose: mark items as valid when not updating (for instance because of the apply operator)
	RTC_CALL void MarkTS(TimeStamp ts) const;
	RTC_CALL void SetProgressAt(ProgressState ps, TimeStamp ts) const; 
	RTC_CALL virtual void SetProgress(ProgressState ps) const; // set lastChangeTS to ChangeSource TimeStamp
	RTC_CALL void RaiseProgress(ProgressState ps = PS_Committed) const { if (m_State.GetProgress() < ps) SetProgress(ps); }
	RTC_CALL bool Was  (ProgressState ps) const; //	true if m_State.GetProgress() >= ps
	RTC_CALL bool Is   (ProgressState ps) const; //	true if still valid. Is encapsulates GetState unless it wasn't there yet
	RTC_CALL bool IsFailed () const;  //	true if erroneous. IsFailed encapsulates GetState.
	FailType GetFailType() const { return m_State.GetFailType(); }
	RTC_CALL bool IsFailed (FailType ps) const;  //	true if erroneous. IsFailed encapsulates GetState.
	         bool IsFailed(bool doCalc) const { return IsFailed(doCalc ? FR_Data : FR_MetaInfo); }
			 bool IsDataFailed () const { return IsFailed(FR_Data); }
	RTC_CALL bool WasFailed() const { return m_State.IsFailed(); }
	RTC_CALL bool WasFailed(FailType fr) const;
	bool WasFailed(ProgressState ps) const; // DON'T CALL THIS ONE
	         bool WasFailed(bool doCalc) const { return WasFailed(doCalc ? FR_Data : FR_MetaInfo); }
			 bool WasValid (ProgressState ps = PS_Committed) const { return m_State.GetProgress() >= ps && !m_State.IsFailed();  }

	RTC_CALL void TriggerEvaluation() const;

	RTC_CALL ErrMsgPtr GetFailReason() const;
	RTC_CALL void ClearFail() const;

	//	Calculates and retrieves the time of last change in this or its suppliers.
	RTC_CALL TimeStamp GetLastChangeTS () const;
	TimeStamp LastChangeTS () const { return m_LastChangeTS; } // for persistency only

	//	Update all actors in the global actor list.

	RTC_CALL virtual void AssertPropChangeRights(CharPtr changeWhat) const;
	RTC_CALL virtual void AssertDataChangeRights(CharPtr changeWhat) const;

	// state info
	bool IsPassor() const { return m_State.IsPassor(); }
	RTC_CALL void SetPassor();
	bool IsPassorOrChecked() const { return IsPassor() || m_State.IsDeterminingCheck(); }

	//	Override this method to identify suppliers.
	RTC_CALL virtual ActorVisitState VisitSuppliers    (SupplierVisitFlag svf, const ActorVisitor& visitor) const;

	[[noreturn]] RTC_CALL void ThrowFail(ErrMsgPtr why, FailType ft) const;
	[[noreturn]] RTC_CALL void ThrowFail(SharedStr str, FailType ft) const;
	[[noreturn]] RTC_CALL void ThrowFail(CharPtr str, FailType ft) const;
	[[noreturn]] RTC_CALL void ThrowFail(const Actor* src, FailType ft) const;
	[[noreturn]] RTC_CALL void ThrowFail(const Actor* src) const;
	[[noreturn]] RTC_CALL void ThrowFail() const;

	RTC_CALL void Fail(WeakStr why, FailType ft) const;
	RTC_CALL void Fail(CharPtr str, FailType ft) const;
	RTC_CALL void Fail(const Actor* src, FailType ft) const;
	RTC_CALL void Fail(const Actor* src) const;
	RTC_CALL void CatchFail(FailType ft) const;

	//	DetermineState determines if this should be invalidated.
	RTC_CALL void DetermineState () const;
	RTC_CALL ActorVisitState UpdateSuppliers(ProgressState ps) const;

protected:
	//	Override this method to implement the working of an SuspendibleUpdate"
	//	function. For example: sort(Data).
	RTC_CALL virtual bool MustApplyImpl() const;

	RTC_CALL virtual ActorVisitState DoUpdate(ProgressState ps);
	RTC_CALL virtual bool DoFail(ErrMsgPtr msg, FailType ft) const;

	//	Override this method to implement reset behaviour. For example:
	//	aMapView->InvalidateLayer(this).
	RTC_CALL virtual void DoInvalidate () const;
	RTC_CALL virtual void UpdateMetaInfo() const;

	RTC_CALL void UpdateSupplMetaInfo() const;

	//	Collect change information from suppliers or a change triggered by specific
	//	derivation. Extend this method to implement specific validity checking. For
	//	example: compare current file-stamp with last.
	RTC_CALL virtual TimeStamp DetermineLastSupplierChange(ErrMsgPtr& failReason, FailType& failType) const; // noexcept;

	RTC_CALL void InvalidateAt(TimeStamp invalidate_ts) const;

public:
	RTC_CALL void IncInterestCount() const;
	RTC_CALL garbage_t DecInterestCount() const noexcept;
	auto GetInterestCount() const noexcept { return m_InterestCount; }
	RTC_CALL bool DoesHaveSupplInterest() const noexcept;

	RTC_CALL SupplInterestListPtr GetSupplInterest() const;
	RTC_CALL SharedActorInterestPtr GetInterestPtrOrNull() const;


protected:
	RTC_CALL virtual void StartInterest() const;
	RTC_CALL virtual garbage_t StopInterest () const noexcept;

public:
	RTC_CALL void StartSupplInterest   () const;          friend struct FuncDC;
	RTC_CALL garbage_t StopSupplInterest() const noexcept; friend struct AbstrOperGroup;

	// Data Members
public:
	mutable actor_flag_set m_State;
	mutable TimeStamp      m_LastChangeTS = 0;
	mutable TimeStamp      m_LastGetStateTS = 0;

#if defined(MG_ITEMLEVEL)
	mutable item_level_type m_ItemLevel = item_level_type(0);
#endif

protected:
	mutable interest_count_t m_InterestCount = 0;
private:

	#if defined(MG_DEBUG_DATA)
		static UInt32 sd_CreationCount;
		static UInt32 sd_InstanceCount;

		static UInt32 sd_InvalidationProtectCount;
	#endif	// MG_DEBUG

public: // status info functions, Debug statistics
	RTC_CALL static UInt32 GetNrInstances();
	RTC_CALL static UInt32 GetNrCreations();


	struct UpdateLock //: ::UpdateLock
	{
		RTC_CALL UpdateLock(const Actor* act, actor_flag_set::TransState ps);
		RTC_CALL ~UpdateLock();

		const Actor*               m_Actor;
		actor_flag_set::TransState m_TransState;
	};
protected:

#if defined(MG_DEBUG)
	RTC_CALL virtual bool IsShvObj() const; 
	RTC_CALL virtual SharedStr GetExpr() const { return SharedStr(); }
#endif

	friend struct OperationContext;
	friend struct DetermineStateLock;
};

#if defined(MG_ITEMLEVEL)
RTC_CALL item_level_type GetItemLevel(const Actor* act);
#endif

RTC_CALL bool WasInFailed(const Actor* a);

#endif // __RTC_ACT_ACTOR_H
