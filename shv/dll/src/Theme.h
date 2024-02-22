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
#pragma once

#ifndef __SHV_THEME_H
#define __SHV_THEME_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "act/Actor.h"
#include "act/any.h"
#include "geo/color.h"
#include "ptr/WeakPtr.h"

#include "TreeItem.h"
#include "DataLocks.h"

#include "CalcClassBreaks.h"
#include "Aspect.h"

class AbstrDataItem;
class AbstrThemeValueGetter;
struct LayerInfo;

const UInt32 THF_IsDisabled = actor_flag_set::AF_Next * 0x0001;

//----------------------------------------------------------------------
//	class  : Theme
//----------------------------------------------------------------------

	//	theme:     (E->V, Cls->V, Cls->Aspect) 
	// conditions:	V is numeric
	//             ValueType(Aspect) depends on AspectNr
	//				any mapping is given; if Cls->V is given, also a Cls->Aspect is given
	// possible simplifications:
	//            E == V:           (nil,          ClsID->E, Cls->Aspect) : missing thematicAttr
	//            E == Cls, V=Aspect(E->Aspect,    nil,      nil)         : missing classificationAttr, paletteAttr 
	//            V == ClsID :      (E->Cls,       nil,      Cls->Aspect) : missing classificationAttr
	//            Cls == Void       (Void->Aspect, nil,      nil)
	// simplicifcations for FeatureThemes are described in LayerInfo.h
Float64 GetMaxValue(const Theme* theme, Float64 defaultMax);

class Theme : public Actor, public std::enable_shared_from_this<Theme>
{
	typedef Actor base_type;
public:
	Theme(AspectNr aNr, const AbstrDataItem* themeAttr, const AbstrDataItem* classBreaks, const AbstrDataItem* palette);
	~Theme();

//	delayed construction
	void Sync(TreeItem* themeContext, ShvSyncMode sm);
	void CheckConsistency() const;

	void SetThemeAttr     (const AbstrDataItem*);
	void SetClassification(const AbstrDataItem*);
	void SetPaletteAttr   (const AbstrDataItem*);

//	extend contract of Actor
	const AbstrDataItem* GetThemeAttr()      const;
	const AbstrDataItem* GetActiveAttr()     const;
	const AbstrDataItem* GetThemeAttrSource()const;
	const AbstrDataItem* GetClassification() const;
	const AbstrDataItem* GetPaletteAttr()    const;
	const AbstrDataItem* GetPaletteOrThemeAttr()    const;
	const AbstrDataItem* GetThemeOrPaletteAttr() const;
	AspectNr             GetAspectNr()       const { return m_AspectNr; }
	CharPtr              GetAspectName()     const;

	const AbstrUnit* GetThemeEntityUnit()    const;
	const AbstrUnit* GetThemeValuesUnit()    const;
	const AbstrUnit* GetClassIdUnit()        const;
	const AbstrUnit* GetPaletteValuesUnit()  const;

	bool IsAspectParameter() const;
	bool IsAspectAttr() const;
	bool IsIdentity() const { return !m_ThemeAttr && !m_Classification && !m_PaletteAttr; }
	const AbstrUnit* GetPaletteDomain()      const;
	bool IsCompatibleTheme(const Theme* othTheme) const;
	bool HasCompatibleDomain(const AbstrUnit* entity) const;

	Float64   GetNumericAspectValue () const;
	Int32     GetOrdinalAspectValue () const;
	UInt32    GetCardinalAspectValue() const;
	DmsColor  GetColorAspectValue   () const;
	SharedStr GetTextAspectValue    () const;

	WeakPtr<const AbstrThemeValueGetter> GetValueGetter() const;
	Float64 GetMaxValue() const;

	void Disable()          { m_State.Set  (THF_IsDisabled); }
	void Enable ()          { m_State.Clear(THF_IsDisabled); }
	bool IsDisabled() const { return m_State.Get  (THF_IsDisabled); }
	bool IsEditable() const
	{
		auto adi = GetThemeAttrSource();
		return !adi || adi->IsEditable();
	}
	static std::shared_ptr<Theme> Create(
		AspectNr aNr,
		const AbstrDataItem* thematicAttr,
		const LayerInfo& featureLayerInfo,
		DataView* dv, bool doThrow
	);

	static std::shared_ptr<Theme> Create(
		AspectNr aNr,
		const AbstrDataItem* thematicAttr,
		const AbstrDataItem* classification,
		const AbstrDataItem* palette
	);

	static std::shared_ptr<Theme> Create(AspectNr aNr, TreeItem* themeContext);
	static std::shared_ptr<Theme> Create(AspectNr aNr, AbstrThemeValueGetter* avg);
	static std::shared_ptr<Theme> Create(Theme* src);
	template <typename V> static std::shared_ptr<Theme> CreateValue(AspectNr aNr, V  value);
	template <typename V> static std::shared_ptr<Theme> CreateVar  (AspectNr aNr, V* valuePtr, GraphicObject* go);


	ActorVisitState PrepareThemeData(const Actor* act) const;
	void ResetDataLock() const;

protected: friend struct ThemeSet;
//	override virtuals of Actor
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;
	void DoInvalidate () const override;
	TokenID GetID() const override;

	MG_DEBUGCODE( bool IsShvObj() const { return true; } )

private: 
	friend class DataItemColumn;
	rtc::any::Any m_ClassTask;
	SharedDataItemInterestPtr
		m_ThemeAttr      // E     -> V
	,	m_Classification // ClsID -> V
	,	m_PaletteAttr;   // ClsID -> Aspect

	AspectNr
		m_AspectNr;
	mutable OwningPtr<AbstrThemeValueGetter>
		m_ValueGetterPtr;
	mutable Float64 m_MaxValue;

	DECL_RTTI(SHV_CALL, ShvClass);
};

SharedDataItemInterestPtr CreatePaletteData(DataView* dv, const AbstrUnit* domain, AspectNr aNr, bool ramp, bool always, const Float64* first, const Float64* last);

#endif // __SHV_THEME_H

