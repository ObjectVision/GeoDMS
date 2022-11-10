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

unit Configuration;

interface

uses
  uDMSInterfaceTypes, uDMSDesktopFunctions, Inifiles, Classes, Graphics;

type
  TConfiguration = class
  private
    m_sConfigurationFile: FileString;
    m_tiRoot,
    m_tiConfigSettings : TTreeItem;
    m_dmThis : TDesktopManager;
    m_iniThis : TInifile;

    m_clrBackground, m_clrValid, m_clrInvalidated,
    m_clrExogenic, m_clrOperator, m_clrFailed, m_clrRampStart, m_clrRampEnd: TColor;

    function MakeOrgConfigPath: FileString;

    function GetProjDirPath : FileString;
    function GetResultsDir : FileString;
    procedure AuxClose;
    function ConfigIniOpen: Boolean;
    procedure LogOpen;
    function AuxOpen: Boolean;


    procedure SetClrBackground(clr: TColor);
    procedure SetClrValid(clr: TColor);
    procedure SetClrInvalidated(clr: TColor);
    procedure SetClrFailed(clr: TColor);
    procedure SetClrExogenic(clr: TColor);
    procedure SetClrOperator(clr: TColor);
    procedure SetClrRampStart(clr: TColor);
    procedure SetClrRampEnd(clr: TColor);
    procedure SetFlushThreshold(percent: UInt32);
    function  GetFlushThreshold: UInt32;
    procedure SetSwapfileMinSize(sz: UInt32);
    function  GetSwapfileMinSize: UInt32;

    function  GetOrCreateContainerSec(name: PItemChar): TTreeItem;

  public
    constructor Create(const sConfigName: string);
    destructor  Destroy; override;

    function GetConfigDir: Filestring;
    function GetResultsPath: FileString; // returns the full path
    function Init: boolean;

    function  ReadUserUInt32 (section, key: PItemChar; defaultValue: UInt32): UInt32;
    procedure WriteUserUInt32(section, key: PItemChar; value: UInt32);

    property ConfigurationFile : FileString read m_sConfigurationFile;
    property DesktopManager : TDesktopManager read m_dmThis;
    property Inifile : TInifile read m_iniThis;
    property Root : TTreeItem read m_tiRoot;
    property ConfigSettings: TTreeItem read m_tiConfigSettings;

    property ClrBackground : TColor read m_clrBackground  write SetClrBackground;
    property ClrValid      : TColor read m_clrValid       write SetClrValid;
    property ClrInvalidated: TColor read m_clrInvalidated write SetClrInvalidated;
    property ClrFailed     : TColor read m_clrFailed      write SetClrFailed;
    property ClrExogenic   : TColor read m_clrExogenic    write SetClrExogenic;
    property ClrOperator   : TColor read m_clrOperator    write SetClrOperator;
    property ClrRampStart  : TColor read m_clrRampStart   write SetClrRampStart;
    property ClrRampEnd    : TColor read m_clrRampEnd     write SetClrRampEnd;
    property SwapFileMinSize: UInt32 read GetSwapfileMinSize write SetSwapfileMinSize;
    property FlushThreshold: UInt32 read GetFlushThreshold write SetFlushThreshold;

    class function CurrentConfiguration: TConfiguration;
    class function LoadConfiguration(const sConfigName: FileString): TConfiguration;
    class function CloseCurrentConfiguration: boolean;
    class function CurrentIsAssigned: boolean;
    class function PreGetDMSProjectDllName(const sConfigname: FileString; var isD5Dll: Boolean): FileString;
    class function CurrentIsDirty: boolean;
  end;

implementation

uses
  SysUtils, dmGeneral, uAppGeneral, Windows, uDmsInterface, uDmsRegistry,
  Forms, fmain, Dialogs, uDMSUtils, uGenLib, CommDlg, OptionalInfo;

const g_cfgCurrent : TConfiguration = nil;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                               CLASS FUNCTIONS                              //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

class function TConfiguration.CurrentConfiguration: TConfiguration;
begin
  if(g_cfgCurrent = nil)
  then
    raise Exception.Create('No configuration is loaded.');
  Result := g_cfgCurrent;
end;

function FullKey(section, key: PItemChar): ItemString;
begin
  Result := StringReplace(section + '/' + key, ' ', '', [rfReplaceAll]);
end;

