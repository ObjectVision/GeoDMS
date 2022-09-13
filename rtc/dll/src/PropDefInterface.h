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

#ifndef __PROPDEF_INTERFACE_H
#define __PROPDEF_INTERFACE_H

class Object;

//----------------------------------------------------------------------
// C style Interface DLL export support 
//----------------------------------------------------------------------

#include "RtcBase.h"

extern "C" {

RTC_CALL void     DMS_CONV DMS_IString_Release  (IStringHandle ptrString);
RTC_CALL CharPtr  DMS_CONV DMS_IString_AsCharPtr(IStringHandle ptrString);

// Object Composition Interface
RTC_CALL const PersistentSharedObj* DMS_CONV DMS_Object_GetParent (const PersistentSharedObj* self, UInt32 index);

// Object Naming Interface
RTC_CALL CharPtr       DMS_CONV DMS_Object_GetName    (const Object* self);
RTC_CALL CharPtr       DMS_CONV DMS_Object_GetFullName(const Object* self);

// Object Dynamic Typing Interface
RTC_CALL const Class*      DMS_CONV DMS_Object_GetDynamicClass(const Object* self);
RTC_CALL Object* DMS_CONV DMS_Object_QueryInterface(Object* self, const Class* requiredClass);

// Specializations
RTC_CALL CharPtr       DMS_CONV DMS_Class_GetName(const Class*);

RTC_CALL AbstrPropDef* DMS_CONV DMS_Class_FindPropDef(const Class* self, CharPtr propName);
RTC_CALL AbstrPropDef* DMS_CONV DMS_Class_GetLastPropDef(const Class* self);

RTC_CALL CharPtr       DMS_CONV DMS_PropDef_GetName(const AbstrPropDef* self);
RTC_CALL AbstrPropDef* DMS_CONV DMS_PropDef_GetPrevPropDef(const AbstrPropDef* self);

/********** Property          Definition (non-polymorphic) **********/

RTC_CALL AbstrValue*       DMS_CONV DMS_PropDef_CreateValue(const AbstrPropDef* self);
RTC_CALL void              DMS_CONV DMS_PropDef_GetValue(const AbstrPropDef* self, const Object* me, AbstrValue* value);
RTC_CALL IStringHandle     DMS_CONV DMS_PropDef_GetValueAsIString(const AbstrPropDef* self, const Object* me);
RTC_CALL void              DMS_CONV DMS_PropDef_SetValue(AbstrPropDef* self, Object* me, const AbstrValue* value);
RTC_CALL void              DMS_CONV DMS_PropDef_SetValueAsCharArray(AbstrPropDef* self, Object* me, CharPtr buffer);
RTC_CALL const ValueClass* DMS_CONV DMS_PropDef_GetValueClass(const AbstrPropDef* self);
RTC_CALL const Class*      DMS_CONV DMS_PropDef_GetItemClass(const AbstrPropDef* self);


/********** Static Class interface funcs **********/

RTC_CALL const Class* DMS_CONV DMS_GetRootClass();
RTC_CALL const Class* DMS_CONV DMS_Class_GetStaticClass();
RTC_CALL const Class* DMS_CONV DMS_PropDef_GetStaticClass();

/********** Inheritance Class Definition (non-polymorphic) **********/

RTC_CALL const Class* DMS_CONV DMS_Class_GetBaseClass(const Class* self);
RTC_CALL const Class* DMS_CONV DMS_Class_GetLastSubClass(const Class* self);
RTC_CALL const Class* DMS_CONV DMS_Class_GetPrevSubClass(const Class* self);

} // end extern "C"

#endif // __PROPDEF_INTERFACE_H
