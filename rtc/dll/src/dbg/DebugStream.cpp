// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(_MSC_VER)
#pragma hdrstop
#endif

#include "DbgInterface.h"
#include "Parallel.h"

#include "debug.h"
#include "DebugReporter.h"
#include "act/TriggerOperator.h"
#include "dbg/DebugLog.h"
#include "utl/MemGuard.h"
#include "utl/mySPrintF.h"

#include <vector>

#include "dbg/DmsCatch.h"
#include "set/VectorFunc.h"
#include "utl/Environment.h"
#include "ptr/StaticPtr.h"
#include "ser/DebugOutStream.h"
#include "ser/MoreStreamBuff.h"
#include "utl/IncrementalLock.h"
#include "utl/swapper.h"
#include "xct/DmsException.h"

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


RTC_CALL void DMS_CONV DMS_SetASyncContinueCheck(TASyncContinueCheck asyncContinueCheckFunc)
{
	s_asyncContinueCheckFunc = asyncContinueCheckFunc;
}

RTC_CALL void DMS_CONV DMS_ASyncContinueCheck()
{
	if (s_asyncContinueCheckFunc)
		s_asyncContinueCheckFunc();
}


RTC_CALL void DMS_CONV DMS_RegisterMsgCallback(MsgCallbackFunc fcb, ClientHandle clientHandle)
{
	DMS_CALL_BEGIN

		assert(IsMainThread());

		if (!g_MsgCallbacks) 
			g_MsgCallbacks.assign( new TMsgCallbackSinkContainer );
		MG_CHECK(g_MsgCallbacks)
		g_MsgCallbacks->push_back(TMsgCallbackSink(fcb, clientHandle));

	DMS_CALL_END
}

RTC_CALL void DMS_CONV DMS_ReleaseMsgCallback(MsgCallbackFunc fcb, ClientHandle clientHandle)
{
	DMS_CALL_BEGIN

		assert(IsMainThread());

		MG_CHECK(g_MsgCallbacks)
		vector_erase(*g_MsgCallbacks, TMsgCallbackSink(fcb, clientHandle));
		if (!g_MsgCallbacks->size())
			g_MsgCallbacks.reset();

	DMS_CALL_END
}

void MsgDispatch(MsgData* msgData)
{
	assert(msgData);
	assert((msgData->m_SeverityType == SeverityTypeID::ST_Nothing) || IsMainThread());
	if (!g_MsgCallbacks)
		return;

	TMsgCallbackSinkContainer::iterator
		b = g_MsgCallbacks->begin(),
		e = g_MsgCallbacks->end();

	for (; b!=e; ++b)
	{
		MsgCallbackFunc callBackFunc = b->first;
		if (callBackFunc) // not blocked?
		{
			tmp_swapper<MsgCallbackFunc> blockDefectiveReentrance(b->first, nullptr);
			callBackFunc(b->second, msgData);
		}
	}
}


/********** DebugOutStreamBuff Singleton **********/

