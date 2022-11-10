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

// notes
// 1. If loading of the dialog is/becomes to slow, we can choose to postpone
//    certain initializations (constructor) until they are needed.

// 1. taborder
// 2. completeness hint

unit fUnitComposer;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ComCtrls, StdCtrls, uDMSInterfaceTypes, Spin, Buttons, ExtCtrls,
  fmDualListBox, DualListBox;

type
  ESecondExecute = class(Exception);

  TApplyProcedure = procedure of object;
  TEnableFunction = function: boolean of object;

  TUnitComposerState = (ucsSelect, ucsTableUnit, ucsGridUnit, ucsQuantityUnit,
                        ucsPointUnit, ucsShapeUnit, ucsCodeUnit, ucsStringUnit,
                        ucsBoolUnit, ucsFocalPointMatrix, ucsAdvancedUnit);

  TSetUnitComposerState = set of TUnitComposerState;

  TfrmUnitComposer = class(TForm)
    stnMain: TStatusBar;
    btnOK_Next: TButton;
    btnCancel: TButton;
    btnHelp: TButton;
    pcMain: TPageControl;
    lblName: TLabel;
    lblDisplayName: TLabel;
    lblDescription: TLabel;
    Label4: TLabel;
    Label5: TLabel;
    Label6: TLabel;
    Label7: TLabel;
    edName: TEdit;
    edDisplayName: TEdit;
    edDescription: TEdit;
    cbxValueType: TComboBox;
    chbBaseUnit: TCheckBox;
    cbxFormat: TComboBox;
    mmExpression: TMemo;
    rbNumberItems: TRadioButton;
    rbRange: TRadioButton;
    seNumberItems: TSpinEdit;
    edFrom: TEdit;
    edTo: TEdit;
    rbTableUnit: TRadioButton;
    rbGridUnit: TRadioButton;
    rbQuantityUnit: TRadioButton;
    rbShapeUnit: TRadioButton;
    rbStringUnit: TRadioButton;
    rbBoolUnit: TRadioButton;
    rbAdvancedUnit: TRadioButton;
    Label8: TLabel;
    tsSelectType: TTabSheet;
    tsTableUnit: TTabSheet;
    tsGridUnit: TTabSheet;
    tsQuantityUnit: TTabSheet;
    tsPointUnit: TTabSheet;
    tsShapeUnit: TTabSheet;
    tsCodeUnit: TTabSheet;
    tsStringUnit: TTabSheet;
    tsBoolUnit: TTabSheet;
    tsFocalPointMatrix: TTabSheet;
    tsAdvancedUnit: TTabSheet;
    rbPointUnit: TRadioButton;
    rbCodeUnit: TRadioButton;
    Label9: TLabel;
    Label10: TLabel;
    rbFocalPointMatrix: TRadioButton;
    Label1: TLabel;
    Label2: TLabel;
    Label3: TLabel;
    Label11: TLabel;
    Label12: TLabel;
    Label13: TLabel;
    Label14: TLabel;
    Label15: TLabel;
    Label16: TLabel;
    Label17: TLabel;
    btnBack: TButton;
    rbQuantityBaseUnit: TRadioButton;
    rbQuantityDerivedUnit: TRadioButton;
    mmQuantityExpression: TMemo;
    lblExpression: TLabel;
    bvlQuantityBaseUnit: TBevel;
    Label19: TLabel;
    cbxQuantityType: TComboBox;
    bvlQuantityDerivedUnit: TBevel;
    Label20: TLabel;
    lblMetric: TLabel;
    Label21: TLabel;
    edQuantityFormat: TEdit;
    btnCheck: TSpeedButton;
    rbGridBaseUnit: TRadioButton;
    Bevel1: TBevel;
    rbGridGridsetUnit: TRadioButton;
    Bevel2: TBevel;
    cbxGridReferenceUnit: TComboBox;
    Label23: TLabel;
    edSfX: TEdit;
    ed0X: TEdit;
    edTlX: TEdit;
    edSfY: TEdit;
    ed0Y: TEdit;
    edTlY: TEdit;
    edBrX: TEdit;
    edBrY: TEdit;
    Label24: TLabel;
    Label25: TLabel;
    Label26: TLabel;
    Label27: TLabel;
    Label28: TLabel;
    Label29: TLabel;
    Label30: TLabel;
    Label31: TLabel;
    edBaseTlX: TEdit;
    edBaseBrX: TEdit;
    edBaseBrY: TEdit;
    edBaseTlY: TEdit;
    Label22: TLabel;
    Label32: TLabel;
    Bevel3: TBevel;
    Bevel4: TBevel;
    Bevel5: TBevel;
    Bevel6: TBevel;
    rbSubsetUnit: TRadioButton;
    rbUnion: TRadioButton;
    rbUnique: TRadioButton;
    rbCombine: TRadioButton;
    Label33: TLabel;
    Label34: TLabel;
    dlbUnion: TDualListBox;
    dlbCombine: TDualListBox;
    Label35: TLabel;
    Label36: TLabel;
    chbCount: TCheckBox;
    Label37: TLabel;
    Bevel7: TBevel;
    rbTableBaseUnit: TRadioButton;
    Label38: TLabel;
    cbxTableBaseType: TComboBox;
    Label39: TLabel;
    edTableCount: TEdit;
    Label40: TLabel;
    cbxPointType: TComboBox;
    Label41: TLabel;
    Label42: TLabel;
    Label43: TLabel;
    Label44: TLabel;
    edPointTLX: TEdit;
    edPointBRX: TEdit;
    edPointBRY: TEdit;
    edPointTLY: TEdit;
    Label45: TLabel;
    cbxShapeType: TComboBox;
    Label46: TLabel;
    Label47: TLabel;
    Label48: TLabel;
    Label49: TLabel;
    edShapeTLX: TEdit;
    edShapeBRX: TEdit;
    edShapeBRY: TEdit;
    edShapeTLY: TEdit;
    Label50: TLabel;
    cbxShapeFormat: TComboBox;
    Label51: TLabel;
    cbxCodeType: TComboBox;
    Label52: TLabel;
    edCodeFrom: TEdit;
    edCodeTo: TEdit;
    Label53: TLabel;
    Label55: TLabel;
    edFPDist: TEdit;
    Label54: TLabel;
    cbxSubitemsType: TComboBox;
    reDescription: TRichEdit;
    sbUniqueFilter: TSpeedButton;
    edUniqueFilter: TEdit;
    edUniqueSourceTU: TEdit;
    sbUniqueSourceTU: TSpeedButton;
    edSourceTU: TEdit;
    sbSourceTU: TSpeedButton;
    edSubsetFilter: TEdit;
    sbSubsetFilter: TSpeedButton;
    edTargetVU: TEdit;
    sbTargetVU: TSpeedButton;
    procedure chbBaseUnitClick(Sender: TObject);
    procedure btnOK_NextClick(Sender: TObject);
    procedure NumberRangeClick(Sender: TObject);
    procedure DataChanged(Sender: TObject);
    procedure SelectUnitType(Sender: TObject);
    procedure btnBackClick(Sender: TObject);
    procedure QuantitySelectBaseUnit(Sender: TObject);
    procedure btnCheckClick(Sender: TObject);
    procedure mmQuantityExpressionChange(Sender: TObject);
    procedure GridSelectBaseUnit(Sender: TObject);
    procedure TableSelectStyle(Sender: TObject);
    procedure cbxCodeTypeChange(Sender: TObject);
    procedure ShowTabSheet(Sender: TObject);
    procedure tsSelectTypeShow(Sender: TObject);
    procedure sbUniqueFilterClick(Sender: TObject);
    procedure edUniqueSourceTUChange(Sender: TObject);
    procedure sbUniqueSourceTUClick(Sender: TObject);
    procedure edSourceTUChange(Sender: TObject);
    procedure sbSubsetFilterClick(Sender: TObject);
    procedure sbSourceTUClick(Sender: TObject);
    procedure edTargetVUChange(Sender: TObject);
    procedure sbTargetVUClick(Sender: TObject);
  private
    { Private declarations }
    m_tiRoot : TTreeItem;
    m_tiContainer : TTreeItem;
    m_tiContext : TTreeItem;
    m_uUnit : TAbstrUnit;
    m_ucsActive : TUnitComposerState;
    m_ucsSelected : TUnitComposerState;
    m_procApply : TApplyProcedure;
    m_funEnable : TEnableFunction;
    m_bQuantityExpressionChecked : boolean;
    m_lstDiv : TList;
    m_suAvailable : TSetUnitComposerState;
    m_bExecuteCalled : boolean;
    m_vidTUBaseUnitType : TValueTypeId;
    m_nTUBaseUnitCount : Cardinal;

    // property functions
    procedure SetContainer(tiValue: TTreeItem);
    procedure SetTUBaseUnitType(const vidValue: TValueTypeId);
    procedure SetUnit(uValue: TAbstrUnit);

    // general function
    function  CreateNewUnit(const vtSelected: TValueType): TAbstrUnit;
    procedure PlaceGeneralControls(tsCurrent: TTabSheet);
    procedure SetButtonStates;
    function  UnitNameOK: boolean;
    function  QuantityFormatOK: boolean;
    procedure ShowAvailableSelections;

    function  ParseExpression(const sExpression: ItemString; var vtResult: TValueType): boolean;
    function  QuantityCheckExpression(var sMetric: ItemString; var vtResult: TValueType): boolean;
    function  GridsetCreateExpression(var sExpr: ItemString; var vtResult: TValueType): boolean;
    function  SubsetCreateExpression (var sExpr: ItemString; var vtResult: TValueType): boolean;
    function  UniqueCreateExpression (var sExpr: ItemString; var vtResult: TValueType): boolean;
    function  CombineCreateExpression(var sExpr: ItemString; var vtResult: TValueType): boolean;
    function  UnionCreateExpression  (var sExpr: ItemString; var vtResult: TValueType): boolean;

    function  CreateCountItem(uDomain: TAbstrUnit): boolean;
    function  CreateDistMatrXItem(uDomain: TAbstrUnit): boolean;
    function  CreateAbsWeightItem(uDomain: TAbstrUnit): boolean;
    function  CreateRelWeightItem(uDomain: TAbstrUnit): boolean;

    // display functions
    procedure DisplaySelect;
    procedure DisplayTableUnit;
    procedure DisplayGridUnit;
    procedure DisplayQuantityUnit;
    procedure DisplayPointUnit;
    procedure DisplayShapeUnit;
    procedure DisplayCodeUnit;
    procedure DisplayStringUnit;
    procedure DisplayBoolUnit;
    procedure DisplayFocalPointMatrix;
    procedure DisplayAdvancedUnit;

    // apply functions
    procedure ApplySelect;
    procedure ApplyTableUnit;
    procedure ApplyGridUnit;
    procedure ApplyQuantityUnit;
    procedure ApplyPointUnit;
    procedure ApplyShapeUnit;
    procedure ApplyCodeUnit;
    procedure ApplyStringUnit;
    procedure ApplyBoolUnit;
    procedure ApplyFocalPointMatrix;
    procedure ApplyAdvancedUnit;

    // enabled functions
    function EnableOKSelect: boolean;
    function EnableOKTableUnit: boolean;
    function EnableOKGridUnit: boolean;
    function EnableOKQuantityUnit: boolean;
    function EnableOKPointUnit: boolean;
    function EnableOKShapeUnit: boolean;
    function EnableOKCodeUnit: boolean;
    function EnableOKStringUnit: boolean;
    function EnableOKBoolUnit: boolean;
    function EnableOKFocalPointMatrix: boolean;
    function EnableOKAdvancedUnit: boolean;
  public
    { Public declarations }
    constructor Create(AOwner: TComponent; tiRoot: TTreeItem); reintroduce;
    destructor  Destroy; override;

    function Execute: boolean;

    property AvailableUnits : TSetUnitComposerState read m_suAvailable write m_suAvailable;
    property BeginState : TUnitComposerState read m_ucsSelected write m_ucsSelected;
    property Container : TTreeItem read m_tiContainer write SetContainer;
    property TUBaseUnitCount : Cardinal read m_nTUBaseUnitCount write m_nTUBaseUnitCount;
    property TUBaseUnitType : TValueTypeId read m_vidTUBaseUnitType write SetTUBaseUnitType;
    property Unit_ : TAbstrUnit read m_uUnit write SetUnit;
  end;


