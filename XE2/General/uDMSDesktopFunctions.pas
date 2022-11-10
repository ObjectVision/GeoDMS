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

unit uDMSDesktopFunctions;
{
Note (TODO) : Watch out when deleting desktops -> could introduce duplicate names!
}
interface

uses
  uDMSInterfaceTypes, Forms, fMdiChild;

const
  DESKTOPCONTAINER_NAME = 'Desktops';
  DEFAULTDESKTOP_NAME = 'Default';
  DESKTOPTEMPLATECONTAINER_NAME = 'DesktopTemplates';
  DESKTOP_TEMP_NAME = 'Temp';

  DESKTOPROOT_NAME_OBSOLETE = 'DesktopRoot';
  DEFAULTDESKTOP_NAME_OBSOLETE = 'DefaultDesktop';

  DESKTOP_NAME_BASE = 'Desktop';
//  DESKTOPTEMPLATE_NAME_BASE = 'DesktopTemplate_';
  DESKTOPTEMPITEM_NAME_BASE = 'Temp';
  DESKTOPVIEW_NAME_BASE = 'View';

  EMPTYDESKTOPTEMPLATE_NAME = 'EmptyTemplate';

type
  TDesktopManager = class
  private
    m_tiRoot : TTreeItem;
    m_tiDesktopRoot : TTreeItem;
    m_tiDesktopTemplateContainer : TTreeItem;
    m_tiDesktopContainer : TTreeItem;
    m_tiDesktopBase: TTreeItem; // root or container; the sub-item of the config that contains all desktop items.
    m_tiActiveDesktop : TTreeItem;
    m_fMdiMain: TForm;
    m_sLastError : string;

    // creator procs
    function CreateDesktop(sDescription: string; const tiTemplate: TTreeItem): TTreeItem;

    // generator procs
    function GetNewDesktopName: ItemString;
    function GetNewViewName: ItemString;
    function DesktopNameOK(const sName: ItemString; var sMessage: ItemString): boolean;

    // display procs
    function IsForm(tiView : TTreeItem): Boolean;
    function CreateForm(tiView : TTreeItem): TfrmMdiChild;
    function RestoreViewPosition(const tiView: TTreeItem; fMdiMain: TForm): Boolean;

    procedure LoadDesktop(tiDesktop: TTreeItem);  overload;
    function GetNewTempItemName: ItemString;
  public
    // init / final
    constructor Create;
    destructor Destroy; override;
    function Init(tiRoot: TTreeItem; fMdiMain: TForm): boolean;

    // user operations
    function  GetDesktopTemplateContainer: TTreeItem;
    procedure NewDesktop;
    procedure OpenDesktop;
    procedure SaveDesktops;
    procedure SetStartupDesktop;

    // system operations
    procedure CloseCurrentDesktop;
    function  GetActiveDesktopName: ItemString;
    function  GetActiveDesktopDescr: SourceString;
    function  GetActiveTemp: TTreeItem;
    function  GetActiveViewData: TTreeItem;
    function  LoadDesktop(sStartupDesktop : ItemString): TTreeItem; overload;
    procedure StoreCurrentDesktopData;

    // view operations
    function  NewView(contextItem: TTreeItem; viewTypeName, ctrlTypeName: PSourceChar; ct: TTreeItemViewStyle): TTreeItem;
    procedure DeleteView(tiView: TTreeItem);
    procedure StoreViewPosition(tiView: TTreeItem; const fMdiMain: TForm);

    // utilities (these should probably be somewhere else)
    function GetNewTempItem: TTreeItem;
    class function GetUniqueName(sNameBase: ItemString; tiContainer: TTreeItem): ItemString;

    property Base: TTreeItem read m_tiDesktopBase;
    property ActiveDesktop : TTreeItem read m_tiActiveDesktop;
  end;


implementation

uses
  fmDmsControl,
  fPrimaryDataViewer,
  Dialogs, SysUtils, fOpenDesktop,
  fEditProperty, uDMSUtils,
  Controls,
  Classes, OptionalInfo,
  uGenLib,
  uDmsInterface,
  fmHistogramControl,
  fMain;


// constructor
constructor TDesktopManager.Create;
begin
  m_tiRoot := nil;
  m_tiDesktopRoot := nil;
  m_tiDesktopTemplateContainer := nil;
  m_tiDesktopContainer := nil;
  m_tiActiveDesktop := nil;
  m_sLastError := '';
end;

