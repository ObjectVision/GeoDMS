unit RecentFiles;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  Menus;

type
  TRecentFilesLocation = (rflBefore, rflAfter, rflSubmenu);

  TFileOpenEvent = procedure(Filename: string) of object;

  TRecentFiles = class(TComponent)
  private
    { Private declarations }
    FOnFileOpen   : TFileOpenEvent;
    FFilelist     : TStringlist;
    FInifilename  : string;
    FMaxfiles     : integer;
    FMenuItem     : TMenuItem;
    FRegistrykey  : string;
    m_Location    : TRecentFilesLocation;

    function AddMenuItemBefore(const sFilename : string): TMenuItem;
    function AddMenuItemAfter(const sFilename : string): TMenuItem;
    function AddMenuItemSubmenu(const sFilename : string): TMenuItem;

    procedure SetInifilename(const Value : string);
    procedure SetMaxfiles(const Value : integer);
    procedure SetMenuItem(Const Value : TMenuItem);
    procedure SetRegistrykey(const Value : string);

    procedure ReadFromInifile;
    procedure ReadFromRegistry;
    procedure WriteToInifile;
    procedure WriteToRegistry;
    procedure MenuItemClick(Sender: TObject);

    procedure ReadFilenames;
    procedure WriteFilenames;

    procedure DeleteAllMenuItems;
  protected
    { Protected declarations }
    procedure Loaded; override;
  public
    { Public declarations }
    constructor Create(AOwner: TComponent); override;
    destructor  Destroy; override;

    procedure AddFile(const Filename: string);
  published
    { Published declarations }
    property Inifilename  : string         read FInifilename write SetInifilename;
    property Location     : TRecentFilesLocation read m_Location write m_Location default rflAfter;
    property Maxfiles     : integer        read FMaxfiles    write SetMaxfiles;
    property MenuItem     : TMenuItem      read FMenuItem    write SetMenuItem;
    property Registrykey  : string         read FRegistrykey write SetRegistrykey;

    property OnFileOpen   : TFileOpenEvent read FOnFileOpen  write FOnFileOpen;
  end;

procedure Register;

implementation

uses
   Registry, Inifiles;

////////////////////////////////////////////////////////////////////////////////
//                           CONSTRUCTOR / DESTRUCTOR                         //
////////////////////////////////////////////////////////////////////////////////

constructor TRecentfiles.Create(AOwner: TComponent);
begin
   inherited;

   FFilelist := TStringlist.Create;
   m_Location := rflAfter;
end;

destructor TRecentfiles.Destroy;
begin
  try
    WriteFilenames;
  finally
    FFilelist.free;
  end;
  inherited;
end;

procedure TRecentfiles.Loaded;
const
  bFirstCall : boolean = true;
begin
  inherited;

  if(bFirstCall and (not(csDesigning in ComponentState)))
  then begin
    bFirstCall := false;
    ReadFilenames;
  end;
end;


////////////////////////////////////////////////////////////////////////////////
//                             PROPERTY FUNCTIONS                             //
////////////////////////////////////////////////////////////////////////////////

procedure TRecentfiles.SetInifilename(const Value : string);
begin
  if(Value <> FInifilename)
  then begin
    if(not(csDesigning in ComponentState))
    then WriteToInifile;

    FInifilename := Value;
    FRegistrykey := '';

    if(not(csDesigning in ComponentState))
    then begin
      DeleteAllMenuItems;
      ReadFromInifile;
    end;
  end;
end;

procedure TRecentfiles.SetRegistrykey(const Value : string);
begin
   if(Value <> FRegistrykey)
   then begin
     if(not(csDesigning in ComponentState))
     then WriteToRegistry;

     FRegistrykey := Value;
     FInifilename := '';

     if(not(csDesigning in ComponentState))
     then begin
       DeleteAllMenuItems;
       ReadFromRegistry;
     end;
   end;
end;

procedure TRecentfiles.SetMaxfiles(const Value : integer);
begin
  if((Value <> FMaxfiles) AND (Value >= 1))
  then
    FMaxfiles := Value;
end;

procedure TRecentfiles.SetMenuItem(Const Value : TMenuItem);
begin
  if(Value <> FMenuItem)
  then FMenuItem := Value;
end;

////////////////////////////////////////////////////////////////////////////////
//                              PUBLIC FUNCTIONS                              //
////////////////////////////////////////////////////////////////////////////////

procedure TRecentfiles.AddFile(const Filename : string);
var
  miNew : TMenuItem;
  sFilename : string;
  nIdx : integer;
