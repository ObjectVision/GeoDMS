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

unit fHistogramParameters;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ComCtrls, StdCtrls, ExtCtrls, Buttons, uDMSInterfaceTypes, fmHistogramControl;

type
  TfrmHistogramParameters = class(TForm)
    btnOK: TButton;
    dlgSave: TSaveDialog;
    Panel1: TPanel;
    cbxFilter: TComboBox;
    lblFilter: TLabel;
    cbxClassification: TComboBox;
    Label3: TLabel;
    lblSelection: TLabel;
    cbxSelection: TComboBox;
    btnCancel: TButton;
    stnMain: TStatusBar;
    procedure FormActivate(Sender: TObject);
  private
      m_ThematicAttr          : TTreeItem;
      constructor       CreateForm(const ti: TTreeItem);
  public
      class function    ShowForm(const ti: TTreeItem; var hp: THistogramParameters): Boolean;
  end;


implementation

{$R *.DFM}

uses
   uGenLib,
   uDMSUtils,
   uDmsInterface,
   uDMSTreeviewFunctions,
   fMain, Configuration;

const
   NO_CLASSIFICATION_NAME = '<DON''T USE CLASSIFICATION>';

class function TfrmHistogramParameters.ShowForm(const ti: TTreeItem; var hp: THistogramParameters): Boolean;
begin
   with TfrmHistogramParameters.CreateForm(ti) do
   begin
      with hp do
      begin
         tiClassification  := nil;
         tiFilter          := nil;
         tiSelection       := nil;
      end;

      Result   := ShowModal = mrOK;

      if Result then
      begin
         with hp do
         begin
            if cbxClassification.Text <> NO_CLASSIFICATION_NAME then
               tiClassification  := DMS_TreeItem_GetItem(
                  TConfiguration.CurrentConfiguration.Root,
                  PItemChar(ItemString(cbxClassification.Text)),
                  nil
               );
            tiFilter          := tiClassification;
         end;
      end;

      Free;
   end;
end;

constructor TfrmHistogramParameters.CreateForm(const ti: TTreeItem);
begin
   m_ThematicAttr    := ti;
   inherited   Create(Application);
end; // CreateForm

procedure TfrmHistogramParameters.FormActivate(Sender: TObject); // REMOVE, move code to CreateForm and test
var   v  :  TAbstrUnit;
begin
   v  := DMS_DataItem_GetValuesUnit(m_ThematicAttr);

   FillCbxClassifications(cbxClassification, m_ThematicAttr, NO_CLASSIFICATION_NAME, false);
   if cbxClassification.Items.Count = 0 then // here NO_CLASSIFICATION_NAME also counts
   begin
      MessageDlg('Internal Error; no appropiate classification found for valueUnit ' + DMS_TreeItem_GetName(v), mtError, [mbOK], 0);
      Exit;
   end;

end; // FormActivate



end.