implementation

{$R *.DFM}

uses
  uDMSUtils,
  uDmsInterface,
  uGenLib,  xxx ItemColumnProvider,
  uAppGeneral, Math, Configuration, dmGeneral,
  xxx fTreeItemPickList, fMain, fExpresEdit;

const
  c_arUnitDescriptions : array [TUnitComposerState] of string = (
    'No type selected',
    'Conceptual an entity, in database terms a table,'#13#10'indicating the domain for which data items are'#13#10'configured and determining the number of elements.'#13#10'A domain unit is e.g. countries in Europe.',
    'Conceptual an entity, in data terms a grid or matrix,'#13#10'indicating the domain for which data items are'#13#10'configured that refer to an X and Y coordinate.'#13#10'The number of rows and columns is obligatory.',
    'A unit describing a numeric data item that indicates'#13#10'a quantitative characteristic, e.g. distance in meters'#13#10'or output in Euro''s.',
    'A unit describing a data item that contains a set of'#13#10'coordinates (X and Y), e.g. point locations of dwellings.',
    'A unit describing a data item that contains collections'#13#10'of points, such as arcs and polygons, e.g. a road'#13#10'network or regional boundary layer.',
    'A unit describing a data item that contains some external'#13#10'code, e.g. city code.',
    'A unit describing a data item that contains text values,'#13#10'e.g. labels used in a map.',
    'A unit describing a data item that contains Yes/No'#13#10'values, e.g. a selection attribute.',
    'A specific unit that results in a focal point matrix '#13#10' (1/(d^2 + 2)) for potential calculations.',
    'Advanced'
  );

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                              GENERAL FUNCTIONS                             //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//**
constructor TfrmUnitComposer.Create(AOwner: TComponent; tiRoot: TTreeItem);
begin
  inherited Create(AOwner);
  m_lstDiv := TList.Create;

  // init members
  m_tiRoot := tiRoot;
  m_uUnit := nil;
  m_tiContainer := nil;
  m_tiContext := nil;
  m_ucsActive := ucsSelect;
  m_ucsSelected := ucsSelect;
  m_procApply := nil;
  m_funEnable := nil;
  m_bQuantityExpressionChecked := false;
  m_suAvailable := [];
  m_bExecuteCalled := false;
  m_vidTUBaseUnitType := VT_Unknown;
  m_nTUBaseUnitCount := 0;
end;

//**
destructor TfrmUnitComposer.Destroy;
begin
  SetCountedRef(m_uUnit,       nil);
  SetCountedRef(m_tiContext,   nil);
  SetCountedRef(m_tiContainer, nil);

  if(m_lstDiv <> nil)
  then m_lstDiv.Free;

  inherited;
end;

//**
function TfrmUnitComposer.Execute: boolean;
begin
  if(m_bExecuteCalled)
  then begin
    raise ESecondExecute.Create('Execute can only be called once per instance.');
    Exit;
  end;

  m_bExecuteCalled := true;

  // fill the general controls
  if not Assigned(m_uUnit)
  then begin
    edName.Text        := '';
    edDisplayName.Text := '';
    edDescription.Text := ''
  end
  else begin
    edName.Text        := DMS_TreeItem_GetName(m_uUnit);
    edDisplayName.Text := TreeItem_GetDisplayName(m_uUnit);
    edDescription.Text := TreeItemDescrOrName(m_uUnit);
    m_ucsActive        := ucsAdvancedUnit;
  end;

  // show what's available
  ShowAvailableSelections;

  // choose what to display
  ApplySelect;

  // show back button ?
  btnBack.Visible := m_ucsActive = ucsSelect;

  Result := ShowModal = mrOK;
end;

//**
function TfrmUnitComposer.CreateNewUnit(const vtSelected: TValueType): TAbstrUnit;
begin
  Result := DMS_CreateUnit(m_tiContainer, PItemChar(ItemString(edName.Text)), DMS_UnitClass_Find(vtSelected));

  if(Result <> nil)
  then begin
    TreeItem_SetDisplayName(Result,             edDisplayName.Text );
    DMS_TreeItem_SetDescr  (Result, PSourceChar(SourceString(edDescription.Text)));
  end;
end;

//**
procedure TfrmUnitComposer.PlaceGeneralControls(tsCurrent: TTabSheet);
begin
  lblName.Parent        := tsCurrent;
  lblDisplayName.Parent := tsCurrent;
  lblDescription.Parent := tsCurrent;
  edName.Parent         := tsCurrent;
  edDisplayName.Parent  := tsCurrent;
  edDescription.Parent  := tsCurrent;
  edName.TabOrder       := 0;
  edDisplayName.TabOrder:= 1;
  edDescription.TabOrder:= 2;
end;

//**
procedure TfrmUnitComposer.SetButtonStates;
begin
  // back button
  btnBack.Enabled := m_ucsActive <> ucsSelect;

  // ok/next button

  // set caption
  if(m_ucsActive = ucsSelect)
  then
    btnOK_Next.Caption := '&Next >'
  else
    btnOK_Next.Caption := 'OK';

  btnOK_Next.Enabled := m_funEnable;
end;

//**
function TfrmUnitComposer.UnitNameOK: boolean;
var
  sMessage : MsgString;
begin
  Result := IsValidTreeItemName(m_tiContainer, edName.Text, sMessage);
end;

function TfrmUnitComposer.QuantityFormatOK: boolean;
var
  sFormat : array[0..512] of char;
begin
  strpcopy(sFormat, edQuantityFormat.Text);
  Result := (length(edQuantityFormat.Text) = 0) or ((pos('%f', edQuantityFormat.Text) > 0) and (strscan(sFormat, '%') = strrscan(sFormat, '%')));
end;

procedure TfrmUnitComposer.ShowAvailableSelections;
begin
  if(m_suAvailable <> [])
  then begin
    rbTableUnit.Enabled        := ucsTableUnit in m_suAvailable;
    rbGridUnit.Enabled         := ucsGridUnit in m_suAvailable;
    rbQuantityUnit.Enabled     := ucsQuantityUnit in m_suAvailable;
    rbPointUnit.Enabled        := ucsPointUnit in m_suAvailable;
    rbShapeUnit.Enabled        := ucsShapeUnit in m_suAvailable;
    rbCodeUnit.Enabled         := ucsCodeUnit in m_suAvailable;
    rbStringUnit.Enabled       := ucsStringUnit in m_suAvailable;
    rbBoolUnit.Enabled         := ucsBoolUnit in m_suAvailable;
    rbFocalPointMatrix.Enabled := ucsFocalPointMatrix in m_suAvailable;
    rbAdvancedUnit.Enabled     := ucsAdvancedUnit in m_suAvailable;
  end;
end;

function TfrmUnitComposer.ParseExpression(const sExpression: ItemString; var vtResult: TValueType): boolean;
var
  pc : TParseResultContext;
