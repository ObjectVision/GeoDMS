//<HEADER>
(*
GeoDmsGui.exe
Copyright (C) Object Vision BV.
http://www.objectvision.nl/geodms/

This is open source software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2
(the License) as published by the Free Software Foundation.

Have a look at our web-site for licensing conditions:
http://www.objectvision.nl/geodms/software/license-and-disclaimer

You can use, redistribute and/or modify this library source code file
provided that this entire header notice is preserved.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*)
//</HEADER>

unit uDMSInterface;

interface

uses uDmsInterfaceTypes;

{*******************************************************************************
                                 CONSTANTS
*******************************************************************************}

const

CLC1_DLL =  'Clc1.dll';
CLC2_DLL =  'Clc2.dll';
GEO_DLL  =  'Geo.dll';
RTC_DLL  =  'Rtc.dll';
STG_DLL  =  'Stg.dll';
STX_DLL  =  'Stx.dll';
TIC_DLL  =  'Tic.dll';
SYM_DLL  =  'Sym.dll';
SHV_DLL  =  'Shv.dll';

{*******************************************************************************
                        Linker functions

   Call the following functions from your form initialization to make sure that
   the corresponding modules are linked
*******************************************************************************}

procedure DMS_Tic_Load;                                                            cdecl; external TIC_DLL; // From TicInterface.h:
procedure DMS_Clc1_Load;                                                           cdecl; external CLC1_DLL; // From ClcInterface.h: TParseResult && operator
procedure DMS_Clc2_Load;                                                           cdecl; external CLC2_DLL; // From ClcInterface.h: TParseResult && operator
procedure DMS_Geo_Load;                                                            cdecl; external GEO_DLL; // From ClcInterface.h: TParseResult && operator
procedure DMS_Stg_Load;                                                            cdecl; external STG_DLL; // Storages for gtf, dbf, cfs, xdb, asc, e00, bmp, pal, odbc, shp
procedure DMS_Shv_Load;                                                            cdecl; external SHV_DLL;
procedure DMS_Terminate;                                                           cdecl; external RTC_DLL;

function DMS_GetVersion: PMsgChar;                                                 cdecl; external RTC_DLL; // From RtcInterface.h
function DMS_GetVersionNumber: Float64;                                            cdecl; external RTC_DLL; // From RtcInterface.h

procedure DMS_VisitVersionComponents(clientHandle: TClientHandle; callBack: TVersionComponentCallbackFunc); cdecl; external RTC_DLL;

{*******************************************************************************
                  Diagnostics functions
*******************************************************************************}

// GetLastError
function  DMS_GetLastErrorMsg : PMsgChar;                                          cdecl; external RTC_DLL;
procedure DMS_ReportError(msg: PMsgChar);                                          cdecl; external RTC_DLL;
function  RTC_GetRegDWord(regID:UInt32): UInt32;                                   cdecl; external RTC_DLL;
function  RTC_GetRegStatusFlags: UInt32;                                           cdecl; external RTC_DLL;
procedure RTC_SetRegDWord(regID:UInt32; value: UInt32);                            cdecl; external RTC_DLL;
function RTC_ParseRegStatusFlag(argv: PSourceChar): Boolean;                       cdecl; external RTC_DLL;

{*******************************************************************************
            C style Interface functions for Exception handling
*******************************************************************************}

// Trace-into Callback registration
procedure DMS_RegisterMsgCallback(f: TMsgCallbackFunc; clientHandle: TClientHandle);     cdecl; external RTC_DLL;
procedure DMS_ReleaseMsgCallback (f: TMsgCallbackFunc; clientHandle: TClientHandle);     cdecl; external RTC_DLL;
procedure DMS_SetGlobalCppExceptionTranslator(trFunc: TCppExceptionTranslator);    cdecl; external RTC_DLL;
procedure DMS_SetGlobalSeTranslator (trFunc: TCppExceptionTranslator);    cdecl; external RTC_DLL;

// TCoalesceHeapFunc
procedure DMS_SetCoalesceHeapFunc(f: TCoalesceHeapFunc);                           cdecl; external RTC_DLL;

// suspend / resume
function  DMS_Actor_MustSuspend: Boolean;                                              cdecl; external RTC_DLL;
function  DMS_Actor_DidSuspend:  Boolean;                                              cdecl; external RTC_DLL;
procedure DMS_Actor_Resume;                                                            cdecl; external RTC_DLL;
function  DMS_HasWaitingMessages: Boolean;                                             cdecl; external RTC_DLL;

// call for reduction of resources before calling an external process.
procedure DMS_RegisterReduceResourcesFunc(fcb: TReduceResources; clientHandle: TClientHandle);cdecl; external RTC_DLL;
procedure DMS_ReleaseReduceResourcesFunc(fcb: TReduceResources; clientHandle: TClientHandle); cdecl; external RTC_DLL;
function  DMS_ReduceRecources(): bool;                                                 cdecl; external RTC_DLL;
procedure DMS_FreeResources;                                                           cdecl; external RTC_DLL;

// call for state change notifications when monitoring the update process
procedure DMS_RegisterStateChangeNotification(fcb: TStateChangeNotificationFunc; clientHandle: TClientHandle); cdecl; external TIC_DLL;
procedure DMS_ReleaseStateChangeNotification (fcb: TStateChangeNotificationFunc; clientHandle: TClientHandle); cdecl; external TIC_DLL;

// call for state change notifications of a specific TreeItem
procedure DMS_TreeItem_RegisterStateChangeNotification(fcb: TStateChangeNotificationFunc; ti: TTreeItem; clientHandle: TClientHandle); cdecl; external TIC_DLL;
procedure DMS_TreeItem_ReleaseStateChangeNotification (fcb: TStateChangeNotificationFunc; ti: TTreeItem; clientHandle: TClientHandle); cdecl; external TIC_DLL;

// call for Context Push/Pop  notifications
procedure DMS_SetContextNotification(cnFunc: TContextNotification; clientHandle: TClientHandle); cdecl; external RTC_DLL;
procedure DMS_NotifyCurrentTargetCount(); cdecl; external RTC_DLL;


// DebugLog handling
function  DBG_DebugLog_Open(fileName: PFileChar): CDebugLogHandle;                 cdecl; external RTC_DLL;
procedure DBG_DebugLog_Close(hnd: CDebugLogHandle);                                cdecl; external RTC_DLL;

// Handling of a local DebugContext
function  DBG_DebugContext_Create(className: PItemChar; methodName: PItemChar): CDebugContextHandle; cdecl; external RTC_DLL;
procedure DBG_DebugContext_Destroy(this: CDebugContextHandle);                     cdecl; external RTC_DLL;

{*******************************************************************************
                  Creating and reading OutStreamBuff's
*******************************************************************************}

function DMS_VectOutStreamBuff_Create:                                 TOutStreamBuff; cdecl; external RTC_DLL;
function DMS_FileOutStreamBuff_Create(fileName: FileString; isAscii: Boolean): TOutStreamBuff; cdecl; external RTC_DLL;
function DMS_OutStreamBuff_CurrPos(this: TOutStreamBuff): StreamSize;                  cdecl; external RTC_DLL;
procedure DMS_OutStreamBuff_Destroy(this: TOutStreamBuff);                             cdecl; external RTC_DLL;
procedure DMS_OutStreamBuff_WriteBytes(this: TOutStreamBuff; source: PAnsiChar; sourceLen: StreamSize); cdecl; external RTC_DLL;
procedure DMS_OutStreamBuff_WriteChars(this: TOutStreamBuff; source: PSourceChar);                      cdecl; external RTC_DLL;

