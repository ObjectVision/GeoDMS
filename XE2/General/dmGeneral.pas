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

unit dmGeneral;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  Menus, uDMSInterfaceTypes, AppEvnts, ActnList, ImgList, fmBasePrimaryDataControl,
  Buttons;

const
  SF_AdminMode       =  1;
  SF_SuspendForGUI   =  2;
  SF_ShowStateColors =  4;
  SF_TraceLogFile    =  8;

  SF_TreeViewVisible =  16;
  SF_DetailsVisible  =  32;
  SF_EventLogVisible =  64;
  SF_ToolBarVisible  = 128;
  SF_AllVisile = SF_TreeViewVisible + SF_DetailsVisible + SF_EventLogVisible + SF_ToolBarVisible;

  SF_MultiThreading1 = 256;
  SF_MultiThreading2 = 1024;
  SF_CurrentItemBarHidden = 2048;
  SF_DynamicROI      = 4096;
  SF_MultiThreading3 = 8192;
  SF_ShowThousandSeparator = 16384;

  SRI_LastConfigFile = 0;
  SRI_HelpUrl        = 1;
  SRI_DmsEditor      = 2;
  SRI_LocalDataDir   = 3;
  SRI_SourceDataDir  = 4;
  SRI_Count          = 5;
  SR_Names: array [0..SRI_Count-1] of string = ('LastConfigFile', 'HelpUrl', 'DmsEditor', 'LocalDataDir', 'SourceDataDir');


  NRI_ErrBoxWidth    = 0;
  NRI_ErrBoxHeight   = 1;
  NRI_Count          = 2;
  NR_Names: array [0..NRI_Count-1] of string = ('ErrBoxWidth', 'ErrBoxHeight');

type
  TATIDisplayProc = procedure(tiActive: TTreeItem; oSender: TObject) of object;
  TUpdateControlProc = procedure(var done: Boolean) of object;
 // TDataControlClass = class of TfraBasePrimaryDataControl;

  TGeneralExtentInfo = record
    fTop     : double;
    fLeft    : double;
    fBottom  : double;
    fRight   : double;
    bPending : boolean;
  end;

  CppException = class (Exception)
    m_IsFatal: boolean;
    constructor Create( msg: PMsgChar; isFatal: Boolean);
  end;

  TdmfGeneral = class(TDataModule)
    pmTreeItem: TPopupMenu;
    miUpdateSubTree: TMenuItem;
    miUpdateTreeItem: TMenuItem;
    miExportPrimaryData: TMenuItem;
    miEditDefinition: TMenuItem;
    N6: TMenuItem;
    miDefaultView: TMenuItem;
    miMapView: TMenuItem;
    miDatagridView: TMenuItem;
    miHistogramView: TMenuItem;
    miProcessSchemesSub: TMenuItem;
    miSubitemsView: TMenuItem;
    miSuppliersView: TMenuItem;
    miExpressionView: TMenuItem;
    miExportBitmap: TMenuItem;
    miExportAsciiGrid: TMenuItem;
    miExportDbfShape: TMenuItem;
    miExportCsvTable: TMenuItem;
    aeGeneral: TApplicationEvents;
    miTest: TMenuItem;
    miEditClassificationandPalette: TMenuItem;
    N4: TMenuItem;
    N5: TMenuItem;
    ilButtons: TImageList;
    miEditConfigSource: TMenuItem;
    miInvalidateTreeItem: TMenuItem;
    Codeanalysis1: TMenuItem;
    SetTarget1: TMenuItem;
    AddTarget1: TMenuItem;
    ClearTargets1: TMenuItem;
    SetSource: TMenuItem;
    miStepUp: TMenuItem;
    miRunUp: TMenuItem;

    procedure pmTreeItemPopup(Sender: TObject);
    procedure miUpdateSubTreeClick(Sender: TObject);
    procedure miUpdateTreeItemClick(Sender: TObject);
    procedure miEditDefinitionClick(Sender: TObject);
    procedure miEditConfigSourceClick(Sender: TObject);
    procedure miDefaultViewClick(Sender: TObject);
    procedure miDatagridViewClick(Sender: TObject);
    procedure miHistogramViewClick(Sender: TObject);
    procedure miSubitemsViewClick(Sender: TObject);
    procedure miSuppliersViewClick(Sender: TObject);
    procedure miExpressionViewClick(Sender: TObject);

    procedure miExportBitmapClick(Sender: TObject);
    procedure miExportDbfShapeClick(Sender: TObject);
    procedure miExportAsciiGridClick(Sender: TObject);
    procedure miExportCsvTableClick(Sender: TObject);

    procedure miSimpleExpressionBuilderNewClick(Sender: TObject);
    procedure miSimpleExpressionBuilderEditClick(Sender: TObject);
    procedure miEditClassificationandPaletteClick(Sender: TObject);
    procedure miMapViewClick(Sender: TObject);
    procedure miTestClick(Sender: TObject);

    procedure IncIdleBlockCount;
    procedure DecIdleBlockCount;
    procedure IncActiveErrorCount;
    procedure DecActiveErrorCount;
    function  HasActiveError: Boolean;

    procedure aeGeneralIdle(Sender: TObject; var Done: Boolean);
    procedure aeGeneralMessage(var Msg: tagMSG; var Handled: Boolean);
    procedure aeGeneralException(Sender: TObject; E: Exception);
    procedure DispError(msg: PMsgChar; isFatal: boolean);

    function  CheckItem(ti: TTreeItem): Boolean;

