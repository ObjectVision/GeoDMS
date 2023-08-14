//<HEADER>
//</HEADER>
unit fMain;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  Menus, ComCtrls, StdCtrls, uDMSInterfaceTypes, ExtCtrls, ImgList,
  Buttons, ToolWin, OleCtrls, SHDocVw, RecentFiles,
  TreeNodeUpdateSet, uViewAction, ActiveX;

type
  TStatusbarPanel = (sbpHints, sbpMain, sbpDesktop, sbpTime);

  TfrmMain = class(TForm)
    StatusBar: TStatusBar;
    pnlEventlog: TPanel;
    lbxEventLog: TListBox;
    spltrTreeView: TSplitter;
    clbrMain: TCoolBar;

    imglstPageControl: TImageList;
    imglstWebbrowser: TImageList;

    pnlExpl: TPanel;
    ToolBar2: TToolBar;

    tlbtnBack: TToolButton;
    tlbtnForward: TToolButton;

    pgcntrlData: TPageControl;

    tbshtGeneral: TTabSheet;
    tbshtExplore: TTabSheet;
    tbshtPropAdvanced: TTabSheet;

    tbshtStatistics: TTabSheet;
    tbshtValueInfo: TTabSheet;
    tbshtMetadata: TTabSheet;
    tbshtSourceDescr: TTabSheet;
    tbshtConfig: TTabSheet;

    wbMetaData: TWebBrowser;

    spltrExplorer: TSplitter;
    pnlMdiToolbar: TPanel;
    mnuMain: TMainMenu;
    File1: TMenuItem;
    Edit1: TMenuItem;
    View1: TMenuItem;
    Tools1: TMenuItem;
    Window1: TMenuItem;
    Help1: TMenuItem;
    OpenConfigurationRoot1: TMenuItem;
    N5: TMenuItem;
    OpenDesktop1: TMenuItem;
    N8: TMenuItem;
    Exit1: TMenuItem;
    miEditDefinition: TMenuItem;
    miDatagridView: TMenuItem;
    miHistogramView: TMenuItem;
    N9: TMenuItem;
    miProcessSchemesSub: TMenuItem;
    miSubitemsView: TMenuItem;
    miSuppliersView: TMenuItem;
    miExpressionView: TMenuItem;
    N10: TMenuItem;
    Tree2: TMenuItem;
    DetailPages1: TMenuItem;
    Eventlog1: TMenuItem;
    miOptions: TMenuItem;
    TileHor: TMenuItem;
    Cascade1: TMenuItem;
    miOnlineHelp: TMenuItem;
    miAbout: TMenuItem;
    ilTreeview: TImageList;
    NewDesktop1: TMenuItem;
    spltrEventlog: TSplitter;
    SetStartupDesktop1: TMenuItem;
    CloseAll1: TMenuItem;


    miDefaultView: TMenuItem;
    miExportPrimaryData: TMenuItem;
    miExportAsciiGrid: TMenuItem;
    miExportBitmap: TMenuItem;
    miExportCsvTable: TMenuItem;
    miExportDbfShape: TMenuItem;
    N2: TMenuItem;
    miEditClassificationandPalette: TMenuItem;
    miUpdateSubTree: TMenuItem;
    miUpdateTreeitem: TMenuItem;
    N3: TMenuItem;
    miMapView: TMenuItem;
    miWinSep: TMenuItem;
    sepRecentfiles: TMenuItem;
    rfMain: TRecentFiles;
    N7: TMenuItem;
    miCopyrightNotice: TMenuItem;
    miDisclaimer: TMenuItem;
    miSourceDescr: TMenuItem;
    tvTreeItem: TTreeView;
    miDebugReport: TMenuItem;
    Toolbar1: TMenuItem;
    miSaveDesktop: TMenuItem;
    miExportViewPorts: TMenuItem;
    ReopenConfig: TMenuItem;
    miEditConfigSource: TMenuItem;
    miInvalidateTreeItem: TMenuItem;
    miCloseWindow: TMenuItem;
    TileVer: TMenuItem;
    clbrCurrentItem: TCoolBar;
    edtCurrentItem: TEdit;
    CurrentItem1: TMenuItem;
    HiddenItems1: TMenuItem;
    SourceDescrvariant1: TMenuItem;
    miConfiguredSourceDescr: TMenuItem;
    miReadonlyStorageManagers: TMenuItem;
    miNonReadonlyStoragemanagers: TMenuItem;
    miAllStorageManagers: TMenuItem;
    CloseAllButThis: TMenuItem;

    procedure LogMsg(msg: PMsgChar);

    procedure FormCreate(Sender: TObject);
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
    procedure FormDestroy(Sender: TObject);
    procedure lbxEventLogDblClick(Sender: TObject);
    procedure tvTreeItemExpanding(Sender: TObject; Node: TTreeNode;
      var AllowExpansion: Boolean);
    procedure tvTreeItemDblClick(Sender: TObject);
    procedure tlbtnBackClick(Sender: TObject);
    procedure tlbtnForwardClick(Sender: TObject);
    procedure tvTreeItemContextPopup(Sender: TObject; MousePos: TPoint;
      var Handled: Boolean);
    procedure File1Click(Sender: TObject);
    procedure Edit1Click(Sender: TObject);
    procedure View1Click(Sender: TObject);
    procedure Tools1Click(Sender: TObject);
    procedure Window1Click(Sender: TObject);
    procedure Help1Click(Sender: TObject);
    procedure OpenConfigurationRoot1Click(Sender: TObject);
    procedure OpenDesktop1Click(Sender: TObject);
    procedure Exit1Click(Sender: TObject);
    procedure miEditDefinitionClick(Sender: TObject);
    procedure miMapViewClick(Sender: TObject);
    procedure miDatagridViewClick(Sender: TObject);
    procedure miHistogramViewClick(Sender: TObject);
    procedure miSubitemsViewClick(Sender: TObject);
    procedure miSuppliersViewClick(Sender: TObject);
    procedure miExpressionViewClick(Sender: TObject);
    procedure Tree2Click(Sender: TObject);
    procedure DetailPages1Click(Sender: TObject);
    procedure Eventlog1Click(Sender: TObject);
    procedure miOptionsClick(Sender: TObject);
    procedure TileHorClick(Sender: TObject);
    procedure Cascade1Click(Sender: TObject);
    procedure miOnlineHelpClick(Sender: TObject);
    procedure miAboutClick(Sender: TObject);
    procedure ModifyHiddenNodesState;
    procedure FormDragOver(Sender, Source: TObject; X, Y: Integer;
      State: TDragState; var Accept: Boolean);
    procedure FormDragDrop(Sender, Source: TObject; X, Y: Integer);
    procedure tvTreeItemChanging(Sender: TObject; Node: TTreeNode;
      var AllowChange: Boolean);
    procedure NewDesktop1Click(Sender: TObject);
    procedure SetStartupDesktop1Click(Sender: TObject);
    procedure CloseAll1Click(Sender: TObject);
    procedure ViewPropValue(ti: TTreeItem; sPropertyName: ItemString);
    procedure miDefaultViewClick(Sender: TObject);
    procedure miExportBitmapClick(Sender: TObject);
    procedure miExportDbfShapeClick(Sender: TObject);
    procedure miExportAsciiGridClick(Sender: TObject);
    procedure miExportCsvTableClick(Sender: TObject);
    procedure lvExploreEnter(Sender: TObject);
    procedure miEditClassificationandPaletteClick(Sender: TObject);
    procedure miUpdateTreeitemClick(Sender: TObject);
    procedure miUpdateSubTreeClick(Sender: TObject);
    procedure rfMainFileOpen(sFilename: String);
    procedure rfMainFileOpenImpl(sFilename, sDesktop: String);
    procedure tvTreeItemCustomDrawItem(Sender: TCustomTreeView;
      Node: TTreeNode; State: TCustomDrawState; var DefaultDraw: Boolean);

    procedure dmfGeneralAdminModeChanged(sender: TObject);
    procedure dmfGeneralShowStateColorsChanged(sender: TObject);
    procedure miCopyrightNoticeClick(Sender: TObject);
    procedure miDisclaimerClick(Sender: TObject);
    procedure miSourceDescrClick(Sender: TObject);
    procedure miDebugReportClick(Sender: TObject);
    procedure tvTreeItemCollapsing(Sender: TObject; Node: TTreeNode;
      var AllowCollapse: Boolean);
    procedure Toolbar1Click(Sender: TObject);
    procedure miSaveDesktopClick(Sender: TObject);
    procedure UpdateMainMenuVisibility;
    procedure miExportViewPortsClick(Sender: TObject);
    procedure ReopenConfigClick(Sender: TObject);
    procedure miEditConfigSourceClick(Sender: TObject);
    procedure miInvalidateTreeItemClick(Sender: TObject);
    procedure miCloseWindowClick(Sender: TObject);
    procedure tvTreeItemChange(Sender: TObject; Node: TTreeNode);
    procedure TileVerClick(Sender: TObject);
    procedure wbMetaDataBeforeNavigate2(ASender: TObject;
      const pDisp: IDispatch; const URL, Flags, TargetFrameName, PostData,
      Headers: OleVariant; var Cancel: WordBool);
    procedure CurrentItem1Click(Sender: TObject);
    procedure edtCurrentItemSearch;
    procedure edtCurrentItemKeyUp(Sender: TObject; var Key: Word; Shift: TShiftState);
    procedure HiddenItems1Click(Sender: TObject);
    procedure miConfiguredSourceDescrClick(Sender: TObject);
    procedure miReadonlyStorageManagersClick(Sender: TObject);
    procedure miNonReadonlyStoragemanagersClick(Sender: TObject);
    procedure miAllStorageManagersClick(Sender: TObject);
    procedure CloseAllButThisClick(Sender: TObject);
    procedure UpdateCaption;
    procedure ProcessUnfoundPart(upStr: ItemString);

  private
    m_StatisticsInterest: TTreeItem;
    m_TreeNodeUpdateSet:  TTreeNodeUpdateSet;

    procedure SetSourceDescr(sdm: TSourceDescrMode);
    procedure OnShowHint(Sender: TObject);
    function  CloseCurrentConfiguration: boolean;

    procedure SaveDetailPage(s: PFileChar);
    procedure WMNotify(var Message: TWMNotify); message WM_NOTIFY;
    procedure WMCopyData(var Message: TWMCopyData); message WM_COPYDATA;

    function  LoadConfiguration(sFilename: FileString; sDesktop: ItemString): boolean;
    procedure LoadDMSConfigDllInfo(const sFilename : FileString; var isD5Dll: Boolean);

    procedure SetStatisticsInterest(si: TTreeItem);
    function  TryToCloseConfig: Boolean;

  public
    m_nErrorCount: Cardinal;
    m_CurrSDM: TSourceDescrMode;

    procedure rfReopen;
    procedure UpdateDataInfo(var action: TViewAction);
    procedure SetMdiToolbar(pnlToolbar: TPanel);
    procedure MnuMdiWindowClick(Sender: TObject);
    function  LoadConfigurationAndProjectDll(sFilename, sDesktop: FileString; mustLoadDll, mustShowError: Boolean): Boolean;

    procedure SetActiveTreeNode(tiActive: TTreeItem; oSender: TObject);
    procedure SetStatusbarText(sbpIdx: TStatusbarPanel; sValue: string);

    procedure SaveTreeView(tiContainer: TTreeItem);
    procedure RestoreTreeView(tiContainer: TTreeItem);

    procedure SetTreeViewVisible(visible: Boolean);
    procedure SetDetailPageVisible(visible: Boolean);
    procedure SetEventLogVisible(visible: Boolean);
    procedure SetToolBarVisible (visible: Boolean);
    procedure SetCurrentItemBarVisible(visible: Boolean);
    function  TryToCloseForm(mayStayOpen: Boolean): Boolean;

    procedure BeginTiming;
    procedure EndTiming;
    procedure ChangeCursor(csr: TCursor);

    property StatusBarText[sbpIdx: TStatusbarPanel]: string write SetStatusbarText;
  end;

  const g_frmMain : TfrmMain = nil;
  function frmMain: TfrmMain;

  procedure OnStateChanged(clientHandle: TClientHandle; item: TTreeItem; newState: TUpdateState); cdecl;
  procedure OnContextNotification(clientHandle: TClientHandle; description: PMsgChar); cdecl;


