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

unit fmDmsControl;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  fmBasePrimaryDataControl, ExtCtrls, StdCtrls, Grids, uDMSInterfaceTypes,
  uGenLib, uDmsUtils, // for TCharBuffer only
  Buttons, Menus, ImgList;

type ToolButtonID = (
	TB_ZoomAllLayers,   // Button Command
	TB_ZoomActiveLayer, // Button Command
	TB_ZoomSelectedObj, // Button Command

  TB_ZoomIn1,         // Key Command to zoom from center (triggered by '+' key)
	TB_ZoomOut1,        // Key Command to zoom from center (triggered by '-' key)
//	===========
	TB_ZoomIn2,				  // DualPoint   Tool
	TB_ZoomOut2,			  // SinglePoint Tool
	OBSOLETE_TB_Pan,
//	===========
	TB_Neutral,				  // zero-Tool, includes cooordinate copying when Ctrl (with Shift) key is pressed
//	===========
	OBSOLETE_TB_Info,
	TB_SelectObject,    // SinglePoint Tool
	TB_SelectRect,      // DualPoint   Tool
	TB_SelectCircle,    // DualPoint   Tool
	TB_SelectPolygon,   // PolyPoint   Tool
  TB_SelectDistrict,  // SinglePoint Tool
//	===========
	TB_SelectAll,       // Button Command
	TB_SelectNone,      // Button Command
	TB_SelectRows,      // Button Command
//	===========
	TB_Copy,            // Button Command: Copy Vieport Contents to Clipboard
	TB_CopyLC,          // Button Command: Copy LayerControl Contents to Clipboard
  TB_TableCopy,       // Button Command: Copy table as text to Clipboard
	TB_Export,          // Button Command: Copy MapControl contexts to .BMP files or TableControl contents to .csv files.
  TB_CutSel,          // Button Command
  TB_CopySel,         // Button Command
  TB_PasteSelDirect,  // Button Command
  TB_PasteSel,        // SinglePoint Tool
  TB_DeleteSel,       // Button Command
//	===========
	TB_Show_VP, TB_Show_VPLC, TB_Show_VPLCOV, // Tri-state Toggle Button for layout
	TB_SP_All, TB_SP_Active, TB_SP_None,      // Tri-state Toggle Button for showing palettes
	TB_NeedleOn,   TB_NeedleOff,         // Toggle-Button
	TB_ScaleBarOn, TB_ScaleBarOff,       // Toggle-Button
  TB_ShowSelOnlyOn, TB_ShowSelOnlyOff, // Toggle-Button
//	===========
	TB_SyncScale, TB_SyncROI,
  TB_TableGroupBy

//	TB_SetData,
);