//  Interface to persistent values
    procedure SetStatus(mask: Integer; v: Boolean);
    procedure SetString(i: Integer; v: String);
    procedure SetNumber(i: Integer; v: Integer);

    function GetStatus(mask: Integer): Boolean;
    function GetString(i:    Integer): String;
    function GetNumber(i:    Integer): Integer;

    procedure ReadDmsRegistry;
    procedure WriteDmsRegistry;
    procedure miInvalidateTreeItemClick(Sender: TObject);
    procedure aeGeneralActivate(Sender: TObject);
    procedure SetTarget1Click(Sender: TObject);
    procedure AddTarget1Click(Sender: TObject);
    procedure ClearTargets1Click(Sender: TObject);
    procedure SetSourceClick(Sender: TObject);
    procedure miStepUpClick(Sender: TObject);
    procedure miRunUpClick(Sender: TObject);

  private
    { Private declarations }
    m_tiActive : TTreeItem;
    m_tiInterested: TTreeItem;

    m_arATIDisplayProcs : array of TATIDisplayProc;
    m_arUpdateControlProcs : array of TUpdateControlProc;
    m_arCurrForms: array of TCustomForm;
    m_geiThis : TGeneralExtentInfo;

    m_bStateChanged, m_bReqReopen: Boolean;

    m_nIdleBlockCount,
    m_nActiveErrCount: Cardinal;

    m_fOnAdminModeChanged:       TNotifyEvent;
    m_fOnShowStateColorsChanged: TNotifyEvent;

    // Register settings
    m_nStatus: Integer;
    m_sStrings: array[0..SRI_Count-1] of string;
    m_nNumbers: array[0..NRI_Count-1] of Integer;
    m_bRegChanged: Boolean;

    procedure SetTerminating(bValue: Boolean);
    function  GetTerminatingState: Boolean;
    function  CloseCurrForms: Boolean;
    procedure SetReopen(v: Boolean);
    procedure CreateNewControl(tvs: TTreeItemViewStyle; showDisclaimer: bool; dcc: TDataControlClass);

    procedure EditExpr;
    procedure DisplayInDefaultView;
  public
    { Public declarations }
    // components wishing to use pmTreeItem,
    // should set m_tiActive to the active treeitem
    // before every call to the menu

    constructor Create(AOwner: TComponent); override;
    destructor  Destroy; override;

    procedure CreateMdiChild(createStruct: PCreateStruct);

    procedure RegisterATIDisplayProc(dpThis: TATIDisplayProc);
    procedure UnRegisterATIDisplayProc(dpThis: TATIDisplayProc);

    procedure RegisterUpdateControlProc(ucpThis: TUpdateControlProc);
    procedure UnRegisterUpdateControlProc(ucpThis: TUpdateControlProc);

    procedure RegisterCurrForm(sender: TCustomForm);
    procedure UnRegisterCurrForm(sender: TCustomForm);

    procedure SetNewGeneralExtent(fTop, fLeft, fBottom, fRight: double);
    function  GetNewGeneralExtent: TGeneralExtentInfo;

    procedure SetActiveTreeItem(tiActive : TTreeItem; oSender: TObject);
    procedure SetActiveVisibleTreeItem(tiActive : TTreeItem);
    function GetActiveTreeItem(requiredClass: CRuntimeClass = nil): TTreeItem;
    function GetUpdateTreeItemEnabled: boolean;
    function GetSubitemSchemaEnabled: boolean;
    function GetSupplierSchemaEnabled: boolean;
    function GetCalculationSchemaEnabled: boolean;
    function GetDatagridViewEnabled: boolean;
    function GetMapViewEnabled: boolean;
    function GetDefaultViewEnabled: boolean;
    function GetHistogramViewEnabled: boolean;
    function GetEditDefinitionEnabled: boolean;
    function GetEditClassificationandPaletteEnabled: boolean;
    function GetExportCsvTableEnabled: Boolean;
    function GetExportDbfShapeEnabled: Boolean;

    function AdminMode: boolean;

    procedure SetButtonImage(sbThis: TSpeedButton; nIdx: integer);
    property  StateChanged : boolean read m_bStateChanged write m_bStateChanged;

    property ReOpen:          Boolean read m_bReqReopen        write SetReopen;
    property Terminating:     Boolean read GetTerminatingState write SetTerminating;

    // Events
    property OnAdminModeChanged:       TNotifyEvent read m_fOnAdminModeChanged write m_fOnAdminModeChanged;
    property OnShowStateColorsChanged: TNotifyEvent read m_fOnShowStateColorsChanged write m_fOnShowStateColorsChanged;
  end;

const
  crZoomIn  = -20000;
  crZoomOut = -20001;
  crPan     = -20002;

var
  dmfGeneral: TdmfGeneral;
  m_bTerminating:        boolean = false;
  SuspendErrorFeedback : boolean = false;
  SuspendMemoErrorFeedback : boolean = false;

procedure DispMessage(sMsg: String);
procedure CppExceptionTranslator(msg: PMsgChar); cdecl;
procedure SeTranslator(msg: PMsgChar); cdecl;

implementation

{$R *.DFM}

uses
  fErrorMsg,
  uDMSUtils,
  uDmsInterface,
  fExpresEdit,
  uAppGeneral, uGenLib, uDmsRegistry,
  fExportASCGrid,
  fExportBitmap,
  fMain,
  fPrimaryDataViewer,
  fmHistogramControl,
  fmDmsControl,
  uDMSTreeviewFunctions,
  Grids, FileCtrl,
  fMdiChild,
  uMapClasses,
  Configuration, ComCtrls,
  OptionalInfo,
  ClipBrd,
  TreeNodeUpdateSet,
  uViewAction;

procedure DispMessage(sMsg: String);
begin
  if not m_bTerminating then
    ShowMessage(sMsg);
end;

function dmfGeneralTerminate: Boolean;
begin
  dmfGeneral.IncActiveErrorCount;
  dmfGeneral.Terminating := true;
  DMS_Terminate;
  Result := True;
end;

procedure TdmfGeneral.SetReopen(v: Boolean);
begin
  if (v) then
  begin
    m_bReqReopen := true;
    Terminating := true;
  end
  else
  begin
    m_bReqReopen := false;
    SuspendErrorFeedback := false;
    SuspendMemoErrorFeedback := false;
    m_bTerminating := false;
  end;
end;

{$IFDEF DEBUG}
const sd_ReadDmsRegistryDone: Boolean = false;
{$ENDIF}

