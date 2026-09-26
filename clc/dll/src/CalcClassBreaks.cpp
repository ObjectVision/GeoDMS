// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "geom/Range.h"
#include "set/DataCompare.h"

#include "CalcClassBreaks.h"

#include "AbstrUnit.h"
#include "DataArray.h"
#include "ParallelTiles.h"
#include "UnitProcessor.h"


//----------------------------------------------------------------------
// class break functions
//----------------------------------------------------------------------

inline void Insert(std::vector<Float64>& faLimits, UInt32 pos, Float64 value)
{
	faLimits.insert(faLimits.begin()+pos, value);
}

CLC_CALL void ClassifyLogInterval(break_array& faLimits, SizeT k, const ValueCountPairContainer& vcpc)
{
	MakeMax(k, 1);
	faLimits.reserve(k);

	UInt32 m = vcpc.size();
	if (!m)
	{
		faLimits.push_back(0);
		return;
	}
	assert(m > 0);

	Range<Float64> valueRange(
		vcpc[  0].first,
		vcpc[m-1].first
	);
	assert(valueRange.first< valueRange.second); // follows from m>1 and postcondition of UpdateCounts()
	assert(valueRange.first >= 0); // PRECONDITION

	if ((valueRange.first==0) && (m>1))
		valueRange.first = vcpc[1].first;
	assert((valueRange.first > 0) || (m <= 1));

	UInt32 nReq = k-1; // nr requested breaks
//	assert(nReq>=1); // follows from PRECONDITION on k.
	
	Float64 fValue = 1;
	if(valueRange.first > 0)
	{
		while (fValue > valueRange.first * 10 ) fValue *= 0.1;
		assert(fValue <= valueRange.first*10);
		while (fValue <= valueRange.first)      fValue *= 10;
		assert(fValue <= valueRange.first*10);
		assert(fValue <= valueRange.first*10);
	}
	assert(fValue > valueRange.first);

	// determine initial classes based on pow(10), even if this increases nr of classes
	while (fValue <= valueRange.second)
	{
		faLimits.push_back(fValue);		
		fValue = fValue *10;
	}
	assert(fValue > valueRange.second);

	// adjust classes according to demand
	if (faLimits.size())
	{
		UInt32 nLastSplit = 0;
		if (IsIncluding(valueRange, 0.5 * fValue)) ++nLastSplit;
		if (IsIncluding(valueRange, 0.2 * fValue)) ++nLastSplit;
		while (IsIncluding(valueRange,0.1*faLimits[0]) && nReq > faLimits.size()*3+nLastSplit)
			Insert(faLimits, 0, 0.1*faLimits[0]);
	}

	// split classes
	UInt32 nCurSplit = faLimits.size();

	// split in 3
	while (nCurSplit > 0 && nReq > faLimits.size()+nCurSplit)
	{
		assert(nReq > faLimits.size() + 1);
		if (IsIncluding(valueRange, 0.5 * fValue)) Insert(faLimits, nCurSplit, 0.5 * fValue);
		if (IsIncluding(valueRange, 0.2 * fValue)) Insert(faLimits, nCurSplit, 0.2 * fValue);
		--nCurSplit;
		fValue *= 0.1;
	}

	// split in 2
	while (nCurSplit > 0 && nReq > faLimits.size())
	{
		if (IsIncluding(valueRange, 0.3 * fValue)) Insert(faLimits, nCurSplit, 0.3 * fValue); else // out of bounds? try other reasonable splits
		if (IsIncluding(valueRange, 0.5 * fValue)) Insert(faLimits, nCurSplit, 0.5 * fValue); else
		if (IsIncluding(valueRange, 0.2 * fValue)) Insert(faLimits, nCurSplit, 0.2 * fValue);
		--nCurSplit;
		fValue *= 0.1;
	}

	// insert first splits
	if (nReq > faLimits.size() && nCurSplit == 0) 
	{
		if (nReq == faLimits.size()+1 &&  IsIncluding(valueRange,0.3 * fValue))
			Insert(faLimits, nCurSplit, 0.3 * fValue);
		else
		if (IsIncluding(valueRange, 0.5 * fValue))
			Insert(faLimits, nCurSplit, 0.5 * fValue);
	}

	if (nReq > faLimits.size() && nCurSplit == 0)
		if (IsIncluding(valueRange, 0.2 * fValue) )
			Insert(faLimits, nCurSplit, 0.2 * fValue);

	// limit number of classes when we have reached max
	Insert(faLimits, 0, vcpc[0].first);
}