//  ParseResult : TParseResult;
  tiResult : TTreeItem;
begin
  Result := False;
  vtResult := nil;

  pc := TParseResultContext.Create(m_tiContext, PItemChar(sExpression), false);
  try
    if(pc.HasParseResult) then
    begin
      if not pc.CheckSyntaxFunc(tiResult) then exit;
      if not assigned(tiResult) then exit;
      if not assigned(DMS_TreeItem_QueryInterface(tiResult, DMS_AbstrUnit_GetStaticClass)) then exit;
      vtResult := DMS_Unit_GetValueType(tiResult);
      Result := true;
    end;
  finally
    pc.DispError;
    pc.Free;
  end;
end;

function TfrmUnitComposer.QuantityCheckExpression(var sMetric: ItemString; var vtResult: TValueType): boolean;
var
  sExpr : ItemString;
  pc : TParseResultContext;
  tiResult : TTreeItem;
begin
  Result := false;
  sMetric := '';
  vtResult := nil;

  sExpr := mmQuantityExpression.Text;
  if(Length(sExpr) = 0) then
  begin
    DispMessage('CalculationRule cannot be empty.');
    exit;
  end;

  pc := TParseResultContext.Create(m_tiContext, PItemChar(sExpr), false);
  try
    if(pc.HasParseResult) then
    begin
      if not pc.CheckSyntaxFunc(tiResult) then exit;
      if not assigned(tiResult) then exit;
      if not assigned(DMS_TreeItem_QueryInterface(tiResult, DMS_AbstrUnit_GetStaticClass)) then
      begin
        DispMessage('CalculationRule should result in a unit.');
        exit;
      end;
      vtResult := DMS_Unit_GetValueType(tiResult);
      sMetric  := AbstrUnit_GetMetricStr(tiResult);
      Result := true;
    end;
  finally
    pc.DispError;
    pc.Free;
  end;
end;

function TfrmUnitComposer.GridsetCreateExpression(var sExpr: ItemString; var vtResult: TValueType): boolean;
var
  sExpression : ItemString;
  sCast1, sCast2, sReqRes : ItemString;
  tiReference: TTreeItem;
  sReference : string;
begin
  sExpr := '';
  vtResult := nil;

  with cbxGridReferenceUnit do
    tiReference := Items.Objects[ItemIndex];
  sReference := TreeItem_GetFullName_Save(tiReference);

  case AbstrUnit_GetValueTypeID(tiReference) of
  VT_SPoint  : sCast1 := 'Int16(';
  VT_IPoint  : sCast1 := 'Int32(';
  VT_FPoint  : sCast1 := 'Float32(';
  VT_DPoint  : sCast1 := 'Float64(';
  end;

  if(BetweenInc(strtoint(edTlX.Text), Low(smallint), High(smallint)) and
     BetweenInc(strtoint(edTlY.Text), Low(smallint), High(smallint)) and
     BetweenInc(strtoint(edBrX.Text), Low(smallint), High(smallint)) and
     BetweenInc(strtoint(edBrY.Text), Low(smallint), High(smallint)))
  then begin
    sReqRes := '''SPoint''';
    sCast2 := 'Int16(';
  end
  else begin
    sReqRes := '''IPoint''';
    sCast2 := 'Int32(';
  end;

  sExpression := 'range(gridset(' + sReference + ',' +
                 'point(' + sCast1 + edSfY.Text + '),' + sCast1 + edSfX.Text + '),' + sReference + '),' +
                 'point(' + sCast1 + ed0Y.Text + '),' + sCast1 + ed0X.Text + '),' + sReference + '),' +
                 sReqRes + '),' +
                 'point(' + sCast2 + edTlY.Text + '),' + sCast2 + edTlX.Text + ')),' +
                 'point(' + sCast2 + edBrY.Text + '),' + sCast2 + edBrX.Text + ')))';

  Result := ParseExpression(sExpression, vtResult);

  if(Result)
  then
    sExpr := sExpression;
end;

function TfrmUnitComposer.SubsetCreateExpression(var sExpr: ItemString; var vtResult: TValueType): boolean;
var
  sExpression : ItemString;
begin
  sExpr := '';
  vtResult := nil;

  sExpression := 'Subset(' + TreeItem_GetFullName_Save(TTreeItem(edSubsetFilter.Tag)) + ')';

  Result := ParseExpression(sExpression, vtResult);

  if(Result)
  then
    sExpr := sExpression;
end;

function TfrmUnitComposer.UniqueCreateExpression(var sExpr: ItemString; var vtResult: TValueType): boolean;
var
  sExpression : ItemString;
begin
  sExpr := '';
  vtResult := nil;

  sExpression := 'unique(' + TreeItem_GetFullName_Save(TTreeItem(edUniqueFilter.Tag)) + ')';

  Result := ParseExpression(sExpression, vtResult);

  if(Result)
  then
    sExpr := sExpression;
end;

function TfrmUnitComposer.CombineCreateExpression(var sExpr: ItemString; var vtResult: TValueType): boolean;
var
  sExpression : ItemString;
  nIdx, nCnt : integer;
begin
  sExpr := '';
  vtResult := nil;

  nCnt := dlbCombine.DstList.Items.Count;

  sExpression := 'Combine(' + TreeItem_GetFullName_Save(dlbCombine.DstList.Items.Objects[0]);

  for nIdx := 1 to nCnt - 1 do
    sExpression := sExpression + ',' + TreeItem_GetFullName_Save(dlbCombine.DstList.Items.Objects[nIdx]);

  sExpression := sExpression + ')';

  Result := ParseExpression(sExpression, vtResult);

  if(Result)
  then
    sExpr := sExpression;
end;

function TfrmUnitComposer.UnionCreateExpression(var sExpr: ItemString; var vtResult: TValueType): boolean;
var
  sExpression : ItemString;
  nIdx, nCnt : integer;
begin
  Result := false;
  sExpr := '';
  vtResult := nil;

  nCnt := dlbUnion.DstList.Items.Count;

  if(nCnt > 2)
  then begin
    DispMessage('At this time it is not possible to do a union of more than 2 dataitems.');
    Exit;
  end;

  sExpression := 'union(' + TreeItem_GetFullName_Save(dlbUnion.DstList.Items.Objects[0]);

  for nIdx := 1 to nCnt - 1 do
    sExpression := sExpression + ',' + TreeItem_GetFullName_Save(dlbUnion.DstList.Items.Objects[nIdx]);

  sExpression := sExpression + ')';

  Result := ParseExpression(sExpression, vtResult);

  if(Result)
  then
    sExpr := sExpression;
end;

function TfrmUnitComposer.CreateCountItem(uDomain: TAbstrUnit): boolean;
var
  tiUInt32Unit : TAbstrUnit;
  diNew        : TAbstrDataItem;
  sExpr        : ItemString;
begin
  Result := false;

  tiUInt32Unit := DMS_GetDefaultUnit(DMS_UInt32Unit_GetStaticClass);

  diNew := DMS_CreateDataItem(uDomain, 'Count', uDomain, tiUInt32Unit, VC_Single);

  if(diNew <> nil)
  then begin
    sExpr := 'count(' + TreeItem_GetFullName_Save(TTreeItem(edUniqueFilter.Tag)) +
             ', rlookup(' + TreeItem_GetFullName_Save(TTreeItem(edUniqueFilter.Tag)) + ', Values))';
    DMS_TreeItem_SetExpr(diNew, PItemChar(sExpr));
    Result := true;
  end;
end;

function TfrmUnitComposer.CreateDistMatrXItem(uDomain: TAbstrUnit): boolean;
var
  uUInt32 : TAbstrUnit;
  diNew   : TAbstrDataItem;
  sExpr   : ItemString;
begin
  Result := false;

  uUInt32 := DMS_GetDefaultUnit(DMS_UInt32Unit_GetStaticClass);

  diNew := DMS_CreateDataItem(uDomain, 'DistMatrX', uDomain, uUInt32, VC_Single);

  if(diNew <> nil)
  then begin
    sExpr := 'dist2(point(Int16(0),Int16(0),' + TreeItem_GetFullName_Save(uDomain) + '),' + lowercase(DMS_ValueType_GetName(DMS_Unit_GetValueType(uUInt32))) + ')';
    DMS_TreeItem_SetExpr(diNew, PItemChar(sExpr));
    Result := true;
  end;
end;

function TfrmUnitComposer.CreateAbsWeightItem(uDomain: TAbstrUnit): boolean;
var
  uValues : TAbstrUnit;
  diNew   : TAbstrDataItem;
  sExpr   : ItemString;
  sCast   : ItemString;
begin
  Result := false;

  uValues := DMS_GetDefaultUnit(DMS_UnitClass_Find(cbxSubitemsType.Items.Objects[cbxSubitemsType.ItemIndex]));

  diNew := DMS_CreateDataItem(uDomain, 'AbsWeight', uDomain, uValues, VC_Single);

  if(diNew <> nil)
  then begin
    if DMS_ValueType_GetID(cbxSubitemsType.Items.Objects[cbxSubitemsType.ItemIndex]) = VT_Float32
    then
      sCast := 'Float32('
    else
      sCast := 'Float64(';

    sExpr := 'iif(DistMatrX <  ' + inttostr(strtoint(edFPDist.Text) * strtoint(edFPDist.Text)) + ',' + sCast + '1.0) / ' + sCast + 'DistMatrX+2),' + sCast + '0.0))';
    DMS_TreeItem_SetExpr(diNew, PItemChar(sExpr));
    Result := true;
  end;
end;