function TConfiguration.ReadUserUInt32(section, key: PItemChar; defaultValue: UInt32): UInt32;
begin
  try
    with TDmsRegistry.Create do
    try
      if OpenKeyReadOnly(Section)
      then try
        if ValueExists(key) then
        begin
           Result := ReadInteger(key);
           exit
        end;
      finally
        CloseKey;
      end;
    finally
      Free;
    end;
  except on E: Exception do
    AppLogMessage(E.ClassName + ': ' + E.Message);
  end;
  Result := IniFile.ReadInteger(section, key, defaultValue); // Backward compatibility, REMOVE
//  Result := defaultValue
end;

procedure TConfiguration.WriteUserUInt32(section, key: PItemChar; value: UInt32);
begin
  try
    with TDmsRegistry.Create do
    try
      if OpenKey(section)
      then try
         WriteInteger(key, value);
      finally
        CloseKey;
      end;
    finally
      Free;
    end;
  except on E: Exception do
    AppLogMessage(E.ClassName + ': ' + E.Message);
  end;
end;

class function TConfiguration.LoadConfiguration(const sConfigName: FileString): TConfiguration;
begin
  AppLogMessage('TConfiguration.LoadConfiguration: ' + sConfigName + ' in ' + GetCurrentDir);

  if(g_cfgCurrent <> nil) then g_cfgCurrent.Free;
  Result := Nil;
  g_cfgCurrent := TConfiguration.Create(sConfigName);
  try
    AppLogMessage('created config');

    if g_cfgCurrent.Init
    then with g_cfgCurrent do begin
        m_clrBackground  := ReadUserUInt32('Colors', 'Background',  $FFFFFF);
        m_clrValid       := ReadUserUInt32('Colors', 'Valid',       clBlue);
        m_clrInvalidated := ReadUserUInt32('Colors', 'Invalidated', $00FF00FF); // magenta $000080FF; // orange
        m_clrFailed      := ReadUserUInt32('Colors', 'Failed',      clRed);
        m_clrExogenic    := ReadUserUInt32('Colors', 'Exogenic',    clGreen);
        m_clrOperator    := ReadUserUInt32('Colors', 'Operator',    clBlack);
        m_clrRampStart   := ReadUserUInt32('Colors', 'RampStart',   clBlue);
        m_clrRampEnd     := ReadUserUInt32('Colors', 'RampEnd',     clRed);

        STG_Bmp_SetDefaultColor(256, ClrBackGround);
        STG_Bmp_SetDefaultColor(257, m_clrRampStart);
        STG_Bmp_SetDefaultColor(258, m_clrRampEnd);

        g_cValid       := m_clrValid;
        g_cInvalidated := m_clrInvalidated;
        g_cFailed      := m_clrFailed;
        g_cExogenic    := m_clrExogenic;
        g_cOperator    := m_clrOperator;
        AppLogMessage('init ok');
        Result := g_cfgCurrent;
    end;
  finally if not Assigned(Result) then
    begin
      AppLogMessage('init failed');

      g_cfgCurrent.Free;
      g_cfgCurrent := nil;
    end
  end
end;

class function TConfiguration.CloseCurrentConfiguration: boolean;
begin
  Result := Assigned(g_cfgCurrent);

  g_cfgCurrent.Free;
  g_cfgCurrent := nil;
end;

class function TConfiguration.CurrentIsAssigned: boolean;
begin
  Result := g_cfgCurrent <> nil;
end;

class function TConfiguration.CurrentIsDirty: boolean;
begin
  Result := (g_cfgCurrent <> nil) and DMS_IsConfigDirty(g_cfgCurrent.m_tiRoot);
end;

class function TConfiguration.PreGetDMSProjectDllName(const sConfigname: FileString; var isD5Dll: Boolean): FileString;
var
  sConfigDirName : FileString;
  sConfigIniName : FileString;
  iniConfig : TInifile;
  sDll : FileString;
  nCnt : integer;
  pcDotPos: PFileChar;