namespace { // DebugOutStreamBuff is local

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
			MsgData msgData(m_Severity, m_MsgCat, false, GetThreadID(), StreamableDateTime(), SharedStr(begin_ptr(m_Data), end_ptr(m_Data)));
			AddMainThreadOper([msg = std::move(msgData)]() mutable {
					if (!s_nrRtcStreamLocks)
						return;
					leveled_critical_section::scoped_lock lock(*g_DebugStream);
					DebugOutStreamBuff::Flush(&msg);
				}
			, true
			);
			m_Data.clear();
		}
		bool AtEnd() const override { return false; }
		static void Flush(MsgData* msgData)
		{
			SizeT minorSkipCount = 0, majorSkipCount = 0;

			UInt32 printedLines = 0;
			SeverityTypeID st = msgData->m_SeverityType;
			MsgCategory msgCat = msgData->m_MsgCategory;
			auto msgTxt = msgData->m_Txt;
			auto i = msgTxt.begin(), e = msgTxt.end();
			while (i!=e)
			{
				assert(e[-1]==0); // guaranteed by caller to have a completed Line.
				assert(st <= SeverityTypeID::ST_DispError);
				if (e - i >= 1024000 && (st < SeverityTypeID::ST_MajorTrace || st == SeverityTypeID::ST_MajorTrace && printedLines > 16)) // filter out large trace sections
					if (st <= SeverityTypeID::ST_MinorTrace)
						++minorSkipCount;
					else
						++majorSkipCount;
				else
				{
					if (minorSkipCount || majorSkipCount) {
						auto skipMsg = mySSPrintF("... skipped %I64u minor and %I64u major trace lines", UInt64(minorSkipCount), UInt64(majorSkipCount));
						auto summaryData = MsgData{
							majorSkipCount ? SeverityTypeID::ST_MajorTrace : SeverityTypeID::ST_MinorTrace
						,	msgCat
						,	false
						,   msgData->m_ThreadID
						,	msgData->m_DateTime
						,	std::move(skipMsg)
						};
						MsgDispatch(&summaryData);
						minorSkipCount = majorSkipCount = 0;
					}
					while (true)
					{
						auto eolPtr = std::find(i, e, '\n');
						if (eolPtr != i)
						{
							auto eosPtr = eolPtr; if (eosPtr[-1] == char(0)) --eosPtr;
							msgData->m_Txt = SharedStr(i, eosPtr); // TODO: can we avoid this extra string copy by forwarding a CharPtrRange ?
							MsgDispatch(msgData);
							msgData->m_IsFollowup = true;
						}
						if (eolPtr == e)
							break;

						i = ++eolPtr;
					}
					++printedLines;
				}
				i = std::find(i, e, char(0));
				++i;
			}
		}
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
	assert(this); // go in a recursive loop if DebugStream is already destructed
	if (!this) 
		abort();
	g_DebugStreamBuff->SetSeverity(st);
}

void DebugOutStream::SetMsgCategory(MsgCategory msgCat)
{
	assert(this); // go in a recursive loop if DebugStream is already destructed
	if (!this)
		abort();
	g_DebugStreamBuff->SetMsgCategory(msgCat);
}

void DebugOutStream::NewLine()
{
	assert(this);
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

RTC_CALL static_ptr<DebugOutStream> g_DebugStream;

/********** DebugOutStream::scopend_lock **********/

DebugOutStream::flush_after::~flush_after()
{
	if (IsMainThread())
		ProcessMainThreadOpers();
}

DebugOutStream::scoped_lock::scoped_lock(DebugOutStream* str, SeverityTypeID st, MsgCategory msgCat)
	: leveled_critical_section::scoped_lock(*str)
	,	m_Str(str)
{
	m_Str->SetSeverity( st );
	m_Str->SetMsgCategory(msgCat);
	m_Str->PrintSpaces();
}

DebugOutStream::scoped_lock::~scoped_lock()
{
	m_Str->NewLine();
	MG_DEBUGCODE( m_Str->SetSeverity(SeverityTypeID::ST_Nothing ); )
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
				*g_DebugStream << "CRT logging started\n" << char(0);
			}
			~CCrtLog() 
			{
				DMS_ReleaseMsgCallback(CrtMsgCallback, typesafe_cast<ClientHandle>(this));
			}

		private:
			static void DMS_CONV CrtMsgCallback(ClientHandle clientHandle, const MsgData* msgData)
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

		if (g_MyNewExceptionHandlerCount > 2 || !IsMainThread())
		{
			dms_assert( g_DebugStream );
			dbg_assert( 0 ); // break to invoke debugger, hopefully before total disorder starts
			throw MemoryAllocFailure();
		}

		const int bufSize = 66+1-29 + 23;
		char buf[bufSize];
		_snprintf(buf, bufSize, "Memory Allocation failed for %I64u bytes", (UInt64)size);

		reportD(SeverityTypeID::ST_Warning, buf);
		if (!CoalesceHeap(size, g_MyNewExceptionHandlerCount-1))
			throwErrorF("Memory", "allocation failed for %I64u bytes and no heap cleanup possible.", (UInt64)size);
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


RtcStreamLock::RtcStreamLock()
{
	if (!s_nrRtcStreamLocks++)
	{
		SetMainThreadID();
#ifdef MG_CRTLOG
		_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF|_CRTDBG_ALLOC_MEM_DF /*| _CRTDBG_CHECK_CRT_DF*/ );
#endif //  MG_CRTLOG

		g_DebugStreamBuff.assign( new DebugOutStreamBuff);
		g_DebugStream    .assign( new DebugOutStream    );
#ifdef MG_CRTLOG
		g_CrtLog         .assign( new CCrtLog           );
#endif //  MG_CRTLOG
	}
}

