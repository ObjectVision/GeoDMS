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

unit uDMSTreeviewFunctions;

{
This unit contains function for populating and manipulating a common treeview
with treeitems from the DMS environment.
}

interface

uses
  uDMSInterfaceTypes, comctrls, shdocvw;

type TDetailPage = (
  DP_General,
  DP_Explore,
  DP_MetaData,
  DP_Statistics,
  DP_ValueInfo,
  DP_Config,
  DP_SourceDescr,
  DP_AllProps,
  DP_Unknown
);

function dp_FromName(sName: string): TDetailPage;
function dp_ToName(dp: TDetailPage): string;

function htmlEncodeTextDoc(str: SourceString): SourceString;

procedure SetDocData  (wb: IWebBrowser2; contents: SourceString);
procedure SetDocUrl   (wb: IWebBrowser2; url: string);

function ViewDetails (wb: IWebBrowser2; ti: TTreeItem; bShowHidden: boolean; dp: TDetailPage;
  funcID: Cardinal = 0; p0: SizeT =0; p1: Integer = -1; p2: Integer = -1; sExtraInfo: PSourceChar = Nil): Boolean; // returns false when not done
procedure ViewMetadata(wb: IWebBrowser2; ti: TTreeItem);

implementation

uses
  forms, controls, uAppGeneral, dmGeneral, uGenLib,
  uDMSUtils,
  uDmsInterface,
  fExpresEdit,
  sysutils, fMain {bad!!},
  dialogs, FileCtrl,
  system.Variants
  ;

resourcestring
  RS_VectorOutStreamBuffError = 'Error in creating a VectorOutStreamBuff while producing a DetailPage';
  RS_XmlOutStreamError = 'Error in creating an XML_OutStream';

function dp_ToName(dp: TDetailPage): string;
begin
  Result := '';
  case dp of
    DP_General:     Result := 'General';
    DP_Explore:     Result := 'Explore';
    DP_MetaData:    Result := 'MetaData';
    DP_Statistics:  Result := 'Stat';
    DP_ValueInfo:   Result := 'vi.stat';
    DP_Config:      Result := 'Config';
    DP_SourceDescr: Result := 'SourceDescr';
    DP_AllProps:    Result := 'Properties';
  end;
  Assert(Result <> '');
end;

function dp_FromName(sName: string): TDetailPage;
begin
  Result := DP_Unknown;
  if Copy(sName,1,3) = 'VI.'  then Result := DP_ValueInfo   else
  if sName = 'STAT'           then Result := DP_Statistics  else
  if sName = 'CONFIG'         then Result := DP_Config      else
  if sName = 'GENERAL'        then Result := DP_General     else
  if sName = 'PROPERTIES'     then Result := DP_AllProps    else
  if sName = 'SOURCEDESCR'    then Result := DP_SourceDescr else
  if sName = 'METADATA'       then Result := DP_MetaData    else
  if sName = 'EXPLORE'        then Result := DP_Explore;
  Assert(Result <> DP_Unknown);
end;

function htmlEncodeTextDoc(str: SourceString): SourceString;
var
  outBuff     : TOutStreamBuff;
  xmlOut      : TXMLOutStreamHandle;
begin
  outBuff := DMS_VectOutStreamBuff_Create;
  if not Assigned(outBuff) then
    Result := SourceString(RS_VectorOutStreamBuffError)
  else try
    DMS_OutStreamBuff_WriteChars(outBuff, '<HTML><BODY><PRE>');

    xmlOut  := XML_OutStream_Create(outBuff, ST_HTM, '', nil);
    if not Assigned(xmlOut) then
      DMS_OutStreamBuff_WriteChars(outBuff, PSourceChar(SourceString(RS_XmlOutStreamError)))
    else try
      XML_OutStream_WriteText(xmlOut, PSourceChar(str));
    finally
      XML_OutStream_Destroy(xmlOut);
    end;

    DMS_OutStreamBuff_WriteChars(outBuff, '</PRE></BODY></HTML>');
    DMS_OutStreamBuff_WriteBytes(outBuff, '', 1); // write null terminator
    Result := DMS_VectOutStreamBuff_GetData(outBuff);
  finally
    DMS_OutStreamBuff_Destroy(outBuff);
  end;
end;

const g_strCurrUrl : string = '';

procedure SetDocData(wb: IWebBrowser2; contents: SourceString);
var
  doc : OleVariant;
  keyFocus: TWinControl;
begin
  keyFocus    := frmMain.ActiveControl;
  try
    wb.Stop;
    doc := wb.Document;
    doc.Open;
    doc.Clear;
    doc.Write(contents);
    doc.Close;
    g_strCurrUrl := 'dms:###';
  finally
    if Assigned(keyFocus) then frmMain.ActiveControl := keyFocus;
  end;