//----------------------------------------------------------------------
// breakAttr functions
//----------------------------------------------------------------------

void FillBreakAttrFromArray(AbstrDataItem* breakAttr, const break_array& data, const SharedObj* abstrValuesRangeData)
{
	auto breakAttrDomainCount = breakAttr->GetAbstrDomainUnit()->GetCount();
	DataWriteLock breakLock(breakAttr, (data.size() == breakAttrDomainCount) ? dms_rw_mode::write_only_all : dms_rw_mode::write_only_mustzero, abstrValuesRangeData);

	bool isOrdered = (data.size() >= breakAttrDomainCount);
	auto dataComp = DataLessThanCompare<Float64>();
	if (data.size())
	{
		auto dataIter = data.begin();
		while (isOrdered)
		{
			auto value = *dataIter;
			assert(IsDefined(value) || dataIter == data.begin());
			do
			{
				++dataIter;
				if (dataIter == data.end())
					goto endOfCheck;
			} while (!IsDefined(*dataIter)); // skip subsequent undefined values
			isOrdered = !dataComp(*dataIter, value); // only assign monotonously increasing classbreaks.
		}
	}
endOfCheck:

	breakAttr->m_StatusFlags.SetHasSortedValues(isOrdered);

	assert(data.size() == breakLock->GetNrFeaturesNow());
	breakLock->SetValuesAsFloat64Array(tile_loc(no_tile, 0), data.size(), begin_ptr(data));

	breakLock.Commit();
}

break_array ClassifyUniqueValues(const ValueCountPairContainer& vcpc, SizeT k)
{
	SizeT m = vcpc.size();
	assert(m <= GetTotalCount(vcpc));

	break_array result; result.reserve(k);

	MakeMin(m, k);
	SizeT j = 0;
	for (; j != m; ++j)
		result.emplace_back(vcpc[j].first);
	if (m)
		for (; j != k; ++j)
			result.emplace_back(vcpc[m - 1].first);
	else
		for (; j != k; ++j)
			result.emplace_back(UNDEFINED_VALUE(Float64));

	return result;
}

break_array ClassifyUniqueValues(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData)
{
	assert(breakAttr);

	auto ba = ClassifyUniqueValues(vcpc, breakAttr->GetAbstrDomainUnit()->GetCount());
	FillBreakAttrFromArray(breakAttr, ba, abstrValuesRangeData);
	return ba;
}

break_array ClassifyEqualInterval(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData)
{
	assert(breakAttr);

	DataWriteLock breakObj(breakAttr, vcpc.size() == breakAttr->GetAbstrDomainUnit()->GetCount() ? dms_rw_mode::write_only_all : dms_rw_mode::write_only_mustzero, abstrValuesRangeData);
	assert(breakAttr->GetDataObjLockCount() < 0);

	break_array ba;
	SizeT k = breakAttr->GetAbstrDomainUnit()->GetCount();
	if (k)
	{

		SizeT m = vcpc.size();
		if (m)
		{
			assert(m <= GetTotalCount(vcpc));

			Float64 minValue = vcpc[0].first;
			Float64 maxValue = vcpc[m - 1].first;

			assert(IsDefined(minValue));
			assert(IsDefined(maxValue));
			assert(minValue <= maxValue); // follows from m>1 and postcondition of UpdateCounts()

			Float64 delta = (k > 1) ? (maxValue - minValue) / (k - 1) : 0;
			assert(delta >= 0); // follows from previous assertions

			for (SizeT j = 0; j != k; ++j)
			{
				ba.emplace_back(minValue);
				breakObj->SetValueAsFloat64(j, minValue);
				minValue += delta;
			}
		}
		else
			for (SizeT j = 0; j != k; ++j)
				breakObj->SetValueAsFloat64(j, UNDEFINED_VALUE(Float64));
	}
	breakObj.Commit();
	return ba;
}

