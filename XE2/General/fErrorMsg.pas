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

unit fErrorMsg;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls, uDmsInterfaceTypes, ComCtrls;

type
   TfrmErrorMsg = class(TForm)
    btnOk: TButton;
    btnIgnore: TButton;
    btnMailMsg: TButton;
    mmLongMsg: TMemo;
    btnTerminate: TButton;
    btnReopen: TButton;
    stnMain: TStatusBar;
      procedure FormShow(Sender: TObject);
      procedure SendErrorMail(subj: String; msg: String);
      procedure btnMailMsgClick(Sender: TObject);
      procedure FormCreate(Sender: TObject);
      procedure FormClose(Sender: TObject; var Action: TCloseAction);
      procedure btnTerminateClick(Sender: TObject);
      procedure mmLongMsgDblClick(Sender: TObject);
    procedure FormKeyUp(Sender: TObject; var Key: Word;
      Shift: TShiftState);
    procedure btnReopenClick(Sender: TObject);
    function OnCleanupCollect(ti: TTreeItem; var done: Boolean): Boolean;

   private
      m_LastHeapProblemCount: Cardinal;
      function FormatMailString(const str : String) : String;
   public
      class function DisplayError(longMsg : String; isFatal: boolean) : Integer;
      class function DisplayMemoryError(size: SizeT; longMsg : String): Integer;
      class procedure DisplayAbout(longMsg : String);

      class procedure StartSourceEditor(sSelection: string);
   end;


implementation
{$R *.DFM}

uses
   uDmsUtils, uDmsInterface,
   uGenLib, //fTreeItemPickList,
   ShellAPI, uAppGeneral, Configuration,
   HttpApp, dmGeneral, fMain;

const
   MIN_HEIGHT = 300;
   MIN_WIDTH  = 508;

   // Maximum length of a mail message
   MAIL_COMMAND_LENGTH = 2000;
   MAIL_SUBJECT_LENGTH = 100;
   MAIL_TO_ADDR        = 'dmserrorreport@objectvision.nl';


   AMPERSAND = '&';
   // Replacement characters for use in email msg
   MAIL_NEWLINE        = '%0D%0A';
   MAIL_SPACE          = '%20';
   MAIL_AMPERSAND      = '%26';

// Displays a modal error message, with the given messages.
// This function is blocking.
// Returns : Either mrOK or mrIgnore
class function TfrmErrorMsg.DisplayError(longMsg : String; isFatal: boolean) : Integer;
var instance : TfrmErrorMsg;
begin
   instance := TfrmErrorMsg.Create(nil);
   dmfGeneral.IncActiveErrorCount;
   with instance do
   try
     mmLongMsg.Text := FormatNewLine(Trim(longMsg));
     btnReopen.Enabled := not isFatal;
     btnIgnore.Enabled := not isFatal;
     btnOK.Enabled := not isFatal;

     dmfGeneral.RegisterCurrForm(instance);
     try
       Result := ShowModal();
     finally
       dmfGeneral.UnRegisterCurrForm(instance);
     end;
   finally
     dmfGeneral.DecActiveErrorCount;
     Free();
   end;
end;

class function TfrmErrorMsg.DisplayMemoryError(size: SizeT; longMsg : String): Integer;
var instance : TfrmErrorMsg;
begin
   instance := TfrmErrorMsg.Create(nil);
   dmfGeneral.IncActiveErrorCount;
   try
     instance.Caption := 'Memory Allocation Error: allocation of  '+IntToStr(size) + ' bytes failed';
     instance.mmLongMsg.Text := FormatNewLine(Trim(longMsg));
     instance.btnReopen.Enabled := TConfiguration.CurrentIsAssigned;

     instance.btnOk.Caption := 'Retry';
     instance.btnIgnore.Caption := 'Fail';

     dmfGeneral.RegisterCurrForm(instance);
     try
       Result := instance.ShowModal();
     finally
       dmfGeneral.UnRegisterCurrForm(instance);
     end;
   finally
     dmfGeneral.DecActiveErrorCount;
     instance.Free();
   end;
