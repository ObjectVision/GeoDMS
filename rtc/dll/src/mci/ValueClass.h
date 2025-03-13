// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_MCI_VALUECLASS_H)
#define __RTC_MCI_VALUECLASS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geo/BitValue.h"
#include "mci/AbstrValue.h"
#include "mci/Class.h"

class UnitClass;
class DataItemClass;

//----------------------------------------------------------------------

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

template <typename Host>
struct ValueClassVisitor {
	virtual void Visit(const Host* /*inviter*/) const { throwIllegalAbstract(MG_POS, "ValueClassVisitor"); }
};

struct ValueClassProcessor1
	: boost::mpl::fold<typelists::vc_types
	, tl_oper::impl::empty_base<>
	, tl_oper::impl::scattered_hierarchy<ValueClassVisitor<boost::mpl::_2>, boost::mpl::_1>
	>	::type
{};

struct ValueClassProcessor2
	: boost::mpl::fold<typelists::range_objects
	, tl_oper::impl::empty_base<>
	, tl_oper::impl::scattered_hierarchy<ValueClassVisitor<boost::mpl::_2>, boost::mpl::_1>
	>	::type
{};

struct ValueClassProcessor : ValueClassProcessor1, ValueClassProcessor2
{};

template <typename Host, typename Base>
struct ValueClassVisitorImpl : Base
{
	using Base::Base; // inherit ctors

	void Visit(const Host* inviter) const override
	{
		this->VisitImpl(inviter);
	}
};


template<typename ValueClassAutoLambda>
struct ValueClassAutoLambdaCallerBase : ValueClassProcessor
{
	ValueClassAutoLambdaCallerBase(ValueClassAutoLambda&& al) : m_AutoLambda(std::forward<ValueClassAutoLambda>(al)) {};
	ValueClassAutoLambda m_AutoLambda;

	template <typename Host>
	void VisitImpl(const Host* inviter) const
	{
		m_AutoLambda(inviter);
	}
};

template<typename TypeList, typename AutoLambda>
struct ValueClassLambdaCaller : boost::mpl::fold<TypeList, ValueClassAutoLambdaCallerBase<AutoLambda>, ValueClassVisitorImpl<_2, _1> >::type
{
	using base_type = boost::mpl::fold<TypeList, ValueClassAutoLambdaCallerBase<AutoLambda>, ValueClassVisitorImpl<_2, _1> >::type;
	using base_type::base_type; // inherit ctors
};

//----------------------------------------------------------------------
// class  : ValueClass
//----------------------------------------------------------------------

class ValueClass  : public Class
{
	using base_type = Class;

public:
	using InviterFunc = void(*)(const ValueClassProcessor*);

	ValueClass(
		Constructor           cFunc, 
		InviterFunc           iFunc,
		TokenID               scriptID, 
		ValueClassID          vt,
		Int32                 size, 
		Int32                 bitSize, 
		DimType               nrDims, 
		bool                  isNumeric, 
		bool                  isIntegral, 
		bool                  isSigned, 
		ValueComposition      vc, 
		const Byte*           undefinedValuePtr, 
		Float64               undefAsFloat64,
		Float64               maxAsFloat64,
		Float64               minAsFloat64,
		const ValueClass*     fieldClass,
		const ValueClass*     scalarClass,
		const ValueClass*     cardinalityClass
	);
	~ValueClass();

	RTC_CALL AbstrValue*  CreateValue() const;
	void InviteProcessor(ValueClassProcessor* vcp) const { dms_assert(m_iFunc); m_iFunc(vcp); }