break_array ClassifyNZEqualInterval(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData)
{
	assert(breakAttr);
	SizeT k = breakAttr->GetAbstrDomainUnit()->GetCount();
	if (k < 2 || !vcpc.size() || vcpc[0].first > 0)
		return ClassifyEqualInterval(breakAttr, vcpc, abstrValuesRangeData);

	DataWriteLock breakObj(breakAttr, vcpc.size() == breakAttr->GetAbstrDomainUnit()->GetCount() ? dms_rw_mode::write_only_all : dms_rw_mode::write_only_mustzero, abstrValuesRangeData);
	assert(breakAttr->GetDataObjLockCount() < 0);

	break_array ba;
	SizeT m = vcpc.size();
	assert(m);

	assert(m <= GetTotalCount(vcpc));
	SizeT mz = 0; while (mz < m && vcpc[mz].first < 0.0) ++mz;
	bool hasNegative = (mz > 0);

	Float64 minValueN = 0.0, maxValueN = 0.0;
	if (hasNegative)
	{
		minValueN = vcpc[0].first;      MG_CHECK(minValueN < 0.0);
		maxValueN = vcpc[mz - 1].first; MG_CHECK(maxValueN < 0.0);
	}
	assert(IsDefined(minValueN));
	assert(IsDefined(maxValueN));
	assert(minValueN <= maxValueN);

	auto kk = k;
	bool hasZero = vcpc[mz].first == 0.0 && k > 2; // if k==2 we just treat zero as a positive number
	if (hasZero)
	{
		++mz;
		--kk;
	}
	bool hasPositive = (mz < m);

	Float64 minValueP = 0.0;
	Float64 maxValueP = 0.0;
	if (hasPositive)
	{
		minValueP = vcpc[mz].first;
		maxValueP = vcpc[m - 1].first;
	}

	Float64 deltaN = (maxValueN - minValueN);
	Float64 deltaP = (maxValueP - minValueP);

	SizeT kn = hasNegative ? 1 : 0;
	if (minValueN == maxValueN)
		;
	else if (minValueP == maxValueP)
	{
		kn = kk - (hasPositive ? 1 : 0);
	}
	else
	{
		auto minDelta = Max(deltaN, deltaP);
		for (SizeT ko = 1; ko + 1 != kk; ++ko)
		{
			auto currDelta = Max(deltaN / ko, deltaP / (kk - ko));
			if (currDelta <= minDelta)
			{
				kn = ko;
				minDelta = currDelta;
			}
		}
	}
	auto kp = kk - kn;
	if (kn > 1) deltaN /= kn;
	if (kp > 1) deltaP /= kp;

	while (kn--)
	{
		ba.emplace_back(minValueN);
		minValueN += deltaN;
	}
	if (hasZero)
		ba.emplace_back(0.0);
	while (kp--)
	{
		ba.emplace_back(minValueP);
		minValueP += deltaP;
	}
	for (SizeT j=0; j!= ba.size(); ++j)
		breakObj->SetValueAsFloat64(j, ba[j]);

	breakObj.Commit();
	return ba;
}

break_array ClassifyLogInterval(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData)
{
	assert(breakAttr);

	DataWriteLock breakObj(breakAttr, dms_rw_mode::write_only_all, abstrValuesRangeData);
	assert(breakAttr->GetDataObjLockCount() < 0);

	UInt32 k = breakAttr->GetAbstrDomainUnit()->GetCount();
	UInt32 m = vcpc.size();

	assert(m <= GetTotalCount(vcpc));

	break_array faLimits;
	if (m)
	{
		::ClassifyLogInterval(faLimits, k, vcpc);

		UInt32 kk = faLimits.size();
		MakeMin(kk, k);
		assert(kk <= k);

		assert(kk > 0);

		SizeT j = 0;
		for (; j != kk; ++j)
			breakObj->SetValueAsFloat64(j, faLimits[j]);
		for (; j != k; ++j)
			breakObj->SetValueAsFloat64(j, faLimits[kk - 1]);
	}
	else
		for (SizeT j = 0; j != k; ++j)
			breakObj->SetValueAsFloat64(j, UNDEFINED_VALUE(Float64));
	breakObj.Commit();
	return faLimits;

}

