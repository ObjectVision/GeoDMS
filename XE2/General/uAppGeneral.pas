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

unit uAppGeneral;

interface

uses Windows, Messages, ComCtrls, StdCtrls, uDMSInterfaceTypes,
     Forms, fMain, Dialogs,
     TreeNodeUpdateSet,
     uDMSUtils, classes, fmBasePrimaryDataControl;

   procedure   AppLogMessage(sMessage : MsgString);
   procedure   LogFileOpen(filename: FileString);
   procedure   LogFileClose;
   procedure   StartNewBrowserWindow(url : SourceString);
   function    IsParameterOrAddrAttr(ti: TTreeItem; var recno: SizeT): Boolean;

   function    FormatTreeNodeText(ti: TTreeItem; recno, denom: SizeT;
                          showText: Boolean; var done: Boolean): ItemString;

   function    TreeView_FillPartial(tv: TTreeView; tnparent: TTreeNode; ti: TTreeItem): TTreeNode;
   procedure   TreeNode_AddDummy(tn: TTreeNode);
   function    TreeNode_SetImage(var tn: TTreeNode; ti: TTreeItem): Boolean;

   function TreeNode_FindChild(const tnParent: TTreeNode; const tiChild: TTreeItem): TTreeNode;
   function TreeView_FindAssTreeNode(const tv: TTreeView; const tiThis: TTreeItem): TTreeNode;
   function TreeView_ShowAssTreeNode(tv: TTreeView; const tiThis: TTreeItem): TTreeNode;

   function    TreeNode_GetTreeItem(const tn: TTreeNode): TTreeItem;
   function    TreeView_OnDrawTreeNode(tv: TTreeView; treeNodeUpdateSet: TTreeNodeUpdateSet; var Message: TWMNotify): Boolean;

   procedure   LogMsgCallback(clienthandle: TClientHandle; s: TSeverityType; msgCat: TMsgCategory; longMsg: PMsgChar);  cdecl;
   function    CoalesceHeap(size: SizeT; longMsg: PMsgChar): Boolean;  cdecl;

var g_HeapProblemCount: Cardinal = 0;

function TheColorDialog: TColorDialog;

implementation

uses
   Math, uGenLib, fErrorMsg,
   SysUtils, Graphics,
   Controls, CommCtrl, Configuration, ShellAPI,
   fmDmsControl,  uDmsInterface,
   dmGeneral;

const
   INIFILENAME =  'dms.ini';

var
   s_LogFile    : CDebugLogHandle;

procedure AppLogMessage(sMessage : MsgString);
begin
  if(g_frmMain <> nil)
  then g_frmMain.LogMsg(PMsgChar(sMessage));
end;

procedure LogFileOpen(filename: FileString);
begin
   LogFileClose;
   s_LogFile   := DBG_DebugLog_Open(PFileChar(filename));
end;

procedure LogFileClose;
begin
   if Assigned(s_LogFile) then
      DBG_DebugLog_Close(s_LogFile);
   s_LogFile := nil;
end;

function ApplicationIniFileName: string;
begin
   Result := ExtractFilePathEx2(Application.ExeName) + INIFILENAME;
end;

procedure StartNewBrowserWindow(url : SourceString);
begin
  if length(url) = 0 then exit;

   AppLogMessage('CMD: '+url);
   ShellExecute(Application.Handle, nil, PChar(AsWinPath(url)), '', '', SW_SHOWNORMAL); //SW_SHOWNOACTIVATE);
end;

