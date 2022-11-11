unit DualListBox;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs, fmDualListBox;

type
  TDualListBox = class(TfraDualListBox)
  private
    { Private declarations }
    function  GetSourceCaption: TCaption;
    procedure SetSourceCaption(const sValue: TCaption);
    function  GetDestinationCaption: TCaption;
    procedure SetDestinationCaption(const sValue: TCaption);
    procedure SetMaxSelection(const nValue: integer);

    procedure SetOnSelectionChangedEvent(evValue: TSelectionChangedEvent);
  protected
    { Protected declarations }
  public
    { Public declarations }
  published
    { Published declarations }
    property SourceCaption : TCaption read GetSourceCaption write SetSourceCaption;
    property DestinationCaption : TCaption read GetDestinationCaption write SetDestinationCaption;
    property MaxSelection : integer read m_nMaxSelection write SetMaxSelection;

    property OnSelectionChanged: TSelectionChangedEvent read m_evOnSelectionChangedEvent write SetOnSelectionChangedEvent;
  end;

procedure Register;

implementation

function TDualListBox.GetSourceCaption: TCaption;
begin
  Result := SrcLabel.Caption;
end;

procedure TDualListBox.SetSourceCaption(const sValue: TCaption);
begin
  SrcLabel.Caption := sValue;
end;

function TDualListBox.GetDestinationCaption: TCaption;
begin
  Result := DstLabel.Caption;
end;

procedure TDualListBox.SetDestinationCaption(const sValue: TCaption);
begin
  DstLabel.Caption := sValue;
end;

procedure TDualListBox.SetMaxSelection(const nValue: integer);
begin
  if((m_nMaxSelection <> nValue) and (nValue >= 1) and (nValue <= 32000))
  then m_nMaxSelection := nValue;
end;

procedure TDualListBox.SetOnSelectionChangedEvent(evValue: TSelectionChangedEvent);
begin
  if(@m_evOnSelectionChangedEvent <> @evValue)
  then m_evOnSelectionChangedEvent := evValue;
end;

procedure Register;
begin
  RegisterComponents('Samples', [TDualListBox]);
end;

end.