begin
  isD5Dll := false;
  sDll := '';
  Result := '';
  pcDotPos := StrRScan(PFileChar(sConfigname), '.');
  if pcDotPos = Nil then exit;
  nCnt := pcDotPos - PFileChar(sConfigname);
  sConfigDirName := copy(sConfigname, 1, nCnt);
  sConfigIniName := GetCurrentDir + '\' + sConfigDirName + '\config.ini';
  iniConfig := TInifile.Create( sConfigIniName );
  if(iniConfig <> nil)
  then try
    sDll := iniConfig.ReadString('general', 'DMSProjectDLL', '%projdir%/bin/DmsProjectXE.dll');
  finally
    iniConfig.Free;
  end;
  sDll := Config_GetFullStorageName(sConfigDirName, sDll);

  if (sDll = '') or not FileExists(sDll)
  then
  begin
    sDll := GetCurrentDir + '\..\bin\DMSProjectXE.dll'; // currentdir is ...\prj_name\cfg
    if not FileExists(sDll)
    then
    begin
      sDll    := GetCurrentDir + '\..\bin\DMSProject.dll'; // currentdir is ...\prj_name\cfg
      isD5Dll := true;
    end
  end;

  Result := sDll;
end;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                              INSTANCE FUNCTIONS                            //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

constructor TConfiguration.Create(const sConfigName: string);
begin
  m_sConfigurationFile := sConfigName;
  m_tiRoot := nil;
  m_tiConfigSettings := nil;
  m_dmThis := nil;
  m_iniThis := nil;
end;

destructor TConfiguration.Destroy;
var tiRoot: TTreeItem;
begin
  Assert( not Assigned( dmfGeneral.GetActiveTreeItem() ) );
  if(m_dmThis <> nil)
  then begin
    m_dmThis.CloseCurrentDesktop;
    m_dmThis.Free;
  end;

  try
    tiRoot := m_tiRoot;
    SetCountedRef(m_tiConfigSettings, nil);
    SetCountedRef(m_tiRoot, nil);
    if Assigned(tiRoot) then
      DMS_TreeItem_SetAutoDelete(tiRoot);
  finally
    AuxClose;
    inherited;
  end
end;

procedure TConfiguration.AuxClose;
begin
  m_iniThis.Free;
  m_iniThis := Nil;
  LogFileClose;
end;

function TConfiguration.ConfigIniOpen: Boolean;
var
  sFilename : FileString;