function IsParameterOrAddrAttr(ti: TTreeItem; var recno: SizeT): Boolean;
var du: TAbstrUnit; c: SizeT;
begin
  Result := Assigned(ti) AND Assigned(DMS_TreeItem_QueryInterface(ti, DMS_AbstrDataItem_GetStaticClass));
  if (Not Result) then Exit;

  c := 0;
  if DMS_AbstrDataItem_HasVoidDomainGuarantee(ti) then c := 1
  else
  begin
    du := DMS_DataItem_GetDomainUnit(ti);
    if not Assigned(du) then
    begin
      Result := false;
      exit;
    end;

    if Assigned(DMS_TreeItem_QueryInterface(du, DMS_VoidUnit_GetStaticClass))
    then c := 1;
    if (c = 0)
      and AbstrUnit_IsInteger(du)
      and (DMS_TreeItem_GetProgressState(du) >= NC2_DataReady) then
          c := DMS_Unit_GetCount(du);
  end;
  if c=1 then recno := 0;
  Result :=  (recno < c) AND(c <> DMS_SIZET_UNDEFINED);
end;

function FormatTreeNodeText(ti: TTreeItem; recno, denom: SizeT;
        showText: Boolean; var done: Boolean): ItemString;

  var us : TUpdateState;
      oldSuspend: Boolean;
      lck:        TDataReadLock;

begin
   Result := ItemString( DMS_TreeItem_GetName(ti) );
   if not showText then
     exit;

   if not IsParameterOrAddrAttr(ti, recno) then exit;

   Assert(recno >= 0);

   us := DMS_TreeItem_GetProgressState(ti);

   if us < NC2_DataReady then
   begin
      Result := Result + ': Invalidated';
      Exit;
   end;
   oldSuspend := SuspendErrorFeedback;
   try
     SuspendErrorFeedback := True;
     if done then
       lck := DMS_DataReadLock_Create(ti, false)
     else
       lck := Nil; // display was suspended, we're in quick&dirty mode now
     if not Assigned(lck) then
     begin
        done := false;
        Result := Result + ': ...calculating';
     end
     else try

       Result := Result + '='+ DMS_AbstrDataItem_DisplayValue(ti, recno, true);

       if (denom<>DMS_SIZET_UNDEFINED) and IsParameterOrAddrAttr(ti, denom) then
         if AbstrUnit_IsNumeric(DMS_DataItem_GetValuesUnit(ti)) then
           Result := Result
              + '( '
              + Format('%.1f',
                  [100.0 * DMS_NumericAttr_GetValueAsFloat64(ti, recno)
                        /  DMS_NumericAttr_GetValueAsFloat64(ti, denom)
                  ]
                )
              + '% )'
         else
           Result := Result + ' / ' + DMS_AbstrDataItem_DisplayValue(ti, denom, true);
     finally
       DMS_DataReadLock_Release(lck);
     end;
   finally
      SuspendErrorFeedback := oldSuspend;
   end;
end;

function TreeNode_SetImage(var tn: TTreeNode; ti: TTreeItem): Boolean;
var i: Integer;
begin
   Result := false;
   try
     i := integer(SHV_GetDefaultViewStyle(ti) );
   except
     i := integer(tvsDefault);
   end;
   with tn do
   begin
     if i <> ImageIndex    then begin ImageIndex    := i; Result := true; end;
     if i <> SelectedIndex then begin SelectedIndex := i; Result := true; end;
   end;
end;

function TreeView_AddTreeNode(tv: TTreeView; tn: TTreeNode; ti: TTreeItem): TTreeNode;
begin
   Result := tv.Items.AddChildObject(tn, DMS_TreeItem_GetName(ti), ti);
   Result.ImageIndex := 5;
   Result.SelectedIndex := 5;
end;

procedure TreeNode_AddDummy(tn: TTreeNode);
begin
   TTreeView(tn.TreeView).Items.AddChildObject(tn, 'DummyForExpansion', Nil);
end;

function TreeView_FillPartial(tv: TTreeView; tnparent: TTreeNode;
        ti: TTreeItem): TTreeNode;
begin
   Result := Nil;
   if(not Assigned(ti))
   then Exit;

   if(not dmfGeneral.AdminMode) and DMS_TreeItem_InHidden(ti)
   then Exit;

   Result := TreeView_AddTreeNode(tv, tnparent, ti);
   Result.DeleteChildren;

   if DMS_TreeItem_HasSubItems(ti) then
      TreeNode_AddDummy(Result);
