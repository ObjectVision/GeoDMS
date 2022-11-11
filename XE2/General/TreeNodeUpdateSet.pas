unit TreeNodeUpdateSet;

interface

uses Windows, ComCtrls, Classes, uDmsInterfaceTypes, uMapClasses;

type TTreeNodeUpdateSet = class
  private
    m_TreeView:                TTreeView;
    m_SetNames:                Boolean;
    m_TreeNodeCollector:       TList;
    m_nProcessed, m_nScanned:  Cardinal;
    m_bTreeNodeSetInvalidated: Boolean;
    m_ShowValue:               Boolean;
    m_nRecNo, m_nDenom:        SizeT;
    m_Interests:               TInterestCountHolder;

    procedure Update(var done: Boolean);
    procedure UpdateNode(tn: TTreeNode; var done: Boolean);
    procedure _InvalidateRect(tn: TTreeNode; bTextOnly: Boolean);
    procedure SetRecNo(_recNo: SizeT);
    procedure SetDenom(_denom: SizeT);
  public
    constructor Create(tv: TTreeView; setNames: Boolean);
    destructor  Destroy; override;

    procedure   Reset(doRescan: Bool);
    procedure   Invalidate;
    procedure   InitRoot(tiRoot: TTreeItem);

    procedure OnDrawNode(tn: TTreeNode);
    procedure OnCollapse;

    class procedure OnStateChange(item: TTreeItem; newState: TUpdateState);
    class procedure UpdateTreeNodeSets(var done: Boolean);

    property RecNo: SizeT read m_nRecNo write SetRecNo;
    property Denom: SizeT read m_nDenom write SetDenom;
end;

implementation

uses SysUtils, uAppGeneral, dmGeneral, uDmsInterface
{$IFDEF DEBUG}
  ,fMain, uDmsUtils // DEBUG
{$ENDIF}
;

var s_AllSets: TList = Nil;
    s_bDispatchStateChange: Boolean = false;

constructor TTreeNodeUpdateSet.Create(tv: TTreeView; setNames: Boolean);
begin
    inherited Create;
    if not Assigned(s_AllSets) then
    begin
        s_AllSets := TList.Create;
    end;
    s_AllSets.Add(self);

    m_TreeView                   := tv;
    m_SetNames                   := setNames;
    m_TreeNodeCollector          := TList.Create;
    m_TreeNodeCollector.Capacity := 100;
    m_nProcessed                 := 0;
    m_nScanned                   := 0;
    m_bTreeNodeSetInvalidated    := false;
    m_nRecNo                     := DMS_SIZET_UNDEFINED;
    m_nDenom                     := DMS_SIZET_UNDEFINED;
    m_Interests                  := TInterestCountHolder.Create;
    m_ShowValue                  := false;
end;

destructor  TTreeNodeUpdateSet.Destroy;
begin
    m_Interests.Free;
    m_TreeNodeCollector.Free;
    Assert(Assigned(s_AllSets));
    s_AllSets.Remove(self);
    if s_AllSets.Count = 0 then
    begin
      FreeAndNil(s_AllSets);
    end;
    inherited;
end;

procedure TTreeNodeUpdateSet.InitRoot(tiRoot: TTreeItem);
begin
  Invalidate;
  m_TreeView.Items.Clear;

  TreeView_FillPartial(m_TreeView, nil, tiRoot);
  if (m_TreeView.Items.Count > 0) then
     m_TreeView.Items[0].Selected := true
  else
    dmfGeneral.SetActiveTreeItem(Nil, m_TreeView);
end;

procedure TTreeNodeUpdateSet.SetRecNo(_recNo: SizeT);
begin
  if (m_nRecNo = _recNo) then exit;
  m_nRecNo := _recNo;
  Invalidate;
end;

procedure TTreeNodeUpdateSet.SetDenom(_denom: SizeT);
begin
  if (m_nDenom = _denom) then exit;
  m_nDenom := _denom;
  Invalidate;
end;


procedure TTreeNodeUpdateSet.OnDrawNode(tn: TTreeNode);
var recNo: SizeT;
begin
  if not Assigned(tn.Data) then exit;

  if m_TreeNodeCollector.IndexOf( tn) = -1 then
  begin
    m_TreeNodeCollector.Add(tn);
    recNo := DMS_SIZET_UNDEFINED;
    if m_ShowValue
    and IsParameterOrAddrAttr(tn.Data, recNo)
    and (DMS_TreeItem_GetProgressState(tn.Data) >= NC2_DataReady)
    then m_Interests.SetInterestCount(tn.Data);
  end
end;

procedure TTreeNodeUpdateSet.OnCollapse;
begin
  if m_TreeNodeCollector.Count > 0 then
  Invalidate;
end;

