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

unit uDMSUtils;

interface

uses
   Dialogs,
   ComCtrls,
   StdCtrls,
   SysUtils,
   Graphics,
   uDMSInterfaceTypes,
   uMapClasses,
   Classes;

{*******************************************************************************
                              TYPES
*******************************************************************************}

const BLOCK_SIZE_MAX = 1000*8;
const INT32_BLOCK_SIZE = BLOCK_SIZE_MAX div 4;
const FLOAT64_BLOCK_SIZE = BLOCK_SIZE_MAX div 8;
const
  g_cValid:       TColor = clBlue;
  g_cExogenic:    TColor= clGreen;
  g_cInvalidated: TColor= $00FF00FF; // magenta $000080FF; // orange
  g_cFailed:      TColor= clRed;
  g_cOperator:    TColor= clBlack;


type
  PBool          = ^Boolean;
  // TPropDef moet synchroon blijven met PropDefName
  TPropDef       =  ( pdDialogType, pdCdf, pdUrl, pdDisableStorage, pdViewAction,
                       pdDialogData, pdLabel, pdDescr, pdKeepData,
                       pdParamType, pdParamData,
                       pdIsTemplate, pdInTemplate,
                       pdIsEndogeous, pdConfigStore,
                       pdStorageName, pdStorageType, pdStorageReadOnly,
                       pdSyncMode, pdSqlString, pdTableType,
                       pdMetric, pdProjection, pdFormat,
                       pdIsHidden, pdExpr
                     );

  TDialogType    =  ( dtExpression, dtClassification, dtBrushColor,
                       dtMap, dtExternal, dtCaseGenerator, dtLabels,
                       diBrushColor);
  TParamType     =  ( ptNone, ptExprList, ptStringList, ptItemList, ptSimilarList, ptNumeric, ptUnknown);
  TFloat64Array = array[0..FLOAT64_BLOCK_SIZE -1] of Float64;
  TInt32Array   = array[0..INT32_BLOCK_SIZE   -1] of Int32  ;

  // added: 4 feb 2002
  TShapeFileType = (sftNone, sftPoint, sftArc, sftPolygon, sftOther); // XXX
  ExportProc = procedure (ti: TTreeItem; fn: TFileName; adminMode: Boolean);


{*******************************************************************************
                              CONSTANTS
*******************************************************************************}

const
   MAX_FLOAT32    =  1e38;
   MAX_INT32      =  $7FFF0000;
   GRIDDATA_NAME  =  'GridData';
   PALETTEDATA_NAME= 'PaletteData';
   VIEWDATA_NAME  =  'ViewData';
   PAR_XLL_CORNER =  'xllcorner';
   PAR_YLL_CORNER =  'yllcorner';
   PAR_CELL_SIZE  =  'cellsize';
   PAR_NODATA_VAL =  'NODATA_val';
   NO_BASEGRID_NAME = '<DON''T USE BASEGRID>';

   PropDefName  : array [TPropDef] of ItemString =
                        ('DialogType', 'cdf', 'url', 'DisableStorage', 'ViewAction',
                         'DialogData', 'Label', 'Descr', 'KeepData',
                         'ParamType', 'ParamData',
                         'IsTemplate', 'InTemplate',
                         'IsEndogenous', 'ConfigStore',
                         'StorageName', 'StorageType', 'StorageReadOnly',
                         'SyncMode', 'SqlString', 'TableType',
                         'Metric', 'Projection', 'Format',
                         'IsHidden', 'Expr'
                         );

var
   PropDefCache : array[TPropDef] of CPropDef;
const g_GetStateLock: UInt32 = 0;
{*******************************************************************************
                              FUNCTIONS
*******************************************************************************}

function  IsDataItem (ti: TTreeItem): boolean;
function  IsUnit     (ti: TTreeItem): boolean;
function  IsParameter(ti: TTreeItem): boolean;

function  ShapeFileType_GetValueTypes(sft: TShapeFileType): TValueTypeIdSet;
function  ShapeFileType_FindFromFormat(format: SourceString): TShapeFileType;

function  ValueTypeId_IsInteger(const vt: TValueTypeID): Boolean;
function  ValueTypeId_IsFloat  (const vt: TValueTypeID): Boolean;
function  ValueTypeId_IsNumeric(const vt: TValueTypeID): Boolean;
function  ValueTypeId_IsBoolean(const vt: TValueTypeID): Boolean;
function  ValueTypeId_IsNumericOrBoolean(const vt: TValueTypeID): Boolean;
function  ValueTypeId_IsPoint  (const vt: TValueTypeID): Boolean;
function  ValueTypeId_IsRect   (const vt: TValueTypeID): Boolean;
function  ValueTypeId_IsArc    (const vt: TValueTypeID): Boolean;
function  ValueTypeId_IsPolygon(const vt: TValueTypeID): Boolean;
function  ValueTypeId_IsGeometric(const vt: TValueTypeID): Boolean;

function IsDomainUnit(au: TAbstrUnit): Boolean;
function  AbstrUnit_GetValueTypeID(const u: TAbstrUnit): TValueTypeID;

function  AbstrUnit_IsInteger(const u: TAbstrUnit): Boolean;
function  AbstrUnit_IsNumeric(const u: TAbstrUnit): Boolean;
function  AbstrUnit_IsNumericOrBoolean(const u: TAbstrUnit): Boolean;
function  AbstrUnit_IsPoint  (const u: TAbstrUnit): Boolean;