function TfrmUnitComposer.CreateRelWeightItem(uDomain: TAbstrUnit): boolean;
var
  uValues : TAbstrUnit;
  diNew   : TAbstrDataItem;
  sExpr   : ItemString;
  sCast   : ItemString;
begin
  Result := false;

  uValues := DMS_GetDefaultUnit(DMS_UnitClass_Find(cbxSubitemsType.Items.Objects[cbxSubitemsType.ItemIndex]));

  diNew := DMS_CreateDataItem(uDomain, 'RelWeight', uDomain, uValues, VC_Single);

  if(diNew <> nil)
  then begin
    if(DMS_ValueType_GetID(cbxSubitemsType.Items.Objects[cbxSubitemsType.ItemIndex]) = VT_Float32)
    then
      sCast := 'Float32('
    else
      sCast := 'Float64(';

    sExpr := 'scalesum(AbsWeight,' + sCast + '1.0))';
    DMS_TreeItem_SetExpr(diNew, PItemChar(sExpr));
    Result := true;
  end;
end;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                              DISPLAY FUNCTIONS                             //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

procedure TfrmUnitComposer.DisplaySelect;
  function IsReassignRequired: Boolean;
  begin
    Result := False;
    if(rbTableUnit.Checked and not rbTableUnit.Enabled)        then Result := True;
    if(rbGridUnit.Checked and not rbGridUnit.Enabled)          then Result := True;
    if(rbQuantityUnit.Checked and not rbQuantityUnit.Enabled)  then Result := True;
    if(rbPointUnit.Checked and not rbPointUnit.Enabled)        then Result := True;
    if(rbShapeUnit.Checked and not rbShapeUnit.Enabled)        then Result := True;
    if(rbCodeUnit.Checked and not rbCodeUnit.Enabled)          then Result := True;
    if(rbStringUnit.Checked and not rbStringUnit.Enabled)      then Result := True;
    if(rbBoolUnit.Checked and not rbBoolUnit.Enabled)          then Result := True;
    if(rbFocalPointMatrix.Checked and not rbFocalPointMatrix.Enabled) then Result := True;
    if(rbAdvancedUnit.Checked and not rbAdvancedUnit.Enabled)  then Result := True;
  end;

  procedure DoReassign;
  begin
    if rbTableUnit.Enabled             then rbTableUnit.Checked := True
    else if rbGridUnit.Enabled         then rbGridUnit.Checked := True
    else if rbQuantityUnit.Enabled     then rbQuantityUnit.Checked := True
    else if rbPointUnit.Enabled        then rbPointUnit.Checked := True
    else if rbShapeUnit.Enabled        then rbShapeUnit.Checked := True
    else if rbCodeUnit.Enabled         then rbCodeUnit.Checked := True
    else if rbStringUnit.Enabled       then rbStringUnit.Checked := True
    else if rbBoolUnit.Enabled         then rbBoolUnit.Checked := True
    else if rbFocalPointMatrix.Enabled then rbFocalPointMatrix.Checked := True
    else if rbAdvancedUnit.Enabled     then rbAdvancedUnit.Checked := True;
  end;

begin
  m_ucsActive := ucsSelect;
  pcMain.ActivePage := tsSelectType;

  m_procApply := ApplySelect;
  m_funEnable := EnableOKSelect;

  // ensure the currently checked option is actually enabled
  if IsReassignRequired then
    DoReassign;
end;

procedure TfrmUnitComposer.DisplayTableUnit;
var
  nIdx, nCnt : integer;
begin
  // fill the type box
  if(cbxTableBaseType.Items.Count = 0)
  then begin
    cbxTableBaseType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_UInt32))), DMS_GetValueType(VT_UInt32));
    cbxTableBaseType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_UInt16))), DMS_GetValueType(VT_UInt16));
    cbxTableBaseType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_UInt8))), DMS_GetValueType(VT_UInt8));
  end;

  // fill the combine duallistbox
  if(dlbCombine.SrcList.Items.Count + dlbCombine.DstList.Items.Count = 0)
  then begin
    m_lstDiv.Clear;
    GetUnitsOfType([VT_UInt32], TConfiguration.CurrentConfiguration.UnitContainers, m_lstDiv); //for now these are not supported by the DMS: VT_UInt16, VT_UInt8
    nCnt := m_lstDiv.Count;
    for nIdx := 0 to nCnt - 1 do
    begin
      dlbCombine.SrcList.Items.AddObject(DMS_TreeItem_GetName(m_lstDiv.Items[nIdx]), m_lstDiv.Items[nIdx]);
    end;
  end;

  m_ucsActive := ucsTableUnit;
  PlaceGeneralControls(tsTableUnit);
  pcMain.ActivePage := tsTableUnit;
  m_procApply := ApplyTableUnit;
  m_funEnable := EnableOKTableUnit;

  // SETTING DEFAULTS

  // set the default type
  if(m_vidTUBaseUnitType <> VT_Unknown)
  then cbxTableBaseType.ItemIndex := cbxTableBaseType.Items.IndexOf(lowercase(DMS_ValueType_GetName(DMS_GetValueType(m_vidTUBaseUnitType))));
  if(m_nTUBaseUnitCount > 0)
  then edTableCount.Text := inttostr(m_nTUBaseUnitCount);
end;

procedure TfrmUnitComposer.DisplayGridUnit;
var
  nIdx, nCnt : integer;
begin
  // fill the reference box
  if(cbxGridReferenceUnit.Items.Count = 0)
  then begin
    m_lstDiv.Clear;
    GetUnitsOfType(
      [VT_SPoint, VT_IPoint, VT_FPoint, VT_DPoint],
      TConfiguration.CurrentConfiguration.GeographyContainer,
//      UnitContainers,
      m_lstDiv
    );
    nCnt := m_lstDiv.Count;
    for nIdx := 0 to nCnt - 1 do
      cbxGridReferenceUnit.Items.AddObject(DMS_TreeItem_GetName(m_lstDiv.Items[nIdx]), m_lstDiv.Items[nIdx]);
  end;

  m_ucsActive := ucsGridUnit;
  PlaceGeneralControls(tsGridUnit);
  pcMain.ActivePage := tsgridUnit;
  m_procApply := ApplyGridUnit;
  m_funEnable := EnableOKGridUnit;
end;

procedure TfrmUnitComposer.DisplayQuantityUnit;
begin
  // fill the type box
  if(cbxQuantityType.Items.Count = 0)
  then begin
    cbxQuantityType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_Float32))), DMS_GetValueType(VT_Float32));
    cbxQuantityType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_Float64))), DMS_GetValueType(VT_Float64));
  end;

  m_ucsActive := ucsQuantityUnit;
  PlaceGeneralControls(tsQuantityUnit);
  pcMain.ActivePage := tsQuantityUnit;
  m_procApply := ApplyQuantityUnit;
  m_funEnable := EnableOKQuantityUnit;
end;

procedure TfrmUnitComposer.DisplayPointUnit;
begin
  // fill the type box
  if(cbxPointType.Items.Count = 0)
  then begin
    cbxPointType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_SPoint))), DMS_GetValueType(VT_SPoint));
    cbxPointType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_IPoint))), DMS_GetValueType(VT_IPoint));
    cbxPointType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_FPoint))), DMS_GetValueType(VT_FPoint));
    cbxPointType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_DPoint))), DMS_GetValueType(VT_DPoint));
  end;

  m_ucsActive := ucsPointUnit;
  PlaceGeneralControls(tsPointUnit);
  pcMain.ActivePage := tsPointUnit;
  m_procApply := ApplyPointUnit;
  m_funEnable := EnableOKPointUnit;
end;

procedure TfrmUnitComposer.DisplayShapeUnit;
begin
  // fill the type box
  if(cbxShapeType.Items.Count = 0)
  then begin
    cbxShapeType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_FPoint))), DMS_GetValueType(VT_FPoint));
    cbxShapeType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_DPoint))), DMS_GetValueType(VT_DPoint));
    cbxShapeType.ItemIndex := 0;
  end;

  m_ucsActive := ucsShapeUnit;
  PlaceGeneralControls(tsShapeUnit);
  pcMain.ActivePage := tsShapeUnit;
  m_procApply := ApplyShapeUnit;
  m_funEnable := EnableOKShapeUnit;
end;

procedure TfrmUnitComposer.DisplayCodeUnit;
begin
  // fill the type box
  if(cbxCodeType.Items.Count = 0)
  then begin
    cbxCodeType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_UInt32))), DMS_GetValueType(VT_UInt32));
    cbxCodeType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_Int32))), DMS_GetValueType(VT_Int32));
    cbxCodeType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_UInt16))), DMS_GetValueType(VT_UInt16));
    cbxCodeType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_Int16))), DMS_GetValueType(VT_Int16));
    cbxCodeType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_UInt8))), DMS_GetValueType(VT_UInt8));
    cbxCodeType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_Int8))), DMS_GetValueType(VT_Int8));
    cbxCodeType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_String))), DMS_GetValueType(VT_String));
  end;

  m_ucsActive := ucsCodeUnit;
  PlaceGeneralControls(tsCodeUnit);
  pcMain.ActivePage := tsCodeUnit;
  m_procApply := ApplyCodeUnit;
  m_funEnable := EnableOKCodeUnit;
end;

procedure TfrmUnitComposer.DisplayStringUnit;
begin
  m_ucsActive := ucsStringUnit;
  PlaceGeneralControls(tsStringUnit);
  pcMain.ActivePage := tsStringUnit;
  m_procApply := ApplyStringUnit;
  m_funEnable := EnableOKStringUnit;
end;