end;


procedure SetDocUrl(wb: IWebBrowser2; url: string);
var oleUrl, oleFlags: OleVariant;
begin
  if(lowercase(url) <> g_strCurrUrl)
  then try
    g_strCurrUrl := lowercase(url);
{            REMOVE ?
    SetDocData(wb,
      '<body>'+
        '<a href="'+url+'" target="_BLANK">'+
          url+
        '</a>'+
      '</body>'
    );
}
    oleUrl   := url;
    oleFlags := navNoHistory;
    wb.Navigate2(oleUrl, oleFlags, EmptyParam, EmptyParam, EmptyParam);
  except
  end
end;

procedure ViewMetadata(wb: IWebBrowser2; ti: TTreeItem);
var
  url: FileString; urlTi: TTreeItem;
label makeBlank, makeAbsPath, setDoc;
begin
  Assert(Assigned(ti));

  urlTi := ti;
  url := TreeItemPropertyValue(urlTi, GetPropDef(pdUrl));

  if (url <> '') then
  begin
    // url of ti can start with '#', skip it
    if url[1] = '#' then
       url := copy(url, 2, length(url)-1);
    goto makeAbsPath;
  end;

  repeat
    urlTi := DMS_TreeItem_GetParent(urlTi);
    if not Assigned(urlTi) then goto makeBlank;
    url := TreeItemPropertyValue(urlTi, GetPropDef(pdUrl));
  until url <> '';

  // url starting with '#' is only active on item itself and not its subitems
  if url[1] = '#' then goto makeBlank;

makeAbsPath:
  if (url <> '') and (url[1]='!')
  then
  begin
    url := copy(url, 2, length(url)-1); // remove indirection indicator to get it evaluated at the user's location
    urlTi := ti;
  end;
  url := TreeItem_GetFullStorageName(urlTi, PItemChar(url));
  if url <> ''  then goto  setDoc;
makeBlank:
   assert(urlTi <> ti);
   url := 'about:blank';
setDoc:
  SetDocUrl(wb, string( url ));
end;

function ViewDetails (wb: IWebBrowser2; ti: TTreeItem; bShowHidden: boolean; dp: TDetailPage;
  funcID: Cardinal = 0; p0: SizeT = 0; p1: Integer = -1; p2: Integer = -1; sExtraInfo: PSourceChar = Nil): Boolean;
var
  streamType  : Cardinal;
  docType     : ItemString;
  xmlResult   : SourceString;
  outBuff     : TOutStreamBuff;
  xmlOut      : TXMLOutStreamHandle;
begin
  try
    Result := true; // be optimistic about being done
    outBuff := DMS_VectOutStreamBuff_Create;
    if not assigned(outBuff) then
      xmlResult := SourceString(RS_VectorOutStreamBuffError)
    else
    begin
      try
        streamType := ST_HTM;
        docType    := 'html';
        if (dp = DP_Config) then begin streamType := ST_DMS; docType := 'DMS' end;
        xmlOut  := XML_OutStream_Create(outBuff, streamType, PItemChar(docType), GetPropDef(pdExpr));
        if not assigned(xmlOut) then
          DMS_OutStreamBuff_WriteChars(outBuff, PSourceChar(SourceString(RS_XmlOutStreamError)))
        else
        try
          case dp of
            DP_AllProps: Result := DMS_TreeItem_XML_DumpAllProps(ti, xmlOut, bShowHidden);
            DP_General:  Result := DMS_TreeItem_XML_DumpGeneral (ti, xmlOut, bShowHidden);
            DP_Explore:  DMS_TreeItem_XML_DumpExplore (ti, xmlOut, bShowHidden);
            DP_Config:   DMS_TreeItem_XML_Dump(ti, xmlOut);
            DP_ValueInfo:
            case funcID of
              1: Result := DMS_DataItem_ExplainAttrValueToXML(ti, xmlOut, p0, sExtraInfo, bShowHidden);
              2: Result := DMS_DataItem_ExplainGridValueToXML(ti, xmlOut, p1, p2, sExtraInfo, bShowHidden);
            end;
          end;
        finally
          XML_OutStream_Destroy(xmlOut);
        end;
        DMS_OutStreamBuff_WriteBytes(outBuff, '', 1); // null terminator
        xmlResult := DMS_VectOutStreamBuff_GetData(outBuff);
      finally
        DMS_OutStreamBuff_Destroy(outBuff);
      end;
      if dp = DP_CONFIG then
        xmlResult := htmlEncodeTextDoc(xmlResult);
    end;
    SetDocData(wb, xmlResult);
  except
     raise Exception.Create('Exception raised in ViewDetails('+dp_ToName(dp)+') ');
  end;
end;

end.
