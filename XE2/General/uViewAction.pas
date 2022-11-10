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

unit uViewAction;

interface

uses Windows, Classes, Controls, Menus, comctrls,
 uDmsInterfaceTypes, uDmsTreeviewFunctions;

function tbsht_FromDP(dp: TDetailPage): TTabSheet;
function tbsht_ToDP  (tbsht: TTabSheet): TDetailPage;

type TViewActionType =
(
  VAT_Execute,
  VAT_PopupTable,
  VAT_EditProp,
  VAT_MenuItem,
  VAT_DetailPage,
  VAT_Url
);

type TViewAction = class
private
  m_VAT: TViewActionType;
  m_isReady,
  m_AddHistory,
  m_InHistory: Boolean;
  m_tiFocus:  TTreeItem; // object invariant: OnTreeItemChanged is registered for m_tiFocus as long as self exists and m_tiFocus points to anything.
  m_Url, m_sExtraInfo: SourceString;
  m_RecNo:    SizeT;
  m_X, m_Y:   Integer;
  m_oSender:  TObject;
  m_MenuItem: TMenuItem;

  constructor Create(tiContext: TTreeItem; sAction: MsgString; nCode: Integer; oSender: TControl; x,y: Integer; doAddHistory, isUrl: Boolean);

  procedure ExecuteAction;
  procedure PopupTable;
  procedure PopupAction(Sender: TObject);
  function  ApplyDirect: Boolean;

  function Apply: Boolean;
  function IdleAction: Boolean;
  function GetExtraInfo: PSourceChar;

public
  destructor  Destroy; override;
  class function IdleProcess: Boolean;

  class procedure DoOrCreate(tiContext: TTreeItem; sAction: MsgString; nCode: Integer; oSender: TControl; x,y: Integer; doAddHistory, isUrl: Boolean);
  class procedure SetActive(newActive: TViewAction);
  class function GetActive: TViewAction;
  class function OnBeforeNavigate(sUrl: string; doAddHistory: boolean): Boolean; // return true if navigation must be cancelled

  procedure LooseFocus;

  property RecNo: SizeT   Read m_RecNo;
  property Row:   Integer Read m_Y;
  property Col:   Integer Read m_X;
  property ExtraInfo: PSourceChar read GetExtraInfo;
  property IsReady: Boolean Read m_isReady Write m_isReady;
  property VAT: TViewActionType read m_VAT;
  property Url: SourceString    read m_Url;
  property TiFocus: TTreeItem   read m_tiFocus;
end;

procedure DoOrCreateViewActionFromDll(tiContext: TTreeItem; sAction: PMsgChar; nCode: Integer; x,y: Integer; doAddHistory, isUrl, mustOpenDetailsPage: Boolean); cdecl;

type THistory = class(TList)
private
  m_Active: Integer;

  constructor Create;
  procedure RemoveForwards;
public
  destructor Destroy; override;
  procedure ClearAll;
  procedure InsertAction(action: TViewAction);
  procedure GoBack;
  procedure GoForward;
  procedure Refresh;
  function AtBegin: boolean;
  function AtEnd: boolean;
  function ActiveAction: TViewAction;

end;

var
 g_History:    THistory;

implementation

uses SysUtils, Forms,
  dmGeneral, uDmsUtils,
  uDmsInterface, uAppGeneral,
  fMain, Configuration, uGenLib;

function FindMenuItem(menuObj: TMenuItem; sMenu: String): TMenuItem;
var sMain, sSub, sCaption: String; i: Integer;
begin
   Split(sMenu, '.', sMain, sSub);
   sMain := UpperCase(sMain);
   for i:=0 to menuObj.Count-1 do
   begin
     sCaption := menuObj.Items[i].Caption;
     if UpperCase(StringReplace(sCaption, '&', '', [rfReplaceAll])) = sMain then
     begin
        if (sSub<>'') then
          Result := FindMenuItem(menuObj.Items[i], sSub)
        else
          Result := menuObj.Items[i];
        exit;
     end;
   end;
   result := Nil;
end;

function tbsht_ToDP  (tbsht: TTabSheet): TDetailPage;
begin
  Result := DP_Unknown;
  with frmMain do
  begin
    if tbsht = tbshtStatistics   then Result := DP_Statistics  else
    if tbsht = tbshtValueInfo    then Result := DP_ValueInfo   else
    if tbsht = tbshtConfig       then Result := DP_Config      else
    if tbsht = tbshtGeneral      then Result := DP_General     else
    if tbsht = tbshtPropAdvanced then Result := DP_AllProps    else
    if tbsht = tbshtSourceDescr  then Result := DP_SourceDescr else
    if tbsht = tbshtExplore      then Result := DP_Explore     else
    if tbsht = tbshtMetaData     then Result := DP_MetaData;
  end;
  Assert(Result <> DP_Unknown);