// destructor
destructor TDesktopManager.Destroy;
var
  tiTemp : TTreeItem;
begin
  if Assigned(m_tiDesktopRoot) then
  begin
    tiTemp := DMS_TreeItem_GetItem(m_tiDesktopRoot, DESKTOP_TEMP_NAME, nil);
    if Assigned(tiTemp)
    then DMS_TreeItem_SetAutoDelete(tiTemp);
  end;
  SetCountedRef(m_tiActiveDesktop, nil);
  SetCountedRef(m_tiDesktopTemplateContainer, nil);
  SetCountedRef(m_tiDesktopContainer, nil);
  SetCountedRef(m_tiDesktopRoot, nil);
  SetCountedRef(m_tiRoot, nil);
  inherited;
end;

// init
function TDesktopManager.Init(tiRoot: TTreeItem; fMdiMain: TForm): boolean;
var
  tiTemp : TTreeItem;
begin
  Result := false;

  // check parameters
  if((fMdiMain = nil) or (fMdiMain.FormStyle <> fsMDIForm))
  then begin
    m_sLastError := 'No valid MDI form specified.';
    Exit;
  end;

  if(tiRoot = nil)
  then begin
    m_sLastError := 'No valid root node specified.';
    Exit;
  end;

  // store parameters
  SetCountedRef(m_tiRoot, tiRoot);
  m_fMdiMain := fMdiMain;

    SetCountedRef(m_tiDesktopRoot, DMS_TreeItem_GetItem(m_tiRoot, DESKTOPROOT_NAME_OBSOLETE, nil));
    m_tiDesktopBase := m_tiDesktopRoot;
    if not Assigned(m_tiDesktopRoot) then
      SetCountedRef(m_tiDesktopRoot, m_tiRoot);

//  init desktop container
    SetCountedRef(m_tiDesktopContainer, DMS_CreateTreeItem(m_tiDesktopRoot, DESKTOPCONTAINER_NAME));
    assert(Assigned(m_tiDesktopContainer));
    if not(Assigned(m_tiDesktopBase)) then m_tiDesktopBase := m_tiDesktopContainer;

    DMS_PropDef_SetValueAsCharArray(GetPropDef(pdIsHidden    ), m_tiDesktopContainer, 'true');
    DMS_PropDef_SetValueAsCharArray(GetPropDef(pdConfigStore), m_tiDesktopContainer, DESKTOPCONTAINER_NAME+'.dms');

//  setup default desktop
    tiTemp := DMS_TreeItem_GetItem(m_tiDesktopContainer, DEFAULTDESKTOP_NAME_OBSOLETE, nil);
    if not Assigned(tiTemp) then
    begin
      tiTemp := DMS_CreateTreeItem(m_tiDesktopContainer, DEFAULTDESKTOP_NAME);
      assert(Assigned(tiTemp));
      DMS_TreeItem_SetDescr(tiTemp, '<Default Desktop>');
      DMS_PropDef_SetValueAsCharArray(GetPropDef(pdConfigStore), tiTemp, DEFAULTDESKTOP_NAME+'.dms');
    end;
    Result := true;
end;

function TDesktopManager.GetDesktopTemplateContainer;
var tiTemp: TTreeItem;
begin
//  init desktop template container
    if not Assigned(m_tiDesktopTemplateContainer) then
    begin
      SetCountedRef(m_tiDesktopTemplateContainer, DMS_TreeItem_GetItem(m_tiDesktopRoot, DESKTOPTEMPLATECONTAINER_NAME, nil));
      if not Assigned(m_tiDesktopTemplateContainer) then
      begin
        SetCountedRef(m_tiDesktopTemplateContainer, DMS_CreateTreeItem(m_tiDesktopRoot, DESKTOPTEMPLATECONTAINER_NAME));
        assert(Assigned(m_tiDesktopTemplateContainer));
        DMS_PropDef_SetValueAsCharArray(GetPropDef(pdIsHidden    ), m_tiDesktopTemplateContainer, 'true');
        DMS_PropDef_SetValueAsCharArray(GetPropDef(pdConfigStore), m_tiDesktopTemplateContainer, DESKTOPTEMPLATECONTAINER_NAME+'.dms');

    //  setup empty template
        tiTemp := DMS_CreateTreeItem(m_tiDesktopTemplateContainer, EMPTYDESKTOPTEMPLATE_NAME);
        assert(Assigned(tiTemp));
        DMS_TreeItem_SetDescr(tiTemp, '<EMPTY DESKTOP>');
      end
    end;
    Result := m_tiDesktopTemplateContainer;
