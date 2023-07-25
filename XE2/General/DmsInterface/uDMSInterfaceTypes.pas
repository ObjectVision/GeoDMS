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

unit uDMSInterfaceTypes;

interface

{*******************************************************************************
                              String TYPES
*******************************************************************************}

type
  IString  = Pointer;

  MsgString = UTF8String;
  PMsgChar  = PAnsiChar;

  SourceString = UTF8String;
  PSourceChar  = PAnsiChar;

  ItemString  = UTF8String;
  PItemChar   = PAnsiChar;
  FileString  = UTF8String;
  PFileChar   = PAnsiChar;

{*******************************************************************************
                  DmRtc generic types - C++ element-types
*******************************************************************************}

type
   Float32  =  Single;
   Float64  =  Double;
   UInt32   =  Cardinal;
   Int32    =  LongInt;
   UInt16   =  Word;     // MTA: WAS INCORRECT
   Int16    =  SmallInt; // MTA: WAS INCORRECT
   UInt8    =  Byte;     // MTA: WAS INCORRECT
   Int8     =  ShortInt; // MTA: WAS INCORRECT
   Bool     =  Boolean;

{$IFDEF WIN64}
   SizeT    =  UInt64;
   DiffT    =  Int64;
{$ELSE}
   SizeT    =  UInt32;
   DiffT    =  Int32;
{$ENDIF}

   PFloat32 =  ^Float32;
   PFloat64 =  ^Float64;
   PInt32   =  ^Int32;
   PUInt32  =  ^UInt32;
   PInt16   =  ^Int16;
   PUInt16  =  ^UInt16;
   PInt8    =  ^Int8;
   PUInt8   =  ^UInt8;
   PBool    =  ^Bool;
   PIString =  ^IString;

type
  PDataView= Pointer;
  HWND     = System.NativeUInt;
  UINT     = UInt32;
  WPARAM   = System.NativeUInt;
  LPARAM   = System.NativeInt;
  LRESULT  = System.NativeInt;
//  PLRESULT = ^Int32;
  PLRESULT = Pointer;
  StreamSize = System.NativeUInt;
//PGraphicClass = Pointer;

{*******************************************************************************
                  DmRtc specific types - C++ element-types
*******************************************************************************}
   TTimeStamp = UInt32;

{*******************************************************************************
   Polymorphism and schema info on element-types (pseudo-rtti/VARIANT)
*******************************************************************************}

    TAbstrValue    =  Pointer;
    TValueType     =  Pointer;
    TValueTypeId   =  (
                       VT_UInt32,    //  0
                       VT_Int32,
                       VT_UInt16,
                       VT_Int16,
                       VT_UInt8,
                       VT_Int8,
                       VT_UInt64,    // 6
                       VT_Int64,

                       VT_Float64,   // 8
                       VT_Float32,
                       VT_Float80,

                       VT_Bool,     // 11
                       VT_UInt2,
                       VT_UInt4,

                       VT_SPoint, // 14
                       VT_WPoint,
                       VT_IPoint,
                       VT_UPoint,
                       VT_FPoint,
                       VT_DPoint,

                       VT_SRect,  // 20
                       VT_WRect,
                       VT_IRect,
                       VT_URect,
                       VT_FRect,
                       VT_DRect,

                       VT_SArc,  // 26
                       VT_WArc,
                       VT_IArc,
                       VT_UArc,
                       VT_FArc,
                       VT_DArc,

                       VT_SPolygon,   // 32
                       VT_WPolygon,
                       VT_IPolygon,
                       VT_UPolygon,
                       VT_FPolygon,
                       VT_DPolygon,

                       VT_String,    // 38
                       VT_TokenID,
                       VT_Void,      // 40

                      VT_RangeUInt32, // 41
                      VT_RangeInt32 ,
                      VT_RangeUInt16,
                      VT_RangeInt16 ,
                      VT_RangeUInt8 ,
                      VT_RangeInt8  ,
                      VT_RangeUInt64,
                      VT_RangeInt64 ,
                      VT_RangeFloat64,
                      VT_RangeFloat32,
                      VT_RangeFloat80,

                      VT_UInt32Seq, // 52
                      VT_Int32Seq, // 53
                      VT_UInt16Seq, // 54
                      VT_Int16Seq, // 55
                      VT_UInt8Seq, // 56
                      VT_Int8Seq, // 57

                      VT_UInt64Seq, // 58
                      VT_Int64Seq, // 59

                      VT_Float64Seq, // 60
                      VT_Float32Seq, // 61
                      VT_Float80Seq, // 62

                      VT_Count,    // 63
                      VT_Unknown   // 64
    );
    TSourceDescrMode = (
      SDM_Configured = 0,
      SDM_ReadOnly = 1,
      SDM_WriteOnly = 2,
      SDM_All = 3
    );

    TValueTypeIdSet = set of TValueTypeId;
    TValueComposition = Cardinal;

