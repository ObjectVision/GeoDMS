unit fmDualListBox;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Forms, Dialogs, Controls, StdCtrls,
  Buttons;

type
  TSelectionChangedEvent = procedure(Sender: TObject) of object;

  TfraDualListBox = class(TFrame)
    SrcList: TListBox;
    DstList: TListBox;
    SrcLabel: TLabel;
    DstLabel: TLabel;
    IncludeBtn: TSpeedButton;
    IncAllBtn: TSpeedButton;
    ExcludeBtn: TSpeedButton;
    ExAllBtn: TSpeedButton;
    procedure IncludeBtnClick(Sender: TObject);
    procedure ExcludeBtnClick(Sender: TObject);
    procedure IncAllBtnClick(Sender: TObject);
    procedure ExcAllBtnClick(Sender: TObject);
    procedure FrameResize(Sender: TObject);
  private
    { Private declarations }
  protected
    m_nMaxSelection : integer;
    m_evOnSelectionChangedEvent : TSelectionChangedEvent;
  public
    { Public declarations }
    constructor Create(AOwner: TComponent); override;

    procedure MoveSelected(List: TCustomListBox; Items: TStrings);
    procedure SetItem(List: TListBox; Index: Integer);
    function  GetFirstSelection(List: TCustomListBox): Integer;
    procedure SetButtons;

    procedure AddObject(const sCaption: string; const pObject: TObject);
  end;

var
  fraDualListBox: TfraDualListBox;

implementation

{$R *.DFM}

constructor TfraDualListBox.Create(AOwner: TComponent);
begin
  inherited;
  m_nMaxSelection := 100;
  m_evOnSelectionChangedEvent := nil;
end;

procedure TfraDualListBox.IncludeBtnClick(Sender: TObject);
var
  Index: Integer;
begin
  if(DstList.Items.Count >= m_nMaxSelection)
  then begin
    ShowMessage('Maximum allowed: ' + inttostr(m_nMaxSelection));
    Exit;
  end;

  Index := GetFirstSelection(SrcList);
  MoveSelected(SrcList, DstList.Items);
  SetItem(SrcList, Index);

  if(Assigned(m_evOnSelectionChangedEvent))
  then m_evOnSelectionChangedEvent(Self);
end;

procedure TfraDualListBox.ExcludeBtnClick(Sender: TObject);
var
  Index: Integer;
begin
  Index := GetFirstSelection(DstList);
  MoveSelected(DstList, SrcList.Items);
  SetItem(DstList, Index);

  if(Assigned(m_evOnSelectionChangedEvent))
  then m_evOnSelectionChangedEvent(Self);
end;

procedure TfraDualListBox.IncAllBtnClick(Sender: TObject);
var
  I: Integer;
begin
  if(DstList.Items.Count + SrcList.Items.Count >= m_nMaxSelection)
  then begin
    ShowMessage('Maximum allowed: ' + inttostr(m_nMaxSelection));
    Exit;
  end;

  for I := 0 to SrcList.Items.Count - 1 do
    DstList.Items.AddObject(SrcList.Items[I], SrcList.Items.Objects[I]);

  SrcList.Items.Clear;
  SetItem(SrcList, 0);

  if(Assigned(m_evOnSelectionChangedEvent))
  then m_evOnSelectionChangedEvent(Self);
end;

procedure TfraDualListBox.ExcAllBtnClick(Sender: TObject);
var
  I: Integer;
begin
  for I := 0 to DstList.Items.Count - 1 do
    SrcList.Items.AddObject(DstList.Items[I], DstList.Items.Objects[I]);
  DstList.Items.Clear;
  SetItem(DstList, 0);

  if(Assigned(m_evOnSelectionChangedEvent))
  then m_evOnSelectionChangedEvent(Self);
end;

procedure TfraDualListBox.MoveSelected(List: TCustomListBox; Items: TStrings);
var
  I: Integer;
begin
  for I := List.Items.Count - 1 downto 0 do
    if List.Selected[I] then
    begin
      Items.AddObject(List.Items[I], List.Items.Objects[I]);
      List.Items.Delete(I);
    end;
end;

procedure TfraDualListBox.SetButtons;
var
  SrcEmpty, DstEmpty: Boolean;
begin
  SrcEmpty := SrcList.Items.Count = 0;
  DstEmpty := DstList.Items.Count = 0;
  IncludeBtn.Enabled := not SrcEmpty;
  IncAllBtn.Enabled := not SrcEmpty;
  ExcludeBtn.Enabled := not DstEmpty;
  ExAllBtn.Enabled := not DstEmpty;
end;

function TfraDualListBox.GetFirstSelection(List: TCustomListBox): Integer;
begin
  for Result := 0 to List.Items.Count - 1 do
    if List.Selected[Result] then Exit;
  Result := LB_ERR;
end;

procedure TfraDualListBox.SetItem(List: TListBox; Index: Integer);
var
  MaxIndex: Integer;
begin
  with List do
  begin
    SetFocus;
    MaxIndex := List.Items.Count - 1;
    if Index = LB_ERR then Index := 0
    else if Index > MaxIndex then Index := MaxIndex;
    Selected[Index] := True;
  end;
  SetButtons;
end;

procedure TfraDualListBox.AddObject(const sCaption: string; const pObject: TObject);
begin
  SrcList.Items.AddObject(sCaption, pObject);
  SetButtons;
end;

procedure TfraDualListBox.FrameResize(Sender: TObject);
begin
  SrcList.Width  := (Width - 32) div 2;
  DstList.Width := (Width - 32) div 2;
  DstList.Left  := Width - DstList.Width;
  DstLabel.Left := DstList.Left;

  IncludeBtn.Left := SrcList.Width + 6;
  IncAllBtn.Left  := SrcList.Width + 6;
  ExcludeBtn.Left := SrcList.Width + 6;
  ExAllBtn.Left   := SrcList.Width + 6;
end;

end.