procedure TdmfGeneral.ReadDmsRegistry;
var i: Integer; regString: String;
begin
  m_bRegChanged := false;
{$IFDEF DEBUG}
  sd_ReadDmsRegistryDone := true;
{$ENDIF}

  // default values for registry settings
  m_nStatus := DMS_Appl_GetRegStatusFlags;

  m_nNumbers[NRI_ErrBoxWidth ] := 0;
  m_nNumbers[NRI_ErrBoxHeight] := 0;

  m_sStrings[SRI_LastConfigFile]  := '';
  m_sStrings[SRI_HelpUrl       ]  :='https://www.geodms.nl/GeoDMS';
  m_sStrings[SRI_DmsEditor     ]  :='"%env:ProgramFiles%\Notepad++\Notepad++.exe" "%F" -n%L';
  m_sStrings[SRI_LocalDataDir  ]  :='C:\LocalData';
  m_sStrings[SRI_SourceDataDir ]  :='C:\SourceData';

  try
    with TDmsRegistry.Create do
    try
      if OpenKeyReadOnly('')
      then try
        for i:=0 to SRI_Count-1 do
           if ValueExists(SR_Names[i]) then
           begin
             regString := ReadString(SR_Names[i]);
             // discard stored old default helpUrl, see issue 199
             if (i <> SRI_HelpUrl) or (regString <> 'http://wiki.objectvision.nl/index.php/GeoDMS') then
                m_sStrings[i] := regString;
           end;
        for i:=0 to NRI_Count-1 do
           if ValueExists(NR_Names[i]) then m_nNumbers[i]  := ReadInteger(NR_Names[i]);
      finally
        CloseKey;
      end;
    finally
      Free;
    end;
  except on E: Exception do
    AppLogMessage(E.ClassName + ': '+ E.Message);
  end;
  SHV_SetAdminMode(AdminMode);
end;

procedure TdmfGeneral.WriteDmsRegistry;
var i: Integer;
begin
  if not m_bRegChanged then exit;

  m_bRegChanged := false;

  try
    with TDmsRegistry.Create do
    try
      if OpenKey('')
      then try
        WriteInteger('StatusFlags', m_nStatus );
        for i:=0 to SRI_Count-1 do
           WriteString(SR_Names[i],m_sStrings[i]);
        for i:=0 to NRI_Count-1 do
           WriteInteger(NR_Names[i], m_nNumbers[i]);
      finally
        CloseKey;
      end;
    finally
      Free;
    end;
  except on E: Exception do
    AppLogMessage(E.ClassName + ': '+ E.Message);
  end;
end;

constructor TdmfGeneral.Create(AOwner: TComponent);
begin
  inherited;
  Assert(self = dmfGeneral);
  setlength(m_arATIDisplayProcs, 0);
  setlength(m_arUpdateControlProcs, 0);
  m_nIdleBlockCount := 0;
  m_tiActive := nil;
  m_tiInterested := nil;
  m_geiThis.bPending := false;
  m_bStateChanged := true;
  m_bTerminating := false; AddTerminateProc(dmfGeneralTerminate);
  m_bReqReopen := false;

  m_fOnAdminModeChanged := Nil;
  m_fOnShowStateColorsChanged := nil;

  ReadDmsRegistry;
end;

destructor TdmfGeneral.Destroy;
begin
  Assert(not Assigned(OnAdminModeChanged));
  Assert(not Assigned(OnShowStateColorsChanged));
  Assert(length(m_arATIDisplayProcs) = 0);
  Assert(length(m_arUpdateControlProcs) = 0);
  Assert(m_bTerminating);
//  Assert(not Assigned(m_tiActive)); is assigned when terminating abnormally
  Assert(not Assigned(m_tiInterested));
  WriteDmsRegistry;
  inherited;
end;

procedure TdmfGeneral.SetStatus(mask: Integer; v: Boolean);
begin
  if (GetStatus(mask) = v) then exit;
  m_nStatus := m_nStatus XOR mask;
  m_bRegChanged := true;

  if (mask = SF_AdminMode) then
  begin
    SHV_SetAdminMode(v);
    if Assigned(OnAdminModeChanged) then
      OnAdminModeChanged(self);
  end
  else if (mask = SF_ShowStateColors) and Assigned(OnShowStateColorsChanged) then
    OnShowStateColorsChanged(self)
  else if (mask AND (SF_MultiThreading1 OR SF_MultiThreading2 OR SF_MultiThreading3 OR SF_ShowThousandSeparator)) <> 0 then
    DMS_Appl_SetRegStatusFlags(m_nStatus);
end;

procedure TdmfGeneral.SetString(i: Integer; v: String);
begin
  if (GetString(i) = v) then exit;
  m_sStrings[i] := v;
  m_bRegChanged := true;
  if (i = SRI_LocalDataDir) and DMS_Appl_LocalDataDirWasUsed then
        DispMessage('new LocalDataDir only becomes active after restarting the GeoDMS');
  if (i = SRI_SourceDataDir) and DMS_Appl_SourceDataDirWasUsed then
        DispMessage('new SourceDataDir only becomes active after restarting the GeoDMS');
end;

procedure TdmfGeneral.SetNumber(i: Integer; v: Integer);
begin
  if (GetNumber(i) = v) then exit;
  m_nNumbers[i] := v;
  m_bRegChanged := true;
end;

function TdmfGeneral.GetStatus(mask: Integer): Boolean;
begin
{$IFDEF DEBUG}
  assert(sd_ReadDmsRegistryDone);
{$ENDIF}
  Result := ((m_nStatus AND mask) <> 0);
end;

function TdmfGeneral.GetString(i:    Integer): String;
begin
{$IFDEF DEBUG}
  assert(sd_ReadDmsRegistryDone);
{$ENDIF}
  assert((i>=0) and (i< SRI_Count));

  Result := m_sStrings[i];
end;

function TdmfGeneral.GetNumber(i:    Integer): Integer;
begin
{$IFDEF DEBUG}
  assert(sd_ReadDmsRegistryDone);
{$ENDIF}
  assert((i>=0) and (i< NRI_Count));

  Result := m_nNumbers[i];
end;

function TdmfGeneral.AdminMode: boolean;
begin
  Result := GetStatus(SF_AdminMode);
