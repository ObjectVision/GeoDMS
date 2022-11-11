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

unit fExpresEdit;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  fMdiChild, uDMSInterfaceTypes, ExtCtrls, StdCtrls, ComCtrls, Buttons, dmGeneral,
  TreeNodeUpdateSet,
  Menus,
  fErrorMsg;

type

   TParseResultContext = class

      constructor Create(context: TTreeItem; expr: ItemString; contextIsTarget: Boolean);
      destructor  Destroy;  override;

      function HasParseResult: Boolean;
      function CheckSyntax: Boolean;
      function CheckSyntaxFunc(var tiResult: TTreeItem): Boolean;
      function TestExpression: Boolean;

      procedure DispError;

  private
      function  LogError(errCount: Cardinal): Boolean;

  private
      m_tiSelected      : TTreeItem;
      m_ContextIsTarget : Boolean;
      m_ErrMsg          : MsgString;
      m_ParseResult     : TParseResult;
  end;

TfrmExprEdit = class(TfrmMdiChild)
    pnlExpression: TPanel;
//    pnlDependencies: TPanel;
    spMain: TSplitter;
    tvDependencies: TTreeView;
    pnlExpressionSub1: TPanel;
    mmExpression: TMemo;
    pnlToolbar: TPanel;
    sbClear: TSpeedButton;
    sbApply: TSpeedButton;
    sbSyntax: TSpeedButton;
    sbTest: TSpeedButton;
    sbReset: TSpeedButton;
    Bevel1: TBevel;
    procedure mmExpressionDragOver(Sender, Source: TObject; X, Y: Integer;
      State: TDragState; var Accept: Boolean);
    procedure mmExpressionDragDrop(Sender, Source: TObject; X, Y: Integer);

    procedure sbClearClick(Sender: TObject);
    procedure sbApplyClick(Sender: TObject);
    procedure FormCloseQuery(Sender: TObject; var CanClose: Boolean);
    procedure mmExpressionChange(Sender: TObject);
    procedure tvDependenciesChanging(Sender: TObject; Node: TTreeNode;
      var AllowChange: Boolean);
    procedure tvDependenciesContextPopup(Sender: TObject; MousePos: TPoint;
      var Handled: Boolean);
    procedure tvDependenciesExpanding(Sender: TObject; Node: TTreeNode;
      var AllowExpansion: Boolean);

//    procedure OnlineOperatorHelpClick(Sender: TObject);
//    procedure pmPopupOperatorPopup(Sender: TObject);
//    procedure tvOperatorsKeyDown(Sender: TObject; var Key: Word;
//      Shift: TShiftState);
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure sbSyntaxClick(Sender: TObject);
    procedure sbTestClick(Sender: TObject);
    procedure sbResetClick(Sender: TObject);
    procedure tvDependenciesCustomDrawItem(Sender: TCustomTreeView;
      Node: TTreeNode; State: TCustomDrawState; var DefaultDraw: Boolean);
    procedure WMNotify(var Message: TWMNotify); message WM_NOTIFY;
    procedure tvDependenciesDblClick(Sender: TObject);
    procedure tvDependenciesCollapsing(Sender: TObject; Node: TTreeNode;
      var AllowCollapse: Boolean);
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
    procedure tvDependenciesChange(Sender: TObject; Node: TTreeNode);
  private
    { Private declarations }
    m_tiSelected           : TTreeItem;
    m_sOriginalExpression  : string;
    m_bFakeExpr: Boolean;
    m_TreeNodeUpdateSet:  TTreeNodeUpdateSet;

    constructor Create(AOwner: TComponent; const ti: TTreeItem); reintroduce;

    procedure ReadExpr;
    function  ApplyChanges: boolean;
    procedure InitDependenciesTree;
    procedure BuildDependenciesTree(ti: TTreeItem; tnCurrent: TTreeNode);
    procedure SetButtonStates;
    procedure DisableControls(bDisable : boolean);
  public
    { Public declarations }
    class function Instance(const ti: TTreeItem): TfrmExprEdit;

    function InitFromDesktop(tiView: TTreeItem): boolean; override;
//    procedure Update; override;
    procedure SetActiveTreeNode(tiActive: TTreeItem; oSender: TObject);
