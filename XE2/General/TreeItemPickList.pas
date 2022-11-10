unit TreeItemPickList;

interface

uses
    Menus, ComCtrls, Classes, Controls, Forms, Windows, 
    uDMSInterfaceTypes,
    ItemColumnProvider;

type
  TCheckTreeItemEvent = procedure(const tiSelected: TTreeItem; var bAllow: boolean) of object;


  TTreeItemPickList = class(TFrame)
    stnMain: TStatusBar;
    lvTreeItems: TListView;
    pmColumnHeaders: TPopupMenu;
    miSetfilter: TMenuItem;
    procedure lvTreeItemsSelectItem(Sender: TObject; Item: TListItem;
      Selected: Boolean);
    procedure lvTreeItemsDblClick(Sender: TObject);
    procedure lvTreeItemsColumnClick(Sender: TObject; Column: TListColumn);
    procedure lvTreeItemsCompare(Sender: TObject; Item1, Item2: TListItem;
      Data: Integer; var Compare: Integer);
    procedure lvTreeItemsColumnRightClick(Sender: TObject;
      Column: TListColumn; Point: TPoint);
    procedure miSetfilterClick(Sender: TObject);

    constructor Create(AOwner: TComponent); override;
    destructor  Destroy; override;

  private
    { Private declarations }
    m_evOnCheckTreeItemEvent: TCheckTreeItemEvent;
    m_evDblClick:             TNotifyEvent;
    m_evSelChange:            TNotifyEvent;

    m_arColumns: array of TAbstrItemColumnProvider;

    m_tiSelected:  TTreeItem;
    m_nSortDirection,
    m_nSortColumn: integer;
    m_lvCopy:      TListView;

    function GetSelCount: integer;
    function GetTheSelectedTreeItem: TTreeItem;
    function GetASelectedTreeItem(nIdx : integer): TTreeItem;

    procedure FilterItems(const nColumn : integer; const sFilter : string);
    function  CompareSemiNumeric(const S1, S2: string): integer;
   public
    { Public declarations }
    procedure AddRow(const tiThis : TTreeItem; bSelect: Boolean);
    procedure AddColumn(cp: TAbstrItemColumnProvider);
    procedure AddRows(const lstTreeItems : TList; tiSelected: TTreeItem);

    property SelCount : integer read GetSelCount;
    property SelectedTreeItem : TTreeItem read GetTheSelectedTreeItem;
    property SelectedTreeItemList[nIdx : integer] : TTreeItem read GetASelectedTreeItem;
    
    function CheckSelection: boolean;

    property OnCheckTreeItem: TCheckTreeItemEvent read m_evOnCheckTreeItemEvent write m_evOnCheckTreeItemEvent;
    property OnSelChange:     TNotifyEvent        read m_evSelChange            write m_evSelChange;
    property OnDblClick:      TNotifyEvent        read m_evDblClick             write m_evDblClick;
  end;

implementation

{$R *.DFM}

uses
    SysUtils
  , uGenLib
  , fEditProperty
//  , uDMSUtils, dmGeneral, fMain, Configuration;
;


constructor TTreeItemPickList.Create(AOwner: TComponent);
begin
  inherited;
  m_tiSelected := nil;
  m_evOnCheckTreeItemEvent := nil;
  m_evDblClick := Nil;
  m_evSelChange := Nil;

  m_nSortColumn := -1;
  m_nSortDirection := 1;
  m_lvCopy := nil;

  SetLength(m_arColumns, 0);
end;

destructor TTreeItemPickList.Destroy;
var i: Integer;
begin
  for i:=0 to Length(m_arColumns)-1 do
    m_arColumns[i].Free;
  m_lvCopy.Free;
  inherited;
end;

procedure TTreeItemPickList.AddRows(const lstTreeItems : TList; tiSelected: TTreeItem);
var
  nIdx : integer;
  ti:    TTreeItem;
