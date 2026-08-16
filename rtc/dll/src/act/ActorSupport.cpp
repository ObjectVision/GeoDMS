// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// Small satellites of the Actor update mechanism, merged (2026-08):
// garbage_can, ActorEnums (flag_set accessors), Waiter, ActorSet.

// ==== from garbage_can.cpp ====

#include "act/garbage_can.h"

void garbage_can::clear() {
    for (auto& [_, bin] : bins) {
        if (bin.destroy) {
            bin.destroy(reinterpret_cast<std::byte*>(bin.storage.data()), bin.count);
        }
    }
    bins.clear();
}

void garbage_can::merge_from(garbage_can&& other) 
{
    for (auto& [tid, other_bin] : other.bins) {
        auto& bin = bins[tid];

        if (bin.storage.empty()) {
            bin = std::move(other_bin);
            continue;
        }

        assert(bin.stride == other_bin.stride);

        bin.ensure_capacity(bin.count + other_bin.count);

        std::byte* src = reinterpret_cast<std::byte*>(other_bin.storage.data());
        std::byte* dst = reinterpret_cast<std::byte*>(bin.storage.data());

        std::size_t dst_offset = bin.count * bin.stride;
        std::size_t src_bytes = other_bin.count * other_bin.stride;

        std::memmove(dst + dst_offset, src, src_bytes);

        bin.count += other_bin.count;
    }

    other.bins.clear();
}




// ==== from ActorEnums.cpp ====

#include "act/ActorEnums.h"

//----------------------------------------------------------------------
// struct flag_set
//----------------------------------------------------------------------

UInt32 flag_set::GetBits(UInt32 sf) const
{
	return m_DW & sf;
}

void flag_set::Set(UInt32 sf)
{
	m_DW |= sf;
}

void flag_set::SetBits(UInt32 sf, UInt32 values)
{
	assert(!(values & ~sf));

	UInt32 oldDW = m_DW;
	while (!m_DW.compare_exchange_weak(oldDW, (oldDW & ~sf) | values))
	{}
}

void flag_set::Clear(UInt32 sf) 
{
	m_DW &= ~sf;
}

void flag_set::Toggle (UInt32 sf) 
{ 
	m_DW ^= sf;
}

RTC_CALL CharPtr FailStateName(FailType fs)
{
	switch (fs)
	{
	case FailType::None: return "None";
	case FailType::Determine: return "DetermineState Failed";
	case FailType::MetaInfo: return "MetaInfo Failed";
	case FailType::Data: return "Primary Data Derivation Failed";
	case FailType::Validate: return "Validation (Integrity Check) Failed";
	case FailType::Committed: return "Committing Data (writing to storage) Failed";
	}
	return "unrecognized";
}



// ==== from Waiter.cpp ====

#include "act/Waiter.h"

#include <set>
#include <tuple>

#include <assert.h>
#include "act/MainThread.h"

using callback_record = std::tuple< wating_event_callback, wating_event_callback, void* >;

static std::atomic<UInt32> s_WaiterCount = 0;
static std::set<callback_record> s_WaitingCallbacks;


//----------------------------------------------------------------------
// config section
//----------------------------------------------------------------------

static bool g_BusyMode;

bool IsBusy()
{
	return g_BusyMode;
}

void SetBusy(bool v)
{
	g_BusyMode = v;
}

void Waiter::start(AbstrMsgGenerator* ach)
{
	assert(IsMetaThread());
	if (m_is_counted)
		return;
	m_is_counted = true;
	m_ContextGenerator = ach;

	if (s_WaiterCount++)
		return;

	SetBusy(true);
	for (const auto& we : s_WaitingCallbacks)
		if (std::get<0>(we))
			std::get<0>(we)(std::get<2>(we), m_ContextGenerator);

}

