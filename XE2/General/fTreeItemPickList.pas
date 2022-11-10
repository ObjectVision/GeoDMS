unit fTreeItemPickList;

interface

uses
  Forms, Classes, Controls, StdCtrls,
   TreeItemPickList, ItemColumnProvider,
   uDMSInterfaceTypes, ExtCtrls, ComCtrls
  ;

type
  TNewTreeItemEvent = procedure(var tiNew: TTreeItem; var bAdd: boolean) of object;

  TfrmTreeItemPickList = class(TForm)
    btnNew: TButton;
    btnOK: TButton;
    btnCancel: TButton;
    pnlItemList: TPanel;
    StatusBar1: TStatusBar;
    procedure btnNewClick (Sender: TObject);
    procedure plSelChanged(Sender: TObject);
    procedure plDblClick  (Sender: TObject);
    procedure btnOKClick  (Sender: TObject);
    procedure FormCreate  (Sender: TObject);
    procedure FormDestroy (Sender: TObject);

  public
    pickList: TTreeItemPickList;
  private
    m_evOnNewTreeItemEvent  : TNewTreeItemEvent;
    m_evOnCheckTreeItemEvent: TCheckTreeItemEvent;

    procedure SetNewTreeItemEvent(evValue: TNewTreeItemEvent);

    function GetMultiSelect: boolean;
    procedure SetMultiSelect(bValue : boolean);
    function GetSelCount: integer;
    function GetTheSelectedTreeItem: TTreeItem;
    function GetASelectedTreeItem(nIdx : integer): TTreeItem;

    procedure InitializeWidth;
  public
    { Public declarations }
    procedure AddColumn(cp: TAbstrItemColumnProvider);
    procedure AddRows(const lstTreeItems : TList; tiSelected: TTreeItem);
    function Execute: boolean;

    property MultiSelect : boolean read GetMultiSelect write SetMultiSelect;
    property SelCount : integer read GetSelCount;
    property SelectedTreeItem : TTreeItem read GetTheSelectedTreeItem;
    property SelectedTreeItemList[nIdx : integer] : TTreeItem read GetASelectedTreeItem;

    property OnCheckTreeItem: TCheckTreeItemEvent read m_evOnCheckTreeItemEvent write m_evOnCheckTreeItemEvent;
    property OnNewTreeItem: TNewTreeItemEvent read m_evOnNewTreeItemEvent write SetNewTreeItemEvent;
  end;

implementation

{$R *.DFM}

uses
  Windows,
  uGenLib, uDMSUtils, fEditProperty, dmGeneral, fMain, Configuration;

procedure TfrmTreeItemPickList.FormCreate(Sender: TObject);
begin
  assert(Assigned(pnlItemList));
  pickList := TTreeItemPickList.Create(pnlItemList);
  pickList.OnSelChange := plSelChanged;
  pickList.OnDblClick  := plDblClick;
  pickList.Parent := pnlItemList;
  pickList.Width := pnlItemList.Width;
  pickList.Height := pnlItemList.Height;
  pickList.Align := alClient;

  m_evOnCheckTreeItemEvent := nil;
  m_evOnNewTreeItemEvent := nil;

  dmfGeneral.RegisterCurrForm(self);
end;

procedure TfrmTreeItemPickList.FormDestroy(Sender: TObject);
begin
  dmfGeneral.UnRegisterCurrForm(self);

  pnlItemList.Free;
end;

procedure TfrmTreeItemPickList.AddColumn(cp: TAbstrItemColumnProvider);
begin
  pickList.AddColumn(cp);
end;

procedure TfrmTreeItemPickList.AddRows(const lstTreeItems : TList; tiSelected: TTreeItem);
begin
  pickList.AddRows(lstTreeItems, tiSelected);
end;

function TfrmTreeItemPickList.Execute: boolean;
begin
  InitializeWidth;
  Result := (ShowModal = mrOK);
end;

procedure TfrmTreeItemPickList.SetNewTreeItemEvent(evValue: TNewTreeItemEvent);
begin
  m_evOnNewTreeItemEvent := evValue;
  btnNew.Visible := Assigned(m_evOnNewTreeItemEvent);
end;


function TfrmTreeItemPickList.GetMultiSelect: boolean;
begin
  Result := pickList.lvTreeItems.MultiSelect;
end;

procedure TfrmTreeItemPickList.SetMultiSelect(bValue : boolean);
begin
  pickList.lvTreeItems.MultiSelect := bValue;
end;


function TfrmTreeItemPickList.GetSelCount: integer;
begin
  Result := pickList.SelCount;
end;

function TfrmTreeItemPickList.GetTheSelectedTreeItem: TTreeItem;
begin
  Result := pickList.SelectedTreeItem;
end;

function TfrmTreeItemPickList.GetASelectedTreeItem(nIdx : integer): TTreeItem;
begin
  Result := pickList.SelectedTreeItemList[nIdx];
end;

procedure TfrmTreeItemPickList.InitializeWidth;
var
  nIdx, nWidth: integer;
begin
  nWidth := GetSystemMetrics(SM_CXVSCROLL);
  with pickList.lvTreeItems do
  begin
    for nIdx := 0 to Columns.Count - 1 do
      inc(nWidth, Columns[nIdx].Width);
    dec(nWidth, ClientWidth);
  end;

  if (nWidth > 0)
  then Width := Width + nWidth;
end;

procedure TfrmTreeItemPickList.btnNewClick(Sender: TObject);
var
  tiNew : TTreeItem;
  bAdd  : boolean;
begin
  assert( Assigned( m_evOnNewTreeItemEvent) );
  tiNew := nil;
  m_evOnNewTreeItemEvent(tiNew, bAdd);

  if bAdd and Assigned(tiNew) then
      pickList.AddRow(tiNew, true);
end;

procedure TfrmTreeItemPickList.plSelChanged(Sender: TObject);
begin
  btnOK.Enabled := pickList.SelCount > 0;
end;

procedure TfrmTreeItemPickList.plDblClick(Sender: TObject);
begin
  btnOKClick(sender);
end;


procedure TfrmTreeItemPickList.btnOKClick(Sender: TObject);
begin
  if pickList.CheckSelection
  then ModalResult := mrOK;
end;

end.
