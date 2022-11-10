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

unit fExportBitmap;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ComCtrls, StdCtrls, ExtCtrls, Buttons, uDMSInterfaceTypes;

type
  TExpBmpInfo = record
    bResult : boolean;
    sBmpFile : string;
    tiRelated : TTreeItem;
    tiClassification : TTreeItem;
    tiPalette : TTreeItem;
    tiExpInfo : TTreeItem;
  end;

  TfrmExportBitmap = class(TForm)
    btnBrowse: TButton;
    dlgSave: TSaveDialog;
    Panel1: TPanel;
    edFileName: TEdit;
    Label1: TLabel;
    cbxBasisGrid: TComboBox;
    Label2: TLabel;
    cbxClassification: TComboBox;
    Label3: TLabel;
    Label4: TLabel;
    cbxPalette: TComboBox;
    btnOK: TButton;
    btnCancel: TButton;
    btnHelp: TButton;
    stnMain: TStatusBar;
    procedure FormActivate(Sender: TObject);
    procedure btnBrowseClick(Sender: TObject);
    procedure cbxClassificationChange(Sender: TObject);

   function  GetSelectedBaseGrid: TAbstrDataItem;
   function  GetSelectedClassification: TAbstrDataItem;
   function  GetSelectedPalette: TAbstrDataItem;
   function  GetValuesUnit: TAbstrUnit;
   function  GetBmpValuesUnit: TAbstrUnit;
   procedure btnOKClick(Sender: TObject);

  private
      m_tiExpInfo : TTreeItem;
      m_tiLayer   : TTreeItem;
      m_TreeItem  : TTreeItem;
      constructor    CreateForm(const ti, tiLayer: TTreeItem);
      function       DisplayForm: TExpBmpInfo;
  public
      class function ShowForm(const ti: TTreeItem; tiLayer: TTreeItem = nil; sCaption: string = 'Export Bitmap'): TExpBmpInfo;
  end;


implementation

{$R *.DFM}

uses
   uGenLib,
   uDMSUtils,
   uDmsInterface,
   fMain,
   uDMSTreeviewFunctions,
   Configuration;

const NO_CLASSIFICATION_NAME = '<DON''T USE CLASSIFICATION>';
const NO_PALETTE_NAME        = '<DON''T USE PALETTE>';

class function TfrmExportBitmap.ShowForm(const ti: TTreeItem; tiLayer: TTreeItem; sCaption: string): TExpBmpInfo;
begin
   with TfrmExportBitmap.CreateForm(ti, tiLayer) do
   begin
      Caption := sCaption;
      Result := DisplayForm;
      Free;
   end;
end;

function TfrmExportBitmap.DisplayForm: TExpBmpInfo;
begin
  Result.bResult := ShowModal <> mrCancel;

  Result.sBmpFile  := edFileName.Text;
  Result.tiRelated := GetSelectedBaseGrid;
  Result.tiClassification := GetSelectedClassification;
  Result.tiPalette := GetSelectedPalette;
  Result.tiExpInfo := m_tiExpInfo;
end;

constructor TfrmExportBitmap.CreateForm(const ti, tiLayer: TTreeItem);
begin
   m_TreeItem := ti;
   m_tiLayer := tiLayer;

   inherited   Create(Application);
end; // CreateForm

procedure TfrmExportBitmap.FormActivate(Sender: TObject); // REMOVE, move to FormCreate and test
var
  d: TAbstrUnit;
begin
   edFileName.Text   :=  TConfiguration.CurrentConfiguration.GetResultsPath + '/' + DMS_TreeItem_GetName(m_TreeItem) + '.tif';

   d := DMS_DataItem_GetDomainUnit(m_TreeItem);

   cbxBasisGrid.Items.Clear;
   FillCbxBaseGrids(cbxBasisGrid, d);
   if cbxBasisGrid.Items.Count = 0 then
   begin
      btnOK.Enabled := False;
      cbxBasisGrid.Items.Add('No appropriate base grid found for entity '+DMS_TreeItem_GetName(d));
      cbxBasisGrid.ItemIndex := 0;
      Exit;
   end;

   FillCbxClassifications(cbxClassification, m_TreeItem, NO_CLASSIFICATION_NAME, true);
   if cbxClassification.Items.Count = 0 then
   begin
      btnOK.Enabled := False;
      cbxClassification.Items.Add('No appropriate classification found. Create a classification first or save as .TIF storage.');
      cbxClassification.ItemIndex := 0;
      Exit;
   end;

   cbxClassificationChange(Sender);