type CmdStatusID = (
        CMD_ENABLED,
        CMD_DISABLED,
        CMD_HIDDEN,
        CMD_DOWN,
        CMD_UP
);

  TfraDmsControl = class(TfraBasePrimaryDataControl)

    sbZoomIn2:      TSpeedButton;
    sbZoomOut2:     TSpeedButton;

    sbZoomAllLayers: TSpeedButton;
    sbZoomActiveLayer: TSpeedButton;

    sbCopy:         TSpeedButton;
    sbExport:       TSpeedButton;
    sbSelectObject: TSpeedButton;
    sbSelectRect:   TSpeedButton;
    sbSelectCircle: TSpeedButton;

    lblCoord:       TLabel;

    pmDmsControl: TPopupMenu;
    sbZoomSelectedObj: TSpeedButton;
    sbSelectPolygon: TSpeedButton;
    sbSelectAll: TSpeedButton;
    sbSelectNone: TSpeedButton;
    sbNeedleToggle: TSpeedButton;
    sbPaletteToggle: TSpeedButton;
    sbShowToggle: TSpeedButton;
    sbScaleBarToggle: TSpeedButton;
    ilLayouts: TImageList;
    sbCopyLC: TSpeedButton;
    pnlTableView: TPanel;
    sbTableSnapshot: TSpeedButton;
    sbTableCopy: TSpeedButton;
    sbSelectRows: TSpeedButton;
    sbSelectAll2: TSpeedButton;
    sbSelectNone2: TSpeedButton;
    sbZoomSelectedObj2: TSpeedButton;
    sbSelectDistrict: TSpeedButton;
    sbCopySel: TSpeedButton;
    sbPasteSel: TSpeedButton;
    sbDeleteSel: TSpeedButton;
    sbCutSel: TSpeedButton;
    sbPasteSelDirect: TSpeedButton;
    sbToggleShowSelOnly2: TSpeedButton;
    sbToggleShowSelOnly: TSpeedButton;
    sbSyncScale: TSpeedButton;
    sbSyncROI: TSpeedButton;
    sbTableExport: TSpeedButton;
    sbGroupBy: TSpeedButton;

    procedure FrameDragDrop(Sender, Source: TObject; X, Y: Integer);
    procedure FrameDragOver(Sender, Source: TObject; X, Y: Integer;
      State: TDragState; var Accept: Boolean);

    procedure sbZoomIn2Click(Sender: TObject);
    procedure sbZoomOut2Click(Sender: TObject);

    procedure sbZoomAllLayersClick(Sender: TObject);
    procedure sbZoomActiveLayerClick(Sender: TObject);

    procedure sbSyncScaleClick(Sender: TObject);
    procedure sbSyncROIClick(Sender: TObject);

    procedure sbExportClick(Sender: TObject);
    procedure sbCopyClick(Sender: TObject);

    procedure sbSelectCircleClick(Sender: TObject);
    procedure sbSelectObjectClick(Sender: TObject);
    procedure sbSelectRectClick(Sender: TObject);
    procedure sbZoomSelectedObjClick(Sender: TObject);
    procedure sbSelectPolygonClick(Sender: TObject);
    procedure sbSelectAllClick(Sender: TObject);
    procedure sbSelectNoneClick(Sender: TObject);
    procedure sbShowToggleClick(Sender: TObject);
    procedure sbPaletteToggleClick(Sender: TObject);
    procedure sbNeedleToggleClick(Sender: TObject);
    procedure sbScaleBarToggleClick(Sender: TObject);
    procedure sbCopyLCClick(Sender: TObject);
    procedure sbSelectRowsClick(Sender: TObject);
    procedure sbPasteSelClick(Sender: TObject);
    procedure sbTableCopyClick(Sender: TObject);
    procedure sbCopySelClick(Sender: TObject);
    procedure sbSelectDistrictClick(Sender: TObject);
    procedure sbCutSelClick(Sender: TObject);
    procedure sbDeleteSelClick(Sender: TObject);
    procedure sbPasteSelDirectClick(Sender: TObject);
    procedure sbToggleShowSelOnlyClick(Sender: TObject);
    procedure sbToggleShowSelOnly2Click(Sender: TObject);
    procedure sbGroupByClick(Sender: TObject);

  private
    { Private declarations }
    m_DataView    : PDataView;
    m_ViewStyle   : TTreeItemViewStyle;

  public
    m_LayoutState, m_ShowPaletteState: ToolButtonID;

    { Public declarations }
    constructor CreateNew(AOwner: TComponent; tiView: TTreeItem; ct: TTreeItemViewStyle; sm: TShvSyncMode); override;

    constructor CreateFromObject(AOwner: TComponent; createStruct: PCreateStruct); override;
    destructor Destroy; override;

    class function AllInfoPresent(const tiSource: TTreeItem; ct: TTreeItemViewStyle): boolean; override;

    function LoadTreeItem(tiSource: TTreeItem; loadContext: PLoadContext): boolean; override;
    function IsLoadable(tiSource: TTreeItem): boolean; override;
    function GetToolbar: TPanel; override;
    procedure OnActivate; override;

    function GetExportInfo: SourceString;

    // for desktop
    function InitFromDesktop(tiView: TTreeItem): boolean; override;
    procedure StoreDesktopData; override;
    procedure SendToolID(id: ToolButtonID);

//  protected
    procedure WndProc(var Message: TMessage); override;
  private
    procedure CreateImpl;
    procedure UpdateButtons;
    procedure UpdateButton(sb: TSpeedButton; id: ToolButtonID);
    procedure UpdateControl(var done: Boolean);
  end;

