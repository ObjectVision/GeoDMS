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

unit fPrimaryDataViewer;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  fMdiChild, fmBasePrimaryDataControl, uDMSInterfaceTypes, ExtCtrls, StdCtrls,
  Buttons;

type
  TfrmPrimaryDataViewer = class(TfrmMdiChild)
    procedure FormActivate(Sender: TObject);
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
  private
    { Private declarations }
    m_ctlMain: TfraBasePrimaryDataControl;
    m_tiView : TTreeItem;

    procedure SetCtlMain(ctlMain: TfraBasePrimaryDataControl);
  public
    { Public declarations }
    constructor CreateNew(AOwner: TComponent; dcClass: TDataControlClass; ct: TTreeItemViewStyle); reintroduce;
    constructor CreateFromDesktop(AOwner: TComponent; dcClass: TDataControlClass; tiView: TTreeItem; ct: TTreeItemViewStyle);
    constructor CreateFromObject (AOwner: TComponent; dcClass: TDataControlClass; createStruct: PCreateStruct);
    function InitFromDesktop(tiView: TTreeItem): boolean; override;
    procedure StoreDesktopData; override;

    procedure CtlMainCaptionChanged(const sCaption: string);

    property DataControl: TfraBasePrimaryDataControl read m_ctlMain;
    property ViewItem:    TTreeItem read m_tiView;
  end;

implementation

{$R *.DFM}

uses
  uDMSUtils,
  Configuration;

//////////
// PUBLIC
//////////

constructor TfrmPrimaryDataViewer.CreateNew(AOwner: TComponent; dcClass: TDataControlClass; ct: TTreeItemViewStyle);
begin
  inherited Create(AOwner);

  m_tiView :=
    TConfiguration.CurrentConfiguration.DesktopManager.NewView(
      nil,
      PSourceChar(SourceString(Self.ClassName)),
      PSourceChar(SourceString(dcClass.ClassName)),
      ct
    );
  SetCtlMain( dcClass.CreateNew(Self, m_tiView, ct, smSave) );
end;

constructor TfrmPrimaryDataViewer.CreateFromDesktop(AOwner: TComponent; dcClass: TDataControlClass; tiView: TTreeItem; ct: TTreeItemViewStyle);
begin
  inherited Create(AOwner);

  assert(Assigned(tiView) );
  m_tiView := tiView;

  SetCtlMain( dcClass.CreateNew(Self, m_tiView, ct, smLoad) );

  InitFromDesktop(m_tiView);
end;

constructor TfrmPrimaryDataViewer.CreateFromObject(AOwner: TComponent; dcClass: TDataControlClass; createStruct: PCreateStruct);
begin
  inherited Create(AOwner);

  m_tiView := TConfiguration.CurrentConfiguration.DesktopManager.NewView(
    createStruct.contextItem,
    PItemChar(ItemString(Self.ClassName)),
    PItemChar(ItemString(dcClass.ClassName)),
    TTreeItemViewStyle(createStruct.ct)
  );
  SetCtlMain( dcClass.CreateFromObject(Self, createStruct) );
end;

procedure TfrmPrimaryDataViewer.SetCtlMain(ctlMain: TfraBasePrimaryDataControl);
begin
  m_ctlMain := ctlMain;
  m_ctlMain.Parent := Self;

  SetMdiToolbar( m_ctlMain.GetToolbar );

  m_ctlMain.OnCaptionChanged := CtlMainCaptionChanged;
  CtlMainCaptionChanged( m_ctlMain.FrameCaption );
end;

procedure TfrmPrimaryDataViewer.FormActivate(Sender: TObject);
begin
  inherited;
  if(m_ctlMain <> nil)
  then m_ctlMain.OnActivate;
end;

function TfrmPrimaryDataViewer.InitFromDesktop(tiView: TTreeItem): boolean;
begin
  Result := m_CtlMain.InitFromDesktop(tiView);
end;

procedure TfrmPrimaryDataViewer.CtlMainCaptionChanged(const sCaption: string);
begin
  Caption := sCaption;
end;

procedure TfrmPrimaryDataViewer.FormClose(Sender: TObject;
  var Action: TCloseAction);
begin
  inherited;
  if(TConfiguration.CurrentIsAssigned)
  then begin
    m_ctlMain.FreeResources;
    TConfiguration.CurrentConfiguration.DesktopManager.DeleteView(m_tiView);
    m_tiView := nil;
  end;
end;

procedure TfrmPrimaryDataViewer.StoreDesktopData;
begin
  inherited;
  if Assigned(m_tiView) and TConfiguration.CurrentIsAssigned
  then begin
    TConfiguration.CurrentConfiguration.DesktopManager.StoreViewPosition(m_tiView, self);
    if(m_ctlMain <> nil)
    then m_ctlMain.StoreDesktopData;
  end;
end;


end.