begin
  lvTreeItems.Items.BeginUpdate;
  try
    if assigned(lstTreeItems) then
    for nIdx := 0 to lstTreeItems.Count - 1 do
    begin
      ti := lstTreeItems[nIdx];
      AddRow(ti, ti = tiSelected);
    end;
  finally
    lvTreeItems.Items.EndUpdate;
  end;
end;

function TTreeItemPickList.GetSelCount: integer;
begin
  Result := lvTreeItems.SelCount;
end;

function TTreeItemPickList.GetTheSelectedTreeItem: TTreeItem;
begin
  if(lvTreeItems.SelCount > 0)
  then Result := lvTreeItems.Selected.Data
  else Result := nil;
end;

function TTreeItemPickList.GetASelectedTreeItem(nIdx : integer): TTreeItem;
var
  nIdxInt : integer;
  liSelected : TListItem;
begin
  Result := Nil;
  if(not BetweenInc(nIdx, 0 , lvTreeItems.SelCount - 1))
  then begin
    ERangeError.Create('Index out of range.');
    Exit;
  end;

  liSelected := lvTreeItems.Selected;
  for nIdxInt := 1 to nIdx do
    liSelected := lvTreeItems.GetNextItem(liSelected, sdBelow, [isSelected]);

  Result := liSelected.Data;
end;

procedure TTreeItemPickList.AddColumn(cp: TAbstrItemColumnProvider);
var
  nc: Integer;
  lcNew : TListColumn;
begin
  Assert(lvTreeItems.Items.Count = 0);
  nc := Length(m_arColumns);
  SetLength(m_arColumns, nc+1);
  m_arColumns[nc] := cp;

  lcNew := lvTreeItems.Columns.Add;
  lcNew.Caption := cp.m_ColumnName;
  lcNew.Width   := cp.m_ColumnWidth;
end;

procedure TTreeItemPickList.AddRow(const tiThis : TTreeItem; bSelect: Boolean);
var
  liNew : TListItem;
  nIdx  : integer;
begin
  Assert(Length(m_arColumns) > 0);
  liNew := lvTreeItems.Items.Add;

  liNew.Caption := m_arColumns[0].GetValue(tiThis);
  for nIdx := 1 to length(m_arColumns) - 1 do
    liNew.SubItems.Add(m_arColumns[nIdx].GetValue(tiThis));

  liNew.Data := tiThis;
  if (bSelect) then lvTreeItems.Selected := liNew;
end;

procedure TTreeItemPickList.lvTreeItemsSelectItem(Sender: TObject;
  Item: TListItem; Selected: Boolean);
begin
  if Assigned(m_evSelChange)
  then m_evSelChange(self);
end;

procedure TTreeItemPickList.lvTreeItemsDblClick(Sender: TObject);
begin
  if not Assigned(m_evDblClick) then exit;
  if (lvTreeItems.SelCount > 0) and CheckSelection
  then m_evDblClick(self);
end;

procedure TTreeItemPickList.lvTreeItemsColumnClick(Sender: TObject;
  Column: TListColumn);
begin
  if(m_nSortColumn = Column.Index)
  then m_nSortDirection := -m_nSortDirection
  else begin
    m_nSortColumn := Column.Index;
    m_nSortDirection := 1;
  end;
  lvTreeItems.AlphaSort;
end;

procedure TTreeItemPickList.lvTreeItemsCompare(Sender: TObject; Item1,
  Item2: TListItem; Data: Integer; var Compare: Integer);
begin
  if(m_nSortColumn = 0)
  then Compare := m_nSortDirection * CompareSemiNumeric(Item1.Caption, Item2.Caption)
  else Compare := m_nSortDirection * CompareSemiNumeric(Item1.SubItems[m_nSortColumn - 1], Item2.SubItems[m_nSortColumn - 1]);
end;

procedure TTreeItemPickList.lvTreeItemsColumnRightClick(Sender: TObject;
  Column: TListColumn; Point: TPoint);
begin
  pmColumnHeaders.Tag := Column.Index;
  pmColumnHeaders.Popup(lvTreeItems.ClientToScreen(Point).X, lvTreeItems.ClientToScreen(Point).Y);