// note: apply the following function only on VectOutstreamBuffs
// note: the returning string might not be null-terminated; use CurrPos as length-indicator to avoid 'GPF'
function DMS_VectOutStreamBuff_GetData(this: TOutStreamBuff): PSourceChar;             cdecl; external RTC_DLL;
procedure DMS_Table_Dump(out: TOutStreamBuff; nrDataItems: UInt32; dataItemArray: TAbstrDataItemPtr); cdecl; external TIC_DLL;

{*******************************************************************************
   Streaming meta-data & Schema data to TOutStreamBuff by using a TXMLOutStreamHandle
*******************************************************************************}

const ST_XML: Cardinal  = 0;
const ST_DMS: Cardinal  = 1;
const ST_HTM: Cardinal  = 2;

function  XML_OutStream_Create(outBuff: TOutStreamBuff; streamType: Cardinal; docType: PItemChar; primaryPropDef: CPropDef)
          : TXMLOutStreamHandle;                                                cdecl; external RTC_DLL;
procedure XML_OutStream_Destroy(self: TXMLOutStreamHandle);                     cdecl; external RTC_DLL;
procedure XML_OutStream_WriteText(self: TXMLOutStreamHandle; txt: PSourceChar); cdecl; external RTC_DLL;

procedure XML_ReportPropDef(this: TXMLOutStreamHandle; pd: CPropDef);           cdecl; external RTC_DLL;
procedure XML_ReportSchema(this: TXMLOutStreamHandle;
                  cls: CRuntimeClass; withSubclases: Boolean);                  cdecl; external RTC_DLL;

procedure DMS_TreeItem_XML_Dump(this: TTreeItem; xmlStr: TXMLOutStreamHandle);  cdecl; external TIC_DLL; // From TicInterface.h
function  DMS_TreeItem_XML_DumpGeneral (this: TTreeItem; xmlStr: TXMLOutStreamHandle; viewHidden: Boolean): Boolean;  cdecl; external TIC_DLL; // From TicInterface.h
function  DMS_TreeItem_XML_DumpAllProps(this: TTreeItem; xmlStr: TXMLOutStreamHandle; viewHidden: Boolean): Boolean;  cdecl; external TIC_DLL; // From TicInterface.h
procedure DMS_TreeItem_XML_DumpExplore (this: TTreeItem; xmlStr: TXMLOutStreamHandle; viewHidden: Boolean);  cdecl; external TIC_DLL; // From TicInterface.h
function  DMS_TreeItem_Dump(this: TTreeItem; fileName: PFileChar): Boolean;     cdecl; external TIC_DLL; // From TicInterface.h

procedure XML_ReportOperator(xmlStr: TXMLOutStreamHandle; oper: COperator);     cdecl; external CLC1_DLL; // From ClcInterface.h
procedure XML_ReportOperGroup(xmlStr: TXMLOutStreamHandle; gr: COperGroup);     cdecl; external CLC1_DLL; // From ClcInterface.h
procedure XML_ReportAllOperGroups(xmlStr: TXMLOutStreamHandle);                 cdecl; external CLC1_DLL; // From ClcInterface.h

{*******************************************************************************
      C style Interface functions for lement ValueType id retrieval
*******************************************************************************}

function DMS_ValueType_GetID  (vc: TValueType): TValueTypeId;                   cdecl; external RTC_DLL;
function DMS_ValueType_GetName(vc: TValueType): PItemChar;                      cdecl; external RTC_DLL;

// MK : Introduced wrapper because sizedifference between Delphi
//      and C++ for enums.
function __DMS_GetValueType(vid: Cardinal): TValueType;                         cdecl; external RTC_DLL name 'DMS_GetValueType';
function DMS_GetValueType(vid: TValueTypeId): TValueType;

function DMS_ValueType_GetUndefinedValueAsFloat64(const vc: TValueType): Float64; cdecl; external RTC_DLL;
function DMS_ValueType_GetSize(const vc: TValueType): Int32;                      cdecl; external RTC_DLL;
function DMS_ValueType_NrDims(const vc: TValueType): UInt8;                       cdecl; external RTC_DLL;
function DMS_ValueType_IsNumeric(const vc: TValueType): boolean;                  cdecl; external RTC_DLL;
function DMS_ValueType_IsIntegral(const vc: TValueType): boolean;                 cdecl; external RTC_DLL;
function DMS_ValueType_IsSigned(const vc: TValueType): boolean;                   cdecl; external RTC_DLL;
function DMS_ValueType_IsSequence(const vc: TValueType): boolean;                 cdecl; external RTC_DLL;
function DMS_ValueType_GetCrdClass  (const vc: TValueType): TValueType;           cdecl; external RTC_DLL;

{*******************************************************************************
      C style Interface functions for opening a DMS datapool
*******************************************************************************}

function  DMS_CreateTreeFromConfiguration(configFileName :  PFileChar): TTreeItem;  cdecl; external STX_DLL;
function  DMS_AppendTreeFromConfiguration(configFileName :  PFileChar;
                                    TreeRoot       :  TTreeItem): TTreeItem;    cdecl; external STX_DLL;
function DMS_IsConfigDirty(treeRoot: TTreeItem): Boolean;                       cdecl; external TIC_DLL;
function DMS_ProcessADMS(context: TTreeItem; url: PFileChar): IString;          cdecl; external STX_DLL;
function DMS_ProcessPostData(ti: TTreeItem; postData: PFileChar; dataSize: UInt32): Boolean; cdecl; external STX_DLL;
function DMS_ReportChangedFiles(updateFileTimes: Boolean): IString;             cdecl; external RTC_DLL;

{*******************************************************************************
      C style Interface functions for storage
*******************************************************************************}

procedure DMS_MakeDirsForFile(fileName: PFileChar);                             cdecl; external RTC_DLL;
function DMS_TreeItem_StorageDoesExist(storageHolder   : TTreeItem;
                                       storageFileName : PFileChar;
                                       storageTypeName : PItemChar): Boolean; cdecl; external TIC_DLL;

function DMS_TreeItem_GetStorageManager(this: TTreeItem): TAbstrStorageManager; cdecl; external TIC_DLL;
function DMS_TreeItem_GetStorageParent (this: TTreeItem): TTreeItem;            cdecl; external TIC_DLL;

function  DMS_StorageManager_GetType  (StgManager: TAbstrStorageManager): PItemChar; cdecl; external TIC_DLL;
function  DMS_StorageManager_GetName  (StgManager: TAbstrStorageManager): PFileChar; cdecl; external TIC_DLL;

procedure DMS_TreeItem_SetStorageManager(this: TTreeItem; storagename: PFileChar; storagetype: PItemChar;
                                         readOnly: Boolean);             cdecl; external TIC_DLL;


function  DMS_TreeItem_HasStorage(this: TTreeItem): Boolean;                    cdecl; external TIC_DLL;