implementation

{$R *.DFM}



uses
  contnrs,
  dmGeneral,
  fmDmsControl,
  fmBasePrimaryDataControl,
  fTreeItemPickList,
  fErrorMsg,
  Configuration,
  fEditProperty,
  fSettings,
  uDMSTreeviewFunctions,
  uTreeViewUtilities,
  uDMSUtils,
  uGenLib,
  uDmsInterface,
  uAppGeneral,
  DMSProjectInterface,
  fPrimaryDataViewer,
  ItemColumnProvider,
  OptionalInfo,
  CommCtrl,
  uDMSDesktopFunctions,
  ShlObj,
  FileCtrl,
  System.Variants,
  System.Math,
  MSHTML;




function frmMain: TfrmMain;
begin
  Result := g_frmMain;
  if(g_frmMain = nil)
  then raise Exception.Create('The mainform has not been created.');
end;

{*******************************************************************************
                              TfrmMain
*******************************************************************************}

procedure TfrmMain.FormCreate(Sender: TObject);
begin
  System.Math.SetExceptionMask(exAllArithmeticExceptions);
  SetGlobalMainWindowHandle(Pointer(Handle));
  m_StatisticsInterest := Nil;
  tvTreeItem.PopupMenu := dmfGeneral.pmTreeItem;
  m_CurrSDM := SDM_Configured;

  Assert(Assigned(dmfGeneral));

  with dmfGeneral do
  begin
     OnAdminModeChanged := dmfGeneralAdminModeChanged;
     OnShowStateColorsChanged := dmfGeneralShowStateColorsChanged;
     RegisterATIDisplayProc(SetActiveTreeNode);
  end;

  m_nErrorCount := 0;

  DMS_RegisterMsgCallback(LogMsgCallback, TClientHandle(Self));
  DMS_SetCoalesceHeapFunc(CoalesceHeap);
{$IFDEF DEBUG}
  LogMsg('FormCreate');
{$ENDIF}
  Application.OnHint := OnShowHint;

  UpdateCaption;

  pgcntrlData.ActivePage := tbshtGeneral;

  m_TreeNodeUpdateSet := TTreeNodeUpdateSet.Create(tvTreeItem, true);

  DMS_RegisterStateChangeNotification(OnStateChanged, TClientHandle(self));
  DMS_SetContextNotification(OnContextNotification,  TClientHandle(self));
  DMS_SetGlobalCppExceptionTranslator(CppExceptionTranslator);
  DMS_SetGlobalSeTranslator(SeTranslator);
  SHV_SetCreateViewActionFunc( DoOrCreateViewActionFromDll );

  rfMain.Registrykey := 'ObjectVision\DMS\RecentFiles';
  SetTreeViewVisible  (dmfGeneral.GetStatus(SF_TreeViewVisible));
  SetDetailPageVisible(dmfGeneral.GetStatus(SF_DetailsVisible));
  SetEventLogVisible  (dmfGeneral.GetStatus(SF_EventLogVisible));
  SetToolBarVisible   (false);
  SetCurrentItemBarVisible(not dmfGeneral.GetStatus(SF_CurrentItemBarHidden ));

  UpdateMainMenuVisibility;
{$IFDEF DEBUG}
  miDebugReport.visible     := true;
{$ELSE}
  miDebugReport.visible     := dmfGeneral.AdminMode;
{$ENDIF}
end;

procedure TfrmMain.UpdateMainMenuVisibility;
begin
  Edit1  .Visible := dmfGeneral.AdminMode;
  miProcessSchemesSub.Visible := dmfGeneral.AdminMode;
end;

function TfrmMain.TryToCloseConfig: Boolean;
begin
    Result := true;
    if not TConfiguration.CurrentIsAssigned then exit;
    Result := CloseCurrentConfiguration;
end;

function TfrmMain.TryToCloseForm(mayStayOpen: Boolean): Boolean;
begin
  Result := TryToCloseConfig;
  if mayStayOpen and not(Result) then exit;

  DMS_ReleaseStateChangeNotification(OnStateChanged, TClientHandle(self));
  DMS_SetContextNotification(Nil, Nil);
  DMS_SetGlobalCppExceptionTranslator(Nil);
  DMS_SetGlobalSeTranslator(Nil);
  SHV_SetCreateViewActionFunc(Nil);

  m_TreeNodeUpdateSet.Free;

  Screen.OnActiveFormChange := nil;
end;

procedure TfrmMain.FormClose(Sender: TObject; var Action: TCloseAction);
begin
  if not TryToCloseForm(true) then
  begin
    Action := caNone;
    Exit;
  end;
end;

procedure TfrmMain.FormDestroy(Sender: TObject);
begin
{$IFDEF DEBUG}
  LogMsg('FormDestroy');
{$ENDIF}

  DMS_SetCoalesceHeapFunc(Nil);
  DMS_ReleaseMsgCallback(LogMsgCallback, TClientHandle(Self));
  with dmfGeneral do
  begin
    UnRegisterATIDisplayProc(SetActiveTreeNode);
    OnShowStateColorsChanged := Nil;
    OnAdminModeChanged := Nil;
  end;
  SetGlobalMainWindowHandle(nil);
end; // FormDestroy