end;

function tbsht_FromDP(dp: TDetailPage): TTabSheet;
begin
  Result := Nil;
  case dp of
    DP_General:     Result := frmMain.tbshtGeneral;
    DP_Explore:     Result := frmMain.tbshtExplore;
    DP_MetaData:    Result := frmMain.tbshtMetaData;
    DP_ValueInfo:   Result := frmMain.tbshtValueInfo;
    DP_Statistics:  Result := frmMain.tbshtStatistics;
    DP_Config:      Result := frmMain.tbshtConfig;
    DP_SourceDescr: Result := frmMain.tbshtSourceDescr;
    DP_AllProps:    Result := frmMain.tbshtPropAdvanced;
  end;
  Assert(Result <> Nil);
end;

procedure OnTreeItemChanged(clientHandle: TClientHandle; item: TTreeItem; newState: TUpdateState);  cdecl;
begin
  frmMain.LogMsg(PAnsiChar('OnTreeItemChanged '+AsString(Cardinal(newState))));

  with TViewAction(clientHandle) do
  begin
    IsReady := false;
    if newState = NC_Deleting then
      LooseFocus;
  end;
end;

// ============================= TViewAction implementation

const g_ActiveAction: TViewAction = nil;

class procedure TViewAction.DoOrCreate(tiContext: TTreeItem; sAction: MsgString; nCode: Integer; oSender: TControl; x,y: Integer; doAddHistory, isUrl: Boolean);
var action: TViewAction;
begin
  action := TViewAction.Create(tiContext, sAction, nCode, oSender, x,y, doAddHistory, isUrl);
  with action do
  begin
     if ApplyDirect then
     try
       m_AddHistory := false;
       Apply;
     finally
       Free
     end
     else
     begin
        SetActive( action );
        m_IsReady := false;
     end
  end;
end;

procedure DoOrCreateViewActionFromDll(tiContext: TTreeItem; sAction: PMsgChar; nCode: Integer; x,y: Integer; doAddHistory, isUrl, mustOpenDetailsPage: Boolean); cdecl;
begin
  try
    if mustOpenDetailsPage then
      frmMain.SetDetailPageVisible(true);

    TViewAction.DoOrCreate(tiContext, sAction, nCode, nil, x, y, doAddHistory, isUrl);
  except
    on E: Exception do
    AppLogMessage(E.ClassName + ': '+ E.Message);
  end;
end;

constructor TViewAction.Create(tiContext: TTreeItem; sAction: MsgString; nCode: Integer; oSender: TControl; x,y: Integer; doAddHistory, isUrl: Boolean);
var sMenu, sPath: SourceString;
    sRecNr: SourceString;
begin
   // quick init of data members