end;

function TreeNode_FindChild(const tnParent: TTreeNode; const tiChild: TTreeItem): TTreeNode;
var nIdx: Integer;
begin
  for nIdx := 0 to tnParent.Count - 1 do
  begin
    if(tnParent.Item[nIdx].Data = tiChild)
    then begin
      Result := tnParent.Item[nIdx];
      Exit;
    end;
  end;
  Result := nil;
end;

function _FindAssTreeNode(const tnRoot: TTreeNode; const tiThis: TTreeItem): TTreeNode;
begin
  if tiThis = Nil then
  begin
    Result := Nil;
    exit;
  end;

  if tnRoot.Data = tiThis then
  begin
    Result := tnRoot;
    exit;
  end;

  Result := _FindAssTreeNode(tnRoot, DMS_TreeItem_GetParent(tiThis));
  if (Result = Nil) then exit;

{$IFDEF DEBUG}
//   g_frmMain.LogMsg(PMsgChar('Found ' +TreeItem_GetFullName_save(Result.Data))); // DEBUG
{$ENDIF}
  Result := TreeNode_FindChild(Result, tiThis);
end;

function TreeView_FindAssTreeNode(const tv: TTreeView; const tiThis: TTreeItem): TTreeNode;
begin
  Assert(dmfGeneral.AdminMode or not DMS_TreeItem_InHidden(tiThis)); // caller already should have made this optimization
  if (tv.Items.Count > 0) then
  begin
    // Try the selected first. (good chance on performance gain)
    if(tv.Selected <> nil) AND (tv.Selected.Data = tiThis) And Assigned(tiThis)
    then Result := tv.Selected
    // else lookup by reverse parent relation ONLY if not hidden
    else Result := _FindAssTreeNode(tv.Items[0], tiThis)
  end
  else Result := nil;
end;

function TreeNode_ExpandChild(const tnAnchestor: TTreeNode; const tiThis: TTreeItem): TTreeNode;
var pn: TTreeNode;
begin
  Result := nil;
  if not Assigned(tiThis) then exit;

  if tnAnchestor.Data = tiThis then
  begin
    Result := tnAnchestor;
    exit;
  end;

  pn := TreeNode_ExpandChild(tnAnchestor, DMS_TreeItem_GetParent(tiTHis));
  if not Assigned(pn) then exit;
  if tiThis = TConfiguration.CurrentConfiguration.DesktopManager.Base then exit; // don't expand beyond DesktopBase

  if pn.Count = 0 then TreeNode_AddDummy(pn);
  if not pn.Expanded then pn.Expand(false);
  Result := TreeNode_FindChild(pn, tiThis);
  if not Assigned(Result) then // maybe child was added AFTER last expand of pn
     Result := TreeView_FillPartial(frmMain.tvTreeItem, pn, tiThis);
end;

function TreeView_ShowAssTreeNode(tv: TTreeView; const tiThis: TTreeItem): TTreeNode;
begin
  Result := nil;
  if Assigned(tiThis) then
  if tv.Items.Count > 0 then
  if dmfGeneral.AdminMode or not DMS_TreeItem_InHidden(tiThis) then
    Result := TreeNode_ExpandChild(tv.Items[0], tiThis);
  if Assigned(Result)
  then Result.MakeVisible
  else tv.Selected := Nil;
end;

function TreeNode_GetTreeItem(const tn: TTreeNode): TTreeItem;
begin
   Result   := nil;
   if Assigned(tn) then
      Result   := tn.Data;
end; // TreeNode_GetTreeItem

procedure TreeNode_GetCustomDrawItemColors(
  tn: TTreeNode;
  State: TCustomDrawState;
  treeNodeUpdateSet: TTreeNodeUpdateSet;
  var clrText, clrTextBk: COLORREF);
