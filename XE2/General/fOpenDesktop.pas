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

unit fOpenDesktop;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ComCtrls, ExtCtrls, uDMSInterfaceTypes, Buttons;

type
  TfrmOpenDesktop = class(TForm)
    pnlMain: TPanel;
    lvDesktops: TListView;
    btnOK: TButton;
    btnCancel: TButton;
    btnHelp: TButton;
    stnMain: TStatusBar;
    procedure lvDesktopsSelectItem(Sender: TObject; Item: TListItem;
      Selected: Boolean);
    procedure lvDesktopsDblClick(Sender: TObject);
  private
    { Private declarations }
    procedure Init(tiContainer: TTreeItem);
  public
    { Public declarations }
    class function Display(tiContainer: TTreeItem; const sCaption: string = 'Open Desktop'): TTreeItem;
  end;

implementation

{$R *.DFM}

uses
  uDMSUtils,
  uDmsInterface,
  dmGeneral;

class function TfrmOpenDesktop.Display(tiContainer: TTreeItem; const sCaption: string): TTreeItem;
begin
  Result := nil;
  with TfrmOpenDesktop.Create(Application) do
  begin
    Caption := sCaption;
    Init(tiContainer);

    if(ShowModal = mrOK)
    then Result := lvDesktops.Selected.Data;

    Free;
  end;
end;

procedure TfrmOpenDesktop.Init(tiContainer: TTreeItem);
var
  liNew : TListItem;
begin
  tiContainer := DMS_TreeItem_GetFirstVisibleSubItem(tiContainer); //Desktops -> first Desktop
  while Assigned(tiContainer) do
  begin
    if (dmfGeneral.AdminMode or not DMS_TreeItem_IsHidden(tiContainer))
    then begin
      liNew := lvDesktops.Items.Add;
      liNew.Caption := TreeItemDescrOrName(tiContainer);
      liNew.Data    := tiContainer;
    end;
    tiContainer := DMS_TreeItem_GetNextVisibleItem(tiContainer);
  end;
end;

procedure TfrmOpenDesktop.lvDesktopsSelectItem(Sender: TObject;
  Item: TListItem; Selected: Boolean);
begin
  btnOK.Enabled := Selected;
end;

procedure TfrmOpenDesktop.lvDesktopsDblClick(Sender: TObject);
begin
  if(lvDesktops.SelCount = 1)
  then ModalResult := mrOK;
end;

end.