procedure DMS_TreeItem_IncInterestCount(this: TTreeItem);                       cdecl; external TIC_DLL;
procedure DMS_TreeItem_DecInterestCount(this: TTreeItem);                       cdecl; external TIC_DLL;
function  DMS_TreeItem_GetInterestCount(this: TTreeItem): UInt32;               cdecl; external TIC_DLL;


{*******************************************************************************
               C style Create interface functions for Item construction
*******************************************************************************}

function DMS_CreateTree     (name: PItemChar):                    TTreeItem;        cdecl; external TIC_DLL;
function DMS_CreateTreeItem (context: TTreeItem; name: PItemChar): TTreeItem;       cdecl; external TIC_DLL;

function DMS_CreateUnit(context: TTreeItem; name: PItemChar;
                  uc : TUnitClass): TAbstrUnit;                                 cdecl; external TIC_DLL;

function DMS_CreateParam(context: TTreeItem; name: PItemChar;
                         ValueUnit: TAbstrUnit; vc: TValueComposition): TAbstrParam;                   cdecl; external TIC_DLL;

function DMS_CreateDataItem(context: TTreeItem; name: PItemChar;
                domainUnit: TAbstrUnit; valueUnit: TAbstrUnit; vc: TValueComposition): TAbstrDataItem; cdecl; external TIC_DLL;

{*******************************************************************************
               C style Interface function for treeitem copy
*******************************************************************************}

function DMS_TreeItem_Copy(dest, source: TTreeItem; name: PItemChar): TTreeItem;    cdecl; external TIC_DLL;
function DMS_TreeItem_IsEndogenous        (x: TTreeItem): Boolean;              cdecl; external TIC_DLL;
function DMS_TreeItem_IsHidden            (x: TTreeItem): Boolean;              cdecl; external TIC_DLL;
procedure DMS_TreeItem_SetIsHidden        (x: TTreeItem; v: Boolean);           cdecl; external TIC_DLL;
function DMS_TreeItem_InHidden            (x: TTreeItem): Boolean;              cdecl; external TIC_DLL;
function DMS_TreeItem_IsTemplate          (x: TTreeItem): Boolean;              cdecl; external TIC_DLL;
function DMS_TreeItem_InTemplate          (x: TTreeItem): Boolean;              cdecl; external TIC_DLL;
function DMS_TreeItem_GetCreator          (x: TTreeItem): TTreeItem;            cdecl; external TIC_DLL;
function DMS_TreeItem_GetTemplInstantiator(x: TTreeItem): TTreeItem;            cdecl; external TIC_DLL;
function DMS_TreeItem_GetTemplSource      (x: TTreeItem): TTreeItem;            cdecl; external TIC_DLL;
function DMS_TreeItem_GetTemplSourceItem  (x, t: TTreeItem): TTreeItem;         cdecl; external TIC_DLL;

{*******************************************************************************
            C style Interface functions for class id retrieval
*******************************************************************************}

function DMS_GetRootClass: CRuntimeClass;                                       cdecl; external RTC_DLL;
function DMS_Class_GetStaticClass: CRuntimeClass;                               cdecl; external RTC_DLL;
function DMS_PropDef_GetStaticClass: CRuntimeClass;                             cdecl; external RTC_DLL;

function DMS_UInt32Unit_GetStaticClass: TUnitClass;                             cdecl; external TIC_DLL;
function DMS_UInt8Unit_GetStaticClass: TUnitClass;                              cdecl; external TIC_DLL;
function DMS_VoidUnit_GetStaticClass: TUnitClass;                               cdecl; external TIC_DLL;
function DMS_StringUnit_GetStaticClass: TUnitClass;                             cdecl; external TIC_DLL;
function DMS_AbstrUnit_GetStaticClass: CRuntimeClass;                           cdecl; external TIC_DLL;
function DMS_UnitClass_Find(vc: TValueType): TUnitClass;                        cdecl; external TIC_DLL;

function DMS_AbstrParam_GetStaticClass: CRuntimeClass;                          cdecl; external TIC_DLL;
function DMS_GetDefaultUnit(ucThis: TUnitClass): TAbstrUnit;                    cdecl; external TIC_DLL;

function DMS_AbstrDataItem_GetStaticClass: CRuntimeClass;                       cdecl; external TIC_DLL;
function DMS_AbstrDataItem_DisplayValue(adu: TAbstrDataItem; index: UInt32; useMetric: Boolean): PSourceChar; cdecl; external TIC_DLL;
function DMS_AbstrDataItem_HasVoidDomainGuarantee(di: TAbstrDataItem): boolean; cdecl; external TIC_DLL;
function DMS_DataItemClass_Find(valuesUnitClass: TUnitClass; vc: TValueComposition): TDataItemClass;   cdecl; external TIC_DLL;
function DMS_TreeItem_GetStaticClass: CRuntimeClass;                            cdecl; external TIC_DLL;

function DMS_AbstrStorageManager_GetStaticClass: CRuntimeClass;                 cdecl; external TIC_DLL;

{*******************************************************************************
   C style Interface functions for Runtime Type Info (RTTI), Dynamic Casting,
   AddRef / Release (IUnknown)
*******************************************************************************}

function DMS_TreeItem_QueryInterface(this: TTreeItem; requiredClass: CRuntimeClass):
                                                                         TTreeItem; cdecl; external TIC_DLL;
function DMS_TreeItem_GetDynamicClass(this: TTreeItem): CRuntimeClass;              cdecl; external TIC_DLL;

procedure DMS_TreeItem_AddRef(this: TTreeItem);                                     cdecl; external TIC_DLL;
procedure DMS_TreeItem_Release(this: TTreeItem);                                    cdecl; external TIC_DLL;

{*******************************************************************************
            C style Interface functions for CPropDef access
*******************************************************************************}

function DMS_Class_GetName(self: CRuntimeClass): PItemChar                      cdecl; external RTC_DLL;
function DMS_Class_FindPropDef(cls: CRuntimeClass; name: PItemChar): CPropDef;     cdecl; external RTC_DLL;

function DMS_Class_GetLastPropDef(self: CRuntimeClass): CPropDef;               cdecl; external RTC_DLL;
function DMS_PropDef_GetName(self: CPropDef): PItemChar;                            cdecl; external RTC_DLL;
function DMS_PropDef_CreateValue(self: CPropDef): TAbstrValue;                  cdecl; external RTC_DLL;
function DMS_PropDef_GetPrevPropDef(self: CPropDef): CPropDef;                  cdecl; external RTC_DLL;

function DMS_TreeItem_FindPropDef(ti: TTreeItem; propName: ItemString): CPropDef; // zie implementatie onder (voorlopig); TODO: naar DMS

{********** Stored PropDef creation                        **********}

function  DMS_StoredStringPropDef_Create(name: PItemChar): CPropDef;     cdecl; external TIC_DLL;
procedure DMS_StoredStringPropDef_Destroy(apd: CPropDef);                cdecl; external TIC_DLL;

{*******************************************************************************
            Property Definition (non-polymorphic)
*******************************************************************************}

procedure DMS_PropDef_GetValue(self: CPropDef; me: TTreeItem; value: TAbstrValue);   cdecl; external RTC_DLL;
procedure DMS_PropDef_SetValue(self: CPropDef; me: TTreeItem; value: TAbstrValue);   cdecl; external RTC_DLL;
procedure DMS_PropDef_SetValueAsCharArray(self: CPropDef; me: TTreeItem; value: PSourceChar);   cdecl; external RTC_DLL;


