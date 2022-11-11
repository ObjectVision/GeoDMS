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

unit fMdiChild;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ExtCtrls, uDMSInterfaceTypes;

type
  TfrmMdiChild = class(TForm)
    procedure FormCreate(Sender: TObject);
    procedure FormActivate(Sender: TObject);
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
  private
    { Private declarations }
    procedure WMWindowPosChanging(var msg: TWMWindowPosChanging); message WM_WINDOWPOSCHANGING;
  private
    m_pnlToolbar : TPanel;

  protected
    procedure SetMdiToolBar(pnlToolBar: TPanel);
  public
    { Public declarations }
    function InitFromDesktop(tiView: TTreeItem): boolean; virtual;
    procedure StoreDesktopData; virtual;
  end;


implementation

uses
  fMain, uAppGeneral;

{$R *.DFM}
const BORDER_SIZE = 4;
const TITLEBAR_SIZE = 20; // exlcuding top-border size (including = 24)

procedure TfrmMdiChild.FormClose(Sender: TObject;
  var Action: TCloseAction);
begin
  Action := caFree;
  frmMain.SetMdiToolbar(nil);
end;

procedure TfrmMdiChild.FormActivate(Sender: TObject);
begin
  if assigned(m_pnlToolbar) then
     frmMain.SetMdiToolbar(m_pnlToolbar);
end;

procedure TfrmMdiChild.SetMdiToolBar(pnlToolBar: TPanel);
begin
  m_pnlToolbar := pnlToolbar;
  if Active then frmMain.SetMdiToolbar(m_pnlToolbar);
end;

procedure TfrmMdiChild.FormCreate(Sender: TObject);
begin
  if(Application.Mainform.MDIChildCount = 1)
  then
    WindowState := wsMaximized;
end;

// cx = width
// left & right are the acceptable bounds
Procedure Fit(var x, cx: integer; left, right: Integer);
begin
  if left        > right then right := left;         // now left <= right
  if cx          < 0     then cx    := 0;
  if (cx + left) > right then cx    := right - left; // now cx <= right - left
  if (cx + x)    > right then x     := right - cx;   // now cx+x<=right
  if (left       > x)    then x     := left;         // now x <= left, thus x+cx <= right
end;

procedure TfrmMdiChild.WMWindowPosChanging(var msg: TWMWindowPosChanging);
var
  r: TRect;
const
   WINDOW_TITLEBAR_SIZE = 23;
Begin
  with msg.Windowpos^ do
  begin
    if((WindowState = wsNormal) and ((flags and SWP_NOMOVE) = 0))
    then begin
      Windows.GetClientrect(Application.Mainform.clienthandle, r);
      Fit(x, cx, r.left, r.right);
      Fit(y, cy, r.top-WINDOW_TITLEBAR_SIZE, r.bottom); // Allow the titlebar to be moved offscreen
    end;
  end;
  inherited;
end;

function TfrmMdiChild.InitFromDesktop(tiView: TTreeItem): boolean;
begin
  Result := true;
end;

procedure TfrmMdiChild.StoreDesktopData;
begin
// nop
end;

end.