implementation

uses ComCtrls,
  dmGeneral,
  uDmsInterface,
  Configuration;

const UM_COMMAND_STATUS = WM_APP;

{$R *.DFM}

class function TfraDmsControl.AllInfoPresent(const tiSource: TTreeItem; ct: TTreeItemViewStyle): boolean;
begin
   Result := SHV_CanUseViewStyle(tiSource, ct);
   if Result then exit;
   if ct <> tvsMapView then exit;

   Result := IsDataItem(tiSource) and CanBeDisplayed(tiSource);
end;

procedure OnStatusText(clientHandle: TClientHandle; st: TSeverityType; msg: PMsgChar); cdecl;
begin
  if (st = ST_MajorTrace) then
    TFraDmsControl(clientHandle).FrameCaption := msg
  else
    TFraDmsControl(clientHandle).lblCoord.Caption := msg;
end;


constructor TfraDmsControl.CreateNew(AOwner: TComponent; tiView: TTreeItem; ct: TTreeItemViewStyle; sm: TShvSyncMode);
begin
  Assert(tiView <> Nil);

  inherited _Create(AOwner, tiView);
  Assert(tiView = m_TiView);

  m_DataView  := Nil;
  m_ViewStyle := ct;

  Align := alClient; // If we do this designtime, Delphi tends to screw up the layout.

//  Assert(Handle<>0);
  m_DataView := SHV_DataView_Create(tiView, ord(ct), ord(sm));


  CreateImpl;

  FrameCaption := TreeItem_GetDisplayName(tiView);
end;

constructor TfraDmsControl.CreateFromObject(AOwner: TComponent; createStruct: PCreateStruct);
begin
  inherited _Create(AOwner, createStruct.contextItem);

  Align := alClient; // If we do this designtime, Delphi tends to screw up the layout.

  m_DataView  := createStruct.dataViewObj;
  m_ViewStyle := TTreeItemViewStyle(createStruct.ct);
  assert(  Assigned(m_DataView) ); // PRECONDITION

  CreateImpl;

  FrameCaption := createStruct.caption;
end;

procedure TfraDmsControl.CreateImpl;
begin
  if Assigned(m_DataView) then
  begin
    dmfGeneral.RegisterUpdateControlProc(UpdateControl);
    SHV_DataView_SetStatusTextFunc(m_DataView, TClientHandle(self), OnStatusText);
  end;

  m_LayoutState      := TB_Show_VP;  ilLayouts.GetBitmap(0, sbShowToggle.Glyph);
  m_ShowPaletteState := TB_SP_Active;
end;

destructor TfraDmsControl.Destroy;
begin
  if Assigned(m_DataView) then
  begin
    dmfGeneral.UnRegisterUpdateControlProc(UpdateControl);
    SHV_DataView_Destroy(m_DataView);
    m_DataView := Nil;
  end;

  inherited;
end;

procedure TfraDmsControl.WndProc(var Message: TMessage);
begin
  if  assigned(m_DataView) then
  begin
     if SHV_DataView_DispatchMessage(m_DataView, Handle, Message.msg, Message.WParam, Message.LParam, @(Message.Result))
     then exit;
  end;

  inherited;
end;

procedure TfraDmsControl.SendToolID(id: ToolButtonID);
var Message: TMessage;
begin
  Message.msg    := WM_COMMAND;
  Message.WParam := ord(id);
  Message.LParam := 0;
  WndProc(Message);
end;

function TfraDmsControl.LoadTreeItem(tiSource: TTreeItem; loadContext: PLoadContext): boolean;
begin
  if Assigned(m_DataView) and SHV_DataView_CanContain(m_DataView, tiSource) then
     Result := SHV_DataView_AddItem(m_DataView, tiSource, Assigned(loadContext) And loadContext^.isDropped)
  else Result := false;
end;

procedure TfraDmsControl.FrameDragOver(Sender, Source: TObject; X,
  Y: Integer; State: TDragState; var Accept: Boolean);
begin
  inherited;
  Accept := true;
end;

