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

unit fExportASCGrid;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls, Buttons, uDMSInterfaceTypes, fTreeItemPickList, ComCtrls;

type
  TfrmExportASCGrid = class(TForm)
    dlgSave: TSaveDialog;
    Panel1: TPanel;
    edtFileName: TEdit;
    Label1: TLabel;
    cobBaseGrid: TComboBox;
    lblBaseGrid: TLabel;
    btnOK: TButton;
    btnCancel: TButton;
    btnHelp: TButton;
    lblSubTitle: TLabel;
    Label5: TLabel;
    cbxUseBaseGrid: TCheckBox;
    lblDomain: TLabel;
    lbxDomain: TListBox;
    cbxExportBoth: TCheckBox;
    cbxProcessIdle: TCheckBox;
    btnShowExports: TButton;
    btnBrowse: TButton;
    lblExportBoth: TLabel;
    stnMain: TStatusBar;
    constructor  Create(const ti: TTreeItem; asAction: boolean); reintroduce;
    destructor Destroy; override;

    procedure FormActivate(Sender: TObject);
    procedure btnBrowseClick(Sender: TObject);
    procedure btnOKClick(Sender: TObject);
    procedure cbxUseBaseGridClick(Sender: TObject);
    procedure lbxDomainClick(Sender: TObject);
    procedure cbxExportBothClick(Sender: TObject);
    procedure btnShowExportsClick(Sender: TObject);
    procedure edtFileNameChange(Sender: TObject);
  private
    m_TreeItem  : TTreeItem;
    m_AsAction, m_AsDir: Boolean;
    m_DataItems, m_GridDomains, m_AttrDomains: TList;
    m_InInit    : Boolean;


    function     IsNumericOrBooleanDataItem(ti: TTreeItem): Boolean;
    function     CanExport(ti: TTreeItem): Boolean;
    function     MustExport(ti: TTreeItem): Boolean;
    function     GetFileNameBase(ti: TTreeItem): String;
    procedure    UpdateFilenameExtension;
    function     ExportListCreate: TfrmTreeItemPickList;
    function     ExportItem(tiExports, ti: TTreeItem): Boolean;
  public
      class function  ShowForm(const ti: TTreeItem; asAction: boolean): boolean;
  end;

implementation

{$R *.DFM}

uses
   uGenLib, uDmsInterface,
   uDMSUtils, fMain,
   uDMSTreeviewFunctions,
   ItemColumnProvider,
   Configuration, dmGeneral, uMapClasses;

var s_strLastDir: FileString;

class function TfrmExportASCGrid.ShowForm(const ti: TTreeItem; asAction: boolean): boolean;
begin
   with TfrmExportASCGrid.Create(ti, asAction) do
   begin
      Result := ShowModal <> mrCancel;
      Free;
   end;
end;

constructor TfrmExportASCGrid.Create(const ti: TTreeItem; asAction: boolean);
begin
   m_TreeItem := ti;
   m_AsAction := asAction;
   inherited   Create(Application);
   m_DataItems   := TList.Create;
   m_GridDomains := TList.Create;
   m_AttrDomains := TList.Create;
   dmfGeneral.RegisterCurrForm(self);
end; // Create

destructor TfrmExportASCGrid.Destroy;
begin
  dmfGeneral.UnRegisterCurrForm(self);
  m_DataItems.Free;
  m_GridDomains.Free;
  m_AttrDomains.Free;
  inherited Destroy;
end;

procedure AddToList(lst: TList; item: Pointer);
begin
  if lst.IndexOf(item) = -1 then lst.Add(item);
end;

procedure FillListBox(lb: TListBox; lst: TList);
var i: Integer;
begin
  lb.Clear;
  for i:=0 to lst.count-1 do
    lb.Items.AddObject(string( TreeItem_GetFullName_Save(lst[i]) ), lst[i]);
end;

function SkipTree(ti: TTreeItem): Boolean;
begin
  Result := ((not dmfGeneral.AdminMode) and DMS_TreeItem_InHidden(ti))
     or DMS_TreeItem_InTemplate(ti);
end;

function TfrmExportASCGrid.IsNumericOrBooleanDataItem(ti: TTreeItem): Boolean;
begin
  Result := IsDataItem(ti) and AbstrUnit_IsNumericOrBoolean(DMS_DataItem_GetValuesUnit(ti));
