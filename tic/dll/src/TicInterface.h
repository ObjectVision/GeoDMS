// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if !defined(__TIC_INTERFACE_H)
#define __TIC_INTERFACE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "TicBase.h"
#include "geo/ElemTraits.h"
#include "utl/Instantiate.h"
#include "act/ActorEnums.h"
#include "TreeItemProps.h"

//----------------------------------------------------------------------
// C++ style Interface functions for TreeItem retrieval
//----------------------------------------------------------------------

TIC_CALL auto TreeItem_GetBestItemAndUnfoundPart(const TreeItem* context, CharPtr path)->BestItemRef;

//----------------------------------------------------------------------
// C style Interface functions for construction
//----------------------------------------------------------------------
extern "C" {

TIC_CALL TreeItem*   DMS_CONV DMS_CreateTree      (CharPtr name);
TIC_CALL TreeItem*   DMS_CONV DMS_CreateTreeItem  (TreeItem* context, CharPtr name);

TIC_CALL AbstrUnit*  DMS_CONV DMS_CreateUnit(TreeItem* parent, CharPtr name, const UnitClass* vt);
TIC_CALL const AbstrUnit*  DMS_CONV DMS_GetDefaultUnit(const UnitClass* uc);

TIC_CALL const UnitClass* DMS_CONV DMS_UnitClass_GetFirstInstance();
TIC_CALL const UnitClass* DMS_CONV DMS_UnitClass_GetNextInstance(const UnitClass* self);

TIC_CALL AbstrParam* DMS_CONV DMS_CreateParam(TreeItem* context, CharPtr name, 
		const AbstrUnit* valueUnit, ValueComposition vc);

TIC_CALL AbstrDataItem* DMS_CONV DMS_CreateDataItem(TreeItem* context, CharPtr name, 
		const AbstrUnit* domainUnit, const AbstrUnit* valuesUnit, ValueComposition vc);

//----------------------------------------------------------------------
// C style Interface function for treeitem copy 
//----------------------------------------------------------------------

TIC_CALL TreeItem* DMS_CONV DMS_TreeItem_Copy(TreeItem* dest, const TreeItem* source, CharPtr name);
//REMOVE TIC_CALL void DMS_CONV DMS_DataItem_CopyData(AbstrDataItem* dest, const AbstrDataItem* source);
TIC_CALL bool DMS_CONV DMS_TreeItem_IsEndogenous(const TreeItem* x);
TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetCreator(const TreeItem* x);
TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetTemplInstantiator(const TreeItem* x);
TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetTemplSource(const TreeItem* x);
TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetTemplSourceItem(const TreeItem* templI, const TreeItem* target);

//----------------------------------------------------------------------
// C style Interface functions for class id retrieval
//----------------------------------------------------------------------

TIC_CALL const Class* DMS_CONV DMS_TreeItem_GetStaticClass();

TIC_CALL const Class*     DMS_CONV DMS_AbstrUnit_GetStaticClass();
TIC_CALL const UnitClass* DMS_CONV DMS_UnitClass_Find(const ValueClass*);

TIC_CALL const UnitClass* DMS_CONV DMS_UInt8Unit_GetStaticClass();
TIC_CALL const UnitClass* DMS_CONV DMS_UInt32Unit_GetStaticClass();
TIC_CALL const UnitClass* DMS_CONV DMS_StringUnit_GetStaticClass();
TIC_CALL const UnitClass* DMS_CONV DMS_VoidUnit_GetStaticClass();
TIC_CALL const Class* DMS_CONV DMS_AbstrParam_GetStaticClass();

TIC_CALL const Class* DMS_CONV DMS_AbstrDataItem_GetStaticClass();
TIC_CALL const DataItemClass* DMS_CONV DMS_DataItemClass_Find(
			const UnitClass* valuesUnitClass, ValueComposition vc);
TIC_CALL bool DMS_AbstrDataItem_HasVoidDomainGuarantee(const AbstrDataItem* self);

//----------------------------------------------------------------------
// C style Interface functions for Runtime Type Info (RTTI), Dynamic Casting, AddRef / Release (IUnknown)
//----------------------------------------------------------------------

TIC_CALL const Object* DMS_CONV DMS_TreeItem_QueryInterface(const Object* self, const Class* requiredClass); // MOVE TO RTC, RENAME
TIC_CALL Object*       DMS_CONV DMS_TreeItemRW_QueryInterface(Object* self, const Class* requiredClass); // MOVE TO RTC, RENAME
TIC_CALL const Class*  DMS_CONV DMS_TreeItem_GetDynamicClass(const TreeItem* self);

TIC_CALL const AbstrDataItem* DMS_CONV DMS_TreeItem_AsAbstrDataItem  (const TreeItem* );
TIC_CALL const AbstrUnit*     DMS_CONV DMS_TreeItem_AsAbstrUnit      (const TreeItem* );

// Helper function: gives the name of a runtime class
TIC_CALL CharPtr              DMS_CONV DMS_CRuntimeClass_GetName(const Class*);

// garbage collection
TIC_CALL void                 DMS_CONV DMS_TreeItem_AddRef(TreeItem* self);
TIC_CALL void                 DMS_CONV DMS_TreeItem_Release(TreeItem* self);
TIC_CALL UInt32               DMS_CONV DMS_TreeItem_GetInterestCount(const TreeItem* );

//----------------------------------------------------------------------
// C style Interface functions for destruction
//----------------------------------------------------------------------

TIC_CALL void   DMS_CONV DMS_TreeItem_SetAutoDelete(TreeItem* self);
TIC_CALL bool   DMS_CONV DMS_TreeItem_GetAutoDeleteState(TreeItem* self);
TIC_CALL bool   DMS_CONV DMS_TreeItem_IsCacheItem(TreeItem* self);
TIC_CALL bool   DMS_CONV DMS_TreeItem_IsHidden(TreeItem* self);
TIC_CALL void   DMS_CONV DMS_TreeItem_SetIsHidden(TreeItem* self, bool isHidden);
TIC_CALL bool   DMS_CONV DMS_TreeItem_InHidden(TreeItem* self);
TIC_CALL bool   DMS_CONV DMS_TreeItem_IsTemplate(TreeItem* self);
TIC_CALL bool   DMS_CONV DMS_TreeItem_InTemplate(TreeItem* self);
TIC_CALL UInt32 DMS_CONV DMS_TreeItem_TotalNrOfItems();

//----------------------------------------------------------------------
// C style Interface functions for storage 
//----------------------------------------------------------------------

TIC_CALL bool DMS_CONV DMS_TreeItem_HasStorage(TreeItem* self);
TIC_CALL void DMS_CONV DMS_TreeItem_Commit(TreeItem* self); // stores unsaved primary data of self and descendants
TIC_CALL void DMS_CONV DMS_TreeItem_DisableStorage(TreeItem* self); // disable storage for 'self' and subitems

TIC_CALL void DMS_CONV DMS_TreeItem_XML_Dump(const TreeItem* self, OutStreamBase* out);
TIC_CALL bool DMS_CONV DMS_TreeItem_XML_DumpAllProps(const TreeItem* self, OutStreamBase* xmlOutStrPtr, bool showAll);
TIC_CALL void DMS_CONV DMS_TreeItem_XML_DumpExplore(const TreeItem* self, OutStreamBase* xmlOutStrPtr, bool viewHidden);

TIC_CALL bool DMS_CONV DMS_TreeItem_Dump(const TreeItem* self, CharPtr fileName); // creates a temp. XML_Stream on a temp. FileOutBuff

//----------------------------------------------------------------------
// C style Interface functions for TreeItem retrieval
//----------------------------------------------------------------------

TIC_CALL TreeItem*        DMS_CONV DMS_TreeItem_CreateItem(TreeItem* context, CharPtr path, const Class* requiredClass =nullptr);
TIC_CALL const TreeItem*  DMS_CONV DMS_TreeItem_GetItem(const TreeItem* current, CharPtr path, const Class* requiredClass = nullptr);
TIC_CALL const TreeItem*  DMS_CONV DMS_TreeItem_GetBestItemAndUnfoundPart(const TreeItem* context, CharPtr path, IStringHandle* unfoundPart);
TIC_CALL const TreeItem*  DMS_CONV DMS_TreeItem_GetRoot(const TreeItem* self);

TIC_CALL bool            DMS_CONV DMS_TreeItem_VisitSuppliers (const TreeItem* self, ClientHandle clientHandle, ProgressState ps, TSupplCallbackFunc func);

TIC_CALL const TreeItem* DMS_CONV DMS_TreeItem_GetSourceObject(const TreeItem* ti);

//----------------------------------------------------------------------
// C style Interface functions for TreeItemVectorType retrieval
//----------------------------------------------------------------------

// Created sets must be released with DMS_TreeItemSet_Release
TIC_CALL const TreeItemVectorType* DMS_CONV DMS_TreeItem_CreateDirectConsumerSet  (const TreeItem* self, bool expandMetaInfo);
TIC_CALL const TreeItemVectorType* DMS_CONV DMS_TreeItem_CreateCompleteSupplierSet(const TreeItem* self);
TIC_CALL const TreeItemVectorType* DMS_CONV DMS_TreeItem_CreateCompleteConsumerSet(const TreeItem* self, bool expandMetaInfo);
TIC_CALL const TreeItemVectorType* DMS_CONV DMS_TreeItem_CreateSimilarItemSet(const TreeItem* pattern, const TreeItem* searchLoc,
					bool mustMatchName, bool mustMatchDomainUnit, 
					bool mustMatchValuesUnit, bool exactValuesUnit, bool expandMetaInfo);

TIC_CALL void         DMS_CONV DMS_TreeItemSet_Release   (const TreeItemVectorType* self);
TIC_CALL UInt32       DMS_CONV DMS_TreeItemSet_GetNrItems(const TreeItemVectorType* self);           
TIC_CALL const TreeItem* DMS_CONV DMS_TreeItemSet_GetItem(const TreeItemVectorType* self, UInt32 i); 

//----------------------------------------------------------------------
// C style Interface functions for Meta Data Access / Modification
//----------------------------------------------------------------------

// TreeItem property access
TIC_CALL const TreeItem*  DMS_CONV DMS_TreeItem_GetParent(const TreeItem* self);
TIC_CALL CharPtr          DMS_CONV DMS_TreeItem_GetName  (const Object* self);    // MOVE TO RTC, RENAME
TIC_CALL IStringHandle    DMS_CONV DMS_TreeItem_GetFullNameAsIString (const TreeItem* self);// arg can be PersistentSharedObj, MOVE TO RTC, RENAME
TIC_CALL IStringHandle DMS_CONV DMS_TreeItem_PropertyValue(TreeItem* context, AbstrPropDef* propDef);

TIC_CALL const TreeItem*  DMS_CONV DMS_TreeItem_GetFirstSubItem(const TreeItem* self);
TIC_CALL const TreeItem*  DMS_CONV DMS_TreeItem_GetNextItem(const TreeItem* self);
TIC_CALL const TreeItem*  DMS_CONV DMS_TreeItem_GetFirstVisibleSubItem(const TreeItem* self);
TIC_CALL const TreeItem*  DMS_CONV DMS_TreeItem_GetNextVisibleItem(const TreeItem* self);

TIC_CALL IStringHandle DMS_CONV DMS_TreeItem_GetStoredNameAsIString(const TreeItem* self);
TIC_CALL IStringHandle DMS_CONV DMS_TreeItem_GetSourceNameAsIString(const TreeItem* self);
TIC_CALL CharPtr  DMS_CONV DMS_TreeItem_GetAssociatedFilename(const TreeItem* self); 
TIC_CALL CharPtr  DMS_CONV DMS_TreeItem_GetAssociatedFiletype(const TreeItem* self); 
TIC_CALL AbstrStorageManager* DMS_CONV DMS_TreeItem_GetStorageManager(const TreeItem* self); 
TIC_CALL const TreeItem*      DMS_CONV DMS_TreeItem_GetStorageParent (const TreeItem* self); 
TIC_CALL bool     DMS_CONV DMS_TreeItem_HasCalculator(const TreeItem* self);
TIC_CALL CharPtr  DMS_CONV DMS_TreeItem_GetExpr(const TreeItem* self);
TIC_CALL bool     DMS_CONV DMS_TreeItem_HasSubItems(const TreeItem* self);

TIC_CALL void     DMS_CONV DMS_TreeItem_SetDescr(TreeItem* self, CharPtr description);
TIC_CALL void     DMS_CONV DMS_TreeItem_SetExpr (TreeItem* self, CharPtr expression);

// TreeItem status management
TIC_CALL UInt32		 DMS_CONV TreeItem_GetProgressState(const TreeItem* self);
TIC_CALL void        DMS_CONV DMS_TreeItem_Invalidate(TreeItem* self);
TIC_CALL UInt32      DMS_CONV DMS_TreeItem_GetProgressState(const TreeItem* self);
TIC_CALL bool        DMS_CONV DMS_TreeItem_IsFailed(const TreeItem* self);

TIC_CALL TimeStamp   DMS_CONV DMS_TreeItem_GetLastChangeTS(const TreeItem* self);
TIC_CALL IStringHandle DMS_CONV DMS_TreeItem_GetFailReasonAsIString(const TreeItem* self);
TIC_CALL void        DMS_CONV DMS_TreeItem_ThrowFailReason(const TreeItem* self);
TIC_CALL void        DMS_CONV DMS_TreeItem_DoViewAction(const TreeItem* x);

TIC_CALL void        DMS_CONV DMS_TreeItem_Update(const TreeItem* self);

// Unit property access
TIC_CALL const ValueClass* DMS_CONV DMS_Unit_GetValueType(const AbstrUnit* self);

// Unit range functions
TIC_CALL void DMS_CONV DMS_NumericUnit_SetRangeAsFloat64(AbstrUnit* self, Float64 begin, Float64 end);
TIC_CALL void DMS_CONV DMS_NumericUnit_GetRangeAsFloat64(const AbstrUnit* self, Float64* begin, Float64* end);

TIC_CALL void DMS_CONV DMS_GeometricUnit_SetRangeAsDPoint(AbstrUnit* self, 
	Float64 rowBegin, Float64 colBegin, Float64 rowEnd, Float64 colEnd );
TIC_CALL void DMS_CONV DMS_GeometricUnit_GetRangeAsDPoint(const AbstrUnit* self, 
	Float64* rowBegin, Float64* colBegin, Float64* rowEnd, Float64* colEnd );

TIC_CALL void DMS_CONV DMS_GeometricUnit_SetRangeAsIPoint(AbstrUnit* self, 
	Int32 rowBegin, Int32 colBegin, Int32 rowEnd, Int32 colEnd );
TIC_CALL void DMS_CONV DMS_GeometricUnit_GetRangeAsIPoint(const AbstrUnit* self, 
	Int32* rowBegin, Int32* colBegin, Int32* rowEnd, Int32* colEnd );

TIC_CALL SizeT		          DMS_CONV DMS_Unit_GetCount(const AbstrUnit* self);
TIC_CALL const AbstrDataItem* DMS_CONV DMS_Unit_GetLabelAttr(const AbstrUnit* self);

// DataItem property access
TIC_CALL const AbstrUnit*  DMS_CONV DMS_DataItem_GetDomainUnit(const AbstrDataItem* self);
TIC_CALL const AbstrUnit*  DMS_CONV DMS_DataItem_GetValuesUnit(const AbstrDataItem* self);
TIC_CALL ValueComposition  DMS_CONV DMS_DataItem_GetValueComposition(const AbstrDataItem* self);

// Param property access
TIC_CALL const AbstrUnit*  DMS_CONV DMS_Param_GetValueUnit(const AbstrParam* self);

//----------------------------------------------------------------------
// C style Interface functions for Primary Data Access / Modification
//----------------------------------------------------------------------

#define INSTANTIATE(T) \
	TIC_CALL api_type<T>::type DMS_CONV DMS_##T##Param_GetValue(const AbstrParam* self); \
	TIC_CALL void              DMS_CONV DMS_##T##Param_SetValue(AbstrParam* self, api_type<T>::type value);

	INSTANTIATE_NUM_ELEM
	INSTANTIATE(Bool)

#undef INSTANTIATE

TIC_CALL bool DMS_CONV DMS_BoolParam_GetValue(const AbstrParam* self);
TIC_CALL void DMS_CONV DMS_BoolParam_SetValue(AbstrParam* self, bool value);

TIC_CALL Float64 DMS_CONV DMS_NumericParam_GetValueAsFloat64(const AbstrParam* self);
TIC_CALL void    DMS_CONV DMS_NumericParam_SetValueAsFloat64(AbstrParam* self, Float64 value);
TIC_CALL void    DMS_CONV DMS_StringParam_SetValue(AbstrParam* self, CharPtr value);     

// DataItem Data Access
TIC_CALL Float64 DMS_CONV DMS_NumericAttr_GetValueAsFloat64 (const AbstrDataItem* self, SizeT index);
TIC_CALL void    DMS_CONV DMS_NumericAttr_SetValueAsFloat64 (AbstrDataItem* self, SizeT index, Float64 value);
TIC_CALL Int32   DMS_CONV DMS_NumericAttr_GetValueAsInt32 (const AbstrDataItem* self, SizeT index);
TIC_CALL void    DMS_CONV DMS_NumericAttr_SetValueAsInt32 (AbstrDataItem* self, SizeT index, Int32 value);

TIC_CALL void    DMS_CONV DMS_NumericAttr_GetValuesAsFloat64Array(const AbstrDataItem* self, SizeT index, SizeT len, Float64* data);
TIC_CALL void    DMS_CONV DMS_NumericAttr_SetValuesAsFloat64Array(AbstrDataItem* self, SizeT index, SizeT len, const Float64* data);
TIC_CALL void    DMS_CONV DMS_NumericAttr_GetValuesAsInt32Array  (const AbstrDataItem* self, SizeT index, SizeT len,  Int32* data);
TIC_CALL void    DMS_CONV DMS_NumericAttr_SetValuesAsInt32Array  (AbstrDataItem* self, SizeT index, SizeT len, const  Int32* data);


TIC_CALL void   DMS_CONV DMS_DataItem_SetMemoDirty(AbstrDataItem* self, bool alsoRead);
TIC_CALL UInt32 DMS_CONV DMS_DataItem_GetNrFeatures(AbstrDataItem* self);
TIC_CALL bool   DMS_CONV DMS_DataItem_IsInMem(AbstrDataItem* self);
TIC_CALL Int32  DMS_CONV DMS_DataItem_GetLockCount(AbstrDataItem* self);
TIC_CALL DataReadLock*  DMS_CONV DMS_DataReadLock_Create(const AbstrDataItem* self, bool mustUpdateCertain);
TIC_CALL void           DMS_CONV DMS_DataReadLock_Release(DataReadLock* self);

// Dense Data Item (attr) Access
TIC_CALL UInt32  DMS_CONV DMS_UInt32Attr_GetValue (const AbstrDataItem* self, SizeT index);										
TIC_CALL Int32   DMS_CONV DMS_Int32Attr_GetValue  (const AbstrDataItem* self, SizeT index);											
TIC_CALL UInt16  DMS_CONV DMS_UInt16Attr_GetValue (const AbstrDataItem* self, SizeT index);										
TIC_CALL Int16   DMS_CONV DMS_Int16Attr_GetValue  (const AbstrDataItem* self, SizeT index);										
TIC_CALL UInt8   DMS_CONV DMS_UInt8Attr_GetValue  (const AbstrDataItem* self, SizeT index);										
TIC_CALL Int8    DMS_CONV DMS_Int8Attr_GetValue   (const AbstrDataItem* self, SizeT index);										
TIC_CALL Float32 DMS_CONV DMS_Float32Attr_GetValue(const AbstrDataItem* self, SizeT index);									
TIC_CALL Float64 DMS_CONV DMS_Float64Attr_GetValue(const AbstrDataItem* self, SizeT index);									
TIC_CALL bool    DMS_CONV DMS_BoolAttr_GetValue   (const AbstrDataItem* self, SizeT index);
TIC_CALL bool    DMS_CONV DMS_AnyDataItem_GetValueAsCharArray    (const AbstrDataItem* self, SizeT index, char* clientBuffer, UInt32 clientBufferLen);
TIC_CALL UInt32  DMS_CONV DMS_AnyDataItem_GetValueAsCharArraySize(const AbstrDataItem* self, SizeT index);

// Dense Data Item (attr) bulk Access
TIC_CALL void		DMS_CONV DMS_UInt32Attr_GetValueArray (const AbstrDataItem* self, SizeT firstRow, SizeT len, UInt32* clientBuffer);
TIC_CALL void		DMS_CONV DMS_Int32Attr_GetValueArray  (const AbstrDataItem* self, SizeT firstRow, SizeT len, Int32* clientBuffer);
TIC_CALL void		DMS_CONV DMS_UInt16Attr_GetValueArray (const AbstrDataItem* self, SizeT firstRow, SizeT len, UInt16* clientBuffer);
TIC_CALL void		DMS_CONV DMS_Int16Attr_GetValueArray  (const AbstrDataItem* self, SizeT firstRow, SizeT len, Int16* clientBuffer);
TIC_CALL void		DMS_CONV DMS_UInt8Attr_GetValueArray  (const AbstrDataItem* self, SizeT firstRow, SizeT len, UInt8* clientBuffer);
TIC_CALL void		DMS_CONV DMS_Int8Attr_GetValueArray   (const AbstrDataItem* self, SizeT firstRow, SizeT len, Int8* clientBuffer);	
TIC_CALL void		DMS_CONV DMS_Float32Attr_GetValueArray(const AbstrDataItem* self, SizeT firstRow, SizeT len, Float32* clientBuffer);
TIC_CALL void		DMS_CONV DMS_Float64Attr_GetValueArray(const AbstrDataItem* self, SizeT firstRow, SizeT len, Float64* clientBuffer);

TIC_CALL void		DMS_CONV DMS_BoolAttr_GetValueArray   (const AbstrDataItem* self, SizeT firstRow, SizeT len, bool* clientBuffer);		

// Dense Data Item (attr) Modification
TIC_CALL void		DMS_CONV DMS_UInt32Attr_SetValue (AbstrDataItem* self, UInt32 index, UInt32  value);					
TIC_CALL void		DMS_CONV DMS_Int32Attr_SetValue  (AbstrDataItem* self, UInt32 index, Int32   value);						
TIC_CALL void		DMS_CONV DMS_UInt16Attr_SetValue (AbstrDataItem* self, UInt32 index, UInt16  value);					
TIC_CALL void		DMS_CONV DMS_Int16Attr_SetValue  (AbstrDataItem* self, UInt32 index, Int16   value);						
TIC_CALL void		DMS_CONV DMS_UInt8Attr_SetValue  (AbstrDataItem* self, UInt32 index, UInt8   value);						
TIC_CALL void		DMS_CONV DMS_Int8Attr_SetValue   (AbstrDataItem* self, UInt32 index, Int8    value);						
TIC_CALL void		DMS_CONV DMS_Float32Attr_SetValue(AbstrDataItem* self, UInt32 index, Float32 value);					
TIC_CALL void		DMS_CONV DMS_Float64Attr_SetValue(AbstrDataItem* self, UInt32 index, Float64 value);					
TIC_CALL void		DMS_CONV DMS_BoolAttr_SetValue   (AbstrDataItem* self, UInt32 index, bool    value);
TIC_CALL void       DMS_CONV DMS_AnyDataItem_SetValueAsCharArray(AbstrDataItem* self, UInt32 index, CharPtr clientBuffer);

// Dense Data Item (attr) bulk Modification
TIC_CALL void		DMS_CONV DMS_UInt32Attr_SetValueArray (AbstrDataItem* self, UInt32 firstRow, UInt32 len, const UInt32* clientBuffer);	
TIC_CALL void		DMS_CONV DMS_Int32Attr_SetValueArray  (AbstrDataItem* self, UInt32 firstRow, UInt32 len, const Int32* clientBuffer);	
TIC_CALL void		DMS_CONV DMS_UInt16Attr_SetValueArray (AbstrDataItem* self, UInt32 firstRow, UInt32 len, const UInt16* clientBuffer);	
TIC_CALL void		DMS_CONV DMS_Int16Attr_SetValueArray  (AbstrDataItem* self, UInt32 firstRow, UInt32 len, const Int16* clientBuffer);	
TIC_CALL void		DMS_CONV DMS_UInt8Attr_SetValueArray  (AbstrDataItem* self, UInt32 firstRow, UInt32 len, const UInt8* clientBuffer);	
TIC_CALL void		DMS_CONV DMS_Int8Attr_SetValueArray   (AbstrDataItem* self, UInt32 firstRow, UInt32 len, const Int8* clientBuffer);		
TIC_CALL void		DMS_CONV DMS_Float32Attr_SetValueArray(AbstrDataItem* self, UInt32 firstRow, UInt32 len, const Float32* clientBuffer);
TIC_CALL void		DMS_CONV DMS_Float64Attr_SetValueArray(AbstrDataItem* self, UInt32 firstRow, UInt32 len, const Float64* clientBuffer);
TIC_CALL void		DMS_CONV DMS_BoolAttr_SetValueArray   (AbstrDataItem* self, UInt32 firstRow, UInt32 len, const bool* clientBuffer);

using ConstAbstrDataItemPtr = const AbstrDataItem*;
struct TableColumnSpec;

TIC_CALL void DMS_CONV DMS_Table_Dump(OutStreamBuff* out, UInt32 nrDataItems, const ConstAbstrDataItemPtr* dataItemArray);
TIC_CALL void DMS_CONV Table_Dump(OutStreamBuff* out, const TableColumnSpec* columnSpecPtr, const TableColumnSpec* columnSpecEnd, const SizeT* recNos, const SizeT* recNoEnd);

/********** Stored PropDef creation                        **********/

TIC_CALL AbstrPropDef* DMS_CONV DMS_StoredStringPropDef_Create(CharPtr name);
TIC_CALL void          DMS_CONV DMS_StoredStringPropDef_Destroy(AbstrPropDef* apd);

TIC_CALL void DMS_CONV DMS_RegisterStateChangeNotification(TStateChangeNotificationFunc fcb, ClientHandle clientHandle);
TIC_CALL void DMS_CONV DMS_ReleaseStateChangeNotification (TStateChangeNotificationFunc fcb, ClientHandle clientHandle);

TIC_CALL void DMS_CONV DMS_TreeItem_RegisterStateChangeNotification(TStateChangeNotificationFunc fcb, const TreeItem*, ClientHandle clientHandle);
TIC_CALL void DMS_CONV DMS_TreeItem_ReleaseStateChangeNotification (TStateChangeNotificationFunc fcb, const TreeItem*, ClientHandle clientHandle);
TIC_CALL void DMS_CONV DMS_TreeItem_IncInterestCount(const TreeItem* self);
TIC_CALL void DMS_CONV DMS_TreeItem_DecInterestCount(const TreeItem* self);

TIC_CALL void DMS_CONV DMS_Tic_Load();

} // end extern "C"

TIC_CALL bool XML_MetaInfoRef(const TreeItem* self, OutStreamBase* xmlOutStrPtr);
TIC_CALL bool TreeItem_XML_DumpGeneral(const TreeItem* self, OutStreamBase* xmlOutStrPtr);
TIC_CALL void TreeItem_XML_DumpSourceDescription(const TreeItem* self, SourceDescrMode mode, OutStreamBase* xmlOutStrPtr);
TIC_CALL void TreeItem_XML_ConvertAndDumpDatasetProperties(const TreeItem* self, const prop_tables& dataset_properties, OutStreamBase* xmlOutStrPtr);
#endif // __TIC_INTERFACE_H