end;

function TdmfGeneral.CloseCurrForms: Boolean;
var i: Integer;
begin
  Result := length(m_arCurrForms) > 0;
  if result then
  for i:= length(m_arCurrForms)-1 downto 0 do
    m_arCurrForms[i].ModalResult := mrCancel;
end;

procedure TdmfGeneral.SetTarget1Click(Sender: TObject);
begin
  DMS_TreeItem_SetAnalysisTarget(GetActiveTreeItem, true);
end;

procedure TdmfGeneral.AddTarget1Click(Sender: TObject);
begin
  DMS_TreeItem_SetAnalysisTarget(GetActiveTreeItem, false);
end;

procedure TdmfGeneral.ClearTargets1Click(Sender: TObject);
begin
  DMS_TreeItem_SetAnalysisSource(nil);
  DMS_TreeItem_SetAnalysisTarget(nil, true);  // REMOVE, al gedaan door SetAnalysisSource
end;

procedure TdmfGeneral.SetSourceClick(Sender: TObject);
begin
  DMS_TreeItem_SetAnalysisSource(dmfGeneral.GetActiveTreeItem);
end;


procedure TdmfGeneral.SetTerminating(bValue: Boolean);
begin
  Assert(bValue);
  m_bTerminating := bValue;
  SuspendErrorFeedback     := true;
  SuspendMemoErrorFeedback := true;
  CloseCurrForms;
end;

function TdmfGeneral.GetTerminatingState: Boolean;
begin
  Result := m_bTerminating;
end;

procedure TdmfGeneral.RegisterATIDisplayProc(dpThis: TATIDisplayProc);
begin
  setlength(m_arATIDisplayProcs, length(m_arATIDisplayProcs) + 1);
  m_arATIDisplayProcs[length(m_arATIDisplayProcs) - 1] := dpThis;
end;

procedure TdmfGeneral.UnRegisterATIDisplayProc(dpThis: TATIDisplayProc);
var
  nIdx, nCnt : integer;
begin
  nCnt := length(m_arATIDisplayProcs);
  Assert(nCnt > 0);
  for nIdx := 0 to nCnt - 1 do
  begin
//  if(m_arATIDisplayProcs[nIdx].code = dpThis.code) AND (m_arATIDisplayProcs[nIdx].data = dpThis.data)
    if CompareMem(@@m_arATIDisplayProcs[nIdx], @@dpThis, sizeof(TATIDisplayProc))
    then break;
  end;
  Assert(nIdx < nCnt);
  while (nIdx < nCnt-1) do
  begin
    m_arATIDisplayProcs[nIdx] := m_arATIDisplayProcs[nIdx + 1];
    inc(nIdx)
  end;
  setlength(m_arATIDisplayProcs, nCnt - 1);
end;

procedure TdmfGeneral.RegisterUpdateControlProc(ucpThis: TUpdateControlProc);
var c: Cardinal;
begin
  c := length(m_arUpdateControlProcs);
  setlength(m_arUpdateControlProcs, c + 1);
  m_arUpdateControlProcs[c] := ucpThis;
end;

procedure TdmfGeneral.UnRegisterUpdateControlProc(ucpThis: TUpdateControlProc);
var
  nIdx, nCnt : integer;
begin
  nCnt := length(m_arUpdateControlProcs);
  Assert(nCnt > 0);
  for nIdx := 0 to nCnt - 1 do
  begin
//    if(m_arUpdateControlProcs[nIdx].code = ucpThis.code) AND (m_arUpdateControlProcs[nIdx].data = ucpThis.data)
    if CompareMem(@@m_arUpdateControlProcs[nIdx], @@ucpThis, sizeof(TUpdateControlProc))
    then break;
  end;
  Assert(nIdx < nCnt);
  while (nIdx < nCnt-1) do
  begin
    m_arUpdateControlProcs[nIdx] := m_arUpdateControlProcs[nIdx + 1];
    inc(nIdx)
  end;

  setlength(m_arUpdateControlProcs, nCnt - 1);
end;

procedure TdmfGeneral.RegisterCurrForm(sender: TCustomForm);
begin
  setlength(m_arCurrForms, length(m_arCurrForms) + 1);
  m_arCurrForms[length(m_arCurrForms) - 1] := sender;
end;

procedure TdmfGeneral.UnRegisterCurrForm(sender: TCustomForm);
begin
  Assert(length(m_arCurrForms) > 0);
  Assert(m_arCurrForms[length(m_arCurrForms)-1] = sender);
  setlength(m_arCurrForms, length(m_arCurrForms) - 1);
end;

procedure TdmfGeneral.SetNewGeneralExtent(fTop, fLeft, fBottom, fRight: double);
begin
  m_geiThis.fTop := fTop;
  m_geiThis.fLeft := fLeft;
  m_geiThis.fBottom := fBottom;
  m_geiThis.fRight := fRight;
  m_geiThis.bPending := true;
end;

function TdmfGeneral.GetNewGeneralExtent: TGeneralExtentInfo;
begin
  Result := m_geiThis;
end;

procedure TdmfGeneral.SetActiveTreeItem(tiActive: TTreeItem; oSender: TObject);
var
  nIdx : integer;
begin
  if (tiActive = m_tiActive) then exit;

  g_frmMain.BeginTiming;
  m_tiActive := tiActive;
  StateChanged := True;

  try
    SetInterestRef(m_tiInterested, tiActive);
  except
    on E: CppException do
      if E.m_IsFatal then
        raise;
  end;
  for nIdx := 0 to length(m_arATIDisplayProcs) - 1 do
      m_arATIDisplayProcs[nIdx](m_tiActive, oSender);
end;

function TdmfGeneral.GetActiveTreeItem(requiredClass: CRuntimeClass = nil): TTreeItem;
begin
  if Assigned(m_tiActive)
  then begin
    if(requiredClass = nil)
    then
      Result := m_tiActive
    else
      Result := DMS_TreeItem_QueryInterface(m_tiActive, requiredClass)
  end
  else
    Result := nil;
end;