end;

implementation

{$R *.DFM}

uses
  uAppGeneral, uGenLib, uDMSUtils, uDmsInterface, fMain, OptionalInfo,
  Configuration;

// Global
function FormIdentity(Item1, Item2: Pointer): Boolean;
begin
   Result   := TfrmExprEdit(Item1).m_tiSelected  =  TTreeItem(Item2);
end;

class function TfrmExprEdit.Instance(const ti: TTreeItem): TfrmExprEdit;
   // local function
   function Find(var f: TfrmExprEdit): boolean;
   var
     nIdx: integer;
   begin
     Result := ScreenFormFind(TfrmExprEdit, FormIdentity, ti, nIdx);
     if(Result)
     then f := TfrmExprEdit(Screen.CustomForms[nIdx]);
   end;
begin
  if(not Find(Result))
  then Result := TfrmExprEdit.Create(Application, ti);
end;

// Constructor
constructor TfrmExprEdit.Create(AOwner: TComponent; const ti: TTreeItem);
var us: TUpdateState; isFailed: Boolean;
begin
  inherited Create(AOwner);

  m_tiSelected := ti;
  SetMdiToolbar( pnlToolbar );

  Caption := Caption + ' ' + string( TreeItem_GetFullName_Save(m_tiSelected) );

  ReadExpr;

  if (m_bFakeExpr) then
  begin
    sbApply.Enabled := False;
    mmExpression.Enabled := False;
    sbClear.Enabled := False;
    sbReset.Enabled := False;
    sbSyntax.Enabled := False;
    sbTest.Enabled := False;
  end;

  us := DMS_TreeItem_GetProgressState(m_tiSelected);
  isFailed := DMS_TreeItem_IsFailed  (m_tiSelected);

  dmfGeneral.SetButtonImage(sbSyntax, iif(us >= NC2_DataReady, 1, iif(isFailed, 2, 0)));
  dmfGeneral.SetButtonImage(sbTest,   iif(us >= NC2_Validated, 4, iif(isFailed, 5, 3)));

  InitDependenciesTree;
end;

procedure TfrmExprEdit.ReadExpr;
begin
  mmExpression.Text  := string( TreeItem_GetExprOrSourceDescr(m_tiSelected, @m_bFakeExpr) );
  m_sOriginalExpression := mmExpression.Text;
end;

// INHERITED

function TfrmExprEdit.InitFromDesktop(tiView: TTreeItem): boolean;
begin
  Result := true;
end;


// EVENTHANDLERS

// Drag & Drop TreeItems
procedure TfrmExprEdit.mmExpressionDragOver(Sender, Source: TObject; X,
  Y: Integer; State: TDragState; var Accept: Boolean);
begin
  inherited;

  Accept := (Source is TTreeView) and Assigned(TTreeView(Source).Selected) and Assigned(TTreeView(Source).Selected.Data);
end;


procedure TfrmExprEdit.mmExpressionDragDrop(Sender, Source: TObject; X, Y: Integer);
var
  scrap: string;
begin
  inherited;

  if((Source is TTreeView) and Assigned(TTreeView(Source).Selected) and Assigned(TTreeView(Source).Selected.Data)) then
    scrap := TreeItem_GetFullName_Save(TTreeView(Source).Selected.Data)
  else exit;

  mmExpression.SetSelTextBuf(PChar(scrap));
end;

// Clear expression
procedure TfrmExprEdit.sbClearClick(Sender: TObject);
begin
  inherited;
  mmExpression.Lines.Clear;
  SetButtonStates;
end;

// Apply changes
procedure TfrmExprEdit.sbApplyClick(Sender: TObject);
begin
  inherited;
  if(ApplyChanges)
  then begin
    InitDependenciesTree;
    DispMessage('Changes applied successfully.');
  end;
end;

// Closing the form
procedure TfrmExprEdit.FormCloseQuery(Sender: TObject; var CanClose: Boolean);
begin
  inherited;

  if(m_sOriginalExpression <> mmExpression.Text)
  then begin
     case MessageDlg('Save changes to ' + DMS_TreeItem_GetName(m_tiSelected) + '?', mtWarning, mbYesNoCancel, 0) of
       mrYes:    CanClose := ApplyChanges;
       mrCancel: CanClose := false;
     end;
  end;
