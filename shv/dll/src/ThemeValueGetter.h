#ifndef __SHV_THEMEVALUEGETTER_H
#define __SHV_THEMEVALUEGETTER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include <map>

#include "geo/color.h"
#include "geo/Conversions.h"
#include "ptr/OwningPtr.h"
#include "ptr/SharedArrayPtr.h"

#include "ptr/InterestHolders.h"

#include "TileLock.h"
#include "DisplayValue.h"

#include "GraphicObject.h"

class Theme;
struct GridDrawer;

//----------------------------------------------------------------------
//	class AbstrThemeValueGetter
//----------------------------------------------------------------------

class AbstrThemeValueGetter
{
protected:
	AbstrThemeValueGetter(const AbstrDataItem* paletteAttr);
public:
	virtual ~AbstrThemeValueGetter();

	Float64   GetNumericValue (entity_id entityIndex) const;
	DmsColor  GetColorValue   (entity_id entityIndex) const;
	Int32     GetOrdinalValue (entity_id entityIndex) const;
	UInt32    GetCardinalValue(entity_id entityIndex) const;
	SharedStr GetStringValue  (entity_id entityIndex, GuiReadLock& lock) const;
	TextInfo  GetTextInfo     (entity_id entityIndex, GuiReadLock& lock) const;
	SharedStr GetDisplayValue (entity_id entityIndex, bool useMetric, SizeT maxLen, GuiReadLockPair& locks) const;
	void GenerateValueInfo(entity_id entityIndex) const;

	virtual entity_id GetClassIndex(entity_id entityID) const= 0;
	virtual bool   MustRecalc() const { return true; }
	virtual SizeT GetCount() const = 0;

	virtual bool   HasSameThemeGuarantee(const AbstrThemeValueGetter* oth) const  { return false; }
	virtual bool   IsParameterGetter() const { return false; }
	virtual bool   IsDirectGetter   () const { return false; }

	virtual void   GridFillDispatch(const GridDrawer* drawer, tile_id t, Range<SizeT> themeArrayIndexRange, bool isLastRun) const; 

	const AbstrThemeValueGetter* CreatePaletteGetter() const;

	const AbstrDataItem* GetPaletteAttr() const { return m_PaletteAttr; }

	virtual void ResetDataLock() const;

private:
	const AbstrDataItem*                      m_PaletteAttr;
	mutable SharedDataItemInterestPtrTuple    m_DisplayInterest;
	mutable OwningPtr<const AbstrThemeValueGetter> m_PaletteGetter;
//	mutable ReadableTileLock m_PaletteLock;
};


//----------------------------------------------------------------------
//	class AbstrArrayValueGetter
//----------------------------------------------------------------------

typedef Point<const AbstrDataItem*> ThemeClassPairType;

//----------------------------------------------------------------------
//	class ArrayValueGetter
//----------------------------------------------------------------------

template <typename ClassIdType>
struct ArrayValueGetter : AbstrThemeValueGetter
{
	typedef typename sequence_traits<ClassIdType>::value_type    value_type;
	typedef typename sequence_traits<ClassIdType>::const_pointer const_pointer;

	typedef SharedArray<value_type>    ClassIndexArrayType;
	typedef SharedArrayPtr<value_type> ClassIndexArrayPtrType;

	ArrayValueGetter(const AbstrDataItem* paletteAttr);
	~ArrayValueGetter() override;

	void Assign(ThemeClassPairType tcp, ClassIndexArrayType* arrayPtr);

	entity_id GetClassIndex(entity_id entityIndex) const override
	{
		assert(entityIndex < GetCount());
		return Convert<entity_id>(m_ClassIndexArray[entityIndex]);
	}
	SizeT GetCount() const override 
	{
		return m_ClassIndexArrayHolder.size();
	}

	bool HasSameThemeGuarantee(const AbstrThemeValueGetter* oth) const override; 
	const_pointer GetClassIndexArray() const { return m_ClassIndexArray; }

	static std::map<ThemeClassPairType, ClassIndexArrayPtrType> s_ArrayMap;

private:
	const_pointer          m_ClassIndexArray;
	ClassIndexArrayPtrType m_ClassIndexArrayHolder;
	ThemeClassPairType     m_ThemeClassPair;
};

//----------------------------------------------------------------------
//	class ConstValueGetter
//----------------------------------------------------------------------

template <typename ClassIdType>
struct ConstValueGetter : AbstrThemeValueGetter
{
	typedef typename sequence_traits<ClassIdType>::value_type    value_type;
//	typedef typename sequence_traits<ClassIdType>::const_pointer const_pointer;

	typedef SharedArray<value_type>    ClassIndexArrayType;
	typedef SharedArrayPtr<value_type> ClassIndexArrayPtrType;

	ConstValueGetter(ClassIdType value) : AbstrThemeValueGetter(nullptr), m_Value(value) {}

	bool IsParameterGetter() const override { return true; }

	entity_id GetClassIndex(entity_id entityIndex) const override
	{
//		dms_assert(entityIndex < GetCount());
		return Convert<entity_id>(m_Value);
	}
	SizeT GetCount() const override
	{
		return 1;
	}

	ClassIdType m_Value;
};

//----------------------------------------------------------------------
//	class ConstVarGetter
//----------------------------------------------------------------------

template <typename ClassIdType>
struct ConstVarGetter : AbstrThemeValueGetter
{
	typedef typename sequence_traits<ClassIdType>::value_type    value_type;
//	typedef typename sequence_traits<ClassIdType>::const_pointer const_pointer;

	typedef SharedArray<value_type>    ClassIndexArrayType;
	typedef SharedArrayPtr<value_type> ClassIndexArrayPtrType;
	bool IsParameterGetter() const override { return true; }

	ConstVarGetter(ClassIdType* valuePtr, GraphicObject* src)
		: AbstrThemeValueGetter(nullptr), m_ValuePtr(valuePtr)
		, m_SrcObject(src->shared_from_this())
	{}

	entity_id GetClassIndex(entity_id entityIndex) const override
	{
		return Convert<entity_id>(*m_ValuePtr);
	}
	SizeT GetCount() const override
	{
		return 1;
	}
	void SetValue(ClassIdType newValue)
	{
		*m_ValuePtr = newValue;
		auto srcObject = m_SrcObject.lock();
		if (srcObject)
			srcObject->Invalidate();
	}
private:
	ClassIdType* m_ValuePtr;
	std::weak_ptr<GraphicObject> m_SrcObject;
};

//----------------------------------------------------------------------
//	helper functions
//----------------------------------------------------------------------

WeakPtr<const AbstrThemeValueGetter> GetValueGetter(const Theme* themeOrNull);

template <typename ClassIdType>
inline WeakPtr<const ArrayValueGetter<ClassIdType> > GetArrayValueGetter(const Theme* theme)
{
	dms_assert(theme);
	return debug_cast<const ArrayValueGetter<ClassIdType>*>(GetValueGetter(theme).get_ptr() );
}

bool CompareValueGetter(WeakPtr<const AbstrThemeValueGetter>& compatibleTheme, const AbstrThemeValueGetter* additionalTheme);

#endif // __SHV_THEMEVALUEGETTER_H

