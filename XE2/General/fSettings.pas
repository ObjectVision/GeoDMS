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

unit fSettings;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  uDMSInterfaceTypes,
  ComCtrls, StdCtrls, {IniFiles, } ExtCtrls, Buttons;

type
  TfrmSettings = class(TForm)
    PageControl1: TPageControl;
    tsAdvanced: TTabSheet;
    btnCancel: TButton;
    btnOk: TButton;
    lblExternalPrograms: TLabel;
    Bevel1: TBevel;
    Bevel2: TBevel;
    btnApply: TButton;
    lblDMSEditor: TLabel;
    edDMSeditor: TEdit;
    btnDMSEditor: TSpeedButton;
    Bevel3: TBevel;
    tsGUI: TTabSheet;
    lblMapviewSettings: TLabel;
    stBackgroundColor: TEdit;
    chbSuspendForGUI: TCheckBox;  // tree_item view color settings
    Label2: TLabel;
    Label3: TLabel;
    Label4: TLabel;
    Label5: TLabel; // valid color
    Label7: TLabel;   // invalid color
    Label8: TLabel;
    Label9: TLabel;
    Label10: TLabel;
    Label11: TLabel;
    stValidColor: TEdit;
    stInvalidatedColor: TEdit;
    stFailedColor: TEdit;
    stRampStart: TEdit;
    stRampEnd: TEdit;
    Label12: TLabel;
    chbTraceLogFile: TCheckBox;
    Label14: TLabel;
    Label15: TLabel;
    edLocalDataDir: TEdit;
    edSourceDataDir: TEdit;
    Label16: TLabel;
    StatusBar1: TStatusBar;
    chbMultiThreading: TCheckBox;
    chbMT2: TCheckBox;
    chbMT3: TCheckBox;
    Label17: TLabel;
    Label18: TLabel;
    tsConfigSettings: TTabSheet;
    Label13: TLabel;
    txtSwapfileMinSize: TEdit;
    bytes: TLabel;
    Label19: TLabel;
    txtFlushThreshold: TEdit;
    percent: TLabel;
    chbShowStateColors: TCheckBox;
    chbShowThousandSeparator: TCheckBox;
    chbAdminMode: TCheckBox;

    procedure FormCreate(Sender: TObject);
    procedure btnDMSEditorClick(Sender: TObject);
    procedure btnOkClick(Sender: TObject);
    procedure btnApplyClick(Sender: TObject);
    procedure SettingChange(Sender: TObject);
    procedure ConfigSettingChange(Sender: TObject);
    procedure ConfigSettingReset (Sender: TObject);
    procedure UpdateVisibility;
    procedure stColorClick(Sender: TObject);

    procedure chbAdminModeClick(Sender: TObject);
    function BuildConfigSettings: Boolean;

  private
    m_bConfigSettingsChanged: Boolean;

    m_Tags: array of TAbstrDataItem;
    function  GetTag(i: Cardinal): TAbstrDataItem;
    function MakeTag(v: TAbstrDataItem): Cardinal;

    procedure ApplyConfigChanges;
    procedure ApplyChanges;
    function  FindEdit (tiCS: TTreeItem): TEdit;
    function  FindReset(tiCS: TTreeItem): TButton;
  public
    class procedure Display;
  end;

implementation

{$R *.DFM}

uses
  uAppGeneral, Configuration, fMain, dmGeneral, uGenLib, uDmsUtils, uDmsRegistry,
  uDmsInterface;

class procedure TfrmSettings.Display;
begin
  with TfrmSettings.Create(Application) do
  begin
    ShowModal;
    Free;
  end;
end;

const clCfgSetting: TColor = clAqua;
const clRegSetting: TColor = clOlive;


function  TfrmSettings.GetTag(i: Cardinal): TAbstrDataItem;
begin
   if i=0 then Result := nil
   else
   begin
     assert(i <= length(m_Tags));
     Result := m_Tags[i-1];
   end;
end;

function TfrmSettings.MakeTag(v: TAbstrDataItem): Cardinal;
var i: Cardinal;
begin
   Result := length(m_Tags);
   if Result <> 0 then
     for i := 0 to Result-1 do
       if m_Tags[i] = v then
       begin
         result := i+1;
         exit;
       end;
   SetLength(m_Tags, Result+1);
   m_Tags[Result] := v;
   Inc(Result);
end;

function TfrmSettings.BuildConfigSettings: Boolean;
var tiOverridableConfigSettings, tiCS: TTreeItem;
    nrRows: Integer;
    lblName: TLabel;
    edtValue: TEdit;
    btnReset: TButton;
    openedKey: Boolean;
