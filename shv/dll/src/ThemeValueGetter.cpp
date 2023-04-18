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

#include "ptr/InterestHolders.h"

#include "ThemeValueGetter.h"

#include "act/Actor.h"
#include "dbg/debug.h"
#include "utl/Instantiate.h"
#include "StgBase.h"


#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DisplayValue.h"
#include "UnitProcessor.h"

#include "rlookup.h"

#include "GridFill.h"
#include "Theme.h"

//----------------------------------------------------------------------
//	MakeClassIndexArray
//----------------------------------------------------------------------

template <typename II, typename T, typename TI>
typename std::enable_if<has_undefines_v<typename std::iterator_traits<TI>::value_type>, UInt32 >::type
classify2index_best(II ib, II ie, const T& dataValue, TI classBoundsPtr)
{
	return classify2index_checked(ib, ie, dataValue, classBoundsPtr);
}

template <typename II, typename T, typename TI>
typename std::enable_if<!has_undefines_v<typename std::iterator_traits<TI>::value_type>, UInt32 >::type
classify2index_best(II ib, II ie, const T& dataValue, TI classBoundsPtr)
{
	return classify2index(ib, ie, dataValue, classBoundsPtr);
}

template <typename ThemeValuesType, typename IndexType = SizeT>
struct ClassifyFunc
{
	typedef typename sequence_traits<ThemeValuesType>::container_type  ClassBreakData;

	ClassifyFunc(const AbstrDataItem* classBreaks)
	{
		//	make index for log(n) accesss: ValuesType by sorting the IndexType->V index on values of V
		PreparedDataReadLock drl(classBreaks);
		auto classBreakData = const_array_cast<ThemeValuesType>( classBreaks )->GetDataRead();
		m_ClassBreakData = ClassBreakData(classBreakData.begin(), classBreakData.end() );
		m_Index.clear();
		make_index(m_Index, m_ClassBreakData.size(), m_ClassBreakData.begin() );
	}
	IndexType operator ()( typename param_type<ThemeValuesType>::type value) const
	{
		return Convert<IndexType>(classify2index_best(
			begin_ptr(m_Index)
		,	end_ptr  (m_Index)
		,	value
		,	begin_ptr(m_ClassBreakData)
		));
	}

	ClassBreakData         m_ClassBreakData;
	std::vector<IndexType> m_Index;
};

template <typename ThemeValuesType, typename ClassIdType>
SharedArrayPtr<typename sequence_traits<ClassIdType>::value_type>
MakeClassIndexArray(ThemeClassPairType tcp)
{
	const ClassifyFunc<ThemeValuesType> classifyFunc(tcp.second);

	//	Prepare loop
	PreparedDataReadLock drl2(tcp.first);
	auto themeData = debug_cast<const DataArray<ThemeValuesType>*>(tcp.first->GetRefObj())->GetDataRead();

	SharedArrayPtr<typename sequence_traits<ClassIdType>::value_type> 
		resultingArray(SharedArray<typename sequence_traits<ClassIdType>::value_type>::Create(themeData.size(), false) );

	classify2index_range_best(
		begin_ptr(resultingArray)
	,	end_ptr  (resultingArray)
	,	begin_ptr(classifyFunc.m_Index)
	,	end_ptr  (classifyFunc.m_Index)
	,	begin_ptr(themeData)
	,	begin_ptr(classifyFunc.m_ClassBreakData)
	,	tcp.first->HasUndefinedValues()
	);

	return resultingArray; // ownership is transferred to caller
}

//----------------------------------------------------------------------
//	class DirectGetter
//----------------------------------------------------------------------

struct DirectGetter : public AbstrThemeValueGetter
{
	DirectGetter(const AbstrDataItem* paletteAttr);

	entity_id GetClassIndex(entity_id entityID) const override;
	SizeT GetCount() const override;

	bool MustRecalc    () const override { return false; }
	bool IsDirectGetter() const override { return true; }
};

//----------------------------------------------------------------------
//	implementation class AbstrThemeValueGetter
//----------------------------------------------------------------------

AbstrThemeValueGetter::AbstrThemeValueGetter(const AbstrDataItem* paletteAttr)
	:	m_PaletteAttr(paletteAttr)
{}

AbstrThemeValueGetter::~AbstrThemeValueGetter()
{}

