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

unit DMSProjectInterface;

interface

uses
  Classes, Windows, uDmsInterfaceTypes;

type
  TGetMainImageHandle = function: HBitmap; stdcall;
  TGetApplicationIconHandle = function: HIcon; stdcall;
  TDisplayAboutBox = procedure(hApp: HWND; AOwner: TComponent; sDMSVersion, sClientVersion, sUrl: shortstring); stdcall;
  TDisplayOrHideSplashWindow = procedure(hApp: HWND; AOwner: TComponent; sClientVersion: shortstring; bShow: boolean); stdcall;

  TToggleSplash = (tsShow, tsHide);

procedure LoadDMSProjectDll(const sFilename: String; var isD5Dll: Boolean);
procedure UnloadDMSProjectDll;

function GetApplicationIconHandle: HIcon;
procedure DisplayAboutBox(hApp: HWND; AOwner: TComponent; sDMSVersion, sClientVersion: ShortString);
procedure DisplayOrHideSplashWindow(hApp: HWND; AOwner: TComponent; sClientVersion: shortstring; doShow: TToggleSplash);


implementation

uses
  fErrorMsg, uDmsInterface;

const
  g_hDll: THandle = 0;

var
  g_GetMainImageHandle        : TGetMainImageHandle;
  g_GetApplicationIconHandle  : TGetApplicationIconHandle;
  g_DisplayAboutBox           : TDisplayAboutBox;
  g_DisplayOrHideSplashWindow : TDisplayOrHideSplashWindow;


procedure LoadDMSProjectDll(const sFileName: string; var isD5Dll: Boolean);
begin
  UnloadDMSProjectDll;

  g_hDll := LoadLibrary(Pchar(sFilename));

  if(g_hDll <> 0)
  then begin
    @g_GetApplicationIconHandle  := GetProcAddress(g_hDll, 'GetApplicationIconHandle');
    if not isD5Dll then
      @g_DisplayAboutBox         := GetProcAddress(g_hDll, 'DisplayAboutBox');
    @g_DisplayOrHideSplashWindow := GetProcAddress(g_hDll, 'DisplayOrHideSplashWindow');
  end;
end;

procedure UnloadDMSProjectDll;
begin
  @g_GetMainImageHandle        := nil;
  @g_GetApplicationIconHandle  := nil;
  @g_DisplayAboutBox           := nil;
  @g_DisplayOrHideSplashWindow := nil;

  if(g_hDll <> 0)
  then begin
// Problemen met GetMainImage    FreeLibrary(g_hDll);
    g_hDll := 0;
  end;
end;

function GetApplicationIconHandle: HIcon;
begin
  Result := 0;
  if Assigned(g_GetApplicationIconHandle)
  then
    Result := g_GetApplicationIconHandle;
end;

procedure OnVersionComponentVisit(strPtr: TClientHandle; level: UInt32; componentName: PMsgChar); cdecl;
type PString = ^String;
var sPtr: PString;
begin
  sPtr := PString(strPtr);
  sPtr^ :=   sPtr^ + #13#10;
  while level > 0 do
  begin
      sPtr^ := sPtr^ +  '-  ';
    dec(level);
  end;
  sPtr^ :=  sPtr^ + componentName;
end;

procedure DisplayAboutMsg;
var s: String;
begin
    s := DMS_GetVersion + ', copyright Object Vision BV';
    DMS_VisitVersionComponents(@s, OnVersionComponentVisit);

    TfrmErrorMsg.DisplayAbout(s);
end;

procedure DisplayAboutBox(hApp: HWND; AOwner: TComponent; sDMSVersion, sClientVersion: ShortString);
begin
  if Assigned(g_DisplayAboutBox)
  then
    g_DisplayAboutBox(hApp, AOwner, sDMSVersion, sClientVersion, 'www.objectvision.nl')
  else
    DisplayAboutMsg;
end;

procedure DisplayOrHideSplashWindow(hApp: HWND; AOwner: TComponent; sClientVersion: shortstring; doShow: TToggleSplash);
begin
  if Assigned(g_DisplayOrHideSplashWindow)
  then
    g_DisplayOrHideSplashWindow(hApp, AOwner, sClientVersion, doShow = tsShow);
end;

end.