function  AbstrUnit_GetProjectionStr(const u: TAbstrUnit): SourceString;
function  AbstrUnit_GetMetricStr(const u: TAbstrUnit): SourceString;
function  AbstrUnit_GetFormat(const u: TAbstrUnit): SourceString;

function  GetValuesUnitValueTypeID(const di: TAbstrDataItem): TValueTypeID;

function  Unit_HasExternalKeyData(const u: TAbstrUnit): Boolean;

function  NumericDenseDataItem_GetFloat64Data(const di: TAbstrDataItem; var floatBuffer: TFloat64Array;
        position: SizeT; requestedCnt: Cardinal): Cardinal;
function  AnyDataItem_GetValueAsString(const di: TAbstrDataItem; nRecNo: SizeT): SourceString;
function  AnyParam_GetValueAsString(const di: TAbstrDataItem): string;

function  GetPropDef(pd: TPropDef): CPropDef;
function  GetUnitPropDef(pd: TPropDef): CPropDef;

function  TreeItemPropertyValue(ti: TTreeItem; pd: CPropDef): SourceString;
function  TreeItemDescr      (ti: TTreeItem): String;
function  TreeItemDescrOrName(ti: TTreeItem): String;
function  TreeItemDialogTypeStr(ti: TTreeItem): String;
function  TreeItemDialogType (ti: TTreeItem): TDialogType;
function  TreeItemDialogData (ti: TTreeItem): SourceString;
function TreeItemSourceDescr (ti: TTreeItem; sdm: TSourceDescrMode): SourceString;
function  TreeItemParamType  (ti: TTreeItem): TParamType;
function  TreeItemParamData  (ti: TTreeItem): SourceString;
function  TreeItemCdf        (ti: TTreeItem): ItemString;
function  TreeItemViewAction (ti: TTreeItem): SourceString;
function  TreeItem_IsShapeAttr(ti: TTreeItem): Boolean;

function  TreeItem_GetExprOrSourceDescr(ti: TTreeItem; didTrick: PBool): SourceString;
function  TreeItem_GetFullName_Save(ti: TTreeItem): ItemString;
function  TreeItem_GetFullStorageName(tiThis: TTreeItem; relativeStorageName: PItemChar): FileString;
function  Config_GetFullStorageName(subDir, relativeStorageName: FileString): FileString;
function  GetFullStorageName(relativeStorageName: PFileChar): FileString;
function  Tree_GetNextSibbling(ti, sr: TTreeItem): TTreeItem;

function IsClassification(const diClass: TAbstrDataItem): boolean;

procedure FillCbxBaseGrids(cbx: TComboBox; valuesUnit: TAbstrUnit);
procedure FillCbxPalettes(cbx: TComboBox; domainUnit: TAbstrUnit; identity: string);
procedure FillCbxClassifications(cbx: TComboBox; diThis: TAbstrDataItem; identity: string; allowElements: Boolean);

function  TreeItem_GetStateColor(ti: TTreeItem): TColor;

function  PropDef_GetValueAsString_Save(self: CPropDef; me: TTreeItem): SourceString;
function  TreeItem_GetDisplayName(const ti: TTreeItem): SourceString;

// Data export function
procedure exportDBF(ti: TTreeItem; fn: TFileName; adminMode: Boolean);
procedure exportCSV(ti: TTreeItem; fn: TFileName; adminMode: Boolean);

function  CanBeDisplayed(diThis: TAbstrDataItem): boolean;

function  IsValidTreeItemName(const tiParent: TTreeItem; const sName: ItemString; var sFailReason: MsgString): boolean;

function  GetMainDMSfile(fileName: string): string;

function  AsWinPath(fn: String): String;
function  AsLcWinPath(fn: String): String;
function  AsDmsPath(fn: String): String;

procedure SetCountedRef(var tiRef: TTreeItem; tiPtr: TTreeItem);
procedure SetInterestRef(var tiRef: TTreeItem; tiPtr: TTreeItem);

implementation

uses
   Configuration,
   uGenLib, dmGeneral, uDmsInterface,
   uDmsDesktopFunctions
   , uAppGeneral // DEBUG, REMOVE
   ;

//============================ Item Types   ====================================

function TreeItem_IsDataItem(ti: TTreeItem): Boolean;
begin
   Assert(Assigned(ti));
   Result := Assigned(DMS_TreeItem_QueryInterface(ti, DMS_AbstrDataItem_GetStaticClass))
end;

function IsDataItem(ti: TTreeItem): boolean;
begin
  Result := Assigned(ti) and TreeItem_IsDataItem(ti);
end;

function  IsUnit     (ti: TTreeItem): boolean;
begin
  Result := Assigned(ti) and Assigned(DMS_TreeItem_QueryInterface(ti, DMS_AbstrUnit_GetStaticClass));
end;

function IsParameter(ti: TTreeItem): boolean;
begin
  Result := IsDataItem(ti)
        and (AbstrUnit_GetValueTypeID(DMS_DataItem_GetDomainUnit(ti)) = VT_Void);
end;


//============================ Item iteration ==================================