begin
  Assert(Assigned(treeNodeUpdateSet));
  if (dmfGeneral.HasActiveError)
  then treeNodeUpdateSet.Invalidate
  else treeNodeUpdateSet.OnDrawNode(tn);

  if cdsSelected in State then
  begin
    if DMS_TreeItem_IsFailed(tn.Data)
    then clrText := g_cFailed
    else clrText := ColorToRGB(clHighlightText);
    if cdsFocused in State
    then begin
      if dmfGeneral.GetStatus(SF_ShowStateColors) then
        clrTextBk := ColorToRGB(TreeItem_GetStateColor(tn.Data))
      else
        clrTextBk := ColorToRGB(clHighlight)
    end
    else clrTextBk := ColorToRGB(clBlack);
  end
  else
  begin
    if DMS_TreeItem_IsFailed(tn.Data) then
      clrTextBk := g_cFailed
    else
    begin
      if dmfGeneral.GetStatus(SF_ShowStateColors) then
        clrText := ColorToRGB(TreeItem_GetStateColor(tn.Data));
      case DMS_TreeItem_GetSupplierLevel(tn.Data) of
        1: clrTextBk := clSkyBlue;
        2: clrTextBk := clMoneyGreen;
        5: clrTextBk := clBlue;
        6: clrTextBk := clGreen;
      end;
    end;
  end;
end;

function TreeView_OnDrawTreeNode(tv: TTreeView; treeNodeUpdateSet: TTreeNodeUpdateSet; var Message: TWMNotify): Boolean;
var tn: TTreeNode;
begin
    TreeView_OnDrawTreeNode := false;
    if Message.NMHdr^.code <> NM_CUSTOMDRAW then exit;
    if Message.NMHdr^.hWndFrom <> tv.Handle then exit;

    with PNMTVCustomDraw(Message.NMHdr)^ do
    begin
        if (nmcd.dwDrawStage and CDDS_ITEM) = 0 then exit;
        if (nmcd.dwDrawStage <>  CDDS_ITEMPREPAINT) then exit;

        tn := tv.Items.GetNode(HTREEITEM(nmcd.dwItemSpec));
        if tn = nil then Exit;

        try
          TreeNode_GetCustomDrawItemColors(tn,
              TCustomDrawState(Word(nmcd.uItemState)),
              treeNodeUpdateSet,
              clrText, clrTextBk
          );
        except
          on x: CppException do if x.m_isFatal then raise else exit;
          else Exit; // return false.
        end;

        Message.Result := CDRF_DODEFAULT;
        TreeView_OnDrawTreeNode := true;
    end;
end;


procedure LogMsgCallback(clienthandle: TClientHandle; s: TSeverityType; msgCat: TMsgCategory; longMsg: PMsgChar); cdecl;
begin
  if (s in [ST_Error, ST_Fatal]) and Assigned(g_frmMain) then
    INC(frmMain.m_nErrorCount);

  if (s = ST_DispError) then
    dmfGeneral.DispError(longMsg, false);

  if s > ST_MinorTrace then
    AppLogMessage(longMsg);
end;

function CoalesceHeap(size: SizeT; longMsg: PMsgChar): Boolean;  cdecl;
begin
  INC(g_HeapProblemCount);
  if (not SuspendMemoErrorFeedback) then
  begin
    if(TfrmErrorMsg.DisplayMemoryError(size, longMsg) <> mrOK)
    then SuspendMemoErrorFeedback := true;
  end;
  Result := not SuspendMemoErrorFeedback;
end;

var s_clrDlg: TColorDialog;
function TheColorDialog: TColorDialog;
begin
  if not Assigned(s_clrDlg) then
  begin
    s_clrDlg := TColorDialog.Create(nil);
    s_clrDlg.Options := [cdFullOpen, cdShowHelp, cdAnyColor];
  end;
  Assert(Assigned(s_clrDlg));
  Result := s_clrDlg;
end;

initialization
   s_LogFile    := nil;
   s_clrDlg     := nil;

finalization
  LogFileClose;

  s_clrDlg.free;
end.
