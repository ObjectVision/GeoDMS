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

#include "ShvDllPch.h"

#include "CounterStacks.h"

#include "act/Actor.h"
#include "dbg/debug.h"
#include "ser/AsString.h"
#include "ser/RangeStream.h"
#include "utl/swap.h"

//----------------------------------------------------------------------
// class  : CounterStacks
//----------------------------------------------------------------------

CounterStacks::CounterStacks()
	:	m_NrActiveCounters(0)
	,	m_CurrCounter(0)
	,	m_DidBreak(false)
{
	m_Stacks.reserve(8);
}

void CounterStacks::Reset(Region&& drawRegion)
{
	DBG_START("CounterStacks", "Reset", MG_DEBUG_COUNTERSTACKS);

	dms_assert(NoActiveCounters());

	m_Stacks.clear();
	m_Counters.clear();

	m_DidBreak = false;
	AddDrawRegion(std::move(drawRegion));

}

bool CounterStacks::CurrStackEmpty()
{
	dms_assert(!Empty());

	auto currStackBase = GetCurrStack().m_StackBase;

	dms_assert(currStackBase <= m_Counters.size());

	while (currStackBase < m_Counters.size())
		if (m_Counters.back() == 0)
			m_Counters.pop_back();
		else
			return false;

	dms_assert(currStackBase == m_Counters.size());

	return true;
}

void CounterStacks::AddDrawRegion(Region&& drawRegion)
{
	DBG_START("CounterStacks", "AddDrawRegion", MG_DEBUG_COUNTERSTACKS);

	assert(NoActiveCounters());
	assert(!DidBreak());
	assert(IsOK());
	assert(!Empty() || m_Counters.size() == 0);

	if (drawRegion.Empty())
		return;

	if (Empty())
		m_Stacks.push_back( CounterStack(std::move(drawRegion), m_Counters.size()) ); // eats drawRegion
	else
	{
		if ( !CurrStackEmpty() )
		{
			// increase existing stacks with new drawRegion
			dms_assert( GetCurrStack().m_StackBase < m_Counters.size() );

			m_Stacks.push_back( CounterStack(drawRegion.Clone(), m_Counters.size()) );
		}

		stack_iterator
			stackPtr = m_Stacks.begin(),
			stackEnd = m_Stacks.end  ();

		while (stackPtr != stackEnd)
		{
			stackPtr->m_DrawRegion |= typesafe_cast<const Region&>(drawRegion); // don't eat drawRegion
			increment_or_remove(stackPtr, stackEnd);
		}
	}
	dbg_assert(IsOK());
}

void CounterStacks::LimitDrawRegions(const GPoint& maxSize)
{
	DBG_START("CounterStacks", "LimitDrawRegions", MG_DEBUG_COUNTERSTACKS);

	dms_assert(NoActiveCounters());
	dms_assert(!DidBreak());

	dbg_assert(IsOK());

	GRect bbox(GPoint(0, 0), maxSize);

	stack_iterator
		stackPtr = m_Stacks.begin(),
		stackEnd = m_Stacks.end  ();
	while (stackPtr != stackEnd)
	{
		Region& drawRegion = stackPtr->m_DrawRegion;
		if ( drawRegion.IsBoundedBy( bbox ) )
			break;
		drawRegion &= bbox;
		if (drawRegion.Empty())
		{
			cutoff_stacks(stackPtr);
			break;
		}

		increment_or_remove(stackPtr, stackEnd);
	}
	dbg_assert(IsOK()); // let op: MustBreak check counter values van NextStack kan wellicht gemist worden wegens scherpere clipping
}

void CounterStacks::ScrollDevice(GPoint delta, const GRect& scrollRect, const GRect& clipRect)
{
	DBG_START("CounterStacks", "Scroll", MG_DEBUG_COUNTERSTACKS);

	DBG_TRACE(("Delta     : (x=%d, y=%d)",  delta.x, delta.y) );
	DBG_TRACE(("ScrollRect: %s", AsString(scrollRect).c_str() ) );

	dms_assert(NoActiveCounters());
	dms_assert(!DidBreak());

	dbg_assert(IsOK());

	Region scrollClipRgn(clipRect);
	stack_iterator
		stackPtr = m_Stacks.begin(),
		stackEnd = m_Stacks.end  ();
	while (stackPtr != stackEnd)
	{
		Region& drawRegion = stackPtr->m_DrawRegion;
		if (! drawRegion.IsIntersecting(clipRect))
		{
			DBG_TRACE(("Scrolling aborted because curr region is outside scrolling area") );
			increment_or_remove(stackPtr, stackEnd);
			break;
		}
		drawRegion.ScrollDevice(delta, scrollRect, scrollClipRgn);
		if (drawRegion.Empty())
		{
			cutoff_stacks(stackPtr);
			break;
		}
		increment_or_remove(stackPtr, stackEnd);
	}
	dbg_assert(IsOK());
}