{*******************************************************************************
            Class Inheritance Definition
*******************************************************************************}

function DMS_Class_GetBaseClass(self: CRuntimeClass): CRuntimeClass;            cdecl; external RTC_DLL;
function DMS_Class_GetLastSubClass(self: CRuntimeClass): CRuntimeClass;         cdecl; external RTC_DLL;
function DMS_Class_GetPrevSubClass(self: CRuntimeClass): CRuntimeClass;         cdecl; external RTC_DLL;

{*******************************************************************************
            C style Interface functions for destruction
*******************************************************************************}

function  DMS_TreeItem_IsCacheItem(this: TTreeItem): boolean;                   cdecl; external TIC_DLL;
function  DMS_TreeItem_GetAutoDeleteState(this: TTreeItem): boolean;            cdecl; external TIC_DLL;
procedure DMS_TreeItem_SetAutoDelete(this: TTreeItem);                          cdecl; external TIC_DLL;

{*******************************************************************************
            C style Interface functions for TTreeItem retrieval
*******************************************************************************}

function  DMS_TreeItem_GetItem(current: TTreeItem; path: PItemChar;
                               requiredClass: CRuntimeClass): TTreeItem;        cdecl; external TIC_DLL;
function  DMS_TreeItem_GetBestItemAndUnfoundPart(current: TTreeItem; path: PItemChar; unfoundPart: PIString): TTreeItem; cdecl; external TIC_DLL;
function  DMS_TreeItem_GetRoot(this: TTreeItem): TTreeItem;                     cdecl; external TIC_DLL;
function  DMS_TreeItem_GetParent(this: TTreeItem): TTreeItem;                   cdecl; external TIC_DLL;
function  DMS_TreeItem_GetNextSupplier(this: TTreeItem; dwStatePtr: PUInt32): TTreeItem; cdecl; external TIC_DLL;
function  DMS_TreeItem_VisitSuppliers(this: TTreeItem; clientHandle: TClientHandle; ps: TUpdateState; func: TSupplCallbackFunc): Boolean; cdecl; external TIC_DLL;
function  DMS_TreeItem_GetSourceObject(const this: TTreeItem): TTreeItem;       cdecl; external TIC_DLL;

function  DMS_DataItem_VisitClassBreakCandidates(context: TAbstrDataItem; callback: TSupplCallbackFunc; clientHandle: TClientHandle): UInt32; cdecl; external TIC_DLL;
function  DMS_DomainUnit_VisitPaletteCandidates (classDomain: TAbstrUnit; callback: TSupplCallbackFunc; clientHandle: TClientHandle): UInt32; cdecl; external TIC_DLL;

{*******************************************************************************
         C style Interface functions for TTreeItemSet retrieval
*******************************************************************************}

// !!!!! Don't forget to release the return sets
function DMS_TreeItem_CreateDirectSupplierSet  (this : TTreeItem): TTreeItemSet;     cdecl; external TIC_DLL;
function DMS_TreeItem_CreateDirectConsumerSet  (this: TTreeItem; expandMetaInfo: Boolean): TTreeItemSet;      cdecl; external TIC_DLL;
function DMS_TreeItem_CreateCompleteSupplierSet(this: TTreeItem): TTreeItemSet;      cdecl; external TIC_DLL;
function DMS_TreeItem_CreateCompleteConsumerSet(this:  TTreeItem; expandMetaInfo: Boolean): TTreeItemSet;     cdecl; external TIC_DLL;
function DMS_TreeItem_CreateSimilarItemSet(pattern, searchLoc:  TTreeItem;
              mustMatchName, mustMatchDomainUnit, mustMatchValuesUnit, exactValuesUnit, expandMetaInfo: Boolean): TTreeItemSet;     cdecl; external TIC_DLL;

procedure DMS_TreeItemSet_Release(this: TTreeItemSet);                              cdecl; external TIC_DLL;
function DMS_TreeItemSet_GetNrItems(this: TTreeItemSet): Integer;                   cdecl; external TIC_DLL;
function DMS_TreeItemSet_GetItem(this: TTreeItemSet; i: Integer): TTreeItem;         cdecl; external TIC_DLL;

{*******************************************************************************
         C style Interface functions for Metadata retrieval
*******************************************************************************}

// TTreeItem property acces
function DMS_TreeItem_PropertyValue(this: TTreeItem; pd: CPropDef ): IString;   cdecl; external TIC_DLL;
function DMS_TreeItem_GetName(this: TTreeItem ): PItemChar;                     cdecl; external TIC_DLL;
function DMS_TreeItem_GetSourceNameAsIString(this: TTreeItem): IString;         cdecl; external TIC_DLL;
function DMS_TreeItem_GetAssociatedFilename(this: TTreeItem): PFileChar;        cdecl; external TIC_DLL;
function DMS_TreeItem_GetAssociatedFiletype(this: TTreeItem): PItemChar;        cdecl; external TIC_DLL;

function DMS_TreeItem_HasCalculator(this: TTreeItem): Boolean;                  cdecl; external TIC_DLL;
function DMS_TreeItem_GetExpr(this: TTreeItem): PItemChar;                      cdecl; external TIC_DLL;
function DMS_TreeItem_HasSubItems(this: TTreeItem): Boolean;                    cdecl; external TIC_DLL;
function DMS_TreeItem_GetFirstSubItem(this: TTreeItem): TTreeItem;              cdecl; external TIC_DLL;
function DMS_TreeItem_GetLastSubItem(this: TTreeItem): TTreeItem;               cdecl; external TIC_DLL;
function DMS_TreeItem_GetNextItem(this: TTreeItem): TTreeItem;                  cdecl; external TIC_DLL;
function DMS_TreeItem_GetFirstVisibleSubItem(this: TTreeItem): TTreeItem;       cdecl; external TIC_DLL;
function DMS_TreeItem_GetNextVisibleItem(this: TTreeItem): TTreeItem;           cdecl; external TIC_DLL;
function DMS_TreeItem_GetErrorSource(src: TTreeItem; unfoundPart: PIString ): TTreeItem; cdecl; external TIC_DLL;

// TTreeItem property modification
procedure DMS_TreeItem_SetDescr(this: TTreeItem; description: PSourceChar);     cdecl; external TIC_DLL;
procedure DMS_TreeItem_SetExpr(this: TTreeItem; expression: PItemChar);         cdecl; external TIC_DLL;

// TTreeItem status management
procedure DMS_TreeItem_Update(this: TTreeItem);                                 cdecl; external TIC_DLL;
function  DMS_TreeItem_GetProgressState(this: TTreeItem): TUpdateState;         cdecl; external TIC_DLL;
function  DMS_TreeItem_IsFailed        (this: TTreeItem): Boolean;              cdecl; external TIC_DLL;
function  DMS_TreeItem_IsMetaFailed    (this: TTreeItem): Boolean;              cdecl; external TIC_DLL;
function  DMS_TreeItem_IsDataFailed    (this: TTreeItem): Boolean;              cdecl; external TIC_DLL;
function  DMS_TreeItem_GetLastChangeTS(this: TTreeItem): TTimeStamp;            cdecl; external TIC_DLL;
function  DMS_TreeItem_GetFailReasonAsIString(this: TTreeItem): IString;        cdecl; external TIC_DLL;
procedure DMS_TreeItem_ThrowFailReason(this: TTreeItem);                        cdecl; external TIC_DLL;
procedure DMS_TreeItem_Invalidate(this: TTreeItem);                             cdecl; external TIC_DLL;