const
    VC_Single  : TValueComposition = 0;
    VC_Range   : TValueComposition = 1;
    VC_Sequence: TValueComposition = 2;

{*******************************************************************************
                     Streaming buffers and stream objects
*******************************************************************************}
type
   TInpStreamBuff          =  Pointer;
   TOutStreamBuff          =  Pointer;
   TXMLOutStreamHandle     =  Pointer; // Handle naar een XML stream, te creeren op een OutStreamBuff
   TClientHandle           =  type Pointer;
   TOutStreamCallbackFunc  =  procedure (clienHandle: TClientHandle; data: PMsgChar; dataSize: Integer); cdecl;
   TVersionComponentCallbackFunc = procedure(clienHandle: TClientHandle; componentLevel: UInt32; componentName: PMsgChar); cdecl;

{*******************************************************************************
               DmTic types -  TTreeItem and their derivatives
*******************************************************************************}

   TTreeItem       =  type Pointer;
   TAbstrUnit      =  TTreeItem;
   TAbstrDataItem  =  TTreeItem;
   TAbstrParam     =  TAbstrDataItem;
   TDataReadLock   =  type Pointer;
   TDataWriteLock  =  type Pointer;
   TLispObj        =  Pointer;
   TAbstrDataItemPtr = ^TAbstrDataItem;

{*******************************************************************************
                     DmStg types -  Storage Management
*******************************************************************************}

   TAbstrStorageManager = Pointer;
   TStorageType         = PItemChar;

{*******************************************************************************
                     DmClc types - Addition types
*******************************************************************************}

   TTreeItemSet =  Pointer;
   TParseResult =  Pointer;

{*******************************************************************************
               Schema info (on both DmRtc and DmTic classes)
*******************************************************************************}

   // Basic (Rtc) Schema info
   TDMSObject     =  Pointer;
   CRuntimeClass  =  Pointer;
   CPropDef       =  Pointer;

   // Additional Schema info for specific Tic Classes
   TParamClass     =  Pointer;
   TDataItemClass  =  Pointer;
   TUnitClass      =  Pointer;

   // Schema info on DmClc class for built-in and registered functions
   COperator      =  Pointer;
   COperGroup     =  Pointer;

{*******************************************************************************
                     enums for DataItem types
*******************************************************************************}

   TUnitCoordType  =  Pointer;

{*******************************************************************************
            enums for Actor states (TTreeItem is derived from Actor)
*******************************************************************************}

