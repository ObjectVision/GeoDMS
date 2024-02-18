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
#include "RtcPCH.h"

#include "dbg/DmsCatch.h"
#include "dbg/DebugCast.h"

#include "geo/BitValue.h"
#include "ptr/OwningPtr.h"

#include "mci/AbstrValue.h"
#include "mci/ValueWrap.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "mci/ValueComposition.h"

#include "ser/AsString.h"
#include "ser/FormattedStream.h"		
#include "ser/RangeStream.h"
#include "ser/StreamTraits.h"
#include "ser/StringStream.h"
#include "ser/VectorStream.h"
#include "geo/StringBounds.h"
#include "utl/Instantiate.h"

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

//----------------------------------------------------------------------

IMPL_CLASS(AbstrValue, 0)

//----------------------------------------------------------------------
// ValueComposition helper functions
//----------------------------------------------------------------------

void Unify(ValueComposition& vc, ValueComposition rhs)
{
	MG_CHECK2((vc == ValueComposition::Single) == (rhs == ValueComposition::Single), "Incompatible value compositions");
	if (rhs == ValueComposition::Single)
	{
		dms_assert(vc == ValueComposition::Single);
		return;
	}
	dms_assert(IsAcceptableValuesComposition(vc));
	dms_assert(IsAcceptableValuesComposition(rhs));
	if (rhs == ValueComposition::Polygon)
		vc = ValueComposition::Polygon;
}

ValueComposition DetermineValueComposition(CharPtr featureType)
{
	if (!featureType || !*featureType || !stricmp(featureType, "single"))
		return ValueComposition::Single;
	else if (!stricmp(featureType, "arc"))     return ValueComposition::Sequence;
	else if (!stricmp(featureType, "polygon")) return ValueComposition::Polygon;
	else if (!stricmp(featureType, "poly"))    return ValueComposition::Polygon;
	return ValueComposition::Unknown;
}

namespace {
	TokenID st_VC[int(ValueComposition::Count)] = { GetTokenID_st("single"), GetTokenID_st("poly"), GetTokenID_st("arc") };
}

TokenID GetValueCompositionID(ValueComposition vc)
{
	static_assert(int(ValueComposition::Single) == 0);
	static_assert(int(ValueComposition::Polygon) == 1);
	static_assert(int(ValueComposition::Sequence) == 2);
	return (UInt32(vc) < UInt32(ValueComposition::Count))
		? st_VC[UInt32(vc)]
		: TokenID(Undefined());
}

//----------------------------------------------------------------------
// class  : ValueWrap<T>
//----------------------------------------------------------------------


template <class T>
ValueWrap<T>::ValueWrap(): m_Value(value_type()) {}


template <class T> 
bool ValueWrap<T>::AsCharArray(char* buffer, SizeT bufLen, FormattingFlags ff) const
{
	return ::AsCharArray(m_Value, buffer, bufLen, ff);
}

template <class T> 
SizeT ValueWrap<T>::AsCharArraySize(SizeT maxLen, FormattingFlags ff) const
{
	return ::AsCharArraySize(m_Value, maxLen, ff);
}

template <class T> 
SharedStr ValueWrap<T>::AsString() const
{
	return ::AsString(m_Value);
}

template <class T> 
Float64 ValueWrap<T>::AsFloat64() const
{
	if (!GetStaticClass()->IsNumeric())
		throwIllegalAbstract(MG_POS, "ValueWrap<T>::AsFloat64");
	return ::AsFloat64(m_Value);
}

template <class T> 
void ValueWrap<T>::AssignFromCharPtr(CharPtr data) // null terminated CharArray
{
	AssignValueFromCharPtr(m_Value, data);
}

template <class T> 
void ValueWrap<T>::AssignFromCharPtrs(CharPtr first, CharPtr last)
{
	AssignValueFromCharPtrs(m_Value, first, last);
}