end;

procedure TDesktopManager.NewDesktop;
var
  sMessage, sNewDesktopName: ItemString;
  tiDesktopTemplate : TTreeItem;
  bOK : boolean;
begin
  // select desktop template
  tiDesktopTemplate := TfrmOpenDesktop.Display(GetDesktopTemplateContainer, 'New Desktop: Select Template');

  if(tiDesktopTemplate <> nil)
  then begin
    sNewDesktopName := '';
    repeat
      bOK := TfrmEditProperty.Display('New Desktop: name ' + sMessage, sNewDesktopName)
    until (DesktopNameOK(sNewDesktopName, sMessage) or (not bOK));

    if bOK then LoadDesktop( CreateDesktop(sNewDesktopName, tiDesktopTemplate) );
  end;
end;

procedure TDesktopManager.SaveDesktops;
begin
  StoreCurrentDesktopData;
  if m_tiDesktopRoot<> m_tiRoot then // OBSOLETE STRUCTURE
    DMS_TreeItem_Dump(m_tiDesktopRoot, DESKTOPROOT_NAME_OBSOLETE+'.dms')
  else
  begin
    DMS_TreeItem_Dump(m_tiDesktopContainer, DESKTOPCONTAINER_NAME+'.dms');
    if Assigned(m_tiDesktopTemplateContainer) then
      DMS_TreeItem_Dump(m_tiDesktopTemplateContainer, DESKTOPTEMPLATECONTAINER_NAME+'.dms');
  end
end;

// open desktop
procedure TDesktopManager.OpenDesktop;
var
  tiDesktop          : TTreeItem;
begin
  // select desktop
  tiDesktop := TfrmOpenDesktop.Display(m_tiDesktopContainer);
  if Assigned(tiDesktop) then LoadDesktop(tiDesktop);
end;

function TrySubItem(tiDesktopContainer: TTreeItem; subItemName: ItemString): TTreeItem;
begin
  if subItemName = '' then
    Result := nil
  else
    Result := DMS_TreeItem_GetItem(tiDesktopContainer, PItemChar(subItemName), nil);
end;

function TDesktopManager.LoadDesktop(sStartupDesktop : ItemString): TTreeItem;
var
  tiTemp : TTreeItem;
begin
  tiTemp := TrySubItem(m_tiDesktopContainer, sStartupDesktop);
  if not assigned(tiTemp) then tiTemp := TrySubItem(m_tiDesktopContainer, TreeItemDialogData(m_tiDesktopContainer));
  if not assigned(tiTemp) then tiTemp := TrySubItem(m_tiDesktopContainer, DEFAULTDESKTOP_NAME);
  if not assigned(tiTemp) then tiTemp := TrySubItem(m_tiDesktopContainer, DEFAULTDESKTOP_NAME_OBSOLETE);

  LoadDesktop(tiTemp);

  Result := m_tiActiveDesktop;
end;

function TDesktopManager.IsForm(tiView : TTreeItem): Boolean;
begin
  Result := TreeItemDialogTypeStr(tiView) = TfrmPrimaryDataViewer.ClassName;
end;

function TDesktopManager.CreateForm(tiView : TTreeItem): TfrmMdiChild;
var
  sCtlName : string;
  p: Integer;
  ct: TTreeItemViewStyle;

begin
  Result := nil;

  sCtlName := TreeItemDialogData(tiView);
  p := pos('.', sCtlname);
  if p=0
  then ct := tvsDefault
  else
  begin
    ct := TTreeItemViewStyle( StrToInt(copy(sCtlName, p+1, length(sCtlName)-p)) );
    sCtlName := copy(sCtlName, 1, p-1);
  end;

  // dmscontrol
  if(sCtlname = TfraDmsControl.ClassName)
  then begin
    if ct <> tvsDefault then
      Result := TfrmPrimaryDataViewer.CreateFromDesktop(Application, TfraDmsControl, tiView, ct);
    Exit;
  end;

end;


function TDesktopManager.CreateDesktop(sDescription: string; const tiTemplate: TTreeItem): TTreeItem;
var newName: ItemString;
begin
  newName := GetNewDesktopName;
  Result := DMS_TreeItem_Copy(m_tiDesktopContainer, tiTemplate, PItemChar(newName));

  if(Result = nil)
  then begin
    m_sLastError := 'Unable to create desktop.';
    Exit;
  end;
  DMS_PropDef_SetValueAsCharArray(GetPropDef(pdConfigStore), Result, PSourceChar(newName+'.dms'))     ;
  DMS_TreeItem_SetDescr(Result, PSourceChar(SourceString(sDescription)));