// Appends to ba the first values of k classes of vcpc[b], ..., vcpc[e-1] whose counts are as equal as possible; k <= e-b.
static void AppendEqualCountBreaks(break_array& ba, const ValueCountPairContainer& vcpc, SizeT b, SizeT e, SizeT k)
{
	assert(b + k <= e);

	CountType n = 0;
	for (SizeT i = b; i != e; ++i)
		n += vcpc[i].second;

	CountType c = 0, cc = 0; // the count of the values before i, and the count from which class j may start
	SizeT i = b;
	for (SizeT j = 0; j != k; ++j)
	{
		SizeT maxI = e - (k - j); // leaves a value for class j and for each class after it
		while (c < cc && i < maxI)
			c += vcpc[i++].second;
		assert(i < e);

		ba.emplace_back(vcpc[i].first);
		cc = c + (n - c) / (k - j);
	}
}

break_array ClassifyEqualCount(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData)
{
	DataWriteLock breakObj(breakAttr, dms_rw_mode::write_only_all, abstrValuesRangeData);

	SizeT k = breakAttr->GetAbstrDomainUnit()->GetCount();
	SizeT m = vcpc.size();
	assert(m <= GetTotalCount(vcpc)); // #(PRECONDITION: vcpc == unique value(themeAttr)) <= #themekAttr

	break_array ba; ba.reserve(Min(k, m));
	AppendEqualCountBreaks(ba, vcpc, 0, m, Min(k, m));

	Float64 breakValue = ba.empty() ? UNDEFINED_VALUE(Float64) : ba.back();
	SizeT j = 0;
	for (; j != ba.size(); ++j)
		breakObj->SetValueAsFloat64(j, ba[j]);
	for (; j != k; ++j)
		breakObj->SetValueAsFloat64(j, breakValue);

	breakObj.Commit();
	return ba;
}

// 0 is treated as ClassifyNonzeroJenksFisher treats it: a compulsory break, with a class of its own when there is room for one,
// and the negative and the positive values are classified apart, each side by equal count over its own values.
break_array ClassifyNZEqualCount(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData)
{
	SizeT k = breakAttr->GetAbstrDomainUnit()->GetCount();
	SizeT m = vcpc.size();

	// data of one sign, and data with no more distinct values than classes, in which every value, 0 included, gets a class of its own
	if (k < 2 || k >= m || vcpc[0].first > 0 || vcpc[m - 1].first < 0)
		return ClassifyEqualCount(breakAttr, vcpc, abstrValuesRangeData);

	SizeT mn = 0; // the number of negative values
	while (vcpc[mn].first < 0)
		++mn;
	bool hasZero = (vcpc[mn].first == 0);
	SizeT bp = hasZero ? mn + 1 : mn; // the first positive value
	SizeT mp = m - bp;

	break_array ba; ba.reserve(k);
	if (k == 2 && hasZero && mn && mp)
	{
		// Two classes leave no room for a negative, a zero and a positive class: 0 stays a break and shares the second class with the
		// positive values, as in ClassifyNonzeroJenksFisher and ClassifyNZEqualInterval.
		ba = { vcpc[0].first, 0.0 };
	}
	else
	{
		// Issue #1146: as in ClassifyNonzeroJenksFisher, data that straddles 0 without containing it also gets a class at 0, which then
		// holds no values, so that a diverging palette is anchored on 0.
		bool zeroClass = hasZero || (mn && mp && k >= 3);
		SizeT kk = k - (zeroClass ? 1 : 0); // the classes of the negative and the positive values
		SizeT kn = mp ? 0 : kk;             // the classes of the negative values
		if (mn && mp)
		{
			// Of the splits that give each side at least one class and no more classes than it has values, take the one that minimises the
			// largest average class count, as ClassifyNZEqualInterval does with class widths. On a tie, the positive values get the extra class.
			CountType nn = 0, np = 0;
			for (SizeT i = 0; i != mn; ++i)
				nn += vcpc[i].second;
			for (SizeT i = bp; i != m; ++i)
				np += vcpc[i].second;

			Float64 minMaxCount = MaxValue<Float64>();
			for (SizeT ko = (kk > mp) ? kk - mp : 1; ko <= Min(mn, kk - 1); ++ko)
			{
				Float64 maxCount = Max(Float64(nn) / ko, Float64(np) / (kk - ko));
				if (maxCount < minMaxCount)
				{
					minMaxCount = maxCount;
					kn = ko;
				}
			}
		}
		AppendEqualCountBreaks(ba, vcpc, 0, mn, kn);
		if (zeroClass)
			ba.emplace_back(0.0);
		AppendEqualCountBreaks(ba, vcpc, bp, m, kk - kn);
	}
	assert(ba.size() == k);

	FillBreakAttrFromArray(breakAttr, ba, abstrValuesRangeData);
	return ba;
}

