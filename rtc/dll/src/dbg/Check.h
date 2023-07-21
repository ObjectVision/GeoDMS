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
 *  Name        : Check.h
 *  SubSystem   : Rtl/Dbg
 *  Description : Macro's that can be used for (debug) checks and tracing messages
 *  Definition  : a "check" is a conditional throw of an exception
 *  Definition  : a "debug check" is a check that is only implemented in a debug build
 */

#if !defined(__DBG_CHECK_H)
#define __DBG_CHECK_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "RtcBase.h"
#include "ser/format.h"
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

RTC_CALL void dms_check_failed(CharPtr msg, CharPtr fileName, unsigned line);
#define dms_check(EXPR) (void)( (!!(EXPR)) || (dms_check_failed(#EXPR, __FILE__, __LINE__), 0) )

RTC_CALL void dms_assertion_failed(CharPtr msg, CharPtr fileName, unsigned line);

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

#define dms_is_debuglocked DebugOnlyLock::IsLocked()
#define dms_check_not_debugonly { DebugOnlyLock::CheckNoLocks(); }
#define dms_assert_without_debugonly_lock(EXPR) dms_assert_impl(EXPR)
#define dms_assert(EXPR) do { DebugOnlyLock lockChanges("Call to modifying function while debug-only checking assertion '" #EXPR "'", __FILE__, __LINE__); dms_assert_without_debugonly_lock(EXPR); } while(0)
#define dbg_assert(EXPR) dms_assert(EXPR)
#define lfs_assert(EXPR) dms_assert(EXPR)

#else

#define dms_is_debuglocked false
#define dms_check_not_debugonly { }
#define dms_assert_without_debugonly_lock(EXPR) __assume(bool(EXPR))
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
[[noreturn]] RTC_CALL void throwErrorD (const TokenID& type, CharPtr msg);

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

[[noreturn]] RTC_CALL void throwPreconditionFailed(CharPtr sourceFile, int line, CharPtr msg);
[[noreturn]] RTC_CALL void throwCheckFailed       (CharPtr sourceFile, int line, CharPtr msgFormat);
[[noreturn]] RTC_CALL void throwIllegalAbstract   (CharPtr sourceFile, int line, const PersistentSharedObj* obj, CharPtr method);
[[noreturn]] RTC_CALL void throwIllegalAbstract   (CharPtr sourceFile, int line, CharPtr method);
[[noreturn]] RTC_CALL void throwNYI               (CharPtr sourceFile, int line, CharPtr method);

RTC_CALL void reportD_impl(MsgCategory msgCat, SeverityTypeID st, const CharPtrRange& msg);
RTC_CALL void reportD(MsgCategory msgCat, SeverityTypeID st, CharPtr msg);
RTC_CALL void reportD(MsgCategory msgCat, SeverityTypeID st, CharPtr msg1, CharPtr msg2);
RTC_CALL void reportD_without_cancellation_check(MsgCategory msgCat, SeverityTypeID st, CharPtr msg);

inline void reportD_impl(SeverityTypeID st, const CharPtrRange& msg) { reportD_impl(MsgCategory::nonspecific, st, msg); }
inline void reportD(SeverityTypeID st, CharPtr msg) { reportD(MsgCategory::nonspecific, st, msg); }
inline void reportD(SeverityTypeID st, CharPtr msg1, CharPtr msg2) { reportD(MsgCategory::nonspecific, st, msg1, msg2); }
inline void reportD_without_cancellation_check(SeverityTypeID st, CharPtr msg) { reportD_without_cancellation_check(MsgCategory::nonspecific, st, msg); }

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

RTC_CALL void ReportSuspension();


#define MG_CHECK2(Cond, Msg)        if(!(Cond))  { throwCheckFailed(MG_POS, Msg); }
#define MG_USERCHECK2(Cond, Msg)    if(!(Cond))  { throwDmsErrD(Msg); }
#define MG_USERCHECK(Cond)          MG_USERCHECK2(Cond, #Cond)
#define MG_CHECK(Cond)              MG_CHECK2(Cond, #Cond)
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

#endif // __DBG_CHECK_H
