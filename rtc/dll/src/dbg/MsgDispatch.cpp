// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "DbgInterface.h"
#include "Parallel.h"

#include "debug.h"
#include "DebugReporter.h"
#include "act/MainThread.h" // GetThreadID for the fatal report
#include "act/TriggerOperator.h"
#include "parallel/portable_task_group.h" // task_canceled, named by the fatal report
#include "dbg/DebugLog.h"
#include "utl/MemGuard.h"
#include "utl/StrFormat.h"
#include "utl/splitPath.h"

#include "dbg/DmsCatch.h"
#include "set/VectorFunc.h"
#include "utl/Environment.h"
#include "utl/Registry.h"
#include "ptr/StaticPtr.h"
#include "ser/DebugOutStream.h"
#include "ser/MoreStreamBuff.h"
#include "utl/IncrementalLock.h"
#include "utl/swapper.h"
#include "xct/DmsException.h"

#include <vector>
#include <ctime>

// for the fatal (abort) diagnostics further down
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <typeinfo>

#if defined(WIN32)
// Declared here rather than pulling in windows.h, which must not precede cpc/CompChar.h. The
// MG_CRTLOG block below declares the same import for the Debug-only CRT report hook; the fatal
// handlers need it in Release too, so it is hoisted out of that guard.
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
#else
inline int IsDebuggerPresent() { return 0; }
#endif

/********** MsgCallback Register **********/


// *****************************************************************************
// RtcLock and memory leak detection
// *****************************************************************************

#if defined(_MSC_VER)
#include <new.h>
#endif //defined(_MSC_VER)

namespace { // local defs

	UInt32  s_nrRtcStreamLocks = 0;
	UInt32  s_nrRtcReportLocks = 0;
	//	RtcReportLock s_RtcLock; Don't init from DLLMAIN

	using TMsgCallbackSink = std::pair<MsgCallbackFunc, ClientHandle>;
	using TMsgCallbackSinkContainer = std::vector<TMsgCallbackSink>;

	static_ptr<TMsgCallbackSinkContainer>       g_MsgCallbacks;

	TASyncContinueCheck s_asyncContinueCheckFunc;
} // end anonymous namespace


RTC_CALL auto SetASyncContinueCheck(TASyncContinueCheck asyncContinueCheckFunc) -> TASyncContinueCheck
{
	std::swap(s_asyncContinueCheckFunc, asyncContinueCheckFunc);
	return asyncContinueCheckFunc;
}

RTC_CALL void ASyncContinueCheck()
{
	if (s_asyncContinueCheckFunc)
		s_asyncContinueCheckFunc();
}


RTC_CALL void DMS_CONV DMS_RegisterMsgCallback(MsgCallbackFunc fcb, ClientHandle clientHandle)
{
	DMS_CALL_BEGIN

		assert(IsMetaThread());

		if (!g_MsgCallbacks) 
			g_MsgCallbacks.assign( new TMsgCallbackSinkContainer );
		MG_CHECK(g_MsgCallbacks)
		g_MsgCallbacks->push_back(TMsgCallbackSink(fcb, clientHandle));

	DMS_CALL_END
}

RTC_CALL void DMS_CONV DMS_ReleaseMsgCallback(MsgCallbackFunc fcb, ClientHandle clientHandle)
{
	DMS_CALL_BEGIN

		assert(IsMetaThread());

		MG_CHECK(g_MsgCallbacks)
		vector_erase(*g_MsgCallbacks, TMsgCallbackSink(fcb, clientHandle));
		if (!g_MsgCallbacks->size())
			g_MsgCallbacks.reset();

	DMS_CALL_END
}

void MsgDispatch(MsgData* msgData, bool moreToCome)
{
	assert(msgData);
	assert((msgData->m_SeverityType == SeverityTypeID::ST_Nothing) || IsMetaThread());
	if (!g_MsgCallbacks)
		return;
	if (msgData->m_Txt.ssize() > 256)
		msgData->m_Txt = SharedStr(CharPtrRange(msgData->m_Txt.begin(), msgData->m_Txt.begin() + 256)) + "...";

	TMsgCallbackSinkContainer::iterator
		b = g_MsgCallbacks->begin(),
		e = g_MsgCallbacks->end();

	for (; b!=e; ++b)
	{
		MsgCallbackFunc callBackFunc = b->first;
		if (callBackFunc) // not blocked?
		{
			tmp_swapper<MsgCallbackFunc> blockDefectiveReentrance(b->first, nullptr);
			callBackFunc(b->second, msgData, moreToCome);
		}
	}
}


/********** DebugOutStreamBuff Singleton **********/

namespace { // DebugOutStreamBuff is local