function Tree_GetNextSibbling(ti, sr: TTreeItem): TTreeItem;
begin
  Result := Nil;
  while Assigned(ti) do
  begin
    if (ti = sr) then exit; // returns nil

    Result := DMS_TreeItem_GetNextItem(ti);
    if Assigned(Result) then exit;

    ti := DMS_TreeItem_GetParent(ti);
  end
end;

function Tree_GetNextItem(ti, sr: TTreeItem): TTreeItem;
begin
  if DMS_TreeItem_HasSubItems(ti)
  then Result := DMS_TreeItem_GetFirstSubItem(ti)
  else Result := Tree_GetNextSibbling(ti, sr);
end;

function Tree_GetNextNonTemplateItem(ti, sr: TTreeItem): TTreeItem;
begin
  assert(not DMS_TreeItem_InTemplate(ti));
  Result := Tree_GetNextItem(ti, sr);
  while Assigned(Result) and DMS_TreeItem_IsTemplate(Result) do
    Result := Tree_GetNextSibbling(Result, sr);
end;

//============================ File Types ======================================

function ShapeFileType_GetValueTypes(sft: TShapeFileType): TValueTypeIdSet;
begin
  case sft of
    sftPoint: Result := [VT_SPoint,   VT_IPoint,   VT_FPoint,   VT_DPoint];
    sftPolygon,
    sftArc:   Result := [VT_SPolygon, VT_IPolygon, VT_FPolygon, VT_DPolygon];
  else
    Result := [];
  end
end;

function  ShapeFileType_FindFromFormat(format: SourceString): TShapeFileType;
begin
       if format = ''        then Result := sftNone
  else if format = 'point'   then Result := sftPoint
  else if format = 'arc'     then Result := sftArc
  else if format = 'polygon' then Result := sftPolygon
  else Result := sftOther;
end;

//============================ ValueTypes ======================================

function ValueTypeId_IsInteger(const vt: TValueTypeId): Boolean;
begin
   Result   := vt in [VT_UInt32, VT_Int32, VT_UInt16, VT_Int16, VT_UInt8, VT_Int8, VT_UInt4, VT_UInt2];
end;

function ValueTypeId_IsFloat(const vt: TValueTypeID): Boolean;
begin
   Result   := vt in [VT_Float32, VT_Float64];
end;

function ValueTypeId_IsNumeric(const vt: TValueTypeID): Boolean;
begin
   Result   := ValueTypeId_IsInteger(vt) OR ValueTypeId_IsFloat(vt);
end;

function ValueTypeId_IsBoolean(const vt: TValueTypeID): Boolean;
begin
   Result   := vt in [VT_Bool];
end;

function ValueTypeId_IsNumericOrBoolean(const vt: TValueTypeID): Boolean;
begin
   Result   := ValueTypeId_IsNumeric(vt) OR ValueTypeId_IsBoolean(vt);
end;

function ValueTypeId_IsPoint(const vt: TValueTypeID): Boolean;
begin
   Result   := vt in [VT_SPoint, VT_FPoint, VT_DPoint, VT_IPoint, VT_WPoint, VT_UPoint];
end;

function ValueTypeId_IsRect(const vt: TValueTypeID): Boolean;
begin
   Result   := vt in [VT_SRect, VT_FRect, VT_DRect, VT_IRect, VT_WRect, VT_URect];
end;

function ValueTypeId_IsArc(const vt: TValueTypeID): Boolean;
begin
   Result   := vt in [VT_SArc, VT_FArc, VT_DArc, VT_IArc, VT_WArc, VT_UArc];
end;

function ValueTypeId_IsPolygon(const vt: TValueTypeID): Boolean;
begin
   Result   := vt in [VT_SPolygon, VT_FPolygon, VT_DPolygon, VT_IPolygon, VT_WPolygon, VT_UPolygon];
end;

function ValueTypeId_IsGeometric(const vt: TValueTypeID): Boolean;
begin
   Result   := ValueTypeId_IsPoint(vt) OR ValueTypeId_IsRect(vt) OR
               ValueTypeId_IsPolygon(vt) OR ValueTypeId_IsArc(vt);
end;

//============================ Unit Types ======================================

function IsDomainUnit(au: TAbstrUnit): Boolean;
begin
  assert(IsUnit(au)); // PRECONDITION, implies Assigned(au)
  Result := DMS_ValueType_IsIntegral(DMS_Unit_GetValueType(au));
  if not Result then exit;

  Result := (DMS_TreeItem_GetProgressState(au) >= NC2_DataReady) and (DMS_Unit_GetCount(au) <> 0);
end;

function AbstrUnit_GetValueTypeID(const u: TAbstrUnit): TValueTypeID;
begin
   Result   := DMS_ValueType_GetID(DMS_Unit_GetValueType(u));
end; // AbstractUnitDynamicClassToStaticClass

function AbstrUnit_IsInteger(const u: TAbstrUnit): Boolean;
begin
   Result   := ValueTypeId_IsInteger(AbstrUnit_GetValueTypeId(u));
end;

function AbstrUnit_IsNumeric(const u: TAbstrUnit): Boolean;
begin
   Result   := ValueTypeId_IsNumeric(AbstrUnit_GetValueTypeId(u));
end;

function AbstrUnit_IsNumericOrBoolean(const u: TAbstrUnit): Boolean;
begin
   Result   := ValueTypeId_IsNumericOrBoolean(AbstrUnit_GetValueTypeId(u));
