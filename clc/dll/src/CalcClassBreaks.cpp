// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "geo/Range.h"
#include "set/DataCompare.h"
#include "ASync.h"

#include "CalcClassBreaks.h"

#include "AbstrUnit.h"
#include "DataArray.h"
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
		while (fValue > valueRange.first * 10 ) fValue *= 0.1; assert(fValue <= valueRange.first*10);
		while (fValue <= valueRange.first)      fValue *= 10;  assert(fValue <= valueRange.first*10);
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
	DataWriteLock breakLock(breakAttr, data.size() == breakAttr->GetAbstrDomainUnit()->GetCount() ? dms_rw_mode::write_only_all : dms_rw_mode::write_only_mustzero, abstrValuesRangeData);

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
	auto mz = 0; while (mz < m && vcpc[mz].first < 0.0) ++mz; 
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

break_array ClassifyEqualCount(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData)
{
	DataWriteLock breakObj(breakAttr, dms_rw_mode::write_only_all, abstrValuesRangeData);

	SizeT k = breakAttr->GetAbstrDomainUnit()->GetCount();

	SizeT  m = vcpc.size(), n = GetTotalCount(vcpc);

	UInt32 kk = Min<UInt32>(k,m);
//	assert(kk>=1); // follows from previous asserts + assignemnt
	assert(kk<=m); // follows from assignment
	assert(m  <= n); // #(PRECONDITION: vcpc == unique value(themeAttr)) <= #themekAttr
	assert(kk <= n); // follows from previous asserts

	UInt32 c = 0, cc=0;
	UInt32 i =0;

	Float64 breakValue = UNDEFINED_VALUE(Float64);
	break_array ba; ba.reserve(kk);

	SizeT j =0;
	for (; j != kk; ++j)
	{
		assert(m+j>=kk); // follows from previous assert and positivity of j
		UInt32 maxI = m+j-kk; // (m-i) > (kk-j)
		assert(maxI < m);  // j < kk
		while (c<cc && i < maxI)
		{
			assert(i < m);
			c += vcpc[i].second;
			++i;
		}
		assert(i<m);

		breakValue = vcpc[i].first;

		ba.emplace_back(breakValue);
		breakObj->SetValueAsFloat64(j, breakValue );
		cc = c + (n -c)/(kk-j);
	}
	for (; j != k; ++j)
		breakObj->SetValueAsFloat64(j, breakValue);

	breakObj.Commit();
	return ba;
}