	void FlushMsg(MsgData* msgData)
	{
		SizeT minorSkipCount = 0, majorSkipCount = 0;

		UInt32 printedLines = 0;
		SeverityTypeID st = msgData->m_SeverityType;
		MsgCategory msgCat = msgData->m_MsgCategory;
		auto msgTxt = std::move(msgData->m_Txt);
		auto i = msgTxt.cbegin(), e = msgTxt.csend();

		assert(*e == 0); // guaranteed by caller to have a completed Line.

		// forget the terminating nulls and drop trailing empty lines in order to point to just beyond a real message that is the last line.
		while ((i != e && e[-1] == 0) || e[-1] == '\n')
			--e; 
		bool islastMsgSentAsMoreToCome = false;
		while (i != e || minorSkipCount || majorSkipCount)
		{
			auto endofline = std::find(i, e, char(0));
			bool isLastLine = (endofline == e);

			assert(st <= SeverityTypeID::ST_DispError);
			if (e - i >= 10240 && (st < SeverityTypeID::ST_MajorTrace || (st == SeverityTypeID::ST_MajorTrace && printedLines > 16))) // filter out large trace sections
				if (st <= SeverityTypeID::ST_MinorTrace)
					++minorSkipCount;
				else
					++majorSkipCount;
			else
			{
				if (minorSkipCount || majorSkipCount) {
					auto skipMsg = mySSPrintF("... skipped {} minor and {} major trace lines", UInt64(minorSkipCount), UInt64(majorSkipCount));
					auto summaryData = MsgData{
						majorSkipCount ? SeverityTypeID::ST_MajorTrace : SeverityTypeID::ST_MinorTrace
					,	msgCat
					,	false
					,   msgData->m_ThreadID
					,	msgData->m_DateTime
					,	std::move(skipMsg)
					};
					MsgDispatch(&summaryData, false);
					minorSkipCount = majorSkipCount = 0;
				}
				msgData->m_IsFollowup = false;
				while (true)
				{
					auto eosPtr = std::find(i, endofline, '\n');
					if (eosPtr != i) // skip empty lines
					{
						assert(eosPtr[-1] != char(0));
						msgData->m_Txt = SharedStr(CharPtrRange(i, eosPtr)); // TODO: can we avoid this extra string copy by forwarding a CharPtrRange ?
						islastMsgSentAsMoreToCome = eosPtr != endofline || !isLastLine;
						MsgDispatch(msgData, islastMsgSentAsMoreToCome);
						msgData->m_IsFollowup = islastMsgSentAsMoreToCome;
					}
					if (eosPtr == endofline)
						break;

					i = ++eosPtr;
				}
				++printedLines;
			}
			i = endofline;
			if (i != e)
			{
				assert(!*i);
				++i;
			}
		}
		assert(!islastMsgSentAsMoreToCome);
	}
	ElemAllocComponent s_AllocComponent;
	static std::vector< MsgData > s_FlushPipeline;

	void ProcessMsgDataPipeline()
	{
		assert(IsMetaThread());

		if (!s_nrRtcStreamLocks)
			return;

		std::vector< MsgData > localFlushPileLine;
		{
			leveled_critical_section::scoped_lock lock(*g_DebugStream);
			localFlushPileLine = std::move(s_FlushPipeline);
		}
		for (auto& msgData : localFlushPileLine)
		{
			assert(!msgData.m_IsFollowup);
			FlushMsg(&msgData);
		}
	}

	void PostLogMsg(MsgData&& msgData)
	{
		if (!s_nrRtcStreamLocks)
			return;

		assert(!g_DebugStream->try_lock());

		if (s_FlushPipeline.empty())
			PostMainThreadOper(ProcessMsgDataPipeline);
		s_FlushPipeline.emplace_back(std::move(msgData));
	}

	struct DebugOutStreamBuff : VectorOutStreamBuff
	{
		bool LineEmpty() const
		{
			return m_Data.empty() || m_Data.back() == char(0);
		}

		void NewLine()
		{
			if (!LineEmpty())
			{
				Byte flusher = 0;
				WriteBytes(&flusher, 1);
			}
		}
		void SetSeverity(SeverityTypeID st) 
		{ 
			NewLine();
			m_Severity = st; 
		}
		void SetMsgCategory(MsgCategory msgCat)
		{
			m_MsgCat = msgCat;
		}
		void WriteBytes(const Byte* data, streamsize_t size) override
		{
			if (!s_nrRtcStreamLocks)
				return;

			assert(m_Severity != SeverityTypeID::ST_Nothing); // tests precondition that DebugOutStream::scoped_lock was obtained
			VectorOutStreamBuff::WriteBytes(data, size);
			if (!size || *(data + size - 1))
				return; // if string wasn't terminated by char(0), we haven't finished a Line yet

			if (m_Data.empty())
				return;

			assert(m_Data.end()[-1] == char(0));

			PostLogMsg(MsgData(m_Severity, m_MsgCat, false, GetThreadID(), StreamableDateTime(), SharedStr(CharPtrRange(begin_ptr(m_Data), end_ptr(m_Data)))));
			m_Data.clear();
		}
		bool AtEnd() const override { return false; }