end;

// Editing the expression
procedure TfrmExprEdit.mmExpressionChange(Sender: TObject);
begin
  inherited;
  dmfGeneral.SetButtonImage(sbSyntax, 0);
  dmfGeneral.SetButtonImage(sbTest, 3);
  Application.ProcessMessages;
  SetButtonStates;
end;

// Selecting a treeitem
procedure TfrmExprEdit.tvDependenciesChanging(Sender: TObject;
  Node: TTreeNode; var AllowChange: Boolean);
begin
  inherited;
  if not Assigned(Node.Data)
    or (DMS_TreeItem_InHidden(Node.Data) and not dmfGeneral.AdminMode)
  then AllowChange := false
end;

procedure TfrmExprEdit.tvDependenciesChange(Sender: TObject;
  Node: TTreeNode);
begin
  inherited;
  if not Assigned(Node) then
    SetFocusedControl(mmExpression)
  else
  begin
    assert(Assigned(Node.Data));
    dmfGeneral.SetActiveTreeItem(Node.Data, tvDependencies);
  end
end;


// Delphi bug workaround
procedure TfrmExprEdit.tvDependenciesContextPopup(Sender: TObject;
  MousePos: TPoint; var Handled: Boolean);
begin
  inherited;
  tvDependencies.Selected := tvDependencies.Selected;
end;


// METHODS

// Apply any pending changes
function TfrmExprEdit.ApplyChanges: boolean;
var
  sExp: ItemString;
  parseresult : TParseResult;
  errCount : Cardinal;
  tiTarget, tiTarget2, tiTempl: TTreeItem;
begin
  Result := false;

  sExp := ItemString( mmExpression.Text );

  errCount := frmMain.m_nErrorCount;

  parseresult := DMS_TreeItem_CreateParseResult(m_tiSelected, PItemChar(sExp), true);

  if(Assigned(parseresult))
  then try
    if not DMS_ParseResult_CheckSyntax(parseresult) then exit;


    tiTarget := m_tiSelected;

    while (errCount = frmMain.m_nErrorCount) and DMS_TreeItem_IsEndogenous(tiTarget)
    do begin
       if not DMS_ParseResult_CheckResultingTreeItem(parseresult, tiTarget) then exit;

       tiTempl := DMS_TreeItem_GetTemplInstantiator(tiTarget);
       repeat
         if (not assigned(tiTempl)) then
         begin
            MessageDlg(
                  string( TreeItem_GetFullName_Save(tiTarget) )+' is endogenous' + #10#13
                +'but not part of an instantiated template;' + #10#13
                +'CalculationRule cannot be assignged',
              mtWarning, [mbOK], 0
            );
            exit;
         end;
         Assert(assigned(DMS_TreeItem_GetParent(tiTempl))); // root can never be template instantiation

         tiTarget2 := DMS_TreeItem_GetTemplSourceItem(tiTempl, tiTarget);
         if not assigned(tiTarget2) then
            tiTempl := DMS_TreeItem_GetTemplInstantiator(DMS_TreeItem_GetParent(tiTempl));
       until assigned(tiTarget2);

       if MessageDlg(TreeItem_GetFullName_Save(tiTarget) + #10#13
            + 'is part of an instantiation of template '
            + TreeItem_GetFullName_Save(DMS_TreeItem_GetTemplSource(tiTempl)) + #10#13
            + 'Do you want to change the CalculationRule for ' + TreeItem_GetFullName_Save(tiTarget2),
         mtWarning, [mbYes, mbNo], 0) = mrYes
       then
         tiTarget := tiTarget2
       else
         exit; // don't change
    end;

    if (errCount = frmMain.m_nErrorCount) // detect shown errors
    then begin
      DMS_TreeItem_SetExpr(tiTarget, PItemChar(sExp)); // should be visible at m_tiSelected

      ReadExpr;
    end;
    if (errCount = frmMain.m_nErrorCount) then Result := true;
  finally
    DMS_ParseResult_Release(parseResult);
  end;
end;