break_array ClassifyNZEqualCount(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, const SharedObj* abstrValuesRangeData)
{
	DataWriteLock breakObj(breakAttr, dms_rw_mode::write_only_all, abstrValuesRangeData);

	SizeT k = breakAttr->GetAbstrDomainUnit()->GetCount();

	SizeT m = vcpc.size(), n = GetTotalCount(vcpc);

	UInt32 kk = Min<UInt32>(k, m);
	//	assert(kk>=1); // follows from previous asserts + assignemnt
	assert(kk <= m); // follows from assignment
	assert(m <= n); // #(PRECONDITION: vcpc == unique value(themeAttr)) <= #themekAttr
	assert(kk <= n); // follows from previous asserts

	UInt32 c = 0, cc = 0;
	UInt32 i = 0;

	Float64 breakValue = UNDEFINED_VALUE(Float64);
	break_array ba; ba.reserve(kk);

	SizeT j = 0;
	for (; j != kk; ++j)
	{
		assert(m + j >= kk); // follows from previous assert and positivity of j
		UInt32 maxI = m + j - kk; // (m-i) > (kk-j)
		assert(maxI < m);  // j < kk
		while (c < cc && i < maxI && (i==0 || ((vcpc[i-1].first >= 0) == vcpc[i].first > 0)))
		{
			assert(i < m);
			c += vcpc[i].second;
			++i;
		}
		assert(i < m);

		breakValue = vcpc[i].first;

		ba.emplace_back(breakValue);
		breakObj->SetValueAsFloat64(j, breakValue);
		cc = c + (n - c) / (kk - j);
	}
	for (; j != k; ++j)
		breakObj->SetValueAsFloat64(j, breakValue);

	breakObj.Commit();
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
#if defined(MG_REPORT_SSM_COUNT)
		,	md_SsmCounter(0)
#endif
	{
		DBG_START("JenksFisher", "JenksFisher", MG_DEBUG_CLASSBREAKS);
		m_CumulValues.reserve(vcpc.size());
		Float64    cwv=0;
		CountType  cw =0, w;

		for(SizeT i=0; i!=m_M; ++i)
		{
			w   = vcpc[i].second;
			assert(w > 0);

			cw += w; 
			assert(cw >= w); // no overflow?

			assert(!i || vcpc[i-1].first < vcpc[i].first);
			cwv+= w * vcpc[i].first;
			m_CumulValues.push_back(ValueCountPair<Float64>(cwv, cw));

			if (i < m_BufSize)
				m_PrevSSM[i] = cwv * cwv / cw; // prepare SSM for first class, last m_K values can be omitted since they never belong to the first class
			DBG_TRACE(("m_PrevSSM[%d]=%f", i, Float32(m_PrevSSM[i])));
		}
	}

#if defined(MG_REPORT_SSM_COUNT)
	~JenksFisher()
	{
		reportF(ST_MajorTrace, "JenksFisher SsmCounter=%I64u", (UInt64)md_SsmCounter);
	}
#endif
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
#if defined(MG_REPORT_SSM_COUNT)
		++md_SsmCounter;
#endif
		Float64 res = GetWV(b,e);
		return res * res / GetW(b,e);
	}
	auto FindMaxBreakIndex(SizeT i, SizeT bp, SizeT ep) -> std::pair<SizeT, Float64>
	{
		DBG_START("JenksFisher", "FindMinBreakIndex", MG_DEBUG_CLASSBREAKS);
		DBG_TRACE(("i=%d bp=%d ep-%d", i, bp, ep));
		assert(bp < ep);
		assert(bp <= i);
		assert(ep <= i+1);
		assert(i  <  m_BufSize);
		assert(ep <= m_BufSize);

		Float64 minSSM = m_PrevSSM[bp] + GetSSM(bp+m_NrCompletedRows, i+m_NrCompletedRows);
		DBG_TRACE(("%f = prevSSM[%d] + GetSSM(%d) = %f + %f", Float32(minSSM), bp, i-bp, Float32(m_PrevSSM[bp]), Float32(minSSM-m_PrevSSM[bp])));

#if defined(MG_ASSUME_CB_INC)
		if (m_NrCompletedRows>1 && (i+1 < m_BufSize))
		{
			SizeT prev_bp = (m_CBPtr-m_BufSize)[i+1];
			if (prev_bp--)
				MakeMax(bp, prev_bp);
		}
		assert(bp < ep);
		assert(bp <= i);
#endif
		SizeT foundP = bp;
		while (++bp < ep)
		{
			Float64 currSSM = m_PrevSSM[bp] + GetSSM(bp+m_NrCompletedRows, i+m_NrCompletedRows);
			DBG_TRACE(("%f = prevSSM[%d] + GetSSM(%d) = %f + %f", Float32(currSSM), bp, i-bp, Float32(m_PrevSSM[bp]), Float32(currSSM-m_PrevSSM[bp])));
			if (currSSM > minSSM)
			{
				DBG_TRACE(("foundP=%d increases from %f to %f", bp, Float32(minSSM), Float32(currSSM)));

				minSSM = currSSM;
				foundP = bp;
			}
		}
		m_CurrSSM[i] = minSSM;
		return { foundP, minSSM };
	}
	void CalcRange(SizeT bi, SizeT ei, SizeT bp, SizeT ep)
	{
		DBG_START("JenksFisher", "CalcRange", MG_DEBUG_CLASSBREAKS);
		DBG_TRACE(("bi=%d ei=%d bp=%d ep=%d", bi, ei, bp, ep));

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
		assert(m_NrCompletedRows==1 || (mi+1) == m_BufSize|| (mp+1) >= (m_CBPtr-m_BufSize)[mi+1]); // assumption right?
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
				DBG_TRACE(("m_NrCompletedRows=%d", m_NrCompletedRows));

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
				//			DBG_TRACE(("Break[%d]=vcpc[%d]=%f", k, lastClassBreakIndex+k, Float32(vcpc[lastClassBreakIndex+k].first)));
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
#if defined(MG_REPORT_SSM_COUNT)
	UInt64 md_SsmCounter;
#endif
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
	if (!separateZero || kk < 2 || kk == 2 && (vcpc[0].first > 0 || vcpc.back().first < 0))
		return ClassifyCRJenksFisher(vcpc, kk);

	DBG_START("ClassifyNonzeroJenksFisher", "", MG_DEBUG_CLASSBREAKS);

	SizeT firstPositivePos = 0;
	while (firstPositivePos < vcpc.size() && vcpc[firstPositivePos].first <= 0)
		++firstPositivePos;
	assert(firstPositivePos == vcpc.size() || vcpc[firstPositivePos].first > 0);

	auto positiveValues = ValueCountPairContainer(vcpc.begin() + firstPositivePos, vcpc.end());
	CountType zeroCount = 0;
	SizeT firstNonnegativePos = firstPositivePos;
	if (firstPositivePos > 0 && vcpc[firstPositivePos - 1].first == 0)
		zeroCount = vcpc[--firstNonnegativePos].second;
	bool hasZeroClass = (zeroCount > 0);

	auto negativeValues = ValueCountPairContainer(vcpc.begin(), vcpc.begin() + firstNonnegativePos);
	if (negativeValues.size() <= 1)
	{
		auto result = ClassifyCRJenksFisher(positiveValues, kk - negativeValues.size() - (hasZeroClass ? 1 : 0));
		if (hasZeroClass)
			result.insert(result.begin(), 0);
		if (negativeValues.size())
			result.insert(result.begin(), negativeValues[0].first);
		return result;
	}
	if (positiveValues.size() <= 1)
	{
		auto result = ClassifyCRJenksFisher(negativeValues, kk - positiveValues.size() - (hasZeroClass ? 1 : 0));
		if (hasZeroClass)
			result.insert(result.end(), 0);
		if (positiveValues.size())
			result.insert(result.end(), positiveValues[0].first);
		return result;
	}

	break_array result(kk);
	SizeT nrNegativeClasses = 1;
	ClassBreakValueType maxSSM = 0;
	for(;; nrNegativeClasses++)
	{
		SizeT nrPositiveClasses = kk - nrNegativeClasses - (hasZeroClass ? 1 : 0);
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
			if (hasZeroClass)
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