	protected:
		SeverityTypeID m_Severity = SeverityTypeID::ST_MinorTrace;
		MsgCategory m_MsgCat = MsgCategory::progress;
	};

	static_ptr<DebugOutStreamBuff>  g_DebugStreamBuff;

} // end anonymous namespace

/********** DebugOutStream Singleton **********/

#include "LockLevels.h"

DebugOutStream::DebugOutStream()
:	FormattedOutStream(g_DebugStreamBuff, FormattingFlags::None) // Don't use ThousandSeparator here, issue with initialization of s_RegAccess by GetRegStatusFlags.
,	leveled_critical_section(item_level_type(0), ord_level_type::DebugOutStream, "DebugOutStream")
{}

void DebugOutStream::SetSeverity(SeverityTypeID st)
{
	g_DebugStreamBuff->SetSeverity(st);
}

void DebugOutStream::SetSeverity(DebugOutStream* self, SeverityTypeID st) // static
{
	if (!self)
		abort();
	self->SetSeverity(st);
}

void DebugOutStream::SetMsgCategory(MsgCategory msgCat)
{
	g_DebugStreamBuff->SetMsgCategory(msgCat);
}

void DebugOutStream::NewLine()
{
	g_DebugStreamBuff->NewLine();
}

void DebugOutStream::PrintSpaces()
{
	UInt32 nrSpaces = GetCallCount() * 3;
	const char* spaces16 = "                ";
	NewLine();
	if (nrSpaces)
	{
		for (; nrSpaces >= 16; nrSpaces -= 16)
			*this << spaces16;
		*this << (spaces16 + 16 - nrSpaces);
	}
}

static_ptr<DebugOutStream> g_DebugStream;

/********** DebugOutStream::scopend_lock **********/

DebugOutStream::flush_after::~flush_after()
{
}

DebugOutStream::scoped_lock::scoped_lock(DebugOutStream* str, SeverityTypeID st, MsgCategory msgCat)
	: leveled_critical_section::scoped_lock(*str)
	,	m_Str(str)
{
	SetSeverity(m_Str, st);
	m_Str->SetMsgCategory(msgCat);
	m_Str->PrintSpaces();
}

DebugOutStream::scoped_lock::~scoped_lock()
{
	m_Str->NewLine();
	MG_DEBUGCODE( SetSeverity(m_Str, SeverityTypeID::ST_Nothing ); )
}

// *****************************************************************************
// CCrtLog Singleton
// *****************************************************************************

#define MG_WAIT_PER_LINE 0
#define MG_WAIT_PER_MSG 0


#if defined(MG_CRTLOG)
	#include <crtdbg.h>
	namespace {

		class CCrtLog {
		public:
			CCrtLog() 
			{
				DMS_RegisterMsgCallback(CrtMsgCallback, typesafe_cast<ClientHandle>(this));

				DebugOutStream::scoped_lock lock(g_DebugStream, SeverityTypeID::ST_MajorTrace);
				*g_DebugStream << "CRT logging started\n";
			}
			~CCrtLog() 
			{
				DMS_ReleaseMsgCallback(CrtMsgCallback, typesafe_cast<ClientHandle>(this));
			}

		private:
			static void DMS_CONV CrtMsgCallback(ClientHandle clientHandle, const MsgData* msgData, bool moreToCome)
			{
				auto st = msgData->m_SeverityType; 
				auto msgCat = msgData->m_MsgCategory;
				auto msg = msgData->m_Txt.c_str();
				if (st != SeverityTypeID::ST_Nothing) // ST_MinorTrace, ST_MajorTrace, ST_Warning, ST_Error, ST_FatalError, ST_Nothing
				{
					_RPT0(_CRT_WARN, "\n");
					if (msgCat > MsgCategory::progress)
						_RPT0(_CRT_WARN, AsString(msgCat));

					if (st >= SeverityTypeID::ST_Error) // ST_Error, ST_FatalError
						_RPT0(_CRT_WARN, (st== SeverityTypeID::ST_FatalError)?"FatalError:\n":"Error: ");

					SizeT n = StrLen(msg);
					while (n > 80)
					{
						_RPT1(_CRT_WARN, "%.80s", msg);
						Wait(MG_WAIT_PER_LINE);
						msg += 80;
						n -= 80;
					}
					_RPT1(_CRT_WARN, "%s", msg);
					Wait(MG_WAIT_PER_MSG);
				}
			}
		};

		static_ptr<CCrtLog> g_CrtLog;
	} // end anonymous namespace