procedure TfraDmsControl.FrameDragDrop(Sender, Source: TObject; X, Y: Integer);
var loadContext: TLoadContext;
begin
  inherited;
  if ((Source is TTreeView) and (TTreeView(Source).Selected <> nil) and (TTreeView(Source).Selected.Data <> nil))
  then begin
    if Assigned(m_DataView) and SHV_DataView_CanContain(m_DataView, TTreeView(Source).Selected.Data)
      then begin
        loadContext.isDropped := true;
        LoadTreeItem(TTreeView(Source).Selected.Data, @loadContext)
      end
      else DispMessage('Item is incompatible with this view.');
  end;
end;

// for desktop

function TfraDmsControl.InitFromDesktop(tiView: TTreeItem): boolean;
begin
  Result := true;
end;


function TfraDmsControl.IsLoadable(tiSource: TTreeItem): boolean;
begin
   // if m_auCommon is defined, make sure tiSource's domain equals m_auCommon
  Result := Assigned(m_DataView) and SHV_DataView_CanContain(m_DataView, tiSource);
end;

function TfraDmsControl.GetToolbar: TPanel;
begin
  case m_ViewStyle of
    tvsDataGrid, tvsClassification, tvsPalette, tvsDataGridContainer:
        Result := pnlTableView;
    tvsMapView, tvsSupplierSchema, tvsSubItemSchema, tvsExprSchema, tvsUpdateItem, tvsUpdateTree:
        Result := inherited GetToolbar;
  else
    dmfGeneral.DispError('DmsView has an Invalid ViewStyle', false);
    Result := Nil;
  end
end;

// Popup menu handling

procedure TfraDmsControl.UpdateButtons;
begin
  case m_ViewStyle of
    tvsDataGrid, tvsClassification, tvsPalette, tvsDataGridContainer:
      begin
        UpdateButton(sbZoomSelectedObj2,   TB_ZoomSelectedObj);
        UpdateButton(sbSelectRows,         TB_SelectRows);
        UpdateButton(sbSelectAll2,         TB_SelectAll);
        UpdateButton(sbSelectNone2,        TB_SelectNone);
        UpdateButton(sbToggleShowSelOnly2, TB_ShowSelOnlyOn);
      end;
    tvsMapView:
        begin
        // REQUIREMENT: Grid
          UpdateButton(sbCopySel,        TB_CopySel);
        // REQUIREMENT: Editable Grid
          UpdateButton(sbCutSel,         TB_CutSel);
          UpdateButton(sbPasteSelDirect, TB_PasteSelDirect);
          UpdateButton(sbPasteSel,       TB_PasteSel);
          UpdateButton(sbDeleteSel,      TB_DeleteSel);

          // Requirement UM_Select
          UpdateButton(sbZoomSelectedObj,   TB_ZoomSelectedObj);
          UpdateButton(sbSelectObject,      TB_SelectObject);
          UpdateButton(sbSelectRect,        TB_SelectRect);
          UpdateButton(sbSelectCircle,      TB_SelectCircle);
          UpdateButton(sbSelectPolygon,     TB_SelectPolygon);
          UpdateButton(sbSelectDistrict,    TB_SelectDistrict); // REQUIREMENT: Grid
          UpdateButton(sbSelectAll,         TB_SelectAll);
          UpdateButton(sbSelectNone,        TB_SelectNone);
          UpdateButton(sbToggleShowSelOnly, TB_ShowSelOnlyOn);
          UpdateButton(sbSyncScale,         TB_SyncScale);
          UpdateButton(sbSyncROI,           TB_SyncROI);
        end;
  end;
end;

procedure TfraDmsControl.UpdateButton(sb: TSpeedButton; id: ToolButtonID);
var Message: TMessage;
begin
  Message.Msg := UM_COMMAND_STATUS;
  Message.WParam := ord(id);
  Message.LParam := 0;
  WndProc(Message);
  case CmdStatusID(Message.Result) of
    CMD_ENABLED:  begin sb.Visible := true;  sb.Enabled := true;  end;
    CMD_DISABLED: begin sb.Visible := true;  sb.Enabled := false; end;
    CMD_HIDDEN:   begin sb.Visible := False; sb.Enabled := false; end;
    CMD_DOWN:     begin sb.Visible := true;  sb.Enabled := true;  sb.Down := true;  end;
    CMD_UP:       begin sb.Visible := true;  sb.Enabled := true;  sb.Down := false; end;
  end;