void CounterStacks::PopBack()
{
	DBG_START("CounterStacks", "PopBack", MG_DEBUG_COUNTERSTACKS);

	dms_assert(!Empty());
	dms_assert(NoActiveCounters());
	dms_assert(!DidBreak() || HasMultipleStacks() );
	// Don't check OK now, we might have come from Scroll or LimitDrawRegions

	dms_assert(GetCurrStack().m_StackBase <= m_Counters.size());

	EraseSuspended();
	m_Stacks.pop_back(); 
	m_DidBreak = false;

	dbg_assert(IsOK());
}

void CounterStacks::cutoff_stacks(stack_iterator stackPtr)
{
	m_Counters.erase(m_Counters.begin() + stackPtr->m_StackBase, m_Counters.end());
	m_Stacks.erase(stackPtr, m_Stacks.end());
}

void CounterStacks::increment_or_remove(stack_iterator& stackPtr, stack_iterator& stackEnd)
{
	if	(	stackPtr != m_Stacks.begin() 
		&&	stackPtr[0].m_DrawRegion == stackPtr[-1].m_DrawRegion
		)
	{
		SizeT storedNextStackSize = stackPtr[0].m_StackBase - stackPtr[-1].m_StackBase;
		m_Counters.erase(
			m_Counters.begin() + stackPtr[-1].m_StackBase, 
			m_Counters.begin() + stackPtr[ 0].m_StackBase
		);
		stack_iterator stackPtr2 = stackPtr;
		while (stackPtr2 != stackEnd)
		{
			stackPtr2->m_StackBase -= storedNextStackSize;
			++stackPtr2;
		}

		SizeT currIndex = stackPtr - m_Stacks.begin(), endIndex = stackEnd - m_Stacks.begin();
		m_Stacks.erase(stackPtr-1); // stackPtr now points to next 
		stackPtr = m_Stacks.begin() + currIndex;
		stackEnd = m_Stacks.begin() + (endIndex-1);
		dms_assert(stackEnd == m_Stacks.end());
	}
	else
		++stackPtr;
}

const Region& CounterStacks::CurrRegion() const
{
	dms_assert(!Empty());
	return GetCurrStack().m_DrawRegion;
}

void CounterStacks::SetCurrCounter(const ResumableCounter* curr)
{
	dms_assert(curr && curr->m_PrevCounter == m_CurrCounter);
	m_CurrCounter = curr;
}

bool CounterStacks::MustBreak() const
{
	return HasMultipleStacks() 
		&& m_CurrCounter 
		&& m_CurrCounter->MustBreak();
}

bool CounterStacks::HasBreakingStackSize() const
{
	// check that we are here only for the right reasons
	SizeT nextStoredStackSize = GetCurrStack().m_StackBase - GetNextStack().m_StackBase;
	assert(nextStoredStackSize >= m_NrActiveCounters); // else we would be either have m_DidBreak already or m_AutoCounter < m_StopValue

	return nextStoredStackSize == m_NrActiveCounters;
}