procedure DMS_TreeItem_DoViewAction(this: TTreeItem);                           cdecl; external TIC_DLL;

// Unit property access
function DMS_Unit_GetNrDataItemsIn(this: TAbstrUnit): Integer;                  cdecl; external TIC_DLL;
function DMS_Unit_GetDataItemIn(this: TAbstrUnit; i: Integer): TAbstrDataItem;  cdecl; external TIC_DLL;
function DMS_Unit_GetNrDataItemsOut(this: TAbstrUnit): Integer;                 cdecl; external TIC_DLL;
function DMS_Unit_GetDataItemOut(this: TAbstrUnit; i: Integer): TAbstrDataItem; cdecl; external TIC_DLL;
function DMS_Unit_GetCount(this: TAbstrUnit): SizeT;                            cdecl; external TIC_DLL;
function DMS_Unit_GetLabelAttr(this: TAbstrUnit): TAbstrDataItem;               cdecl; external TIC_DLL;

function DMS_Unit_GetValueType(this: TAbstrUnit): TValueType;                   cdecl; external TIC_DLL;

// Unit range functions
procedure DMS_NumericUnit_SetRangeAsFloat64(this: TAbstrUnit; fBegin, fEnd: Float64);       cdecl; external TIC_DLL;
procedure DMS_NumericUnit_GetRangeAsFloat64(const this: TAbstrUnit; pBegin, pEnd: PFloat64); cdecl; external TIC_DLL;

procedure DMS_GeometricUnit_SetRangeAsDPoint(this: TAbstrUnit; rowBegin, colBegin, rowEnd, colEnd : Float64);            cdecl; external TIC_DLL;
procedure DMS_GeometricUnit_GetRangeAsDPoint(const this: TAbstrUnit; pRowBegin, pColBegin, pRowEnd, pColEnd : PFloat64); cdecl; external TIC_DLL;

procedure DMS_GeometricUnit_SetRangeAsIPoint(this: TAbstrUnit; rowBegin, colBegin, rowEnd, colEnd : Int32);            cdecl; external TIC_DLL;
procedure DMS_GeometricUnit_GetRangeAsIPoint(const this: TAbstrUnit; pRowBegin, pColBegin, pRowEnd, pColEnd : PInt32); cdecl; external TIC_DLL;

// DataItem property access
function DMS_DataItem_GetDomainUnit(this: TAbstrDataItem): TAbstrUnit;               cdecl; external TIC_DLL;
function DMS_DataItem_GetValuesUnit(this: TAbstrDataItem): TAbstrUnit;               cdecl; external TIC_DLL;
function DMS_DataItem_GetValueComposition(this: TAbstrDataItem): TValueComposition;  cdecl; external TIC_DLL;

function DMS_TreeItem_GetSourceDescr(studyObject: TTreeItem; sdMode: TSourceDescrMode; bShowHidden: Boolean): PSourceChar;cdecl; external TIC_DLL;
procedure DMS_ExplainValue_Clear;                                                    cdecl; external TIC_DLL;
function DMS_DataItem_ExplainAttrValueToXML(studyObject: TAbstrDataItem; xmls: TXMLOutStreamHandle; location: SizeT; sExtraInfo: PSourceChar; showHidden: Boolean): Boolean; cdecl; external TIC_DLL;
function DMS_DataItem_ExplainGridValueToXML(studyObject: TAbstrDataItem; xmls: TXMLOutStreamHandle; row, col: Int32; sExtraInfo: PSourceChar; showHidden: Boolean): Boolean; cdecl; external TIC_DLL;
function DMS_NumericDataItem_GetStatistics (studyObject: TAbstrDataItem; donePtr: PBool):   PSourceChar; cdecl; external CLC1_DLL;

// Param property access
function DMS_Param_GetValueUnit(this: TAbstrParam): TAbstrUnit;                      cdecl; external TIC_DLL;

// ParamClass property access
function DMS_ParamClass_GetFirstInstance: TParamClass;                              cdecl; external TIC_DLL;
function DMS_ParamClass_GetNextInstance(this: TParamClass): TParamClass;             cdecl; external TIC_DLL;

{*******************************************************************************
      C style Interface functions for Primary Data Access / Modification
*******************************************************************************}

// Param Data Access
function DMS_NumericParam_GetValueAsFloat64(this: TAbstrParam): Float64;        cdecl; external TIC_DLL;

function DMS_UInt32Param_GetValue(this: TAbstrParam): Cardinal;                 cdecl; external TIC_DLL;
function DMS_Int32Param_GetValue(this: TAbstrParam): Longint;                   cdecl; external TIC_DLL;
function DMS_UInt16Param_GetValue(this: TAbstrParam): SmallInt;                 cdecl; external TIC_DLL;
function DMS_Int16Param_GetValue(this: TAbstrParam): SmallInt;                  cdecl; external TIC_DLL;
function DMS_UInt8Param_GetValue(this: TAbstrParam): ShortInt;                  cdecl; external TIC_DLL;
function DMS_Int8Param_GetValue(this: TAbstrParam): ShortInt;                   cdecl; external TIC_DLL;
function DMS_Float32Param_GetValue(this: TAbstrParam): Single;                  cdecl; external TIC_DLL;
function DMS_Float64Param_GetValue(this: TAbstrParam): Double;                  cdecl; external TIC_DLL;
function DMS_BoolParam_GetValue(this: TAbstrParam): Boolean;                    cdecl; external TIC_DLL;

// Param Data Modification
procedure DMS_NumericParam_SetValueAsFloat64(this: TAbstrParam; value: Float64);cdecl; external TIC_DLL;
procedure DMS_StringParam_SetValue(this: TAbstrParam; value: PSourceChar);      cdecl; external TIC_DLL;

procedure DMS_Float32Param_SetValue(this: TAbstrParam; Value: Float32);         cdecl; external TIC_DLL;
procedure DMS_UInt32Param_SetValue(this: TAbstrParam; Value: UInt32);           cdecl; external TIC_DLL;
procedure DMS_Int32Param_SetValue(this: TAbstrParam; Value: Int32);             cdecl; external TIC_DLL;
procedure DMS_UInt16Param_SetValue(this: TAbstrParam; Value: UInt16);           cdecl; external TIC_DLL;
procedure DMS_Int16Param_SetValue(this: TAbstrParam; Value: Int16);             cdecl; external TIC_DLL;
procedure DMS_UInt8Param_SetValue(this: TAbstrParam; Value: UInt8);             cdecl; external TIC_DLL;
procedure DMS_Int8Param_SetValue(this: TAbstrParam; Value: Int8);               cdecl; external TIC_DLL;
procedure DMS_Float64Param_SetValue(this: TAbstrParam; Value: Float64);         cdecl; external TIC_DLL;
procedure DMS_BoolParam_SetValue(this: TAbstrParam; Value: Bool);               cdecl; external TIC_DLL;

// ===================================
// Attr data access
// ===================================

