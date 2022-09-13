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

#ifndef __RTC_MCI_CLASS_H
#define __RTC_MCI_CLASS_H

#include "mci/Object.h"

//----------------------------------------------------------------------
// Macro's for RunTimeTypeInfo (introspection) and dynamic creation
//----------------------------------------------------------------------

// IMPL

#define IMPL_ABSTR(OBJ, CLASS) \
	const CLASS* s_register##OBJ = OBJ::GetStaticClass();

#define IMPL_RTTI(OBJ, CLASS) \
	const Class* OBJ::GetDynamicClass() const { return GetStaticClass(); }  \
	IMPL_ABSTR(OBJ, CLASS)

// IMPL_T1

#define IMPL_RTTI_T1(cls, CLASS, T) \
	const Class* cls<T>::GetDynamicClass() const { return GetStaticClass(); }  \
	static const CLASS*  g_##T##cls##Cls = cls<T>::GetStaticClass();

//**********  dynamic creation                           **********

using Constructor = Object* (*)();

/********** Class Class Definition (non-polymorphic) **********/

class MetaClass;

class Class : public Object
{
	using base_type = Object;

public:
	RTC_CALL Class(Constructor cFunc, const Class* baseCls, TokenID typeID);
	RTC_CALL ~Class();

	RTC_CALL TokenID GetID() const override { return m_TypeID; }

	RTC_CALL bool IsDerivedFrom(const Class* base) const;
	RTC_CALL Object*         CreateObj() const;
	RTC_CALL virtual Object* CreateObj(PolymorphInpStream*) const;
	RTC_CALL         bool    HasDefaultCreator() const;
	RTC_CALL virtual bool    IsDataObjType() const;

	RTC_CALL static const Class*     Find(TokenID id);
	RTC_CALL static UInt32           Size(); // # in ClassKernel
	RTC_CALL static const ClassCPtr* Begin();
	RTC_CALL static const ClassCPtr* End();

	class  AbstrPropDef* GetLastPropDef        () const { return m_LastPD;         }
	class  AbstrPropDef* GetLastCopyablePropDef() const { return m_LastCopyablePD; }
	RTC_CALL class  AbstrPropDef* FindPropDef(TokenID nameID) const;

	const Class* GetBaseClass()    const { return m_BaseClass;    }
	const Class* GetLastSubClass() const { return m_LastSubClass; }
	const Class* GetPrevClass()    const { return m_PrevClass;    }

	DECL_RTTI(RTC_CALL, MetaClass)

private:
	mutable const Class*  m_BaseClass;
	mutable const Class*  m_LastSubClass;
	mutable const Class*  m_PrevClass;

	mutable AbstrPropDef* m_LastPD; friend class  AbstrPropDef;
	mutable AbstrPropDef* m_LastCopyablePD; 
	Constructor           m_Constructor;
	TokenID               m_TypeID;
};

// IMPL_CLASS

#define IMPL_CLASS(cls, CreateFunc) \
	const Class* cls::GetStaticClass() \
	{ \
		static Class s_Cls(CreateFunc, cls::base_type::GetStaticClass(), GetTokenID_st(#cls) ); \
		return &s_Cls; \
	} 

#define IMPL_ABSTR_CLASS(OBJ) IMPL_ABSTR(OBJ, Class) IMPL_CLASS(OBJ, nullptr)
#define IMPL_RTTI_CLASS(OBJ)  IMPL_RTTI(OBJ, Class)  IMPL_CLASS(OBJ, nullptr)

// IMPL_CLASS_T1

#define IMPL_CLASS_T1(cls, T, cFunc) \
	const Class* cls<T>::GetStaticClass() \
	{ \
		static Class s_StaticCls(cFunc, base_type::GetStaticClass(), GetTokenID_st(#cls "<" #T ">") ); \
		return &s_StaticCls; \
	} 

/********** Class MetaClass Definition (non-polymorphic) **********/

class MetaClass : public Class
{
	typedef Class base_type;
public:
	typedef Object* (*createFromXmlFuncType)(Object* currObj, struct XmlElement& elem);

	RTC_CALL MetaClass(
		TokenID scriptID, 
		const Class* baseCls,
		createFromXmlFuncType xmlFunc);
	RTC_CALL ~MetaClass();

	RTC_CALL static MetaClass* Find(TokenID id);
	RTC_CALL Object* CreateFromXml(Object* currObj, struct XmlElement& elem) const;

	DECL_RTTI(RTC_CALL, MetaClass)

private:
	createFromXmlFuncType m_XmlFunc;
};


// IMPL_METACLASS

#define IMPL_METACLASS(cls, SCRIPTNAME, XML_FUNC) \
	const MetaClass*  cls::GetStaticClass() \
	{ \
		static MetaClass s_Cls(GetTokenID_st(SCRIPTNAME), base_type::GetStaticClass(), XML_FUNC); \
		return &s_Cls; \
	} 

#define IMPL_RTTI_METACLASS(cls, SCRIPTNAME, XML_FUNC) \
	IMPL_RTTI(cls, MetaClass) \
	IMPL_METACLASS(cls, SCRIPTNAME, XML_FUNC) 


#endif // __RTC_MCI_CLASS_H
