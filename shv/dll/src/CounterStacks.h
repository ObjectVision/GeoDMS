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

#if !defined(__SHV_COUNTERSTACKS_H)
#define __SHV_COUNTERSTACKS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geo/Geometry.h"
#include "geo/SeqVector.h"
//#include "set/ResourceCollection.h"
#include "utl/swap.h"

#include <boost/utility.hpp>

#include "Region.h"

struct CounterStacks;
struct ResumableCounter;

#if defined(MG_DEBUG)
#	define MG_CHECK_PAST_BREAK
#endif

//----------------------------------------------------------------------
// class  : CounterStack
//----------------------------------------------------------------------

struct CounterStack : private boost::noncopyable
{
	CounterStack() = default;

	CounterStack(Region&& drawRegion, SizeT stackBase)
		:	m_DrawRegion(std::move(drawRegion))
		,	m_StackBase(stackBase)
	{}

	CounterStack(CounterStack&& rhs) noexcept
		: m_DrawRegion(std::move(rhs.m_DrawRegion))
		, m_StackBase(rhs.m_StackBase)
	{}

	CounterStack& operator = (CounterStack&& rhs) noexcept
	{
		m_DrawRegion = std::move(rhs.m_DrawRegion);
		m_StackBase  = rhs.m_StackBase;
		return *this;
	}

	Region m_DrawRegion;
	SizeT m_StackBase = 0;
};

//----------------------------------------------------------------------
// class  : CounterStacks
//----------------------------------------------------------------------

struct CounterStacks : private boost::noncopyable
{
	CounterStacks();
	~CounterStacks();

	CounterStacks(const CounterStacks&) = delete;
	CounterStacks& operator =(const CounterStacks&) = delete;

	void Reset           (Region&& drawRegion);
	void AddDrawRegion   (Region&& drawRegion);
	void LimitDrawRegions(const GPoint& maxSize);

	void Scroll(GPoint delta, const GRect& scrollRect, const GRect& clipRect);
	void PopBack();

	bool Empty()               const { return m_Stacks.empty(); }
	bool HasMultipleStacks()   const { return m_Stacks.size() > 1; }
	bool DidBreak()            const { return m_DidBreak; }

	const CounterStack& GetCurrStack() const { dms_assert(!Empty           ()); return m_Stacks.end()[-1]; }
	      CounterStack& GetCurrStack()       { dms_assert(!Empty           ()); return m_Stacks.end()[-1]; }
	const CounterStack& GetNextStack() const { dms_assert(HasMultipleStacks()); return m_Stacks.end()[-2]; }
	      CounterStack& GetNextStack()       { dms_assert(HasMultipleStacks()); return m_Stacks.end()[-2]; }
	// query most recent (least advanced) CounterStack
	const Region& CurrRegion() const;

	// actions for most recent (least advanced) CounterStack
	SizeT  IncLevel(SizeT firstValue);
	void   SetCurrCounter(const ResumableCounter* curr);
	void   Suspend(SizeT currValue, const ResumableCounter* prevCounter);
	void   CloseCounter(const ResumableCounter* prevCounter);
	void   EraseSuspended();

	//	Diagnostic checks
	UInt32 NrActiveCounters()    const { return m_NrActiveCounters; }
	bool   NoActiveCounters()    const { return m_NrActiveCounters == 0; }
	bool   NoSuspendedCounters() const { return Empty() || GetCurrStack().m_StackBase + m_NrActiveCounters >= m_Counters.size(); }

	const ResumableCounter* GetCurrCounter() const { return m_CurrCounter; }

	bool    MustBreak()          const;

private: friend struct ResumableCounter;

	typedef std::vector<CounterStack>                         CounterStackCollection;
//	typedef ResourceCollection< NaiveCounterStackCollection > CounterStackCollection;

	typedef CounterStackCollection::iterator       stack_iterator; 
	typedef CounterStackCollection::const_iterator const_stack_iterator; 

	bool HasBreakingStackSize()  const;
	bool CurrStackEmpty();


#if defined(MG_DEBUG)
	bool IsOK() const; // checks invariant constraints, for debugging purpose only
	bool MustBreakCounterStack(const_stack_iterator stackPtr) const;
#endif

	void cutoff_stacks      (stack_iterator  stackPtr);
	void increment_or_remove(stack_iterator& stackPtr, stack_iterator& stackEnd);

	CounterStackCollection    m_Stacks;
	std::vector<SizeT>        m_Counters;
	SizeT                     m_NrActiveCounters;
	const ResumableCounter*   m_CurrCounter;
	bool                      m_DidBreak;
};

//----------------------------------------------------------------------
// class  : ResumableCounter
//----------------------------------------------------------------------

struct ResumableCounter : private boost::noncopyable
{
	ResumableCounter(CounterStacks* cs, bool markProgress);
	~ResumableCounter();
	void Close();

	SizeT Value() const
	{
		dms_assert(IsOK());
		return m_AutoCounter; 
	}

	void SetValue(SizeT newValue)
	{
		dms_assert(IsChangable());
		dms_assert(newValue >= m_AutoCounter);
		m_AutoCounter = newValue;
		dms_assert(IsOK());

	}

	bool Inc()
	{
		dms_assert(IsOK());
		if (m_CounterStacksPtr)
			m_CounterStacksPtr->EraseSuspended();
		if (MustBreakNext())
			return false;
		operator ++();
		return !MustBreak();
	}

	void operator ++()
	{
		dms_assert(IsChangable());
		++m_AutoCounter;
		if (m_MarkProgress)
			SuspendTrigger::MarkProgress();
	}

	void operator +=(SizeT inc)
	{
		dms_assert(IsChangable());
		m_AutoCounter += inc;
	}

	bool operator ==(SizeT value) const { return m_AutoCounter == value; }

	bool MustBreak         () const;
	bool MustBreakOrSuspend() const;
	bool MustBreakOrSuspend100  () const { return (m_AutoCounter % 0x0080) == 0 && MustBreakOrSuspend(); }
	bool MustBreakOrSuspend1000 () const { return (m_AutoCounter % 0x0400) == 0 && MustBreakOrSuspend(); }
	bool MustBreakOrSuspend10000() const { return (m_AutoCounter % 0x2000) == 0 && MustBreakOrSuspend(); }

	bool MustBreakNext() const;

private: friend CounterStacks;
	bool IsChangable() const 
	{ 
		dms_assert( IsActive() );
		dms_assert(!m_CounterStacksPtr || m_CounterStacksPtr->NoSuspendedCounters());
		return m_AutoCounter < m_StopValue;
	}
	bool IsOK ()       const 
	{ 
#		if defined(MG_CHECK_PAST_BREAK)
		dms_assert(m_AutoCounter <= m_StopValue || (m_CounterStacksPtr && m_CounterStacksPtr->HasBreakingStackSize()) );
#		endif
		return m_AutoCounter != UNDEFINED_VALUE(SizeT); 
	}
	bool IsActive()    const { return !m_CounterStacksPtr || m_CounterStacksPtr->m_CurrCounter == this; }

	CounterStacks*          m_CounterStacksPtr;
	SizeT                   m_AutoCounter; // alternative in case Visitor doen't provide a CounterStack
	SizeT                   m_StopValue; 
	const ResumableCounter* m_PrevCounter;
	bool                    m_MarkProgress;

#if defined(MG_DEBUG)
	friend int LexCompareLastImpl(const SizeT*& firstCounter, const SizeT* lastCounter, const ResumableCounter* c);
#endif
};


#endif // __SHV_COUNTERSTACKS_H