function TfrmMain.CloseCurrentConfiguration: boolean;
begin
  try
    g_History.ClearAll;
    tvTreeItem.Items.Clear;
    m_TreeNodeUpdateSet.Reset(false);
    dmfGeneral.SetActiveTreeItem(nil, tvTreeItem);
    SetStatisticsInterest(Nil);

    Result := TConfiguration.CloseCurrentConfiguration;
  except
    Application.HandleException(Self);
    Result := false;
  end;
end;

procedure TfrmMain.OnShowHint(Sender: TObject);
begin
  StatusBarText[sbpHints] := Application.Hint;
end; // OnShowHint

function CrPos(s: MsgString): UInt32;
begin
    Result := 1;
    while Result <= length(s) do
    begin
      if s[Result] = chr(10) then exit;
      INC(Result);
    end;
    Result := 0;
end;

procedure TfrmMain.LogMsg(msg: PMsgChar);
var s: MsgString; p: UInt32;
begin
  if not pnlEventlog.Visible then exit;

  s := MsgString( msg );
  p := CrPos(s); // count #bytes and not UFT8 chars

  while (p > 0) do
  begin
    if (p > 1) then
        lbxEventLog.Items.Add(Copy(s, 1, p - 1));
    s := Copy(s, p+1, length(s) );
    p := CrPos(s);
  end;
  if (length(s) > 0) then
     lbxEventLog.Items.Add(s);
  while (lbxEventLog.Items.Count > 500) do
     lbxEventLog.Items.Delete(0);

  lbxEventLog.ItemIndex := lbxEventLog.Items.Count - 1;
  lbxEventLog.Update;
end; // LogMsg

procedure TfrmMain.lbxEventLogDblClick(Sender: TObject);
var
  item: string;
begin
    item := lbxEventLog.Items[ lbxEventLog.ItemIndex ];
    TfrmErrorMsg.StartSourceEditor(item);
end;


procedure TfrmMain.tvTreeItemChanging(Sender: TObject; Node: TTreeNode;
  var AllowChange: Boolean);
begin
  assert(Assigned(Node));
  assert(Assigned(Node.Data));
end;

procedure TfrmMain.tvTreeItemChange(Sender: TObject; Node: TTreeNode);
begin
  if Assigned(Node) then
    dmfGeneral.SetActiveTreeItem(TreeNode_GetTreeItem(Node), tvTreeItem)
end;

procedure TfrmMain.tvTreeItemExpanding(Sender: TObject; Node: TTreeNode;
  var AllowExpansion: Boolean);
var
   ti: TTreeItem;
begin
   ti := TreeNode_GetTreeItem(Node);
   BeginTiming;
   AllowExpansion := DMS_TreeItem_HasSubItems(ti);
   if (AllowExpansion) then
   begin
      Node.DeleteChildren;
      ti := DMS_TreeItem_GetFirstSubItem(ti);
      while Assigned(ti) do
      begin
         TreeView_FillPartial(tvTreeItem, Node, ti);
         ti := DMS_TreeItem_GetNextItem(ti);
      end;
   end;
end;

procedure TfrmMain.tvTreeItemCollapsing(Sender: TObject; Node: TTreeNode;
  var AllowCollapse: Boolean);
begin
  Assert(Assigned(Node.Data));
  Node.DeleteChildren;
  TreeNode_AddDummy(Node);
  m_TreeNodeUpdateSet.OnCollapse;
end;

procedure TfrmMain.tvTreeItemDblClick(Sender: TObject);
begin
  dmfGeneral.pmTreeItemPopup(nil);
  dmfGeneral.miDefaultView.Click;
end;

procedure TfrmMain.SetStatisticsInterest(si: TTreeItem);
begin
  SetInterestRef(m_StatisticsInterest, si);
  if not Assigned(si) then
     DMS_ExplainValue_Clear;
end;

const bActive         : boolean = false;
const bDataInfoCleaned: boolean = false;

procedure TfrmMain.UpdateDataInfo(var action: TViewAction);
var ti: TTreeItem;
 dp: TDetailPage;
 tbsht: TTabSheet;
 actionStr: MsgString;
 done: Boolean;
begin
  // DEBUG
{$IFDEF DEBUG}
  LogMsg(PMsgChar('UpdateDataInfo '+action.Url));
  if action.IsReady
  then LogMsg(' Ready')
  else LogMsg(' TODO');
  if bActive
  then LogMsg(' BUSY')
  else LogMsg(' GO');
  LogMsg(PMsgChar('sExtraInfo '+action.ExtraInfo));
{$ENDIF}
  if not pnlExpl.Visible then action.IsReady := true;
  if bActive or action.IsReady then Exit;
  action.IsReady := true; // be optimistic

  Assert(pnlExpl.Visible);
  dp := DP_Unknown;
  try
    bActive := true;
    if action.VAT = VAT_Url then
    begin
      pgcntrlData.ActivePage := tbshtMetaData;
      SetDocUrl(wbMetaData.DefaultInterface, action.url);
      exit;
    end;

    actionStr := UpperCase(action.Url);
    dp    := dp_FromName(actionStr);
    tbsht := tbsht_FromDP(dp);

{$IFDEF DEBUG}
     LogMsg(PMsgChar(MsgString('ActionStr = ') + actionStr));
{$ENDIF}

    Assert(Assigned(tbsht));
    pgcntrlData.ActivePage := tbsht;

    ti := action.TiFocus;

    if not Assigned(ti) then
    begin
      if bDataInfoCleaned then exit; // leave now if nothing to show and previous call already cleaned everything
      SetDocData(wbMetaData.DefaultInterface, htmlEncodeTextDoc('Item is Deleted'));
      bDataInfoCleaned := true;
      exit;
    end;
    bDataInfoCleaned := false;
    case dp of
      DP_Metadata:
        begin
          ViewMetadata(wbMetadata.DefaultInterface, ti);
        end;
      DP_Explore, DP_General, DP_AllProps, DP_Config:
        begin
{$IFDEF DEBUG}
          LogMsg(PAnsiChar('UpdateDataInfo DP'));
{$ENDIF}
          action.IsReady := ViewDetails(wbMetaData.DefaultInterface, ti, dmfGeneral.AdminMode, dp);
        end;
      DP_SourceDescr:
        begin
          SetDocData(wbMetaData.DefaultInterface, htmlEncodeTextDoc(TreeItemSourceDescr(ti, TSourceDescrMode(action.RecNo))));
        end;
      DP_Statistics:
        begin
          SetStatisticsInterest(ti);
          Assert(length(actionStr) = 4); // required by valid syntax as generated by DMS.
          Assert(copy(actionStr,1, 4) = 'STAT'); // guaranteed by dp_FromName.
          DMS_ExplainValue_Clear;
          done := true;
          SetDocData(wbMetaData.DefaultInterface, htmlEncodeTextDoc(DMS_NumericDataItem_GetStatistics(ti, @done)));
          action.IsReady := done;

{$IFDEF DEBUG}
          LogMsg(PMsgChar(MsgString('Statistics ' + iif(action.IsReady, 'ready', 'suspended'))));
{$ENDIF}

          end;
      DP_ValueInfo:
        begin
          SetStatisticsInterest(ti);
          Assert(copy(actionStr,1, 3) = 'VI.'); // guaranteed by dp_FromName.
          Assert(length(actionStr) = 7); // required by valid syntax as generated by DMS.
          actionStr := copy(actionStr, 4, 4);
          if actionStr = 'GRID' then
            action.IsReady := ViewDetails(wbMetaData.DefaultInterface, ti, dmfGeneral.AdminMode, dp, 2, 0, action.Row, action.Col, PSourceChar(action.ExtraInfo))
          else if actionStr = 'ATTR' then
            action.IsReady := ViewDetails(wbMetaData.DefaultInterface, ti, dmfGeneral.AdminMode, dp, 1, action.RecNo, 0, 0, PSourceChar(action.ExtraInfo));
        end
    end;
  finally
    bActive := false;
    if action.IsReady and (dp <> DP_ValueInfo) then SetStatisticsInterest(nil);
  end
end;

procedure TfrmMain.tlbtnBackClick(Sender: TObject);
begin
  g_History.GoBack;
end;

procedure TfrmMain.tlbtnForwardClick(Sender: TObject);
begin
  g_History.GoForward;
end;

procedure TfrmMain.tvTreeItemContextPopup(Sender: TObject;
  MousePos: TPoint; var Handled: Boolean);
begin
  tvTreeItem.Selected := tvTreeItem.Selected;
end;

procedure TfrmMain.SetMdiToolbar(pnlToolbar: TPanel);
var
  nIdx : integer;
begin
  for nIdx := pnlMdiToolbar.ControlCount - 1 downto 0 do
  begin
    pnlMdiToolbar.Controls[nIdx].Parent := nil;
  end;

  if(pnlToolbar <> nil)
  then begin
    pnlToolbar.Parent := pnlMdiToolbar;
    pnlToolbar.Align := alClient;
    pnlToolbar.Visible := true;
    SetToolBarVisible(true);
  end
  else
    SetToolBarVisible(false);
end;

