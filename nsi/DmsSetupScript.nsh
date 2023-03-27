; DmsSetupScript
;--------------------------------

!define GeoDmsVersion "$%GeoDmsVersion%"
!echo "Current Version: {GeoDmsVersion}"
Name "GeoDms${GeoDmsVersion} for ${GeoDmsPlatform}"


; The file to write
OutFile "..\distr\GeoDms${GeoDmsVersion}-Setup-${GeoDmsPlatform}.exe"

; The default installation directory
InstallDir ${PlatformPF}\ObjectVision\GeoDms${GeoDmsVersion}

; Request application privileges for Windows Vista
RequestExecutionLevel admin ; required for writing in program files and start menu

;--------------------------------

; Pages

Page directory
Page instfiles

;--------------------------------

; The stuff to install
Section "" ;No components page, name is not important

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  CreateDirectory $INSTDIR
  File ..\bin\Release\${GeoDmsPlatform}\GeoDmsGui.exe
  File ..\bin\Release\${GeoDmsPlatform}\GeoDmsRun.exe
;  File ..\bin\Release\${GeoDmsPlatform}\GeoDmsImGui.exe
  File ..\bin\Release\${GeoDmsPlatform}\GeoDmsCaller.exe
  File ..\bin\Release\${GeoDmsPlatform}\RewriteExpr.lsp
  File ..\bin\Release\${GeoDmsPlatform}\*.dll
  File ..\res\readme.txt
  File ..\res\NotePadPlusPlus\GeoDMS_npp_def.xml
  
  WriteUninstaller $INSTDIR\uninstaller.exe

  SetOutPath $INSTDIR\gdaldata
  File ..\bin\Release\${GeoDmsPlatform}\gdaldata\*.*
  
  SetOutPath $INSTDIR\proj4data
  File ..\bin\Release\${GeoDmsPlatform}\proj4data\*.*

  SetOutPath $INSTDIR\misc\icons
  File ..\bin\Release\${GeoDmsPlatform}\misc\icons\*.*
  
  SetOutPath $INSTDIR\misc\fonts
  File ..\bin\Release\${GeoDmsPlatform}\misc\fonts\*.*
  
  
  MessageBox MB_YESNO 'Install startmenu shortcuts for all users?' IDNO skip_set_all
    SetShellVarContext all
  skip_set_all:

  CreateDirectory "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}"
  CreateShortCut "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\GeoDms Gui ${GeoDmsVersion}.lnk" "$INSTDIR\GeoDmsGui.exe"   "" "$INSTDIR\GeoDmsGui.exe"   0 SW_SHOWMAXIMIZED "" "Start the GeoDMS GUI"
;  CreateShortCut "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\GeoDms imGui ${GeoDmsVersion}.lnk" "$INSTDIR\GeoDmsImGui.exe"   "" "$INSTDIR\GeoDmsImGui.exe"   0 SW_SHOWMAXIMIZED "" "Preview the new GeoDMS GUI"
  CreateShortCut "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\uninstall.lnk" "$INSTDIR\uninstaller.exe" "" "$INSTDIR\uninstaller.exe" 0 SW_SHOWNORMAL    "" "Remove the Geographic Data & Model Software"
 
  
SectionEnd ; end the section


Section uninstall

  Delete $INSTDIR\GeoDmsGui.exe
;  Delete $INSTDIR\GeoDmsImGui.exe
  Delete $INSTDIR\GeoDmsRun.exe
  Delete $INSTDIR\GeoDmsCaller.exe
  Delete $INSTDIR\RewriteExpr.lsp
  Delete $INSTDIR\*.dll
  Delete $INSTDIR\readme.txt
  Delete $INSTDIR\GeoDMS_npp_def.xml

  Delete $INSTDIR\gdaldata\*.*
  Delete $INSTDIR\proj4data\*.*
  Delete $INSTDIR\misc\icons\*.*
  Delete $INSTDIR\misc\fonts\*.*

  Delete "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\GeoDms Gui ${GeoDmsVersion}.lnk"
;  Delete "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\GeoDms imGui ${GeoDmsVersion}.lnk"
  Delete "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\uninstall.lnk"
  Delete $INSTDIR\uninstaller.exe
  
  RMDIR $SMPROGRAMS\GeoDMS\version${GeoDmsVersion}
  RMDIR $SMPROGRAMS\GeoDMS
  RMDIR $INSTDIR\gdaldata
  RMDIR $INSTDIR\proj4data
  RMDIR $INSTDIR\misc\icons
  RMDIR $INSTDIR\misc\fonts
  RMDIR $INSTDIR\misc
  RMDIR $INSTDIR
  
SectionEnd ; end the section
  