end;

function GetObject(var cbx: TComboBox): TTreeItem;
var i: Integer;
begin
  Result := Nil;
  if cbx.Text = '' then exit;
  i := cbx.ItemIndex;
  if i < 0 then exit;
  Result := cbx.Items.Objects[i];
end;

function TfrmExportBitmap.GetSelectedBaseGrid: TAbstrDataItem;
begin
  result := GetObject(cbxBasisGrid);
end;

function TfrmExportBitmap.GetSelectedClassification: TAbstrDataItem;
begin
  result := GetObject(cbxClassification);
end;

function TfrmExportBitmap.GetSelectedPalette: TAbstrDataItem;
begin
  result := GetObject(cbxPalette);
end;

function TfrmExportBitmap.GetValuesUnit: TAbstrUnit;
var classification: TAbstrDataItem;
begin
   classification := GetSelectedClassification;
   if Assigned(classification) then
      Result := DMS_DataItem_GetDomainUnit(classification)
   else
      Result := DMS_DataItem_GetValuesUnit(m_TreeItem);
end;

function TfrmExportBitmap.GetBmpValuesUnit: TAbstrUnit;
begin
   Result := DMS_TreeItem_QueryInterface(GetValuesUnit, DMS_UInt8Unit_GetStaticClass);
end;

procedure TfrmExportBitmap.btnOKClick(Sender: TObject);
var
   filename,
   filenameext : FileString;
   expr        : ItemString;

   basegrid,
   griddata,
   palettedata,
   classification,
   palette     : TAbstrDataItem;
   viewData    : CPropDef;
   domainUnit,
   valuesUnit  : TAbstrUnit;