procedure TfrmMain.MnuMdiWindowClick(Sender: TObject);
begin
  if((Sender is TMenuItem) and (TMenuItem(Sender).GroupIndex = 42))
  then TForm(TMenuItem(Sender).Tag).BringToFront;
end;

procedure TfrmMain.File1Click(Sender: TObject);
var
   bConfig, bDataItem : Boolean;
   tiActive: TTreeItem;
begin
   bConfig                         := TConfiguration.CurrentIsAssigned;
   OpenConfigurationRoot1.Enabled  := true;
   ReopenConfig.Enabled            := bConfig;

   NewDesktop1.Enabled             := bConfig;
   OpenDesktop1.Enabled            := bConfig;
   SetStartupDesktop1.Enabled      := bConfig;
   miSaveDesktop.Enabled           := bConfig;

   Exit1.Enabled                   := true;

   // Export Primary Data
   tiActive  := dmfGeneral.GetActiveTreeItem(DMS_TreeItem_GetStaticClass);
   bDataItem := Assigned(dmfGeneral.GetActiveTreeItem(DMS_AbstrDataItem_GetStaticClass));

   miExportAsciiGrid.Enabled        := Assigned(tiActive) and not DMS_TreeItem_InTemplate(tiActive);
   miExportBitmap.Enabled           := bDataItem AND not DMS_TreeItem_InTemplate(tiActive);
   miExportDbfShape.Enabled         := Assigned(tiActive) AND dmfGeneral.GetExportDbfShapeEnabled;
   miExportCsvTable.Enabled         := Assigned(tiActive) AND dmfGeneral.GetExportCsvTableEnabled;
   miExportPrimaryData.Enabled      := miExportBitmap.Enabled or miExportCsvTable.Enabled or miExportAsciiGrid.Enabled or miExportDbfShape.Enabled;

   EndTiming;
end;

procedure TfrmMain.Edit1Click(Sender: TObject);
var bRealTreeItem : Boolean;
begin
   bRealTreeItem := Assigned(dmfGeneral.GetActiveTreeItem(DMS_TreeItem_GetStaticClass));

   miEditConfigSource.Enabled       := bRealTreeItem;
   miEditDefinition.Enabled         := bRealTreeItem and dmfGeneral.GetEditDefinitionEnabled;
   miEditClassificationandPalette.Enabled := bRealTreeItem and dmfGeneral.GetEditClassificationandPaletteEnabled;
   miUpdateTreeItem.Enabled         := bRealTreeItem and dmfGeneral.GetUpdateTreeItemEnabled;
   miUpdateSubTree.Enabled          := bRealTreeItem and DMS_TreeItem_HasSubItems(dmfGeneral.GetActiveTreeItem);
   miInvalidateTreeItem.Enabled     := miUpdateTreeItem.Enabled or miUpdateSubTree.Enabled;

   EndTiming;
end;

procedure TfrmMain.View1Click(Sender: TObject);
var
   bRealTreeItem  : boolean;
begin
   bRealTreeItem := Assigned(dmfGeneral.GetActiveTreeItem(DMS_TreeItem_GetStaticClass));

   miDefaultView.Enabled    := (bRealTreeItem) and dmfGeneral.GetDefaultViewEnabled;

   miMapView.Enabled        := (bRealTreeItem) and dmfGeneral.GetMapViewEnabled;

   miDatagridView.Enabled   := (bRealTreeItem) and dmfGeneral.GetDatagridViewEnabled;
   miHistogramView.Enabled  := (bRealTreeItem) and dmfGeneral.GetHistogramViewEnabled;

   miSubItemsView.Enabled   := (bRealTreeItem) and dmfGeneral.GetSubitemSchemaEnabled;
   miSuppliersView.Enabled  := (bRealTreeItem) and dmfGeneral.GetSupplierSchemaEnabled;
   miExpressionView.Enabled := (bRealTreeItem) and dmfGeneral.GetCalculationSchemaEnabled;
   miProcessSchemesSub.Enabled := miSubItemsView.Enabled or miSuppliersView.Enabled or miExpressionView.Enabled;

   Tree2.Enabled            := true;
   DetailPages1.Enabled     := true;

   Eventlog1.Enabled        := true;
   HiddenItems1.Enabled     := true;
   HiddenItems1.Checked     := dmfGeneral.AdminMode;

   EndTiming;
end;

procedure TfrmMain.Tools1Click(Sender: TObject);
begin
  miOptions.Enabled           := true;
  miDebugReport.Enabled       := dmfGeneral.AdminMode;

   EndTiming;
end;

procedure TfrmMain.Window1Click(Sender: TObject);
var
  nIdx: integer;
  mnuNew : TMenuItem;
begin
  TileHor.Enabled       := (MDIChildCount > 0);
  TileVer.Enabled       := (MDIChildCount > 0);
  Cascade1.Enabled      := (MDIChildCount > 0);
  miCloseWindow.Enabled := (MDIChildCount > 0);
  CloseAll1.Enabled     := (MDIChildCount > 0);
  CloseAllButThis.Enabled := (MDIChildCount > 1);

  // remove menu items
  while(Window1.Items[Window1.Count-1].Name <> 'miWinSep') do
  begin
    Window1.Items[(Window1.Count-1)].Free;
  end;
//  miWinSep.Visible := false;

  // add them again
  mnuNew := nil;
  for nIdx := 0 to MDIChildCount - 1 do
  begin
    mnuNew := TMenuItem.Create(self);
    mnuNew.Name := 'mdiwindow' + inttostr(nIdx);
    mnuNew.Caption := MDIChildren[nIdx].Caption;
    mnuNew.GroupIndex := 42;
    mnuNew.RadioItem := true;
    mnuNew.Tag := integer(MDIChildren[nIdx]);
    mnuNew.Checked := (MDIChildren[nIdx] = ActiveMDIChild);
    mnuNew.OnClick := MnuMdiWindowClick;
    Window1.Add(mnuNew);
  end;
  miWinSep.Visible := (mnuNew <> nil);

   EndTiming;
end;

procedure TfrmMain.Help1Click(Sender: TObject);
begin
  miOnlineHelp.Enabled      := true;
  miAbout.Enabled           := true;
  miCopyRightNotice.Enabled := TfrmOptionalInfo.Exists(OI_CopyRight);
  miDisclaimer.Enabled      := TfrmOptionalInfo.Exists(OI_Disclaimer);
  miSourceDescr.Enabled     := TfrmOptionalInfo.Exists(OI_SourceDescr);

  EndTiming;
end;

procedure TfrmMain.OpenConfigurationRoot1Click(Sender: TObject);
var
  sFilename : string;
begin
  // get filename
  with TOpenDialog.Create(nil) do
  try
    InitialDir := GetCurrentDir;

    Options := Options + [ofNoChangeDir, ofFileMustExist];
    Title := 'Open configuration...';
    DefaultExt := 'dms';
    Filter := 'Configuration files (*.dms)|*.dms';
    FilterIndex := 1;

    if not Execute then exit;
    sFilename := Filename;
  finally
    Free;
  end;

  sFilename := GetMainDMSfile(sFilename);

  rfMainFileOpen(sFilename);
end;


procedure TfrmMain.NewDesktop1Click(Sender: TObject);
begin
  TConfiguration.CurrentConfiguration.DesktopManager.NewDesktop;
  StatusBarText[sbpDesktop] := 'Desktop: ' + TConfiguration.CurrentConfiguration.DesktopManager.GetActiveDesktopDescr;
end;

procedure TfrmMain.OpenDesktop1Click(Sender: TObject);
begin
  TConfiguration.CurrentConfiguration.DesktopManager.OpenDesktop;
  StatusBarText[sbpDesktop] := 'Desktop: ' + TConfiguration.CurrentConfiguration.DesktopManager.GetActiveDesktopDescr;
end;

procedure TfrmMain.SetStartupDesktop1Click(Sender: TObject);
begin
  TConfiguration.CurrentConfiguration.DesktopManager.SetStartupDesktop;
end;

procedure TfrmMain.miSaveDesktopClick(Sender: TObject);
begin
  TConfiguration.CurrentConfiguration.DesktopManager.SaveDesktops;
end;

procedure TfrmMain.Exit1Click(Sender: TObject);
begin
  Close;
end;

procedure TfrmMain.miEditConfigSourceClick(Sender: TObject);
begin
  dmfGeneral.miEditConfigSource.Click;
end;

procedure TfrmMain.miEditDefinitionClick(Sender: TObject);
begin
  dmfGeneral.miEditDefinition.Click;
end;

procedure TfrmMain.miMapViewClick(Sender: TObject);
begin
  dmfGeneral.miMapView.Click;
end;

procedure TfrmMain.miDatagridViewClick(Sender: TObject);
begin
  dmfGeneral.miDatagridView.Click;
end;

procedure TfrmMain.miHistogramViewClick(Sender: TObject);
begin
  dmfGeneral.miHistogramView.Click;