end;

function AbstrUnit_IsPoint(const u: TAbstrUnit): Boolean;
begin
   Result := ValueTypeId_IsPoint(AbstrUnit_GetValueTypeId(u));
end;

const pdProjPtr:    CPropDef = Nil;
const pdMetrPtr:    CPropDef = Nil;
const pdFormatPtr:  CPropDef = Nil;

function    AbstrUnit_GetProjectionStr(const u: TAbstrUnit): SourceString;
begin
  Result := TreeItemPropertyValue(u, GetUnitPropDef(pdProjection));
end;

function    AbstrUnit_GetMetricStr(const u: TAbstrUnit): SourceString;
begin
  Result := TreeItemPropertyValue(u, GetUnitPropDef(pdMetric));
end;

function    AbstrUnit_GetFormat(const u: TAbstrUnit): SourceString;
begin
  Result := TreeItemPropertyValue(u, GetUnitPropDef(pdFormat));
end;

//============================ Get Data ======================================

function NumericDenseDataItem_GetFloat64Data(const di: TAbstrDataItem; var floatBuffer: TFloat64Array;
  position:SizeT; requestedCnt: Cardinal): Cardinal;
var cnt: SizeT;
begin
   assert(DMS_TreeItem_GetInterestCount(di) <> 0);
   cnt := DMS_Unit_GetCount(DMS_DataItem_GetDomainUnit(di)); // PRECONDITION
   if (position > cnt) then
      raise ERangeError.CreateFmt('NumericDenseDataItem_GetFloat64Data: position %s out of range [0, %d>', [PChar(inttostr(position)), cnt]);

   if requestedCnt > FLOAT64_BLOCK_SIZE then requestedCnt := FLOAT64_BLOCK_SIZE;
   if requestedCnt > (cnt - position) then
      requestedCnt := cnt - position;

   if requestedCnt > 0 then
      DMS_NumericAttr_GetValuesAsFloat64Array(di, position, requestedCnt, @(floatBuffer[0]));
   Result := requestedCnt;
end;

function  AnyDataItem_GetValueAsString(const di: TAbstrDataItem; nRecNo: SizeT): SourceString;
var nSize: Cardinal;
begin
    nSize := DMS_AnyDataItem_GetValueAsCharArraySize(di, nRecNo);
    if (nSize <> DMS_UINT32_UNDEFINED) then
    begin
       SetLength(Result, nSize); // we don't need the null-terminator
       if nSize > 0 then
         DMS_AnyDataItem_GetValueAsCharArray(di, nRecNo, Addr(Result[1]), nSize);
    end
    else
       Result := '<data-error>';
end;

function  AnyParam_GetValueAsString(const di: TAbstrDataItem): string;
begin
   assert(assigned(di));
   DMS_TreeItem_IncInterestCount(di);
   try
     Result := string( AnyDataItem_GetValueAsString(di, 0) );
   finally DMS_TreeItem_DecInterestCount(di); end
end;

{*******************************************************************************
                              Some TreeItem meta-info prop access
*******************************************************************************}

function TreeItemPropertyValue(ti: TTreeItem; pd: CPropDef): SourceString;
begin
   Assert(Assigned(ti));
   Assert(Assigned(pd));
   Result := DMS_ReleasedIString2String( DMS_TreeItem_PropertyValue(ti, pd) );
end;

function GetPropDefBase(pd: TPropDef; tiClass: CRuntimeClass): CPropDef;
begin
   if not Assigned(PropDefCache[pd]) then
      PropDefCache[pd] := DMS_Class_FindPropDef(tiClass, PItemChar(PropDefName[pd]));
   Result := PropDefCache[pd];
   assert(Assigned(Result));
end;

function  GetPropDef(pd: TPropDef): CPropDef;
begin
  Result := GetPropDefBase(pd, DMS_TreeItem_GetStaticClass);
end;

function  GetUnitPropDef(pd: TPropDef): CPropDef;
begin
  Result := GetPropDefBase(pd, DMS_AbstrUnit_GetStaticClass);
end;

function TreeItemDescr(ti: TTreeItem): string;
begin
  Result := TreeItemPropertyValue(ti, GetPropDef(pdDescr)); // do evaluate and do look to source
end;

function TreeItemDescrOrName(ti: TTreeItem): String;
begin
  Result := TreeItemPropertyValue(ti, GetPropDef(pdDescr)); // do evaluate and do look to source
  if Length(trim(Result)) = 0  then
    Result := string( TreeItem_GetFullName_Save(ti) );
end;



function TreeItemDialogTypeStr(ti: TTreeItem): String;
begin
   Assert(Assigned(ti));
   Result := TreeItemPropertyValue(ti, GetPropDef(pdDialogType));
end;