//   m_isActive   := true;
   m_AddHistory := doAddHistory;
   m_InHistory  := false;
   m_tiFocus    := Nil;
   m_oSender    := Nil;
   m_RecNo      := 0;
   m_MenuItem   := Nil;
   m_X          := x;
   m_Y          := y;
   m_MenuItem   := nil;
   m_IsReady    := true;
   m_sExtraInfo := '';

   if isUrl then
   begin
      m_VAT := VAT_Url;
      m_Url := sAction;
      exit;
   end;

   Split(sAction, ':', sMenu, sPath);
   if (UpperCase(sMenu) = 'EXECUTE') then
   begin
     m_VAT := VAT_Execute;
     m_Url := sPath;
     exit; // when called from DoOrCreate, self will be Applied Directly and Destroyed.
   end;

   m_tiFocus := tiContext;
   if not Assigned(m_tiFocus) then m_tiFocus := dmfGeneral.GetActiveTreeItem;
   if Assigned(m_tiFocus) then
   begin
     if (sPath <> '') then
     begin
       m_tiFocus := DMS_TreeItem_GetItem(m_tiFocus, PItemChar(sPath), Nil);
       if not assigned(m_tiFocus) then
       begin
         DMS_ReportError(PMsgChar(
            'ViewAction "'  + sAction
          + '": TreeItem "' + sPath
          + '" not found from context ' + TreeItem_GetFullName_Save(tiContext) ));
         exit;
       end;
     end;

     DMS_TreeItem_RegisterStateChangeNotification(OnTreeItemChanged, m_tiFocus, TClientHandle(self)); // Resource aquisition which must be matched by a call to LooseFocus

     if UpperCase(sMenu) = 'POPUPTABLE' then
     begin
       m_VAT := VAT_PopupTable;
       m_RecNo := nCode;
       m_oSender := oSender;
       exit; // when called from DoOrCreate, self will be Applied Directly and Destroyed.
     end
   end;
   sMenu := SubstituteChar(sMenu, '#', '!');
   Split(sMenu, '!', sMenu, sRecNr);
   if (UpperCase(sMenu) = 'EDIT') then
   begin
     m_VAT := VAT_EditProp;
     m_Url := sRecNr;
     exit; // when called from DoOrCreate, self will be Applied Directly and Destroyed.
   end;

   Split(sRecNr, '!', sRecNr, m_sExtraInfo);
   if (sRecNr <> '')
   then m_RecNo := StrToInt64(sRecNr)
   else m_RecNo := DMS_SIZET_UNDEFINED;

   if (UpperCase(Copy(sMenu, 1, 3)) = 'DP.') then
   begin
      m_VAT := VAT_DetailPage;
      Assert(assigned(m_tiFocus));
      Assert(m_AddHistory);
      m_Url := Copy(sMenu, 4, length(sMenu)-3);
      if nCode <> -1 then
        m_RecNo := nCode;
      exit;
   end;

   m_VAT := VAT_MenuItem;
   m_MenuItem := FindMenuItem(dmfGeneral.pmTreeItem.Items, sMenu);
   if not Assigned(m_MenuItem) then
   begin
     DMS_ReportError(PMsgChar('ViewAction "' + sAction + '": MenuItem not found'));
     exit;
   end;
end;

destructor TViewAction.Destroy;
begin
  if Assigned(m_tiFocus) then
    LooseFocus;
end;

function TViewAction.GetExtraInfo: PSourceChar;
begin
  Result := PSourceChar(m_sExtraInfo);
end;

procedure TViewAction.LooseFocus;
begin
  Assert(Assigned(m_tiFocus));
  DMS_TreeItem_ReleaseStateChangeNotification(OnTreeItemChanged, m_tiFocus, TClientHandle(self));
  m_tiFocus := nil;
end;

function urlDecode(url: string): string;
begin
  Result := StringReplace(url, '%20', ' ', [rfReplaceAll]);
end;

function SystemCall(sUrl: string): Boolean;
var ext: FileString;
    anchorLoc: Integer;
begin
   AppLogMessage('BeforeNavigate url: '+sUrl); // DEBUG
   ext := sUrl;
   anchorLoc := pos('#', ext);
   if anchorLoc > 0
   then ext := copy(ext, 1, anchorLoc-1);
   ext := LowerCase(ExtractFileExtEx2(ext));
   Result := //(ext = '.html') or (ext = '.htm') or
     (ext = '.pdf')
     or (ext = '.doc');

   if not Result then exit;
   StartNewBrowserWindow(sUrl);
end;

class function TViewAction.OnBeforeNavigate(sUrl: string; doAddHistory: boolean): Boolean; // return true if navigation must be cancelled
var sPrefix, sRest: string;
begin
   Split(sUrl, ':', sPrefix, sRest);
   Result := UpperCase(sPrefix) = 'DMS'; // cancel
   if Result then
   begin
     DoOrCreate(nil, urlDecode(sRest), -1, Nil, -1,-1, doAddHistory, false);
     exit;
   end;

   if (sPrefix = 'about') or (sPrefix = 'res') then exit;

   Result := SystemCall(sUrl);
   if Result then exit;

   if doAddHistory then
     DoOrCreate(nil, SourceString( sUrl ), -1, Nil, -1,-1, true, true);
end;

function TViewAction.ApplyDirect: Boolean;
begin
  Result := (m_VAT in [VAT_Execute, VAT_PopupTable, VAT_MenuItem, VAT_EditProp]);
end;

class function TViewAction.GetActive: TViewAction;
begin
  Result := g_ActiveAction;
end;

class procedure TViewAction.SetActive(newActive: TViewAction);
begin
  if (g_ActiveAction = newActive) then exit;

  if Assigned(g_ActiveAction) and not g_ActiveAction.m_InHistory then
     g_ActiveAction.Free;
  g_ActiveAction := newActive;
end;