#if defined(MG_DEBUG)

	int LexCompareLastImpl(const SizeT*& firstCounter, const SizeT* lastCounter, const ResumableCounter* c)
	{
		if (c->m_PrevCounter)
		{
			int result = LexCompareLastImpl(firstCounter, lastCounter, c->m_PrevCounter);
			if (result) return result;
		}
		if (firstCounter == lastCounter || *firstCounter < c->m_AutoCounter)
			return -1;
		if (*firstCounter > c->m_AutoCounter)
			return 1;
		++firstCounter;
		return 0;
	}

	int LexCompareLast(const SizeT* firstCounter, const SizeT* lastCounter, const ResumableCounter* c)
	{
		int result = (c)
			?	LexCompareLastImpl(firstCounter, lastCounter, c)
			:	0;
		if (!result && firstCounter != lastCounter)
			result = 1;
		return result;
	}

	bool CounterStacks::MustBreakCounterStack(const_stack_iterator stackPtr) const
	{
		dms_assert(HasMultipleStacks());
		dms_assert(stackPtr != m_Stacks.begin());
		dms_assert(stackPtr != m_Stacks.end());
		dms_assert(stackPtr != m_Stacks.end()-1);

#		if defined(MG_CHECK_PAST_BREAK)

		// m_Counters must be a reverse ordered sequence, second counters may not be more advanced than first
		dms_assert(!	std::lexicographical_compare(
				m_Counters.begin() + stackPtr[-1].m_StackBase, 
				m_Counters.begin() + stackPtr[ 0].m_StackBase,
				m_Counters.begin() + stackPtr[ 0].m_StackBase, 
				m_Counters.begin() + stackPtr[ 1].m_StackBase)
		);

#		endif

		// m_Counters must be a strict reverse ordered sequence, first m_Counters must be most advanced
		return	!	std::lexicographical_compare(
				m_Counters.begin() + stackPtr[ 0].m_StackBase, 
				m_Counters.begin() + stackPtr[ 1].m_StackBase,
				m_Counters.begin() + stackPtr[-1].m_StackBase, 
				m_Counters.begin() + stackPtr[ 0].m_StackBase
			);
	}

	bool CounterStacks::IsOK() const
	{
		DBG_START("CounterStacks", "IsOK", MG_DEBUG_COUNTERSTACKS);

		if (Empty())
		{
			dms_assert(m_Counters.size() == 0);
			return true;
		}

#if defined(MG_DEBUG)
		if (debugContext.m_Active)
		{
			const_stack_iterator
				stackPtr = m_Stacks.begin(),
				stackEnd = m_Stacks.end  ();
			while (stackPtr != stackEnd)
			{
				DBG_TRACE(("Rects: %s", stackPtr->m_DrawRegion.AsString().c_str() ) );
				++stackPtr;
			}
		}
#endif

		dms_assert(!DidBreak() || HasMultipleStacks());
		dms_assert(m_Stacks[0].m_StackBase == 0);
		dms_assert(!m_Stacks.back().m_DrawRegion.Empty());

		const_stack_iterator
			stackPtr = m_Stacks.begin()+1,
			stackEnd = m_Stacks.end  ();
		while (stackPtr != stackEnd)
		{
			dms_assert(stackPtr[0].m_StackBase >= stackPtr[-1].m_StackBase);
			dms_assert(!stackPtr[0].m_DrawRegion.Empty());

			// m_DrawRegion must be a strict reverse ordered sequence, first m_DrawRegion must be largest
			dms_assert( stackPtr[-1].m_DrawRegion.IsIncluding(stackPtr[0].m_DrawRegion) );
//			dms_assert( stackPtr[ 0].m_DrawRegion != stackPtr[-1].m_DrawRegion );

			// m_Counters must be a strict reverse ordered sequence, first m_Counters must be most advanced
			if	(stackPtr + 1 != stackEnd)
			{
				if (MustBreakCounterStack(stackPtr))
					return false;
			}
			else
			{
				dms_assert(m_Counters.size());
				int result = LexCompareLast(
					&*m_Counters.begin() + stackPtr[-1].m_StackBase, 
					&*m_Counters.begin() + stackPtr[ 0].m_StackBase, 
					m_CurrCounter
				);

#				if defined(MG_CHECK_PAST_BREAK)
				dms_assert( result >= 0 || HasBreakingStackSize() );
#				endif

				return (result > 0);
			}
			++stackPtr;
		}
		return true;
	}
#endif

CounterStacks::~CounterStacks()
{
	dms_assert(!m_NrActiveCounters); // there should be no active ResumableCounters
}

SizeT CounterStacks::IncLevel(SizeT firstValue)
{
	dms_assert(!Empty());

	auto counterPos = GetCurrStack().m_StackBase + m_NrActiveCounters;

	if (m_Counters.size() > counterPos)
		firstValue = m_Counters[counterPos]; // resume from suspended value
	else
		m_Counters.reserve(counterPos + 1); // make sure that suspension from RecursiveCounter dtor doesn't throw
	++m_NrActiveCounters;                   // we now expect RecursiveCounter ctor not to throw anymore so that the dtor will be called

	return firstValue;
}

void CounterStacks::Suspend(SizeT currValue, const ResumableCounter* prevCounter)
{
	dms_assert(!Empty());
	dms_assert(m_NrActiveCounters > 0);
	dms_assert(!DidBreak());

	auto counterPos = GetCurrStack().m_StackBase + --m_NrActiveCounters;
	auto counterSize = m_Counters.size();

	if (counterSize <= counterPos)
		m_Counters.insert(m_Counters.end(), counterPos+1 - counterSize, currValue);
	else
		m_Counters[counterPos] = currValue;

	m_CurrCounter = prevCounter;

	dbg_assert( IsOK() );
}

void CounterStacks::CloseCounter(const ResumableCounter* prevCounter)
{
	dms_assert(!Empty());
	dms_assert(NrActiveCounters() > 0);
//	dms_assert(NoSuspendedCounters());

	--m_NrActiveCounters;
	EraseSuspended();

	m_CurrCounter = prevCounter;

	dms_assert(NoSuspendedCounters());

	dbg_assert(IsOK());
}