end;

function TDesktopManager.GetNewDesktopName: ItemString;
begin
  Result := GetUniqueName(DESKTOP_NAME_BASE, m_tiDesktopContainer);
end;

function TDesktopManager.GetActiveDesktopName: ItemString;
begin
  if Assigned(m_tiActiveDesktop)
  then Result := DMS_TreeItem_GetName(m_tiActiveDesktop)
  else Result := '';
end;

function TDesktopManager.GetActiveDesktopDescr: SourceString;
begin
  if Assigned(m_tiActiveDesktop)
  then Result := TreeItemDescrOrName(m_tiActiveDesktop)
  else Result := '<NO ACTIVE DESKTOP>';
end;

function TDesktopManager.GetActiveTemp: TTreeItem;
begin
  assert(Assigned(m_tiDesktopRoot));
  Result := DMS_CreateTreeItem(m_tiDesktopRoot, DESKTOP_TEMP_NAME);
  DMS_PropDef_SetValueAsCharArray(GetPropDef(pdIsHidden    ), Result, 'true');
end;

function  TDesktopManager.GetActiveViewData: TTreeItem;
begin
  if Assigned(m_tiActiveDesktop) then
     Result := DMS_CreateTreeItem(m_tiActiveDesktop, VIEWDATA_NAME)
  else
     Result := Nil;
end;

function TDesktopManager.GetNewTempItemName: ItemString;
begin
  Result := GetUniqueName(DESKTOPTEMPITEM_NAME_BASE, GetActiveTemp);
end;

function TDesktopManager.GetNewTempItem: TTreeItem;
begin
  Result := DMS_CreateTreeItem(GetActiveTemp, PItemChar( GetNewTempItemName ) );
  Assert(Assigned(Result));
end;

procedure TDesktopManager.LoadDesktop(tiDesktop: TTreeItem);
var
  tiView, tiTemp : TTreeItem;
  frmNew: TfrmMdiChild;
  slTemp : TStringList;
  bSourceDescrShown: Boolean;
  bMustTile: Boolean;