function  DMS_DataItem_GetNrFeatures(this: TAbstrDataItem): UInt32;             cdecl; external TIC_DLL;
function  DMS_DataItem_IsInMem(this: TAbstrDataItem): Boolean;                  cdecl; external TIC_DLL;
function  DMS_DataItem_GetLockCount(this: TAbstrDataItem): Int32;               cdecl; external TIC_DLL;
function  DMS_DataReadLock_Create(this: TAbstrDataItem; mustUpdateCertain: Boolean): TDataReadLock;         cdecl; external TIC_DLL;
procedure DMS_DataReadLock_Release(this: TDataReadLock);                        cdecl; external TIC_DLL;

// Indexed value Access

function DMS_AnyDataItem_GetValueAsCharArray(this: TAbstrDataItem; index: SizeT; buffer: PSourceChar; bufSize: UInt32): Boolean; cdecl; external TIC_DLL;
function DMS_AnyDataItem_GetValueAsCharArraySize(this: TAbstrDataItem; index: SizeT): UInt32;                              cdecl; external TIC_DLL;

function DMS_NumericAttr_GetValueAsFloat64 (const this: TAbstrDataItem; index: SizeT): Float64;                            cdecl; external TIC_DLL;
function DMS_NumericAttr_GetValueAsInt32   (const this: TAbstrDataItem; index: SizeT): Int32;                              cdecl; external TIC_DLL;

function DMS_UInt32Attr_GetValue (this: TAbstrDataItem; index: SizeT): Cardinal;  cdecl; external TIC_DLL;
function DMS_Int32Attr_GetValue  (this: TAbstrDataItem; index: SizeT): Longint;   cdecl; external TIC_DLL;
function DMS_UInt16Attr_GetValue (this: TAbstrDataItem; index: SizeT): SmallInt;  cdecl; external TIC_DLL;
function DMS_Int16Attr_GetValue  (this: TAbstrDataItem; index: SizeT): SmallInt;  cdecl; external TIC_DLL;
function DMS_UInt8Attr_GetValue  (this: TAbstrDataItem; index: SizeT): ShortInt;  cdecl; external TIC_DLL;
function DMS_Int8Attr_GetValue   (this: TAbstrDataItem; index: SizeT): ShortInt;  cdecl; external TIC_DLL;
function DMS_Float32Attr_GetValue(this: TAbstrDataItem; index: SizeT): single;    cdecl; external TIC_DLL;
function DMS_Float64Attr_GetValue(this: TAbstrDataItem; index: SizeT): double;    cdecl; external TIC_DLL;
function DMS_BoolAttr_GetValue   (this: TAbstrDataItem; index: SizeT): Boolean;   cdecl; external TIC_DLL;

// Indexed value Array Access

procedure DMS_NumericAttr_GetValuesAsFloat64Array(this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PFloat64); cdecl; external TIC_DLL;
procedure DMS_NumericAttr_GetValuesAsInt32Array  (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PInt32  ); cdecl; external TIC_DLL;

procedure DMS_Int32Attr_GetValueArray  (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PInt32  ); cdecl; external TIC_DLL;
procedure DMS_UInt32Attr_GetValueArray (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PUInt32 ); cdecl; external TIC_DLL;
procedure DMS_UInt16Attr_GetValueArray (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PUInt16 ); cdecl; external TIC_DLL;
procedure DMS_Int16Attr_GetValueArray  (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PInt16  ); cdecl; external TIC_DLL;
procedure DMS_UInt8Attr_GetValueArray  (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PUInt8  ); cdecl; external TIC_DLL;
procedure DMS_Int8Attr_GetValueArray   (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PInt8   ); cdecl; external TIC_DLL;
procedure DMS_Float64Attr_GetValueArray(this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PFloat64); cdecl; external TIC_DLL;
procedure DMS_Float32Attr_GetValueArray(this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PFloat32); cdecl; external TIC_DLL;

// ===================================
// Attr Data Modification
// ===================================

procedure DMS_DataItem_ClearData(this: TAbstrDataItem);                         cdecl; external TIC_DLL;

// Indexed value Modification

procedure DMS_NumericAttr_SetValueAsFloat64 (this: TAbstrDataItem; index: SizeT; value: Float64);                     cdecl; external TIC_DLL;
procedure DMS_NumericAttr_SetValueAsInt32   (this: TAbstrDataItem; index: SizeT; value: Int32);                       cdecl; external TIC_DLL;
procedure DMS_NumericDataItem_SetValuePairAsFloat64 (this: TAbstrDataItem; loc, value: Float64);                       cdecl; external TIC_DLL;
procedure DMS_NumericDataItem_SetRangeValueAsFloat64(this: TAbstrDataItem; fBegin, fEnd, value: Float64);              cdecl; external TIC_DLL;
procedure DMS_NumericDataItem_SetIncRangeValueAsFloat64(this: TAbstrDataItem; fBegin, fEnd, value: Float64);           cdecl; external TIC_DLL;
procedure DMS_AnyDataItem_SetValueAsCharArray(this: TAbstrDataItem; index: SizeT; buffer: PSourceChar); cdecl; external TIC_DLL;

procedure DMS_UInt32Attr_SetValue (this: TAbstrDataItem; index: SizeT; value: UInt32);  cdecl; external TIC_DLL;
procedure DMS_Int32Attr_SetValue  (this: TAbstrDataItem; index: SizeT; value: Int32);   cdecl; external TIC_DLL;
procedure DMS_UInt16Attr_SetValue (this: TAbstrDataItem; index: SizeT; value: UInt16);  cdecl; external TIC_DLL;
procedure DMS_Int16Attr_SetValue  (this: TAbstrDataItem; index: SizeT; value: Int16);   cdecl; external TIC_DLL;
procedure DMS_UInt8Attr_SetValue  (this: TAbstrDataItem; index: SizeT; value: UInt8);   cdecl; external TIC_DLL;
procedure DMS_Int8Attr_SetValue   (this: TAbstrDataItem; index: SizeT; value: Int8);    cdecl; external TIC_DLL;
procedure DMS_Float32Attr_SetValue(this: TAbstrDataItem; index: SizeT; value: Float32); cdecl; external TIC_DLL;
procedure DMS_Float64Attr_SetValue(this: TAbstrDataItem; index: SizeT; value: Float64); cdecl; external TIC_DLL;
procedure DMS_BoolAttr_SetValue   (this: TAbstrDataItem; index: SizeT; value: Boolean); cdecl; external TIC_DLL;

// Indexed value Array Modification

procedure DMS_NumericAttr_SetValuesAsFloat64Array(this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PFloat64); cdecl; external TIC_DLL;
procedure DMS_NumericAttr_SetValuesAsInt32Array  (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PInt32);   cdecl; external TIC_DLL;