#endif // MG_CRTLOG


// *****************************************************************************
// memory heap full handler
// *****************************************************************************
// Define a function to be called if new fails to allocate memory.

std::atomic<UInt32> g_CoalesceHeapContextRecursion = 0;

namespace { // local defs
	CoalesceHeapFuncType g_ExternalCoalesceHeapFunc;

	// statics used by MyNewExceptionHandler
	THREAD_LOCAL UInt32 g_MyNewExceptionHandlerCount = 0;

	bool CoalesceHeap(std::size_t requestedSize, UInt32 recursion )	
	{
		if (!g_ExternalCoalesceHeapFunc)
			return false;

		if (recursion)
			return g_ExternalCoalesceHeapFunc(requestedSize, "during handling of previous memory error");

		if (g_CoalesceHeapContextRecursion)
			return g_ExternalCoalesceHeapFunc(requestedSize, "during collection of context info for previous memory error");

		VectorOutStreamBuff buff;
		{
			StaticMtIncrementalLock<g_CoalesceHeapContextRecursion> lock;
			FormattedOutStream outstr(&buff, FormattingFlags::ThousandSeparator);
			AbstrContextHandle* ach = AbstrContextHandle::GetLast();
			while (ach)
			{
				CharPtr extraInfo = ach->GetDescription();
				if (extraInfo)
					outstr << '\n' << extraInfo;
				ach = ach->GetPrev();
			}
			outstr << char(0);
		}
		return g_ExternalCoalesceHeapFunc(requestedSize, buff.GetData()); 
	}

	// re-entrant check!
	int MyNewExceptionHandler( size_t size )
	{
		DynamicIncrementalLock<> lock(g_MyNewExceptionHandlerCount);

		//	g_MyNewExceptionHandlerCount == 1:
		//		first  entry of MyNewExceptionHandler
		//	g_MyNewExceptionHandlerCount == 2:
		//		second entry of MyNewExceptionHandler,
		//		possibly after CoalesceHeap failed to provide context description, 
		//		let it try again without descr provision
		//	g_MyNewExceptionHandlerCount == 3:
		//		third entry of MyNewExceptionHandler,
		//		don't try to call CalesceHeap, just fire a silent (static) exception

		if (g_MyNewExceptionHandlerCount > 2 || !IsMetaThread())
		{
			dms_assert( g_DebugStream );
			dbg_assert( 0 ); // break to invoke debugger, hopefully before total disorder starts
			throw MemoryAllocFailure();
		}

		const int bufSize = 66+1-29 + 23;
		char buf[bufSize];
		snprintf(buf, bufSize, "Memory Allocation failed for %llu bytes", (unsigned long long)size);

		reportD(SeverityTypeID::ST_Warning, buf);
		if (!CoalesceHeap(size, g_MyNewExceptionHandlerCount-1))
			throwErrorF("Memory", "allocation failed for {} bytes and no heap cleanup possible.", (UInt64)size);
		reportD(SeverityTypeID::ST_MajorTrace, "Try again after heap cleanup");
		return true;
	}

} // end anonymous namespace

RTC_CALL void DMS_CONV DMS_SetCoalesceHeapFunc(CoalesceHeapFuncType coalesceHeapFunc)
{
	g_ExternalCoalesceHeapFunc = coalesceHeapFunc;
}

RTC_CALL bool DMS_CONV DMS_CoalesceHeap(std::size_t requestedSize)
{
	return CoalesceHeap(requestedSize, 0);
}

void MustCoalesceHeap(SizeT size)
{
	if (!MyNewExceptionHandler(size))
		throw std::bad_alloc();
}

// *****************************************************************************
// RtcLock and memory leak detection
// *****************************************************************************

// RtcStreamLock gets installed before 
// any other Rtc Install activity, except the above installed static debug stream facility
// and de-installed (DumpMemoryLeaks) after all destructions


#ifdef MG_CRTLOG
// <cstdio>/<cstdlib> and the IsDebuggerPresent import are declared near the top of this file, for the
// fatal handlers that need them in Release too.