Float64 AbstrThemeValueGetter::GetNumericValue(SizeT entityIndex) const
{
	entity_id classIndex = GetClassIndex(entityIndex);
	if (!IsDefined(classIndex))
		return UNDEFINED_VALUE(Float64);
	return m_PaletteAttr->GetRefObj()->GetValueAsFloat64(classIndex);
}

DmsColor AbstrThemeValueGetter::GetColorValue(entity_id entityIndex) const
{
	entity_id classIndex = GetClassIndex(entityIndex);
	if (!IsDefined(classIndex))
		return STG_Bmp_GetDefaultColor(CI_NODATA);
	if (!m_PaletteAttr)
		return classIndex;
	DataReadLock lock(m_PaletteAttr);
	return m_PaletteAttr->GetValue<DmsColor>(classIndex);
}

Int32 AbstrThemeValueGetter::GetOrdinalValue(SizeT entityIndex) const
{
	entity_id classIndex = GetClassIndex(entityIndex);
	if (!IsDefined(classIndex))
		return UNDEFINED_VALUE(Int32);

	if (!m_PaletteAttr)
		return classIndex;
	return m_PaletteAttr->GetRefObj()->GetValueAsInt32(classIndex);

}

UInt32 AbstrThemeValueGetter::GetCardinalValue(SizeT entityIndex) const
{
	assert(m_PaletteAttr);
	entity_id classIndex = GetClassIndex(entityIndex);
	if (!IsDefined(classIndex))
		return UNDEFINED_VALUE(Int32);
	return m_PaletteAttr->GetRefObj()->GetValueAsUInt32(classIndex);
}

SharedStr AbstrThemeValueGetter::GetStringValue(SizeT entityIndex, GuiReadLock& lock) const
{
	if (!m_PaletteAttr)
	{
		if (auto cvg = dynamic_cast<const ConstValueGetter<SharedStr>*>(this))
			return cvg->m_Value;
	}
	dms_assert(m_PaletteAttr);
	entity_id classIndex = GetClassIndex(entityIndex);
	if (!IsDefined(classIndex))
		return UNDEFINED_VALUE(SharedStr);
	return m_PaletteAttr->GetRefObj()->AsString(classIndex, lock);
}

TextInfo AbstrThemeValueGetter::GetTextInfo(SizeT entityIndex, GuiReadLock& lock) const
{
	if (!m_PaletteAttr)
	{
		if (auto cvg = dynamic_cast<const ConstValueGetter<SharedStr>*>(this))
			return TextInfo{ cvg->m_Value, false };
	}
	assert(m_PaletteAttr);
	entity_id classIndex = GetClassIndex(entityIndex);
	if (!IsDefined(classIndex))
	{
		static auto undefinedText = SharedStr("null class");
		return TextInfo{ undefinedText, true };
	}
	auto refObj = m_PaletteAttr->GetRefObj();
	if (refObj->IsNull(classIndex))
	{
		static auto undefinedText = SharedStr("null");
		return TextInfo{ undefinedText, true };
	}
	return TextInfo{ refObj->AsString(classIndex, lock), false };
}

SharedStr AbstrThemeValueGetter::GetDisplayValue(SizeT entityIndex, bool useMetric, SizeT maxLen, GuiReadLockPair& locks) const
{
	assert(m_PaletteAttr);
	entity_id classIndex = GetClassIndex(entityIndex);
	if (!IsDefined(classIndex))
		return UNDEFINED_VALUE(SharedStr);

	return DisplayValue(m_PaletteAttr, classIndex, useMetric, m_DisplayInterest, maxLen, locks);
}

const AbstrThemeValueGetter* AbstrThemeValueGetter::CreatePaletteGetter() const
{
	assert(m_PaletteAttr);
	if (IsDirectGetter())
		return this;
	if (m_PaletteGetter.is_null() || m_PaletteAttr != m_PaletteGetter->GetPaletteAttr())
		m_PaletteGetter.assign( new DirectGetter(m_PaletteAttr) );
	return m_PaletteGetter;
}

void AbstrThemeValueGetter::GridFillDispatch(const GridDrawer* drawer, tile_id t, Range<SizeT> themeArrayIndexRange, bool isLastRun) const
{
	throwIllegalAbstract(MG_POS, "GridFillDispatch");
}


void AbstrThemeValueGetter::ResetDataLock() const 
{
	if (m_PaletteGetter)
		m_PaletteGetter->ResetDataLock();
};

//----------------------------------------------------------------------
//	NullGetter for parameter aspects
//----------------------------------------------------------------------