end;

procedure TfraDmsControl.UpdateControl(var done: Boolean);
begin
  UpdateButtons;
  if Assigned(m_DataView) and not SHV_DataView_Update(m_DataView) then
     Done := false;
end;

procedure TfraDmsControl.OnActivate;
begin
  SetFocus;
end;

procedure TfraDmsControl.StoreDesktopData;
begin
  if Assigned(m_DataView) then
    SHV_DataView_StoreDesktopData(m_DataView);
end;


//==============================================================

procedure TfraDmsControl.sbZoomAllLayersClick(Sender: TObject);
begin
  SendToolId(TB_ZoomAllLayers);
end;

procedure TfraDmsControl.sbZoomActiveLayerClick(Sender: TObject);
begin
  SendToolId(TB_ZoomActiveLayer);
end;

procedure TfraDmsControl.sbZoomSelectedObjClick(Sender: TObject);
begin
  SendToolId(TB_ZoomSelectedObj);
end;


//==============================================================

procedure TfraDmsControl.sbZoomIn2Click(Sender: TObject);
begin
  if sbZoomIn2.Down
  then SendToolId(TB_ZoomIn2)
  else SendToolId(TB_Neutral);
end;

procedure TfraDmsControl.sbZoomOut2Click(Sender: TObject);
begin
  if sbZoomOut2.Down
  then SendToolId(TB_ZoomOut2)
  else SendToolId(TB_Neutral);
end;

procedure TfraDmsControl.sbSyncScaleClick(Sender: TObject);
begin
  SendToolID(TB_SyncScale);
end;

procedure TfraDmsControl.sbSyncROIClick(Sender: TObject);
begin
  SendToolID(TB_SyncROI);
end;


//==============================================================

procedure TfraDmsControl.sbSelectObjectClick(Sender: TObject);
begin
  if sbSelectObject.Down
  then SendToolId(TB_SelectObject)
  else SendToolId(TB_Neutral);
end;

procedure TfraDmsControl.sbSelectRectClick(Sender: TObject);
begin
  if sbSelectRect.Down
  then SendToolId(TB_SelectRect)
  else SendToolId(TB_Neutral);
end;

procedure TfraDmsControl.sbSelectCircleClick(Sender: TObject);
begin
  if sbSelectCircle.Down
  then SendToolId(TB_SelectCircle)
  else SendToolId(TB_Neutral);
end;

procedure TfraDmsControl.sbSelectPolygonClick(Sender: TObject);
begin
  if sbSelectPolygon.Down
  then SendToolId(TB_SelectPolygon)
  else SendToolId(TB_Neutral);
end;

procedure TfraDmsControl.sbSelectDistrictClick(Sender: TObject);
begin
  SendToolId(TB_SelectDistrict);
end;

//==============================================================

procedure TfraDmsControl.sbSelectAllClick(Sender: TObject);
begin
  SendToolId(TB_SelectAll);
end;

procedure TfraDmsControl.sbSelectNoneClick(Sender: TObject);
begin
  SendToolId(TB_SelectNone);
end;

procedure TfraDmsControl.sbSelectRowsClick(Sender: TObject);
begin
  SendToolId(TB_SelectRows);
end;


//==============================================================

procedure TfraDmsControl.sbCopyClick(Sender: TObject);
begin
  SendToolId(TB_Copy);
end;

procedure TfraDmsControl.sbCopyLCClick(Sender: TObject);
begin
  SendToolId(TB_CopyLC);
end;

function TfraDmsControl.GetExportInfo: SourceString;
var nrRows, nrCols, nrDotRows, nrDotCols: UInt32;
  fullFileNameBaseStr: IString;