procedure TfrmExprEdit.InitDependenciesTree;
var tnNew: TTreeNode;
begin
  tvDependencies.Items.Clear;
  tnNew := tvDependencies.Items.AddChildObject(nil, TreeItem_GetFullName_Save(m_tiSelected), m_tiSelected);
  BuildDependenciesTree(m_tiSelected, tnNew);
  SetButtonStates;
end;

type TSupplVisitor = record
   frm:       TFrmExprEdit;
   tiParent:  TTreeItem;
   tnCurrent: TTreeNode;
end;
type PSupplVisitor = ^TSupplVisitor;

function AddSupplFunc(clientHandle: TClientHandle; tiSuppl: TTreeItem): Boolean; cdecl;
var
  fullName: String;
  pr: TParseResult;
  psv: PSupplVisitor;
  nIdx : integer;
  Node: TTreeNode;
begin
    AddSupplFunc := true;
    assert(Assigned(tiSuppl));
    psv := PSupplVisitor(clientHandle);
    if tiSuppl = psv^.tiParent then exit; // skip parent
    for nIdx := 0 to psv^.tnCurrent.Count - 1 do
      if psv^.tnCurrent.Item[nIdx].Data = tiSuppl then exit; // already added

    fullName := TreeItem_GetFullName_Save(tiSuppl);
    if (fullName = '') then
    begin
      pr := DMS_TreeItem_GetParseResult(tiSuppl);
      if assigned(pr) then
        FullName := 'slisp: ' + DMS_ParseResult_GetAsSLispExpr(pr, false)
      else
        FullName := '<unknown>';
    end;
    if DMS_TreeItem_InHidden(tiSuppl) and not dmfGeneral.AdminMode then
      fullName := 'hidden: ' + fullName;

    Node := psv^.frm.tvDependencies.Items.AddChildObject(psv^.tnCurrent, fullName, tiSuppl);
    TreeNode_AddDummy(Node);
end;

procedure TfrmExprEdit.BuildDependenciesTree(ti: TTreeItem; tnCurrent: TTreeNode);
var
  sv: TSupplVisitor;
begin
  if(ti = nil) then Exit;
  sv.frm := self;
  sv.tiParent  := DMS_TreeItem_GetParent(ti);
  sv.tnCurrent := tnCurrent;
  try
    DMS_TreeItem_VisitSuppliers(ti, Addr(sv), NC2_DataReady, AddSupplFunc);
  except
  end;
end;

// Set the state of the buttons
procedure TfrmExprEdit.SetButtonStates;
begin
  sbApply.Enabled  := mmExpression.Text <> m_sOriginalExpression;
  sbReset.Enabled  := mmExpression.Text <> m_sOriginalExpression;
  sbClear.Enabled  := length(mmExpression.Text) > 0;
  sbSyntax.Enabled := length(mmExpression.Text) > 0;
  sbTest.Enabled   := length(mmExpression.Text) > 0;
end;

procedure TfrmExprEdit.tvDependenciesExpanding(Sender: TObject;
  Node: TTreeNode; var AllowExpansion: Boolean);
begin
  inherited;

  assert(assigned(Node.Data));
  Node.DeleteChildren;

  BuildDependenciesTree(Node.Data, Node);
end;

procedure TfrmExprEdit.tvDependenciesCollapsing(Sender: TObject;
  Node: TTreeNode; var AllowCollapse: Boolean);
begin
  Node.DeleteChildren;
  TreeNode_AddDummy(Node);
  m_TreeNodeUpdateSet.OnCollapse;
end;


procedure TfrmExprEdit.SetActiveTreeNode(tiActive: TTreeItem; oSender: TObject);
var
  nIdx, nCnt : integer;
begin
  if(oSender <> tvDependencies)
  then begin
    try
      tvDependencies.OnChanging := nil;
      nCnt := tvDependencies.Items.Count;
      if assigned(tiActive) then
        for nIdx := 0 to nCnt - 1 do
        begin
          if(tvDependencies.Items[nIdx].Data = tiActive)
          then begin
            if(not tvDependencies.Items[nIdx].Selected)
            then begin
              tvDependencies.Items[nIdx].MakeVisible;
              tvDependencies.Items[nIdx].Selected := true;
            end;
            break;
          end;
        end;

      if (nIdx = nCnt) or not assigned(tiActive)
      then tvDependencies.Selected := nil;
    finally
      tvDependencies.OnChanging := tvDependenciesChanging;
    end;
  end;
