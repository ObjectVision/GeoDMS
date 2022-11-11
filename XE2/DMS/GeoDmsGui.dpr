program GeoDmsGui;

uses
  Forms,
  uDMSInterface in '..\General\DmsInterface\uDMSInterface.pas',
  uDMSInterfaceTypes in '..\General\DmsInterface\uDMSInterfaceTypes.pas',
  uDmsRegistry in '..\General\DmsInterface\uDmsRegistry.pas',
  fMain in '..\General\fMain.pas' {frmMain},
  dmGeneral in '..\General\dmGeneral.pas' {dmfGeneral: TDataModule},
  uDMSUtils in '..\General\DmsInterface\uDMSUtils.pas',
  fMdiChild in '..\General\fMdiChild.pas' {frmMdiChild},
  fPrimaryDataViewer in '..\General\fPrimaryDataViewer.pas' {frmPrimaryDataViewer},
  fmBasePrimaryDataControl in '..\General\fmBasePrimaryDataControl.pas' {fraBasePrimaryDataControl: TFrame},
  fmDmsControl in '..\General\fmDmsControl.pas' {fraDmsControl: TFrame},
  uViewAction in '..\General\uViewAction.pas',
  uDMSDesktopFunctions in '..\General\uDMSDesktopFunctions.pas',
  fOpenDesktop in '..\General\fOpenDesktop.pas' {frmOpenDesktop},
  fEditProperty in '..\General\fEditProperty.pas' {frmEditProperty},
  fExpresEdit in '..\General\fExpresEdit.pas' {frmExprEdit},
  fHistogramParameters in '..\General\fHistogramParameters.pas' {frmHistogramParameters},
  fmHistogramControl in '..\General\fmHistogramControl.pas' {fraHistogramControl: TFrame},
  fExportASCGrid in '..\General\DmsInterface\fExportASCGrid.pas' {frmExportASCGrid},
  fExportBitmap in '..\General\DmsInterface\fExportBitmap.pas' {frmExportBitmap},
  fErrorMsg in '..\General\fErrorMsg.pas' {frmErrorMsg},
  fSettings in '..\General\fSettings.pas' {frmSettings},
  uMapClasses in '..\General\uMapClasses.pas',
  Configuration in '..\General\Configuration.pas',
  uAppGeneral in '..\General\uAppGeneral.pas',
  ItemColumnProvider in '..\General\ItemColumnProvider.pas',
  DMSProjectInterface in '..\General\DMSProjectInterface.pas',
  SysUtils,
  Windows,
  uGenLib in '..\General\uGenLib.pas',
  uDMSTreeviewFunctions in '..\General\uDMSTreeviewFunctions.pas',
  uStartupController in '..\General\uStartupController.pas',
  uTreeViewUtilities in '..\General\uTreeViewUtilities.pas',
  TreeItemPickList in '..\General\TreeItemPickList.pas' {TreeItemPickList},
  fTreeItemPickList in '..\General\fTreeItemPickList.pas' {frmTreeItemPickList},
  OptionalInfo in '..\General\OptionalInfo.pas' {frmOptionalInfo},
  TreeNodeUpdateSet in '..\General\TreeNodeUpdateSet.pas';

{$R *.RES}

var
  sConfigStart: string;
  StartupController : TStartupController;
  StartupDir : String;

begin
   with Application do
   begin
      Initialize;
      MainFormOnTaskBar := True;
      HintPause     :=  300;
      HintHidePause := 7000;

      with FormatSettings do
      begin
        DecimalSeparator := '.';
        ThousandSeparator := ',';
      end;

      DMS_Appl_SetExeDir(PFileChar(FileString(copy(ExtractFilePath(Application.ExeName), 1, length(ExtractFilePath(Application.ExeName)) - 1))));

      StartupDir := GetCurrentDir;
      StartupController := TStartupController.Create;

      try
         StartupController.DetermineMode;
      except
         on E: EInvalidCommandLine do
         begin
            DispMessage(E.ToString);
            Exit;
         end;
      end;

      // create the main form and datamodule
      CreateForm(TdmfGeneral, dmfGeneral);
      CreateForm(TfrmMain, g_frmMain);

      (* Try to load a configfile. The StartupController will serve us an ordered list
       * of configs to try. If that list runs out, and none was succesfull, then
       * we simply start without a configfile.
       *)
      if not StartupController.IsEmpty then
      repeat
         SetCurrentDir(StartupDir); // make sure we start cleanly on each iteration

         sConfigStart := StartupController.ActiveConfigFile;
         sConfigStart := GetMainDMSfile(sConfigStart);

         if  not g_frmMain.LoadConfigurationAndProjectDll(FileString( sConfigStart ), '', true, false) then
            StartupController.OnConfigFailed;

      until TConfiguration.CurrentIsAssigned
        or dmfGeneral.Terminating
        or not StartupController.SelectNextConfig;

      StartupController.Free;

      if not dmfGeneral.Terminating then
      begin
         if g_frmMain.tvTreeItem.visible then
           g_frmMain.tvTreeItem.setFocus; // focus on the treeitem
         Run;
      end;
   end;
end.