void CounterStacks::EraseSuspended()
{
	dms_assert(!Empty());

	dms_assert(m_Counters.size() >= GetCurrStack().m_StackBase);

	auto nrCounters = GetCurrStack().m_StackBase + m_NrActiveCounters;

	if (m_Counters.size() > nrCounters)
		m_Counters.erase(m_Counters.begin()+nrCounters, m_Counters.end());

	dms_assert(NoSuspendedCounters());
}

//----------------------------------------------------------------------
// class  : ResumableCounter
//----------------------------------------------------------------------

ResumableCounter::ResumableCounter(CounterStacks* cs, bool markProgress)
	:	m_CounterStacksPtr(cs)
	,	m_AutoCounter(0)
	,	m_StopValue(UNDEFINED_VALUE(SizeT))
	,	m_PrevCounter(cs ? cs->GetCurrCounter() : 0)
	,	m_MarkProgress(markProgress && cs)
{
	if (cs)
	{
		dms_assert((m_PrevCounter && m_PrevCounter->IsOK()) || cs->NrActiveCounters() == 0);
		if (	!	cs->DidBreak()
			&&	(	(m_PrevCounter)
				?	(m_PrevCounter->m_AutoCounter >= m_PrevCounter->m_StopValue)
				:	cs->HasMultipleStacks()
				)
			)
		{
			if ( !m_PrevCounter || !m_PrevCounter->MustBreak() )
			{
				dms_assert(cs->GetNextStack().m_StackBase + cs->NrActiveCounters() < cs->m_Counters.size()); // guaranteed by ! cs->HasBreakingStackSize() || ! m_PrevCounter
				m_StopValue = cs->m_Counters[cs->GetNextStack().m_StackBase + cs->NrActiveCounters()]; // MustBreak at suspended value of next stack (this counter isn't counted yet)
			}
		}

		m_AutoCounter = cs->IncLevel(0);


#	if defined(MG_CHECK_PAST_BREAK)
		dms_assert( cs->IsOK() ); // check ordering still valid ( before SetCurrCounter )
		dms_assert( !cs->DidBreak() );
#	endif

		cs->SetCurrCounter(this); // no more throws in ctor expected, thus dtor WILL be called to reset 
		// may now be at or beyond next advance level (when MustBreak must become is true, but leave this to the client to decide)

		if (cs->DidBreak())
			m_StopValue = m_AutoCounter;
	}

	dms_assert( IsOK() );
	dms_assert( IsActive() );
}

ResumableCounter::~ResumableCounter()
{
	dms_assert( IsActive() );

	if (m_CounterStacksPtr)
	{
		if (m_CounterStacksPtr->DidBreak())
			m_CounterStacksPtr->CloseCounter(m_PrevCounter);
		else
			m_CounterStacksPtr->Suspend(m_AutoCounter, m_PrevCounter);
	}
}

void ResumableCounter::Close()
{
	dms_assert(IsActive());

	if (m_CounterStacksPtr)
		m_CounterStacksPtr->CloseCounter(m_PrevCounter);

	m_CounterStacksPtr = 0;
	MG_DEBUGCODE(m_AutoCounter = UNDEFINED_VALUE(UInt32));
}

bool ResumableCounter::MustBreak() const 
{
	assert( IsOK() );
	assert( IsActive() );

	assert(!m_CounterStacksPtr || !m_CounterStacksPtr->DidBreak() );   // after suspension we should return completely to DataView without more queries

	if (m_AutoCounter < m_StopValue)
		return false;

	assert(m_CounterStacksPtr);
	assert(m_CounterStacksPtr->HasMultipleStacks()); // else m_StopValue would not have been set

	assert(! m_CounterStacksPtr->m_DidBreak );

	bool result =  m_AutoCounter > m_StopValue
			||	m_CounterStacksPtr->HasBreakingStackSize(); // checks that next stack doesn't have more sub-counters

	if (!result)
		return false;

	m_CounterStacksPtr->m_DidBreak = true;
	return true;
}

bool ResumableCounter::MustBreakNext() const 
{
	assert( IsOK() );
	assert( IsActive() );

	assert(!m_CounterStacksPtr || !m_CounterStacksPtr->DidBreak() );   // after suspension we return completely to DataView without more queries

	if (m_AutoCounter < m_StopValue)
		return false;

	assert(m_CounterStacksPtr);
	assert(m_CounterStacksPtr->HasMultipleStacks()); // else m_StopValue would not have been set

#	if defined(MG_CHECK_PAST_BREAK)
	// same checkpoints as previous time
	dms_assert(m_AutoCounter == m_StopValue); 
#	endif

	m_CounterStacksPtr->m_DidBreak = true;
	return true;
}

bool ResumableCounter::MustBreakOrSuspend() const 
{
	return MustBreak() 
		||	(	m_CounterStacksPtr 
			&&	SuspendTrigger::MustSuspend()
			);
}

