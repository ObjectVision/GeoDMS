// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#define MG_DEBUG
#include "dbg/Diagnostics.h"
#include "vt/BaseBounds.h"
//#include "geo/SequenceArray.hpp"

#include "dbg/debug.h"




// *****************************************************************************
// DebugContext
// *****************************************************************************

// use your own timing struct here
#if !defined(DBG_START)
#	define DBG_START(ClassName, FuncName, IsActivated)
#endif

// *****************************************************************************
// Tests for both Vector of Vectors (vov's) and SequenceArrays
// *****************************************************************************
/*
template <typename SeqArrType>
void TestSequentialFillWithPushBack(SeqArrType*, unsigned n, unsigned k)
{
	DBG_START(typeid(SeqArrType).name(), "TestSequentialFillWithPushBack", true); // present timer

	typedef SeqArrType::value_type   SequenceType;
	typedef SequenceType::value_type ElemType;
	ElemType x;
	SeqArrType seqArr;
	seqArr.resize(n);
	for (unsigned i = 0; i!=n; ++i)
		for (unsigned j = 0; j!=k; ++j)
			seqArr[i].push_back(x);
}

template <typename SeqArrType>
void TestSequentialFillWithResize(SeqArrType*, unsigned n, unsigned k)
{
	DBG_START(typeid(SeqArrType).name(), "TestSequentialFillWithResize", true); // present timer

	typedef SeqArrType::value_type   SequenceType;
	typedef SequenceType::value_type ElemType;
	ElemType x;
	SeqArrType seqArr;
	seqArr.resize(n);
	for (unsigned i = 0; i!=n; ++i)
	{
		seqArr[i].resize(k);
		for (unsigned j = 0; j!=k; ++j)
			seqArr[i][j] = x;
	}
}
*/
// *****************************************************************************
// Test only for SequenceArray (the fastest way to fill a sequence_array)
// requires you to pre-specify the expected total data size.
// *****************************************************************************
/*
template <typename SeqArrType>
void TestSequentialFillWithDataReserve(SeqArrType*, unsigned n, unsigned k)
{
	DBG_START(typeid(SeqArrType).name(), "TestSequentialFillWithResize", true); // present timer

	typedef SeqArrType::value_type   SequenceType;
	typedef SequenceType::value_type ElemType;
	ElemType x;
	SeqArrType seqArr;
	seqArr.resize(n);
	seqArr.data_reserve(n*k); // here it the trick to get the most fast result
	for (unsigned i = 0; i!=n; ++i)
	{
		seqArr[i].resize(k);
		for (unsigned j = 0; j!=k; ++j)
			seqArr[i][j] = x;
	}
}
*/
// *****************************************************************************
// BigInt
// *****************************************************************************

/*
#include "BigInt.h"

void BigIntTest()
{
	BigUInt a(0xFFFFFFFF);
	BigUInt b = a*a;
};
*/

// *****************************************************************************
// Main
// *****************************************************************************
/*

struct DblPointVec   : std::vector<DblPoint>   {};
struct DblPolygonVec : std::vector<DblPointVec>{};
*/
/* cleanup test after exception in ctor
#include "ptr/RefObject.h"
#include "ptr/RefPtr.h"
struct A
{
	A() { DBG_START("A", "CTor", true); }
  ~A() { DBG_START("A", "DTor", true); }
  void* operator new(UInt32 sz)
  {
		DBG_START("A", "new", true);
		return ::operator new(sz);
  }
  void operator delete(void * ptr)
  {
		DBG_START("A", "delete", true);
	  ::operator delete(ptr);
  }
};

struct C: RefObject
{
	C()
	{ 
		DBG_START("C", "CTor", true); 
	}
  ~C()
	{ 
		DBG_START("C", "DTor", true); 
	}
	void Destruct ()                { delete this; }
};

struct B : A
{
	RefPtr<C> m_C;
	B()
	{ 
		DBG_START("B", "CTor", true); 
		m_C = new C;
		throwDmsErr("Hopla"); 
	}
  ~B() 
  { 
	  DBG_START("B", "DTor", true); 
  }
};
*/

//#include "ser/AsString.h"

#include "ser/FormattedStream.h"
#include "ser/StringStream.h"
#include "ser/PolygonStream.h"
#include "ser/MoreStreamBuff.h"
#include "utl/Quotes.h"

template <typename T>
struct sar
{
	struct inner {};
};

template <typename T>
void func1(sar<T>)
{
}

template <typename T>
void func2(typename sar<T>::inner vec)
{
}

template <template <typename T> class SAR>
void func3(typename SAR<T>::inner* vec)
{
}

//struct  DblPoint : std::pair<double, double> {}; 
//typedef sequence_array<DblPoint>  DblPolygonArray;

int main()
{
	DMS_CALL_BEGIN

	const sar<float>  xx;
	func1(xx);

	sar<float>::inner yy;
	func3(&yy);

/*
	DBG_START("SequenceArrayTester", "main", true); // present timer

	TestSequentialFillWithPushBack((DblPolygonVec*  )0, 10000, 1000); // 13 secs
	TestSequentialFillWithPushBack((DblPolygonArray*)0, 10000, 1000); //  9 secs

	TestSequentialFillWithResize  ((DblPolygonVec*  )0, 10000, 1000); // 12 secs
	TestSequentialFillWithResize  ((DblPolygonArray*)0, 10000, 1000); //  6 secs

	TestSequentialFillWithDataReserve((DblPolygonArray*)0, 10000, 1000); // 3 secs
*/

//		ProcessHeapTest();
//	BigIntTest();

	DMS_CALL_END
	return 0;
} // total: 43 secs