end;

function TfrmExportASCGrid.CanExport(ti: TTreeItem): Boolean;
  var i: Integer; du: TAbstrUnit;
begin
  Result := false;
  if IsNumericOrBooleanDataItem(ti) then
  begin
    du := DMS_DataItem_GetDomainUnit(ti);
    i := lbxDomain.Items.IndexOfObject(du);
    if (i >= 0) and lbxDomain.Selected[i] then
       Result := true;
  end;
end;

function TfrmExportASCGrid.MustExport(ti: TTreeItem): Boolean;
begin
  Result := CanExport(ti) and
    (cbxExportBoth.Checked OR DMS_TreeItem_HasCalculator(ti))
end;

procedure TfrmExportASCGrid.FormActivate(Sender: TObject); // REMOVE, move to FormCreate and test
  function HasBaseGrid(du: TAbstrUnit): Boolean;
  begin
    cobBaseGrid.Clear;
    FillCbxBaseGrids( cobBaseGrid, du);
    Result := cobBaseGrid.Items.Count > 0;
  end;

  var visitors: TList;

  procedure ScanItemsImpl(ti: TTreeItem);
  var du: TAbstrUnit;
  begin
    if SkipTree(ti) then exit;
    if visitors.IndexOf(ti) <> -1 then exit;
    visitors.Add(ti);

    if IsNumericOrBooleanDataItem(ti) then
    begin
      du := DMS_DataItem_GetDomainUnit(ti);
      if AbstrUnit_IsPoint(du)   then AddToList(m_GridDomains, du);
      if AbstrUnit_IsInteger(du) AND HasBaseGrid(du) then
        AddToList(m_AttrDomains, du);
    end;

    // recursive search
    ti := DMS_TreeItem_GetFirstVisibleSubItem(ti);
    while Assigned(ti) do
    begin
      ScanItemsImpl( ti );
      ti := DMS_TreeITem_GetNextVisibleItem(ti);
    end;
  end;

  procedure ScanItems(ti: TTreeItem);
  begin
    visitors := TList.Create;
    try
      ScanItemsImpl(ti);
    finally
      visitors.Destroy;
    end;
  end;

var strWorkDir : FileString;
begin
   lblSubTitle.Caption := 'Container: ' + string( TreeItem_GetFullName_Save(m_TreeItem) );
   strWorkDir := s_strLastDir;
   if (strWorkDir='') then
      strWorkDir := TConfiguration.CurrentConfiguration.GetResultsPath;
   assert(strWorkDir[ length(strWorkDir) ] <> '/');

   edtFileName.Text   := strWorkDir + '/' + string( DMS_TreeItem_GetName(m_TreeItem) ) + '.asc';

   // first make a list of candidate data-items, domains, and some flags
   m_InInit := true;
   try
     m_GridDomains.Clear;
     m_AttrDomains.Clear;
     ScanItems(m_TreeItem);
     cbxUseBaseGrid.Visible := (m_AttrDomains.Count > 0);
     cbxUseBaseGrid.Enabled := (m_GridDomains.Count > 0) AND (m_AttrDomains.Count > 0);
     cbxUseBaseGrid.Checked := (m_GridDomains.Count = 0) AND (m_AttrDomains.Count > 0);

     cbxProcessIdle.Visible := NOT m_AsAction;
   finally
     m_InInit := false;
   end;

   cbxUseBaseGridClick(Nil);
end; // FormActivate

procedure TfrmExportASCGrid.cbxUseBaseGridClick(Sender: TObject);
var i: Integer;
begin
  if (m_InInit) then exit;
  m_InInit := true;
  try
    lbxDomain.Clear;
    if cbxUseBaseGrid.Checked then
    begin
       lblDomain.Caption := 'VatDomain';
       lblBaseGrid.Visible := true;
       cobBaseGrid.Visible := true;
       lbxDomain.MultiSelect := false;
       FillListBox(lbxDomain, m_AttrDomains);
       if lbxDomain.items.count > 0 then lbxDomain.ItemIndex := 0;
    end
    else
    begin
       lblDomain.Caption := 'GridDomain';
       lblBaseGrid.Visible := false;
       cobBaseGrid.Visible := false;
       lbxDomain.MultiSelect := true;
       FillListBox(lbxDomain, m_GridDomains);
       for i:=0 to lbxDomain.items.Count-1 do lbxDomain.Selected[i] := true;
    end;

    if lbxDomain.Items.Count = 1 then
      lbxDomain.Enabled := false;
  finally
    m_InInit := false;
  end;
  lbxDomainClick(Sender);
