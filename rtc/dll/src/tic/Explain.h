// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if !defined(__TIC_EXPLAIN_H)
#define __TIC_EXPLAIN_H

#include "TicBase.h"

using AbstrValueRef = std::unique_ptr<AbstrValue>;

namespace Explain {
	struct CalcExplImpl;

	// One (row, value) pair as shown on a value-info page.
	// The members are named first/second because this used to be a std::pair<SizeT, AbstrValueRef>.
	// #612: m_Reason optionally holds a short note on why the value is what it is, recorded by an
	// operator that can explain its result (see SetValueReason). It is shown next to the value and is
	// what tells a user why e.g. an invert() row has no related row, instead of leaving them guessing.
	struct CoordinateType
	{
		CoordinateType(SizeT index, AbstrValueRef value)
			:	first(index)
			,	second(std::move(value))
		{}

		SizeT         first;
		AbstrValueRef second;
		SharedStr     m_Reason;
	};

	struct Context
	{
		CalcExplImpl*    m_CalcExpl;
		const AbstrUnit* m_Domain;
		CoordinateType*  m_Coordinate;
	};

	// magic constants
	const UInt32 MaxNrEntries = 6;
	const UInt32 MaxLevel     = 3;

	struct CalcExplImpl;
	struct CalcExplanations;

	struct NonStaticCalcExplanations
	{
		NonStaticCalcExplanations(OutStreamBase& xmlOutStr, const AbstrDataItem* studyObject, SizeT index, CharPtr extraInfo);
		bool ProcessQueue();
		void WriteDescr();

		std::unique_ptr<CalcExplImpl>     m_Impl;
		std::unique_ptr<CalcExplanations> m_Interface;
		SharedDataItem   m_StudyObject;
	};


	using context_handle = std::unique_ptr<Explain::CalcExplImpl, void (*)(Explain::CalcExplImpl*)>;
	TIC_CALL context_handle CreateContext();
	TIC_CALL void AddQueueEntry(Explain::CalcExplImpl* explImpl, const AbstrUnit* domain, SizeT index);

	// #612: record why the value now being explained is what it is. Called from an operator's
	// CalcResult while it explains one result element; the note ends up on that element's coordinate
	// and is shown next to the value. The first note wins, so an operator can state its reason
	// without having to know whether a nested one already did.
	TIC_CALL void SetValueReason(Explain::Context* context, SharedStr reason);
	TIC_CALL bool AttrValueToXML(Explain::CalcExplImpl* context, const AbstrDataItem* studyObject, OutStreamBase* xmlOutStrPtr, SizeT index, CharPtr extraInfo, bool bShowHidden);
}


#endif //!defined(__TIC_EXPLAIN_H)