// Headless Debug-run report handling, installed for EVERY exe that loads Rtc (GeoDmsRun, GeoDmsGuiQt,
// TicTst): a failed assert or abort() would pop a modal Retry/Ignore dialog that silently stalls any
// automated run (the unit suite's GUI /T tests hung exactly this way). Route the text to stderr and
// _exit(3) so the driving batch proceeds to the next test with a visible failure. Only when no
// debugger is attached -- under cdb/VS the normal break-into-debugger behaviour is kept.
static int DmsHeadlessCrtReportHook(int reportType, char* message, int* returnValue)
{
	if (returnValue)
		*returnValue = 0;
	if (reportType == _CRT_WARN)
		return 0; // FALSE: continue default processing (routed to stderr below); warnings don't dialog
	if (message)
		std::fputs(message, stderr);
	std::fflush(stderr);
	std::fflush(stdout);
	_exit(3); // 3 == abort-like; ends THIS process so an automated driver proceeds to the next test
	return 1; // TRUE: unreached
}
#endif //  MG_CRTLOG

RtcStreamLock::RtcStreamLock()
{
	if (!s_nrRtcStreamLocks++)
	{
		SetMainThreadID();
		DBG_InstallFatalHandlers(); // main thread; each task_group worker installs its own (per-thread on MSVC)
#ifdef MG_CRTLOG
		_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF|_CRTDBG_ALLOC_MEM_DF /*| _CRTDBG_CHECK_CRT_DF*/ );
		if (!IsDebuggerPresent())
		{
			// Route _CRT_WARN reports to stderr for HEADLESS Debug runs: the at-exit heap-leak dump
			// (_CRTDBG_LEAK_CHECK_DF) and ReportExistingObjects' "Memory Leak of N Objects" arrive as
			// _CRT_WARN, which by default goes only to the debugger output channel -- a redirected
			// unit-suite run would never see them. With a debugger attached keep the Output window.
			_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
			_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
			_CrtSetReportHook(DmsHeadlessCrtReportHook);
		}
#endif //  MG_CRTLOG

		g_DebugStreamBuff.assign( new DebugOutStreamBuff);
		g_DebugStream    .assign( new DebugOutStream    );
#ifdef MG_CRTLOG
		g_CrtLog         .assign( new CCrtLog           );
#endif //  MG_CRTLOG
	}
}

void ReportFixedAllocFinalSummary(); // defined in FixedAlloc.cpp

// Deliberately does NOT report the final allocator summary any more; DMS_Rtc_Terminate() does,
// and ~CDebugLog does it earlier still when there is a log file to put it in.
//
// The single RtcReportLock of RtcMain.cpp is a namespace-scope static, so this destructor runs
// from __cxa_finalize, and reporting is not a passive act: it formats a SharedStr, which allocates
// from the stock allocator, and AllocateFromStock() -> ConsiderReporting() -> PostReporting() ends
// in PostMainThreadOper(). By then the main-thread operation queue -- another static of this same
// library -- has already been destroyed, so that Post() called emplace_back() on a destroyed
// std::vector whose growth path freed the already-freed buffer a second time. Valgrind names it:
//
//   Invalid free() ... operation_queue::Post <- PostMainThreadOper <- PostReporting
//   <- ConsiderReporting <- AllocateFromStock <- GetVmSysCallStats
//   <- ReportFixedAllocFinalSummary <- RtcStreamLock::~RtcStreamLock <- __cxa_finalize
//
// on a block freed moments earlier by __cxa_finalize itself. That double free corrupts the heap;
// glibc only notices later while consolidating and blames whichever chunk it walks first, so the
// abort names an innocent Qt allocation ("corrupted size vs. prev_size in fastbins") and nothing
// of ours. Measured on the shipped .l packages, 96 runs each under contention: 20.17.0 0 aborts,
// 20.18.0 8. It reproduces only under load because ConsiderReporting() reports periodically.
//
// The rule this encodes: a static destructor may release, but must not perform work that needs
// infrastructure which is itself static -- the queue here, and the ini cache that
// IsPerformanceLogging() reads two lines into the same summary. Ordering the reporting ahead of
// static destruction fixes both at once, which is why neither of those singletons needs to
// outlive the process. Same reasoning, and the same shape, as DMS_Stg_Terminate() for GDAL (#1206).
RtcStreamLock::~RtcStreamLock()
{
	if (!--s_nrRtcStreamLocks)
	{
		// Clear Tracing system
#if defined(MG_CRTLOG)
		g_CrtLog.reset();
#endif //  MG_CRTLOG
		g_DebugStream.reset();
		g_DebugStreamBuff.reset();
	}
}