end;

procedure TfrmMain.miSubitemsViewClick(Sender: TObject);
begin
  dmfGeneral.miSubitemsView.Click;
end;

procedure TfrmMain.miSuppliersViewClick(Sender: TObject);
begin
  dmfGeneral.miSuppliersView.Click;
end;

procedure TfrmMain.miExpressionViewClick(Sender: TObject);
begin
  dmfGeneral.miExpressionView.Click;
end;

procedure TfrmMain.Tree2Click(Sender: TObject);
begin
  SetTreeViewVisible( not Tree2.Checked );
end;

procedure TfrmMain.DetailPages1Click(Sender: TObject);
begin
  SetDetailPageVisible( not DetailPages1.Checked );
end;

procedure TfrmMain.Eventlog1Click(Sender: TObject);
begin
  SetEventLogVisible( not Eventlog1.Checked);
end;

procedure TfrmMain.CurrentItem1Click(Sender: TObject);
begin
  SetCurrentItemBarVisible( not CurrentItem1.Checked);
end;

procedure TfrmMain.HiddenItems1Click(Sender: TObject);
begin
  dmfGeneral.SetStatus(SF_AdminMode, not dmfGeneral.AdminMode);
end;

procedure TfrmMain.Toolbar1Click(Sender: TObject);
begin
  SetToolBarVisible(not Toolbar1.Checked);
end;

procedure TfrmMain.SetTreeViewVisible(visible: Boolean);
begin
  Tree2.Checked         := visible;
  tvTreeItem.Visible    := visible;
  spltrTreeView.Visible := visible;
  dmfGeneral.SetStatus(SF_TreeViewVisible, visible);
end;

procedure TfrmMain.SetDetailPageVisible(visible: Boolean);
begin
  DetailPages1.Checked  := visible;
  pnlExpl.Visible       := visible;
  spltrExplorer.Visible := visible;
  dmfGeneral.SetStatus(SF_DetailsVisible, visible);
end;

procedure TfrmMain.SetEventLogVisible(visible: Boolean);
begin
  Eventlog1.Checked      := visible;
  pnlEventlog.Visible    := visible;
  spltrEventlog. Visible := visible;
  dmfGeneral.SetStatus(SF_EventLogVisible, visible);
  if not visible then
    lbxEventLog.Clear;
end;

procedure TfrmMain.SetToolBarVisible(visible: Boolean);
begin
  Toolbar1.Checked      := visible;
  clbrMain.Visible      := visible;
  pnlMdiToolBar.Visible := visible;
  dmfGeneral.SetStatus(SF_ToolBarVisible, visible);
end;

procedure TfrmMain.SetCurrentItemBarVisible(visible: Boolean);
begin
  CurrentItem1.Checked  := visible;
  clbrCurrentItem.Visible := visible;
  edtCurrentItem.Visible  := visible;
  dmfGeneral.SetStatus(SF_CurrentItemBarHidden, not visible);
  if visible then
    edtCurrentItem.Text := TreeItem_GetFullName_Save(dmfGeneral.GetActiveTreeItem);
end;

procedure TfrmMain.miOptionsClick(Sender: TObject);
begin
  TfrmSettings.Display;
end;

procedure TfrmMain.miDebugReportClick(Sender: TObject);
begin
  dmfGeneral.miTestClick(sender);
end;

procedure TfrmMain.TileHorClick(Sender: TObject);
begin
  TileMode := tbHorizontal;
  Tile;
end;

procedure TfrmMain.TileVerClick(Sender: TObject);
begin
  TileMode := tbVertical;
  Tile;
end;

procedure TfrmMain.Cascade1Click(Sender: TObject);
begin
  Cascade;
end;

procedure TfrmMain.miCloseWindowClick(Sender: TObject);
begin
    MDIChildren[0].Close;
end;

procedure TfrmMain.CloseAll1Click(Sender: TObject);
var
  nIdx : integer;
begin
  for nIdx := MDIChildCount - 1 downto 0 do
    MDIChildren[nIdx].Close;
end;

procedure TfrmMain.CloseAllButThisClick(Sender: TObject);
var
  nIdx : integer;
begin
  for nIdx := MDIChildCount - 1 downto 1 do
    MDIChildren[nIdx].Close;
end;

procedure TfrmMain.miOnlineHelpClick(Sender: TObject);
begin
  StartNewBrowserWindow( dmfGeneral.GetString(SRI_HelpUrl) );
end;


procedure TfrmMain.miAboutClick(Sender: TObject);
var verNum: Float64; verNumStr: SourceString;
begin
   verNum := DMS_GetVersionNumber;
   Str(verNum:5:3, verNumStr);
   DisplayAboutBox(Application.Handle, self, DMS_GetVersion, verNumStr);
end;

procedure TfrmMain.ModifyHiddenNodesState;
begin
  // main treeview
  m_TreeNodeUpdateSet.InitRoot(TConfiguration.CurrentConfiguration.Root);
end;

procedure TfrmMain.FormDragOver(Sender, Source: TObject; X, Y: Integer;
  State: TDragState; var Accept: Boolean);
begin
  Accept := true;
end;

procedure TfrmMain.FormDragDrop(Sender, Source: TObject; X, Y: Integer);
begin
  dmfGeneral.pmTreeItemPopup(nil);
  dmfGeneral.miDefaultView.Click;
end;

procedure TfrmMain.ViewPropValue(ti: TTreeItem; sPropertyName: ItemString);
var
  sPropertyValue : SourceString;
  sCheckValue : SourceString;
  pdAny : CPropDef;
begin
  pdAny := DMS_TreeItem_FindPropDef(ti, sPropertyName);
  if not assigned(pdAny) then exit;

  sPropertyValue := PropDef_GetValueAsString_Save(pdAny, ti);

  TfrmEditProperty.Display(sPropertyName, sPropertyValue, true);
end;

procedure TfrmMain.edtCurrentItemSearch;
var txt: ItemString;
    ti, bti: TTreeItem;
    unfoundPart: IString;
    upStr: ItemString;
begin
  if not TConfiguration.CurrentIsAssigned then exit;

  txt := ItemString(edtCurrentItem.Text);

  ti := dmfGeneral.GetActiveTreeItem;
  if not Assigned(ti) then ti := TConfiguration.CurrentConfiguration.Root;
  unfoundPart := nil;
  bti := DMS_TreeItem_GetBestItemAndUnfoundPart(ti, PItemChar(txt), @unfoundPart);
  upStr := DMS_ReleasedIString2String(unfoundPart);

  dmfGeneral.SetActiveVisibleTreeItem(bti);
  ProcessUnfoundPart(upStr);
end;

procedure TfrmMain.ProcessUnfoundPart(upStr: ItemString);
var cursorPos: Cardinal;
begin
  edtCurrentItem.Text := TreeItem_GetFullName_Save(dmfGeneral.GetActiveTreeItem);
  cursorPos := length(edtCurrentItem.Text);
  edtCurrentItem.SelStart  := cursorPos;
  if upStr = '' then exit;
  edtCurrentItem.SetSelText('/'+upStr);
  edtCurrentItem.SelStart  := cursorPos;
  edtCurrentItem.SelLength  := 1+length(upStr);
end;

procedure TfrmMain.edtCurrentItemKeyUp(Sender: TObject; var Key: Word; Shift: TShiftState);
begin
  if (Key <> VK_RETURN) and (key <> VK_F5) then exit;
  Key := 0;
  edtCurrentItemSearch;
end;

function TfrmMain.LoadConfiguration(sFilename: FileString; sDesktop: ItemString): boolean;
var
  sDir : FileString;
  sConfigName : FileString;
begin
  Result := true;

  StatusBarText[sbpDesktop] := 'Loading configuration ' + sFilename;
  SetEventLogVisible(false);

  // set current directory
  sDir := ExtractFileDirEx2(sFilename);
  if(sDir <> '') and not SetCurrentDir(sDir) then
  begin
    StatusBarText[sbpDesktop] := 'Failed to SetCurrentDir to ' + sDir;
    Result := false;
  end
  else
  begin
    // initialize treeview
    sConfigName := ExtractFileNameEx2(sFilename);
    if(TConfiguration.LoadConfiguration(sConfigName) = nil)
    then begin
      StatusBarText[sbpDesktop] := 'Failed to load configuration ' + sFilename;
      Result := false;
    end;
  end;

  if not result then
  begin
    tvTreeItem.Items.Clear;
    dmfGeneral.SetActiveTreeItem(nil, tvTreeItem);
    Exit;
  end;

  TfrmOptionalInfo.Display(OI_CopyRight,  false);
  TfrmOptionalInfo.Display(OI_Disclaimer, false);

  StatusBarText[sbpDesktop] := 'Building treeview ...';

  m_TreeNodeUpdateSet.InitRoot(TConfiguration.CurrentConfiguration.Root);
  if(tvTreeItem.Items.Count > 0)
  then begin
    tvTreeItem.Items[0].Expand(false);
    tvTreeItem.Selected := tvTreeItem.Items[0];
    dmfGeneral.SetActiveTreeItem(TConfiguration.CurrentConfiguration.Root, tvTreeItem)
  end;

  StatusBarText[sbpDesktop] := 'Loading desktop ...' + sDesktop;

  TConfiguration.CurrentConfiguration.DesktopManager.LoadDesktop(sDesktop);

  StatusBarText[sbpDesktop] := 'Desktop: ' + TConfiguration.CurrentConfiguration.DesktopManager.GetActiveDesktopDescr;

  dmfGeneral.SetString(SRI_LastConfigFile, GetCurrentDir + '/' + sConfigName);

  UpdateCaption;