template <typename   T> inline bool IsNullValue(const T& t  )        { return !IsDefined(t); } 
                        inline bool IsNullValue(const Void& )        { return true; }
template <bit_size_t N> inline bool IsNullValue(const bit_value<N>&) { return false; }

template <class T> 
bool ValueWrap<T>::IsNull() const
{
	return IsNullValue(m_Value);
}

//----------------------------------------------------------------------
// class  : ValueClass
//----------------------------------------------------------------------

#include "mci/register.h"

StaticRegister<ValueClass, TokenID, CompareLtItemIdPtrs<ValueClass> > g_ValueClassRegister;
TokenID g_HighestValueClassID = TokenID::GetEmptyID();

ValueClass::ValueClass(
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
)	:	Class(cFunc, Object::GetStaticClass(), scriptID)
	,	m_iFunc            (iFunc)
	,	m_ValueClassID     (vt)
	,	m_Size             (size)
	,	m_BitSize          (bitSize)
	,	m_NrDims           (nrDims)
	,	m_IsNumeric        (isNumeric)
	,	m_IsIntegral       (isIntegral)
	,	m_IsSigned         (isSigned)
	,	m_ValueComposition (vc)
	,	m_UndefinedValuePtr(undefinedValuePtr)
	,	m_UndefinedValueAsFloat64(undefAsFloat64)
	,	m_MaxValueAsFloat64(maxAsFloat64)
	,	m_MinValueAsFloat64(minAsFloat64)
	,	m_FieldClass       (fieldClass)
	,	m_ScalarClass      (scalarClass)
	,	m_CardinalityClass(cardinalityClass)
	,	m_RangeClass       (nullptr)
	,	m_SequenceClass    (nullptr)
	,	m_UnitClass        (nullptr)
	,	m_DataItemClass    (nullptr)
{ 
	g_ValueClassRegister.Register(this);
	MakeMax(g_HighestValueClassID, scriptID);

	if (fieldClass && vc == ValueComposition::Range)
	{
		dms_assert(fieldClass != this);
		dms_assert(size == 2*fieldClass->GetSize() );
		fieldClass->m_RangeClass = this;
	}
	if (fieldClass && vc == ValueComposition::Sequence)
	{
		dms_assert(fieldClass != this);
		fieldClass->m_SequenceClass = this;
	}
}

ValueClass::~ValueClass()
{
	g_ValueClassRegister.Unregister(this);
	dms_assert(!m_UnitClass);
	dms_assert(!m_DataItemClass);
}

RTC_CALL ValueClass* ValueClass::FindByScriptName(TokenID valueTypeID)
{
	if (g_HighestValueClassID < valueTypeID)
		return nullptr;
	return g_ValueClassRegister.Find(valueTypeID);
}

AbstrValue* ValueClass::CreateValue() const
{
	return debug_cast<AbstrValue*>(CreateObj());
}

bool ValueClass::IsRange   () const { return m_ValueComposition == ValueComposition::Range; }
bool ValueClass::IsSequence() const { return m_ValueComposition == ValueComposition::Sequence; }

IMPL_RTTI_METACLASS(ValueClass, "VALUE", nullptr);

//----------------------------------------------------------------------
// instantiation and registration
//----------------------------------------------------------------------

template <typename T, typename F>
struct get_field_type
{
	using type = ValueWrap<F>;
};

template <typename T>
struct get_field_type<T, T>
{
	struct type
	{
		static ValueClass* GetStaticClass() { return nullptr; }
	};
};

template <typename T, typename F> using get_field_type_t = typename get_field_type<T, F>::type;

template <bool IsNumeric, typename T>
struct GetExtremesAsFloat64
{
	static Float64 MinValue() { return 0; }
	static Float64 MaxValue() { return 0; }
};

template <typename T>
struct GetExtremesAsFloat64<true, T>
{
	static Float64 MinValue() { return ::MinValue<T>(); }
	static Float64 MaxValue() { return ::MaxValue<T>(); }
};