procedure TdmfGeneral.SetActiveVisibleTreeItem(tiActive : TTreeItem);
begin
  if not AdminMode then
    while Assigned(tiActive) and DMS_TreeItem_InHidden(tiActive) do
      tiActive := DMS_TreeItem_GetParent(tiActive);
  SetActiveTreeItem(tiActive, nil);
end;


function TdmfGeneral.GetUpdateTreeItemEnabled: boolean;
begin
  Assert( Assigned(GetActiveTreeItem(DMS_TreeItem_GetStaticClass)) );
  Result := (not DMS_TreeItem_InTemplate(m_tiActive))
     and DMS_TreeItem_HasCalculator(m_tiActive);
end;

function TdmfGeneral.GetSubitemSchemaEnabled: boolean;
begin
  Result := (not DMS_TreeItem_InTemplate(m_tiActive))
    and DMS_TreeItem_HasSubItems(m_tiActive);
end;

function TdmfGeneral.GetSupplierSchemaEnabled: boolean;
begin
  Result := (not DMS_TreeItem_InTemplate(m_tiActive))
    and DMS_TreeItem_HasCalculator(m_tiActive);
end;

function TdmfGeneral.GetCalculationSchemaEnabled: boolean;
begin
  Result := (not DMS_TreeItem_InTemplate(m_tiActive))
    and (DMS_TreeItem_HasCalculator(m_tiActive));
end;

function TdmfGeneral.GetDatagridViewEnabled: boolean;
begin

  Result := SHV_CanUseViewStyle(m_tiActive, tvsDataGrid)
         OR SHV_CanUseViewStyle(m_tiActive, tvsDataGridContainer);
end;

function TdmfGeneral.GetMapViewEnabled: boolean;
begin
  Result := SHV_CanUseViewStyle(m_tiActive, tvsMapView);
end;

function TdmfGeneral.GetHistogramViewEnabled: boolean;
begin
   Result := (not DMS_TreeItem_InTemplate(m_tiActive))
    and (TfraHistogramControl.GetHistogramViewEnabled(m_tiActive));
end;

function TdmfGeneral.GetDefaultViewEnabled: boolean;
begin
  Result := (not DMS_TreeItem_InTemplate(m_tiActive))
    and (GetMapViewEnabled or GetDatagridViewEnabled or GetHistogramViewEnabled);
end;

function TdmfGeneral.GetEditDefinitionEnabled: boolean;
begin
  Assert(m_tiActive <> nil);
  Result := (  (DMS_TreeItem_GetExpr(m_tiActive) <> '')
            or Assigned(GetActiveTreeItem(DMS_AbstrDataItem_GetStaticClass))
            or Assigned(GetActiveTreeItem(DMS_AbstrUnit_GetStaticClass))
            );
end;

function TdmfGeneral.GetEditClassificationandPaletteEnabled: boolean;
var
  tiSubItem: TTreeItem;
begin
  Result := Assigned(m_tiActive);

  if not Result then exit;

  Result  := IsClassification(m_tiActive);
  if Result then exit;

  tiSubItem := DMS_TreeItem_GetFirstVisibleSubItem(m_tiActive);
  while Assigned(tiSubItem) do
  begin
    Result := IsClassification(tiSubItem);
    if Result then exit;
    tiSubItem := DMS_TreeItem_GetNextVisibleItem(tiSubItem);
  end;
end;

function TdmfGeneral.GetExportCsvTableEnabled: Boolean;
begin
  Result := GetDataGridViewEnabled;
end;

function TdmfGeneral.GetExportDbfShapeEnabled: Boolean;
begin
  Result := GetDataGridViewEnabled;
end;

procedure TdmfGeneral.miEditConfigSourceClick(Sender: TObject);
var ti: TTreeItem;

begin
  ti := GetActiveTreeItem;
  if assigned(ti) then
    TfrmErrorMsg.StartSourceEditor(TreeItem_GetSourceName(ti));
end;

// Please remember: if you change any .Enabled code in this function
// you _must_ update fMain.pas as well, otherwise the two menus will be
// out of sync.
procedure TdmfGeneral.pmTreeItemPopup(Sender: TObject);

   procedure SetAdminMode(mi: TMenuItem);
   begin
     if AdminMode then
       mi.Visible := true
     else
     begin
       mi.Visible := false;
       mi.Enabled := false;
     end;
   end;

   procedure UpdateFiles(bDataItem: Boolean);
   begin
      miExportAsciiGrid.Enabled   := Assigned(m_tiActive) AND (not DMS_TreeItem_InTemplate(m_tiActive));
      miExportBitmap.Enabled      := bDataItem            AND (not DMS_TreeItem_InTemplate(m_tiActive));
      miExportCsvTable.Enabled    := Assigned(m_tiActive) AND GetExportCsvTableEnabled;
      miExportDbfShape.Enabled    := Assigned(m_tiActive) AND GetExportDbfShapeEnabled;
      miExportPrimaryData.Enabled := miExportBitmap.Enabled or miExportCsvTable.Enabled or miExportAsciiGrid.Enabled or miExportDbfShape.Enabled;
   end;

   procedure UpdateEdits(bRealTreeItem: Boolean);
   begin
      SetAdminMode(miEditDefinition);