// Counterpart of DMS_Stg_Terminate() (#1206), and called next to it: the executable says when the
// run is over, while its own main() frame, the DLLs and all worker-thread teardown are still
// alive. Everything that must not happen from a static destructor belongs here.
//
// Reporting the final allocator summary is the reason this exists. It is a no-op when ~CDebugLog
// already reported it -- that runs at log close, which is earlier and equally safe, and puts the
// summary in the log file where it belongs. A run without /L has no such moment, and used to get
// its summary from ~RtcStreamLock at __cxa_finalize; that is the call this replaces.
//
// Closing the main-thread queues afterwards is what makes the ordering an invariant rather than a
// convention: past this point nothing drains them, so a late post can only be a leak or, once
// their own static destructor has run, a use of freed memory. Dropping such posts is exactly what
// their delivery would have been worth.
extern "C" RTC_CALL void DMS_CONV DMS_Rtc_Terminate()
{
	ReportFixedAllocFinalSummary();
	CloseMainThreadQueues();
}

RtcReportLock::RtcReportLock()
{
	if (!s_nrRtcReportLocks++)
	{
#if defined(_MSC_VER)
		_set_new_handler(MyNewExceptionHandler);
#endif
		assert(IsMetaThread());
	}
}

RtcReportLock::~RtcReportLock()
{
	if (!--s_nrRtcReportLocks)
	{
#if defined(_MSC_VER)
		_set_new_handler(nullptr);
#endif
		DMS_CALL_BEGIN
			
			ReportExistingObjects(); // Dump Memory leaks of PersistentObjects; if registered
			assert(IsMetaThread());

		DMS_CALL_END
	}
}

// *****************************************************************************
// CDebugLog
// *****************************************************************************

#include <time.h>

std::mutex s_timeObjectAccess;

int write_time_str(char* buff, SizeT n, time_t t)
{
	auto lock = std::lock_guard(s_timeObjectAccess);

	auto tm = std::localtime(&t);
	return std::strftime(buff, n, "%Y-%m-%d %H:%M:%S", tm);
}

int write_now_str(char* buff, SizeT n)
{
	auto t = std::time(nullptr);
	return write_time_str(buff, n, t);
}

#include <chrono>

SharedStr DatedName(WeakStr fileName)
{
	auto nameStart = fileName.c_str();
	auto ext = getFileNameExtension(nameStart);
	auto nameEnd = ext; if (nameEnd != nameStart && nameEnd[-1] == '.') 
		--nameEnd;

	int year, month, day_of_month;

	auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());

	std::chrono::year_month_day ymd{ today };

	year = int(ymd.year());
	month = unsigned(ymd.month());
	day_of_month = unsigned(ymd.day());
	return mySSPrintF("{}_{:04}-{:02}-{:02}{}", CharPtrRange(nameStart, nameEnd), year, month, day_of_month, nameEnd);
}

// *****************************************************************************
// live log registry, log flushing, and the fatal (abort) diagnostics
// *****************************************************************************
//
// Why this exists (#1191): closing the GUI while a calculation is in flight can end the process at
// ucrtbase!abort, which reports as exception 0xC0000409. That code reads as STATUS_STACK_BUFFER_OVERRUN
// but is the generic __fastfail: abort() ends with `mov ecx, 7 (FAST_FAIL_FATAL_APP_EXIT); int 29h`.
// So it is a DELIBERATE self-kill, not corruption -- reached in a Release build via std::terminate,
// i.e. an exception crossing a noexcept boundary. Two things were wrong with how that landed:
//
//  - nothing said WHICH exception, from WHERE. WER gives a module and an offset and no more.
//  - the session log lost its tail: a fail-fast runs no destructors, so ~CDebugLog never closed the
//    ofstream and everything still sitting in its buffer was discarded -- including the warnings the
//    teardown drain had just emitted about what it was still waiting for.
//
// The handlers below close both gaps: they name the in-flight exception and the thread's context
// chain, push the logs to disk, and then end the process with a defined exit code.

namespace {

	// Guards s_LiveLogs only; never held while arbitrary code runs, so the fatal path can take it.
	std::mutex          s_LiveLogsMutex;
	std::vector<CDebugLog*> s_LiveLogs;

} // end anonymous namespace

static void RegisterLiveLog(CDebugLog* log)
{
	std::lock_guard lock(s_LiveLogsMutex);
	s_LiveLogs.emplace_back(log);
}

static void UnregisterLiveLog(CDebugLog* log)
{
	std::lock_guard lock(s_LiveLogsMutex);
	s_LiveLogs.erase(std::remove(s_LiveLogs.begin(), s_LiveLogs.end(), log), s_LiveLogs.end());
}

void DMS_CONV DBG_FlushLogs()
{
	std::lock_guard lock(s_LiveLogsMutex);
	for (auto* log : s_LiveLogs)
		log->Flush();
}

namespace {