const g_LastActivePage: TTabSheet = nil;
const g_LastActiveTreeItem: TTreeItem = nil;

function TViewAction.Apply: Boolean;
// var oldActiveAction: TViewAction;
begin
  Result := true;
//  oldActiveAction := g_ActiveAction;
//  g_ActiveAction := self;
//  try
    case m_VAT of
      VAT_Execute:    ExecuteAction;
      VAT_EditProp:
        begin
          if not Assigned(m_tiFocus) then exit;
          dmfGeneral.SetActiveTreeItem(m_tiFocus, m_oSender);
          frmMain.ViewPropValue(m_tiFocus, m_Url);
        end;
      VAT_PopupTable: PopupTable;
      VAT_MenuItem:
         Begin
            if (not assigned(m_MenuItem)) then exit;
            if Assigned(m_tiFocus) then dmfGeneral.SetActiveTreeItem(m_tiFocus, m_oSender);
            dmfGeneral.pmTreeItemPopup(m_oSender);
            m_MenuItem.Click;
         end;

      VAT_URL,
      VAT_DetailPage:
         Begin
            if Assigned(m_tiFocus) then
            begin
              dmfGeneral.SetActiveTreeItem(m_tiFocus, m_oSender);
              g_LastActiveTreeItem := m_tiFocus;
            end;
            frmMain.UpdateDataInfo(self);
            g_LastActivePage := frmMain.pgcntrlData.ActivePage;
            Result := m_isReady;
         end;
    end;
//  finally
//    g_ActiveAction := oldActiveAction;
//  end;
end;

procedure TViewAction.PopupAction(Sender: TObject);
var diAction: TAbstrDataItem;
   mi: TMenuItem;
   i: Integer;
   buffer: SourceString;
begin
  mi := TMenuItem(Sender);
  i := mi.tag;
  diAction := TAbstrDataItem(mi.Parent.tag);
  if not Assigned(diAction) then exit;
  buffer := AnyDataItem_GetValueAsString(diAction, i);
  DoOrCreate(diAction, buffer, -1, TControl(mi.Owner.Owner), 0, 0, true, false);
end;

procedure TViewAction.PopupTable;
var diCode, diLabel, diAction: TAbstrDataItem;
    rlCode, rlLabel, rlAction: TDataReadLock;
   pm: TPopupMenu;
   mi: TMenuItem;
   i, n: SizeT;
begin
  Assert( m_oSender is TFrame);
  diCode  := DMS_TreeItem_GetItem(m_tiFocus, 'code'  , DMS_AbstrDataItem_GetStaticClass);
  diLabel := DMS_TreeItem_GetItem(m_tiFocus, 'label' , DMS_AbstrDataItem_GetStaticClass);
  diAction:= DMS_TreeItem_GetItem(m_tiFocus, 'action', DMS_AbstrDataItem_GetStaticClass);

  rlCode  := DMS_DataReadLock_Create(diCode,   true); try
  rlLabel := DMS_DataReadLock_Create(diLabel,  true); try
  rlAction:= DMS_DataReadLock_Create(diAction, true); try
    if (not Assigned(TFrame(m_oSender).PopupMenu)) then
          TFrame(m_oSender).PopupMenu := TPopupMenu.Create(TFrame(m_oSender));
    pm := TFrame(m_oSender).PopupMenu;
    pm.Items.Clear;
    pm.Items.Tag := Integer(diAction);
    n := DMS_Unit_GetCount(DMS_DataItem_GetDomainUnit(diCode));
    i := 0; while i <> n do
    begin
      if DMS_NumericAttr_GetValueAsInt32(diCode, i) = m_RecNo then
      begin
        mi := TMenuItem.Create(pm);
        mi.Caption := AnyDataItem_GetValueAsString(diLabel, i);
        mi.OnClick := PopupAction;
        mi.Tag     := i;
        pm.Items.Add(mi);
      end;
      INC(i);
    end;
    pm.Popup(m_X,m_Y);
  finally DMS_DataReadLock_Release(rlAction); end
  finally DMS_DataReadLock_Release(rlLabel ); end
  finally DMS_DataReadLock_Release(rlCode  ); end
end;

procedure TViewAction.ExecuteAction;
var ti: TTreeItem;
begin
  ti := DMS_CreateTreeItem(TConfiguration.CurrentConfiguration.DesktopManager.GetActiveTemp, 'ExecuteContext');
  Assert(Assigned(ti));
  DMS_PropDef_SetValueAsCharArray(GetPropDef(pdViewAction), ti, PSourceChar(m_Url));
  DMS_TreeItem_DoViewAction(ti);