//      SetAdminMode(miEditConfigSource);
      SetAdminMode(miEditClassificationandPalette);
      SetAdminMode(miUpdateTreeItem);
      SetAdminMode(miInvalidateTreeItem);
      SetAdminMode(miUpdateSubTree);

      miStepUp.Enabled := bRealTreeItem AND DMS_TreeItem_IsFailed(GetActiveTreeItem);
      miStepUp.Visible := true;

      if not AdminMode then exit;

      miEditConfigSource.Enabled       := bRealTreeItem;
      miEditDefinition.Enabled         := bRealTreeItem and GetEditDefinitionEnabled;
      miEditClassificationandPalette.Enabled := bRealTreeItem and GetEditClassificationandPaletteEnabled;
      miUpdateTreeItem.Enabled         := bRealTreeItem and GetUpdateTreeItemEnabled;
      miUpdateSubTree.Enabled          := bRealTreeItem and DMS_TreeItem_HasSubItems(m_tiActive);
      miInvalidateTreeItem.Enabled     := miUpdateTreeItem.Enabled or miUpdateSubTree.Enabled;
   end;

   procedure UpdateViews(bRealTreeItem: Boolean);
   begin
      miDefaultView.Enabled    := (bRealTreeItem) and GetDefaultViewEnabled;

      miMapView.Enabled        := (bRealTreeItem) and GetMapViewEnabled;

      miDatagridView.Enabled   := (bRealTreeItem) and GetDatagridViewEnabled;
      miHistogramView.Enabled  := (bRealTreeItem) and GetHistogramViewEnabled;

      SetAdminMode(miProcessSchemesSub);
      if not AdminMode then exit;

      miSubItemsView.Enabled   := (bRealTreeItem) and GetSubitemSchemaEnabled;
      miSuppliersView.Enabled  := (bRealTreeItem) and GetSupplierSchemaEnabled;
      miExpressionView.Enabled := (bRealTreeItem) and GetCalculationSchemaEnabled;
      miProcessSchemesSub.Enabled := miSubItemsView.Enabled or miSuppliersView.Enabled or miExpressionView.Enabled;
   end;

var
  bRealTreeItem : boolean;
  bDataItem : boolean;

begin // pmTreeItemPopup
   bRealTreeItem := Assigned(GetActiveTreeItem(DMS_TreeItem_GetStaticClass));
   bDataItem := Assigned(GetActiveTreeItem(DMS_AbstrDataItem_GetStaticClass));

   UpdateFiles  (bDataItem);
   UpdateEdits  (bRealTreeItem);
   UpdateViews  (bRealTreeItem);

   // the simple expression builder is set invisible at this moment ...
   // miSimpleExpressionBuilderNew.Enabled  := bRealTreeItem and frmMain.AdminMode;
end;

{******************************************************************************}
{                                 MENU OPTIONS                                 }
{******************************************************************************}

procedure TdmfGeneral.CreateNewControl(tvs: TTreeItemViewStyle; showDisclaimer: bool; dcc: TDataControlClass);
var
  tiThis  : TTreeItem;
  dv: TfrmPrimaryDataViewer;
begin
  tiThis := GetActiveTreeItem;
  if not CheckItem(tiThis) then exit;
  if (dcc = nil) then Exit;

  DMS_TreeItem_IncInterestCount(tiThis);
  try
    if not dcc.AllInfoPresent(tiThis, tvs) then exit;

    if showDisclaimer then TfrmOptionalInfo.Display(OI_Disclaimer, false);

    dv := TfrmPrimaryDataViewer.CreateNew(Application, dcc, tvs);
    try
      with dv do
      begin
        DataControl.LoadTreeItem(tiThis, nil);
        Show;
        dv := nil;
      end;
    finally
      dv.Free;
    end;
  finally
    DMS_TreeItem_DecInterestCount(tiThis);
  end;
end;

procedure TdmfGeneral.CreateMdiChild(createStruct: PCreateStruct);
begin
   TfrmOptionalInfo.Display(OI_Disclaimer, false);
   with TfrmPrimaryDataViewer.CreateFromObject(Application, TfraDmsControl, createStruct) do
   begin
      Show;
      if (createStruct^.makeOverlapped and $FF) <> 0 then
      begin
        WindowState  := wsNormal;
        ClientHeight := createStruct^.maxSizeY;
        ClientWidth  := createStruct^.maxSizeX;
      end;
      createStruct.hWnd := DataControl.Handle;
   end;
end;

// CURRENT MENU OPTIONS

procedure TdmfGeneral.miUpdateTreeItemClick(Sender: TObject);
begin
  CreateNewControl(tvsUpdateItem, false, TfraDmsControl);
end;

procedure TdmfGeneral.miUpdateSubTreeClick(Sender: TObject);
begin
  CreatenewControl(tvsUpdateTree, false, TfraDmsControl);
end;

procedure TdmfGeneral.miExportAsciiGridClick(Sender: TObject);
begin
  TfrmOptionalInfo.Display(OI_SourceDescr, false);
  TfrmExportASCGrid.ShowForm(m_tiActive, true);
end;

procedure TdmfGeneral.miExportBitmapClick(Sender: TObject);
begin
  TfrmOptionalInfo.Display(OI_SourceDescr, false);
  TfrmExportBitmap.ShowForm(m_tiActive);
end;

procedure exportTable(this: TdmfGeneral; exportProc_: ExportProc; extent_, filter_: String);
var curdir : string;
 ti: TTreeItem;
begin
  TfrmOptionalInfo.Display(OI_SourceDescr, false);

  curdir := GetCurrentDir;
  ti     := this.GetActiveTreeItem;

  with TSaveDialog.Create(this) do
  try
    Options := Options + [ofNoChangeDir, ofHideReadOnly, ofEnableSizing];
    Title    := 'Export '+extent_+' Table: ' + DMS_TreeItem_GetName(ti);
    DefaultExt := extent_;
    InitialDir := AsWinPath( TConfiguration.CurrentConfiguration.GetResultsPath );
    FileName := InitialDir + '\' + stringreplace(Copy(TreeItem_GetFullName_Save(ti), 2), '/', '_', [rfReplaceAll]) + '.'+extent_;
    DMS_MakeDirsForFile(PFileChar(FileString(FileName)));
    Filter := filter_;

    if Execute then
    begin
      DMS_TreeItem_Update(ti);
      exportProc_(ti, AsDmsPath(FileName), this.AdminMode);
    end;
  finally
    Free;
    SetCurrentDir(curdir);
  end;
end;

procedure TdmfGeneral.miExportDbfShapeClick(Sender: TObject);
begin
  exportTable(self, exportDbf, 'DBF', 'Dbf files|*.DBF');
end;

procedure TdmfGeneral.miExportCsvTableClick(Sender: TObject);
begin
  exportTable(self, exportCSV, 'csv', 'Semicolon separated values files|*.csv; *.CSV; *.txt; *.TXT; *.dat; *.DAT');
