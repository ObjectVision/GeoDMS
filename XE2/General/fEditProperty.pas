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

unit fEditProperty;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  uDmsInterfaceTypes,
  StdCtrls;

type
  TfrmEditProperty = class(TForm)
    edPropertyValue: TEdit;
    btnOk: TButton;
    btnCancel: TButton;
    procedure edPropertyValueKeyUp(Sender: TObject; var Key: Word;
      Shift: TShiftState);
  private
    { Private declarations }
  public
    { Public declarations }
    class function Display(sPropertyName: ItemString; var sPropertyValue: SourceString; bReadOnly: boolean = false): boolean;
  end;


implementation

{$R *.DFM}

uses uDmsUtils, dmGeneral;

class function TfrmEditProperty.Display(sPropertyName: ItemString; var sPropertyValue: SourceString; bReadOnly: boolean): boolean;
var instance: TfrmEditProperty;
begin
  instance := TfrmEditProperty.Create(Application);
  with instance do
  try
      Caption := sPropertyName;
      edPropertyValue.Text := sPropertyValue;
      btnOK.Visible := not bReadOnly;
      if(bReadOnly)
      then btnCancel.Caption := 'Close';

      edPropertyValue.ReadOnly := bReadOnly;

      dmfGeneral.RegisterCurrForm(instance);
      try
        Result := ShowModal = mrOK;
      finally
        dmfGeneral.UnRegisterCurrForm(instance);
      end;

      if Result then sPropertyValue := edPropertyValue.Text;

  finally
    Free;
  end;
end;

procedure TfrmEditProperty.edPropertyValueKeyUp(Sender: TObject;
  var Key: Word; Shift: TShiftState);
begin
  if(Key = VK_RETURN)
  then
    ModalResult := mrOK
  else if(Key = VK_ESCAPE)
  then
    ModalResult := mrCancel;
end;

end.