struct NullGetter : public AbstrThemeValueGetter
{
	NullGetter(const AbstrDataItem* paletteAttr)
		:	AbstrThemeValueGetter(paletteAttr)
	{}
	// void domain
	entity_id GetClassIndex(SizeT entityID) const override { return 0; } 
	SizeT GetCount() const override { return 1; }
	bool MustRecalc() const override { return false; }

	bool IsParameterGetter  () const override { return true; }
	bool IsDirectGetter     () const override { return true; }
};

//----------------------------------------------------------------------
//	class IdentityGetter for 
//----------------------------------------------------------------------

struct IdentityGetter : public AbstrThemeValueGetter
{
	IdentityGetter() : AbstrThemeValueGetter(nullptr) {}

	// void domain
	entity_id GetClassIndex(entity_id entityID) const override { return entityID; }
	SizeT GetCount() const override { throwIllegalAbstract(MG_POS, "IdentityGetter::GetCount"); }
	bool MustRecalc() const override { return false; }

	bool IsDirectGetter  () const override { return true; }
};

//----------------------------------------------------------------------
//	DirectGetter implementation
//----------------------------------------------------------------------

DirectGetter::DirectGetter(const AbstrDataItem* paletteAttr)
	:	AbstrThemeValueGetter(paletteAttr)
{}

entity_id DirectGetter::GetClassIndex(entity_id entityID) const
{
	return entityID; // same domain
}

SizeT DirectGetter::GetCount() const 
{
	return GetPaletteAttr()->GetAbstrDomainUnit()->GetCount();
}

//----------------------------------------------------------------------
//	class IndirectGetter
//----------------------------------------------------------------------


template <typename V>
struct LazyGetter : public AbstrThemeValueGetter
{
	LazyGetter(
		const AbstrDataItem*    themeAttr,
		const AbstrDataItem*    paletteAttr
	)
		:	AbstrThemeValueGetter(paletteAttr)
//		,	m_ThemeData(const_array_cast<V>(themeAttr))
		,	m_ThemeAttr(themeAttr)
		,	m_UltimateDomain(AsUnit(themeAttr->GetAbstrDomainUnit()->GetUltimateItem()))
	{}

	V GetThemeValue(entity_id entityID) const
	{
//		dms_assert(m_ThemeData);

		tile_loc tl = m_UltimateDomain->GetTiledRangeData()->GetTiledLocation(entityID, m_PrevTileID);
		if (!IsDefined(tl.first))
			return UNDEFINED_OR_ZERO(V);
		if (m_PrevTileID != tl.first)
		{
			auto themeData = const_array_cast<V>(m_ThemeAttr);
			m_ThemeDataTileLock = std::move(themeData->GetTile(tl.first));
			m_PrevTileID = tl.first;
		}
		dms_assert(m_ThemeDataTileLock.m_TileHolder);
		CheckIndexToTileDataSize(tl.second, m_ThemeDataTileLock.size());

		return m_ThemeDataTileLock[tl.second];
	}

	void ResetDataLock() const override
	{
		m_ThemeDataTileLock = typename DataArray<V>::locked_cseq_t();
		m_PrevTileID = no_tile;
		AbstrThemeValueGetter::ResetDataLock();
	}


	SizeT GetCount() const override
	{

		return m_ThemeAttr->GetCurrRefObj()->GetTiledRangeData()->GetRangeSize();
	}

	const AbstrDataItem*  GetThemeAttr() const { return m_ThemeAttr.get_ptr(); }

private:
	SharedPtr<const AbstrDataItem>               m_ThemeAttr;
	mutable typename DataArray<V>::locked_cseq_t m_ThemeDataTileLock;
	mutable tile_id                              m_PrevTileID = no_tile;
	const AbstrUnit*                             m_UltimateDomain = nullptr;
};

template <typename V>
struct IndirectGetter : public LazyGetter<V>
{
	IndirectGetter(
		const AbstrDataItem*    themeAttr,
		const typename Unit<V>::range_t& valuesRange,
		const AbstrDataItem*    paletteAttr
	)
		:	LazyGetter<V>(themeAttr, paletteAttr)
		,	m_ValuesRange(valuesRange)
	{
	}

	entity_id GetClassIndex(entity_id entityID) const override
	{
		return Convert<entity_id>(Range_GetIndex_checked(m_ValuesRange, this->GetThemeValue(entityID)));
	}

