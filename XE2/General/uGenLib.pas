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

unit uGenLib;

{$V-}

{******************************************************************************}
                              interface
{******************************************************************************}

uses Forms, uDmsInterfaceTypes;

const
   DEF_STR_LENGTH             =  256;
   LINEFEED                   =  Chr(10);
   CARRIAGERETURN             =  Chr(13);
   NEW_LINE                   =  CARRIAGERETURN + LINEFEED;
   SPACE                      =  Chr(32);
   EMPTYSTRING                =  '';
   CHAR_SINGLE_QUOTE          =  '''';
   CHAR_DOUBLE_QUOTE          =  '"';

{*******************************************************************************
                              COMPARISON FUNCTIONS
*******************************************************************************}

function CompareDouble(const a, b: Double): Integer;

function IIF(expr: Boolean; const a, b: Integer): Integer; overload;
function IIF(expr: Boolean; const a, b: String): String; overload;
function IIF(expr: Boolean; const a, b: Pointer): Pointer; overload;

{*******************************************************************************
                              STRING ROUTINES
*******************************************************************************}

function GetStrPart(var s: SourceString; c: AnsiChar): SourceString; overload;// {split string: GetStrPart('aaa;bbb',';') -> Result = aaa, s = bbb}

function FormatNewLine(const str : String) : String;

{ checking }
function IsAlphaNumeric(const s: SourceString): Boolean;
function IsFloat(const s: string): Boolean;
function IsInteger(const s: string): Boolean;
function AsString(x: Cardinal): SourceString;

function SubstituteChar(const s: string; c1, c2: char): string;

{ filenames }
function ExtractFileDirEx2 (const FileName: FileString): FileString;
function ExtractFileNameEx2(const FileName: FileString): FileString;
function ExtractFilePathEx2(const FileName: FileString): FileString;
function ExtractFileExtEx2 (const FileName: FileString): FileString;

procedure SetFileAttrRW(const FileName: FileString);

procedure Split(src, sep: String; var sLeft, sRight: String); overload;
procedure Split(src, sep: SourceString; var sLeft, sRight: SourceString); overload;

{*******************************************************************************
                        CharBuffer ROUTINES
*******************************************************************************}

type TCharBuffer = record
  sBuf:             PSourceChar;
  nCapacity, nSize: Cardinal;
  maxSize:          Cardinal;
end;

procedure CharBuffer_Init(var cb: TCharBuffer);
procedure CharBuffer_Exit(var cb: TCharBuffer);
procedure CharBuffer_Rewind(var cb: TCharBuffer);
procedure CharBuffer_Reloc(var cb: TCharBuffer; nNewCount: Cardinal);
function CharBuffer_Add(var cb: TCharBuffer; const appendix: SourceString): Boolean;
function CharBuffer_End(const cb: TCharBuffer): PSourceChar;

{*******************************************************************************
                        ARITHMETICAL ROUTINES
*******************************************************************************}

function Min(a, b: Integer): Integer; export;
function Max(a, b: Integer): Integer; export;

function BetweenInc(const nValue, nLower, nUpper: int64): boolean; overload;
function BetweenInc(const fValue, fLower, fUpper: extended): boolean; overload;

{*******************************************************************************
                        FILE HANDLING ROUNTINES
*******************************************************************************}

procedure DeleteFile(const filename: string);

{*******************************************************************************
                           MISCELANNEUOS ROUNTINES
*******************************************************************************}

// REMOVE procedure   DoCommand(const command: string);
procedure   CheckApplicationDuplicate(const caption: string);

function    ConfirmationYesNoDlg(const s: string): Boolean;

type
   TScreenIdentity = function (Item1, Item2: Pointer): Boolean;

function ScreenFormFind(const fc: TFormClass; const si: TScreenIdentity; const id: Pointer; var index: Integer): Boolean;

function IsValidFileName(const FileName: FileString): Boolean;

{******************************************************************************}
                           implementation
{******************************************************************************}

uses Dialogs, Controls, SysUtils, WinProcs;

type
   TCharSet = set of AnsiChar;
   PDouble  = ^Double;

const ALPHANUMERICCHARS: TCharSet = ['0'..'9', 'A'..'Z', 'a'..'z', '_'];

{******************************************************************************}
//                            STRING ROUNTINES
{******************************************************************************}

// ============================= helper funcs

procedure Split(src, sep: String; var sLeft, sRight: String); overload;
var p: Integer;
begin
  p := pos(sep, src);
  if (p = 0) then
  begin
    sLeft := src;
    sRight := '';
  end
  else
  begin
     sLeft  := copy(src, 1, p-1);
     sRight := copy(src, p+length(sep), Length(src));
   end;
end;

procedure Split(src, sep: SourceString; var sLeft, sRight: SourceString); overload;
var p: Integer;
begin
  p := pos(sep, src);
  if (p = 0) then
  begin
    sLeft := src;
    sRight := '';
  end
  else
  begin
     sLeft  := copy(src, 1, p-1);
     sRight := copy(src, p+length(sep), Length(src));
   end;
end;

function GetStrPart(var s: SourceString; c: AnsiChar): SourceString;
var
   i: Integer;
begin
   i := Pos(c, s);
   if i > 0 then
   begin
      Result := Copy(s, 1, i-1);
      s := Copy(s, i+1, Length(s)-i);
   end else
   begin
      Result := s;
      s:= '';
   end;
end;

function SubstituteChar(const s: string; c1, c2: char): string;
var i : LongInt;
begin
   Result := s;
   for i := 1 to Length(Result) do
      if (Result[i] = c1) then
         Result[i] := c2;
end;

function FormatNewLine(const str : String) : String;
begin
   Result := StringReplace(str, NEW_LINE, LINEFEED, [rfReplaceAll]);
   Result := StringReplace(Result, LINEFEED, NEW_LINE, [rfReplaceAll]);
end;

{ checking }

function AllCharInSet(const s: SourceString; const charset: TCharSet): Boolean;
var i: LongInt;
begin
   Result := true;
   for i := 1 to Length(s) do
      if not (s[i] in charset) then
      begin
         Result := false;
         Exit;
      end;
end;

function IsAlphaNumeric(const s: SourceString): Boolean;
begin
   Result := AllCharInSet(s, AlphaNumericChars);
end;

function IsFloat(const s: string): Boolean;
var dummy: double;
begin
   Result := TextToFloat(PChar(S), dummy, fvExtended)
end;

{$hints off}
function IsInteger(const s: string): Boolean;
var
  nVal, nCode : integer;
begin
  val(s, nVal, nCode);
  Result := nCode = 0;
end;
{$hints on}

function AsString(x: Cardinal): SourceString;
var res: ShortString;
begin
  Str(x, res);
  Result := res;
end;


{*******************************************************************************
                        CharBuffer ROUTINES
*******************************************************************************}

procedure CharBuffer_Init(var cb: TCharBuffer);
begin
  cb.sBuf      := Nil;
  cb.nCapacity := 0;
  cb.nSize     := 0;
  cb.maxSize   := $40000000;
end;

procedure CharBuffer_Rewind(var cb: TCharBuffer);
begin
  cb.nSize     := 0;
  if Assigned(cb.sBuf) then cb.sBuf^ := chr(0);
end;

procedure CharBuffer_Exit(var cb: TCharBuffer);
begin
  if Assigned(cb.sBuf) then
    FreeMem(cb.sBuf);
  CharBuffer_Init(cb);
end;

procedure CharBuffer_Reloc(var cb: TCharBuffer; nNewCount: Cardinal);
var newBuf: PSourceChar;
begin
  if (nNewCount < cb.nCapacity) then exit;
  Assert((cb.nSize < cb.nCapacity) or (cb.nCapacity = 0));
  cb.nCapacity := max(cb.nCapacity*2, nNewCount + 1);
  GetMem(newBuf, cb.nCapacity);
  if Assigned(cb.sBuf) then
  begin
    Move(cb.sBuf^, newBuf^, cb.nSize);
    FreeMem(cb.sBuf);
  end;
  cb.sBuf := newBuf;
  PChar(cardinal(cb.sBuf)+ cb.nSize)^ := chr(0);
end;

function CharBuffer_Add(var cb: TCharBuffer; const appendix: SourceString): Boolean;
var nNewSize, len: cardinal;
begin
  Result := false;
  len := length(appendix);
  nNewSize := cb.nSize+len;
  if (nNewSize >= cb.nCapacity ) then
  begin
    if nNewSize >= cb.maxSize then exit;
    CharBuffer_Reloc(cb, nNewSize);
  end;
  Result := True;

  Move(PSourceChar(appendix)^, CharBuffer_End(cb)^, len);
  cb.nSize := nNewSize;
  CharBuffer_End(cb)^ := chr(0);
end;

function CharBuffer_End(const cb: TCharBuffer): PSourceChar;
begin
  result := cb.sBuf + cb.nSize;
end;

{******************************************************************************}
//                      ARITHMETICAL ROUTINES
{******************************************************************************}

function Min(a, b: Integer): Integer;
begin
   if (a<b) then Result := a else Result := b;
end;

function Max(a, b: Integer): Integer;
begin
   if (a>b) then Result := a else Result := b;
end;

function BetweenInc(const nValue, nLower, nUpper: int64): boolean;
begin
  Result := (nValue >= nLower) and (nValue <= nUpper);
end;

function BetweenInc(const fValue, fLower, fUpper: extended): boolean;
begin
  Result := (fValue >= fLower) and (fValue <= fUpper);
end;

{******************************************************************************}
//                           FILE HANDLING ROUNTINES
{******************************************************************************}

procedure DeleteFile(const filename: string);
var  f : file;
begin
   Assign(f, filename);
   {$I-} Erase(f); {$I+}
end;

{******************************************************************************}
//                         MISCELANNEOUS ROUNTINES
{******************************************************************************}

// dialogs
function  ConfirmationYesNoDlg(const s: string): Boolean;
begin
   Result   := MessageDlg(s, mtConfirmation, [mbYes, mbNo], 0) = mrYes;
end; // ConfirmationYesNoDlg

procedure CheckApplicationDuplicate(const caption: string);
var
   pstr  : PChar;
   hnd   : HWnd;
begin
   GetMem(pstr, DEF_STR_LENGTH);
   StrPCopy(pstr, caption);
   hnd   := FindWindow(nil, pstr);
   FreeMem(pstr, DEF_STR_LENGTH);
   if (hnd <> 0) and not ConfirmationYesNoDlg('Application "' + caption + '" already started. Start again ?') then
   begin
      SetActiveWindow(hnd);
      Halt;
   end;
end; // CheckApplicationDuplicate

(****************************************************************************
                        Compare Functions
****************************************************************************)

function IIF(expr: Boolean; const a, b: Integer): Integer;
begin
   if expr then
      Result := a
   else
      Result := b;
end;

function IIF(expr: Boolean; const a, b: String): String;
begin
   if expr then
      Result := a
   else
      Result := b;
end;

function IIF(expr: Boolean; const a, b: Pointer): Pointer;
begin
   if expr then
      Result := a
   else
      Result := b;
end;

function CompareDouble(const a, b: Double): Integer;
begin
   if a < b then
      Result := -1
   else if a = b then
      Result := 0
   else
      Result := 1;
end;

{*******************************************************************************
                           Component Instantiation
*******************************************************************************}

function   ScreenFormFind(const fc: TFormClass; const si: TScreenIdentity; const id: Pointer; var index: Integer): Boolean;
var   i: Integer;
begin
   Result   :=  false;

   with Screen do
      for i := 0 to CustomFormCount - 1 do
         if (CustomForms[i] is fc) and si(CustomForms[i], id) then
         begin
            Result := true;
            index := i;
            Exit;
         end;
end;

function IsValidFileName(const FileName: FileString): Boolean;
// FileName may only contain filename+extension.
// FileName may not contain a path.
const
  InvalidChars = [Chr(0)..Chr(31), '\', '/', ':', '*', '?', '"', '<', '>',
'|'];
var
  i: Integer;
begin
  Result := False;
  if FileName = '' then
    Exit;
  if FileName[1] in [' ', '.'] then
    Exit;
  if FileName[Length(FileName)] in [' ', '.'] then
    Exit;
  for i := 1 to Length(FileName) do
    if FileName[i] in InvalidChars then
      Exit;
  Result := True;
end;

function ExtractFileDirEx2(const FileName: FileString): FileString;
var
  I: Integer;
begin
  I := LastDelimiter('\/:',Filename);
  if (I > 1) and (FileName[I] in ['\', '/']) and
    (not (FileName[I - 1] in ['\', ':', '/']) or
    (ByteType(FileName, I-1) = mbTrailByte)) then Dec(I);
  Result := Copy(FileName, 1, I);
end;

function ExtractFileNameEx2(const FileName: FileString): FileString;
var
  I: Integer;
begin
  I := LastDelimiter('\/:', FileName);
  Result := Copy(FileName, I + 1, MaxInt);
end;

function ExtractFilePathEx2(const FileName: FileString): FileString;
var
  I: Integer;
begin
  I := LastDelimiter('\/:', FileName);
  Result := Copy(FileName, 1, I);
end;

function ExtractFileExtEx2(const FileName: FileString): FileString;
var
  I: Integer;
begin
  I := LastDelimiter('.\/:', FileName);
  if (I > 0) and (FileName[I] = '.')
  then Result := Copy(FileName, I, MaxInt)
  else Result := '';
end;

procedure SetFileAttrRW(const FileName: FileString);
var attrs: Integer;
const faReadOnly: Integer = $00000001;
begin
   attrs := FileGetAttr(FileName);
   if (attrs and faReadOnly) <> 0 then
     FileSetAttr(FileName, attrs - faReadOnly);
end;

end.