procedure TfrmUnitComposer.DisplayBoolUnit;
begin
  m_ucsActive := ucsBoolUnit;
  PlaceGeneralControls(tsBoolUnit);
  pcMain.ActivePage := tsBoolUnit;
  m_procApply := ApplyBoolUnit;
  m_funEnable := EnableOKBoolUnit;
end;

procedure TfrmUnitComposer.DisplayFocalPointMatrix;
begin
  // fill the type box
  if(cbxSubitemsType.Items.Count = 0)
  then begin
    cbxSubitemsType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_Float32))), DMS_GetValueType(VT_Float32));
    cbxSubitemsType.Items.AddObject(lowercase(DMS_ValueType_GetName(DMS_GetValueType(VT_Float64))), DMS_GetValueType(VT_Float64));
  end;

  m_ucsActive := ucsFocalPointMatrix;
  PlaceGeneralControls(tsFocalPointMatrix);
  pcMain.ActivePage := tsFocalPointMatrix;
  m_procApply := ApplyFocalPointMatrix;
  m_funEnable := EnableOKFocalPointMatrix;
end;

procedure TfrmUnitComposer.DisplayAdvancedUnit;
var
  vtiIdx : TValueTypeId;
  vtCur  : TValueType;
begin
  // fill type box
  if(cbxValueType.Items.Count = 0)
  then begin
    for vtiIdx := Low(TValueTypeId) to High(TValueTypeId) do
    begin
      vtCur := DMS_GetValueType(vtiIdx);
      if(vtCur <> nil)
      then
        cbxValueType.Items.AddObject(lowercase(DMS_ValueType_GetName(vtCur)), vtCur);
    end;
  end;

  m_ucsActive := ucsAdvancedUnit;
  PlaceGeneralControls(tsAdvancedUnit);
  pcMain.ActivePage := tsAdvancedUnit;
  m_procApply := ApplyAdvancedUnit;
  m_funEnable := EnableOKAdvancedUnit;
end;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                              APPLY FUNCTIONS                               //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

procedure TfrmUnitComposer.ApplySelect;
begin
  assert(not DMS_Actor_DidSuspend);
  case m_ucsSelected of
  ucsSelect:
    DisplaySelect;
  ucsTableUnit:
    DisplayTableUnit;
  ucsGridUnit:
    DisplayGridUnit;
  ucsQuantityUnit:
    DisplayQuantityUnit;
  ucsPointUnit:
    DisplayPointUnit;
  ucsShapeUnit:
    DisplayShapeUnit;
  ucsCodeUnit:
    DisplayCodeUnit;
  ucsStringUnit:
    DisplayStringUnit;
  ucsBoolUnit:
    DisplayBoolUnit;
  ucsFocalPointMatrix:
    DisplayFocalPointMatrix;
  ucsAdvancedUnit:
    DisplayAdvancedUnit;
  end;

  SetButtonStates;
end;

procedure TfrmUnitComposer.ApplyTableUnit;
var
  uNew : TAbstrUnit;
  sExpr : ItemString;
  vtNew : TValueType;
begin
  if(m_uUnit = nil)
  then begin
    // create unit
    if(rbTableBaseUnit.Checked)
    then begin
      vtNew := cbxTableBaseType.Items.Objects[cbxTableBaseType.ItemIndex];
    end
    else if(rbSubsetUnit.Checked)
    then begin
      if(not SubsetCreateExpression(sExpr, vtNew))
      then
        Exit;
    end
    else if(rbUnique.Checked)
    then begin
      if(not UniqueCreateExpression(sExpr, vtNew))
      then
        Exit;
    end
    else if(rbCombine.Checked)
    then begin
      if(not CombineCreateExpression(sExpr, vtNew))
      then
        Exit;
    end
    else if(rbUnion.Checked)
    then begin
      if(not UnionCreateExpression(sExpr, vtNew))
      then
        Exit;
    end;

    uNew := CreateNewUnit(vtNew);
    if(uNew = nil)
    then
      Exit;

    // set expression, etc.
    if(rbTableBaseUnit.Checked)
    then begin
      DMS_NumericUnit_SetRangeAsFloat64(uNew, 0, strtoint(edTableCount.Text));
    end
    else if(rbSubsetUnit.Checked)
    then begin
      DMS_TreeItem_SetExpr(uNew, PItemChar(sExpr));
    end
    else if(rbUnique.Checked)
    then begin
      DMS_TreeItem_SetExpr(uNew, PItemChar(sExpr));

      if(chbCount.Checked)
      then
        CreateCountItem(uNew);
    end
    else if(rbCombine.Checked)
    then begin
      DMS_TreeItem_SetExpr(uNew, PItemChar(sExpr));
    end
    else if(rbUnion.Checked)
    then begin
      DMS_TreeItem_SetExpr(uNew, PItemChar(sExpr));
    end;

    Unit_ := uNew;

    ModalResult := mrOK;
  end;
end;

procedure TfrmUnitComposer.ApplyGridUnit;
var
  uNew : TAbstrUnit;
  sExpr : ItemString;
  vtNew : TValueType;
begin
  if(m_uUnit = nil)
  then begin
    // create unit
    if(rbGridBaseUnit.Checked)
    then begin
      if(BetweenInc(strtoint(edBaseTlX.Text), Low(smallint), High(smallint)) and
         BetweenInc(strtoint(edBaseTlY.Text), Low(smallint), High(smallint)) and
         BetweenInc(strtoint(edBaseBrX.Text), Low(smallint), High(smallint)) and
         BetweenInc(strtoint(edBaseBrY.Text), Low(smallint), High(smallint)))
      then
        vtNew := DMS_GetValueType(VT_SPoint)
      else
        vtNew := DMS_GetValueType(VT_IPoint);
    end
    else begin
      if(not GridsetCreateExpression(sExpr, vtNew))
      then
        Exit;
    end;

    uNew := CreateNewUnit(vtNew);
    if(uNew = nil)
    then
      Exit;

    // set expression
    if(rbGridBaseUnit.Checked)
    then
      DMS_GeometricUnit_SetRangeAsIPoint(uNew, strtoint(edBaseTlY.Text), strtoint(edBaseTlX.Text), strtoint(edBaseBrY.Text), strtoint(edBaseBrX.Text))
    else
      DMS_TreeItem_SetExpr(uNew, PItemChar(sExpr));

    Unit_ := uNew;

    ModalResult := mrOK;
  end;
end;

procedure TfrmUnitComposer.ApplyQuantityUnit;
var
  uNew : TAbstrUnit;
  sMetric : ItemString;
  vtNew   : TValueType;