end;

function TfrmMain.LoadConfigurationAndProjectDll(sFilename, sDesktop: FileString; mustLoadDll, mustShowError: Boolean): Boolean;
var
  sDirName, sName, sFullname, dmsProjectDllName: FileString;
  isD5Dll: Boolean;
begin
  sDirName := ExtractFileDirEx2(sFilename);
  sName    := ExtractFilenameEx2(sFilename);
  Result   := (sDirName = '') OR SysUtils.DirectoryExists(sDirName);
  if (not Result) then
  begin
    if  mustShowError then
      DispMessage('Error finding dir for configuration '+sFilename);
    exit;
  end;

  Result := false;
  if sDirname <> '' then SetCurrentDir(sDirName);
  if sName <> '' then
  repeat
      dmfGeneral.Reopen := false;
      if (mustLoadDll) then
      begin
        dmsProjectDllName := TConfiguration.PreGetDMSProjectDllName(sName, isD5Dll);
        LoadDMSConfigDllInfo(dmsProjectDllName, isD5Dll);
        DisplayOrHideSplashWindow(Application.Handle, Application, DMS_GetVersion, tsShow);
      end;
      try
         Result := LoadConfiguration(sName, sDesktop);
      except
        on E: Exception do dmfGeneral.aeGeneralException(self, e);
      end;

      if Result then
      begin
        sFullname := GetCurrentDir + '/' + sName;
        rfMain.AddFile(sFullname);
        SHAddToRecentDocs(SHARD_PATH, PChar(AsLcWinPath(sFullname)));
      end
      else if  mustShowError then
        DispMessage('Error loading configuration '+sFilename);

      if (mustLoadDll) then
        DisplayOrHideSplashWindow(0, nil, '', tsHide);
  until Result or not dmfGeneral.Reopen;
end;

procedure TfrmMain.UpdateCaption;
var
  sTmpCaption : string;
begin
  sTmpCaption := DMS_GetVersion;

  if TConfiguration.CurrentIsAssigned
  then
    sTmpCaption :=
        TConfiguration.CurrentConfiguration.ConfigurationFile  + ' in ' + GetCurrentDir
    + iif(dmfGeneral.AdminMode,'', '[Hiding]')
    + iif(dmfGeneral.GetStatus(SF_ShowStateColors), '', '[HSC]')
    + iif(dmfGeneral.GetStatus(SF_TraceLogFile   ), '[TL]', '')
    + iif(dmfGeneral.GetStatus(SF_SuspendForGUI  ), '', '[C0]')
    + iif(dmfGeneral.GetStatus(SF_MultiThreading1), '', '[C1]')
    + iif(dmfGeneral.GetStatus(SF_MultiThreading2), '', '[C2]')
    + ' - ' + sTmpCaption;

  Caption := sTmpCaption
end;

var s_InitialApplIconHandle: HIcon;

procedure TfrmMain.LoadDMSConfigDllInfo(const sFilename : FileString; var isD5Dll: Boolean);
var
  newIconHandle: HIcon;
begin
  if s_InitialApplIconHandle = 0 then
     s_InitialApplIconHandle := Application.Icon.Handle;

  BeginTiming;
  LoadDMSProjectDll(sFilename, isD5Dll);

  newIconHandle := GetApplicationIconHandle;
  if newIconHandle = 0 then
    newIconHandle := s_InitialApplIconHandle;
  if Application.Icon.Handle <> newIconHandle then
    Application.Icon.Handle := newIconHandle;

  UpdateCaption;
end;

procedure TfrmMain.dmfGeneralAdminModeChanged(sender: TObject);
var ti: TTreeItem;
begin
  if(TConfiguration.CurrentIsAssigned)
  then begin
    ti := dmfGeneral.GetActiveTreeItem;
    ModifyHiddenNodesState;

    if(tvTreeItem.Items.Count > 0)
    then begin
      tvTreeItem.Items[0].Expand(false);
//      tvTreeItem.Selected := tvTreeItem.Items[0];
      dmfGeneral.SetActiveVisibleTreeItem(ti);

    end;
  end;
  UpdateMainMenuVisibility;
  UpdateCaption;
end;

procedure TfrmMain.dmfGeneralShowStateColorsChanged(sender: TObject);
begin
  tvTreeItem.Invalidate;
end;

procedure TfrmMain.miDefaultViewClick(Sender: TObject);
begin
   dmfGeneral.miDefaultView.Click;
end;

procedure TfrmMain.miExportAsciiGridClick(Sender: TObject);
begin
   dmfGeneral.miExportAsciiGrid.Click;
end;

procedure TfrmMain.miExportBitmapClick(Sender: TObject);
begin
   dmfGeneral.miExportBitmap.Click;
end;

procedure TfrmMain.miExportDbfShapeClick(Sender: TObject);
begin
   dmfGeneral.miExportDbfShape.Click;
end;

procedure TfrmMain.miExportCsvTableClick(Sender: TObject);
begin
   dmfGeneral.miExportCsvTable.Click;
end;

procedure TfrmMain.SetActiveTreeNode(tiActive: TTreeItem; oSender: TObject);
var
  tnActive : TTreeNode;
begin

  tvTreeItem.OnChanging := nil;
  try
      if oSender <> tvTreeItem then
      begin
        if Assigned(tiActive)
        then tnActive := TreeView_ShowAssTreeNode(tvTreeItem, tiActive)
        else tnActive := Nil;
        if Assigned(tnActive) then
        begin
          tnActive.MakeVisible;
          if not tnActive.Selected
          then tnActive.Selected := true;
        end
        else tvTreeItem.Selected := nil;
      end;

      if edtCurrentItem.Visible then
        edtCurrentItem.Text := TreeItem_GetFullName_Save(tiActive);
  finally
      tvTreeItem.OnChanging := tvTreeItemChanging;
  end;
end;

var s_MainText, s_TimeText: String;
procedure TfrmMain.SetStatusbarText(sbpIdx: TStatusbarPanel; sValue: string);
begin
  if (sbpIdx = sbpMain) then
  begin
    s_MainText := sValue;
    sValue := s_MainText + s_TimeText;
  end;

  if (sbpIdx = sbpTime) then
  begin
    s_TimeText := sValue;
    sValue := s_MainText + s_TimeText;
    sbpIdx := sbpMain;
  end;

  if Statusbar.Panels[integer(sbpIdx)].Text = sValue then
    exit;

  Statusbar.Panels[integer(sbpIdx)].Text := sValue;
  StatusBar.Update;
end;

procedure TfrmMain.lvExploreEnter(Sender: TObject);
begin
  tvTreeItem.SetFocus;
end;

procedure TfrmMain.miEditClassificationandPaletteClick(Sender: TObject);
begin
   dmfGeneral.miEditClassificationandPalette.Click;
end;

procedure TfrmMain.miUpdateTreeitemClick(Sender: TObject);
begin
   dmfGeneral.miUpdateTreeItem.Click;
end;

procedure TfrmMain.miUpdateSubTreeClick(Sender: TObject);
begin
   dmfGeneral.miUpdateSubTree.Click;
end;

procedure TfrmMain.miInvalidateTreeItemClick(Sender: TObject);
begin
   dmfGeneral.miInvalidateTreeItem.Click;
end;

procedure OnStateChanged(clientHandle: TClientHandle; item: TTreeItem; newState: TUpdateState);
begin
  try
    Assert(dmfGeneral <> nil);

{$IFDEF DEGUG}
    frmMain.LogMsg(PAnsiChar('OnStateChanged '+AsString(Cardinal(newState))));
{$ENDIF}

    case newState of
      CC_CreateMdiChild:
        dmfGeneral.CreateMdiChild(PCreateStruct(item));

      CC_Activate:
        dmfGeneral.SetActiveTreeItem(item, nil);

      CC_ShowStatistics:
      begin
        Assert(Assigned(item));
        DispMessage(DMS_NumericDataItem_GetStatistics(item, Nil));
      end;

	    NC2_MetaReady, NC2_DataReady, NC2_Validated, NC2_Committed,
      NC_Creating, NC_Deleting,
      NC2_DetermineCheckFailed, NC2_DetermineStateFailed, NC2_InterestChangeFailed,
    	NC2_MetaFailed, NC2_DataFailed, NC2_CheckFailed, NC2_CommitFailed:
      begin
        TTreeNodeUpdateSet.OnStateChange(item, newState);
        dmfGeneral.StateChanged := true;
      end
    else
        dmfGeneral.StateChanged := true;
    end;
  except
    on E: Exception do
    AppLogMessage(E.ClassName + ': '+ E.Message);
  end;