function TreeItemDialogType(ti: TTreeItem): TDialogType;
var dialogTypeString: String;
begin
   Assert(Assigned(ti));
   dialogTypeString := UpperCase(TreeItemDialogTypeStr(ti)); // do evaluate and possibly look to source

   Result := dtExpression;
   if dialogTypeString = '' then exit;
   if (dialogTypeString[1] < 'E') then
   begin
            if dialogTypeString = 'BRUSHCOLOR'    then Result := dtBrushColor
       else if dialogTypeString = 'CASEGENERATOR' then Result := dtCaseGenerator
       else if dialogTypeString = 'CLASSIFICATION'then Result := dtClassification
       else if dialogTypeString = 'CLASS_BREAKS'  then Result := dtClassification
   end
   else
   begin
            if dialogTypeString = 'EXTERNAL'      then Result := dtExternal
       else if dialogTypeString = 'LABELS'        then Result := dtLabels
       else if dialogTypeString = 'MAP'           then Result := dtMap
       else if dialogTypeString = 'PALETTE'       then Result := dtBrushColor
   end
end;

function TreeItemDialogData(ti: TTreeItem): SourceString;
begin
  Result := TreeItemPropertyValue(ti, GetPropDef(pdDialogData)); // do evaluate and do look to source
end;

function TreeItemSourceDescr(ti: TTreeItem; sdm: TSourceDescrMode): SourceString;
begin
  Result := DMS_TreeItem_GetSourceDescr(ti, sdm, true);
end;

function TreeItemParamType(ti: TTreeItem): TParamType;
var paramTypeString: String;
begin
   paramTypeString := string( PropDef_GetValueAsString_Save(GetPropDef(pdParamType), ti) ); // don't evaluate and don't look to source

        if paramTypeString = ''            then Result := ptNone
   else if paramTypeString = 'ExprList'    then Result := ptExprList
   else if paramTypeString = 'ItemList'    then Result := ptItemList
   else if paramTypeString = 'SimilarList' then Result := ptSimilarList
   else if paramTypeString = 'StringList'  then Result := ptStringList
   else if paramTypeString = 'Numeric'     then Result := ptNumeric
   else
       Result := ptUnknown;
end;

function TreeItemParamData (ti: TTreeItem): SourceString;
begin
  Result := TreeItemPropertyValue(ti, GetPropDef(pdParamData));
end;

function TreeItemCdf(ti: TTreeItem): ItemString;
begin
   Result := TreeItemPropertyValue(ti, GetPropDef(pdCdf));
end;

function    TreeItemViewAction(ti: TTreeItem): SourceString;
begin
   Assert(Assigned(ti));
   Result   := TreeItemPropertyValue(ti, GetPropDef(pdViewAction));
end;

function  TreeItem_IsShapeAttr(ti: TTreeItem): Boolean;
var vtID : TValueTypeId;
begin
  Result := IsDataItem(ti);
  if not Result then exit;

  vtID := AbstrUnit_GetValueTypeID(DMS_DataItem_GetValuesUnit(ti));
  Result := ValueTypeId_IsGeometric(vtID);
end;


function TreeItem_GetExprOrSourceDescr(ti: TTreeItem; didTrick: PBool): SourceString;
var tiCalc: TParseResult;
begin
  Result := DMS_TreeItem_GetExpr(ti);
  if Assigned(didTrick) then didTrick^ := false;
  if Result = '' then
  begin
    tiCalc := DMS_TreeItem_GetParseResult(ti);
    if Assigned(tiCalc) then
    begin
      ti := DMS_TreeItem_GetSourceObject(ti);
      if Assigned(ti) then
         Result := 'assigned by parent to ' + TreeItem_GetFullName_Save(ti)
      else
         Result := 'defined by parent as (in SLisp syntax): ' + DMS_ParseResult_GetAsSLispExpr(tiCalc, false);
      if Assigned(didTrick) then didTrick^ := true;
    end
  end;
end;

function TreeItem_GetFullName_Save(ti: TTreeItem): SourceString;
begin
  if Assigned(ti)
  then Result := DMS_ReleasedIString2String(DMS_TreeItem_GetFullNameAsIString(ti))
  else Result := '@';
end;

function TreeItem_GetFullStorageName(tiThis: TTreeItem; relativeStorageName: PItemChar): FileString;
begin
  if Assigned(tiThis)
  then Result := DMS_ReleasedIString2String( DMS_TreeItem_GetFullStorageName(tiThis, relativeStorageName) );
end;

function Config_GetFullStorageName(subDir, relativeStorageName: FileString): FileString;
begin
  Result := DMS_ReleasedIString2String( DMS_Config_GetFullStorageName(PFileChar(subDir), PFileChar(relativeStorageName)) );
end;

function GetFullStorageName(relativeStorageName: PFileChar): FileString;
begin
  Result := Config_GetFullStorageName(
    iif(TConfiguration.CurrentIsAssigned, TConfiguration.CurrentConfiguration.GetConfigDir, ''), relativeStorageName);
end;

{*******************************************************************************
                              GetClassifications
*******************************************************************************}

function IsClassification(const diClass: TAbstrDataItem): boolean;
begin
  Result := Assigned(diClass)
        and IsDataItem(diClass)
        and (TreeItemDialogType(diClass) = dtClassification)
        and DMS_ValueType_IsIntegral(DMS_Unit_GetValueType(DMS_DataItem_GetDomainUnit(diClass)));
end;

function AddListCallback(clientHandle: TClientHandle; classBreakAttr: TTreeItem): Boolean; cdecl;
begin
  TList(clientHandle).Add(classBreakAttr);
  Result := True;
end;

procedure GetClassifications(di: TAbstrDataItem; var lst: TList);
begin
  DMS_DataItem_VisitClassBreakCandidates(di, AddListCallback, TClientHandle(lst))
