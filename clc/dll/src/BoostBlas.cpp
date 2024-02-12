// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "RtcTypeLists.h"
#include "geo/Range.h"
#include "mci/ValueWrap.h"
#include "utl/TypeListOper.h"

#include "BoostBlas.h"

#include "Unit.h"
#include "UnitClass.h"

#include "UnitCreators.h"

#include <boost/numeric/ublas/blas.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/operation.hpp>

#include "VersionComponent.h"
static VersionComponent s_BoostBlas("boost::blas"); // BOOST_STRINGIZE(BOOST_POLYGON_VERSION));

CommonOperGroup cogMM("matr_mul");
CommonOperGroup cogMV("matr_var");
CommonOperGroup cogMI("matr_inv");

// *****************************************************************************
//	MatrMulOperator
// *****************************************************************************

class AbstrMatrMulOperator : public TernaryOperator
{
protected:
	AbstrMatrMulOperator(const DataItemClass* matrAttrClass)
		:	TernaryOperator(&cogMM, matrAttrClass
			,	matrAttrClass
			,	matrAttrClass
			,	AbstrUnit::GetStaticClass()
			)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		const AbstrUnit* resDomainUnit = AsUnit(args[2]);
		dms_assert(arg1A); 
		dms_assert(arg2A); 
		dms_assert(resDomainUnit);

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = domain1Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();

		const AbstrUnit* domain2Unit = arg2A->GetAbstrDomainUnit(); bool e2IsVoid = domain2Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values2Unit = arg2A->GetAbstrValuesUnit();

//		values1Unit->UnifyValues(values2Unit, UM_Throw);

		if (!resultHolder)
		{
			ConstUnitRef resValuesUnit = mulx_unit_creator(GetGroup(), args);
			resultHolder = CreateCacheDataItem(resDomainUnit, resValuesUnit);
		}

		if (mustCalc)
		{
			auto domain1Size = domain1Unit->GetRangeAsIRect();
			auto domain2Size = domain2Unit->GetRangeAsIRect();
			auto resDomainSize = resDomainUnit->GetRangeAsIRect();

			MG_CHECK(Height(domain1Size) == Height(resDomainSize));
			MG_CHECK(Width (domain2Size) == Width (resDomainSize));
			MG_CHECK(Width (domain1Size) == Height(domain2Size  ));

			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);
			Calculate(resLock.get(), arg1A, arg2A, Height(domain1Size), Width(domain2Size), Width(domain1Size));

			resLock.Commit();
		}

		return true;
	}
	virtual void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A,  SizeT resRowCount, SizeT resColCount, SizeT collapsedIndexCount) const=0;
};

template <typename T>
class MatrMulOperator : public AbstrMatrMulOperator
{
	typedef T ValueType;
	typedef DataArray<T> ArgType;

public:
	MatrMulOperator()
		:	AbstrMatrMulOperator(ArgType::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A,  SizeT resRowCount, SizeT resColCount, SizeT collapsedIndexCount) const override
	{
		const ArgType* matr1 = const_array_cast<T>(arg1A);
		const ArgType* matr2 = const_array_cast<T>(arg2A);
		      ArgType* matrR = mutable_array_cast<T>(res);

		dms_assert(matr1); 
		dms_assert(matr2);
		dms_assert(matrR);

		auto data1Array = matr1->GetDataRead();
		auto data2Array = matr2->GetDataRead();
		auto dataRArray = matrR->GetDataWrite();

		dms_assert(data1Array.size() == resRowCount * collapsedIndexCount);
		dms_assert(data2Array.size() == resColCount * collapsedIndexCount);
		dms_assert(dataRArray.size() == resRowCount * resColCount);

		using namespace boost::numeric::ublas;

		auto data1ArrayPtr = data1Array.begin();
		auto data2ArrayPtr = data2Array.begin();

		matrix<T> data1Matr(resRowCount, collapsedIndexCount); for (SizeT i=0; i!=resRowCount; ++i) for (SizeT j=0; j!=collapsedIndexCount; ++j) data1Matr(i, j) = *data1ArrayPtr++;
		matrix<T> data2Matr(collapsedIndexCount, resColCount); for (SizeT i=0; i!=collapsedIndexCount; ++i) for (SizeT j=0; j!=resColCount; ++j) data2Matr(i, j) = *data2ArrayPtr++;
		matrix<T> dataRMatr = prod(data1Matr, data2Matr);

		auto dataRArrayPtr = dataRArray.begin();
		for (SizeT i=0; i!=resRowCount; ++i) for (SizeT j=0; j!=resColCount; ++j) *dataRArrayPtr++ = dataRMatr(i, j);
	}
};

// *****************************************************************************
//	MatrVarOperator
// *****************************************************************************

class AbstrMatrVarOperator : public BinaryOperator
{
protected:
	AbstrMatrVarOperator(const DataItemClass* matrAttrClass)
		:	BinaryOperator(&cogMV, matrAttrClass
			,	matrAttrClass
			,	AbstrUnit::GetStaticClass()
			)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrUnit* resDomainUnit = AsUnit(args[1]);
		dms_assert(arg1A); 
		dms_assert(resDomainUnit);

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = domain1Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();

//		values1Unit->UnifyValues(values2Unit, UM_Throw);


		if (!resultHolder)
		{
			ConstUnitRef resValuesUnit = square_unit_creator(GetGroup(), args);
			resultHolder = CreateCacheDataItem(resDomainUnit, resValuesUnit);
		}