end;

procedure TdmfGeneral.miEditClassificationandPaletteClick(Sender: TObject);
var
  tiCdf, tiDesktop : TTreeItem;
  diCreated: TAbstrDataItem;
begin
  tiDesktop := TConfiguration.CurrentConfiguration.DesktopManager.ActiveDesktop;
  assert(assigned(tiDesktop));

  // 1. The selected item is a classification.
  if IsClassification(m_tiActive)
  then begin
    diCreated := SHV_EditPaletteView_Create(tiDesktop, m_tiActive, NIL);
    if Assigned(diCreated)
    then TreeView_ShowAssTreeNode(frmMain.tvTreeItem, diCreated);

    Exit;
  end;

  // 2. The selected item is the container of a classification.
  tiCdf := DMS_TreeItem_GetFirstVisibleSubItem(m_tiActive);
  while Assigned(tiCdf) do
  begin
    if IsClassification(tiCdf) then break;
    tiCdf := DMS_TreeItem_GetNextVisibleItem(tiCdf);
  end;

  if Assigned(tiCdf)
  then begin
    diCreated := SHV_EditPaletteView_Create(tiDesktop, tiCdf, nil);
    if Assigned(diCreated)
    then TreeView_ShowAssTreeNode(frmMain.tvTreeItem, diCreated);
  end;
end;

procedure TdmfGeneral.miEditDefinitionClick(Sender: TObject);
var
  diCreated: TAbstrDataItem;
begin
  if IsClassification(m_tiActive)
  then begin
    diCreated := SHV_EditPaletteView_Create(TConfiguration.CurrentConfiguration.DesktopManager.ActiveDesktop, m_tiActive, NIL);
    if Assigned(diCreated)
    then TreeView_ShowAssTreeNode(frmMain.tvTreeItem, diCreated);
    Exit;
  end;

  if(TreeItemDialogType(m_tiActive) = dtBrushColor)
  then SHV_EditPaletteView_Create(TConfiguration.CurrentConfiguration.DesktopManager.ActiveDesktop, m_tiActive, NIL)
  else EditExpr;
end;

procedure TdmfGeneral.miSimpleExpressionBuilderNewClick(Sender: TObject);
begin
//FLOW  TfrmSchemaViewer2.Display(nil, m_tiActive);
end;

procedure TdmfGeneral.miStepUpClick(Sender: TObject);
var ti: TTreeItem;
  unfoundPart: IString;
  upStr: ItemString;
begin
  unfoundPart := nil;
  ti := DMS_TreeItem_GetErrorSource(GetActiveTreeItem, @unfoundPart);
  upStr := DMS_ReleasedIString2String(unfoundPart);
  if Assigned(ti) then
  begin
    SetActiveTreeItem(ti, nil);
    frmMain.ProcessUnfoundPart(upStr);
  end;
end;

procedure TdmfGeneral.miRunUpClick(Sender: TObject);
var ti: TTreeItem;
begin
  while true do
  begin
    ti := GetActiveTreeItem;
    if not assigned(ti) then exit;    
    miStepUpClick(Sender);
    if GetActiveTreeItem = ti then exit;
  end;
end;

procedure TdmfGeneral.miSimpleExpressionBuilderEditClick(Sender: TObject);
begin
//FLOW  TfrmSchemaViewer2.Display(m_tiActive, DMS_TreeItem_GetParent(m_tiActive));
end;

procedure TdmfGeneral.miDefaultViewClick(Sender: TObject);
begin
  if not CheckItem(m_tiActive) then exit;
  DisplayInDefaultView;
end;

procedure TdmfGeneral.miMapViewClick(Sender: TObject);
begin
   CreateNewControl(tvsMapView, true, TfraDmsControl);
end;


procedure TdmfGeneral.miDatagridViewClick(Sender: TObject);
begin
  if SHV_CanUseViewStyle(m_tiActive, tvsDataGridContainer)
  then CreateNewControl(tvsDataGridContainer, true, TfraDmsControl)
  else CreateNewControl(tvsDataGrid,          true, TfraDmsControl);
end;

procedure TdmfGeneral.miHistogramViewClick(Sender: TObject);
begin
  CreateNewControl(tvsHistogram, true, TfraHistogramControl);
end;

procedure TdmfGeneral.miSubitemsViewClick(Sender: TObject);
begin
  CreateNewControl(tvsSubItemSchema, false, TfraDmsControl)
end;

procedure TdmfGeneral.miSuppliersViewClick(Sender: TObject);
begin
  CreateNewControl(tvsSupplierSchema, false, TfraDmsControl)
//FLOW    TfrmSchemaViewer.Display(m_tiActive, TSupplierSchemaCalculator, AdminMode);
end;

procedure TdmfGeneral.miExpressionViewClick(Sender: TObject);
begin
//FLOW    TfrmSchemaViewer.Display(m_tiActive, TCalculationSchemaCalculator, AdminMode);
end;

{******************************************************************************}
{                              TTREEITEM FUNCTIONS                             }
{******************************************************************************}

procedure TdmfGeneral.EditExpr;
begin
  TfrmExprEdit.Instance(m_tiActive).Show;
end;

procedure TdmfGeneral.IncActiveErrorCount;
begin
  INC(m_nActiveErrCount);
  IncIdleBlockCount;
end;

procedure TdmfGeneral.DecActiveErrorCount;
begin
  Assert( m_nActiveErrCount > 0 );
  DEC(m_nActiveErrCount);
  DecIdleBlockCount;
end;

function TdmfGeneral.HasActiveError: Boolean;
begin
  Result := (m_nActiveErrCount > 0);
end;

procedure TdmfGeneral.IncIdleBlockCount;
begin
  INC(m_nIdleBlockCount);
end;

procedure TdmfGeneral.DecIdleBlockCount;
begin
  Assert( m_nIdleBlockCount > 0 );
  DEC(m_nIdleBlockCount);
end;

procedure TdmfGeneral.aeGeneralIdle(Sender: TObject; var done: Boolean);
var
  nIdx : integer;