template <typename T> CharPtr GetScriptName();
template <typename T> ValueClassID GetTypeID();

typedef SharedStr String;
#define INSTANTIATE(T) \
	template <> CharPtr      GetScriptName<T>() { return #T; } \
	template <> ValueClassID GetTypeID    <T>() { return ValueClassID::VT_##T; }

INSTANTIATE_ALL_VC

#undef INSTANTIATE

#define INSTANTIATE(T) \
	template <> CharPtr      GetScriptName<Range<T> >() { return "Range" #T;  } \
	template <> ValueClassID GetTypeID    <Range<T> >() { return ValueClassID::VT_Range##T; }

INSTANTIATE_NUM_ORG

#undef INSTANTIATE

template <typename T> 
const ValueClass* ValueWrap<T>::GetStaticClass()
{
	static OwningPtr<ValueClass> s_StaticCls;
	if (!s_StaticCls) {
		static T s_UndefinedValue;
		ValueClass::InviterFunc inviterFunc = [](const ValueClassProcessor* vcp) -> void
		{ 
			const ValueClassVisitor<T>* castedVisitor = vcp;
			castedVisitor->Visit((const T*)nullptr);
		};
		MakeUndefinedOrZero(s_UndefinedValue);
		s_StaticCls.assign(
			new ValueClass(
				CreateFunc<ValueWrap<T> >, inviterFunc, GetTokenID_st(GetScriptName<T>() ), GetTypeID<T>(),
				Int32(has_fixed_elem_size_v<T> ? sizeof(T)      : -1),
				Int32(has_fixed_elem_size_v<T> ? nrbits_of_v<T> :  -1),
				dimension_of<field_of_t<T>>::value,
				is_numeric_v<T>,
				is_integral<T>::value,
				is_signed<T>::value,
				composition_of_v<T>,
				(const Byte*)(&s_UndefinedValue),
				is_numeric_v<T> ? ::AsFloat64(s_UndefinedValue):0.0,
				GetExtremesAsFloat64<is_numeric_v<T>, T>::MaxValue(),
				GetExtremesAsFloat64<is_numeric_v<T>, T>::MinValue(),
				get_field_type_t<T, field_of_t<T>>::GetStaticClass(),
				get_field_type_t<T, scalar_of_t<T>>::GetStaticClass(),
				get_field_type_t<T, cardinality_type_t<T>>::GetStaticClass()
			)
		);
	}
	return s_StaticCls;
}

template <typename T> 
const Class* ValueWrap<T>::GetDynamicClass() const { return GetStaticClass(); }

#include "ser/PairStream.h"

namespace {

	TypeListClassReg< tl::transform<typelists::vc_types, ValueWrap<_> > > s_x;
	TypeListClassReg< tl::transform<typelists::range_objects, ValueWrap<_> > > s_rx;
//	TypeListClassReg< tl::transform<typelists::all_vc_types, ValueWrap<Range<_>> > > s_rx;
//	TypeListClassReg< tl::transform<typelists::ranged_unit_objects, ValueWrap<Range<_>> > > s_rx;
}

//----------------------------------------------------------------------
// other mf of ValueClass
//----------------------------------------------------------------------

RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& os, const ValueClass& vc)
{
	return os << "ValueClass  " << vc.GetName().c_str();
}

const ValueClass* ValueClass::GetUnsignedClass() const
{
	assert(IsIntegral() && IsSigned()); 
	return FindByValueClassID(ValueClassID(UInt8(m_ValueClassID)-1));
}

const ValueClass* ValueClass::GetSignedClass()   const
{
	dms_assert(IsIntegral() && !IsSigned()); 
	return FindByValueClassID(ValueClassID(UInt8(m_ValueClassID)+1));
}

const ValueClass* ValueClass::GetCrdClass() const // ord version for Countable
{
	return m_CardinalityClass ? m_CardinalityClass : this;
}

//----------------------------------------------------------------------
// C style Interface functions for class id retrieval
//----------------------------------------------------------------------

#include "RtcInterface.h"

// Helper function: gives the name of a runtime class

RTC_CALL CharPtr  DMS_CONV DMS_ValueType_GetName(const ValueClass* self)
{
	DMS_CALL_BEGIN

		return self
			? self->GetName().c_str()
			: "null";

	DMS_CALL_END
	return nullptr;
}


const ValueClass* ValueClass::FindByValueClassID(ValueClassID vt)
{
	switch (vt) {

#	define INSTANTIATE(T) case ValueClassID::VT_##T:  return ValueWrap<T> ::GetStaticClass();

	INSTANTIATE_ALL_VC

#	undef INSTANTIATE
	}
	return nullptr;
}

RTC_CALL const ValueClass* DMS_CONV DMS_GetValueType(ValueClassID vt)
{
	DMS_CALL_BEGIN

		return ValueClass::FindByValueClassID(vt);

	DMS_CALL_END
	return nullptr;
}

RTC_CALL ValueClassID DMS_CONV DMS_ValueType_GetID(const ValueClass* vc)
{
	DMS_CALL_BEGIN
		ObjectContextHandle checkPtr(vc, ValueClass::GetStaticClass(), "DMS_ValueType_GetID");
		return vc->GetValueClassID();
	DMS_CALL_END
	return ValueClassID::VT_Unknown;
}

RTC_CALL Float64  DMS_CONV DMS_ValueType_GetUndefinedValueAsFloat64(const ValueClass* vc)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(vc, ValueClass::GetStaticClass(), "DMS_ValueType_GetUndefinedValueAsFloat64");
		return vc->GetUndefinedValueAsFloat64();

	DMS_CALL_END
	return UNDEFINED_VALUE(Float64);
}

RTC_CALL Int32  DMS_CONV DMS_ValueType_GetSize(const ValueClass* vc)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(vc, ValueClass::GetStaticClass(), "DMS_ValueType_GetSize");
		return vc->GetSize();

	DMS_CALL_END
	return UNDEFINED_VALUE(Int32);
}

RTC_CALL UInt8  DMS_CONV DMS_ValueType_NrDims(const ValueClass* vc)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(vc, ValueClass::GetStaticClass(), "DMS_ValueType_NrDims");
		return vc->GetNrDims();

	DMS_CALL_END
	return UNDEFINED_VALUE(UInt8);
}