end;

class procedure TfrmErrorMsg.DisplayAbout(longMsg : String);
var instance : TfrmErrorMsg;
begin
   instance := TfrmErrorMsg.Create(nil);
   try
     instance.Caption := 'About';
     instance.mmLongMsg.Text := FormatNewLine(Trim(longMsg));
     instance.btnReopen.Visible := False;
     instance.btnMailMSg.Visible := false;
     instance.btnIgnore.Visible := false;
     instance.btnTerminate.Visible := false;

     dmfGeneral.RegisterCurrForm(instance);
     try
       instance.ShowModal();
     finally
       dmfGeneral.UnRegisterCurrForm(instance);
     end;
   finally
     instance.Free();
   end;
end;

procedure TfrmErrorMsg.FormShow(Sender: TObject);
begin
  Height := Max(dmfGeneral.GetNumber(NRI_ErrBoxHeight), MIN_HEIGHT);
  Width  := Max(dmfGeneral.GetNumber(NRI_ErrBoxWidth ), MIN_WIDTH );
end;


function GetFirstLineWithText(mm: TMemo): string;
var i: integer;
begin
  for i:=0 to mm.Lines.count-1 do
    if length( trim(mm.lines[i])) > 10 then
    begin
      GetFirstLineWithText := mm.Lines[i];
      exit;
    end
end;

procedure TfrmErrorMsg.btnMailMsgClick(Sender: TObject);
begin
   SendErrorMail(GetFirstLineWithText(mmLongMsg), mmLongMsg.Text);
end;

function TfrmErrorMsg.OnCleanupCollect(ti: TTreeItem; var done: Boolean): Boolean;
begin
  Result := false;
  if g_HeapProblemCount > m_LastHeapProblemCount then exit;
  done := false;
  Result := Assigned(DMS_TreeItem_QueryInterface(ti, DMS_AbstrDataItem_GetStaticClass))
    and DMS_DataItem_IsInMem(ti)
    and (DMS_DataItem_GetNrFeatures(ti) > 1000);
end;

// Prepare an email message by executing a mailto:a?subject=b&body=c string.
procedure TfrmErrorMsg.SendErrorMail(subj: String; msg: String);
var
   mailCommand : String;
   body        : String;
   subject     : String;