end;


procedure TfrmExprEdit.FormCreate(Sender: TObject);
begin
  inherited;
  dmfGeneral.RegisterATIDisplayProc(SetActiveTreeNode);
  m_TreeNodeUpdateSet := TTreeNodeUpdateSet.Create(tvDependencies, false);
end;

procedure TfrmExprEdit.FormDestroy(Sender: TObject);
begin
  inherited;
  m_TreeNodeUpdateSet.Free;
  m_TreeNodeUpdateSet := Nil;
  dmfGeneral.UnRegisterATIDisplayProc(SetActiveTreeNode);
end;

procedure TfrmExprEdit.FormClose(Sender: TObject; var Action: TCloseAction);
begin
  inherited;
  // NOP
end;

procedure TfrmExprEdit.sbSyntaxClick(Sender: TObject);
var pc: TParseResultContext;
 expr: ItemString;
begin
  expr := mmExpression.Lines.GetText;
  pc := TParseResultContext.Create(m_tiSelected, expr, true);
  if assigned(pc) then
  try
    if pc.CheckSyntax
    then begin
      dmfGeneral.SetButtonImage(sbSyntax, 1);
      DispMessage('Syntax check ok.');
    end
    else
    begin
      dmfGeneral.SetButtonImage(sbSyntax, 2);
      pc.DispError;
    end
  finally
    pc.free;
  end;
end;

procedure TfrmExprEdit.sbTestClick(Sender: TObject);
var pc: TParseResultContext;
  bResult : boolean;
begin
  pc := TParseResultContext.Create(m_tiSelected, PItemChar(mmExpression.Lines.GetText), true);
  if assigned(pc) then
  try
    try
      DisableControls(true);
      bResult := pc.TestExpression;
    finally
      DisableControls(false);
    end;

    if bResult
    then begin
      dmfGeneral.SetButtonImage(sbTest, 4);
      dmfGeneral.SetButtonImage(sbSyntax, 1);
      DispMessage('Update check ok.');
    end
    else
    begin
      dmfGeneral.SetButtonImage(sbTest, 5);
      pc.DispError;
    end
  finally
    pc.free;
  end;
end;

constructor TParseResultContext.Create(context: TTreeItem; expr: ItemString; contextIsTarget: Boolean);
var
   errCount: Cardinal;
begin
  assert(Assigned(context));
  m_ErrMsg := '';
  m_tiSelected := context;
  m_ParseResult := Nil;
  m_ContextIsTarget := contextIsTarget;
  errCount := frmMain.m_nErrorCount;
//   SuspendErrorFeedback := false;

  m_ParseResult := DMS_TreeItem_CreateParseResult(context, PItemChar(expr), contextIsTarget);
  if not Assigned(m_ParseResult) then LogError(errCount);
end;

destructor TParseResultContext.Destroy;
begin
  if Assigned(m_ParseResult) then
      DMS_ParseResult_Release(m_ParseResult);
end;

function TParseResultContext.LogError(errCount: Cardinal): Boolean;
begin
    Result := (errCount > frmMain.m_nErrorCount);
    if Result then
      m_ErrMsg := m_ErrMsg + DMS_GetLastErrorMsg;
end;

procedure TParseResultContext.DispError;
begin
    if m_ErrMsg <> '' then
      TfrmErrorMsg.DisplayError(string( m_ErrMsg ), false);
end;

function TParseResultContext.HasParseResult: Boolean;
begin
  Result := Assigned(m_ParseResult);
end;