struct JenksFisher
{
	JenksFisher(const ValueCountPairContainer& vcpc, SizeT k)
		:	m_M(vcpc.size())
		,	m_K(k)
		,	m_BufSize(vcpc.size()+1-k)
		,	m_PrevSSM(new ClassBreakValueType[m_BufSize])
		,	m_CurrSSM(new ClassBreakValueType[m_BufSize])
		,	m_CB     ( new SizeT[m_BufSize * (m_K-1)])
		,	m_CBPtr()
	{
		DBG_START("JenksFisher", "JenksFisher", MG_DEBUG_CLASSBREAKS);
		m_CumulValues.reserve(vcpc.size() MG_DEBUG_ALLOCATOR_SRC("JenksFischer CumulValues"));
		Float64    cwv=0;
		CountType  cw =0, w;

		// Accumulate w*(v-s), with s the weighted median. Over the classes of any partition of a prefix [0, e],
		// sum (sum w(v-s))^2 / sum w = sum (sum wv)^2 / sum w - 2s sum_[0,e] wv + s^2 sum_[0,e] w, and the added terms do
		// not depend on the breaks, so every argmax stays where it was. Uncentred, the terms grow with the square of the
		// values: around 3e9 with a spread of 1000, their rounding exceeded the differences between candidate breaks,
		// which gave wrong breaks and tripped the assert in CalcRange. s is a data value, so integral values stay integral.
		const Float64 s = vcpc[WeightedMedianIndex(vcpc)].first;

		for(SizeT i=0; i!=m_M; ++i)
		{
			w   = vcpc[i].second;
			assert(w > 0);

			cw += w; 
			assert(cw >= w); // no overflow?

			assert(!i || vcpc[i-1].first < vcpc[i].first);
			cwv+= w * (vcpc[i].first - s);
			m_CumulValues.push_back(ValueCountPair<Float64>(cwv, cw) MG_DEBUG_ALLOCATOR_SRC("JenksFischer CumulValues"));

			if (i < m_BufSize)
				m_PrevSSM[i] = cwv * cwv / cw; // prepare SSM for first class, last m_K values can be omitted since they never belong to the first class
			DBG_TRACE(("m_PrevSSM[{}]={:f}", i, Float32(m_PrevSSM[i])));
		}
	}

	// the index of the first value at which the cumulative count reaches half of the total count
	static SizeT WeightedMedianIndex(const ValueCountPairContainer& vcpc)
	{
		assert(vcpc.size());
		CountType totalCount = GetTotalCount(vcpc), cw = vcpc[0].second;
		SizeT i = 0;
		while (cw < totalCount - cw)
			cw += vcpc[++i].second;
		return i;
	}

	Float64 GetW(SizeT b, SizeT e)
	{
		assert(b<=e);
		assert(e<m_M);

		Float64 res  = m_CumulValues[e].second;
		if (b)  res -= m_CumulValues[b-1].second;
		return res;
	}