	UInt32  GetSize   () const { return m_Size;      }
	UInt32  GetBitSize() const { return m_BitSize;   }
	DimType GetNrDims () const { return m_NrDims;    }
	bool    IsNumeric () const { return m_IsNumeric; }
	bool    IsSubByteElem() const { return GetBitSize() > 0 && GetBitSize() < 8; }
	bool    IsNumericOrBool() const { return m_IsNumeric || IsSubByteElem(); }
	bool    IsIntegral()  const { return m_IsIntegral;}
	bool    IsCountable() const { return m_IsIntegral || (m_ScalarClass && m_ScalarClass->m_IsIntegral); }
	bool    IsSigned  () const { return m_IsSigned;  }
	bool    IsShape   () const { return GetNrDims() == 2 && IsSequence(); }
	bool    HasUndefined() const { return !IsSubByteElem(); }
	bool    HasFixedValues() const { return IsSubByteElem(); }
	// hide ValueComposition from header bloat
	RTC_CALL bool    IsRange   () const;
	RTC_CALL bool    IsSequence() const;

	ValueClassID     GetValueClassID           () const { return m_ValueClassID; }
	const Byte*      GetUndefinedValuePtr      () const { return m_UndefinedValuePtr; }
	Float64          GetUndefinedValueAsFloat64() const { dms_assert(IsNumericOrBool()); return m_UndefinedValueAsFloat64; }
	Float64          GetMaxValueAsFloat64      () const { dms_assert(IsNumericOrBool()); return m_MaxValueAsFloat64; }
	Float64          GetMinValueAsFloat64      () const { dms_assert(IsNumericOrBool()); return m_MinValueAsFloat64; }
	ValueComposition GetValueComposition       () const { return m_ValueComposition; }

	         const ValueClass* GetScalarClass  () const { return m_ScalarClass; }
	         const ValueClass* GetRangeClass   () const { return m_RangeClass; }
	         const ValueClass* GetSequenceClass() const { return m_SequenceClass; }
	RTC_CALL const ValueClass* GetUnsignedClass() const; 
	RTC_CALL const ValueClass* GetSignedClass  () const;
	RTC_CALL const ValueClass* GetCrdClass     () const; // ord version for Countable

	RTC_CALL static const ValueClass* FindByValueClassID(ValueClassID vt);
	RTC_CALL static       ValueClass* FindByScriptName(TokenID valueTypeID);

private:
	InviterFunc      m_iFunc;
	ValueClassID     m_ValueClassID;
	ValueComposition m_ValueComposition;
	UInt32           m_Size,
	                 m_BitSize;
	DimType          m_NrDims;
	bool             m_IsNumeric, 
	                 m_IsIntegral, 
	                 m_IsSigned;
	const Byte*      m_UndefinedValuePtr;
	Float64          m_UndefinedValueAsFloat64;
	Float64          m_MaxValueAsFloat64;
	Float64          m_MinValueAsFloat64;

			const ValueClass* m_FieldClass;
	        const ValueClass* m_ScalarClass;
			const ValueClass* m_CardinalityClass;

	mutable const ValueClass* m_RangeClass;
	mutable const ValueClass* m_SequenceClass;

	mutable UnitClass*        m_UnitClass;     friend UnitClass;
	mutable DataItemClass*    m_DataItemClass; friend DataItemClass;

	DECL_RTTI(RTC_CALL, MetaClass)
};

inline Bool OverlappingTypes(const ValueClass *vc1, const ValueClass *vc2)
{
	return vc1 == vc2 || (vc1->IsIntegral() && vc2->IsIntegral() && vc1->GetSize() == vc2->GetSize());
}
//----------------------------------------------------------------------
// utility operations
//----------------------------------------------------------------------

template<typename TypeList, typename ValueClassAutoLambda>
void visit(const ValueClass* inviter, ValueClassAutoLambda&& al)
{
	ValueClassLambdaCaller<TypeList, ValueClassAutoLambda> alc(std::forward<ValueClassAutoLambda>(al));
	inviter->InviteProcessor(&alc);
}

inline bool operator ==(const ValueClass& a, const ValueClass& b) { return &a == &b; }


#endif // __RTC_MCI_VALUECLASS_H

