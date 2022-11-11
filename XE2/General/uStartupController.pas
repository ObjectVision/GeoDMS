unit uStartupController;

interface

uses Contnrs, SysUtils;

type
   EInvalidCommandLine = class(Exception);

   TStartupController = class
   private
      FConfigSources : TStack;
   public
      constructor Create;
      destructor  Destroy; override;
      procedure   DetermineMode;

      function    IsEmpty: Boolean;
      function    ActiveConfigFile : String;
      function    SelectNextConfig : Boolean;
      procedure   OnConfigFailed;
   end;

implementation

uses Dialogs, dmGeneral, uAppGeneral, Windows, uDMSInterface, uDMSInterfaceTypes;

const
   PARAM_OPTIONAL_DMSFILE = '/fallback';
   PARAM_NO_DMSFILE = '/noconfig';

type
   // interface representing a source for a configfile
   TConfigSource = class
      public
         function  GetConfigFile : String; virtual; abstract;
         procedure OnConfigFailure; virtual; abstract;
   end;

   // implementation of TConfigSource which represents the last config file
   TLastConfig = class(TConfigSource)
      public
         function  GetConfigFile : String; override;
         procedure OnConfigFailure; override;
   end;

   // implementation of TConfigSource which represents the command line supplied
   // config file
   TCommandLineConfig = class(TConfigSource)
      private
         FParamIdx : Integer;
      public
         constructor Create(param : Integer);
         function    GetConfigFile : String; override;
         procedure   OnConfigFailure; override;
   end;


   (*
    *  TStartupController
    *)
   constructor TStartupController.Create;
   begin
      FConfigSources := TStack.Create;
   end;

   destructor TStartupController.Destroy;
   begin
      while FConfigSources.Count > 0 do
        SelectNextConfig;
      FConfigSources.Free;
      inherited;
   end;

   // there are three modes in which we can start up, depending on the commandline parameters:
   // no parameters   : Try loading the last config
   // /noconfig       : don't try loading any config
   // config          : Only try loading the parameter config
   // /fallback config: Try loading the last config, if that fails, read parameter config
   //
   // in any case, no config or failure of loading any config results in a Open Config Dialog

   procedure TStartupController.DetermineMode;
   var c, s: Integer; p: SourceString;
   label endLoop;
   begin
      c := ParamCount;
      s := 0;
      while c>0 do
      begin
        p := SourceString(ParamStr(s+1));
        if not RTC_ParseRegStatusFlag(PSourceChar(p)) then goto endLoop;
        DEC(c);
        INC(s);
      end;
    endLoop:
      With FConfigSources do
      begin
         if (c = 0) then
            Push(TLastConfig.Create)
         else if (c = 1) then
         begin
            if ParamStr(s+1) <> PARAM_NO_DMSFILE then
              Push(TCommandLineConfig.Create(s+1))
         end
         else if (c = 2) and (ParamStr(s+1) = PARAM_OPTIONAL_DMSFILE) then
         begin
            Push(TCommandLineConfig.Create(s+2));
            Push(TLastConfig.Create);
         end
         else
            Raise EInvalidCommandLine.Create('Invalid command line parameters specified. Use [' + PARAM_NO_DMSFILE + ' | [' + PARAM_OPTIONAL_DMSFILE + '] configfile.dms');
      end;
   end;

   function TStartupController.ActiveConfigFile : String;
   begin
      Result := TConfigSource(FConfigSources.Peek).GetConfigFile;
   end;

   function TStartupController.IsEmpty: Boolean;
   begin
      Result := (FConfigSources.Count <= 0);
   end;

   function TStartupController.SelectNextConfig: Boolean;
   begin
      TConfigSource(FConfigSources.Pop).Free;
      Result := not IsEmpty;
   end;

   procedure TStartupController.OnConfigFailed;
   begin
      TConfigSource(FConfigSources.Peek).OnConfigFailure;
   end;



   (*
    *  TLastConfig
    *)
   function TLastConfig.GetConfigFile: String;
   begin
      Result := dmfGeneral.GetString(SRI_LastConfigFile);
   end;

   procedure TLastConfig.OnConfigFailure;
   begin
   end;



   (*
    *  TCommandLineConfig
    *)
   constructor TCommandLineConfig.Create(param : Integer);
   begin
      FParamIdx := param;
   end;

   function TCommandLineConfig.GetConfigFile: String;
   begin
      Result := ParamStr(FParamIdx);
   end;

   procedure TCommandLineConfig.OnConfigFailure;
   begin
      DispMessage('The specified config file ('+GetConfigFile+') did not load correctly.');
   end;

end.