	Float64 GetWV(SizeT b, SizeT e)
	{
		assert(b<=e);
		assert(e<m_M);

		Float64 res  = m_CumulValues[e].first;
		if (b)  res -= m_CumulValues[b-1].first;
		return res;
	}

	Float64 GetSSM(SizeT b, SizeT e)
	{
		Float64 res = GetWV(b,e);
		return res * res / GetW(b,e);
	}
	auto FindMaxBreakIndex(SizeT i, SizeT bp, SizeT ep) -> std::pair<SizeT, Float64>
	{
		DBG_START("JenksFisher", "FindMinBreakIndex", MG_DEBUG_CLASSBREAKS);
		DBG_TRACE(("i={} bp={} ep-{}", i, bp, ep));
		assert(bp < ep);
		assert(bp <= i);
		assert(ep <= i+1);
		assert(i  <  m_BufSize);
		assert(ep <= m_BufSize);

		Float64 minSSM = m_PrevSSM[bp] + GetSSM(bp+m_NrCompletedRows, i+m_NrCompletedRows);
		DBG_TRACE(("{:f} = prevSSM[{}] + GetSSM({}) = {:f} + {:f}", Float32(minSSM), bp, i-bp, Float32(m_PrevSSM[bp]), Float32(minSSM-m_PrevSSM[bp])));

		SizeT foundP = bp;
		while (++bp < ep)
		{
			Float64 currSSM = m_PrevSSM[bp] + GetSSM(bp+m_NrCompletedRows, i+m_NrCompletedRows);
			DBG_TRACE(("{:f} = prevSSM[{}] + GetSSM({}) = {:f} + {:f}", Float32(currSSM), bp, i-bp, Float32(m_PrevSSM[bp]), Float32(currSSM-m_PrevSSM[bp])));
			if (currSSM > minSSM)
			{
				DBG_TRACE(("foundP={} increases from {:f} to {:f}", bp, Float32(minSSM), Float32(currSSM)));

				minSSM = currSSM;
				foundP = bp;
			}
		}
		m_CurrSSM[i] = minSSM;
		return { foundP, minSSM };
	}

	// for the assert in CalcRange: does candidate p score as well as maxSSM, the best for i, up to rounding?
	bool IsTie(SizeT i, SizeT p, Float64 maxSSM)
	{
		Float64 ssm = m_PrevSSM[p] + GetSSM(p+m_NrCompletedRows, i+m_NrCompletedRows);
		return ssm >= maxSSM - maxSSM * 1e-12;
	}

	void CalcRange(SizeT bi, SizeT ei, SizeT bp, SizeT ep)
	{
		DBG_START("JenksFisher", "CalcRange", MG_DEBUG_CLASSBREAKS);
		DBG_TRACE(("bi={} ei={} bp={} ep={}", bi, ei, bp, ep));

		assert(bi <= ei);

		assert(ep <= ei);
		assert(bp <= bi);

		if (bi == ei)
			return;
		assert(bp < ep);

		SizeT mi = (bi + ei)/2;
		auto [mp, minSSM] = FindMaxBreakIndex(mi, bp, Min<SizeT>(ep, mi+1));

		assert(bp <= mp);
		assert(mp <  ep);
		assert(mp <= mi);
		
		CalcRange(bi, mi, bp, Min<SizeT>(mi, mp+1));

#if !defined(MG_ASSUME_CB_INC)
		// CB(i, j-1) <= CB(i, j): adding a class does not move the last break to the left. In exact arithmetic that holds
		// when every row breaks ties alike, but rounding breaks exact ties, common in integral data with repeated gaps,
		// either way; so a violation passes when the candidate that satisfies it scores as well as mp, up to rounding.
		assert(m_NrCompletedRows==1 || (mi+1) == m_BufSize|| (mp+1) >= (m_CBPtr-m_BufSize)[mi+1]
			|| IsTie(mi, (m_CBPtr-m_BufSize)[mi+1]-1, minSSM));
#endif
		m_CBPtr[ mi ] = mp;
		CalcRange(mi+1, ei, mp, ep);
	}