	bool HasSameThemeGuarantee(const AbstrThemeValueGetter* oth) const override 
	{ 
		const IndirectGetter<V>* other = dynamic_cast<const IndirectGetter<V>*>(oth);
		return other && this->GetThemeAttr() == other->GetThemeAttr();
	}

private:
	typename Unit<V>::range_t m_ValuesRange;
};

//----------------------------------------------------------------------
//	class LazyClassifyGetterCreator
//----------------------------------------------------------------------

template<typename V, typename ClassIdType>
struct LazyClassifyGetter : public LazyGetter<V>
{
	LazyClassifyGetter(ThemeClassPairType tcp, const AbstrDataItem* paletteAttr)
		:	LazyGetter<V>(tcp.first, paletteAttr)
		,	m_ClassifyAttr(tcp.second)
		,	m_ClassifyFunc(tcp.second)
	{}

	entity_id GetClassIndex(entity_id entityID) const override
	{
		return Convert<entity_id>(m_ClassifyFunc(this->GetThemeValue(entityID)));
	}

	bool HasSameThemeGuarantee(const AbstrThemeValueGetter* oth) const override 
	{ 
		const LazyClassifyGetter* other = dynamic_cast<const LazyClassifyGetter*>(oth);
		return other 
			&& (this->GetThemeAttr() == other->GetThemeAttr()) 
			&& (m_ClassifyAttr == other->m_ClassifyAttr);
	}

	void GridFillDispatch(const GridDrawer* drawer, tile_id t, Range<SizeT> themeArrayIndexRange, bool isLastRun) const override
	{
		using PixelType = pixel_type_t<ClassIdType>;
		auto themeData = const_array_cast<V>(this->GetThemeAttr())->GetLockedDataRead(t);
		dms_assert(themeData.size() == Cardinality(themeArrayIndexRange) || drawer->m_EntityIndex.is_null());
		GridFill<V, PixelType>(
			drawer
		,	themeData.begin()
		,	m_ClassifyFunc
		,   themeArrayIndexRange
		,   isLastRun
		);
	}

private:
	SharedDataItemInterestPtr                    m_ClassifyAttr;
	ClassifyFunc<V>                              m_ClassifyFunc;
};


//----------------------------------------------------------------------
//	class ArrayValueGetter
//----------------------------------------------------------------------

template <typename ClassIdType>
std::map<ThemeClassPairType, typename ArrayValueGetter<ClassIdType>::ClassIndexArrayPtrType > 
ArrayValueGetter<ClassIdType>::s_ArrayMap;

template <typename ClassIdType>
ArrayValueGetter<ClassIdType>::ArrayValueGetter(const AbstrDataItem* paletteAttr)
	:	AbstrThemeValueGetter(paletteAttr)
	,	m_ClassIndexArray( const_pointer() )
{}

template <typename ClassIdType>
ArrayValueGetter<ClassIdType>::~ArrayValueGetter()
{
	if (m_ClassIndexArrayHolder && m_ClassIndexArrayHolder->GetRefCount() == 2)
		s_ArrayMap.erase(m_ThemeClassPair);
}

template <typename ClassIdType>
void ArrayValueGetter<ClassIdType>::Assign(ThemeClassPairType tcp, ClassIndexArrayType* arrayPtr)
{
	dms_assert(!m_ClassIndexArrayHolder); // PRECONDITION, Only Assign once
	dms_assert(arrayPtr); // PRECONDITION
	m_ClassIndexArray       = arrayPtr->begin();
	m_ClassIndexArrayHolder = arrayPtr;
	m_ThemeClassPair        = tcp;
}

template <typename ClassIdType>
bool ArrayValueGetter<ClassIdType>::HasSameThemeGuarantee(const AbstrThemeValueGetter* oth) const
{
	const ArrayValueGetter* other = dynamic_cast<const ArrayValueGetter*>(oth);
	return other && m_ClassIndexArray == other->m_ClassIndexArray;
}

//----------------------------------------------------------------------
//	IndirectGetterCreator
//----------------------------------------------------------------------

struct IndirectGetterCreator : UnitProcessor
{
	IndirectGetterCreator(const AbstrDataItem* themeAttr, const AbstrDataItem* paletteAttr)
		:	m_ThemeAttr(themeAttr)
		,	m_PaletteAttr(paletteAttr)
		,	m_ResultingGetter(0)
	{
		dms_assert(themeAttr);
		themeAttr->GetAbstrValuesUnit()->InviteUnitProcessor(*this);
	}
	template <typename V>
	void VisitImpl(const Unit<V>* themeValuesUnit) const
	{
		m_ResultingGetter = new IndirectGetter<V>(
			AsDataItem( m_ThemeAttr->GetUltimateItem() )
		,	themeValuesUnit->GetRange()
		,	m_PaletteAttr
		);
	}