Procedure DMS_UInt32Attr_SetValueArray (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PUInt32);  cdecl; external TIC_DLL;
Procedure DMS_Int32Attr_SetValueArray  (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PInt32);   cdecl; external TIC_DLL;
Procedure DMS_UInt16Attr_SetValueArray (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PUInt16);  cdecl; external TIC_DLL;
Procedure DMS_Int16Attr_SetValueArray  (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PInt16);   cdecl; external TIC_DLL;
Procedure DMS_UInt8Attr_SetValueArray  (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PUInt8);   cdecl; external TIC_DLL;
Procedure DMS_Int8Attr_SetValueArray   (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PInt8);    cdecl; external TIC_DLL;
Procedure DMS_Float64Attr_SetValueArray(this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PFloat64); cdecl; external TIC_DLL;
Procedure DMS_Float32Attr_SetValueArray(this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PFloat32); cdecl; external TIC_DLL;
Procedure DMS_BoolAttr_SetValueArray   (this: TAbstrDataItem; firstRow, len: SizeT; clientBuffer: PBool);    cdecl; external TIC_DLL;

{*******************************************************************************
      C style Interface functions for Expressions
*******************************************************************************}

function  DMS_TreeItem_GetParseResult(this: TTreeItem): TParseResult;              cdecl; external TIC_DLL;
function  DMS_TreeItem_CreateParseResult(this: TTreeItem; expr: PItemChar; contextIsTarget: Boolean): TParseResult;              cdecl; external TIC_DLL;
procedure DMS_ParseResult_Release       (PS_result: TParseResult);                                 cdecl; external TIC_DLL;

function  DMS_ParseResult_CheckSyntax(PS_result: TParseResult): Boolean;                           cdecl; external TIC_DLL;
function  DMS_ParseResult_HasFailed(PS_result: TParseResult): Boolean;                             cdecl; external TIC_DLL;
function  DMS_ParseResult_GetFailReasonAsIString(PS_result: TParseResult): IString;                cdecl; external TIC_DLL;
function  DMS_ParseResult_GetAsSLispExpr(PS_result: TParseResult; bAfterRewrite: boolean): PSourceChar;  cdecl; external TIC_DLL;
function  DMS_ParseResult_GetAsLispObj(PS_result: TParseResult; bAfterRewrite: boolean): TLispObj; cdecl; external TIC_DLL;
function  DMS_ParseResult_CreateResultingTreeItem(PS_result: TParseResult): TTreeItem;             cdecl; external TIC_DLL;
function  DMS_ParseResult_CheckResultingTreeItem(PS_result: TParseResult; this: TTreeItem): boolean; cdecl; external TIC_DLL;
function  DMS_ParseResult_CalculateResultingData (PS_result: TParseResult): TTreeItem;              cdecl; external TIC_DLL;

{*******************************************************************************
      C style Interface functions for LispObjects
*******************************************************************************}

function LispObj_IsNumb(this: TLispObj): boolean; cdecl; external SYM_DLL;
function LispObj_GetNumbVal(this: TLispObj): double; cdecl; external SYM_DLL;

function LispObj_IsSymb(this: TLispObj): boolean; cdecl; external SYM_DLL;
function LispObj_GetSymbID(this: TLispObj): integer; cdecl; external SYM_DLL;
function LispObj_GetSymbStr(this: TLispObj): PSourceChar; cdecl; external SYM_DLL;
function LispObj_IsVar(this: TLispObj): boolean; cdecl; external SYM_DLL;

function LispObj_IsStrn(this: TLispObj): boolean; cdecl; external SYM_DLL;
function LispObj_GetStrnStr(this: TLispObj): PSourceChar; cdecl; external SYM_DLL;

function LispObj_IsList(this: TLispObj): boolean; cdecl; external SYM_DLL;
function LispObj_IsEndp(this: TLispObj): boolean; cdecl; external SYM_DLL;
function LispObj_IsRealList(this: TLispObj): boolean; cdecl; external SYM_DLL;
function LispObj_GetLeft(this: TLispObj): TLispObj; cdecl; external SYM_DLL;
function LispObj_GetRight(this: TLispObj): TLispObj; cdecl; external SYM_DLL;

{*******************************************************************************
            C style Interface functions for COperator Info
*******************************************************************************}

function DMS_OperatorSet_GetNrOperatorGroups(): UInt32;                         cdecl; external CLC1_DLL;
function DMS_OperatorSet_GetOperatorGroup(i: UInt32): COperGroup;               cdecl; external CLC1_DLL;
function DMS_OperatorGroup_GetName       (this: COperGroup): PItemChar;         cdecl; external CLC1_DLL;
function DMS_OperatorGroup_GetFirstMember(this: COperGroup): COperator;         cdecl; external CLC1_DLL;
function DMS_Operator_GetNextGroupMember (this: COperator):  COperator;         cdecl; external CLC1_DLL;

function  DMS_Operator_GetNrArguments(This: COperator): Integer;                          cdecl; external CLC1_DLL; //TIC_DLL; (AK)
function  DMS_Operator_GetArgumentClass (This: COperator; argnr: Integer): CRuntimeClass; cdecl; external CLC1_DLL; //TIC_DLL; (AK)
function  DMS_Operator_GetResultingClass(This: COperator): CRuntimeClass;                 cdecl; external CLC1_DLL; //TIC_DLL; (AK)

{*******************************************************************************
      C style Interface functions for ClientOperator registration
*******************************************************************************}

type
    TreeItemArray                =  ^TTreeItem;
    CRuntimeClassArray           =  ^CRuntimeClass;
    TCreateFunc =  function (  clientHandle: Integer;
                               configRoot  : TTreeItem;
                               nrArgs      : Integer;
                               args        : TreeItemArray): TTreeItem; cdecl;
    TApplyFunc  =  procedure(  clientHandle: Integer;
                               result      : TTreeItem;
                               nrArgs      : Integer;
                               args        : TreeItemArray); cdecl;

function DMS_ClientDefinedOperator_Create(   name        :  PItemChar;
                                             resultCls   :  CRuntimeClass;
                                             nrArgs      :  Integer;
                                             args        :  CRuntimeClassArray;
                                             createFunc  :  TCreateFunc;
                                             applyFunc   :  TApplyFunc;
                                             ClientHandle:  Integer): COperator;    cdecl; external CLC1_DLL;

procedure DMS_ClientDefinedOperator_Release(this: COperator);                       cdecl; external CLC1_DLL;

{*******************************************************************************
      IString
*******************************************************************************}

function  DMS_IString_AsCharPtr(const isThis: IString): PSourceChar; cdecl; external RTC_DLL;
procedure DMS_IString_Release(isThis: IString); cdecl; external RTC_DLL;
function  DMS_ReleasedIString2String(isThis: IString): SourceString;

function DMS_TreeItem_GetFullNameAsIString(this: TTreeItem): IString; cdecl; external TIC_DLL;
function DMS_PropDef_GetValueAsIString(self: CPropDef; me: TTreeItem): IString; cdecl; external RTC_DLL;


{*******************************************************************************
                           Debug logging and tracing
*******************************************************************************}

// Opening and closing of logfiles
function  DBG_LogOpen(fileName: PFileChar): CDebugLogHandle;                      cdecl; external RTC_DLL;
procedure DBG_LogClose(hnd: CDebugLogHandle);                                     cdecl; external RTC_DLL;

// Opening and closing of new debugcontexts (a block between accolades)
function  DBG_Create(className, memberfuncName: PItemChar): CDebugContextHandle;  cdecl; external RTC_DLL;
procedure DBG_Delete(hnd: CDebugContextHandle);                                   cdecl; external RTC_DLL;

// Dumping logging text
procedure DBG_Dump(Format: PSourceChar);                                          cdecl; external RTC_DLL;