void ReportFixedAllocFinalSummary(); // defined in FixedAlloc.cpp

RtcStreamLock::~RtcStreamLock()
{
	if (!--s_nrRtcStreamLocks)
	{
		ReportFixedAllocFinalSummary();

		// Clear Tracing system
#if defined(MG_CRTLOG)
		g_CrtLog.reset();
#endif //  MG_CRTLOG
		g_DebugStream.reset();
		g_DebugStreamBuff.reset();
	}
}

RtcReportLock::RtcReportLock()
{
	if (!s_nrRtcReportLocks++)
	{
#if defined(_MSC_VER)
		_set_new_handler(MyNewExceptionHandler);
#endif
		assert(IsMainThread());
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
			assert(IsMainThread());

		DMS_CALL_END
	}
}
// *****************************************************************************
// CDebugLog
// *****************************************************************************
#include <time.h>

CDebugLog::CDebugLog(WeakStr name) 
	:	m_FileBuff(name, 0, true, true), m_Stream(&m_FileBuff, FormattingFlags::ThousandSeparator)
{
	bool isOpened = m_FileBuff.IsOpen();
	if (isOpened) 
		DMS_RegisterMsgCallback(DebugMsgCallback, typesafe_cast<ClientHandle>(this));

	DebugOutStream::scoped_lock lock(g_DebugStream, isOpened ? SeverityTypeID::ST_MajorTrace : SeverityTypeID::ST_Warning);
	if (!isOpened) 
		*g_DebugStream << "Unable to open debug output file " << name.c_str();
	else
	{
		// Display operating system-style date and time. 
		char dateBuff[128], timeBuff[128];
		_strdate_s( dateBuff, 128 );
		_strtime_s( timeBuff, 128 );
		*g_DebugStream << "@@@@@ Logging started for " << name.c_str() << " at " << dateBuff << " - " << timeBuff;
	}
}

CDebugLog::~CDebugLog() 
{
	ReportFixedAllocFinalSummary();
	{
		DebugOutStream::scoped_lock lock(g_DebugStream, SeverityTypeID::ST_MajorTrace);

		// Display operating system-style date and time. 
		char dateBuff[128], timeBuff[128];
		_strdate_s( dateBuff, 128 );
		_strtime_s( timeBuff, 128 );
		*g_DebugStream << "@@@@@ Logging ended for " << m_FileBuff.FileName().c_str() << " at " << dateBuff << " - " << timeBuff;
	}
	DMS_ReleaseMsgCallback(DebugMsgCallback, typesafe_cast<ClientHandle>(this));
}

void DMS_CONV CDebugLog::DebugMsgCallback(ClientHandle clientHandle, const MsgData* msgData)
{
	CDebugLog* dl = reinterpret_cast<CDebugLog*>(clientHandle);
	dl->m_Stream << '\n' << msgData->m_DateTime << "[" << msgData->m_ThreadID << "]" << msgData->m_Txt;
}

CDebugLog* DMS_CONV DBG_DebugLog_Open(CharPtr fileName)
{
	DMS_CALL_BEGIN

		return new CDebugLog(SharedStr(fileName));

	DMS_CALL_END
	return nullptr;
}

void DMS_CONV DBG_DebugLog_Close(CDebugLog* log)
{
	DMS_CALL_BEGIN

		delete log;

	DMS_CALL_END
}