begin
  AuxClose;
  sFilename := GetCurrentDir + '\' + GetConfigDir + '\config.ini';
  m_iniThis := TIniFile.Create(sFilename);
  Result := Assigned(m_iniThis);
  if not Result then begin
    DispMessage('Failed to open config.ini:' + #13#10 + sFilename);
    Exit;
  end;
end;

procedure TConfiguration.LogOpen;
begin
  if dmfGeneral.GetStatus(SF_TraceLogFile) then
    LogFileOpen(Config_GetFullStorageName(GetConfigDir, '%localDataProjDir%/trace.log'));
end;

function TConfiguration.AuxOpen: Boolean;
begin
   Result := ConfigIniOpen;
   if Result then
     LogOpen;
end;

function TConfiguration.GetOrCreateContainerSec(name: PItemChar): TTreeItem;
begin
   Result := DMS_TreeItem_GetItem(m_tiRoot, name, nil);
   if Assigned(Result) then exit;
   Result := DMS_CreateTreeItem(m_tiRoot, name);
   if(Result = nil)
     then DispMessage('The '+name+' container could not be found nor created.')
     else DMS_TreeItem_SetIsHidden(Result, true);
end;

procedure TrimLeadingSlashes(var fullName: ItemString);
begin
    while (length(fullName) > 0) and (fullName[1] = '/') do
      fullName := copy(fullName, 2, length(fullname)-1);
end;

function TConfiguration.Init: boolean;
begin
  Result := false;

  Assert(not Assigned(m_tiRoot));
  Assert(not Assigned(m_tiConfigSettings));

  if(not FileExists(m_sConfigurationFile))
  then begin
    DispMessage('File does not exist:' + #13#10 + m_sConfigurationFile);
    Exit;
  end;

  if not AuxOpen then Exit;

  SetCountedRef(m_tiRoot, DMS_CreateTreeFromConfiguration(PFileChar(m_sConfigurationFile)));

  if not Assigned(m_tiRoot)
  then begin
    DispMessage('Failed to read the configurationfile:' + #13#10 + m_sConfigurationFile);
    Exit;
  end;

  SetCountedRef(m_tiConfigSettings, DMS_TreeItem_GetItem(m_tiRoot, 'ConfigSettings', nil));

  m_dmThis := TDesktopManager.Create;
  if not m_dmThis.Init(m_tiRoot, Application.MainForm)
  then begin
    m_dmThis.Free;
    DispMessage('Failed to initialize the desktopmanager.');
    Exit;
  end;

  Result := true;
end;

function TConfiguration.MakeOrgConfigPath: FileString;
var sProjDir: FileString;
begin
  // assume current config is original
  Result := AsLcWinPath(m_sConfigurationFile);

  // replace %projDir% in sOrgConfig to make it relative
  sProjDir := AsLcWinPath(GetProjDirPath)+'\';
  if Copy(Result, 1, length(sProjDir)) = sProjDir then
    Result := '%projdir%\'+Copy(Result, length(sProjDir) + 1, length(Result)-Length(sProjDir));
end;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                              PROPERTY FUNCTIONS                            //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function TConfiguration.GetConfigDir: FileString;
begin
  Result := copy(m_sConfigurationFile, 1, pos('.dms', lowercase(m_sConfigurationFile)) - 1);
end;


function TConfiguration.GetResultsDir: FileString;
begin
  Result := '%LocalDataProjDir%/Results/%configName%';
end;

function TConfiguration.GetResultsPath: FileString;
begin
   Result := Config_GetFullStorageName(GetConfigDir, GetResultsDir);
end;

function TConfiguration.GetProjDirPath: FileString;
begin
   Result := Config_GetFullStorageName(GetConfigDir, '%projdir%');
end;


procedure TConfiguration.SetClrBackground(clr: TColor);
begin
  if m_clrBackground = clr then exit;
  m_clrBackground := clr;
  STG_Bmp_SetDefaultColor(256, clr);
  WriteUserUInt32('Colors', 'Background',      clr);
end;

procedure TConfiguration.SetClrValid(clr: TColor);
begin
  if m_clrValid=clr then exit;
  m_clrValid:=clr;
  g_cValid:=clr;
  WriteUserUInt32('Colors', 'Valid',      clr);
end;

procedure TConfiguration.SetClrInvalidated(clr: TColor);
begin
  if m_clrInvalidated=clr then exit;
  m_clrInvalidated:=clr;
  g_cInvalidated:=clr;
  WriteUserUInt32('Colors', 'Invalidated',      clr);
end;

procedure TConfiguration.SetClrFailed      (clr: TColor);
begin
  if m_clrFailed=clr then exit;
  m_clrFailed:=clr;
  g_cFailed:=clr;
  WriteUserUInt32('Colors', 'Failed',      clr);
end;

procedure TConfiguration.SetClrExogenic    (clr: TColor);
begin
  if m_clrExogenic=clr then exit;
  m_clrExogenic:=clr;
  g_cExogenic:=clr;
  WriteUserUInt32('Colors', 'Exogenic',      clr);
end;

procedure TConfiguration.SetClrOperator    (clr: TColor);
begin
  if m_clrOperator=clr then exit;
  m_clrOperator:=clr;
  g_cOperator:=clr;
  WriteUserUInt32('Colors', 'Operator',      clr);
end;

procedure TConfiguration.SetClrRampStart   (clr: TColor);
begin
  if m_clrRampStart = clr then exit;
  m_clrRampStart := clr;
  STG_Bmp_SetDefaultColor(257, m_clrRampStart);
  WriteUserUInt32('Colors', 'RampStart',      clr);
end;

procedure TConfiguration.SetClrRampEnd     (clr: TColor);
begin
  if m_clrRampEnd = clr then exit;
  m_clrRampEnd := clr;
  STG_Bmp_SetDefaultColor(258, m_clrRampEnd);
  WriteUserUInt32('Colors', 'RampEnd',      clr);
end;

procedure TConfiguration.SetFlushThreshold(percent: UInt32);
begin
  RTC_SetRegDWord(0, percent);
  WriteUserUInt32('', 'MemoryFlushThreshold', percent);
end;

function  TConfiguration.GetFlushThreshold: UInt32;
begin
  Result := RTC_GetRegDWord(0);
end;

procedure TConfiguration.SetSwapfileMinSize(sz: UInt32);
begin
  RTC_SetRegDWord(1, sz);
  WriteUserUInt32('', 'SwapfileMinSize', sz);
end;

function  TConfiguration.GetSwapfileMinSize: UInt32;
begin
  Result := RTC_GetRegDWord(1);
end;

end.

