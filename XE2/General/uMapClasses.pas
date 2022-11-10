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

unit uMapClasses;

interface

uses
  uDMSInterfaceTypes, Classes;

type
  TInterestCountHolder = class(TList)
    m_FailReason: String;
    m_LastState:  TUpdateState;
    m_LastStateTS: TTimeStamp;

    procedure SetInterestCount(ti: TTreeItem);
    procedure ClrInterestCounts;

    constructor Create;
    destructor Destroy; override;
  end;

implementation

uses SysUtils,
 uAppGeneral, uDMSUtils, uGenLib,
 uDmsInterface,
 Configuration, dmGeneral;

//========================================= TInterestHolder

constructor TInterestCountHolder.Create;
begin
  m_LastStateTS := 0;
end;

destructor TInterestCountHolder.Destroy;
begin
  ClrInterestCounts;
end;

procedure TInterestCountHolder.SetInterestCount(ti: TTreeItem);
begin
  if not Assigned(ti) then exit;
  if IndexOf(ti) >= 0 then exit;
  m_LastStateTS := 0;
  DMS_TreeItem_IncInterestCount(ti);
  Add(ti);
end;

procedure TInterestCountHolder.ClrInterestCounts;
var i: Integer;
begin
  for i:=0 to Count-1 do
    DMS_TreeItem_DecInterestCount(Items[i]);
  Clear;
  m_LastStateTS := 0;
end;


end.