	// Write one line to every sink that can still take it, without going through MsgDispatch: the
	// dispatch path locks g_DebugStream, and terminate can strike on a thread that already holds it.
	// Losing the report to a self-deadlock would defeat the whole point of this handler.
	void WriteFatalLine(CharPtr line)
	{
		std::fputs(line, stderr);
		std::fputs("\n", stderr);

		std::lock_guard lock(s_LiveLogsMutex);
		for (auto* log : s_LiveLogs)
			log->WriteRawLine(line);
	}

	// Describe the in-flight exception, if there is one. task_canceled is called out by name because
	// it is the one this path expects: an expired TreeItem weak_ptr throws it by design once the config
	// tree is being torn down (lock_or_cancel, SharedTreePtr.h), so it is the likely traveller here.
	void ReportCurrentException()
	{
		auto ep = std::current_exception();
		if (!ep)
		{
			WriteFatalLine("FATAL: terminate called with no exception in flight");
			return;
		}
		try {
			std::rethrow_exception(ep);
		}
		catch (const task_canceled&) {
			WriteFatalLine("FATAL: uncaught task_canceled -- a cancellation escaped its CancelableFrame");
		}
		catch (const DmsException& x) {
			WriteFatalLine(mySSPrintF("FATAL: uncaught DmsException: {}", x.what()).c_str());
		}
		catch (const std::exception& x) {
			WriteFatalLine(mySSPrintF("FATAL: uncaught {}: {}", typeid(x).name(), x.what()).c_str());
		}
		catch (...) {
			WriteFatalLine("FATAL: uncaught exception of non-standard type");
		}
	}

	// The context chain is THREAD_LOCAL, so this names what THIS thread was doing -- for a worker that
	// is the item and operator it was calculating, which is precisely what the #1191 report could not
	// say. Bounded: a corrupted chain must not cost us the report we already have.
	void ReportContextChain()
	{
		UInt32 level = 0;
		for (auto* ch = AbstrContextHandle::GetLast(); ch && level < 25; ch = ch->GetPrev(), ++level)
		{
			CharPtr descr = nullptr;
			try {
				descr = ch->GetDescription();
			}
			catch (...) {
				descr = "<GetDescription() threw>";
			}
			WriteFatalLine(mySSPrintF("FATAL:   [{}] {}", level, descr ? descr : "<null>").c_str());
		}
		if (!level)
			WriteFatalLine("FATAL:   <no context handles on this thread>");
	}

	THREAD_LOCAL bool tl_InFatalHandler = false;

	void ReportFatalAndExit(CharPtr what)
	{
		if (tl_InFatalHandler)
			std::_Exit(3); // second fault inside the handler: take what already reached disk and go
		tl_InFatalHandler = true;

		WriteFatalLine("FATAL: ==================================================================");
		WriteFatalLine(mySSPrintF("FATAL: {} on thread {}", what, GetThreadID()).c_str());
		ReportCurrentException();
		WriteFatalLine("FATAL: context chain of this thread (innermost first):");
		ReportContextChain();
		WriteFatalLine("FATAL: see GeoDMS issue #1191; report this log.");
		WriteFatalLine("FATAL: ==================================================================");

		DBG_FlushLogs();
		std::fflush(stderr);
		std::fflush(stdout);

		// With a debugger attached, keep the break: the stack is not yet unwound when terminate runs,
		// so this is the one moment the originating frame can still be inspected. Same rule the
		// headless CRT report hook above follows.
		if (IsDebuggerPresent())
			return; // the CRT calls abort() next, which breaks into the debugger

		// std::_Exit is the portable _exit: no destructors, no atexit, no WER dialog. 3 == abort-like,
		// matching DmsHeadlessCrtReportHook, so a driving batch sees a clean failure rather than a crash.
		std::_Exit(3);
	}

	void DmsTerminateHandler()
	{
		ReportFatalAndExit("std::terminate called");
		std::abort(); // only reached with a debugger attached
	}

#if defined(_MSC_VER)
	void __cdecl DmsPurecallHandler()
	{
		// A virtual call on an object whose vtable is mid-destruction; the other route to abort().
		ReportFatalAndExit("pure virtual function call");
	}
#endif

} // end anonymous namespace

void DMS_CONV DBG_InstallFatalHandlers()
{
	// MSVC keeps the terminate handler PER THREAD -- verified with a standalone probe: with the handler
	// installed on the main thread only, an exception escaping a std::thread does NOT call it and the
	// process dies at 0xC0000409 with no output, whereas installing it inside that thread does call it.
	// Hence every thread that can throw installs it for itself; on Linux the call is simply idempotent.
	std::set_terminate(DmsTerminateHandler);

#if defined(_MSC_VER)
	_set_purecall_handler(DmsPurecallHandler); // process-wide, so repeat calls are harmless
#endif
}