begin
   filename := AsDmsPath(edFileName.Text);
   if Length(filename) = 0 then
   begin
      MessageDlg('Give a filename', mtError, [mbOk], 0);
      Exit;
   end;

   filenameext := ExtractFileExtEx2(filename);
   if Length(filenameext) = 0 then
   begin
      filename    := ChangeFileExt(filename, '.tif');
      filenameext := '.tif';
   end;
   filenameext := Copy(filenameext, 2, Length(filenameext) - 1); // remove '.' from storagetype
   if (UpperCase(filenameext) <> 'BMP') and (UpperCase(filenameext) <> 'TIF') then
   begin
      MessageDlg('Filename must have a .tif or .bmp extension', mtError, [mbOk], 0);
      Exit;
   end;

   valuesUnit := GetValuesUnit();
   palette := GetSelectedPalette();

   if (UpperCase(filenameext) = 'BMP') then
   begin

     if not Assigned(GetBmpValuesUnit()) then
     begin
        MessageDlg('For .BMP storages, data values should be classified to UInt8 numbers for storage in 256 color bitmap'+#13+#10+
         'Select a classification (or create a new one first with Insert New Classification) or consider saving the unclassified data as .TIF file', mtError, [mbOk], 0);
        Exit;
     end;
     if not Assigned(palette) then
     begin
        MessageDlg('256 color bitmaps require a color palette for the selected (classified) data'+#13+#10+
         'Select a palette or create a new one with Insert New Classification or consider saving the unclassified data as .TIF file', mtError, [mbOk], 0);
        Exit;
     end;
   end;

   if(m_tiLayer = nil)
   then
     m_tiExpInfo := TConfiguration.CurrentConfiguration.DesktopManager.GetNewTempItem
   else
     m_tiExpInfo := DMS_CreateTreeItem(m_tiLayer, 'bitmap');

   if m_tiExpInfo = Nil then
   begin
      MessageDlg('Unable to create new treeitem', mtError, [mbOk], 0);
      Exit;
   end;

   try // guarantee that m_tiExpInfo is AutoDeleted if it is a temporary ie if not Assigned(m_tiLayer)
     expr     := TreeItem_GetFullName_Save(m_TreeItem);

     classification := GetSelectedClassification;
     if Assigned(classification) then
        expr := 'classify(' + expr + ', ' + TreeItem_GetFullName_Save(classification) + ')';

     baseGrid := GetObject(cbxBasisGrid);
     if not Assigned(baseGrid) then
        domainUnit := DMS_DataItem_GetDomainUnit(m_TreeItem)
     else
     begin
        domainUnit := DMS_DataItem_GetDomainUnit(basegrid);
        expr := 'lookup(' + cbxBasisGrid.Text + ', ' + expr + ')';
     end;

     griddata := DMS_CreateDataItem(m_tiExpInfo, GRIDDATA_NAME, domainUnit, valuesUnit, VC_Single);
     DMS_TreeItem_SetExpr(griddata, ''); // addition MTA: make shure that griddata is invalidated, to make it be updated
     DMS_TreeItem_SetExpr(griddata, PItemChar(expr));

     if Assigned(palette) then
     begin
        palettedata := DMS_CreateDataItem( m_tiExpInfo, PALETTEDATA_NAME,
                                           valuesUnit, DMS_DataItem_GetValuesUnit(palette), VC_Single);
        DMS_TreeItem_SetExpr(palettedata, PItemChar('')); // addition MTA: make shure that palettedata is invalidated, to make it be updated
        DMS_TreeItem_SetExpr(palettedata, PItemChar(TreeItem_GetFullName_Save(palette))); // BREEKPUNT HIER!
     end;

     if DMS_TreeItem_StorageDoesExist(m_tiExpInfo, PFileChar(filename), PItemChar(filenameext)) then
     begin
         case MessageDlg('Storage: ' + filename + ' already exists!' +
                                   NEW_LINE + 'Delete existing file ?',
                                   mtInformation, [mbYes, mbNo, mbCancel], 0) of
           mrYes    :  Deletefile(filename);
           mrCancel :  Exit;
           mrNo     :  ;
        end; // case
     end;

     DMS_TreeItem_SetStorageManager(m_tiExpInfo, PFileChar(filename), PItemChar(filenameext), false);

     viewData   := DMS_Class_FindPropDef(DMS_TreeItem_GetStaticClass, VIEWDATA_NAME);
     DMS_PropDef_SetValueAsCharArray(viewData,   m_tiExpInfo, PSourceChar(filename) );

     DMS_TreeItem_Update(griddata);

     ModalResult := mrOK;
   finally
     if(m_tiLayer = nil)
     then DMS_TreeItem_SetAutoDelete(m_tiExpInfo);
   end
end; // btnCreateClick

procedure TfrmExportBitmap.btnBrowseClick(Sender: TObject);
var   curdir : string;
begin
   curdir   := GetCurrentDir;
   try
     dlgSave.FileName   := AsWinPath(edFileName.Text);
     DMS_MakeDirsForFile(PFileChar(FileString(dlgSave.FileName)));
     dlgSave.InitialDir := ExtractFilePathEx2(dlgSave.FileName);
     if dlgSave.Execute then
        edFileName.Text := AsDmsPath(dlgSave.FileName);
   finally
     SetCurrentDir(curdir);
   end;
end; // btnBrowseClick

procedure TfrmExportBitmap.cbxClassificationChange(Sender: TObject);
var valuesUnit: TAbstrUnit;
begin
   cbxPalette.Items.Clear;
   valuesUnit := GetValuesUnit;
   if Assigned(valuesUnit) then
     FillCbxPalettes(cbxPalette, valuesUnit, NO_PALETTE_NAME);
   if cbxPalette.Items.Count = 0 then
   begin
      cbxPalette.Items.Add('No appropriate palette found for this classification. Select another classification or create a new one for the exported data item.');
      cbxPalette.ItemIndex := 0;
   end
end;

end.