function TParseResultContext.CheckSyntaxFunc(var tiResult: TTreeItem): Boolean;
var errCount: Cardinal;
begin
  errCount := frmMain.m_nErrorCount;
  Result := DMS_ParseResult_CheckSyntax(m_ParseResult);
  if not Result then
  if not LogError(errCount) then
  begin
    if DMS_ParseResult_HasFailed(m_ParseResult)
    then m_ErrMsg := DMS_ReleasedIString2String(DMS_ParseResult_GetFailReasonAsIString(m_ParseResult))
    else m_ErrMsg := 'CheckSyntax: unknown error';
  end;

  if Result then
  begin
    tiResult := DMS_ParseResult_CreateResultingTreeItem(m_ParseResult);
    Result := not LogError(errCount);
  end
  else tiResult := Nil;

  if Result and m_ContextIsTarget then
  begin
     Result := DMS_ParseResult_CheckResultingTreeItem(m_ParseResult, m_tiSelected) AND not LogError(errCount);
     if not Result then
       m_ErrMsg := 'Semantically correct, but calculation result is incompatible with target item:'#10#13 + m_ErrMsg;
  end;
end;

function TParseResultContext.CheckSyntax: Boolean;
var
   tiResult: TTreeItem;
begin
   Result := HasParseResult and CheckSyntaxFunc(tiResult);
end;

function TParseResultContext.TestExpression: Boolean;
var
   tiResult : TTreeItem;
   errCount: Cardinal;
begin
   if not Assigned(m_ParseResult) then
     Result := false
   else
   begin
      errCount := frmMain.m_nErrorCount;
      Result := CheckSyntaxFunc(tiResult);

      if Result then
      begin
         DMS_TreeItem_Update(tiResult);
         if LogError(errCount) then
           m_ErrMsg := 'Semantically correct, but error message occured during data derivation.'#10#13 + m_ErrMsg
         else if DMS_TreeItem_IsFailed(tiResult) then
           m_ErrMsg := DMS_ReleasedIString2String(DMS_TreeItem_GetFailReasonAsIString(tiResult));
      end
   end;
end;

procedure TfrmExprEdit.sbResetClick(Sender: TObject);
begin
  inherited;
  mmExpression.Text := m_sOriginalExpression;
end;

procedure TfrmExprEdit.DisableControls(bDisable : boolean);
const
  bClear        : boolean = false;
  bApply        : boolean = false;
  bReset        : boolean = false;
  bSyntax       : boolean = false;
  bTest         : boolean = false;
  bExpression   : boolean = false;
  bDependencies : boolean = false;
begin
  if(bDisable)
  then begin
    bClear        := sbClear.Enabled;
    bApply        := sbApply.Enabled;
    bReset        := sbReset.Enabled;
    bSyntax       := sbSyntax.Enabled;
    bTest         := sbTest.Enabled;
    bExpression   := mmExpression.Enabled;
    bDependencies := tvDependencies.Enabled;

    sbClear.Enabled        := false;
    sbApply.Enabled        := false;
    sbReset.Enabled        := false;
    sbSyntax.Enabled       := false;
    sbTest.Enabled         := false;
    mmExpression.Enabled   := false;
    tvDependencies.Enabled := false;
  end
  else begin
    sbClear.Enabled        := bClear;
    sbApply.Enabled        := bApply;
    sbReset.Enabled        := bReset;
    sbSyntax.Enabled       := bSyntax;
    sbTest.Enabled         := bTest;
    mmExpression.Enabled   := bExpression;
    tvDependencies.Enabled := bDependencies;
  end;
end;

procedure TfrmExprEdit.tvDependenciesCustomDrawItem(
  Sender: TCustomTreeView; Node: TTreeNode; State: TCustomDrawState;
  var DefaultDraw: Boolean);
begin
  // DEBUG, deze functie blijft alleen als handler aanwezig
  // om procedure TCustomTreeView.CNNotify(var Message: TWMNotify);
  // CDRF_NOTIFYITEMDRAW te laten retourneren wanneer (dwDrawStage and CDDS_ITEM) = 0
  exit;
end;

procedure TfrmExprEdit.WMNotify(var Message: TWMNotify); //message WM_NOTIFY;
begin

  if not (assigned(m_TreeNodeUpdateSet) and TreeView_OnDrawTreeNode(tvDependencies, m_TreeNodeUpdateSet, Message)) then
    inherited;
end;


procedure TfrmExprEdit.tvDependenciesDblClick(Sender: TObject);
begin
  dmfGeneral.pmTreeItemPopup(nil);
  dmfGeneral.miDefaultView.Click;
end;


end.