end;

{*******************************************************************************
                              Combo Box Fillers
*******************************************************************************}

function GetDialogDataRef(ti: TTreeItem): TAbstrDataItem;
var dd: SourceString;
begin
   dd := TreeItemDialogData(ti);
   if dd = '' then
     Result := Nil
   else
     Result := DMS_TreeItem_GetItem(ti, PItemChar(dd), DMS_AbstrDataItem_GetStaticClass);
end;

procedure FillCbxBaseGrids(cbx: TComboBox; valuesUnit: TAbstrUnit);
var di: TAbstrDataItem;
    unitClass : TClass;
    ultimateValuesUnit, defUnit: TAbstrUnit;

    function Consider(di: TAbstrDataItem): Boolean;
    var foundValuesUnit: TAbstrUnit;
    begin
        assert(assigned(di)); // PRECONDITION
        Result := false;
        if not AbstrUnit_IsPoint(DMS_DataItem_GetDomainUnit(di))
        then exit;
        foundValuesUnit := DMS_DataItem_GetValuesUnit(di);

        if DMS_TreeItem_GetDynamicClass(foundValuesUnit) <> unitClass then exit;

        Result :=
           (foundValuesUnit = defUnit)
        OR (valuesUnit = defUnit)
        OR (TreeItem_GetUltimate(valuesUnit) = ultimateValuesUnit);
    end;
begin
    Assert(Assigned(valuesUnit));
    if AbstrUnit_IsPoint(valuesUnit) then
      cbx.Items.Add(NO_BASEGRID_NAME);

    unitClass := DMS_TreeItem_GetDynamicClass(valuesUnit);
    defUnit   := DMS_GetDefaultUnit(unitClass);
    ultimateValuesUnit := TreeItem_GetUltimate(valuesUnit);

    repeat
        if TreeItemDialogType(valuesUnit) = dtMap then
        begin
            di := GetDialogDataRef(valuesUnit);
            if Assigned(di) and IsDataItem(di) and Consider(di) then
                cbx.Items.AddObject(string( TreeItem_GetFullName_Save(di) ), di);
        end;
        valuesUnit := DMS_TreeItem_GetSourceObject(valuesUnit);
    until not assigned(valuesUnit);

    if cbx.Items.Count > 0 then
        cbx.ItemIndex := 0;
end;

procedure FillCbxPalettes(cbx: TComboBox; domainUnit: TAbstrUnit; identity: string);
var
  i: Integer;
  diThis: TAbstrDataItem;
  itemLabel: String;
  lst: TList;
label end_loop;
begin
   Assert(Assigned(domainUnit));

   lst := TList.Create;
   try
      DMS_DomainUnit_VisitPaletteCandidates(domainUnit, AddListCallback, TClientHandle(lst));
      for i := 0 to lst.Count - 1 do
      begin
          diThis := lst[i]; // reuse the same var
          Assert(Assigned(diThis));
          itemLabel := TreeItemDialogTypeStr(diThis);
          if (itemLabel <> '') then itemLabel := '['+itemLabel+']:';
          itemLabel := itemLabel + string( TreeItem_GetFullName_Save(diThis) );
          cbx.Items.AddObject(PChar(itemLabel), diThis);
      end;

   finally
     lst.Free;
   end;

   cbx.Items.Add(identity);

   if cbx.Items.Count > 0 then
      cbx.ItemIndex := 0;
end;

procedure FillCbxClassifications(cbx: TComboBox; diThis: TAbstrDataItem; identity: string; allowElements: Boolean);
var
  i: Integer;
  lstCdf : TList;
  vu: TAbstrUnit;
begin
  cbx.Items.Clear;
  Assert(Assigned(diThis));
  if not IsDataItem(diThis) then Exit;

  vu := DMS_DataItem_GetValuesUnit(diThis);
  if (allowElements and DMS_ValueType_IsNumeric(DMS_Unit_GetValueType(vu)))
  or IsDomainUnit(vu)
  then cbx.Items.Add(identity);

  lstCdf := TList.Create;
  try
    GetClassifications(diThis, lstCdf);

    for i := 0 to lstCdf.Count - 1 do
    begin
      diThis := lstCdf[i]; // reuse the same var
      Assert(Assigned(diThis));
      if not IsParameter(diThis) then
        cbx.Items.AddObject(string( TreeItem_GetFullName_Save(diThis) ), diThis);
    end;

  finally
    lstCdf.Free;
  end;

  if(cbx.Items.Count > 0)
    then cbx.ItemIndex := 0;
end;

function TreeItem_GetStateColor(ti: TTreeItem): TColor;
begin
    assert(assigned(ti));
    if dmfGeneral.HasActiveError then
      Result := (g_cExogenic + g_cFailed) div 2
    else
    begin
      case DMS_TreeItem_GetProgressState(ti) of
         NC2_Invalidated, NC2_MetaReady: Result := g_cInvalidated;
         NC2_DataReady:                  Result := g_cValid;
         NC2_Validated, NC2_Committed:   Result := g_cExogenic;
         else                            Result := g_cFailed
      end;
    end;
end;

function PropDef_GetValueAsString_Save(self: CPropDef; me: TTreeItem): SourceString;
begin
  if Assigned(me)
  then Result := DMS_ReleasedIString2String( DMS_PropDef_GetValueAsIString(self, me) );