end;

procedure OnContextNotification(clientHandle: TClientHandle; description: PMsgChar); cdecl;
begin
  if (g_GetStateLock>0) then exit;

  Assert(Assigned(TfrmMain(clientHandle)));

  TfrmMain(clientHandle).BeginTiming;
  TfrmMain(clientHandle).StatusBarText[sbpMain] := description;
end;

const s_IsTiming:   Boolean = false;
var   g_OldCursor:  TCursor;
var   s_BeginTime,
      s_PassedTime:    TDateTime;
const s_MaxPassedTime: TDateTime = 0;

procedure TfrmMain.ChangeCursor(csr: TCursor);
begin
    Cursor := csr;
end;

procedure TfrmMain.BeginTiming;
begin
  if not s_IsTiming then
  begin
    s_IsTiming := true;
    s_BeginTime := Now;
    g_OldCursor := Cursor;
    ChangeCursor( crHourGlass );
    SHV_SetBusyMode(true);
  end;
end;

var sd_FrozenTime: Boolean = false; // DEBUG

procedure TfrmMain.EndTiming;
var dayString: String;
begin
  if s_IsTiming and not sd_FrozenTime then
  begin
    s_IsTiming   := false;
    s_PassedTime := Now - s_BeginTime;

//  dmfGeneral.AmIdle;

    if s_PassedTime > 5 / (24*3600) then
        MessageBeep(MB_OK); // Beep after 5 sec of continuous work

    // BEGIN DEBUG
    if s_PassedTime < 0 then // weird stuff
    begin
        MessageBeep(MB_OK);
        MessageBeep(MB_OK); // Beep again
        StatusBarText[sbpTime] := 'Weird time diff ' + FormatDateTime('c', s_MaxPassedTime) + ' with begintime=' + FormatDateTime('c', s_BeginTime);
        sd_FrozenTime := true;
    end;
    // END DEBUG

    SHV_SetBusyMode(false);
    ChangeCursor( g_OldCursor );

    if (s_PassedTime > s_MaxPassedTime) then
    begin
      s_MaxPassedTime := s_PassedTime;
      dayString := '';
      if s_MaxPassedTime >= 1.0
      then
      begin
        dayString := AsString(Trunc(s_MaxPassedTime)) + ' days and ';
        s_MaxPassedTime := s_MaxPassedTime - Trunc(s_MaxPassedTime);
      end;
      StatusBarText[sbpTime] := ' - max processing time: ' + dayString + FormatDateTime('hh:nn:ss', s_MaxPassedTime)
    end;
  end;
end;


procedure TfrmMain.SaveTreeView(tiContainer: TTreeItem);
var
   tiTemp : TTreeItem;
begin
   if not Assigned(tiContainer) then
      Exit;

   with TTreeViewSerializer.Create(tvTreeItem) do try
     tiTemp := DMS_CreateTreeItem(tiContainer, 'expanded');
     DMS_PropDef_SetValueAsCharArray(GetPropDef(pdDialogData), tiTemp,
       PSourceChar(SourceString(SaveExpandedState)));

     tiTemp := DMS_CreateTreeItem(tiContainer, 'focused');
     DMS_PropDef_SetValueAsCharArray(GetPropDef(pdDialogData), tiTemp,
       PSourceChar(SourceString(SaveFocusedNode)));
   finally
     Free;
   end;
end;

procedure TfrmMain.RestoreTreeView(tiContainer: TTreeItem);
var
   tiTemp : TTreeItem;
begin
   if not Assigned(tiContainer) then
      Exit;

   with TTreeViewSerializer.Create(tvTreeItem) do try
     tiTemp := DMS_TreeItem_GetItem(tiContainer, 'expanded', nil);
     if Assigned(tiTemp) then
        LoadExpandedState(TreeItemDialogData(tiTemp));

     tiTemp := DMS_TreeItem_GetItem(tiContainer, 'focused', nil);
     if Assigned(tiTemp) then
        LoadFocusedNode(TreeItemDialogData(tiTemp));
   finally
     Free;
   end;
end;


procedure TfrmMain.rfMainFileOpen(sFilename: String);
begin
  rfMainFileOpenImpl(sFilename, '');
end;

procedure TfrmMain.rfMainFileOpenImpl(sFilename, sDesktop: String);
begin
  // if we have one loaded, do we save it ?
  if not TryToCloseConfig then Exit;

  LoadConfigurationAndProjectDll(sFilename, sDesktop, true, true);
end;

procedure TfrmMain.rfReopen;
var sCurrentTi, sDesktop: ItemString;
   sFileName: FileString;
   currentTi: TTreeItem;
   p: Cardinal;
begin
  Assert(TConfiguration.CurrentIsAssigned);
  sCurrentTi := TreeItem_GetFullName_Save(dmfGeneral.GetActiveTreeItem);
  sDesktop   := TConfiguration.CurrentConfiguration.DesktopManager.GetActiveDesktopName;
  sFileName := TConfiguration.CurrentConfiguration.ConfigurationFile;
  rfMainFileOpenImpl(sFileName, sDesktop);

  currentTi := nil;
  try
  while true do
  begin
    currentTi := DMS_TreeItem_GetItem(TConfiguration.CurrentConfiguration.Root, PItemChar(sCurrentTi), nil);
    if Assigned(currentTi) then break;

    p :=  length(sCurrentTi);
    while (p>0) and (sCurrentTi[p] <> '/') do Dec(p);
    if p=0 then
    begin
      currentTi := TConfiguration.CurrentConfiguration.Root;
      break;
    end;

    assert(sCurrentTi[p] = '/');
    sCurrentTi := Copy(sCurrentTi, 1, p-1);
  end;
  finally
  end;
  dmfGeneral.SetActiveVisibleTreeItem(currentTi);
end;

procedure TfrmMain.ReopenConfigClick(Sender: TObject);
begin
    rfReopen;
end;