end;

procedure TfrmExportASCGrid.lbxDomainClick(Sender: TObject);
var hasCalculated, hasSource: Boolean;

  var visitors: TList;

  procedure ScanItemsImpl(ti: TTreeItem);
  begin
    if SkipTree(ti) then exit;
    if visitors.IndexOf(ti) <> -1 then exit;
    visitors.Add(ti);

    if CanExport(ti) then
       if DMS_TreeItem_HasCalculator(ti)
       then hasCalculated := true
       else hasSource := true;
    if (hasCalculated AND hasSource) then exit;

    // recursive search
    ti := DMS_TreeItem_GetFirstVisibleSubItem(ti);
    while Assigned(ti) do
    begin
      ScanItemsImpl( ti );
      ti := DMS_TreeITem_GetNextVisibleItem(ti);
    end;
  end;

  procedure ScanItems(ti: TTreeItem);
  begin
    visitors := TList.Create;
    try
      ScanItemsImpl(ti);
    finally
      visitors.Destroy;
    end;
  end;

var i: Integer; oldBaseGrid: TAbstrDataItem;
begin
  if (m_InInit) then exit;

  hasCalculated := false;
  hasSource := false;

  m_InInit := true;
  try
    ScanItems(m_TreeItem);

    lblExportBoth.Caption := '';
    if (hasCalculated) then
    begin
      cbxExportBoth.Enabled := hasSource;
      if not hasSource
      then begin lblExportBoth.Caption := '(Only calculated data selected for export)'; cbxExportBoth.Checked := false; end
    end
    else
    begin
      cbxExportBoth.Checked := hasSource;
      cbxExportBoth.Enabled := false;
      if hasSource
      then lblExportBoth.Caption := '(Only source data selected for export)'
      else lblExportBoth.Caption := 'No data selected for export'
    end;

    if cbxUseBaseGrid.Checked then
    begin
       oldBaseGrid := Nil;
       if cobBaseGrid.Text <> '' then
          oldBaseGrid  := DMS_TreeItem_GetItem(
                DMS_TreeItem_GetRoot(m_TreeItem),
                PItemChar(ItemString(cobBaseGrid.Text)),
                DMS_AbstrDataItem_GetStaticClass
          );
       cobBaseGrid.Clear;
       i := lbxDomain.ItemIndex;
       if (i>=0) then
       begin
          FillCbxBaseGrids( cobBaseGrid, lbxDomain.Items.Objects[i]);
          if cobBaseGrid.Items.Count = 0 then
            MessageDlg('no appropiate base grids found for domain '+lbxDomain.Items[i], mtError, [mbOK], 0);
       end;
       i := cobBaseGrid.Items.IndexOfObject(oldBaseGrid);
       if (i<0) then i:=0;
       if (i < cobBaseGrid.Items.Count)
       then cobBaseGrid.Text := cobBaseGrid.Items[i]
       else cobBaseGrid.Text := '';
       cobBaseGrid.Enabled := cobBaseGrid.Items.Count > 1;
    end;
  finally
    m_InInit := false;
  end;

  cbxExportBothClick(Sender);
end;


procedure TfrmExportASCGrid.cbxExportBothClick(Sender: TObject);
var nrItems: Integer;

  var visitors: TList;

  procedure ScanItemsImpl(ti: TTreeItem);
  begin
    if SkipTree(ti)   then exit;
    if visitors.IndexOf(ti) <> -1 then exit;
    visitors.Add(ti);

    if MustExport(ti) then INC(nrItems);

    // recursive search
    ti := DMS_TreeItem_GetFirstVisibleSubItem(ti);
    while Assigned(ti) do
    begin
      ScanItemsImpl( ti );
      ti := DMS_TreeITem_GetNextVisibleItem(ti);
    end;
  end;

  procedure ScanItems(ti: TTreeItem);
  begin
    visitors := TList.Create;
    try
      ScanItemsImpl(ti);
    finally
      visitors.Destroy;
    end;
  end;

