// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  The diagnostics prelude: the check/assert macro
 *  families (dms_assert, dbg_assert, MG_CHECK, MG_PRECONDITION, ...), the
 *  throw* error entry points (throwErrorD/F, throwDmsErrD/F, throwNYI,
 *  throwIllegalAbstract, ...) and the report* tracing functions. A "check"
 *  is a conditional throw of an exception; a "debug check" is a check that
 *  is only implemented in a debug build.
 */

#if !defined(__DBG_DIAGNOSTICS_H)
#define __DBG_DIAGNOSTICS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "RtcBase.h"
#include "utl/MgFormat.h"
#include "dbg/SeverityType.h"
class Object;
struct TokenID;
struct CharPtrRange;

//----------------------------------------------------------------------
// Statements like:
//		#pragma reminder("Fix this problem!")
// Which will cause messages like:
//		C:\Source\Project\main.cpp(47): Reminder: Fix this problem!
// to show up during compiles.  Note that you can NOT use the
// words "error" or "warning" in your reminders, since it will
// make the IDE think it should abort execution.  You can double
// click on these messages and jump to the line in question.
//----------------------------------------------------------------------

#define $Stringize( L )     #L
#define $MakeString( M, L ) M(L)
#define reminder(MSG)       message( __FILE__ "(" $MakeString( $Stringize, __LINE__ ) ") : Reminder: " MSG )

//----------------------------------------------------------------------
// fix assertion problem by safe implementation of dms_assert if neccesary
//----------------------------------------------------------------------

void dms_check_failed(CharPtr msg, CharPtr fileName, unsigned line);
#define dms_check(EXPR) (void)( (!!(EXPR)) || (dms_check_failed(#EXPR, __FILE__, __LINE__), 0) )

void dms_assertion_failed(CharPtr msg, CharPtr fileName, unsigned line);

#if defined(CC_FIX_ASSERT)

#define dms_assert_impl(EXPR) (void)( (!!(EXPR)) || (dms_assertion_failed(#EXPR, __FILE__, __LINE__), 0) )

#else

#include <assert.h>
#define dms_assert_impl(EXPR) assert(EXPR);

#endif

#if defined(MG_DEBUG)

struct DebugOnlyLock
{
	RTC_CALL DebugOnlyLock(CharPtr assertStr, CharPtr file, UInt32 line);
	RTC_CALL ~DebugOnlyLock();
	RTC_CALL static bool IsLocked();
	RTC_CALL static void CheckNoLocks();

private:
	CharPtr m_AssertStr, m_File;
	UInt32 m_Line;
	DebugOnlyLock* m_PrevLock;
};

#define dms_check_not_debugonly { DebugOnlyLock::CheckNoLocks(); }
#define dms_assert_without_debugonly_lock(EXPR) dms_assert_impl(EXPR)
#define dms_assert(EXPR) do { DebugOnlyLock lockChanges("Call to modifying function while debug-only checking assertion '" #EXPR "'", __FILE__, __LINE__); dms_assert_without_debugonly_lock(EXPR); } while(0)
#define dbg_assert(EXPR) dms_assert(EXPR)
#define lfs_assert(EXPR) dms_assert(EXPR)

#else

#define dms_check_not_debugonly { }
#define dms_assert_without_debugonly_lock(EXPR) CC_ASSUME(bool(EXPR))
#define dms_assert(EXPR) dms_assert_without_debugonly_lock(EXPR)
#define dbg_assert(EXPR)
#define lfs_assert(EXPR)

#endif //defined(CC_FIX_ASSERT)

//----------------------------------------------------------------------
// Exception Generation & Message functions
//----------------------------------------------------------------------

#define MG_POS __FILE__, __LINE__
#define MG_NIL 0, 0

[[noreturn]] RTC_CALL void throwErrorD (CharPtr type, CharPtr msg);
[[noreturn]] RTC_CALL void throwDmsErrD(              CharPtr msg);
[[noreturn]] RTC_CALL void throwErrorD (TokenID type, CharPtr msg);

template<typename Type, typename ...Args>
[[noreturn]] void throwErrorF(Type&& type, CharPtr format, Args&&... args)
{
	throwErrorD(std::forward<Type>(type), mgFormat2string<Args...>(format, std::forward<Args>(args)...).c_str());
}

template<typename ...Args>
[[noreturn]] void throwDmsErrF(CharPtr format, Args&&... args)
{
	throwDmsErrD(mgFormat2string<Args...>(format, std::forward<Args>(args)...).c_str());
}

// The internal-error family: a violated invariant of the GeoDms code itself, as opposed to
// anything a configuration can provoke. Each of these names a source file and line and tells the
// reader that the problem is not in their configuration. #1202: they also mark their ErrMsg
// (ErrMsg::m_IsInternalError), so that the report keeps error severity wherever it surfaces.
[[noreturn]] RTC_CALL void throwPreconditionFailed(CharPtr sourceFile, int line, CharPtr msg);
[[noreturn]] RTC_CALL void throwCheckFailed       (CharPtr sourceFile, int line, CharPtr msgFormat);
[[noreturn]] RTC_CALL void throwIllegalAbstract   (CharPtr sourceFile, int line, const Object* obj, CharPtr method);
[[noreturn]] RTC_CALL void throwIllegalAbstract   (CharPtr sourceFile, int line, CharPtr method);
[[noreturn]] RTC_CALL void throwNYI               (CharPtr sourceFile, int line, CharPtr method);