begin
  if(m_uUnit = nil)
  then begin
    // create unit
    if(rbQuantityBaseUnit.Checked)
    then
      uNew := CreateNewUnit(cbxQuantityType.Items.Objects[cbxQuantityType.ItemIndex])
    else begin
      QuantityCheckExpression(sMetric, vtNew);
      uNew := CreateNewUnit(vtNew);
    end;

    if(uNew = nil)
    then
      Exit;

    // set expression
    if(rbQuantityBaseUnit.Checked)
    then
      DMS_TreeItem_SetExpr(uNew, PItemChar(ItemString('BaseUnit(''' + edName.Text + ''', ''' + cbxQuantityType.Text + ''')')))
    else
      DMS_TreeItem_SetExpr(uNew, PItemChar(ItemString(mmQuantityExpression.Text)));

     DMS_Unit_SetFormat(uNew, PSourceChar(SourceString(edQuantityFormat.Text)));

    Unit_ := uNew;

    ModalResult := mrOK;
  end;
end;

procedure TfrmUnitComposer.ApplyPointUnit;
var
  uNew : TAbstrUnit;
begin
  if(m_uUnit = nil)
  then begin
    uNew := CreateNewUnit(cbxPointType.Items.Objects[cbxPointType.ItemIndex]);

    if(uNew <> nil)
    then begin
      if(edPointTLX.Text + edPointTLY.Text + edPointBRX.Text + edPointBRY.Text <> '')
      then
        DMS_GeometricUnit_SetRangeAsDPoint(uNew, strtofloat(edPointTLY.Text), strtofloat(edPointTLX.Text), strtofloat(edPointBRY.Text), strtofloat(edPointBRX.Text));

      Unit_ := uNew;

      ModalResult := mrOK;
    end;
  end;
end;

procedure TfrmUnitComposer.ApplyShapeUnit;
var
  uNew : TAbstrUnit;
begin
  if(m_uUnit = nil)
  then begin
    uNew := CreateNewUnit(cbxShapeType.Items.Objects[cbxShapeType.ItemIndex]);

    if(uNew <> nil)
    then begin
      if(edShapeTLX.Text + edShapeTLY.Text + edShapeBRX.Text + edShapeBRY.Text <> '')
      then
        DMS_GeometricUnit_SetRangeAsDPoint(uNew, strtofloat(edShapeTLY.Text), strtofloat(edShapeTLX.Text), strtofloat(edShapeBRY.Text), strtofloat(edShapeBRX.Text));

      DMS_Unit_SetFormat(uNew, PSourceChar(SourceString(cbxShapeFormat.Text)));

      Unit_ := uNew;

      ModalResult := mrOK;
    end;
  end;
end;

procedure TfrmUnitComposer.ApplyCodeUnit;
var
  uNew : TAbstrUnit;
begin
  if(m_uUnit = nil)
  then begin
    uNew := CreateNewUnit(cbxCodeType.Items.Objects[cbxCodeType.ItemIndex]);

    if(uNew <> nil)
    then begin
      if(edCodeFrom.Text + edCodeTo.Text <> '')
      then
        DMS_NumericUnit_SetRangeAsFloat64(uNew, strtofloat(edCodeFrom.Text), strtofloat(edCodeTo.Text));

      Unit_ := uNew;

      ModalResult := mrOK;
    end;
  end;
end;

//**
procedure TfrmUnitComposer.ApplyStringUnit;
var
  uNew : TAbstrUnit;
begin
  if(m_uUnit = nil)
  then begin
    uNew := CreateNewUnit(DMS_GetValueType(VT_String));
    if(uNew <> nil)
    then begin
      Unit_ := uNew;
      ModalResult := mrOK;
    end;
  end;
end;

//**
procedure TfrmUnitComposer.ApplyBoolUnit;
var
  uNew : TAbstrUnit;
begin
  if(m_uUnit = nil)
  then begin
    uNew := CreateNewUnit(DMS_GetValueType(VT_Bool));
    if(uNew <> nil)
    then begin
      Unit_ := uNew;
      ModalResult := mrOK;
    end;
  end;
end;

procedure TfrmUnitComposer.ApplyFocalPointMatrix;
var
  uNew : TAbstrUnit;
begin
  if(m_uUnit = nil)
  then begin
    uNew := CreateNewUnit(DMS_GetValueType(VT_SPoint));

    if(uNew <> nil)
    then begin
      DMS_GeometricUnit_SetRangeAsDPoint(uNew, -strtoint(edFPDist.Text), -strtoint(edFPDist.Text), strtoint(edFPDist.Text) + 1, strtoint(edFPDist.Text) + 1);

      CreateDistMatrXItem(uNew);
      CreateAbsWeightItem(uNew);
      CreateRelWeightItem(uNew);

      Unit_ := uNew;

      ModalResult := mrOK;
    end;
  end;
end;

procedure TfrmUnitComposer.ApplyAdvancedUnit;
begin
// uNew := DMS_CreateUnit(m_tiContainer, PChar(edName.Text), DMS_UnitClass_Find(cbxValueType.Items.Objects[cbxValueType.ItemIndex]));
end;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                             ENABLE FUNCTIONS                               //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function TfrmUnitComposer.EnableOKSelect: boolean;
begin
  Result :=
    rbTableUnit.Checked or
    rbGridUnit.Checked or
    rbQuantityUnit.Checked or
    rbPointUnit.Checked or
    rbShapeUnit.Checked or
    rbCodeUnit.Checked or
    rbStringUnit.Checked or
    rbBoolUnit.Checked or
    rbFocalPointMatrix.Checked or
    rbAdvancedUnit.Checked;
end;

function TfrmUnitComposer.EnableOKTableUnit: boolean;
var
  nCount : integer;
begin
  Result := UnitNameOK;

  if(rbTableBaseUnit.Checked)
  then begin
    Result := Result and (cbxTableBaseType.ItemIndex >= 0) and (IsInteger(edTableCount.Text));
    if(Result)
    then begin
      nCount := strtoint(edTableCount.Text);
      case DMS_ValueType_GetID(cbxTableBaseType.Items.Objects[cbxTableBaseType.ItemIndex]) of
      VT_UInt32:
        Result := BetweenInc(nCount, 1, High(longword));
      VT_UInt16:
        Result := BetweenInc(nCount, 1, High(word));
      VT_UInt8:
        Result := BetweenInc(nCount, 1, High(byte));
      else
        Result := false;
      end;
    end;
  end
  else if(rbSubsetUnit.Checked)
  then begin
    Result := Result and (edSourceTU.Tag <> 0) and (edSubsetFilter.Tag <> 0);
  end
  else if(rbUnique.Checked)
  then begin
    Result := Result and (edUniqueSourceTU.Tag <> 0) and (edUniqueFilter.Tag <> 0);
  end
  else if(rbCombine.Checked)
  then begin
    Result := Result and (dlbCombine.DstList.Items.Count >= 2);
  end
  else if(rbUnion.Checked)
  then begin
    Result := Result and (dlbUnion.DstList.Items.Count >= 2);
  end
end;

function TfrmUnitComposer.EnableOKGridUnit: boolean;
begin
  Result := UnitNameOK;

  if(rbGridBaseUnit.Checked)
  then begin
    Result := Result and
              IsInteger(edBaseTlX.Text) and
              IsInteger(edBaseTlY.Text) and
              IsInteger(edBaseBrX.Text) and
              IsInteger(edBaseBrY.Text);
  end
  else begin
    Result := Result and
              (cbxGridReferenceUnit.ItemIndex >= 0) and
              IsFloat(edSfX.Text) and
              IsFloat(edSfY.Text) and
              IsFloat(ed0X.Text) and
              IsFloat(ed0Y.Text) and
              IsInteger(edTlX.Text) and
              IsInteger(edTlY.Text) and
              IsInteger(edBrX.Text) and
              IsInteger(edBrY.Text);
  end;
end;

function TfrmUnitComposer.EnableOKQuantityUnit: boolean;
begin
  Result := UnitNameOK;

  if(rbQuantityBaseUnit.Checked)
  then begin
    Result := Result and (cbxQuantityType.ItemIndex >= 0)
  end
  else begin
    Result := Result and m_bQuantityExpressionChecked;
  end;

  Result := Result and QuantityFormatOK;
end;

function TfrmUnitComposer.EnableOKPointUnit: boolean;
var
  nLow, nHigh : int64;
  fLow, fHigh : extended;
begin
  Result := UnitNameOK;

  if(Result)
  then begin
    // range is optional
    if(edPointTLX.Text + edPointTLY.Text + edPointBRX.Text + edPointBRY.Text = '')
    then
      Exit;

    case DMS_ValueType_GetID(cbxPointType.Items.Objects[cbxPointType.ItemIndex]) of
    VT_SPoint:
    begin
      Result := IsInteger(edPointTLX.Text) and IsInteger(edPointTLY.Text) and IsInteger(edPointBRX.Text) and IsInteger(edPointBRY.Text);
      if(Result)
      then begin
        nLow  := Low(smallint);
        nHigh := High(smallint);
        Result := BetweenInc(strtoint(edPointTLX.Text), nLow, nHigh) and
                  BetweenInc(strtoint(edPointTLY.Text), nLow, nHigh) and
                  BetweenInc(strtoint(edPointBRX.Text), nLow, nHigh) and
                  BetweenInc(strtoint(edPointBRY.Text), nLow, nHigh);
      end;
    end;
    VT_IPoint:
    begin
      Result := IsInteger(edPointTLX.Text) and IsInteger(edPointTLY.Text) and IsInteger(edPointBRX.Text) and IsInteger(edPointBRY.Text);
      if(Result)
      then begin
        nLow  := Low(longint);
        nHigh := High(longint);
        Result := BetweenInc(strtoint(edPointTLX.Text), nLow, nHigh) and
                  BetweenInc(strtoint(edPointTLY.Text), nLow, nHigh) and
                  BetweenInc(strtoint(edPointBRX.Text), nLow, nHigh) and
                  BetweenInc(strtoint(edPointBRY.Text), nLow, nHigh);
      end;
    end;
    VT_FPoint:
    begin
      Result := IsFloat(edPointTLX.Text) and IsFloat(edPointTLY.Text) and IsFloat(edPointBRX.Text) and IsFloat(edPointBRY.Text);
      if(Result)
      then begin
        fLow  := MinSingle;
        fHigh := MaxSingle;
        Result := BetweenInc(strtofloat(edPointTLX.Text), fLow, fHigh) and
                  BetweenInc(strtofloat(edPointTLY.Text), fLow, fHigh) and
                  BetweenInc(strtofloat(edPointBRX.Text), fLow, fHigh) and
                  BetweenInc(strtofloat(edPointBRY.Text), fLow, fHigh);
      end;
    end;
    VT_DPoint:
    begin
      Result := IsFloat(edPointTLX.Text) and IsFloat(edPointTLY.Text) and IsFloat(edPointBRX.Text) and IsFloat(edPointBRY.Text);
      if(Result)
      then begin
        fLow  := MinDouble;
        fHigh := MaxDouble;
        Result := BetweenInc(strtofloat(edPointTLX.Text), fLow, fHigh) and
                  BetweenInc(strtofloat(edPointTLY.Text), fLow, fHigh) and
                  BetweenInc(strtofloat(edPointBRX.Text), fLow, fHigh) and
                  BetweenInc(strtofloat(edPointBRY.Text), fLow, fHigh);
      end;
    end
    else
      Result := false;
    end;
  end
end;

function TfrmUnitComposer.EnableOKShapeUnit: boolean;
var
  fLow, fHigh : extended;
begin
  Result := UnitNameOK;

  if(Result)
  then begin
    // range is optional
    if(edShapeTLX.Text + edShapeTLY.Text + edShapeBRX.Text + edShapeBRY.Text = '')
    then
      Exit;

    case DMS_ValueType_GetID(cbxShapeType.Items.Objects[cbxShapeType.ItemIndex]) of
    VT_FPoint:
    begin
      Result := IsFloat(edShapeTLX.Text) and IsFloat(edShapeTLY.Text) and IsFloat(edShapeBRX.Text) and IsFloat(edShapeBRY.Text);
      if(Result)
      then begin
        fLow  := MinSingle;
        fHigh := MaxSingle;
        Result := BetweenInc(strtofloat(edShapeTLX.Text), fLow, fHigh) and
                  BetweenInc(strtofloat(edShapeTLY.Text), fLow, fHigh) and
                  BetweenInc(strtofloat(edShapeBRX.Text), fLow, fHigh) and
                  BetweenInc(strtofloat(edShapeBRY.Text), fLow, fHigh);
      end;
    end;
    VT_DPoint:
    begin
      Result := IsFloat(edShapeTLX.Text) and IsFloat(edShapeTLY.Text) and IsFloat(edShapeBRX.Text) and IsFloat(edShapeBRY.Text);
      if(Result)
      then begin
        fLow  := MinDouble;
        fHigh := MaxDouble;
        Result := BetweenInc(strtofloat(edShapeTLX.Text), fLow, fHigh) and
                  BetweenInc(strtofloat(edShapeTLY.Text), fLow, fHigh) and
                  BetweenInc(strtofloat(edShapeBRX.Text), fLow, fHigh) and
                  BetweenInc(strtofloat(edShapeBRY.Text), fLow, fHigh);
      end;
    end
    else
      Result := false;
    end;
  end
end;

function TfrmUnitComposer.EnableOKCodeUnit: boolean;
var vid: TValueTypeId;
begin
  Result := UnitNameOK and (cbxCodeType.ItemIndex >= 0);

  if not Result then Exit;

  if(edCodeFrom.Text + edCodeTo.Text = '') then Exit; // returns true

  vid :=  DMS_ValueType_GetID(cbxCodeType.Items.Objects[cbxCodeType.ItemIndex]);
  if vid = VT_String then Exit; // returns true;

  Result := IsInteger(edCodeFrom.Text) and IsInteger(edCodeTo.Text);
  if not Result then Exit; // returns false;

  case  vid of
  VT_UInt32: Result := BetweenInc(strtoint(edCodeFrom.Text), Low(longword), High(longword)) and BetweenInc(strtoint(edCodeTo.Text), Low(longword), High(longword));
  VT_Int32:  Result := BetweenInc(strtoint(edCodeFrom.Text), Low(longint),  High(longint))  and BetweenInc(strtoint(edCodeTo.Text), Low(longint),  High(longint));
  VT_UInt16: Result := BetweenInc(strtoint(edCodeFrom.Text), Low(word),     High(word))     and BetweenInc(strtoint(edCodeTo.Text), Low(word),     High(word));
  VT_Int16:  Result := BetweenInc(strtoint(edCodeFrom.Text), Low(smallint), High(smallint)) and BetweenInc(strtoint(edCodeTo.Text), Low(smallint), High(smallint));
  VT_UInt8:  Result := BetweenInc(strtoint(edCodeFrom.Text), Low(byte),     High(byte))     and BetweenInc(strtoint(edCodeTo.Text), Low(byte),     High(byte));
  VT_Int8:   Result := BetweenInc(strtoint(edCodeFrom.Text), Low(shortint), High(shortint)) and BetweenInc(strtoint(edCodeTo.Text), Low(shortint), High(shortint));
  else
    Result := false;
  end;
end;

function TfrmUnitComposer.EnableOKStringUnit: boolean;
begin
  Result := UnitNameOK;
end;

function TfrmUnitComposer.EnableOKBoolUnit: boolean;
begin
  Result := UnitNameOK;
end;

function TfrmUnitComposer.EnableOKFocalPointMatrix: boolean;
begin
  Result := UnitNameOK;

  Result := Result and IsInteger(edFPDist.Text) and BetweenInc(strtoint(edFPDist.Text), 0, High(smallint) - 1);

  Result := Result and (cbxSubitemsType.ItemIndex >= 0);
end;

function TfrmUnitComposer.EnableOKAdvancedUnit: boolean;
begin
  Result := UnitNameOK;
end;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                              PROPERTY FUNCTIONS                            //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

procedure TfrmUnitComposer.SetContainer(tiValue: TTreeItem);
var newContext: TTreeItem;
begin
  if(m_tiContainer = tiValue) then exit;

  newContext := nil;

  SetCountedRef(m_tiContainer, tiValue);

  if Assigned(m_tiContainer)
  then begin
    if DMS_TreeItem_HasSubItems(m_tiContainer)
    then
      newContext := DMS_TreeItem_GetLastSubItem(m_tiContainer)
    else
      newContext := m_tiContainer;
  end;
  SetCountedRef(m_tiContext, newContext);
end;

procedure TfrmUnitComposer.SetTUBaseUnitType(const vidValue: TValueTypeId);
begin
  if(vidValue in [VT_UInt32, VT_UInt16, VT_UInt8])
  then m_vidTUBaseUnitType := vidValue;
end;

procedure TfrmUnitComposer.SetUnit(uValue: TAbstrUnit);
begin
  if(m_uUnit <> uValue)
  then begin
    if (uValue = nil) or Assigned(DMS_TreeItem_QueryInterface(uValue, DMS_AbstrUnit_GetStaticClass))
    then
      SetCountedRef(m_uUnit, uValue)
    else
      raise Exception.Create('AbstrUnit required.');
  end;
end;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                               EVENT HANDLERS                               //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

procedure TfrmUnitComposer.chbBaseUnitClick(Sender: TObject);
begin
  mmExpression.Enabled := not chbBaseUnit.Checked;
end;

procedure TfrmUnitComposer.btnOK_NextClick(Sender: TObject);
begin
  m_procApply;
end;

procedure TfrmUnitComposer.btnBackClick(Sender: TObject);
begin
  DisplaySelect;
  SetButtonStates;
end;

procedure TfrmUnitComposer.NumberRangeClick(Sender: TObject);
begin
  if(rbNumberItems.Checked)
  then begin
    seNumberItems.Enabled := true;
    edFrom.Enabled        := false;
    edTo.Enabled          := false;
  end
  else begin
    seNumberItems.Enabled := false;
    edFrom.Enabled        := true;
    edTo.Enabled          := true;
  end;
end;

procedure TfrmUnitComposer.DataChanged(Sender: TObject);
begin
  SetButtonStates;
end;

procedure TfrmUnitComposer.SelectUnitType(Sender: TObject);
begin
  if(rbTableUnit.Checked)
  then begin
    m_ucsSelected := ucsTableUnit;
  end
  else if(rbGridUnit.Checked)
  then begin
    m_ucsSelected := ucsGridUnit;
  end
  else if(rbQuantityUnit.Checked)
  then begin
    m_ucsSelected := ucsQuantityUnit;
  end
  else if(rbPointUnit.Checked)
  then begin
    m_ucsSelected := ucsPointUnit;
  end
  else if(rbShapeUnit.Checked)
  then begin
    m_ucsSelected := ucsShapeUnit;
  end
  else if(rbCodeUnit.Checked)
  then begin
    m_ucsSelected := ucsCodeUnit;
  end
  else if(rbStringUnit.Checked)
  then begin
    m_ucsSelected := ucsStringUnit;
  end
  else if(rbBoolUnit.Checked)
  then begin
    m_ucsSelected := ucsBoolUnit;
  end
  else if(rbFocalPointMatrix.Checked)
  then begin
    m_ucsSelected := ucsFocalPointMatrix;
  end
  else if(rbAdvancedUnit.Checked)
  then begin
    m_ucsSelected := ucsAdvancedUnit;
  end;

  reDescription.Lines.Text := c_arUnitDescriptions[m_ucsSelected];

  SetButtonStates;
end;

procedure TfrmUnitComposer.QuantitySelectBaseUnit(Sender: TObject);
begin
  cbxQuantityType.Enabled      := rbQuantityBaseUnit.Checked;
  mmQuantityExpression.Enabled := not rbQuantityBaseUnit.Checked;
  btnCheck.Enabled             := not rbQuantityBaseUnit.Checked;
  SetButtonStates;
end;

procedure TfrmUnitComposer.btnCheckClick(Sender: TObject);
var
  sMetric : ItemString;
  vtExpr  : TValueType;
begin
  m_bQuantityExpressionChecked := QuantityCheckExpression(sMetric, vtExpr);
  if(m_bQuantityExpressionChecked)
  then begin
    lblMetric.Caption := sMetric;
    dmfGeneral.SetButtonImage(btnCheck, 1);
  end
  else
    dmfGeneral.SetButtonImage(btnCheck, 2);

  SetButtonStates;
end;

procedure TfrmUnitComposer.mmQuantityExpressionChange(Sender: TObject);
begin
  m_bQuantityExpressionChecked := false;
  dmfGeneral.SetButtonImage(btnCheck, 0);
  SetButtonStates;
end;

procedure TfrmUnitComposer.GridSelectBaseUnit(Sender: TObject);
begin
  edBaseTlX.Enabled   := rbGridBaseUnit.Checked;
  edBaseBrX.Enabled   := rbGridBaseUnit.Checked;
  edBaseBrY.Enabled   := rbGridBaseUnit.Checked;
  edBaseTlY.Enabled   := rbGridBaseUnit.Checked;

  cbxGridReferenceUnit.Enabled := not rbGridBaseUnit.Checked;
  edSfX.Enabled := not rbGridBaseUnit.Checked;
  edSfY.Enabled := not rbGridBaseUnit.Checked;
  ed0X.Enabled  := not rbGridBaseUnit.Checked;
  ed0Y.Enabled  := not rbGridBaseUnit.Checked;
  edTlX.Enabled := not rbGridBaseUnit.Checked;
  edTlY.Enabled := not rbGridBaseUnit.Checked;
  edBrX.Enabled := not rbGridBaseUnit.Checked;
  edBrY.Enabled := not rbGridBaseUnit.Checked;

  SetButtonStates;
end;

procedure TfrmUnitComposer.edSourceTUChange(Sender: TObject);
begin
  edSubsetFilter.Tag := 0;
  edSubsetFilter.Text := '';

  SetButtonStates;
end;

procedure TfrmUnitComposer.TableSelectStyle(Sender: TObject);
begin
  cbxTableBaseType.Enabled := rbTableBaseUnit.Checked;
  edTableCount.Enabled     := rbTableBaseUnit.Checked;

  edSourceTU.Enabled     := rbSubsetUnit.Checked;
  sbSourceTU.Enabled     := rbSubsetUnit.Checked;
  edSubsetFilter.Enabled := rbSubsetUnit.Checked;
  sbSubsetFilter.Enabled := rbSubsetUnit.Checked;

  edUniqueSourceTU.Enabled := rbUnique.Checked;
  sbUniqueSourceTU.Enabled := rbUnique.Checked;
  edUniqueFilter.Enabled   := rbUnique.Checked;
  sbUniqueFilter.Enabled   := rbUnique.Checked;
  chbCount.Enabled         := rbUnique.Checked;

  dlbCombine.Enabled := rbCombine.Checked;

  edTargetVU.Enabled := rbUnion.Checked;
  sbTargetVU.Enabled := rbUnion.Checked;
  dlbUnion.Enabled   := rbUnion.Checked;

  SetButtonStates;
end;

procedure TfrmUnitComposer.edUniqueSourceTUChange(Sender: TObject);
begin
  edUniqueFilter.Tag := 0;
  edUniqueFilter.Text := '';

  SetButtonStates;
end;

procedure TfrmUnitComposer.edTargetVUChange(Sender: TObject);
var
  nIdx, nCnt : integer;
  uSelected, UDomain : TAbstrUnit;
  diCurr    : TAbstrDataItem;
begin
  dlbUnion.SrcList.Clear;
  dlbUnion.DstList.Clear;

  uSelected := TTreeItem(edTargetVU.Tag);
  nCnt := DMS_Unit_GetNrDataItemsIn(uSelected);

  for nIdx := 0 to nCnt - 1 do
  begin
    diCurr := DMS_Unit_GetDataItemIn(uSelected, nIdx);
    if not assigned(diCurr) then continue;
    uDomain := DMS_DataItem_GetDomainUnit(diCurr);
    if  (not DMS_TreeItem_InTemplate(diCurr))  then
      dlbUnion.AddObject(DMS_TreeItem_GetName(diCurr) + ' [' + DMS_TreeItem_GetName(uDomain) + ']', diCurr);
  end;

  SetButtonStates;
end;

procedure TfrmUnitComposer.cbxCodeTypeChange(Sender: TObject);
begin
  if(DMS_ValueType_GetID(cbxCodeType.Items.Objects[cbxCodeType.ItemIndex]) = VT_String)
  then begin
    edCodeFrom.Enabled := false;
    edCodeTo.Enabled   := false;
  end
  else begin
    edCodeFrom.Enabled := true;
    edCodeTo.Enabled   := true;
  end;

  SetButtonStates;
end;

procedure TfrmUnitComposer.ShowTabSheet(Sender: TObject);
begin
  edName.SetFocus;
end;

procedure TfrmUnitComposer.tsSelectTypeShow(Sender: TObject);
begin
   SelectUnitType(nil);
end;

procedure TfrmUnitComposer.sbUniqueFilterClick(Sender: TObject);
var
  nCnt, nIdx : integer;
  diCurr  : TAbstrDataItem;
  uDomain : TAbstrUnit;
begin
  uDomain := TTreeItem(edUniqueSourceTU.Tag);

  if(uDomain = nil)
  then nCnt := 0
  else nCnt := DMS_Unit_GetNrDataItemsOut(uDomain);

  m_lstDiv.Clear;
  for nIdx := 0 to nCnt - 1 do
  begin
    diCurr := DMS_Unit_GetDataItemOut(uDomain, nIdx);
    if not assigned(diCurr) then continue;
    if GetValuesUnitValueTypeID(diCurr)
    in [VT_UInt32, VT_Int32, VT_UInt16, VT_Int16, VT_UInt8, VT_Int8, VT_String]
    then m_lstDiv.Add(diCurr);
  end;

  with TfrmTreeItemPickList.Create(nil) do
  try
    AddColumn(TFullNameColumn.Create);
    AddColumn(TValuesUnitColumn.Create);

    AddRows(m_lstDiv, TTreeItem(edUniqueFilter.Tag));
    if Execute then
    begin
      edUniqueFilter.Tag  := integer(SelectedTreeItem);
      edUniqueFilter.Text := DMS_TreeItem_GetName(SelectedTreeItem);
      edUniqueFilter.Hint := TreeItem_GetFullName_Save(SelectedTreeItem);
    end;
  finally
    Free;
  end;
end;

procedure TfrmUnitComposer.sbUniqueSourceTUClick(Sender: TObject);
begin
  m_lstDiv.Clear;
  GetUnitsOfType([VT_UInt32], TConfiguration.CurrentConfiguration.UnitContainers, m_lstDiv); //for now these are not supported by the DMS: VT_UInt16, VT_UInt8

  with TfrmTreeItemPickList.Create(nil) do
  try
    AddColumn(TFullNameColumn.Create);
    AddColumn(TNrElementsColumn.Create);
    AddColumn(TProjectionColumn.Create);

    AddRows(m_lstDiv, TTreeItem(edUniqueSourceTU.Tag));
    if Execute then
    begin
      edUniqueSourceTU.Tag  := integer(SelectedTreeItem);
      edUniqueSourceTU.Text := DMS_TreeItem_GetName(SelectedTreeItem);
      edUniqueSourceTU.Hint := TreeItem_GetFullName_Save(SelectedTreeItem);
    end;
  finally
    Free;
  end;
end;

procedure TfrmUnitComposer.sbSubsetFilterClick(Sender: TObject);
var
  nCnt, nIdx : integer;
  diCurr  : TAbstrDataItem;
  uDomain : TAbstrUnit;
begin
  uDomain := TTreeItem(edSourceTU.Tag);

  if(uDomain = nil)
  then nCnt := 0
  else nCnt := DMS_Unit_GetNrDataItemsOut(uDomain);

  m_lstDiv.Clear;
  for nIdx := 0 to nCnt - 1 do
  begin
    diCurr := DMS_Unit_GetDataItemOut(uDomain, nIdx);
    if not assigned(diCurr) then continue;
    if GetValuesUnitValueTypeID(diCurr) = VT_Bool
    then m_lstDiv.Add(diCurr);
  end;

  with TfrmTreeItemPickList.Create(nil) do
  try
    AddColumn(TFullNameColumn.Create);
    AddColumn(TValuesUnitColumn.Create);

    AddRows(m_lstDiv, TTreeItem(edSubsetFilter.Tag));
    if Execute then
    begin
      edSubsetFilter.Tag  := integer(SelectedTreeItem);
      edSubsetFilter.Text := DMS_TreeItem_GetName(SelectedTreeItem);
      edSubsetFilter.Hint := TreeItem_GetFullName_Save(SelectedTreeItem);
    end;
  finally
    Free;
  end;
end;

procedure TfrmUnitComposer.sbSourceTUClick(Sender: TObject);
begin
  m_lstDiv.Clear;
  GetUnitsOfType([VT_UInt32, VT_UInt16, VT_UInt8], TConfiguration.CurrentConfiguration.UnitContainers, m_lstDiv);

  with TfrmTreeItemPickList.Create(nil) do
  try
    AddColumn(TFullNameColumn.Create);
    AddColumn(TNrElementsColumn.Create);
    AddColumn(TProjectionColumn.Create);

    AddRows(m_lstDiv, TTreeItem(edSourceTU.Tag));
    if Execute then
    begin
      edSourceTU.Tag  := integer(SelectedTreeItem);
      edSourceTU.Text := DMS_TreeItem_GetName(SelectedTreeItem);
      edSourceTU.Hint := TreeItem_GetFullName_Save(SelectedTreeItem);
    end;
  finally
    Free;
  end;
end;

procedure TfrmUnitComposer.sbTargetVUClick(Sender: TObject);
var
  lstUnits : TList;
  diIn : TAbstrDataItem;
  nCnt, nIdx, nCntDI, nIdxDI, nReq : integer;
begin
  lstUnits := TList.Create;
  m_lstDiv.Clear;
  GetUnitsOfType([], TConfiguration.CurrentConfiguration.UnitContainers, m_lstDiv);
  nCnt := m_lstDiv.Count;
  for nIdx := 0 to nCnt - 1 do
  begin
    nCntDI := DMS_Unit_GetNrDataItemsIn(m_lstDiv.Items[nIdx]);
    if(nCntDI > 1)
    then begin
      nReq := 0;
      nIdxDI := 0;
      while ((nReq < 2) and (nIdxDI < nCntDI)) do
      begin
        diIn := DMS_Unit_GetDataItemIn(m_lstDiv.Items[nIdx], nIdxDI);
        if assigned(diIn)
        and (not DMS_TreeItem_InTemplate(diIn))
        then
          inc(nReq);
        inc(nIdxDI);
      end;

      if(nReq >= 2)
      then lstUnits.Add(m_lstDiv.Items[nIdx]);
    end;
  end;

  with TfrmTreeItemPickList.Create(nil) do
  begin
    AddColumn(TFullNameColumn.Create);
    AddColumn(TRangeColumn.Create);
    AddColumn(TValueTypeColumn.Create);
    AddColumn(TMetricColumn.Create);

    AddRows(lstUnits, TTreeItem(edTargetVU.Tag));
    if(Execute)
    then begin
      edTargetVU.Tag  := integer(SelectedTreeItem);
      edTargetVU.Text := DMS_TreeItem_GetName(SelectedTreeItem);
      edTargetVU.Hint := TreeItem_GetFullName_Save(SelectedTreeItem);
    end;

    Free;
  end;

  lstUnits.Free;
end;

end.
