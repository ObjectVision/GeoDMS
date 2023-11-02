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
  File ..\bin\Release\${GeoDmsPlatform}\GeoDmsRun.exe
  File ..\bin\Release\${GeoDmsPlatform}\GeoDmsGuiQt.exe
  File ..\bin\Release\${GeoDmsPlatform}\RewriteExpr.lsp
  File ..\bin\Release\${GeoDmsPlatform}\*.dll
  File ..\res\readme.txt
  File ..\res\NotePadPlusPlus\GeoDMS_npp_def.xml
  
  WriteUninstaller $INSTDIR\uninstaller.exe

  SetOutPath $INSTDIR\gdaldata
  File ..\bin\Release\${GeoDmsPlatform}\gdaldata\*.*
  
  SetOutPath $INSTDIR\generic
  File ..\bin\Release\${GeoDmsPlatform}\generic\*.*
  
  SetOutPath $INSTDIR\iconengines
  File ..\bin\Release\${GeoDmsPlatform}\iconengines\*.*
  
  SetOutPath $INSTDIR\imageformats
  File ..\bin\Release\${GeoDmsPlatform}\imageformats\*.*
  
  SetOutPath $INSTDIR\misc\fonts
  File ..\bin\Release\${GeoDmsPlatform}\misc\fonts\*.*
  
  SetOutPath $INSTDIR\networkinformation
  File ..\bin\Release\${GeoDmsPlatform}\networkinformation\*.*
  
  SetOutPath $INSTDIR\platforms
  File ..\bin\Release\${GeoDmsPlatform}\platforms\*.*
  
  SetOutPath $INSTDIR\proj4data
  File ..\bin\Release\${GeoDmsPlatform}\proj4data\*.*

  SetOutPath $INSTDIR\styles
  File ..\bin\Release\${GeoDmsPlatform}\styles\*.*
  
  SetOutPath $INSTDIR\tls
  File ..\bin\Release\${GeoDmsPlatform}\tls\*.*
  
;  SetOutPath $INSTDIR\translations
;  File ..\bin\Release\${GeoDmsPlatform}\translations\*.*
  
  MessageBox MB_YESNO 'Install startmenu shortcuts for all users?' IDNO skip_set_all
    SetShellVarContext all
  skip_set_all:

  CreateDirectory "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}"
  CreateShortCut "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\GeoDms Qt GUI ${GeoDmsVersion}.lnk" "$INSTDIR\GeoDmsGuiQt.exe"   "" "$INSTDIR\GeoDmsGuiQt.exe"   0 SW_SHOWMAXIMIZED "" "Preview the new GeoDMS GuiQt"
  CreateShortCut "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\uninstall.lnk" "$INSTDIR\uninstaller.exe" "" "$INSTDIR\uninstaller.exe" 0 SW_SHOWNORMAL    "" "Remove the Geographic Data & Model Software"
 
  
SectionEnd ; end the section


Section uninstall

  Delete $INSTDIR\GeoDmsGuiQt.exe
  Delete $INSTDIR\GeoDmsRun.exe
  Delete $INSTDIR\RewriteExpr.lsp
  Delete $INSTDIR\*.dll
  Delete $INSTDIR\readme.txt
  Delete $INSTDIR\GeoDMS_npp_def.xml

  Delete $INSTDIR\gdaldata\*.*
  Delete $INSTDIR\generic\*.*
  Delete $INSTDIR\iconengines\*.*
  Delete $INSTDIR\imageformats\*.*
  Delete $INSTDIR\misc\fonts\*.*
  Delete $INSTDIR\networkinformation\*.*
  Delete $INSTDIR\platforms\*.*
  Delete $INSTDIR\proj4data\*.*
  Delete $INSTDIR\styles\*.*
  Delete $INSTDIR\tls\*.*
;  Delete $INSTDIR\translations\*.*

  Delete "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\GeoDms Qt GUI ${GeoDmsVersion}.lnk"
  Delete "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\uninstall.lnk"
  Delete $INSTDIR\uninstaller.exe
  
  RMDIR $SMPROGRAMS\GeoDMS\version${GeoDmsVersion}
  RMDIR $SMPROGRAMS\GeoDMS
  RMDIR $INSTDIR\gdaldata
  RMDIR $INSTDIR\generic
  RMDIR $INSTDIR\iconengines
  RMDIR $INSTDIR\imageformats
  RMDIR $INSTDIR\misc\fonts
  RMDIR $INSTDIR\misc
  RMDIR $INSTDIR\networkinformation
  RMDIR $INSTDIR\platforms
  RMDIR $INSTDIR\proj4data
  RMDIR $INSTDIR\styles
  RMDIR $INSTDIR\tls
;  RMDIR $INSTDIR\translations
  RMDIR $INSTDIR
  
SectionEnd ; end the section
  