	// called from themeAttr->GetAbstrValuesUnit()->Invite(*this) == paletteAttr->GetAbstrDomainUnit
	#define INSTANTIATE(V) \
		void Visit(const Unit<V>* valuesUnit) const override { VisitImpl<V>(valuesUnit); }
		INSTANTIATE_DOMAIN_INTS
	#undef INSTANTIATE

	const AbstrDataItem*      m_ThemeAttr;
	const AbstrDataItem*      m_PaletteAttr;
	mutable AbstrThemeValueGetter* m_ResultingGetter;
};

struct ClassifiedGetterCreator : UnitProcessor
{
	ClassifiedGetterCreator(
		const AbstrDataItem* themeAttr, 
		const AbstrDataItem* classification, 
		const AbstrDataItem* paletteAttr
	)	:	m_ThemeClassPair(themeAttr, classification)
		,	m_PaletteAttr(paletteAttr)
		,	m_ResultingGetter(0)

	{
		dms_assert(themeAttr);
		dms_assert(classification);
		dms_assert(paletteAttr);
		classification->GetAbstrDomainUnit()->InviteUnitProcessor( *this );
	}

	template <typename ClassIdType>
	struct ArrayFiller : UnitProcessor
	{
		ArrayFiller(const ThemeClassPairType& themeClassPair,
			SharedArrayPtr<typename sequence_traits<ClassIdType>::value_type>& arrayPtrRef
		)
			:	m_ThemeClassPair(themeClassPair)
			,	m_ArrayPtrRef   (arrayPtrRef)
		{}

		template <typename ThemeValuesType>
		void VisitImpl() const
		{
			m_ArrayPtrRef = MakeClassIndexArray<ThemeValuesType, ClassIdType>( m_ThemeClassPair );
		}

		#define INSTANTIATE(V) \
			void Visit(const Unit<V>* themeValuesUnit) const override { VisitImpl<V>(); }
			INSTANTIATE_NUM_ELEM
		#undef INSTANTIATE


	private:
		ThemeClassPairType                                                  m_ThemeClassPair;
		SharedArrayPtr<typename sequence_traits<ClassIdType>::value_type>&  m_ArrayPtrRef;
	};

	template <typename ClassIdType>
	struct LazyClassifyGetterCreator: UnitProcessor
	{
		LazyClassifyGetterCreator(const ThemeClassPairType& themeClassPair, const AbstrDataItem* paletteAttr)
			:	m_ThemeClassPair(themeClassPair)
			,	m_PaletteAttr(paletteAttr)
		{}

		template <typename ThemeValuesType>
		void VisitImpl() const
		{
			m_ValueGetter.assign(new LazyClassifyGetter<ThemeValuesType, ClassIdType>(m_ThemeClassPair, m_PaletteAttr) );
		}

		#define INSTANTIATE(V) \
			void Visit(const Unit<V>* themeValuesUnit) const override { VisitImpl<V>(); }
			INSTANTIATE_NUM_ELEM
		#undef INSTANTIATE

		AbstrThemeValueGetter* GetValueGetter() { return m_ValueGetter.release(); }

	private:
		ThemeClassPairType                  m_ThemeClassPair;
		const AbstrDataItem*                m_PaletteAttr;
		mutable OwningPtr<AbstrThemeValueGetter> m_ValueGetter;
	};

	template <typename ClassIdType>
	void CreateBufferedClassGetter() const
	{
		DBG_START("ClassifiedGetterCreator", "CreateBufferedClassGetter", MG_DEBUG_VALUEGETTER);
		DBG_TRACE(("Theme %s", m_ThemeClassPair.first ->GetFullName().c_str())); 
		DBG_TRACE(("Clsfn %s", m_ThemeClassPair.second->GetFullName().c_str())); 

		SharedArrayPtr<typename sequence_traits<ClassIdType>::value_type>& arrayPtrRef = ArrayValueGetter<ClassIdType>::s_ArrayMap[m_ThemeClassPair];
		if (!arrayPtrRef)
		{
			DBG_TRACE(("New Classification created"));
			
			m_ThemeClassPair.first->GetAbstrValuesUnit()->InviteUnitProcessor( 
				ArrayFiller<ClassIdType>(m_ThemeClassPair, arrayPtrRef) 
			);
		}
		else
		{
			DBG_TRACE(("Old Classification re-used"));
		}

		DBG_TRACE(("Count %d", arrayPtrRef->GetRefCount())); 

		OwningPtr< ArrayValueGetter<ClassIdType> > resultingGetter( new ArrayValueGetter<ClassIdType>(m_PaletteAttr) );

		resultingGetter->Assign(m_ThemeClassPair, arrayPtrRef);

		m_ResultingGetter = resultingGetter.release();
	}

