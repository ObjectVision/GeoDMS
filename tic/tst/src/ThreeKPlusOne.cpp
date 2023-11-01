#include "MlModel.h"

#include "cpc/EndianConversions.h"
#include "dbg/Check.h"
#include "ptr/SharedStr.h"
#include "set/BitVector.h"
#include "geo/MinMax.h"
#include "mth/BigInt.h"

#include <iostream>
#include <map>

using big_int = Big::UInt;

/*
big_int MakeBinAdic(big_int src)
{
	BitReverse(src.begin(), src.end());
	return src;
}
*/

bool IsEven(const big_int& x)
{
	return x.empty() || ((x[0] & 0x0001)==0);
}

struct Pow3Cache : std::vector<big_int>
{
	Pow3Cache()
	{
		push_back(big_int(1));
		push_back(big_int(3));
	}
};

const big_int& Pow3(SizeT p)
{
	static Pow3Cache cache;
	if (cache.size() <= p)
		cache.resize(p+1);

	if (cache[p].empty())
	{
		dms_assert(p > 1); // first two elem were already filled
		SizeT q = p /2;
		cache[p] = Pow3(q)*Pow3(p-q);
	}
	return cache[p];
}

std::ostream& operator << (std::ostream& os, const big_int& rhs)
{
	Big::UInt::const_iterator
		B = rhs.begin()
	,	I = rhs.end();

	char buffer[20];

	while (I != B)
	{
		snprintf(buffer, 20, "%x", *--I);
		os << buffer;
	}
	return os;
}

// =================

bool CompareMSB(const big_int& left, const big_int& right, UInt32 nrBits)
{
	typedef bit_sequence<1, const big_int::value_type> sequence;
	return sequence(&left[0], Min<UInt32>(nrBits, left.size() * 32)) < sequence(&right[0], Min<UInt32>(nrBits, right.size()*32));
}

bool CompareBigInt(const big_int& left, const big_int& right)
{
	typedef bit_sequence<1, const big_int::value_type> sequence;
	return sequence(&left[0], left.size() * 32) < sequence(&right[0], right.size()*32);
}
template<typename KeyRef, bool (*Cmp)(KeyRef, KeyRef)>
struct bindCmpFunc
{
	bool operator()(KeyRef a, KeyRef b) const { return Cmp(a, b); }
};

// =================

struct CollatzRec
{
	CollatzRec()
		:	m_Up(0)
		,	m_Dn(0)
		,	m_Stopped(false)
	{}

	CollatzRec(const big_int& res, UInt32 up, UInt32 dn, bool stopped)
		:	m_Result(res)
		,	m_Up(up)
		,	m_Dn(dn)
		,	m_Stopped(stopped)
	{}

	void Calc();

	big_int m_Result;
	UInt32 m_Up, m_Dn;
	bool m_Stopped;
};


struct CollatzReg : std::map<big_int, CollatzRec, bindCmpFunc<const big_int&, CompareBigInt> >
{
	CollatzReg()
	{
		(*this)[big_int(01)] = CollatzRec(big_int(1), 1, 2,  true);
		(*this)[big_int(03)] = CollatzRec(big_int(3), 0, 0, false);
	}
};

CollatzReg reg;

// =================
CollatzReg::value_type& GetCollatzRec(const big_int& v)
{
	dms_assert(!IsEven(v)); // only odd numbers

	CollatzReg::iterator i = reg.upper_bound(v);
	dms_assert(i != reg.begin()); // only possible for 1
	--i;
	UInt32 nrDn = i->second.m_Dn;
	if (CompareMSB(i->first, v, nrDn))
	{
		big_int newKey = v; newKey.StripLSB(i->second.m_Dn);
		i	=	reg.insert(
					i
				,	std::map<big_int, CollatzRec>::value_type(
						newKey
					,	CollatzRec(
							newKey
						,	0, 0, false
						)
					)
				);
	}
	return *i;
}

void Calc(CollatzReg::value_type& v)
{
	if (v.second.m_Stopped)
		return;
	dms_assert(v.first < v.second.m_Result);
	if (!v.second.m_Up)
	{
		v.second.m_Result += (v.second.m_Result >> 1);
		++v.second.m_Result;
		++v.second.m_Up;
		++v.second.m_Dn;
		v.second.m_Dn += v.second.m_Result.ShiftToOdd();
		v.second.m_Stopped = v.first <= v.second.m_Result;
	}
	while (!v.second.m_Stopped)
	{
		GetCollatzRec(v.second.m_Result);
	}
}

UInt32 CalcNrUpSteps(const big_int& v)
{
	if (v.IsOne())
		return 0;
	CollatzReg::value_type& rec = GetCollatzRec(v);
	if (rec.second.m_Stopped)
		return rec.second.m_Up + CalcNrUpSteps( rec.second.m_Result + (v >> rec.second.m_Dn) * Pow3(rec.second.m_Up) );
	return -1;
}

UInt32 CalcNrDnSteps(const big_int& v)
{
	big_int w = v;
	UInt32  nrDn = 0;
	while (w >= v)
	{
		w += (w >> 1);
		++w;
		++nrDn;
		nrDn += w.ShiftToOdd();
		std::cout << w << std::endl;
	}
	std::cout << nrDn << std::endl;
	if (!w.IsOne())
		nrDn += CalcNrDnSteps(w);
	std::cout << v << " in " << nrDn << " steps" << std::endl;
	return nrDn;
}
// ============== 

void ThreeKPlusOne()
{
	std::cout << CalcNrDnSteps(big_int( 7)) << std::endl;
	std::cout << CalcNrDnSteps(big_int(-1)) << std::endl;
}


