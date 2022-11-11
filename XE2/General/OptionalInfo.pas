unit OptionalInfo;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, OleCtrls, ExtCtrls, ComCtrls,
  uDmsInterfaceTypes;

type
  TOptionalInfoIndex =
  (
     OI_Disclaimer,
     OI_SourceDescr,
     OI_CopyRight
  );

  TfrmOptionalInfo = class(TForm)
    cbxHide: TCheckBox;
    btnOK: TButton;
    Panel1: TPanel;
    btnCancel: TButton;
    stnMain: TStatusBar;
    Label1: TLabel;
  private
    { Private declarations }
  public
    { Public declarations }
      class procedure Display(ii: TOptionalInfoIndex; forcedDisplay: Boolean);
      class function Name(ii: TOptionalInfoIndex): FileString;
      class function FileName(ii: TOptionalInfoIndex) : FileString;
      class function Exists(ii: TOptionalInfoIndex): Boolean;
  end;


implementation
uses Configuration, uGenLib, uDmsUtils, dmGeneral, uAppGeneral;
{$R *.DFM}

class function TfrmOptionalInfo.Name(ii: TOptionalInfoIndex): FileString;
begin
  case ii of
    OI_Disclaimer:  Result := 'Disclaimer';
    OI_SourceDescr: Result := 'Source Description';
    OI_CopyRight:   Result := 'Copyright notice';
  end;
end;

class function TfrmOptionalInfo.FileName(ii: TOptionalInfoIndex) : FileString;
var symbPath: PAnsiChar;
begin
  if not TConfiguration.CurrentIsAssigned then
    Result := ''
  else
  begin
    symbPath := '';
    case ii of
      OI_Disclaimer:  symbPath := '%projDir%/Disclaimer.htm';
      OI_SourceDescr: symbPath := '%projDir%/SourceDescr.htm';
      OI_CopyRight:   symbPath := '%projDir%/CopyRight.htm';
    end;
    assert(length(symbPath)<>0);
    Result := GetFullStorageName(symbPath);
  end;
end;

class function TfrmOptionalInfo.Exists(ii: TOptionalInfoIndex): Boolean;
begin
  Result := FileExists(FileName(ii));
end;

class procedure TfrmOptionalInfo.Display(ii: TOptionalInfoIndex; forcedDisplay: Boolean);
var keyName: string;
  url: FileString;
  hideStatus: Boolean;
begin
  keyName := 'Hide'+inttostr(ord(ii));
  hideStatus := TConfiguration.CurrentConfiguration.IniFile.ReadInteger('messages', keyName, 0) <> 0;
  if hideStatus and not forcedDisplay then exit;

  url := FileName(ii);
  if not FileExists(url) then exit;

  StartNewBrowserWindow(url);

  if forcedDisplay and not hideStatus then exit;
  assert(hideStatus = forcedDisplay);

  with TfrmOptionalInfo.Create(Nil) do
  begin
    try
      Caption := Name(ii);
      cbxHide.Checked := hideStatus;
      cbxHide.Caption := stringreplace(cbxHide.Caption, 'this information', Name(ii), []);
      if ShowModal = mrOK then
      begin
        if cbxHide.Checked <> hideStatus then
          TConfiguration.CurrentConfiguration.IniFile.WriteInteger('messages', keyName, IIF(cbxHide.Checked, 1, 0) );
      end;
    except
      Free;
    end;
  end;
end;

end.