begin
  assert(TConfiguration.CurrentIsAssigned);
  Result := false;

  tiOverridableConfigSettings := TConfiguration.CurrentConfiguration.ConfigSettings;
  if not Assigned(tiOverridableConfigSettings) then exit; // easy way out

  tiOverridableConfigSettings := DMS_TreeItem_GetItem(tiOverridableConfigSettings, 'Overridable', nil);
  if not Assigned(tiOverridableConfigSettings) then exit; // easy way out

  tiCS := DMS_TreeItem_GetFirstSubItem(tiOverridableConfigSettings);
  if not Assigned(tiCS) then exit; // easy way out


  with TDmsRegistry.Create do
  try
    openedKey := OpenKeyReadOnly('');
    try

      nrRows := 0;
      while Assigned(tiCS) do
      begin
        if  Assigned(DMS_TreeItem_QueryInterface(tiCS, DMS_AbstrDataItem_GetStaticClass))
        and Assigned(DMS_TreeItem_QueryInterface(DMS_DataItem_GetValuesUnit(tiCS), DMS_StringUnit_GetStaticClass))
        then
        begin
           lblName := TLabel.Create(tsConfigSettings);
           lblName.Caption := string( DMS_TreeItem_GetName(tiCS) );
           lblName.Parent := tsConfigSettings;
           lblName.Left   :=   8;
           lblName.Width  :=  88;
           lblName.Height :=  14;
           lblName.Top    :=  nrRows * 32 + 35;
           lblName.ParentFont := true;
           lblName.Anchors    := [akLeft,akTop];
           lblName.Hint       := string( DMS_TreeItem_GetName(tiCS) );
           lblName.ShowHint   := true;
           lblName.Show;

           edtValue     := TEdit.Create(tsConfigSettings);
           edtValue.Tag := MakeTag(tiCS);

           if openedKey and ValueExists(string( DMS_TreeItem_GetName(tiCS) )) then
           begin
              edtValue.Text  := ReadString(string( DMS_TreeItem_GetName(tiCS) ));
              edtValue.Color := clRegSetting;
           end
           else
           begin
              edtValue.Text := AnyParam_GetValueAsString(tiCS);
              edtValue.Color := clCfgSetting;
           end;
           edtValue.Parent := tsConfigSettings;
           edtValue.Left   :=  104;
           edtValue.Width  :=  tsConfigSettings.Width - 104 - 32;
           edtValue.Height :=  22;
           edtValue.Top    :=  nrRows * 32 + 32;
           edtValue.ParentFont := true;
           edtValue.Anchors := [akLeft, akRight,akTop];
           edtValue.Hint := AnyParam_GetValueAsString(tiCS);
           edtValue.OnChange := ConfigSettingChange;
           edtValue.Show;

           btnReset := TButton.Create(tsConfigSettings );
           btnReset.Tag := edtValue.Tag;
           btnReset.Parent := tsConfigSettings;
           btnReset.Left   :=  tsConfigSettings.Width - 28;
           btnReset.Width  :=  48;
           btnReset.Height :=  16;
           btnReset.Top    :=  nrRows * 32 + 35;
           btnReset.ParentFont := true;
           btnReset.Anchors := [akRight,akTop];
           btnReset.Caption := 'Reset';
           btnReset.Hint    := 'Reset to the configured value: ' + AnyParam_GetValueAsString(tiCS);
           btnReset.Enabled := (edtValue.Color = clRegSetting);
           btnReset.OnClick := ConfigSettingReset;
           btnReset.Show;

           INC(nrRows);
           Result := true;
        end;
        tiCS := DMS_TreeItem_GetNextItem(tiCS);
      end
    finally
      if openedKey then CloseKey;
    end
  finally
    Free;
  end;
end;

procedure TfrmSettings.ApplyConfigChanges;
var tiOverridableConfigSettings, tiCS: TTreeItem;
    edtValue: TEdit;
begin
  tiOverridableConfigSettings := TConfiguration.CurrentConfiguration.ConfigSettings;
  if not Assigned(tiOverridableConfigSettings) then exit; // easy way out

  tiOverridableConfigSettings := DMS_TreeItem_GetItem(tiOverridableConfigSettings, 'Overridable', nil);
  if not Assigned(tiOverridableConfigSettings) then exit; // easy way out

  tiCS := DMS_TreeItem_GetFirstSubItem(tiOverridableConfigSettings);
  if not Assigned(tiCS) then exit; // easy way out

  with TDmsRegistry.Create do
  try
    if OpenKey('')
    then try
      while Assigned(tiCS) do
      begin
         edtValue := FindEdit(tiCS);
         if Assigned(edtValue) then
         begin
           if edtValue.Color = clRegSetting
           then WriteString(string( DMS_TreeItem_GetName(tiCS) ), edtValue.Text)
           else DeleteValue(string( DMS_TreeItem_GetName(tiCS) ));
         end;
         tiCS := DMS_TreeItem_GetNextItem(tiCS);
      end;
    finally
      CloseKey;
    end
  finally
    Free;
  end;