TUpdateState =  (
	NC2_Invalidated,// = 0,
        NC2_DetermineCheck, // = 1
        NC2_DetermineState, // = 2
        NC2_InterestChange, // = 3
	NC2_MetaReady,  // = 4,
	NC2_DataReady,  // = 5,
	NC2_Validated,  // = 6,
	NC2_Committed,  // = 7,

        NC2_DetermineCheckFailed,
        NC2_DetermineStateFailed,
        NC2_InterestChangeFailed,
	NC2_MetaFailed,   //  = NC2_FailedFlag|NC2_Invalidated,
	NC2_DataFailed,   //  = NC2_FailedFlag|NC2_MetaReady,
	NC2_CheckFailed,  //  = NC2_FailedFlag|NC2_DataReady,
	NC2_CommitFailed, //  = NC2_FailedFlag|NC2_Validated,
        NC2_Impossible,   //  = NC2_FailedFlag|NC2_Comitted,

        NC_Updating,          // = 16
        NC_UpdatingElsewhere, // = 17

        NC_Creating,          // = 18
        NC_Deleting,          // = 19
        NC_PropValueChanged,  // = 20

        CC_Activate,          // = 21
        CC_ShowStatistics,    // = 22
        CC_CreateMdiChild     // = 23
   );

{*******************************************************************************
                           Debug logging and tracing
*******************************************************************************}

   CDebugLogHandle      =  Pointer; // Handle naar een DebugLog (= file waarnaar trace-info wordt geschreven).
   CDebugContextHandle  =  Pointer; // Handle naar een locale context
   TSeverityType        =  (
                              ST_MinorTrace,
                              ST_MajorTrace,
                              ST_Warning,
                              ST_Error,
                              ST_Fatal,
                              ST_DispError,
                              ST_Nothing
                           );
   TMsgCategory          = (
                              MC_nonspecific,
                              MC_system,
	                          MC_flaggable,
	                          MC_wms,
	                          MC_progress
                           );
   // Callback function for trace-info subscription
   TMsgCallbackFunc = procedure(clientHandle: TClientHandle; s: TSeverityType; msgCat: TMsgCategory; msg: PMsgChar);  cdecl;
   TReduceResources = function(clientHandle: TClientHandle): boolean; cdecl;

{*******************************************************************************
                           Other Callback functions
*******************************************************************************}

   TStatusTextFunc   =  procedure (clienHandle: TClientHandle; st: TSeverityType; data: PMsgChar); cdecl;
   TTriggerFunc      = function(clientHandle: TClientHandle): Boolean;  cdecl;
   TCoalesceHeapFunc = function(size: SizeT; context: PMsgChar): Boolean; cdecl;
   TStateChangeNotificationFunc = procedure(clientHandle: TClientHandle; item: TTreeItem; newState: TUpdateState);  cdecl;
   TContextNotification = procedure(clientHandle: TClientHandle; description: PMsgChar); cdecl;
   TCppExceptionTranslator = procedure(msg: PMsgChar); cdecl;
   TCreateViewActionFunc   =  procedure (tiContext: TTreeItem; sAction: PMsgChar; nCode, x, y: Integer; dooAddHistory, isUrl, mustOpenDetailsPage: Boolean); cdecl;
   TSupplCallbackFunc       = function(clientHandle: TClientHandle; supplier: TTreeItem): Boolean; cdecl;

{*******************************************************************************
                           FUNCTIONS
*******************************************************************************}

type
  // NOTE: This enum has to be in sync with ilTreeview in unit fMain, and with ViewStyle in DataView.h
  TTreeItemViewStyle = (
  //    possible default views
        tvsMapView, tvsDataGrid, tvsExpr, tvsClassification, tvsPalette, tvsDefault, tvsContainer, tvsDataGridContainer,
  //    other views
        tvsHistogram
  ,     tvsUpdateItem
  ,     tvsUpdateTree
  ,     tvsSubItemSchema
  ,     tvsSupplierSchema
  ,     tvsExprSchema
  );
  TShvSyncMode = ( smLoad, smSave );


type TCreateStruct = record
  ct:             Cardinal;  // was TTreeItemViewStyle;
  dataViewObj:    PDataView;
  contextItem:    TTreeItem;
  caption:        PSourceChar;
  makeOverlapped: Cardinal; // was Boolean;
  maxSizeX:       Integer;
  maxSizeY:       Integer;
  hWnd:           HWND;
end;

type PCreateStruct = ^TCreateStruct;


implementation

end.