begin
   // Format subject
   subject := 'Bugreport: "'+ StringReplace(subj, #13#10, ' ', [rfReplaceAll]);

   if(Length(subject) > MAIL_SUBJECT_LENGTH) then
      subject := Copy(subject, 1, MAIL_SUBJECT_LENGTH-3) + '...';

   // Format body
   body := '{You can insert an optional message here}'+MAIL_NEWLINE+
           MAIL_NEWLINE+
           '==================================================='+MAIL_NEWLINE+
           'DMS version: '+String(DMS_GetVersion)+MAIL_NEWLINE+
           '==================================================='+MAIL_NEWLINE+
           msg;

   // Format mailto: string
   mailCommand := 'mailto:'+MAIL_TO_ADDR+
                  '?subject='+FormatMailString(subject)+
                  '&body='+FormatMailString(body);

   // make sure it does not exceed 2000 characters, which will not be handled by windows correctly.
   if(length(mailCommand) > MAIL_COMMAND_LENGTH) then
   begin
      mailCommand := Copy(mailCommand, 0, MAIL_COMMAND_LENGTH-3) + '...';
   end;

   ShellExecute(Self.Handle, nil, PChar(mailCommand), EMPTYSTRING, EMPTYSTRING, SW_SHOWNORMAL); //SW_SHOWNOACTIVATE);
end;

// Format the given string to fit in a mailto: string
function TfrmErrorMsg.FormatMailString(const str : String) : String;
begin
   Result := str;
   Result := StringReplace(Result, CHAR_DOUBLE_QUOTE, CHAR_SINGLE_QUOTE+CHAR_SINGLE_QUOTE, [rfReplaceAll]);
   Result := StringReplace(Result, SPACE, MAIL_SPACE, [rfReplaceAll]);
   Result := StringReplace(Result, NEW_LINE, MAIL_NEWLINE, [rfReplaceAll]);
   Result := StringReplace(Result, AMPERSAND, MAIL_AMPERSAND, [rfReplaceAll]);
end;

procedure TfrmErrorMsg.FormCreate(Sender: TObject);
begin
// Height := Max(dmfGeneral.GetNumber(NRI_ErrBoxHeight), MIN_HEIGHT);
// Width  := Max(dmfGeneral.GetNumber(NRI_ErrBoxWidth ), MIN_WIDTH );
end;

procedure TfrmErrorMsg.FormClose(Sender: TObject; var Action: TCloseAction);
begin
  with dmfGeneral do
  begin
    SetNumber(NRI_ErrBoxHeight, Height);
    SetNumber(NRI_ErrBoxWidth , Width);
    WriteDmsRegistry;
  end;
end;

procedure TfrmErrorMsg.btnTerminateClick(Sender: TObject);
begin
  if(not btnIgnore.Enabled) or (MessageDlg('This will terminate the application.' + #13#10 + 'Are you sure?', mtWarning, [mbYes, mbNo], 0) = mrYes) then
  begin
    TerminateProcess(GetCurrentProcess(), 2);
    try
       frmMain.TryToCloseForm(false);
    finally
    end;
    Application.Terminate;
  end;
end;

procedure TfrmErrorMsg.btnReopenClick(Sender: TObject);
begin
   dmfGeneral.ReOpen := true;
end;

class procedure TfrmErrorMsg.StartSourceEditor(sSelection: string);
var
  sFilename : string;
  sLine, sColumn : string;
  nPosHO, nPosC, nPosHS : integer;
  sCommand : AnsiString;
begin
  nPosHO := pos('(', sSelection);
  nPosC  := pos(',', sSelection);
  nPosHS := pos(')', sSelection);
  if nPosC = 0 then nPosC := nPosHS;

  sFileName := copy(sSelection, 1, nPosHO - 1);
  sFileName := AsWinPath(sFileName);
  if (nPosHO < nPosC) then
      sLine := copy(sSelection, nPosHO + 1, (nPosC - nPosHO) - 1)
  else
      sLine := '0';

  if (nPosC < nPosHS) then
      sColumn := copy(sSelection, nPosC  + 1, (nPosHS - nPosC) - 1)
  else
      sColumn := '0';

  if(not IsInteger(sLine))
  then Exit;

  if(not IsInteger(sColumn))
  then Exit;

  if(fileexists(sFilename))
  then begin
    sCommand := dmfGeneral.GetString(SRI_DMSEditor);
    if(length(sCommand) < 3)
    then Exit;
    sCommand := StringReplace(sCommand, '%F', sFilename, []);
    sCommand := StringReplace(sCommand, '%L', sLine, []);
    sCommand := StringReplace(sCommand, '%C', sColumn, []);
    sCommand := Config_GetFullStorageName(GetCurrentDir, sCommand);
    AppLogMessage('CMD: '+sCommand);
    WinExec(PAnsiChar(sCommand), SW_MAXIMIZE);
  end;
end;

procedure TfrmErrorMsg.mmLongMsgDblClick(Sender: TObject);
begin
//EM_XXX
  StartSourceEditor(
    mmLongMsg.Lines[
      mmLongMsg.Perform(EM_LINEFROMCHAR, mmLongMsg.SelStart, 0 )
    ]
  );

//  StartSourceEditor( lowercase(mmLongMsg.SelText) );
end;

procedure TfrmErrorMsg.FormKeyUp(Sender: TObject; var Key: Word;
  Shift: TShiftState);
begin
   if Key = VK_ESCAPE then
      btnOk.Click;
end;


end.