begin
  StoreCurrentDesktopData;
  CloseCurrentDesktop;
  SetCountedRef(m_tiActiveDesktop, tiDesktop);

  // restore panel settings
  slTemp := TStringList.Create;

  try
    // treeview
    tiTemp := DMS_TreeItem_GetItem(tiDesktop, 'showtreedata', nil);
    bSourceDescrShown := true;
    if Assigned(tiTemp) then
    begin
      slTemp.Text := StringReplace(TreeItemDialogData(tiTemp), ';', #13#10, [rfReplaceAll]);
      if (slTemp.Count = 2) and IsInteger(slTemp[1]) then
      begin
        bSourceDescrShown := (slTemp[0] = 'Y');
        TfrmMain(m_fMdiMain).tvTreeItem.Width := strtoint(slTemp[1]);
      end
    end;
    TfrmMain(m_fMdiMain).SetTreeViewVisible(bSourceDescrShown);

    // details
    tiTemp := DMS_TreeItem_GetItem(tiDesktop, 'showdetailpagesdata', nil);
    bSourceDescrShown := true;
    if Assigned(tiTemp) then
    begin
      slTemp.Text := StringReplace(TreeItemDialogData(tiTemp), ';', #13#10, [rfReplaceAll]);
      if (slTemp.Count = 2) and IsInteger(slTemp[1]) then
      begin
        bSourceDescrShown:= (slTemp[0] = 'Y');
        TfrmMain(m_fMdiMain).pnlExpl.Width := strtoint(slTemp[1]);
      end
    end;
    TfrmMain(m_fMdiMain).SetDetailPageVisible(bSourceDescrShown);

    // eventlog, already set from bin/dms.ini, but overridable in dms configuration files
    tiTemp := DMS_TreeItem_GetItem(tiDesktop, 'showlogdata', nil);
    bSourceDescrShown := true;
    if Assigned(tiTemp) then
    begin
      slTemp.Text := StringReplace(TreeItemDialogData(tiTemp), ';', #13#10, [rfReplaceAll]);

      if (slTemp.Count = 2) and IsInteger(slTemp[1]) then
      begin
        bSourceDescrShown := (slTemp[0] = 'Y');
        TfrmMain(m_fMdiMain).pnlEventlog.Height := strtoint(slTemp[1]);
      end;
    end;
    TfrmMain(m_fMdiMain).SetEventLogVisible(bSourceDescrShown);
    TfrmMain(m_fMdiMain).SetToolbarVisible (true);
  finally
    slTemp.Free;
  end;

  // treeview
  tiTemp := DMS_TreeItem_GetItem(tiDesktop, 'treeviewdata', nil);
  frmMain.RestoreTreeview(tiTemp);

  // restore views
  bSourceDescrShown := false;
  bMustTile         := true;
  tiView := DMS_TreeItem_GetFirstVisibleSubItem(tiDesktop);
  while Assigned(tiView) do
  begin
    if (not DMS_TreeItem_IsHidden(tiView)) and (not DMS_TreeItem_InTemplate(tiView)) and IsForm(tiView)
    then begin
      frmNew := CreateForm(tiView);
      if Assigned(frmNew) then
      begin
        if (not bSourceDescrShown) then
        begin
          TfrmOptionalInfo.Display(OI_Disclaimer, false);
          bSourceDescrShown := True;
        end;
        if RestoreViewPosition(tiView, frmNew) then
          bMustTile := false;
        frmNew.Show;
      end;
    end;
    tiView := DMS_TreeITem_GetNextVisibleItem(tiView);
  end;

  if bSourceDescrShown and bMustTile then // all (at least 1) views did not have a valid WindowPos
    frmMain.TileVerClick(Nil);

  DMS_Config_SetActiveDesktop(tiDesktop);
end;


function TDesktopManager.NewView(contextItem: TTreeItem; viewTypeName, ctrlTypeName: PSourceChar; ct: TTreeItemViewStyle): TTreeItem;
begin
  if Assigned(contextItem) then
    Result := contextItem
  else
    Result := DMS_CreateTreeItem(m_tiActiveDesktop, PItemChar(ItemString(GetNewViewName)));
  DMS_PropDef_SetValueAsCharArray(GetPropDef(pdDialogType), Result, viewTypeName);
  DMS_PropDef_SetValueAsCharArray(GetPropDef(pdDialogData), Result, PSourceChar(SourceString(String(ctrlTypeName)+'.'+IntToStr(ord(ct)))));
end;

function TDesktopManager.GetNewViewName: ItemString;
begin
  Result := GetUniqueName(DESKTOPVIEW_NAME_BASE, m_tiActiveDesktop);
end;

function TDesktopManager.DesktopNameOK(const sName: ItemString; var sMessage: ItemString): boolean;
var
  ti: TTreeItem;
begin
  Result := length(trim(sName)) > 0;

  if(not Result)
  then begin
    sMessage := '';
    Exit;
  end
  else begin
    ti := DMS_TreeItem_GetFirstVisibleSubItem(m_tiDesktopContainer);
    while Assigned(ti) do
    begin
      if(lowercase(TreeItemDescrOrName(ti)) = lowercase(sName))
      then begin
        Result := false;
        break;
      end;
      ti := DMS_TreeItem_GetNextVisibleItem(ti);
    end;
  end;

  if(not Result)
  then sMessage := '(already exists)';
end;


class function TDesktopManager.GetUniqueName(sNameBase: ItemString; tiContainer: TTreeItem): ItemString;
var
  nIdx : integer;
begin
  Result := sNameBase;
  nIdx := 1;
  while(DMS_TreeItem_GetItem(tiContainer, PItemChar(Result), nil) <> nil) do
  begin
    inc(nIdx);
    Result := sNameBase + inttostr(nIdx);
  end;
end;

procedure TDesktopManager.CloseCurrentDesktop;
var
  nIdx : integer;
begin
  for nIdx := m_fMdiMain.MDIChildCount - 1 downto 0 do
    m_fMdiMain.MDIChildren[nIdx].Free;
  DMS_Config_SetActiveDesktop(nil);
end;

procedure TDesktopManager.DeleteView(tiView: TTreeItem);
begin
  DMS_PropDef_SetValueAsCharArray(GetPropDef(pdIsHidden), tiView, 'True');
  DMS_TreeItem_SetAutoDelete(tiView);
end;

procedure TDesktopManager.SetStartupDesktop;
var
  tiDesktop : TTreeItem;
begin
  // select desktop
  tiDesktop := TfrmOpenDesktop.Display(m_tiDesktopContainer, 'Set Startup Desktop');

  if(tiDesktop <> nil)
  then begin
    DMS_PropDef_SetValueAsCharArray(GetPropDef(pdDialogData), m_tiDesktopContainer, DMS_TreeItem_GetName(tiDesktop));
  end;
end;

procedure TDesktopManager.StoreViewPosition(tiView: TTreeItem; const fMdiMain: TForm);
var
  sViewPosition : SourceString;
begin
  if DMS_TreeItem_IsEndogenous(tiView) then exit;

  sViewPosition :=
      inttostr(integer(fMdiMain.WindowState)) + ','
    + inttostr(fMdiMain.Left) + ','
    + inttostr(fMdiMain.Top) + ','
    + inttostr(fMdiMain.Width) + ','
    + inttostr(fMdiMain.Height);

  DMS_PropDef_SetValueAsCharArray(
    GetPropDef(pdDialogData),
    DMS_CreateTreeItem(tiView, 'WindowPos'),
    PSourceChar(sViewPosition)
  );
end;

function TDesktopManager.RestoreViewPosition(const tiView: TTreeItem; fMdiMain: TForm): Boolean;
var
  ti: TTreeItem;
  slWindowPos : TStringList;
begin
  Result := false;
  ti := DMS_TreeItem_GetItem(tiView, 'WindowPos', Nil);
  if not Assigned(ti) then exit;

  slWindowPos := TStringList.Create;
  try
    slWindowPos.CommaText := TreeItemDialogData(ti);

    if(slWindowPos.Count = 5)
    then begin
//    Problem with wsMaximized: ShowWindow van een volgend window naar SW_NORMAL zorgt alsnog voor wsNormal van dit window
//    fMdiMain.WindowState := TWindowState(strtoint(slWindowPos[0]));
      fMdiMain.WindowState := wsNormal;
//    if(fMdiMain.WindowState = wsNormal)
//    then begin
        fMdiMain.Width := strtoint(slWindowPos[3]);
        fMdiMain.Height := strtoint(slWindowPos[4]);
        fMdiMain.Left := strtoint(slWindowPos[1]);
        fMdiMain.Top := strtoint(slWindowPos[2]);
        Result := true; // block tiling after processing WindosPos of all Views.
//      end;
    end;
  finally
    slWindowPos.Free;
  end;
end;

procedure TDesktopManager.StoreCurrentDesktopData;
var
  nIdx : integer;
  tiTemp : TTreeItem;
begin
  if not assigned(m_tiActiveDesktop) then Exit;

  // save panel settings
  tiTemp := DMS_CreateTreeItem(m_tiActiveDesktop, 'showtreedata');
  DMS_PropDef_SetValueAsCharArray(GetPropDef(pdDialogData), tiTemp,
    PSourceChar(SourceString(IIF(TfrmMain(m_fMdiMain).Tree2.Checked, 'Y', 'N') + ';' + inttostr(TfrmMain(m_fMdiMain).tvTreeItem.Width))));

  tiTemp := DMS_CreateTreeItem(m_tiActiveDesktop, 'showdetailpagesdata');
  DMS_PropDef_SetValueAsCharArray(GetPropDef(pdDialogData), tiTemp,
    PSourceChar(SourceString(IIF(TfrmMain(m_fMdiMain).DetailPages1.Checked, 'Y', 'N') + ';' + inttostr(TfrmMain(m_fMdiMain).pnlExpl.Width))));

  tiTemp := DMS_CreateTreeItem(m_tiActiveDesktop, 'showlogdata');
  DMS_PropDef_SetValueAsCharArray(GetPropDef(pdDialogData), tiTemp,
    PSourceChar(SourceString(IIF(TfrmMain(m_fMdiMain).Eventlog1.Checked, 'Y', 'N') + ';' + inttostr(TfrmMain(m_fMdiMain).pnlEventlog.Height))));

  tiTemp := DMS_CreateTreeItem(m_tiActiveDesktop, 'treeviewdata');
  frmMain.SaveTreeView(tiTemp);

  for nIdx := m_fMdiMain.MDIChildCount - 1 downto 0 do
  begin
    if(m_fMdiMain.MDIChildren[nIdx] is TfrmMdiChild)
    then TfrmMdiChild(m_fMdiMain.MDIChildren[nIdx]).StoreDesktopData;
  end;
end;

end.