end;

function TreeItem_GetDisplayName(const ti: TTreeItem): SourceString;
begin
  Result := '';
  if not Assigned(ti) then exit;

  Result := TreeItemPropertyValue(ti, GetPropDef(pdLabel));
  if Result = '' then Result := DMS_TreeItem_GetName(ti);
end;

procedure exportDBF(ti: TTreeItem; fn: TFileName; adminMode: Boolean);
var
  auCommon   : TAbstrUnit;           // common domain for applicable TreeItems
  avd, vdc, vda, tiAttr, tiContainer,
  tiGeometry : TTreeItem;
  col        : UInt32;
  vc         : TValueComposition;
label
  geometryFound, retry1, retry2;
begin
  // find commom domain
  if IsDataItem(ti) then
    auCommon := DMS_DataItem_GetDomainUnit(ti)
  else
    auCommon := SHV_DataContainer_GetDomain(ti, 1, adminMode);

  // find geometry, if any.
  tiGeometry := nil;
  if TreeItemDialogType(auCommon) = dtMap then
  begin
     tiGeometry := GetDialogDataRef(auCommon);
     if not TreeItem_IsShapeAttr(tiGeometry) then
       tiGeometry := nil;
  end;
  if not Assigned(tiGeometry) then
  begin
    tiContainer := ti;
 retry1:
    for col := 1 to SHV_DataContainer_GetItemCount(tiContainer, auCommon, 1, true) do
    begin
      tiGeometry := SHV_DataContainer_GetItem(tiContainer, auCommon, col-1, 1, true);
      if TreeItem_IsShapeAttr(tiGeometry) then goto geometryFound;
    end;
    if tiContainer <> auCommon then
    begin
      tiContainer := auCommon;
      goto retry1;
    end;
    tiGeometry := DMS_TreeItem_GetItem(ti, './Geometry', DMS_AbstrDataItem_GetStaticClass);
    if Assigned(tiGeometry) and TreeItem_IsShapeAttr(tiGeometry) then goto geometryFound;

    tiGeometry := DMS_TreeItem_GetItem(auCommon, './Geometry', DMS_AbstrDataItem_GetStaticClass);
    if Assigned(tiGeometry) and TreeItem_IsShapeAttr(tiGeometry) then goto geometryFound;
    tiGeometry := nil;
  end;

  // install the storage manager(s) and run the export
geometryFound: // create context in ViewData
  avd := TConfiguration.CurrentConfiguration.DesktopManager.GetActiveViewData;
  vdc := DMS_CreateTreeItem(avd, PItemChar( TDesktopManager.GetUniqueName('DbfTable', avd)));

  if assigned(tiGeometry) then
  begin
    vc := DMS_DataItem_GetValueComposition(tiGeometry);
    vda := DMS_CreateDataItem(vdc, 'Geometry', auCommon, DMS_DataItem_GetValuesUnit(tiGeometry), vc);
    DMS_TreeItem_SetExpr(vda, PItemChar(TreeItem_GetFullName_Save(tiGeometry)));
    DMS_PropDef_SetValueAsCharArray(GetPropDef(pdStorageType), vda, 'shp');
    DMS_PropDef_SetValueAsCharArray(GetPropDef(pdStorageName), vda, PSourceChar( SourceString(ChangeFileExt(fn, '.shp')) ));
  end;
  tiContainer := ti;

retry2:
  for col := 1 to SHV_DataContainer_GetItemCount(tiContainer, auCommon, 1, adminMode) do
  begin
    tiAttr := SHV_DataContainer_GetItem(tiContainer, auCommon, col-1, 1, adminMode);

    if not TreeItem_IsShapeAttr(tiAttr) then // shape attr don't fit in a .DBF
    begin
      assert(tiAttr <> tiGeometry);
      vc := DMS_DataItem_GetValueComposition(tiAttr);
      if not assigned(DMS_TreeItem_GetItem(vdc, DMS_TreeItem_GetName(tiAttr), nil)) then
      begin
        vda := DMS_CreateDataItem(
          vdc,
          PItemChar(DMS_TreeItem_GetName(tiAttr)),
          auCommon,
          DMS_DataItem_GetValuesUnit(tiAttr), vc
        );
        DMS_TreeItem_SetExpr(vda, PItemChar( TreeItem_GetFullName_Save(tiAttr) ));
      end;
    end;
  end;
  if tiContainer <> auCommon then
  begin
    tiContainer := auCommon;
    goto retry2;
  end;

  DMS_PropDef_SetValueAsCharArray(GetPropDef(pdStorageType), vdc, 'DBF');
  DMS_PropDef_SetValueAsCharArray(GetPropDef(pdStorageName), vdc, PSourceChar( SourceString(ChangeFileExt(fn, '.DBF')) ));

  // update it all
  vda := DMS_TreeItem_GetFirstSubItem(vdc);
  while assigned(vda) do
  begin
    DMS_TreeItem_Update(vda);
    vda := DMS_TreeItem_GetNextItem(vda);
  end;
  DMS_TreeItem_Update(vdc);

  // now open the result in a table
  dmfGeneral.SetActiveTreeItem( vdc, nil);
  dmfGeneral.miDatagridView.Click;
end;