end;

procedure TTreeItemPickList.miSetfilterClick(Sender: TObject);
var
  sCaption : ItemString;
  sFilter  : SourceString;
begin
  sFilter := '';
  sCaption := 'Set filter on: ' + lvTreeItems.Columns[pmColumnHeaders.tag].Caption;
  if(TfrmEditProperty.Display(sCaption, sFilter, false))
  then begin
    FilterItems(pmColumnHeaders.tag, sFilter);
  end;
end;

procedure TTreeItemPickList.FilterItems(const nColumn : integer; const sFilter : string);
var
  nIdx : integer;
  SFilterLC : string;
  liNew : TListItem;
begin
  if(sFilter = '')
  then begin
    if assigned(m_lvCopy) then
    begin
      lvTreeItems.Items.Assign(m_lvCopy.Items);
      for nIdx := 0 to m_lvCopy.Items.Count - 1 do
         lvTreeItems.Items[nIdx].Selected := m_lvCopy.Items[nIdx].Selected;
    end;
    stnMain.Panels[0].Text := 'Filter: <NO FILTER>';
  end
  else begin
    if not assigned(m_lvCopy) then
    begin
      m_lvCopy := TListView.Create(nil);
      m_lvCopy.MultiSelect := lvTreeItems.MultiSelect;
      m_lvCopy.Visible := false;
      m_lvCopy.Parent := self;
      m_lvCopy.Items.Assign(lvTreeItems.Items);
      for nIdx := 0 to m_lvCopy.Items.Count - 1 do
         m_lvCopy.Items[nIdx].Selected := lvTreeItems.Items[nIdx].Selected;
    end;
    SFilterLC := lowercase(sFilter);
    lvTreeItems.Items.Clear;
    if(nColumn = 0)
    then begin
      for nIdx := 0 to m_lvCopy.Items.Count - 1 do
      begin
        if(pos(SFilterLC, lowercase(m_lvCopy.Items[nIdx].Caption)) > 0)
        then begin
          liNew := lvTreeItems.Items.Add;
          liNew.Assign(m_lvCopy.Items[nIdx]);
          liNew.Selected := m_lvCopy.Items[nIdx].Selected;
        end;
      end;
    end
    else begin
      for nIdx := 0 to m_lvCopy.Items.Count - 1 do
      begin
        if(pos(SFilterLC, lowercase(m_lvCopy.Items[nIdx].SubItems[nColumn - 1])) > 0)
        then begin
          liNew := lvTreeItems.Items.Add;
          liNew.Assign(m_lvCopy.Items[nIdx]);
          liNew.Selected := m_lvCopy.Items[nIdx].Selected;
        end;
      end;
    end;

    stnMain.Panels[0].Text := 'Filter: "' + sFilter + '" on ' + lvTreeItems.Columns[nColumn].Caption;
  end;

  if(m_nSortColumn >= 0)
  then lvTreeItems.AlphaSort;
end;

function TTreeItemPickList.CheckSelection: boolean;
var
  nIdx :integer;
begin
  Result := true;

  if(Assigned(m_evOnCheckTreeItemEvent))
  then for nIdx := 0 to lvTreeItems.SelCount - 1 do
  begin
      m_evOnCheckTreeItemEvent(GetASelectedTreeItem(nIdx), Result);
      if(not Result)
      then break;
  end;
end;

function TTreeItemPickList.CompareSemiNumeric(const S1, S2: string): integer;
var
  bIsFloat1, bIsFloat2 : boolean;
begin
  bIsFloat1 := IsFloat(S1);
  bIsFloat2 := IsFloat(S2);

       if (S1 = S2) then Result := 0
  else if bIsFloat1 and bIsFloat2             then Result := CompareDouble(strtofloat(S1), strtofloat(S2))
  else if (not bIsFloat1) and (not bIsFloat2) then Result := CompareStr(S1, S2)
                                              else Result := IIF(bIsFloat1, 1, -1);
end;


end.
