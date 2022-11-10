unit ItemColumnProvider;

interface

uses
  uDMSInterfaceTypes;

type
  TNewTreeItemEvent   = procedure(var tiNew: TTreeItem; var bAdd: boolean) of object;
  TCheckTreeItemEvent = procedure(const tiSelected: TTreeItem; var bAllow: boolean) of object;


TAbstrItemColumnProvider = class
  m_ColumnName: string;
  m_ColumnWidth: Cardinal;

  constructor Create(columnName: String; columnWidth: Cardinal);
  function GetValue(ti: TTreeItem): String; virtual; abstract;
end;

TFullNameColumn = class (TAbstrItemColumnProvider)
  constructor Create;
  function GetValue(ti: TTreeItem): String; override;
end;


TNameColumn = class (TAbstrItemColumnProvider)
  constructor Create;
  function GetValue(ti: TTreeItem): String; override;
end;

TDomainUnitColumn = class (TAbstrItemColumnProvider)
  constructor Create;
  function GetValue(ti: TTreeItem): String; override;
end;

TValuesUnitColumn = class (TAbstrItemColumnProvider)
  constructor Create;
  function GetValue(ti: TTreeItem): String; override;
end;

TNrElementsColumn = class (TAbstrItemColumnProvider)
  constructor Create;
  function GetValue(ti: TTreeItem): String; override;
end;

TRangeColumn = class (TAbstrItemColumnProvider)
  constructor Create;
  function GetValue(ti: TTreeItem): String; override;
end;

TValueTypeColumn = class (TAbstrItemColumnProvider)
  constructor Create;
  function GetValue(ti: TTreeItem): String; override; 
end;

TMetricColumn = class (TAbstrItemColumnProvider)
  constructor Create;
  function GetValue(ti: TTreeItem): String; override;
end;

TProjectionColumn = class (TAbstrItemColumnProvider)
  constructor Create;
  function GetValue(ti: TTreeItem): String; override;
end;

TFormatColumn = class (TAbstrItemColumnProvider)
  constructor Create;
  function GetValue(ti: TTreeItem): String; override;
end;

TInterestCountColumn = class (TAbstrItemColumnProvider)
  constructor Create;
  function GetValue(ti: TTreeItem): String; override;
end;

TDataLockCountColumn = class (TAbstrItemColumnProvider)
  constructor Create;
  function GetValue(ti: TTreeItem): String; override;
end;

implementation


uses
  SysUtils,
  uDMSUtils,
  uDmsInterface;


constructor TAbstrItemColumnProvider.Create(columnName: String; columnWidth: Cardinal);
begin
  m_ColumnName  := columnName;
  m_ColumnWidth := columnWidth;
end;

constructor TFullNameColumn.Create;
begin
  inherited Create('FullName', 400);
end;

function TFullNameColumn.GetValue(ti: TTreeItem): String;
begin
  Result := TreeItem_GetFullName_Save(ti);
end;

constructor TNameColumn.Create;
begin
  inherited Create('Name', 100);
end;

function TNameColumn.GetValue(ti: TTreeItem): String;
begin
  Result := DMS_TreeItem_GetName(ti);
end;

constructor TDomainUnitColumn.Create;
begin
  inherited Create('DomainUnit', 75);
end;

function TDomainUnitColumn.GetValue(ti: TTreeItem): String;
begin
  if IsDataItem(ti)
  then Result := DMS_TreeItem_GetName(DMS_DataItem_GetDomainUnit(ti))
  else Result := '<not a dataitem>';
end;

constructor TValuesUnitColumn.Create;
begin
  inherited Create('ValuesUnit', 75);
end;

function TValuesUnitColumn.GetValue(ti: TTreeItem): String;
begin
  if IsDataItem(ti)
  then Result := DMS_TreeItem_GetName(DMS_DataItem_GetValuesUnit(ti))
  else Result := '<not a dataitem>';