// export data from the given TreeItem to a file of the given FileName
// The data is exported in semicolon separated format (AKA csv)
// author: Mathieu van Loon
procedure exportCSV(ti: TTreeItem; fn: TFileName; adminMode: Boolean);
var
  arrTi       : array of TTreeItem;   // dynamic array of applicable Treeitems
  auCommon    : TAbstrUnit;           // common domain for applicable TreeItems
  fout        : TOutStreamBuff;
  col         : integer;              // current row, and col
begin
  // create set of applicable TTreeItems
  Assert(Assigned(ti));
  if IsDataItem(ti) then
  begin
    auCommon := DMS_DataItem_GetDomainUnit(ti);
    setLength(arrTi, 1);
    arrTi[0] := ti;
  end
  else
  begin
    auCommon := SHV_DataContainer_GetDomain(ti, 1, adminMode);
    setLength(arrTi, SHV_DataContainer_GetItemCount(ti, auCommon, 1, adminMode));
    for col := 0 to high(arrTi) do
    begin
      arrTi[col] := SHV_DataContainer_GetItem(ti, auCommon, col, 1, adminMode);
    end;
  end;
  if(length(arrTi)=0) then Exit; // no applicable TreeItems in given Treeitem
  fout := DMS_FileOutStreamBuff_Create(fn, true);
  try
    DMS_Table_Dump(fout, length(arrTi), Addr(arrTi[0]));
  finally
    DMS_OutStreamBuff_Destroy(fout);
  end;
end;


function CanBeDisplayed(diThis: TAbstrDataItem): boolean;
var diClassification: TAbstrDataItem;
begin
  Assert(IsDataItem(diThis));

  Result := Assigned(SHV_Dataitem_FindClassification(diThis));
  if Result then exit;

  diClassification := SHV_EditPaletteView_Create(TConfiguration.CurrentConfiguration.DesktopManager.ActiveDesktop, NIL, diThis);

  Result := Assigned(diClassification)
    and IsClassification(diClassification)
    and (DMS_DataItem_GetValuesUnit(diClassification) = DMS_DataItem_GetValuesUnit(diThis));
end;

function IsValidTreeItemName(const tiParent: TTreeItem; const sName: ItemString; var sFailReason: MsgString): boolean;
begin
  Result := false;
  sFailReason := '';

  if(length(sName) = 0)
  then begin
    sFailReason := 'no name';
    Exit;
  end;

  if not IsAlphaNumeric(sName)
  then begin
    sFailReason := 'invalid characters';
    Exit;
  end;

  if(sName[1] in ['0'..'9', '_'])
  then begin
    sFailReason := 'invalid first character';
    Exit;
  end;

  if((tiParent <> nil) and (DMS_TreeItem_GetItem(tiParent, PItemChar(sName), nil) <> nil))
  then begin
    sFailReason := 'already exists';
    Exit;
  end;

  Result := true;
end;

function    GetValuesUnitValueTypeID(const di: TAbstrDataItem): TValueTypeID;
begin
   Result := AbstrUnit_GetValueTypeID(DMS_DataItem_GetValuesUnit(di));
end;

function    Unit_HasExternalKeyData(const u: TAbstrUnit): Boolean;
begin
  Result := Assigned( DMS_TreeItem_GetItem(u, 'ExternalKeyData', DMS_AbstrDataItem_GetStaticClass) );
end;

//
// local functions
//

function GetMainDMSfile(fileName: string): string;
begin
  fileName := AsWinPath(fileName);

  repeat
    result := fileName;
    fileName := ExtractFileDir(result) + '.dms';
  until (fileName = Result) or not FileExists(fileName);
end;

//
// Helper functions
//

function  AsWinPath(fn: String): String;
begin
  if (copy(fn, 1, 5) = 'file:') then
     fn := copy(fn, 6, length(fn)-5);
  Result := StringReplace(fn, '/','\', [rfReplaceAll]);
end;

function  AsLcWinPath(fn: String): String;
begin
  Result := lowerCase(AsWinPath(fn));
end;

function  AsDmsPath(fn: String): String;
begin
  Result := StringReplace(fn, '\','/', [rfReplaceAll]);
  if copy(Result,1,2) = '//' then
    Result := 'file:' + Result;
end;

procedure SetCountedRef(var tiRef: TTreeItem; tiPtr: TTreeItem);
begin
  if Assigned(tiPtr) then DMS_TreeItem_AddRef (tiPtr);
  if Assigned(tiRef) then DMS_TreeItem_Release(tiRef);
  tiRef := tiPtr;
end;

procedure SetInterestRef(var tiRef: TTreeItem; tiPtr: TTreeItem);
var oldRef: TTreeItem;
begin
  oldRef := tiRef;
  tiRef  := nil;
  try
    if Assigned(tiPtr) then DMS_TreeItem_IncInterestCount(tiPtr); // can throw, in which case tiRef is left nil.
    tiRef := tiPtr;
  finally
    if Assigned(oldRef) then DMS_TreeItem_DecInterestCount(oldRef);
  end;
end;

{*******************************************************************************
                              END OF FILE
*******************************************************************************}
var
  nGlIdx : integer;

initialization

  for nGlIdx := 0 to Length(PropDefCache) - 1 do
    PropDefCache[TPropDef(nGlIdx)] := nil;

finalization

end.