	template <typename ClassIdType>
	void CreateLazyClassifyGetter() const
	{
		DBG_START("ClassifiedGetterCreator", "CreateLazyClassifyGetter", MG_DEBUG_VALUEGETTER);
		DBG_TRACE(("Theme %s", m_ThemeClassPair.first ->GetFullName().c_str())); 
		DBG_TRACE(("Clsfn %s", m_ThemeClassPair.second->GetFullName().c_str())); 

		LazyClassifyGetterCreator<ClassIdType> classifyCreator(m_ThemeClassPair, m_PaletteAttr);
		m_ThemeClassPair.first->GetAbstrValuesUnit()->InviteUnitProcessor(classifyCreator);

		m_ResultingGetter = classifyCreator.GetValueGetter();
	}

	template <typename ClassIdType>
	void VisitImpl() const
	{
		if (m_ThemeClassPair.first->GetAbstrDomainUnit()->IsTiled())
			CreateLazyClassifyGetter<ClassIdType>();
		else
			CreateBufferedClassGetter<ClassIdType>();
	}

	#define INSTANTIATE(E) \
		void Visit(const Unit<E>* classIdUnit) const override { VisitImpl<E>(); }
		INSTANTIATE_DOMAIN_INTS
	#undef INSTANTIATE

	AbstrThemeValueGetter* GetResultingGetter() const 
	{
		dms_assert(m_ResultingGetter);
		return m_ResultingGetter;
	}


private:
	ThemeClassPairType        m_ThemeClassPair;
	const AbstrDataItem*      m_PaletteAttr;
	mutable AbstrThemeValueGetter* m_ResultingGetter;
};

/* ============================= Theme::GetValueGetter =============================*/

WeakPtr<const AbstrThemeValueGetter> Theme::GetValueGetter() const
{
	DetermineState(); // invalidate m_ValueGetter if m_ThemeAttr or m_ClassBreaks changes (or m_Palette, TODO: don't invalidate it on that)
	dms_assert(GetAspectNr() != AN_Feature);

	if (!m_ValueGetterPtr)
	{
		if (IsIdentity())
			m_ValueGetterPtr.assign(new IdentityGetter());           // no theme or classification
		else if (IsAspectParameter())
			m_ValueGetterPtr.assign(new NullGetter(GetThemeAttr())); // no theme or classification
		else if (GetClassification())
		{
			dms_assert(GetPaletteAttr());
			dms_assert(GetThemeAttr());
			if (GetThemeAttr())
				m_ValueGetterPtr.assign(
					ClassifiedGetterCreator(
						GetThemeAttr(), 
						GetClassification(), 
						GetPaletteAttr()
					).GetResultingGetter()
				);
			else
				throwErrorF("Theme", "OrderedValueGetter NYI: Cannot make valueGetter for a Theme without ThematicAttr and with a Classification (%s)", GetClassification()->GetFullName().c_str());
		}
		else
		{
			dms_assert(GetThemeAttr());
			if (GetPaletteAttr())
			{
				m_ValueGetterPtr.assign(
					IndirectGetterCreator(
						GetThemeAttr(), 
						GetPaletteAttr()
					).m_ResultingGetter
				);
			}
			else // no paletteAttr, thus themeAttr contains aspects directly
				m_ValueGetterPtr.assign(
					new DirectGetter( GetThemeAttr() )
				);
		}
	}
	dms_assert(m_ValueGetterPtr);
	return m_ValueGetterPtr.get_ptr();
}


//----------------------------------------------------------------------
//	helper function
//----------------------------------------------------------------------

WeakPtr<const AbstrThemeValueGetter> GetValueGetter(const Theme* themeOrNull)
{
	if (themeOrNull)
		return themeOrNull->GetValueGetter();
	return nullptr;
}