begin
  assert(Assigned(m_DataView));
  Result := '';
  if not SHV_DataView_GetExportInfo(m_DataView, @nrRows, @nrCols, @nrDotRows, @nrDotCols, @fullFileNameBaseStr)
  then exit;
  try
    if nrCols <= 0 then
    begin
      Result := AsString(nrDotRows)+' x '+AsString(nrDotCols) + ' values to ' + DMS_IString_AsCharPtr(fullFileNameBaseStr);
      if nrRows <> 1 then
        Result := 'Export ' + AsString(nrRows) + ' tiles of ' + Result;
    end
    else
    begin
      Result := AsString(nrDotRows)+' x '+AsString(nrDotCols) + ' pixels to ' + DMS_IString_AsCharPtr(fullFileNameBaseStr);
      if (nrRows <> 1) OR (nrCols <> 1) then
        Result := 'Export ' + AsString(nrRows) +' x ' + AsString(nrCols) + ' tiles of ' + Result;
    end;
  finally
    DMS_IString_Release(fullFileNameBaseStr);
  end;
end;

procedure TfraDmsControl.sbExportClick(Sender: TObject);
var exportInfo: SourceString;
begin
  if not Assigned(m_DataView) then exit;
  exportInfo := GetExportInfo;
  if exportInfo = '' then exit;

  if Application.MessageBox(PChar(String(exportInfo)), PChar('Export View'),  MB_OKCANCEL) = ID_OK
  then SendToolId(TB_Export);
end;

procedure TfraDmsControl.sbTableCopyClick(Sender: TObject);
begin
  SendToolId(TB_TableCopy);
end;

procedure TfraDmsControl.sbCutSelClick(Sender: TObject);
begin
  SendToolID(TB_CutSel);
end;

procedure TfraDmsControl.sbCopySelClick(Sender: TObject);
begin
  SendToolId(TB_CopySel);
end;

procedure TfraDmsControl.sbPasteSelDirectClick(Sender: TObject);
begin
  SendToolId(TB_PasteSelDirect);
end;

procedure TfraDmsControl.sbPasteSelClick(Sender: TObject);
begin
  SendToolId(TB_PasteSel);
end;

procedure TfraDmsControl.sbDeleteSelClick(Sender: TObject);
begin
  SendToolID(TB_DeleteSel);
end;

procedure TfraDmsControl.sbGroupByClick(Sender: TObject);
begin
  if TSpeedButton(Sender).Down
  then SendToolId(TB_TableGroupBy)
  else SendToolId(TB_Neutral);
end;

//==============================================================

procedure TfraDmsControl.sbShowToggleClick(Sender: TObject);
begin
  SendToolID(m_LayoutState);
  INC(m_LayoutState);
  if m_LayoutState > TB_Show_VPLCOV then m_LayoutState := TB_Show_VP;
  sbShowToggle.Glyph := nil;
  ilLayouts.GetBitmap(Ord(m_LayoutState)-Ord(TB_Show_VP), sbShowToggle.Glyph);
end;

procedure TfraDmsControl.sbPaletteToggleClick(Sender: TObject);
begin
  SendToolID(m_ShowPaletteState);
  INC(m_ShowPaletteState);
  if (m_ShowPaletteState > TB_SP_None) then m_ShowPaletteState := TB_SP_All;
end;

procedure TfraDmsControl.sbNeedleToggleClick(Sender: TObject);
begin
  if sbNeedleToggle.Down
  then SendToolID(TB_NeedleOn)
  else SendToolID(TB_NeedleOff);
end;

procedure TfraDmsControl.sbScaleBarToggleClick(Sender: TObject);
begin
  if sbScaleBarToggle.Down
  then SendToolID(TB_ScaleBarOn)
  else SendToolID(TB_ScaleBarOff);
end;

procedure TfraDmsControl.sbToggleShowSelOnlyClick(Sender: TObject);
begin
  if sbToggleShowSelOnly.Down
  then SendToolID(TB_ShowSelOnlyOn )
  else SendToolID(TB_ShowSelOnlyOff);
end;

procedure TfraDmsControl.sbToggleShowSelOnly2Click(Sender: TObject);
begin
  if sbToggleShowSelOnly2.Down
  then SendToolID(TB_ShowSelOnlyOn )
  else SendToolID(TB_ShowSelOnlyOff);
end;

end.