// Opening of a new line (spationing according to number of open debug contexts)
procedure DBG_PrintSpaces;                                                        cdecl; external RTC_DLL;
procedure DBG_DebugReport;                                                        cdecl; external RTC_DLL;

// os operations

procedure DMS_TreeItem_SetAnalysisTarget(tiThis: TTreeItem; mustClean: Boolean); cdecl; external TIC_DLL;
procedure DMS_TreeItem_SetAnalysisSource(tiThis: TTreeItem); cdecl; external TIC_DLL;
function  DMS_TreeItem_GetSupplierLevel(tiThis: TTreeItem): UInt32; cdecl; external TIC_DLL;
function DMS_TreeItem_GetFullStorageName(tiThis: TTreeItem; relativeStorageName: PFileChar): IString; cdecl; external TIC_DLL;
function DMS_Config_GetFullStorageName(subDir, relativeStorageName: PFileChar): IString; cdecl; external TIC_DLL;
procedure DMS_Config_SetActiveDesktop(tiActive: TTreeItem);                                       cdecl; external TIC_DLL;

procedure DMS_Appl_SetExeDir(sExeDir: PFileChar);                                 cdecl; external RTC_DLL;
procedure DMS_Appl_SetRegStatusFlags(sf: UInt32);                                 cdecl; external RTC_DLL;
function  DMS_Appl_GetRegStatusFlags: UInt32;                                     cdecl; external RTC_DLL;
function  DMS_Appl_LocalDataDirWasUsed: Boolean;                                  cdecl; external RTC_DLL;
function  DMS_Appl_SourceDataDirWasUsed: Boolean;                                 cdecl; external RTC_DLL;
procedure STG_Bmp_SetDefaultColor(i: UInt16; color: UInt32);                      cdecl; external STG_DLL;

procedure DMS_SetCurrentDir(dir: PFileChar);                                      cdecl; external RTC_DLL;

procedure PumpWaitingMessages;                                                    cdecl; external RTC_DLL;
function HasWaitingMessages: boolean;                                             cdecl; external RTC_DLL;
function  DMS_GetLastChangeTS: TTimeStamp;                                        cdecl; external RTC_DLL;

//function  GetConfigKeyValue(configFileName: PFileChar; sectionName, keyName: PItemChar; defaultValue: PSourceChar): PSourceChar; cdecl; external RTC_DLL;
//procedure SetConfigKeyValue(configFileName: PFileChar; sectionName, keyName: PItemChar; keyValue: PSourceChar);                  cdecl; external RTC_DLL;

// ================================== SheetVisual (tbv uDmsControl)

function  SHV_DataView_Create  (context: TTreeItem; ct: Cardinal {TTreeItemViewStyle}; sm: Cardinal {ShvSyncMode} ): PDataView; cdecl; external SHV_DLL;
function  SHV_DataView_CanContain(this: PDataView; viewCandidate: TTreeItem): Boolean; cdecl; external SHV_DLL;
function  SHV_DataView_AddItem(this: PDataView; viewItem: TTreeItem; isDragging: Boolean): Boolean;  cdecl; external SHV_DLL;
procedure SHV_DataView_Destroy(this: PDataView);  cdecl; external SHV_DLL;
function  SHV_DataView_Update(this: PDataView): Boolean;  cdecl; external SHV_DLL;
procedure SHV_DataView_StoreDesktopData(this: PDataView);  cdecl; external SHV_DLL;
function  SHV_DataView_GetExportInfo   (this: PDataView; nrTileRows, nrTileCols, nrSubDotRows, nrSubDotCols: PUInt32; fullFileNameBaseStr: PIString): Boolean;  cdecl; external SHV_DLL;

function  SHV_DataView_DispatchMessage(this: PDataView;  hWnd: HWND; msg: UINT;  wParam: WPARAM; lParam: LPARAM; resultPtr: PLRESULT): Boolean;  cdecl; external SHV_DLL;
procedure SHV_DataView_SetStatusTextFunc(this: PDataView; clientHandle: TClientHandle; sft: TStatusTextFunc)  cdecl; external SHV_DLL;
procedure SHV_SetCreateViewActionFunc(cvaf: TCreateViewActionFunc) cdecl; external SHV_DLL;
function  SHV_EditPaletteView_Create(desktopContainer: TTreeItem; classAttr, themeAttr: TAbstrDataItem): TAbstrDataItem; cdecl; external SHV_DLL;

function  SHV_GetDefaultViewStyle(item: TTreeItem): TTreeItemViewStyle;               cdecl external SHV_DLL;
function  SHV_CanUseViewStyle    (item: TTreeItem; vs: TTreeItemViewStyle): boolean;  cdecl external SHV_DLL;
procedure SHV_SetAdminMode(v: Boolean);                                               cdecl external SHV_DLL;
procedure SHV_SetBusyMode (v: Boolean);                                               cdecl external SHV_DLL;

// DataContainer functions
function  SHV_DataContainer_GetDomain   (ti: TTreeItem; level: UInt32; adminMode: Boolean): TAbstrUnit; cdecl external SHV_DLL;
function  SHV_DataContainer_GetItemCount(ti: TTreeItem; commonDomainUnit: TAbstrUnit; level: UInt32; adminMode: Boolean): UInt32; cdecl external SHV_DLL;
function  SHV_DataContainer_GetItem     (ti: TTreeItem; commonDomainUnit: TAbstrUnit; k: integer; level: UInt32; adminMode: Boolean): TAbstrDataItem; cdecl external SHV_DLL;

function  SHV_DataItem_FindClassification(themeAttr: TAbstrDataItem): TAbstrDataItem; cdecl external SHV_DLL;

function TreeItem_GetSourceName(this: TTreeItem): SourceString;
function TreeItem_GetUltimate(this: TTreeItem): TTreeItem;

const
   DMS_UINT32_UNDEFINED : UInt32 = $FFFFFFFF;
   {$IFDEF WIN64}
   DMS_SIZET_UNDEFINED : SizeT = $FFFFFFFFFFFFFFFF;
   {$ELSE}
   DMS_SIZET_UNDEFINED : SizeT = $FFFFFFFF;
   {$ENDIF}

implementation

function DMS_TreeItem_FindPropDef(ti: TTreeItem; propName: ItemString): CPropDef;
begin
   Result   := DMS_Class_FindPropDef(DMS_TreeItem_GetDynamicClass(ti), PItemChar(propName));
end;

function DMS_GetValueType(vid: TValueTypeId): TValueType;
begin
  Result := __DMS_GetValueType(Cardinal(vid));
end;

function DMS_ReleasedIString2String(isThis: IString): SourceString;
begin
  if not Assigned(isThis) then exit;
  Result := DMS_IString_AsCharPtr(isThis);
  DMS_IString_Release(isThis);
end;

function TreeItem_GetSourceName(this: TTreeItem): SourceString;
begin
  Result := DMS_ReleasedIString2String(DMS_TreeItem_GetSourceNameAsIString(this))
end;

function TreeItem_GetUltimate(this: TTreeItem): TTreeItem;
begin
   Result := nil;
   while Assigned(this) do
   begin
     Result := this;
     this := DMS_TreeItem_GetSourceObject(this);
   end;
end;

initialization
   DMS_Shv_Load;
end.