end;

constructor TNrElementsColumn.Create;
begin
  inherited Create('Nr. elements', 50);
end;

function TNrElementsColumn.GetValue(ti: TTreeItem): String;
var c: SizeT;
begin
  if IsDataItem(ti) then ti := DMS_DataItem_GetDomainUnit(ti);
  if IsUnit(ti) then
    if (DMS_TreeItem_GetProgressState(ti) < NC2_DataReady)
    then Result := '<not calcualed>'
    else begin
      c := DMS_Unit_GetCount(ti);
      if c <> DMS_SIZET_UNDEFINED
      then Result := IntToStr(c)
      else result := '<undefined>';
    end
  else Result := '<not a dataitem>';
end;

constructor TRangeColumn.Create;
begin
  inherited Create('Range', 100);
end;

function TRangeColumn.GetValue(ti: TTreeItem): String;
var
  fBegin, fEnd, fRowBegin, fColBegin, fRowEnd, fColEnd : Float64;
begin
  if IsUnit(ti) then
  begin
    if(ValueTypeId_IsGeometric(AbstrUnit_GetValueTypeID(ti)))
    then begin
      DMS_GeometricUnit_GetRangeAsDPoint(ti, @fRowBegin, @fColBegin, @fRowEnd, @fColEnd);
      Result := Format('(%.5g ,%.5g) - (%.5g ,%.5g)', [fColBegin, fRowBegin, fColEnd, fRowEnd]);
    end
    else if(ValueTypeId_IsNumeric(AbstrUnit_GetValueTypeID(ti))) then
    begin
      DMS_NumericUnit_GetRangeAsFloat64(ti, @fBegin, @fEnd);
      if(fBegin < fEnd)
      then Result := Format('%.5g - %.5g', [fBegin, fEnd])
      else Result := '<undefined>';
    end
    else Result := '<undefined>';
  end
  else Result := '<not a unit>';
end;

constructor TValueTypeColumn.Create;
begin
  inherited Create('ValueType', 75);
end;

function TValueTypeColumn.GetValue(ti: TTreeItem): String;
begin
  if IsDataItem(ti) then ti := DMS_DataItem_GetValuesUnit(ti);
  if IsUnit(ti)
  then Result := DMS_ValueType_GetName(DMS_Unit_GetValueType(ti))
  else Result := '<not a unit>';
end;

constructor TMetricColumn.Create;
begin
  inherited Create('Metric', 100);
end;

function TMetricColumn.GetValue(ti: TTreeItem): String;
begin
  if IsUnit(ti)
  then Result := AbstrUnit_GetMetricStr(ti)
  else Result := '<not a unit>';
end;

constructor TProjectionColumn.Create;
begin
  inherited Create('Projection', 100);
end;

function TProjectionColumn.GetValue(ti: TTreeItem): String;
begin
  if IsUnit(ti)
  then Result := AbstrUnit_GetProjectionStr(ti)
  else Result := '<not a unit>';
end;

constructor TFormatColumn.Create;
begin
  inherited Create('Format', 50);
end;

function TFormatColumn.GetValue(ti: TTreeItem): String;
begin
  if IsUnit(ti)
  then Result := AbstrUnit_GetFormat(ti)
  else Result := '<not a unit>';
end;

constructor TInterestCountColumn.Create;
begin
  inherited Create('InterestCount', 50);
end;

function TInterestCountColumn.GetValue(ti: TTreeItem): String;
begin
  Result := inttostr(DMS_TreeItem_GetInterestCount(ti))
end;

constructor TDataLockCountColumn.Create;
begin
  inherited Create('DataLockCount', 50);
end;

function TDataLockCountColumn.GetValue(ti: TTreeItem): String;
begin
  if IsDataItem(ti)
  then Result := inttostr(DMS_DataItem_GetLockCount(ti))
  else Result := '<not a dataitem>';
end;

end.