end;

procedure TfrmSettings.FormCreate(Sender: TObject);
var hasConfigSettings: Boolean;
    cfg: TConfiguration;
begin
  SetLength(m_Tags, 0);
  with dmfGeneral do
  begin
    chbAdminMode.Checked       := AdminMode;
    chbShowStateColors.Checked := GetStatus(SF_ShowStateColors);
    chbSuspendForGUI.Checked   := GetStatus(SF_SuspendForGUI  );
    chbTraceLogFile.Checked    := GetStatus(SF_TraceLogFile   );
    chbMultiThreading.Checked  := GetStatus(SF_MultiThreading1);
    chbMT2.Checked             := GetStatus(SF_MultiThreading2);
    chbMT3.Checked             := GetStatus(SF_MultiThreading3);
    chbShowThousandSeparator.Checked :=  GetStatus(SF_ShowThousandSeparator);

    edLocalDataDir.Text        := GetString(SRI_LocalDataDir);
    edSourceDataDir.Text       := GetString(SRI_SourceDataDir);
    edDmsEditor.Text           := GetString(SRI_DmsEditor  );
  end;
  UpdateVisibility;

  tsGUI.TabVisible := true;
  tsAdvanced.TabVisible := dmfGeneral.AdminMode;
  hasConfigSettings := TConfiguration.CurrentIsAssigned;
  tsConfigSettings.TabVisible := hasConfigSettings;
  if hasConfigSettings then
  begin
    cfg := TConfiguration.CurrentConfiguration;
    stBackgroundColor.Color := cfg.clrBackground;
    stValidColor.Color      := cfg.clrValid;
    stInvalidatedColor.Color:= cfg.clrInvalidated;
    stFailedColor.Color     := cfg.clrFailed;
    stRampStart.Color       := cfg.clrRampStart;
    stRampEnd.Color         := cfg.clrRampEnd;
    txtSwapfileMinSize.Text := IntToStr(cfg.SwapFileMinSize);
    txtFlushThreshold.text  := IntToStr(cfg.FlushThreshold);
    hasConfigSettings := BuildConfigSettings;
  end;

  if hasConfigSettings
  then PageControl1.ActivePage := tsConfigSettings
  else PageControl1.ActivePage := tsGUI;

  btnOk.Enabled            := true;
  btnCancel.Enabled        := false;
  btnApply.Enabled         := false;
  m_bConfigSettingsChanged := false;
end;

procedure TfrmSettings.btnOkClick(Sender: TObject);
begin
  ApplyChanges;
  ModalResult := mrOK;
end;

procedure TfrmSettings.btnApplyClick(Sender: TObject);
begin
  ApplyChanges;
end;

procedure TfrmSettings.btnDMSEditorClick(Sender: TObject);
begin
  with TOpenDialog.Create(nil) do
  begin
    Options := Options + [ofNoChangeDir];
    Title := 'Select editor...';
    DefaultExt := 'exe';
    Filter := 'Executables (*.exe)|*.exe';
    FilterIndex := 1;

    if(Execute)
    then begin
      edDMSeditor.Text := Filename;
    end;
    Free;
  end;
end;

procedure TfrmSettings.ApplyChanges;
begin
  with dmfGeneral do
  begin
    SetStatus(SF_AdminMode      , chbAdminMode.Checked);
    SetStatus(SF_TraceLogFile   , chbTraceLogFile.Checked);
    SetStatus(SF_ShowStateColors, chbShowStateColors.Checked);
    SetStatus(SF_SuspendForGUI  , chbSuspendForGUI.Checked);
    SetStatus(SF_MultiThreading1, chbMultiThreading.Checked);
    SetStatus(SF_MultiThreading2, chbMT2.Checked);
    SetStatus(SF_MultiThreading3, chbMT3.Checked);
    SetStatus(SF_ShowThousandSeparator, chbShowThousandSeparator.Checked);

    SetString(SRI_DmsEditor     , edDmsEditor .Text);
    SetString(SRI_LocalDataDir  , edLocalDataDir.Text);
    SetString(SRI_SourceDataDir , edSourceDataDir.Text);

    WriteDmsRegistry;
  end;

  if(TConfiguration.CurrentIsAssigned)
  then begin
    with TConfiguration.CurrentConfiguration do
    begin
      clrBackground  := stBackgroundColor.Color;
      clrValid       := stValidColor.Color;
      clrInvalidated := stInvalidatedColor.Color;
      clrFailed      := stFailedColor.Color;
      clrRampStart   := stRampStart.Color;
      clrRampEnd     := stRampEnd.Color;
      SwapFileMinSize:= StrToInt(txtSwapFileMinSize.text);
      FlushThreshold := StrToInt(txtFlushThreshold.text);
    end;
  end;
  frmMain.UpdateCaption;

  if m_bConfigSettingsChanged then
    ApplyConfigChanges; // commit to registry
  btnCancel.Enabled := false;
  btnApply.Enabled := false;