end;

// Syntax ViewAction triggering urls:
//    dms://<viewAction>
// Syntax ViewActions:
//    Action
//    Action:path
//    Execute:TreeItemViewAction
// Syntax Actions:
//    PopupTable // requires nCode, oSender, x,y and path points to table(code, label, action)
//    Command
//    Command#recno
//    Command#x,y
// Syntax Commands:
//    MenuItem
//    MenuItem.SubItem
//    MenuItem.SubItem.SubItem
//    DP.pageName

function TViewAction.IdleAction: Boolean;
begin
  Result := true;
  if dmfGeneral.StateChanged then m_IsReady := false;
  if m_IsReady then exit;

  if m_AddHistory then
  begin
    g_History.InsertAction(self);
    m_AddHistory := false;
  end;

  Result := Apply;
end;

class function TViewAction.IdleProcess: Boolean;
var dp: TDetailPage;
begin
  Result := true;
  if not frmMain.pnlExpl.Visible then exit;

  if (dmfGeneral.GetActiveTreeItem <> g_LastActiveTreeItem)
  or (frmMain.pgcntrlData.ActivePage <> g_LastActivePage) then
  begin
    g_LastActiveTreeItem := dmfGeneral.GetActiveTreeItem;
    g_LastActivePage     := frmMain.pgcntrlData.ActivePage;
    if assigned(g_LastActiveTreeItem) and assigned(g_LastActivePage) //and not(Assigned(g_ActiveAction))
    then
    begin
      dp := tbsht_ToDP(g_LastActivePage);
      DoOrCreate(g_LastActiveTreeItem, 'dp.'+dp_ToName(dp),
        iif(dp=DP_SourceDescr, Integer(frmMain.m_CurrSDM), -1), Nil, -1, -1, true, false);
    end;
    Result := false;
    exit;
  end;

  Result := true;
  if not(assigned(g_ActiveAction)) then exit;

  try
    Result := g_ActiveAction.IdleAction;

  finally
    if result then
       TViewAction.SetActive(nil)
  end;
end;

// ============================= THistory implementation

constructor THistory.Create;
begin
  inherited;
  m_Active := 0;
end;

destructor THistory.Destroy;
begin
  ClearAll;
  inherited;
end;

procedure THistory.ClearAll;
begin
  m_Active := 0;
  RemoveForwards;
  TViewAction.SetActive(Nil); // Free active action even if not in history
end;

procedure THistory.RemoveForwards;
var action: TViewAction;
begin;
  // remove all strings after active TList
  while not AtEnd do
  begin
    action := TViewAction(Items[Count-1]);
    if (action = g_ActiveAction) then g_ActiveAction := nil;
    action.Free;
    Delete(Count-1);
  end;
end;

procedure THistory.InsertAction(action: TViewAction);
begin
  assert(Assigned(action));
  assert(not action.m_InHistory);
{$IFDEF DEBUG}
  AppLogMessage('THistory.InsertAction: '+action.Url);
{$ENDIF}

  RemoveForwards;
  Add(action);
  INC(m_Active);
  action.m_InHistory := true;

  frmMain.tlbtnBack   .Enabled := not AtBegin;
  frmMain.tlbtnForward.Enabled := false;

  // postcondition
  assert(ActiveAction = action);
end;

function THistory.ActiveAction: TViewAction;
begin
  Assert(m_Active > 0);
  Result := TViewAction(Items[m_Active-1]);
end;

procedure THistory.GoBack;
begin
  Assert(not AtBegin);
  DEC(m_Active);
  Refresh;
  if AtBegin then frmMain.tlbtnBack.Enabled := false;
  frmMain.tlbtnForward.Enabled := true;
end;

procedure THistory.GoForward;
begin
  Assert(not AtEnd);
  INC(m_Active);
  Refresh;
  frmMain.tlbtnBack.Enabled := true;
  if AtEnd then frmMain.tlbtnForward.Enabled := false;
end;

procedure THistory.Refresh;
begin
  Assert(m_Active > 0);
  Assert(m_Active <= Count);
  TViewAction.SetActive( ActiveAction );
  ActiveAction.IsReady := false;
end;

function THistory.AtBegin: boolean;
begin
  Result := m_Active <= 1;
end;

function THistory.AtEnd: boolean;
begin
  Result := m_Active >= Count;
end;

initialization

g_History    := THistory.Create;

finalization

g_History.Free;

end.