begin
  if (m_InInit) then exit;
  m_InInit := true;
  try
    nrItems := 0;
    if (not cbxUseBaseGrid.Checked) or (cobBaseGrid.Text<>'') then
    begin
      ScanItems(m_TreeItem);
      btnShowExports.Caption := 'Show '+IntToStr(nrItems) + ' exports';
      btnShowExports.Enabled := (nrItems > 0);
      btnOK.Enabled := (nrItems > 0);
    end
    else
    begin
      btnShowExports.Caption := 'NO BASE GRID';
      btnShowExports.Enabled := FALSE;
      btnOK.Enabled := FALSE;
    end;
  finally
    m_InInit := false;
  end;
  m_AsDir := (nrItems <> 1) or not MustExport(m_TreeItem);
  UpdateFilenameExtension;
end;

procedure TfrmExportASCGrid.edtFileNameChange(Sender: TObject);
begin
  UpdateFilenameExtension;
end;

procedure TfrmExportASCGrid.UpdateFilenameExtension;
begin
  if (m_InInit) then exit;
  if m_AsDir then
    edtFileName.Text := GetFileNameBase(m_TreeItem) + '/'
  else
    edtFileName.Text := GetFileNameBase(m_TreeItem) + '.asc';
end;


function TfrmExportASCGrid.GetFileNameBase(ti: TTreeItem): String;
begin
   if (ti = m_TreeItem) or not Assigned(ti) then
   begin
     Result := edtFileName.Text;
     Result := ChangeFileExt(Result, '');
     if (Length(Result) > 0) and (Result[Length(Result)] = '/') then Result := copy(Result, 1, length(Result) - 1); // cut off last '/'
   end
   else
     Result := GetFileNameBase(DMS_TreeItem_GetParent(ti) ) + '/' + DMS_TreeItem_GetName(ti);
end;


type TAscFileNameColumn = class (TAbstrItemColumnProvider)
  m_Frm: TfrmExportASCGrid;
  constructor Create(frm: TfrmExportASCGrid);
  function GetValue(ti: TTreeItem): String; override;
end;

constructor TAscFileNameColumn.Create(frm: TfrmExportASCGrid);
begin
  inherited Create('FileName', 500);
  m_Frm := frm;
end;

function TAscFileNameColumn.GetValue(ti: TTreeItem): String;
begin
  Result := m_Frm.GetFileNameBase(ti)+'.asc';
end;

function TfrmExportASCGrid.ExportListCreate: TfrmTreeItemPickList;
var pl: TfrmTreeItemPickList;

  var visitors: TList;

  procedure ScanItemsImpl(ti: TTreeItem);
  begin
    if SkipTree(ti) then exit;
    if visitors.IndexOf(ti) <> -1 then exit;
    visitors.Add(ti);

    if MustExport(ti) then
      pl.pickList.AddRow(ti, false);

    // recursive search
    ti := DMS_TreeItem_GetFirstVisibleSubItem(ti);
    while Assigned(ti) do
    begin
      ScanItemsImpl( ti );
      ti := DMS_TreeITem_GetNextVisibleItem(ti);
    end;
  end;

  procedure ScanItems(ti: TTreeItem);
  begin
    visitors := TList.Create;
    try
      ScanItemsImpl(ti);
    finally
      visitors.Destroy;
    end;
  end;

begin
  pl := TfrmTreeItemPickList.Create(Self);
  try
    pl.AddColumn(TFullNameColumn.Create);
    pl.AddColumn(TAscFileNameColumn.Create(self));
    pl.Width := 900;
    ScanItems(m_TreeItem);
    pl.Caption := 'Export list';
    pl.btnOK.Visible := false;
    pl.btnCancel.Default := true;
    pl.btnCancel.Caption  := 'OK';
    Result := pl;
    pl := Nil;
  finally
    pl.Free; // clean mess in case of exception in ScanItems
  end;
end;

procedure TfrmExportASCGrid.btnShowExportsClick(Sender: TObject);
var pl: TfrmTreeItemPickList;
begin
  pl := ExportListCreate;
  try
    pl.Execute;
  finally
    pl.Free;
  end
end;


function TfrmExportASCGrid.ExportItem(tiExports, ti: TTreeItem): Boolean;
var
   filename    : FileString;
   expr        : ItemString;
   newti,
   basegrid    : TAbstrDataItem;
   domainUnit,
   valuesUnit  : TAbstrUnit;