end;

procedure TfrmSettings.SettingChange(Sender: TObject);
begin
  btnOk.Enabled := IsInteger(txtSwapfileMinSize.Text) AND (StrToInt(txtSwapfileMinSize.Text) >= 0);
  if not btnOk.Enabled then
    btnOK.Hint := 'Configuration.MinSwapfileSize must be a nonnegative integer'
  else
  begin
    btnOk.Enabled := IsInteger(txtFlushThreshold.Text) AND (StrToInt(txtFlushThreshold.Text) >= 10) AND (StrToInt(txtFlushThreshold.Text) <= 100);
    if not btnOk.Enabled then
      btnOK.Hint := 'ConfigurationMemoryFlushThreshold must be an integer between 10 and 100'
    else
      btnOK.Hint := ''
  end;

  btnApply.Enabled := btnOk.Enabled;
  btnApply.Hint    := btnOK.Hint;
  btnCancel.Enabled:= true;
end;

procedure TfrmSettings.ConfigSettingChange(Sender: TObject);
var tiCS: TTreeItem;
begin
   assert(Assigned(Sender as TEdit));
   assert(TEdit(Sender).Tag <> 0);
   tiCS := GetTag(TEdit(Sender).Tag);
   assert(Assigned(tiCS));
   if TEdit(Sender).Color = clCfgSetting then
   begin
       if TEdit(Sender).Text = AnyParam_GetValueAsString(tiCS) then exit; // easy way out
     TEdit(Sender).Color := clRegSetting;
     FindReset(tiCS).Enabled := True;
   end;
   assert(TEdit(Sender).Color = clRegSetting);
   assert(FindReset(tiCS).Enabled);
   m_bConfigSettingsChanged := True;
   SettingChange(Sender);
end;

procedure TfrmSettings.ConfigSettingReset(Sender: TObject);
var tiCS: TTreeItem; edtConfig: TEdit;
begin
   assert(Assigned(Sender as TButton));
   assert(TButton(Sender).Tag <> 0);
   tiCS := GetTag(TButton(Sender).Tag);
   assert(Assigned(tiCS));
   assert(TButton(Sender).Enabled);
   edtConfig := FindEdit(tiCS);
   assert(edtConfig.Color = clRegSetting);
   edtConfig.Color := clCfgSetting;
   edtConfig.Text  := AnyParam_GetValueAsString(tiCS);
   TButton(Sender).Enabled := false;

   m_bConfigSettingsChanged := True;
   SettingChange(Sender);
end;

procedure TfrmSettings.UpdateVisibility;
var adminMode: Boolean;
begin
  adminMode:= chbAdminMode.Checked;
  tsAdvanced.TabVisible := adminMode;
end;

procedure TfrmSettings.stColorClick(Sender: TObject);
begin
  Assert(Sender is TEdit);
  with TheColorDialog do
  begin
    Color := TEdit(Sender).Color;
    if(Execute and (TEdit(Sender).Color <> Color))
    then begin
      TEdit(Sender).Color := Color;
      SettingChange(Sender);
    end;
  end;
end;

procedure TfrmSettings.chbAdminModeClick(Sender: TObject);
begin
  UpdateVisibility;
  SettingChange(Sender);
end;

function TfrmSettings.FindEdit(tiCS: TTreeItem): TEdit;
var i: Integer; child: TControl;
begin
  Result := Nil;
  for i := 0 to tsConfigSettings.ControlCount - 1 do
  begin
    child := tsConfigSettings.Controls[i];
    if (child is TEdit) and (GetTag(TEdit(child).Tag) = tiCS) then
    begin
      Result := TEdit(child);
      exit;
    end;
  end;
end;

function TfrmSettings.FindReset(tiCS: TTreeItem): TButton;
var i: Integer; child: TControl;
begin
  Result := Nil;
  for i := 0 to tsConfigSettings.ControlCount - 1 do
  begin
    child := tsConfigSettings.Controls[i];
    if (child is TButton) and (GetTag(TButton(child).Tag) = tiCS) then
    begin
      Result := TButton(child);
      exit;
    end;
  end;
end;

end.
