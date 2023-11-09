// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_ACT_ENUMS_H)
#define __RTC_ACT_ENUMS_H

#include "dbg/Check.h"
#include "geo/mpf.h"

#if defined(_MSC_VER)
#pragma warning( disable: 26812 ) // enum type 'xxx' is unscoped. Prefer 'enum class' over 'enum'
#endif

//  -----------------------------------------------------------------------
//  ActorVisitor enum
//  -----------------------------------------------------------------------

enum ActorVisitState : unsigned char // see also the DIFFERENT used GraphVisitState
{
	AVS_SuspendedOrFailed = false,
	AVS_Ready = true,    // task completed, continue with next work
};

//----------------------------------------------------------------------
// generic facilities for status bits
//----------------------------------------------------------------------

struct flag_set {
	flag_set(UInt32 dw=0) noexcept 
		: m_DW(dw) 
	{}

	RTC_CALL UInt32 GetBits(UInt32 sf) const;
	RTC_CALL void   Set    (UInt32 sf);
	RTC_CALL void   Clear  (UInt32 sf);
	RTC_CALL void   SetBits(UInt32 sf, UInt32 values);
	RTC_CALL void   Toggle (UInt32 sf);

	bool Get(UInt32 sf) const { return GetBits(sf) != 0; }
	void Set(UInt32 sf, bool value) { if (value) Set(sf); else Clear(sf); }

private:
	UInt32 m_DW;
};

template <bool VALUE> void SetV(flag_set& fs, UInt32 sf);
template <> inline void SetV<true >(flag_set& fs, UInt32 sf) { fs.Set(sf); }
template <> inline void SetV<false>(flag_set& fs, UInt32 sf) { fs.Clear(sf); }

template<UInt32 MASK>
struct auto_flag
{
	auto_flag(flag_set& flags, bool autoVal = true)
		:	m_Flags(flags)
		,	m_OldFlags(flags)
	{
		flags.Set(MASK, autoVal); 
	}
	~auto_flag() { m_Flags.SetBits(MASK, m_OldFlags.GetBits(MASK)); }

private:
	flag_set& m_Flags;
	flag_set  m_OldFlags;
};

template<UInt32 MASK, bool VALUE = true>
struct auto_flag_recursion_lock
{
	auto_flag_recursion_lock(flag_set& flags)
		:	m_Flags(flags) 
	{
		dms_assert(flags.Get(MASK) != VALUE); 
		SetV<VALUE>(flags, MASK);
	}
	~auto_flag_recursion_lock()
	{
		dms_assert(m_Flags.Get(MASK) == VALUE);
		SetV<!VALUE>(m_Flags, MASK);
	}

private:
	flag_set& m_Flags;
};

template<UInt32 MASK>
struct auto_flag_recursion_check
{
	auto_flag_recursion_check(flag_set& flags) : m_Flags(flags) { MG_CHECK(!flags.Get(MASK)); flags.Set(MASK); }
	~auto_flag_recursion_check() { MG_CHECK(m_Flags.Get(MASK)); m_Flags.Clear(MASK); }

private:
	flag_set& m_Flags;
};

//----------------------------------------------------------------------
// enums ProgressState
//----------------------------------------------------------------------
// See comments in ActorEnums.txt

enum ProgressState : UInt32 {
	PS_None      = 0,
	PS_MetaInfo  = 1,
	PS_Validated = 2,
	PS_Committed = 3,

	PS_Mask      = 3
};

//----------------------------------------------------------------------
// enums for Actor types
//----------------------------------------------------------------------

enum FailType : UInt32 {
	FR_None      =   0,
	FR_Determine =   1 * 4,
	FR_MetaInfo  =   2 * 4,
	FR_Data      =   3 * 4,
	FR_Validate  =   4 * 4,
	FR_Committed =   5 * 4,

	AF_FailedMask =  7 * 4
};

enum class SupplierVisitFlag;

inline bool Test(SupplierVisitFlag svf, SupplierVisitFlag tv) { return unsigned(svf) & unsigned(tv); }

struct actor_flag_set : flag_set
{
public:
	enum TransState : UInt32 {
		AF_TransientBase     = 0x0020,

		AF_Committing        = 1 * AF_TransientBase,
		AF_Validating        = 2 * AF_TransientBase,
		AF_CalculatingData   = 3 * AF_TransientBase,
		AF_ChangingInterest  = 4 * AF_TransientBase,
		AF_UpdatingMetaInfo  = 5 * AF_TransientBase,
		AF_DeterminingState  = 6 * AF_TransientBase,
		AF_DeterminingCheck  = 7 * AF_TransientBase,

		AF_TransientMask     = 0x07 * AF_TransientBase,
	};

	enum : UInt32 {
		AF_IsPassor          = 0x0100,
		AF_InvalidationBlock = 0x0200,  // temporary flag,set from (UpdateView and LayerOperations) -> ChangeLock, to not call DoInvalidate and thus avoid InvalidateView, InvalidateDraw, InvalidateCaches, etc
		AF_SupplInterest     = 0x0400, 
		AF_CommitInterest    = 0x0800,

		#if defined(MG_DEBUG)
		AFD_ProcessCounted   = 0x1000,
		AFD_PivotElem        = 0x2000,
		#endif
		AF_Next              = 0x4000,
		AF_Mask              = AF_Next -1
	};	


	FailType GetFailType() const { return FailType( GetBits(AF_FailedMask) ); }
	bool IsFailed     () const   { return GetBits(AF_FailedMask); }
	bool IsFailed     (FailType ft) const  { dms_assert((ft & AF_FailedMask) == ft); return IsFailed() && GetBits(AF_FailedMask) <= ft; }
	bool IsDataFailed () const  { return IsFailed(FR_Data); }
	void ClearFailed() { Clear(AF_FailedMask); }

	ProgressState GetProgress() const  { return ProgressState(GetBits(PS_Mask)); }
	void SetProgress(ProgressState sf) 
	{ 
		dms_assert(sf <= PS_Mask); 
		SetBits(PS_Mask, sf); 
	}
	void SetFailure(FailType ft) 
	{ 
		dms_assert((ft & AF_FailedMask) == ft); 
		SetBits(AF_FailedMask, ft); 
	}

	TransState GetTransState() const { return TransState( GetBits(AF_TransientMask) ); }
	bool   IsInTrans    () const { return GetTransState() > AF_IsPassor; }
	void   SetTransState(TransState state) { SetBits(AF_TransientMask, state); }
	void   ClearTransState() { Clear(AF_TransientMask); }

	void SetPassor  () { Set  (AF_IsPassor); }
	void ClearPassor() { Clear(AF_IsPassor); }
	bool IsPassor   () const { return Get(AF_IsPassor); }

	bool IsDeterminingCheck() const { return GetTransState()==AF_DeterminingCheck; }
	bool IsDeterminingState() const { return GetTransState()==AF_DeterminingState; }
	bool IsUpdatingMetaInfo() const { return GetTransState()==AF_UpdatingMetaInfo; }
	bool IsCalculatingData () const { return GetTransState()==AF_CalculatingData ; }
	bool IsValidating      () const { return GetTransState()==AF_Validating      ; }
	bool IsCommitting      () const { return GetTransState()==AF_Committing      ; }

	void SetInvalidationBlock()       { Set  (AF_InvalidationBlock); }
	void ClearInvalidationBlock()     { Clear(AF_InvalidationBlock); }
	bool HasInvalidationBlock() const { return Get(AF_InvalidationBlock); }
};

RTC_CALL CharPtr FailStateName(UInt32 fs);

#endif // __RTC_ACT_ENUMS_H