void Waiter::end()
{
	assert(IsMetaThread());
	if (!m_is_counted)
		return;
	m_is_counted = false;

	if (--s_WaiterCount)
		return;

	for (const auto& we : s_WaitingCallbacks)
	{
		auto onEndWaitingFunc = std::get<1>(we);
		if (!onEndWaitingFunc)
			continue;
		auto clientHandle = std::get<2>(we);
		onEndWaitingFunc(clientHandle, m_ContextGenerator);
	}
	SetBusy(false);
}

bool Waiter::IsWaiting()
{
	return s_WaiterCount;
}


void register_overlapping_periods_callback(wating_event_callback starting, wating_event_callback ending, void* clientHandle)
{
	assert(IsMetaThread());
	s_WaitingCallbacks.insert(callback_record( starting, ending, clientHandle ));
}

void unregister_overlapping_periods_callback(wating_event_callback starting, wating_event_callback ending, void* clientHandle)
{
	assert(IsMetaThread());
	s_WaitingCallbacks.erase(callback_record(starting, ending, clientHandle));
}



// ==== from ActorSet.cpp ====

#include "act/ActorSet.h"

#if defined(MG_DEBUG_UPDATESOURCE)

#include "act/Actor.h"
#include "act/ActorVisitor.h"
#include "act/UpdateMark.h"
#include "dbg/SeverityType.h"
#include "set/VectorFunc.h"

#include <algorithm>

// *****************************************************************************
// Section:     ActorSet helper funcs for SupplInclusionTester
// *****************************************************************************

class ActorSetType : public std::set<const Actor*>  {};

static void storeAllSuppliers(const Actor* self, SupplierVisitFlags svf, ActorSetType& itemSet)
{
	VisitSupplProcImpl(self, svf, 
		[&] (const Actor* supplier)
		{
			if (supplier->IsPassor())
				return;
			ActorSetType::iterator i = itemSet.lower_bound(supplier);
			if (i == itemSet.end() || (*i) != supplier)
			{
				dms_assert(supplier->m_LastGetStateTS >= UpdateMarker::LastTS() );
				itemSet.insert(i, supplier);
				storeAllSuppliers(supplier, svf, itemSet);
			}
		}
	);
}

static void GetCompleteSupplierSet(const Actor* self, SupplierVisitFlags svf, ActorVectorType& buffer)
{
	ActorSetType supplSet;
	dms_assert(self->m_LastGetStateTS >= UpdateMarker::LastTS() );
	supplSet.insert(self);
	storeAllSuppliers(self, svf, supplSet);
	set2vector(supplSet, buffer);
}

// *****************************************************************************
// Section:     MG_DEBUG_UPDATESOURCE
// *****************************************************************************

static SupplInclusionTester* s_Active = 0;

SupplInclusionTester::SupplInclusionTester(const Actor* actor): m_Prev(s_Active)
{
	GetCompleteSupplierSet(actor, *this);
	if (s_Active)
	{
		// report suppliers that were not included in s_Active
		ActorVectorType::const_iterator
			f1 = s_Active->begin(), f2 = begin(),
			l1 = s_Active->end  (), l2 = end();
		while (true)
		{
			if (f2 == l2)
				break;
			if (f1==l1 || *f2 < *f1)
			{
				reportD(ST_MajorTrace, "Intransitive supplier: ", (*f2)->GetSourceName().c_str());
				dms_assert(0);
				++f2;
			}
			else if (*f1< *f2)
				++f1;
			else
			{	// advance both
				++f1;
				++f2;
			}
		}
		dms_assert(std::includes(s_Active->begin(), s_Active->end(), begin(), end()) );
	}
	s_Active = this;
}

SupplInclusionTester::~SupplInclusionTester()
{
	s_Active = m_Prev;
}

bool SupplInclusionTester::ActiveDoesContain(const Actor* actor)
{
	return !s_Active 
		|| std::binary_search(s_Active->begin(), s_Active->end(), actor);
}

#endif // defined(MG_DEBUG_UPDATESOURCE)