	void CalcCB()
	{
		DBG_START("JenksFisher", "CalcCB", MG_DEBUG_CLASSBREAKS);
		if (m_K>=2)
		{
			m_CBPtr = m_CB.get();
			for (m_NrCompletedRows=1; m_NrCompletedRows<m_K-1; ++m_NrCompletedRows)
			{
				DBG_TRACE(("m_NrCompletedRows={}", m_NrCompletedRows));

				assert(std::find(m_PrevSSM.get(), m_PrevSSM.get() + m_BufSize, -9999.0 ) == m_PrevSSM.get() + m_BufSize);

				CalcRange(0, m_BufSize, 0, m_BufSize);

				m_PrevSSM.swap(m_CurrSSM);
				m_CBPtr += m_BufSize;

				MG_DEBUGCODE(  fast_fill(m_CurrSSM.get(), m_CurrSSM.get() + m_BufSize, -9999.0 ) );
			}
		}
	}

	auto GetBreaks(const ValueCountPairContainer& vcpc) -> std::pair< break_array, ClassBreakValueType>
	{
		break_array result(m_K);

		ClassBreakValueType lastSSM = 0;
		if (m_K > 1)
		{
			CalcCB();

			SizeT* cbPtr = m_CBPtr;
			auto [lastClassBreakIndex, minSSM] = FindMaxBreakIndex(m_BufSize - 1, 0, m_BufSize);
			if (lastSSM == 0)
				lastSSM = minSSM;
			SizeT k = m_K;
			while (--k)
			{
				assert(k);
				//			DBG_TRACE(("Break[{}]=vcpc[{}]={:f}", k, lastClassBreakIndex+k, Float32(vcpc[lastClassBreakIndex+k].first)));
				result[k] = vcpc[lastClassBreakIndex + k].first;
				assert(lastClassBreakIndex < m_BufSize);
				if (k > 1)
				{
					cbPtr -= m_BufSize;
					lastClassBreakIndex = cbPtr[lastClassBreakIndex];
				}
			}
			assert(cbPtr == m_CB.get());
		}
		else
			lastSSM = GetSSM(0, m_M-1);
		result[0] = vcpc[0].first;
		return { result, lastSSM };
	}

	SizeT                   m_M, m_K, m_BufSize;
	ValueCountPairContainer m_CumulValues;

	std::unique_ptr<ClassBreakValueType[]> m_PrevSSM;
	std::unique_ptr<ClassBreakValueType[]> m_CurrSSM;
	std::unique_ptr<SizeT[]>  m_CB;
	SizeT*                 m_CBPtr;

	SizeT                  m_NrCompletedRows = 0;
};

break_array ClassifyCRJenksFisher(const ValueCountPairContainer& vcpc, SizeT kk)
{
	DBG_START("ClassifyJenksFisher", "", MG_DEBUG_CLASSBREAKS);

	SizeT m = vcpc.size();

	if (kk >= m)
		return ClassifyUniqueValues(vcpc, kk);

	if (!kk)
		return{};

	JenksFisher jf(vcpc, kk);
	return jf.GetBreaks(vcpc).first;
}