begin
  sFilename := StringReplace(lowercase(Filename), '/', '\', [rfReplaceAll]);

  case m_Location of
    rflBefore  : miNew := AddMenuItemBefore(sFilename);
    rflAfter   : miNew := AddMenuItemAfter(sFilename);
    rflSubmenu : miNew := AddMenuItemSubmenu(sFilename);
  else
    miNew := nil;
  end;

  nIdx := FFilelist.IndexOf(sFilename);
  if(nIdx >= 0)
  then begin
    FFilelist.Objects[nIdx].Free;
    FFilelist.Delete(nIdx);
  end;

  FFilelist.InsertObject(0, sFilename, miNew);
  if(FFilelist.Count > FMaxfiles)
  then begin
    FFilelist.Objects[FFilelist.Count - 1].Free;
    FFilelist.Delete(FFilelist.Count - 1);
  end;

  for nIdx := 0 to FFilelist.Count -1 do
    TMenuItem(FFilelist.Objects[nIdx]).Caption := '&' + inttostr(nIdx + 1) + ' ' + FFilelist[nIdx];
end;

function TRecentfiles.AddMenuItemBefore(const sFilename : string): TMenuItem;
var
  miParent : TMenuItem;
begin
  Result := TMenuItem.Create(self);
  Result.Caption := sFilename;
  Result.OnClick := MenuItemClick;
  miParent := FMenuItem.Parent;
  miParent.Insert(FMenuItem.MenuIndex - FFileList.Count, Result);
end;

function TRecentfiles.AddMenuItemAfter(const sFilename : string): TMenuItem;
var
  miParent : TMenuItem;
begin
  Result := TMenuItem.Create(self);
  Result.Caption := sFilename;
  Result.OnClick := MenuItemClick;
  miParent := FMenuItem.Parent;
  miParent.Insert(FMenuItem.MenuIndex + 1, Result);
end;

function TRecentfiles.AddMenuItemSubmenu(const sFilename : string): TMenuItem;
begin
  Result := TMenuItem.Create(self);
  Result.Caption := sFilename;
  Result.OnClick := MenuItemClick;
  FMenuItem.Insert(0, Result);
end;

////////////////////////////////////////////////////////////////////////////////
//                             PRIVATE FUNCTIONS                              //
////////////////////////////////////////////////////////////////////////////////

procedure TRecentfiles.ReadFilenames;
begin
  if(Inifilename <> '')
  then ReadFromInifile
  else if(Registrykey <> '')
  then ReadFromRegistry;
end;

procedure TRecentfiles.WriteFilenames;
begin
  if(Inifilename <> '')
  then WriteToInifile
  else if(Registrykey <> '')
  then WriteToRegistry;
end;

procedure TRecentfiles.ReadFromInifile;
var
  tmpStr : string;
  i : integer;
begin
  i := 0;
  with TInifile.Create(Inifilename) do
  begin
    tmpStr := ReadString('RECENTFILES','File'+inttostr(i),'');
    while((tmpStr <> '') and (FFilelist.Count < FMaxfiles)) do
    begin
      AddFile(tmpStr);
      inc(i);
      tmpStr := ReadString('RECENTFILES','File'+inttostr(i),'');
    end;
    Free;
  end;
end;

procedure TRecentfiles.ReadFromRegistry;
var
   i : integer;
begin
  i := 0;
  with TRegistry.Create do
  begin
    Rootkey := HKEY_CURRENT_USER;

    if(OpenKey('\software\'+Registrykey,FALSE))
    then begin
      while((ValueExists('File'+inttostr(i))) and (FFilelist.Count < FMaxfiles)) do
      begin
        AddFile(ReadString('File'+inttostr(i)));
        inc(i);
      end;
    CloseKey;
    end;

    Free;
  end;
end;

procedure TRecentfiles.WriteToInifile;
var
  i : integer;
begin
  with TInifile.Create(Inifilename) do
  try
    for i := FFilelist.Count-1 downto 0  do
      WriteString('RECENTFILES','File'+inttostr(i),FFilelist[(FFilelist.Count - 1) - i]);
  finally
    Free;
  end;
end;

procedure TRecentfiles.WriteToRegistry;
var
   i : integer;
begin
  with TRegistry.Create do
  begin
    Rootkey := HKEY_CURRENT_USER;
    if(OpenKey('\software\'+Registrykey,TRUE))
    then begin
      for i := FFilelist.Count-1 downto 0  do
        WriteString('File'+inttostr(i),FFilelist[(FFilelist.Count - 1) - i]);
      CloseKey;
    end;
    Free;
  end;
end;

procedure TRecentfiles.MenuItemClick(Sender: TObject);
var
  nIdx : integer;
begin
  if(Assigned(FOnFileOpen))
  then begin
    nIdx := FFilelist.IndexOfObject(Sender);
    if(nIdx >= 0)
    then begin
      FOnFileOpen(FFilelist[nIdx]);
      AddFile(FFilelist[nIdx]);
    end;
  end;
end;

procedure TRecentfiles.DeleteAllMenuItems;
var
  nIdx : integer;
begin
  for nIdx := 0 to FFilelist.Count - 1 do
    FFilelist.Objects[nIdx].Free;

  FFilelist.Clear;
end;

procedure Register;
begin
  RegisterComponents('Additional', [TRecentFiles]);
end;



end.
