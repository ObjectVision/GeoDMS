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

unit uDmsRegistry;

interface
uses Registry;

type
TDmsRegistry = class(TObject)
  m_ImplLM: TRegistry;
  m_ImplCU: TRegistry;

  constructor Create;
  destructor  Destroy; override;

  function OpenKey(const section: String): boolean;
  function OpenKeyReadOnly(const section: String): boolean;
  function ValueExists(const key: string): Boolean;
  function ReadInteger(const key: string): Integer;
  function ReadString(const key: string): string;
  procedure WriteInteger(const key: string; value: Integer);
  procedure WriteString (const key: string; value: string);
  procedure DeleteValue(const key: string);
  procedure CloseKey;
end;

implementation

uses windows;

constructor TDmsRegistry.Create;
begin
  m_ImplLM := TRegistry.Create;
  m_ImplLM.Rootkey := HKEY_CURRENT_USER;
  m_ImplCU := TRegistry.Create;
  m_ImplCU.Rootkey := HKEY_CURRENT_USER;
end;

destructor TDmsRegistry.Destroy;
begin
  m_ImplCU.Destroy;
  m_ImplLM.Destroy;
end;

var ComputerName: string;

function GetComputerNameImpl: string;
var buffer: array[0..1000] of AnsiChar;
 size: DWORD;
begin
  size := GetEnvironmentVariableA('COMPUTERNAME', addr(buffer), 1000);
  Result := buffer;
  assert(size < 1000);
  assert(length(Result) = size);
end;

function GetComputerName: string;
begin
  if ComputerName = '' then
    ComputerName := GetComputerNameImpl;
  Result := ComputerName;
end;

function GetLmKey(const section: string): string;
begin
  Result := '\Software\ObjectVision\'+ GetComputerName+'\GeoDMS';
  if section <> '' then
    Result := Result + '\' + section;
end;

function GetCuKey(const section: string): string;
begin
  Result := '\Software\ObjectVision\DMS';
  if section <> '' then
    Result := Result + '\' + section;
end;

function TDmsRegistry.OpenKey(const section: String): boolean;
begin
  m_ImplCU.CloseKey;
  Result := m_ImplLM.OpenKey(GetLmKey(section), true);
end;

function TDmsRegistry.OpenKeyReadOnly(const section: String): boolean;
var b1: Boolean;
begin
  b1     := m_ImplLM.OpenKeyReadOnly(GetLmKey(section));
  result := m_ImplCU.OpenKeyReadOnly(GetCuKey(section)) or B1;
end;

procedure TDmsRegistry.CloseKey;
begin
  m_ImplLM.CloseKey;
  m_ImplCU.CloseKey;
end;

function TDmsRegistry.ValueExists(const key: string): Boolean;
begin
  if m_ImplLM.ValueExists(key) then
    Result := m_ImplLM.GetDataAsString(key) <> '#DELETED#'
  else
    Result := m_ImplCU.ValueExists(key);
end;

function TDmsRegistry.ReadInteger(const key: string): Integer;
begin
  if m_ImplLM.ValueExists(key)
  then Result := m_ImplLM.ReadInteger(key)
  else Result := m_ImplCU.ReadInteger(key);
end;

function TDmsRegistry.ReadString(const key: string): string;
begin
  if m_ImplLM.ValueExists(key)
  then Result := m_ImplLM.ReadString(key)
  else Result := m_ImplCU.ReadString(key);
end;

procedure TDmsRegistry.WriteInteger(const key: string; value: Integer);
begin
  m_ImplLM.WriteInteger(key, value);
end;

procedure TDmsRegistry.WriteString (const key: string; value: string);
begin
  m_ImplLM.WriteString(key, value);
end;

procedure TDmsRegistry.DeleteValue(const key: string);
begin
  m_ImplLM.WriteString(key, '#DELETED#');
end;

end.

