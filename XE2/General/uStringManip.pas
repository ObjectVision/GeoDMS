//<HEADER> 
(*
DmsClient.pas (DMS = Data & Model Server, a server for DSS applications). 
Version 4.26.00, date: 11-09-2002
Copyright (C) 1998-2002  YUSE GSO Object Vision BV. 
http://www.objectvision.nl/DMS/

This Delphi source code is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public Licence version 2 
(the Licence) as published by the Free Software Foundation.

See LICENCE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/Licence.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

You can use, redistribute and/or modify this library source code file
provided that this entire header notice is preserved.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public Licence for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development

Additional to the terms of the licence, we request you to apply the following
guidelines (these additional requests are not limiting your rights under the terms of the licence):
- An appropriate copyright notice that an interactive program/web-site
  containing or using this DMS library should announce according to article 2c
  of the Licence is the inclusion of the following text in a 
  start splash-screen/page and about-box (if any):
  "This software/web-site is using the Data & Model Server (DMS)
   Copyright (c) 1998-2002 YUSE GSO Object Vision BV.
   For documentation, source code, and licence rights, see:
   http://www.ObjectVision.nl/DMS/ 
   The DMS is licenced WITHOUT ANY WARRANTY. Specific warranties can be granted
   by an additional written contract for support, assistance and/or development"
- Before distributing the DMS library and/or work containing it to others, 
  check at our web-site to see if any newer version has been made available.
- We would like to be notified if you make any modification and receive your 
  source code at email://mhilferink@ObjectVision.nl
- In case of bug-fixes and small additions, we might or might not use your
  improvements in a possible future version of the DMS without notification of
  your modification (ie you will not be displayed as the author), nevertheless,
  when you distribute the DMS source code with your modifications, you should
  give a prominent notification as described in article 2a of the Licence.
- In case of larger additions, you will remain the author of these additions
  and this will be displayed if your addition is distributed by us. 
- When you have made a product that uses the DMS, you can distribute it under
  the same GPL Licence conditions. We would like to receive a copy of your product
  including source code. You will remain the author of this product.

Documentation on using the Data & Model Server library can be found at:
http://www.ObjectVision.nl/DMS/

This software is developed by YUSE GSO Object Vision BV.
YUSE GSO Object Vision BV has specialized in developing software for 
policy oriented data analysis applications.

Do you want to work on the development of the DMS? Check for vacancies at:
http://www.ObjectVision.nl/Vacancies.html

YUSE GSO Object Vision BV
http://www.ObjectVision.Nl/
email://m.hilferink@yusegso.nl
Zijlweg 148-a
2015 BJ Haarlem
the Netherlands
tel +31-23-5319161
fax +31-23-5420252
*)
//</HEADER>
unit uStringManip;

interface

Type
  TDataFields = array of string;

  function ParseRecData(RecordSpec: string; LenRecTypeSpecifier: Integer): TDataFields;

  // Replace \n by \r\n
  function FormatNewLine(const str : String) : String;

implementation

Uses
   uGenLib,
   SysUtils;


function ParseRecData(RecordSpec: string; LenRecTypeSpecifier: Integer): TDataFields;
const
  Separators: set of char = [' ',#9];
  Quotes: set of char = ['''','"'];
var
  RecordData: string;
  NrOfFields, I, J, FieldStart, NewFieldStart, FieldEnd, FieldLen: Integer;
  C, DelimitingQuote: Char;
  EndOfField: Boolean;
  State: (OutsideField, InUnquotedField, InQuotedField);
begin
  SetLength(RecordData, Length(RecordSpec)-LenRecTypeSpecifier);
  for I := LenRecTypeSpecifier+1 to Length(RecordSpec) do begin
    RecordData[I-LenRecTypeSpecifier] := RecordSpec[I];
  end;
  State := OutsideField;
  EndOfField := False;
  NrOfFields := 0;
  for I := 1 to Length(RecordData) do begin
    C := RecordData[I];
    if I < Length(RecordData) then begin
      case State of
        OutsideField:
          if C in Quotes then begin
            DelimitingQuote := C;
            State := InQuotedField;
          end else if not (C in Separators) then begin
            State := InUnquotedField;
          end;
        InUnquotedField:
          if C in Separators then begin
            EndOfField := True;
            State := OutsideField;
          end else if C in Quotes then begin
            EndOfField := True;
            DelimitingQuote := C;
            State := InQuotedField;
          end;
        InQuotedField:
          if C = DelimitingQuote then begin
            EndOfField := True;
            State := OutsideField;
          end;
      end;
    end else begin
      case State of
        OutsideField:
          if not ((C in Separators) or (C in Quotes)) then begin
            EndOfField := True;
          end;
        InUnquotedField, InQuotedField:
          EndOfField := True;
      end;
    end;
    If EndOfField then begin
      NrOfFields := NrOfFields+1;
      EndOfField := False;
    end;
  end;
  SetLength(Result,NrOfFields);
  State := OutsideField;
  EndOfField := False;
  NewFieldStart := -1;
  NrOfFields := 0;
  for I := 1 to Length(RecordData) do begin
    C := RecordData[I];
    if I < Length(RecordData) then begin
      case State of
        OutsideField:
          if C in Quotes then begin
            FieldStart := I+1;
            DelimitingQuote := C;
            State := InQuotedField;
          end else if not (C in Separators) then begin
            FieldStart := I;
            State := InUnquotedField;
          end;
        InUnquotedField:
          if C in Separators then begin
            EndOfField := True;
            FieldEnd := I-1;
            State := OutsideField;
          end else if C in Quotes then begin
            EndOfField := True;
            FieldEnd := I-1;
            NewFieldStart := I+1;
            DelimitingQuote := C;
            State := InQuotedField;
          end;
        InQuotedField:
          if C = DelimitingQuote then begin
            EndOfField := True;
            FieldEnd := I-1;
            State := OutsideField;
          end;
      end;
    end else begin
      case State of
        OutsideField:
          if not ((C in Separators) or (C in Quotes)) then begin
            EndOfField := True;
            FieldStart := I;
            FieldEnd := I;
          end;
        InUnquotedField:
          if (C in Separators) or (C in Quotes)  then begin
            EndOfField := True;
            FieldEnd := I-1;
          end else begin
            EndOfField := True;
            FieldEnd := I;
          end;
        InQuotedField:
          if C = DelimitingQuote then begin
            EndOfField := True;
            FieldEnd := I-1;
          end else begin
            EndOfField := True;
            FieldEnd := I;
          end;
      end;
    end;
    If EndOfField then begin
      NrOfFields := NrOfFields+1;
      FieldLen := FieldEnd - FieldStart + 1;
      if FieldLen > 0 then begin
        SetLength(Result[NrOfFields-1],FieldLen);
        for J := 1 to FieldLen do begin
          Result[NrOfFields-1][J] := RecordData[FieldStart+J-1];
        end;
      end else begin
        Result[NrOfFields-1] := '';
      end;
      EndOfField := False;
      if NewFieldStart >= 0 then begin
        FieldStart := NewFieldStart;
        NewFieldStart := -1;
      end;
    end;
  end;
end; // End ParseRecData


function FormatNewLine(const str : String) : String;
begin
   Result := StringReplace(str, NEW_LINE, LINEFEED, [rfReplaceAll]);
   Result := StringReplace(Result, LINEFEED, NEW_LINE, [rfReplaceAll]);
end;

end.