//  prClass: DWORD; REMOVE Priority reduction, issue
begin
//  AmIdle;

  if (m_nIdleBlockCount > 0) then exit;
  if not assigned(g_frmMain) then exit;
  if not done                then exit;

  DMS_NotifyCurrentTargetCount; // and calls ProcessMainThreadOpers and DMS_NotifyCurrentTargetCount
//  g_frmMain.BeginTiming;

  IncIdleBlockCount;
//  prClass := GetPriorityClass(GetCurrentProcess);
//  SetPriorityClass(GetCurrentProcess, IDLE_PRIORITY_CLASS);
  assert(not DMS_Actor_DidSuspend);
  try
    if (m_bReqReopen) then
    begin
      if not CloseCurrForms then // only when all forms are closed we do reopen
        g_frmMain.rfReopen;
      exit;
    end;

    TTreeNodeUpdateSet.UpdateTreeNodeSets(done);
    if not done then exit;

    done := TViewAction.IdleProcess;

    if done then
    begin
      StateChanged := false; // ViewAction processed
      if DMS_Actor_MustSuspend then
        done := false; // process paint events van UpdateDataInfo, if any, en kom dan terug
    end;
    if not done then exit;

    Assert(done);
    nIdx := length(m_arUpdateControlProcs);
    while (nIdx > 0 ) do
    begin
      dec(nIdx);
      if nIdx < length(m_arUpdateControlProcs) then
      begin
        m_arUpdateControlProcs[nIdx](done);
        if not done then exit;
      end;
    end;
  finally
    DMS_Actor_Resume;
  //  SetPriorityClass(GetCurrentProcess, prClass);
    DecIdleBlockCount;
    if done then
    begin
       DMS_NotifyCurrentTargetCount;
       frmMain.EndTiming;
       StateChanged := false;
    end;
  end;
  m_geiThis.bPending := false;
  if done then
  begin
     SuspendErrorFeedback := false;
     SuspendMemoErrorFeedback := false;
  end;
end;

procedure TdmfGeneral.DisplayInDefaultView;
var
  tiThis : TTreeItem;
  pdcSelected : TfraBasePrimaryDataControl;
begin
  // assign to local
  tiThis := m_tiActive;

  // check
  if not assigned(tiThis) then Exit;

  // get active viewer, if any
  if(frmMain.ActiveMDIChild is TfrmPrimaryDataViewer)
  then pdcSelected := TfrmPrimaryDataViewer(frmMain.ActiveMDIChild).DataControl
  else pdcSelected := nil;

  // do we have an active viewer of the correct type and
  // can it load the selected treeitem
  if((pdcSelected <> nil) and (pdcSelected.IsLoadable(tiThis)))
  then // yes ==> load it
  begin
    pdcSelected.LoadTreeItem(tiThis, nil);
    exit;
  end;

  // get default view type class
  CreateNewControl(SHV_GetDefaultViewStyle(tiThis), true, TfraDmsControl);
end;

procedure TdmfGeneral.SetButtonImage(sbThis: TSpeedButton; nIdx: integer);
begin
  if(BetweenInc(nIdx, 0, ilButtons.Count-1))
  then begin
    sbThis.Glyph := nil;
    ilButtons.GetBitmap(nIdx, sbThis.Glyph);
  end;
end;

constructor CppException.Create(msg: PMsgChar; isFatal: Boolean);
begin
  inherited Create(msg);
  m_IsFatal := isFatal;
end;

procedure CppExceptionTranslator(msg: PMsgChar); cdecl;
begin
  raise CppException.Create(msg, false);
end;

procedure SeTranslator(msg: PMsgChar); cdecl;
begin
  raise CppException.Create(msg, true);
end;

var reloadMsgActive: Boolean;

procedure TdmfGeneral.aeGeneralActivate(Sender: TObject);
var
  changedFileList: IString;
  msgStr: String;

begin
  if reloadMsgActive then exit; // function is not re-entrant. Would be nice if text would be updated.

  changedFileList := DMS_ReportChangedFiles(true);
  if not Assigned(changedFileList) then exit;
  try
    msgStr := DMS_IString_AsCharPtr(changedFileList);
  finally
    DMS_IString_Release(changedFileList);
  end;
  reloadMsgActive := true;
  try
    if  MessageDlg( // can cause re-entrancy of this function or other modal dialogs.
          'The following configuration files have changed:' + #13#10
          + msgStr + #13#10
          + 'Do you want to reload the configuration ?',
          mtWarning,
          mbYesNo, 0, mbYes)
        = mrYes
    then ReOpen := true;
  finally
    reloadMsgActive := false;
  end;
end;

procedure TdmfGeneral.aeGeneralException(Sender: TObject; E: Exception);
begin
  if SuspendErrorFeedback then exit;

  if not (E is CppException) then
    E.Message := 'Delphi error:' + chr(10)+ E.Message;
  DispError(PMsgChar(MsgString(E.Message)), (E is CppException) and (E As CppException).m_IsFatal);
end;

procedure TdmfGeneral.DispError(msg: PMsgChar; isFatal: boolean);
begin
  if SuspendErrorFeedback then exit;
  if TfrmErrorMsg.DisplayError(msg, isFatal) = mrIgnore
  then SuspendErrorFeedback := true;
end;

function TdmfGeneral.CheckItem(ti: TTreeItem): Boolean;
begin
  Result := Assigned(ti); // and not DMS_TreeItem_IsMetaFailed(ti)
end;

procedure TdmfGeneral.aeGeneralMessage(var Msg: tagMSG;
  var Handled: Boolean);
begin
  if Msg.message = WM_SYSKEYUP then
    if Msg.wParam = VK_SNAPSHOT
    then TfrmOptionalInfo.Display(OI_SourceDescr, false);
end;

procedure TdmfGeneral.miTestClick(Sender: TObject);
begin
    DBG_DebugReport;
//  frmMain.tvTreeItem.FullExpand;
end;

procedure TdmfGeneral.miInvalidateTreeItemClick(Sender: TObject);
begin
    DMS_TreeItem_Invalidate(m_tiActive);
end;

end.