RTC_CALL bool  DMS_CONV DMS_ValueType_IsNumeric(const ValueClass* vc)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(vc, ValueClass::GetStaticClass(), "DMS_ValueType_IsNumeric");
		return vc->IsNumeric();

	DMS_CALL_END
	return false;
}

RTC_CALL bool  DMS_CONV DMS_ValueType_IsIntegral(const ValueClass* vc)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(vc, ValueClass::GetStaticClass(), "DMS_ValueType_IsIntegral");
		return vc->IsIntegral();

	DMS_CALL_END
	return false;
}

RTC_CALL bool DMS_CONV DMS_ValueType_IsSigned(const ValueClass* vc)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(vc, ValueClass::GetStaticClass(), "DMS_ValueType_IsSigned");
		return vc->IsSigned();

	DMS_CALL_END
	return false;
}

RTC_CALL bool  DMS_CONV DMS_ValueType_IsSequence(const ValueClass* vc)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(vc, ValueClass::GetStaticClass(), "DMS_ValueType_IsSequence");
		return vc->IsSequence();

	DMS_CALL_END
	return false;
}


RTC_CALL const ValueClass* DMS_CONV DMS_ValueType_GetCrdClass(const ValueClass* vc)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(vc, ValueClass::GetStaticClass(), "DMS_ValueType_GetCrdClass");
		return vc->GetCrdClass();

	DMS_CALL_END
	return nullptr;
}