void reportD_impl(MsgCategory msgCat, SeverityTypeID st, CharPtrRange&& msg);
RTC_CALL void reportD(MsgCategory msgCat, SeverityTypeID st, CharPtr msg);
RTC_CALL void reportD(MsgCategory msgCat, SeverityTypeID st, CharPtr msg1, CharPtr msg2);
RTC_CALL void reportD_without_cancellation_check(MsgCategory msgCat, SeverityTypeID st, CharPtr msg);

inline void reportD_impl(SeverityTypeID st, CharPtrRange&& msg) { reportD_impl(MsgCategory::progress, st, std::move(msg)); }
inline void reportD(SeverityTypeID st, CharPtr msg) { reportD(MsgCategory::progress, st, msg); }
inline void reportD(SeverityTypeID st, CharPtr msg1, CharPtr msg2) 
{ 
	reportD(st >= SeverityTypeID::ST_Warning ? MsgCategory::other : MsgCategory::progress, st, msg1, msg2); 
}
inline void reportD_without_cancellation_check(SeverityTypeID st, CharPtr msg) { reportD_without_cancellation_check(MsgCategory::progress, st, msg); }

struct CharPtrRange;
template<typename CharIterType>
void reportD(SeverityTypeID st, IterRange<CharIterType> value)
{
	reportD_impl(st, CharPtrRange(value.begin(), value.end()));
}

template<typename CharIterType>
void reportD(MsgCategory msgCat, SeverityTypeID st, IterRange<CharIterType> value)
{
	reportD_impl(msgCat, st, CharPtrRange(value.begin(), value.end()));
}


template <typename ...Args>
void reportF(SeverityTypeID st, CharPtr format, Args&&... args)
{
	reportD(st, mgFormat2string<Args...>(format, std::forward<Args>(args)...).c_str());
}

template <typename ...Args>
void reportF(MsgCategory msgCat, SeverityTypeID st, CharPtr format, Args&&... args)
{
	reportD(msgCat, st, mgFormat2string<Args...>(format, std::forward<Args>(args)...).c_str());
}

template <typename ...Args>
void reportF_without_cancellation_check(SeverityTypeID st, CharPtr format, Args&&... args)
{
	reportD_without_cancellation_check(st, mgFormat2string<Args...>(format, std::forward<Args>(args)...).c_str());
}

template <typename ...Args>
void reportF_without_cancellation_check(MsgCategory msgCat, SeverityTypeID st, CharPtr format, Args&&... args)
{
	reportD_without_cancellation_check(msgCat, st, mgFormat2string<Args...>(format, std::forward<Args>(args)...).c_str());
}

void ReportSuspension();

// #1202: the item that the calling thread is currently reporting about, as "[[/full/item/name]]",
// or empty when no context handle on this thread knows one. This is the same name that reportD
// appends to a message of severity MajorTrace and up; a reporter that puts the name in FRONT of
// its message asks for it here instead, so that the appending is suppressed (see reportD).
RTC_CALL auto GetReportingItemName() -> SharedStr;


#define MG_CHECK2(Cond, Msg)        if(!(Cond))  { throwCheckFailed(MG_POS, Msg); }
#define MG_USERCHECK2(Cond, Msg)    if(!(Cond))  { throwDmsErrD(Msg); }
#define MG_USERCHECK(Cond)          MG_USERCHECK2(Cond, #Cond)
#define MG_CHECK(Cond)              MG_CHECK2(Cond, #Cond)
#define MG_ASSERT(Cond)             if(!(Cond)) { abort(); }
#define MG_CHECK2_OBJ(Cond, Msg)    if(!(Cond)) { throwItemErrorD(Msg); }
#define MG_CHECK_OBJ(Cond)          MG_CHECK2_OBJ(Cond, #Cond)
#define MG_PRECONDITION2(Cond, Msg) if(!(Cond)) { throwPreconditionFailed(MG_POS, Msg); }
#define MG_PRECONDITION(Cond)       MG_PRECONDITION2(Cond, #Cond)
#define MG_ILLEGAL_ABSTRACT(Mthd)   throwIllegalAbstract(MG_POS, this, Mthd)

#if defined(MG_DEBUG)
#	define MGD_CHECK_OBJ(Cond)         dms_assert(Cond)
#	define MGD_PRECONDITION(Cond)      dms_assert(Cond)
#	define MG_DEBUGCODE(X) X
#else
#	define MGD_CHECK_OBJ(Cond)
#	define MGD_PRECONDITION(Cond)
#	define MG_DEBUGCODE(X)
#endif

#if defined(MG_DEBUG_DATA)
#	define MGD_CHECKDATA(Cond)       dms_assert(Cond)
#	define MG_DEBUG_DATA_CODE(X) X
#else
#	define MGD_CHECKDATA(Cond)
#	define MG_DEBUG_DATA_CODE(X)
#endif

/****************** class compilecheck, move to rtc   *******************/

#if defined(MG_DEBUG)
	template<bool> struct ctime_check;
	template<> struct ctime_check<true> { void ok(); };
#	define CTIME_CHECK(COND) (&(ctime_check<COND>::ok))
#else
#	define CTIME_CHECK(COND)
#endif

#endif // __DBG_DIAGNOSTICS_H