class procedure TTreeNodeUpdateSet.OnStateChange(item: TTreeItem; newState: TUpdateState);
var i: Cardinal;
begin
  Assert(Assigned(s_AllSets));

  if (not dmfGeneral.AdminMode) and DMS_TreeItem_InHidden(item) then exit;

  if (newState = NC_Deleting) then
  begin
{$IFDEF DEBUG}
 //   g_frmMain.LogMsg(PMsgChar('NC_Deleting ' +TreeItem_GetFullName_save(item))); // DEBUG
{$ENDIF}
    i := s_AllSets.Count;
    while (i>0) do
    begin
      DEC(i);
      TreeView_FindAssTreeNode(TTreeNodeUpdateSet(s_AllSets.Items[i]).m_TreeView, item).Free;
    end;
  end;

  s_bDispatchStateChange := true;
end;

class procedure TTreeNodeUpdateSet.UpdateTreeNodeSets(var done: Boolean);
var i: Cardinal;
begin
  Assert(Assigned(s_AllSets));
  if s_bDispatchStateChange then
  begin
    i := s_AllSets.Count;
    while (i>0) do
    begin
      DEC(i);
      TTreeNodeUpdateSet(s_AllSets.Items[i]).Invalidate;
    end;
    s_bDispatchStateChange := false;
  end;

  i := s_AllSets.Count;
  while (i>0) do
  begin
    DEC(i);
    TTreeNodeUpdateSet(s_AllSets.Items[i]).Update(done);
    if not done then exit;
  end;
end;


procedure TTreeNodeUpdateSet.Reset(doRescan: Bool);
var oldInterests: TInterestCountHolder;
begin
  m_TreeNodeCollector.Count := 0;
  m_nProcessed              := 0;
  m_nScanned                := 0;
  oldInterests              := m_Interests;
  m_Interests               := TInterestCountHolder.Create;
  try
    if (doRescan) then
    begin
      m_TreeView.Invalidate;
      m_TreeView.Update; // repaint without leaving messages in queue and collect invalidated TreeNodes in view.
    end;
    m_bTreeNodeSetInvalidated := false;
  finally
    oldInterests.free;
    assert(assigned(m_Interests));
  end;
end;

procedure TTreeNodeUpdateSet.Invalidate;
begin
    m_bTreeNodeSetInvalidated := true;
end;

function TreeNode_SetText(tn: TTreeNode; txt: ItemString): Boolean;
begin
  if Length(txt) > 250 then txt := Copy(txt,1, 250) + '...';
  Result := (tn.text <> txt);
  if Result then tn.text := txt;
end;

procedure TTreeNodeUpdateSet._InvalidateRect(tn: TTreeNode; bTextOnly: Boolean);
var rect: TRect;
begin
   rect := tn.DisplayRect(bTextOnly);
   InvalidateRect(m_TreeView.Handle, @rect, not bTextOnly);
end;

procedure TTreeNodeUpdateSet.UpdateNode(tn: TTreeNode; var done: Boolean);
begin
  Assert(tn.IsVisible);
  Assert(Assigned(tn.Data));
  Assert(tn.TreeView = m_TreeView);

  if  m_SetNames
  and TreeNode_SetText(tn, FormatTreeNodeText(tn.Data, RecNo, Denom, m_ShowValue, done))
  then _InvalidateRect(tn, true);

//if not done then exit; // liever rest afmaken om op Calculating te zetten
  if TreeNode_SetImage(tn, tn.Data)
  then _InvalidateRect(tn, false);
end;

procedure TTreeNodeUpdateSet.Update(var done: Boolean);
begin
  if (m_bTreeNodeSetInvalidated) then Reset(true);
  if (s_bDispatchStateChange) then
  begin
    done := false;
    exit;
  end;

//  Assert(m_nProcessed <= m_nScanned);
  Assert(m_TreeNodeCollector.Count >= 0);
  Assert(m_nScanned <= UInt32(m_TreeNodeCollector.Count));

  if (m_TreeNodeCollector.Count = 0) then exit;

  try
    while (m_nProcessed < UInt32(m_TreeNodeCollector.Count)) and done do
    begin
      INC(m_nProcessed); // make shure progress is made, even when an exception is raised.
      UpdateNode(TTreeNode(m_TreeNodeCollector.Items[m_nProcessed-1]), done);
      if not done then
        DEC(m_nProcessed); // rollback when suspended
      if DMS_Actor_MustSuspend then
        done := false;
    end;
    if (m_nScanned < m_nProcessed) then m_nScanned := m_nProcessed;
    while (m_nScanned < UInt32(m_TreeNodeCollector.Count)) do
    begin
      INC(m_nScanned); // make shure progress is made, even when an exception is raised.
      UpdateNode(TTreeNode(m_TreeNodeCollector.Items[m_nScanned-1]), done);
    end;

  finally
    m_TreeView.Update; // repaint without leaving messages in queue and collect invalidated TreeNodes in view.
    Assert(m_TreeNodeCollector.Count >= 0);
    if m_nProcessed < UInt32(m_TreeNodeCollector.Count)
    then done := false; // maybe new TreeNodes have been added
  end;
  if (done) then
  begin
    Assert(m_nProcessed = UInt32(m_TreeNodeCollector.Count));
    Assert(m_nScanned   = m_nProcessed);
    m_TreeNodeCollector.Count := 0;
    m_nProcessed              := 0;
    m_nScanned                := 0;
  end;
end;

end.