break_array ClassifyJenksFisher(const ValueCountPairContainer& vcpc, SizeT kk, bool separateZero)
{
	SizeT m = vcpc.size();
	if (kk >= m)
		return ClassifyUniqueValues(vcpc, kk);
	if (!separateZero || kk < 2 || (kk == 2 && (vcpc[0].first > 0 || vcpc.back().first < 0)))
		return ClassifyCRJenksFisher(vcpc, kk);

	DBG_START("ClassifyNonzeroJenksFisher", "", MG_DEBUG_CLASSBREAKS);

	SizeT firstPositivePos = 0;
	while (firstPositivePos < vcpc.size() && vcpc[firstPositivePos].first <= 0)
		++firstPositivePos;
	assert(firstPositivePos == vcpc.size() || vcpc[firstPositivePos].first > 0);

	auto positiveValues = ValueCountPairContainer(vcpc.begin() + firstPositivePos, vcpc.end() MG_DEBUG_ALLOCATOR_SRC("ClassifyJenksFischer"));
	CountType zeroCount = 0;
	SizeT firstNonnegativePos = firstPositivePos;
	if (firstPositivePos > 0 && vcpc[firstPositivePos - 1].first == 0)
		zeroCount = vcpc[--firstNonnegativePos].second;
	bool hasZeroClass = (zeroCount > 0);

	auto negativeValues = ValueCountPairContainer(vcpc.begin(), vcpc.begin() + firstNonnegativePos MG_DEBUG_ALLOCATOR_SRC("ClassifyJenksFischer"));

	// Issue #1146: a diverging color palette anchors "white" on a class-break whose value is exactly 0.
	// The exact-zero test above only fires when the data contains a literal 0 value; signed data with a
	// near-zero mass but no exact 0 (e.g. floating-point residue) then gets no 0-break and the white is
	// off-center. Also force a break at 0 whenever the data straddles zero (both negative and positive
	// values present, with room for a negative + zero + positive class), so it gets a 0-centered classification.
	bool insertZeroBreak = hasZeroClass || (negativeValues.size() != 0 && positiveValues.size() != 0 && kk >= 3);

	// Two classes leave no room for a negative, a zero and a positive class, and the splits below would then leave one
	// side without a class. The break at 0 stays and the first break is the minimum, so the negative values get the
	// first class and 0 shares the second with the positive values, as in ClassifyNZEqualInterval.
	if (kk == 2 && hasZeroClass && negativeValues.size() != 0)
		return { negativeValues[0].first, 0.0 };

	if (negativeValues.size() <= 1)
	{
		auto result = ClassifyCRJenksFisher(positiveValues, kk - negativeValues.size() - (insertZeroBreak ? 1 : 0));
		if (insertZeroBreak)
			result.insert(result.begin(), 0);
		if (negativeValues.size())
			result.insert(result.begin(), negativeValues[0].first);
		return result;
	}
	if (positiveValues.size() <= 1)
	{
		auto result = ClassifyCRJenksFisher(negativeValues, kk - positiveValues.size() - (insertZeroBreak ? 1 : 0));
		if (insertZeroBreak)
			result.insert(result.end(), 0);
		if (positiveValues.size())
			result.insert(result.end(), positiveValues[0].first);
		return result;
	}

	break_array result(kk);
	SizeT nrNegativeClasses = 1;
	ClassBreakValueType maxSSM = MinValue<ClassBreakValueType>(); // a side's SSM is centred on its median: one class can have 0
	for(;; nrNegativeClasses++)
	{
		SizeT nrPositiveClasses = kk - nrNegativeClasses - (insertZeroBreak ? 1 : 0);
		assert(nrPositiveClasses >= 1);
		if (nrPositiveClasses > positiveValues.size())
			continue;
		if (nrNegativeClasses > negativeValues.size())
			break;

		assert(nrPositiveClasses >= 1);
		JenksFisher njf(negativeValues, nrNegativeClasses), pjf(positiveValues, nrPositiveClasses);
		auto [res1, ssm1] = njf.GetBreaks(negativeValues);
		auto [res2, ssm2] = pjf.GetBreaks(positiveValues);
		if (ssm1 + ssm2 > maxSSM)
		{
			maxSSM = ssm1 + ssm2;
			auto resIter = std::copy(res1.begin(), res1.end(), result.begin());
			if (insertZeroBreak)
				*resIter++ = 0;
			resIter = std::copy(res2.begin(), res2.end(), resIter);
			assert(resIter == result.end());
		}
		if (nrPositiveClasses == 1)
			break;
	}
	return result;
}

break_array ClassifyJenksFisher(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData, bool separateZero)
{
	assert(breakAttr);

	auto ba = ClassifyJenksFisher(vcpc, breakAttr->GetAbstrDomainUnit()->GetCount(), separateZero);
	FillBreakAttrFromArray(breakAttr, ba, abstrValuesRangeData);
	return ba;
}
