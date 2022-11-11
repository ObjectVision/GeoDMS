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

unit fmBasePrimaryDataControl;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ExtCtrls, uDMSInterfaceTypes, uDmsUtils;

type
  TOnCaptionChangedEvent = procedure (const sCaption: string) of object;

  TLoadContext = record
    isDropped: Boolean;
  end;
  PLoadContext = ^TLoadContext;

  TfraBasePrimaryDataControl = class(TFrame)
    pnlToolbar: TPanel;
  private
    { Private declarations }
    FOnCaptionChangedEvent: TOnCaptionChangedEvent;
    m_sCaption : string;

    procedure SetFrameCaption(sValue : string);
    procedure DoCaptionChangedEvent;
  protected
    m_wcTools: TWinControl;
    m_tiView : TTreeItem;
  public
    { Public declarations }

    property OnCaptionChanged: TOnCaptionChangedEvent read FOnCaptionChangedEvent write FOnCaptionChangedEvent;
    property FrameCaption: string read m_sCaption write SetFrameCaption;

    constructor _Create(AOwner: TComponent; tiView: TTreeItem);
    constructor CreateNew(AOwner: TComponent; tiView: TTreeItem; ct: TTreeItemViewStyle; sm: TShvSyncMode); virtual; abstract;
    constructor CreateFromObject(AOwner: TComponent; createStruct: PCreateStruct); virtual; abstract;
    class function AllInfoPresent(const tiSource: TTreeItem; ct: TTreeItemViewStyle): boolean; virtual; abstract;

    procedure SetVisible(pKey: Pointer; bVisible: boolean); virtual; abstract;
    procedure SetMinimized(pKey: Pointer; bMinimized: boolean); virtual; abstract;
    procedure Remove(pKey: Pointer); virtual; abstract;
    procedure Move(pKeySrc, pKeyDst: Pointer; bBefore: boolean); virtual; abstract;
    function GetToolbar: TPanel; virtual;
    function InitFromDesktop(tiView: TTreeItem): boolean; virtual; abstract;

    function LoadTreeItem(tiSource: TTreeItem; loadContext: PLoadContext): boolean; virtual; abstract;
    function IsLoadable(tiSource: TTreeItem): boolean; virtual; abstract;
    procedure FreeResources; virtual;
    procedure StoreDesktopData; virtual;
    procedure OnActivate; virtual;

//    procedure Update; reintroduce; virtual; abstract;
  end;

  TDataControlClass = class of TfraBasePrimaryDataControl;

implementation

{$R *.DFM}

constructor TfraBasePrimaryDataControl._Create(AOwner: TComponent; tiView: TTreeItem);
begin
  inherited Create(AOwner);
  m_tiView := tiView;
end;

function TfraBasePrimaryDataControl.GetToolbar: TPanel;
begin
  Result := pnlToolbar;
end;

procedure TfraBasePrimaryDataControl.SetFrameCaption(sValue: string);
begin
  if(m_sCaption <> sValue)
  then begin
    m_sCaption := sValue;
    DoCaptionChangedEvent;
  end;
end;

procedure TfraBasePrimaryDataControl.DoCaptionChangedEvent;
begin
  if(Assigned(FOnCaptionChangedEvent))
  then FOnCaptionChangedEvent(m_sCaption);
end;

procedure TfraBasePrimaryDataControl.FreeResources;
begin
//
end;

procedure TfraBasePrimaryDataControl.StoreDesktopData;
begin
//
end;

procedure TfraBasePrimaryDataControl.OnActivate;
begin
//
end;

end.