begin
   ExportItem := false;

   expr  := TreeItem_GetFullName_Save(ti);

   if not cbxUseBaseGrid.Checked then
      domainUnit := DMS_DataItem_GetDomainUnit(ti)
   else
   begin
      basegrid  := DMS_TreeItem_GetItem(
              DMS_TreeItem_GetRoot(ti),
              PItemChar(ItemString(cobBaseGrid.Text)), DMS_AbstrDataItem_GetStaticClass);
      domainUnit := DMS_DataItem_GetDomainUnit(basegrid);
      expr := 'lookup(' + cobBaseGrid.Text + ', ' + expr + ')';
   end;
   valuesUnit := DMS_DataItem_GetValuesUnit(ti);

   // ============ Set Storage manager
   filename := GetFileNameBase(ti) + '.asc';
   if DMS_TreeItem_StorageDoesExist(tiExports, PFileChar(FileString(filename)), 'asc') then
   begin
       case MessageDlg('Storage: ' + filename + ' already exists!' +
                                 NEW_LINE + 'Delete existing file ?',
                                 mtInformation, [mbYes, mbNo, mbCancel], 0) of
         mrNo     :  begin ExportItem := true; Exit; end;
         mrCancel :  Exit;
         mrYes    :  Deletefile(filename);
      end; // case
   end;

   newti := DMS_CreateDataItem(tiExports, GRIDDATA_NAME, domainUnit, valuesUnit, VC_Single);
   if newti = Nil then
   begin
      MessageDlg('Unable to create new treeitem', mtError, [mbOk], 0);
      Exit;
   end;

   try
     DMS_TreeItem_SetExpr(newti, PItemChar(expr));

     DMS_TreeItem_SetStorageManager(newti, PFileChar(filename), 'asc', false);
     DMS_TreeItem_Update(newti);

     ExportItem := true;
   finally
     DMS_TreeItem_SetAutoDelete(newti);
   end;
end;

procedure TfrmExportASCGrid.btnOKClick(Sender: TObject);
var
   tiExports : TTreeItem;
   pl: TfrmTreeItemPickList;
   i: Integer;
   il: TInterestCountHolder;
begin
   Assert(m_AsAction);

   if Length(edtFileName.Text) = 0 then
   begin
      MessageDlg('Provide a name for the new dir or .ASC grid file', mtError, [mbOk], 0);
      Exit;
   end;

   tiExports := TConfiguration.CurrentConfiguration.DesktopManager.GetNewTempItem;
   pl := Nil;
   il := Nil;
   try
     pl := ExportListCreate;
     pl.MultiSelect := true;
     pl.show;
     pl.Update;

     il := TInterestCountHolder.Create;
     for i := 0 to pl.pickList.lvTreeItems.Items.Count-1 do
        il.SetInterestCount(pl.pickList.lvTreeItems.Items[i].Data);

     for i := 0 to pl.pickList.lvTreeItems.Items.Count-1 do
     begin
       pl.pickList.lvTreeItems.Items[i].Selected := true;
       pl.Update;
       if not ExportItem(tiExports, pl.pickList.lvTreeItems.Items[i].Data) then exit;
       if dmfGeneral.Terminating then exit;
     end;
   finally
     il.Free;
     pl.free;
     DMS_TreeItem_SetAutoDelete(tiExports);
   end;

   ModalResult := mrOK;
end; // btnCreateClick

procedure TfrmExportASCGrid.btnBrowseClick(Sender: TObject);
begin
  if m_AsDir
  then dlgSave.Filter := 'Directory; *.*'
  else dlgSave.Filter := 'Ascii grids|*.asc; *.ASC';

  dlgSave.FileName   := AsWinPath(GetFileNameBase(m_TreeItem));
  DMS_MakeDirsForFile(PFileChar(FileString(dlgSave.FileName)));

  dlgSave.InitialDir := ExtractFilePathEx2(dlgSave.FileName);
  if dlgSave.Execute then
  begin
     edtFileName.Text := AsDmsPath(dlgSave.FileName);
     s_strLastDir := AsDmsPath(ExtractFilePathEx2(FileString(dlgSave.FileName)));
     if (length(s_strLastDir)>0) and (s_strLastDir[length(s_strLastDir)] = '/') then s_strLastDir := copy(s_strLastDir, 1, length(s_strLastDir)-1);
  end;
end; // btnBrowseClick


end.