{ NYI, SEE COMMENT IN tvTreeItemCustomDrawItem
procedure TfrmMain.tvTreeItemGetCustomDrawItemColors(
  tn: TTreeNode; State: TCustomDrawState; var clrText, clrTextBk: COLORREF);
begin
  TreeNode_GetCustomDrawItemColors(tn,
      TCustomDrawState(Word(nmcd.uItemState)),
      m_TreeNodeUpdateSet,
      clrText, clrTextBk
  );
end;
//}

procedure TfrmMain.tvTreeItemCustomDrawItem(Sender: TCustomTreeView;
  Node: TTreeNode; State: TCustomDrawState; var DefaultDraw: Boolean);
begin
  // DEBUG, deze functie blijft alleen als handler aanwezig
  // om procedure TCustomTreeView.CNNotify(var Message: TWMNotify);
  // CDRF_NOTIFYITEMDRAW te laten retourneren wanneer (dwDrawStage and CDDS_ITEM) = 0
  exit;
end;

procedure TfrmMain.WMNotify(var Message: TWMNotify); //message WM_NOTIFY;
begin
  if not TreeView_OnDrawTreeNode(tvTreeItem, m_TreeNodeUpdateSet, Message) then
    inherited;
end;

function GetHTML(w: TWebBrowser): String;
  Var
    e: IHTMLElement;
  begin
    Result := '';
    if Assigned(w.Document) then
    begin
       e := (w.Document as IHTMLDocument2).body;

       while e.parentElement <> nil do
       begin
         e := e.parentElement;
       end;

       Result := e.outerHTML;
    end;
  end;

procedure TfrmMain.SaveDetailPage(s: PFileChar);
var
  htmlSource: String;
  f: TextFile;
begin
  htmlSource := GetHTML(wbMetaData);
  AssignFile(f, s);
  Rewrite(f);
  try
    Write(f, htmlSource);
  finally
    CloseFile(f);
  end;
end;

function Get4Bytes(pcds: PCopyDataStruct; i: UInt32): UInt32;
begin
   Result := 0;
   if pcds^.cbData < (i+1)*4 then exit;
   Result := PUInt32(UInt64(pcds^.lpData)+i*4)^
end;

procedure TfrmMain.WMCopyData(var Message: TWMCopyData); //message WM_COPYDATA;
var
  pcds: PCopyDataStruct;
  cds2: TCopyDataStruct;
  msg:  TMessage;
  code: UINT;
  ctrl: TfraDmsControl;
  hWindow: HWnd;

begin
   pcds := Message.CopyDataStruct;
   code := pcds^.dwData;

{$IFDEF DEBUG}
   LogMsg(PMsgChar(MsgString('WM_COPYDATA ')));
   LogMsg(PMsgChar(MsgString('CODE   ')+AsString(code)));
   LogMsg(PMsgChar(MsgString('#Bytes ')+AsString(pcds^.cbData)));
{$ENDIF}

   case code of
     0: hWindow := HWnd(nil);
     1: hWindow := WindowHandle;
     2: hWindow := GetFocus;
     5: begin dmfGeneral.miDefaultView.Click; exit; end;
     6: begin edtCurrentItem.Text := PFileChar(pcds.lpData); edtCurrentItemSearch; exit; end; // GOTO
     7: begin miExportViewPorts.Click; exit end;
     8: begin  // EXPAND
            if assigned(frmMain.tvTreeItem.Selected) then
                frmMain.tvTreeItem.Selected.Expand(Get4Bytes(pcds,0)<>0); 
            exit 
        end; 
     9: begin pgcntrlData.ActivePageIndex := Get4Bytes(pcds,0); exit end;
    10: begin SaveDetailPage(PFileChar(pcds.lpData)); exit end; // SAVE_DP
    11: begin dmfGeneral.miDatagridView.Click; exit; end;
    12: begin dmfGeneral.miHistogramView.Click; exit; end;
   end;
   if code < 4  then
   begin
      msg.Msg    := UINT(Get4Bytes(pcds, 0));
      msg.WParam := WPARAM(Get4Bytes(pcds, 1));
      msg.LParam := LPARAM(Get4Bytes(pcds, 2));
   end
   else begin  // code >= 4
      cds2.dwData := UINT(Get4Bytes(pcds, 0));
      cds2.cbData := iif(pcds.cbData >= 4, pcds.cbData - 4, 0);
      cds2.lpData := iif(pcds.cbData >= 4, Pointer(UInt32(pcds.lpData) + 4), nil);
      msg.Msg    := WM_COPYDATA;
      msg.WParam := WPARAM(WindowHandle);
      msg.LParam := LPARAM(@cds2);
   end;
{$IFDEF DEBUG}
   LogMsg(PMsgChar(MsgString('MSG ')+AsString(msg.Msg)));
   LogMsg(PMsgChar(MsgString('WPARAM ')+AsString(msg.WParam)));
   LogMsg(PMsgChar(MsgString('LPARAM ')+AsString(msg.LParam)));
{$ENDIF}
   if code < 3 then
      SendMessage(hWindow, msg.Msg,  msg.WParam, msg.LParam)
   else
   begin
     if (ActiveMDIChild is TfrmPrimaryDataViewer)
     and  (TfrmPrimaryDataViewer(ActiveMDIChild).DataControl is TfraDmsControl) then
     begin
       ctrl := TfrmPrimaryDataViewer(ActiveMDIChild).DataControl as TfraDmsControl;
       if assigned(ctrl) then
         ctrl.WndProc(msg);
     end
   end;
   Message.Result := HRESULT(TRUE);
end;

procedure TfrmMain.miCopyrightNoticeClick(Sender: TObject);
begin
   TfrmOptionalInfo.Display(OI_Copyright, true);
end;

procedure TfrmMain.miDisclaimerClick(Sender: TObject);
begin
   TfrmOptionalInfo.Display(OI_Disclaimer, true);
end;

procedure TfrmMain.miSourceDescrClick(Sender: TObject);
begin
   TfrmOptionalInfo.Display(OI_SourceDescr, true);
end;

procedure ProcessPostData(ti: TTreeItem; const PostDataRef: OleVariant);
var
  PostDataPtr: PVariant;
  PostDataArray: PVarArray;

begin
  if VarType(PostDataRef) <> $400C then exit;
  PostDataPtr := TVarData(PostDataRef).VPointer;
  if not VarIsArray(PostDataPtr^) then exit;

  PostDataArray := TVarData(PostDataPtr^).VArray;
  assert(PostDataArray^.DimCount = 1);
  assert(PostDataArray^.ElementSize = 1);

  DMS_ProcessPostData(ti, PostDataArray^.Data, PostDataArray^.Bounds[0].ElementCount);
end;

function ProcessADMS(ti: TTreeItem; const URL: FileString): SourceString;
var istr: IString;
begin
  istr := DMS_ProcessADMS(ti, PFileChar(URL) );
  try
    Result := DMS_IString_AsCharPtr(istr);
  finally
    DMS_IString_Release(istr);
  end
end;

procedure TfrmMain.wbMetaDataBeforeNavigate2(ASender: TObject;
  const pDisp: IDispatch; const URL, Flags, TargetFrameName, PostData,
  Headers: OleVariant; var Cancel: WordBool);
var
  currentBrowser : IWebBrowser2;
begin
  currentBrowser := pDisp as IWebBrowser2;

  ProcessPostData(dmfGeneral.GetActiveTreeItem, PostData);

  if Pos('.adms', LowerCase(URL)) >0 then
  begin
    SetDocData(currentBrowser, ProcessADMS(dmfGeneral.GetActiveTreeItem, URL) );
    Cancel := true;
    exit;
  end;

  Cancel := TViewAction.OnBeforeNavigate(URL, (integer(FLAGS) and navNoHistory) = 0);
//  Flags := navNoHistory;
//  TargetFrameName := Null;
end;

type TExportInfoColumn = class (TAbstrItemColumnProvider)
  constructor Create;
  function GetValue(ti: TTreeItem): String; override;
end;

constructor TExportInfoColumn.Create;
begin
  inherited Create('ExportInfo', 900);
end;

function TExportInfoColumn.GetValue(ti: TTreeItem): String;
begin
  Result := TfraDmsControl(ti).GetExportInfo;
end;

procedure TfrmMain.miExportViewPortsClick(Sender: TObject);
  var pl: TfrmTreeItemPickList;
  var i, n: UInt32;
    child: TForm;
    ctrl: TfraBasePrimaryDataControl;

    // recursive search
begin
  pl := TfrmTreeItemPickList.Create(Self);
  try
    pl.MultiSelect := true;
    pl.AddColumn(TExportInfoColumn.Create );
//    pl.AddColumn(TAscFileNameColumn.Create(self));
    pl.Width := 900;
    i := 0;
    n := MdiChildCount;
    while (i<>n) do
    begin
       child := MdiChildren[i];
       if (child is TfrmPrimaryDataViewer) then
       begin
           ctrl:= (child as TfrmPrimaryDataViewer).DataControl;
           if (ctrl is TfraDmsControl) and ((ctrl as TfraDmsControl).GetExportInfo <> '')
           then pl.pickList.AddRow(ctrl, true);
       end;
       INC(i);
    end;
//    pl.Caption := 'Export ViewPorts';
//    pl.pickList.lvTreeItems.SetFocus;
    if pl.Execute then
    begin
       pl.Caption := 'Exporting ...';
       pl.btnOK.Enabled := false;
//       pl.btnCancel.Enabled := false;
       pl.show;
       pl.Update;

       i := 0;
       n := pl.pickList.lvTreeItems.Items.Count;
       while (i<>n) do
       begin
         if (pl.pickList.lvTreeItems.Items[i].Selected) then
         begin
             pl.pickList.lvTreeItems.Items[i].Selected := false;
             pl.Update;
             TfraDmsControl(pl.pickList.lvTreeItems.Items[i].Data).SendToolId(TB_Export);
             if pl.ModalResult = mrCancel then exit;
             if dmfGeneral.Terminating then exit;
         end;
         INC(i);
       end;
    end
  finally
    pl.Free; // clean mess in case of exception in ScanItems
  end;
end;

procedure TfrmMain.SetSourceDescr(sdm: TSourceDescrMode);
begin
  m_CurrSDM := sdm;
  DoOrCreateViewActionFromDll(dmfGeneral.GetActiveTreeItem, 'dp.SourceDescr', Integer(sdm), 0, 0, true, false, true);
  miConfiguredSourceDescr.Checked := (sdm = SDM_Configured);
  miReadonlyStorageManagers.Checked := (sdm = SDM_ReadOnly);
  miNonReadonlyStorageManagers.Checked := (sdm = SDM_WriteOnly);
  miAllStorageManagers.Checked := (sdm = SDM_All);
end;

procedure TfrmMain.miConfiguredSourceDescrClick(Sender: TObject);
begin
  SetSourceDescr(SDM_Configured);
end;

procedure TfrmMain.miReadonlyStorageManagersClick(Sender: TObject);
begin
  SetSourceDescr(SDM_ReadOnly);
end;

procedure TfrmMain.miNonReadonlyStoragemanagersClick(Sender: TObject);
begin
  SetSourceDescr(SDM_WriteOnly);
end;

procedure TfrmMain.miAllStorageManagersClick(Sender: TObject);
begin
  SetSourceDescr(SDM_All);
end;

Initialization
begin
  OleInitialize(nil)
end;

Finalization
begin
  OleUninitialize
end;

end.