void DMS_CONV DBG_ReportBoundaryException(CharPtr where)
{
	// NOTE the _without_cancellation_check variants. Plain reportF/reportD perform a cancellation
	// check that itself throws task_canceled while a session is tearing down -- which is exactly the
	// situation this is called from. Reporting must never be the thing that kills the process.
	try {
		try {
			throw; // re-raise whatever the enclosing catch handler is holding
		}
		catch (const task_canceled&) {
			// Not an error: an expired TreeItem weak_ptr throws this by design once the config tree is
			// being torn down (lock_or_cancel). Worth a trace, because it records where work stopped.
			reportF_without_cancellation_check(MsgCategory::other, SeverityTypeID::ST_MajorTrace
				, "{}: task cancelled, abandoning this work", where);
		}
		catch (const DmsException& x) {
			reportF_without_cancellation_check(MsgCategory::other, SeverityTypeID::ST_Error, "{}: {}", where, x.what());
		}
		catch (const std::exception& x) {
			reportF_without_cancellation_check(MsgCategory::other, SeverityTypeID::ST_Error, "{}: {}", where, x.what());
		}
		catch (...) {
			reportF_without_cancellation_check(MsgCategory::other, SeverityTypeID::ST_Error
				, "{}: exception of non-standard type", where);
		}
	}
	catch (...) {} // a boundary reporter that throws would defeat its own purpose
}

CDebugLog::CDebugLog(WeakStr name)
	: CDebugLog(DatedName(name), true)
{}

CDebugLog::CDebugLog(WeakStr name, bool tag)
	:	m_FileBuff(name, true, true), m_Stream(&m_FileBuff, FormattingFlags::ThousandSeparator)
{
	DMS_ENTERS(ord_level_type::IndexedString, dms_shared_v);
	bool isOpened = m_FileBuff.IsOpen();
	if (isOpened)
	{
		DMS_RegisterMsgCallback(DebugMsgCallback, typesafe_cast<ClientHandle>(this));
		RegisterLiveLog(this);
	}

	DebugOutStream::scoped_lock lock(g_DebugStream, isOpened ? SeverityTypeID::ST_MajorTrace : SeverityTypeID::ST_Warning);
	if (!isOpened) 
		*g_DebugStream << "Unable to open debug output file " << name.c_str();
	else
	{
		// Display date and time. 
		char buff[128];
		if (write_now_str(buff, sizeof buff) > 0)
		{
			*g_DebugStream << "@@@@@ Logging started for " << name.c_str() << " at " << buff;
		}
	}
}

CDebugLog::~CDebugLog() 
{
	DMS_ENTERS(ord_level_type::IndexedString, dms_shared_v);
	ReportFixedAllocFinalSummary();
	{
		DebugOutStream::scoped_lock lock(g_DebugStream, SeverityTypeID::ST_MajorTrace);

		// Display date and time. 
		char buff[128];
		if (write_now_str(buff, sizeof buff) > 0)
		{
			*g_DebugStream << "@@@@@ Logging ended for " << m_FileBuff.FileName().c_str() << " at " << buff;
		}
	}
	ProcessMsgDataPipeline();
	UnregisterLiveLog(this);
	DMS_ReleaseMsgCallback(DebugMsgCallback, typesafe_cast<ClientHandle>(this));
}

void DMS_CONV CDebugLog::DebugMsgCallback(ClientHandle clientHandle, const MsgData* msgData, bool moreToCome)
{
	CDebugLog* dl = reinterpret_cast<CDebugLog*>(clientHandle);
	dl->m_Stream << '\n' << msgData->m_DateTime
		<< "[" << msgData->m_ThreadID << "]"
		<< "[" << SeverityAsChar(msgData->m_SeverityType) << "]"
		<< AsString(msgData->m_MsgCategory)
		<< msgData->m_Txt;
}

void CDebugLog::Flush()
{
	m_FileBuff.Flush();
}

void CDebugLog::WriteRawLine(CharPtr line)
{
	m_Stream << '\n' << line;
}

CDebugLog* DMS_CONV DBG_DebugLog_Open(CharPtr fileName)
{
	DMS_CALL_BEGIN

		return new CDebugLog(SharedStr(fileName MG_DEBUG_ALLOCATOR_SRC("DBG_DebugLog_Open")));

	DMS_CALL_END
	return nullptr;
}

void DMS_CONV DBG_DebugLog_Close(CDebugLog* log)
{
	DMS_CALL_BEGIN

		delete log;

	DMS_CALL_END
}