		if (mustCalc)
		{
//			FencedInterestRetainContext context;
//			InterestRetainContextBase::Add(domain1Unit);
//			InterestRetainContextBase::Add(resDomainUnit);

			auto domain1Size = domain1Unit->GetRangeAsIRect();
			auto resDomainSize = resDomainUnit->GetRangeAsIRect();
			MG_CHECK(Width (domain1Size) == Width (resDomainSize));
			MG_CHECK(Width (domain1Size) == Height(resDomainSize));

			DataReadLock arg1Lock(arg1A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);
			Calculate(resLock.get(), arg1A, Width(domain1Size), Height(domain1Size));

			resLock.Commit();
		}

		return true;
	}
	virtual void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, SizeT argWidth, SizeT argHeight) const=0;
};

template <typename T>
class MatrVarOperator : public AbstrMatrVarOperator
{
	typedef T ValueType;
	typedef DataArray<T> ArgType;

public:
	MatrVarOperator()
		:	AbstrMatrVarOperator(ArgType::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, SizeT argWidth, SizeT argHeight) const override
	{
		const ArgType* matr1 = const_array_cast<T>(arg1A);
		      ArgType* matrR = mutable_array_cast<T>(res);

		dms_assert(matr1);
		dms_assert(matrR);

		auto data1Array = matr1->GetDataRead();
		auto  dataRArray = matrR->GetDataWrite();

		dms_assert(data1Array.size() == argWidth * argHeight);
		dms_assert(dataRArray.size() == argWidth * argWidth);

		using namespace boost::numeric::ublas;

		auto data1ArrayPtr = data1Array.begin();

		matrix<T> data1Matr(argHeight, argWidth); for (SizeT i=0; i!=argHeight; ++i) for (SizeT j=0; j!=argWidth; ++j) data1Matr(i, j) = *data1ArrayPtr++;
		matrix<T> dataRMatr = prod(trans(data1Matr), data1Matr);

		auto dataRArrayPtr = dataRArray.begin();
		for (SizeT i=0; i!=argWidth; ++i) for (SizeT j=0; j!=argWidth; ++j) *dataRArrayPtr++ = dataRMatr(i, j);
	}
};

// *****************************************************************************
//	MatrInvOperator
// *****************************************************************************

class AbstrMatrInvOperator : public UnaryOperator
{
protected:
	AbstrMatrInvOperator(const DataItemClass* matrAttrClass)
		:	UnaryOperator(&cogMI, matrAttrClass
			,	matrAttrClass
			)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		dms_assert(arg1A); 

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = domain1Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();

		if (!resultHolder)
		{

			ConstUnitRef resValuesUnit = inv_unit_creator(GetGroup(), args);
			resultHolder = CreateCacheDataItem(domain1Unit, resValuesUnit);
		}

		if (mustCalc)
		{
			auto domain1Size = domain1Unit->GetRangeAsIRect();
			MG_CHECK(Width (domain1Size) == Height(domain1Size));

			DataReadLock arg1Lock(arg1A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);
			Calculate(resLock.get(), arg1A, Width(domain1Size));

			resLock.Commit();
		}

		return true;
	}
	virtual void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, SizeT n) const=0;
};

#include <boost/numeric/ublas/matrix.hpp> 
#include <boost/numeric/ublas/lu.hpp> 

namespace ublas = boost::numeric::ublas; 

// http://www.crystalclearsoftware.com/cgi-bin/boost_wiki/wiki.pl?LU_Matrix_Inversion
// Reference: Numerical Recipies in C, 2nd ed., by Press, Teukolsky, Vetterling & Flannery.

template<class T> 
bool InvertMatrix(const ublas::matrix<T>& input, ublas::matrix<T>& inverse) { 
    using namespace boost::numeric::ublas; 
    typedef permutation_matrix<std::size_t> pmatrix; 
    // create a working copy of the input 
    matrix<T> A(input); 
    // create a permutation matrix for the LU-factorization 
    pmatrix pm(A.size1()); 
    // perform LU-factorization 
    int res = lu_factorize(A,pm); 
    if( res != 0 )
        return false; 
    // create identity matrix of "inverse" 
    inverse.assign(ublas::identity_matrix<T>(A.size1())); 
    // backsubstitute to get the inverse 
    lu_substitute(A, pm, inverse); 
    return true; 
}

template <typename T>
class MatrInvOperator : public AbstrMatrInvOperator
{
	typedef T ValueType;
	typedef DataArray<T> ArgType;

public:
	MatrInvOperator()
		:	AbstrMatrInvOperator(ArgType::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, SizeT n) const override
	{
		const ArgType* matr1 = const_array_cast<T>(arg1A);
		      ArgType* matrR = mutable_array_cast<T>(res);

		dms_assert(matr1);
		dms_assert(matrR);

		auto data1Array = matr1->GetDataRead();
		auto dataRArray = matrR->GetDataWrite();

		dms_assert(data1Array.size() == n * n);
		dms_assert(dataRArray.size() == n * n);

		using namespace boost::numeric::ublas;

		auto data1ArrayPtr = data1Array.begin();

		matrix<T> data1Matr(n, n); for (SizeT i=0; i!=n; ++i) for (SizeT j=0; j!=n; ++j) data1Matr(i, j) = *data1ArrayPtr++;
		matrix<T> dataRMatr(n, n);

		if (!InvertMatrix(data1Matr, dataRMatr))
			fast_undefine(dataRArray.begin(), dataRArray.end());
		else
		{
			auto dataRArrayPtr = dataRArray.begin();
			for (SizeT i=0; i!=n; ++i) for (SizeT j=0; j!=n; ++j) *dataRArrayPtr++ = dataRMatr(i, j);
		}
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	tl_oper::inst_tuple_templ<typelists::floats, MatrMulOperator> matrMulOperators;
	tl_oper::inst_tuple_templ<typelists::floats, MatrVarOperator> matrCovOperators;
	tl_oper::inst_tuple_templ<typelists::floats, MatrInvOperator> matrInvOperators;
}



