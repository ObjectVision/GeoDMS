; DmsSetupScript
;--------------------------------

!define GeoDmsVersion "$%GeoDmsVersion%"
!ifndef GeoDmsBinDir
  !define GeoDmsBinDir "..\bin\Release\${GeoDmsPlatform}"
!endif
!echo "Current Version: {GeoDmsVersion}"
Name "GeoDms${GeoDmsVersion}.${GeoDmsFlavor} for ${GeoDmsPlatform}"


; The file to write
OutFile "..\distr\GeoDms${GeoDmsVersion}.${GeoDmsFlavor}-Setup-${GeoDmsPlatform}.exe"

; The default installation directory
InstallDir ${PlatformPF}\ObjectVision\GeoDms${GeoDmsVersion}.${GeoDmsFlavor}

; Request application privileges for Windows Vista
RequestExecutionLevel admin ; required for writing in program files and start menu

;--------------------------------

; Pages

Page directory
Page instfiles

;--------------------------------

; The stuff to install
Section "GeoDMS Program Folder" ;No components page, name is not important

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  CreateDirectory $INSTDIR
  File ${GeoDmsBinDir}\GeoDmsRun.exe
  File ${GeoDmsBinDir}\GeoDmsGuiQt.exe
  File ${GeoDmsBinDir}\RewriteExpr.lsp
  File ${GeoDmsBinDir}\prelude.dms
  File ${GeoDmsBinDir}\profiler.py
  File ${GeoDmsBinDir}\regression.py

!ifdef GeoDmsGlobio
  ; The G output is wiped before every release build and its dependency-closure
  ; verifier rejects gdal.dll, so package its validated DLL set as one unit.
  File ${GeoDmsBinDir}\*.dll
!else
  File ${GeoDmsBinDir}\Rtc.dll
  File ${GeoDmsBinDir}\Shv.dll
  File ${GeoDmsBinDir}\Stg.dll
  File ${GeoDmsBinDir}\Stx.dll
  File ${GeoDmsBinDir}\Clc.dll
  File ${GeoDmsBinDir}\Geo.dll

  File ${GeoDmsBinDir}\Qt6Core.dll
  File ${GeoDmsBinDir}\Qt6Gui.dll
  File ${GeoDmsBinDir}\Qt6Svg.dll
  File ${GeoDmsBinDir}\Qt6Widgets.dll
  File ${GeoDmsBinDir}\tinyxml2.dll
  
  File ${GeoDmsBinDir}\vccorlib140.dll
  File ${GeoDmsBinDir}\vcomp140.dll
  File ${GeoDmsBinDir}\vcruntime140.dll
  File ${GeoDmsBinDir}\vcruntime140_1.dll
  File ${GeoDmsBinDir}\vcruntime140_threads.dll
  File ${GeoDmsBinDir}\concrt140.dll
  File ${GeoDmsBinDir}\msvcp140.dll
  File ${GeoDmsBinDir}\msvcp140_1.dll
  File ${GeoDmsBinDir}\msvcp140_2.dll
  File ${GeoDmsBinDir}\msvcp140_atomic_wait.dll
  File ${GeoDmsBinDir}\msvcp140_codecvt_ids.dll

  File ${GeoDmsBinDir}\fftw3.dll
  File ${GeoDmsBinDir}\fftw3f.dll
  File ${GeoDmsBinDir}\gdal.dll
  File ${GeoDmsBinDir}\tiff.dll
  File ${GeoDmsBinDir}\arrow.dll
  File ${GeoDmsBinDir}\brotlicommon.dll
  File ${GeoDmsBinDir}\brotlidec.dll
  File ${GeoDmsBinDir}\brotlienc.dll
  File ${GeoDmsBinDir}\bz2.dll
  File ${GeoDmsBinDir}\freexl-1.dll
  File ${GeoDmsBinDir}\geos.dll
  File ${GeoDmsBinDir}\geos_c.dll
  File ${GeoDmsBinDir}\geotiff.dll
  File ${GeoDmsBinDir}\gif.dll
  File ${GeoDmsBinDir}\gmp-10.dll
  File ${GeoDmsBinDir}\hdf5.dll
  File ${GeoDmsBinDir}\hdf5_hl.dll
  File ${GeoDmsBinDir}\iconv-2.dll
  File ${GeoDmsBinDir}\jpeg62.dll
  File ${GeoDmsBinDir}\json-c.dll
  File ${GeoDmsBinDir}\Lerc.dll
  File ${GeoDmsBinDir}\libcrypto-3-x64.dll
  File ${GeoDmsBinDir}\libcurl.dll
  File ${GeoDmsBinDir}\libexpat.dll
  File ${GeoDmsBinDir}\liblzma.dll
  File ${GeoDmsBinDir}\libpng16.dll
  File ${GeoDmsBinDir}\LIBPQ.dll
  File ${GeoDmsBinDir}\libsharpyuv.dll
  File ${GeoDmsBinDir}\libssl-3-x64.dll
  File ${GeoDmsBinDir}\libwebp.dll
  File ${GeoDmsBinDir}\libxml2.dll
  File ${GeoDmsBinDir}\lz4.dll
  File ${GeoDmsBinDir}\minizip.dll
  File ${GeoDmsBinDir}\mpfr-6.dll
  File ${GeoDmsBinDir}\netcdf.dll
  File ${GeoDmsBinDir}\openjp2.dll
  File ${GeoDmsBinDir}\parquet.dll
  File ${GeoDmsBinDir}\pcre2-8.dll
  File ${GeoDmsBinDir}\proj_9.dll
  File ${GeoDmsBinDir}\qhull_r.dll
  File ${GeoDmsBinDir}\snappy.dll
  File ${GeoDmsBinDir}\spatialite.dll
  File ${GeoDmsBinDir}\sqlite3.dll
  File ${GeoDmsBinDir}\szip.dll
  File ${GeoDmsBinDir}\z.dll
  File ${GeoDmsBinDir}\zstd.dll
!endif

  ; The pre-package verifier requires the exact configured ABI matrix, so this
  ; wildcard cannot silently admit a stale or missing binding.
  File ${GeoDmsBinDir}\geodms.cp*-${GeoDmsPythonPlatformTag}.pyd

  File ..\res\NotePadPlusPlus\GeoDMS_npp_def.xml
  
  WriteUninstaller $INSTDIR\uninstaller.exe

  ; Register in Windows "Apps & Features" so the install is visible there
  ; and can be removed via the standard system UI. Subkey includes flavor
  ; so side-by-side .m/.c/.g/.l installs of the same version each get their
  ; own row. See issue #499.
  !define UninstKey "Software\Microsoft\Windows\CurrentVersion\Uninstall\GeoDms${GeoDmsVersion}.${GeoDmsFlavor}"
  !if "${GeoDmsPlatform}" == "x64"
    SetRegView 64
  !endif
  WriteRegStr   HKLM "${UninstKey}" "DisplayName"          "GeoDMS ${GeoDmsVersion}.${GeoDmsFlavor} (${GeoDmsPlatform})"
  WriteRegStr   HKLM "${UninstKey}" "DisplayVersion"       "${GeoDmsVersion}.${GeoDmsFlavor}"
  WriteRegStr   HKLM "${UninstKey}" "Publisher"            "Object Vision B.V."
  WriteRegStr   HKLM "${UninstKey}" "URLInfoAbout"         "https://github.com/ObjectVision/GeoDMS"
  WriteRegStr   HKLM "${UninstKey}" "InstallLocation"      "$INSTDIR"
  WriteRegStr   HKLM "${UninstKey}" "DisplayIcon"          "$INSTDIR\GeoDmsGuiQt.exe"
  WriteRegStr   HKLM "${UninstKey}" "UninstallString"      '"$INSTDIR\uninstaller.exe"'
  WriteRegStr   HKLM "${UninstKey}" "QuietUninstallString" '"$INSTDIR\uninstaller.exe" /S'
  WriteRegDWORD HKLM "${UninstKey}" "NoModify" 1
  WriteRegDWORD HKLM "${UninstKey}" "NoRepair" 1

  SetOutPath $INSTDIR\gdaldata
  File ${GeoDmsBinDir}\gdaldata\*.*
  
  SetOutPath $INSTDIR\generic
  File ${GeoDmsBinDir}\generic\*.*
  
  SetOutPath $INSTDIR\iconengines
  File ${GeoDmsBinDir}\iconengines\*.*
  
  SetOutPath $INSTDIR\imageformats
  File ${GeoDmsBinDir}\imageformats\*.*
  
  SetOutPath $INSTDIR\misc\fonts
  File ${GeoDmsBinDir}\misc\fonts\*.*
  
  SetOutPath $INSTDIR\networkinformation
  File ${GeoDmsBinDir}\networkinformation\*.*
  
  SetOutPath $INSTDIR\platforms
  File ${GeoDmsBinDir}\platforms\*.*
  
  SetOutPath $INSTDIR\proj4data
  File ${GeoDmsBinDir}\proj4data\*.*

  SetOutPath $INSTDIR\styles
  File ${GeoDmsBinDir}\styles\*.*
  
  SetOutPath $INSTDIR\tls
  File ${GeoDmsBinDir}\tls\*.*
  
  SetOutPath $INSTDIR\examples
  File ${GeoDmsBinDir}\examples\*.*

  ; typed-function testcases battery (also the regression suite: run via
  ; examples\testcases\run_testcases.bat against the installed GeoDmsRun)
  SetOutPath $INSTDIR\examples\testcases
  File ${GeoDmsBinDir}\examples\testcases\*.*

  SetOutPath $INSTDIR\library
  File ${GeoDmsBinDir}\library\*.*

  SetOutPath $INSTDIR\library\basedata_nl
  File ${GeoDmsBinDir}\library\basedata_nl\*.*

  SetOutPath $INSTDIR\library\basedata_nl\rdc
  File ${GeoDmsBinDir}\library\basedata_nl\rdc\*.*

  SetOutPath $INSTDIR\library\geometry
  File ${GeoDmsBinDir}\library\geometry\*.*

  IfSilent skip_set_all
    MessageBox MB_YESNO 'Install startmenu shortcuts for all users?' IDNO skip_set_all
    SetShellVarContext all
  skip_set_all:

  CreateDirectory "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}"
  ; SW_SHOWNORMAL, not SW_SHOWMAXIMIZED: Explorer passes a shortcut's "Run:" field to
  ; CreateProcess as STARTUPINFO.wShowWindow, and Windows lets that OVERRIDE the nCmdShow of
  ; the process's first ShowWindow call, so a maximized shortcut beat the window placement the
  ; GUI restores at startup -- the window flashed at its remembered spot and was maximized by
  ; the shell right after. Harmless while the GUI always came up maximized anyway; since 20.0.0
  ; it remembers its placement, so the flag has to go.
  CreateShortCut "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\GeoDms Qt GUI ${GeoDmsVersion}.lnk" "$INSTDIR\GeoDmsGuiQt.exe"   "" "$INSTDIR\GeoDmsGuiQt.exe"   0 SW_SHOWNORMAL    "" "Preview the new GeoDMS GuiQt"
  CreateShortCut "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\uninstall.lnk" "$INSTDIR\uninstaller.exe" "" "$INSTDIR\uninstaller.exe" 0 SW_SHOWNORMAL    "" "Remove the Geographic Data & Model Software"
 
  
SectionEnd ; end the section

Section "Install VSCode extension"

    ; gebruik context zoals eerder ingesteld
    ; (current of all)

    CreateDirectory "$PROFILE\.vscode"
    CreateDirectory "$PROFILE\.vscode\extensions"
    CreateDirectory "$PROFILE\.vscode\extensions\local.geodms-language-0.0.2"

    SetOutPath "$PROFILE\.vscode\extensions\local.geodms-language-0.0.2"
    File /r "..\res\Visual Studio Code\local.geodms-language-0.0.2\*"

SectionEnd


Section uninstall

  Delete $INSTDIR\GeoDmsRun.exe
  Delete $INSTDIR\GeoDmsGuiQt.exe
  Delete $INSTDIR\RewriteExpr.lsp
  Delete $INSTDIR\profiler.py
  Delete $INSTDIR\regression.py
  Delete $INSTDIR\*.dll
  Delete $INSTDIR\geodms*.pyd
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
  Delete $INSTDIR\library\geometry
  Delete $INSTDIR\library\basedata_nl\rdc
  Delete $INSTDIR\library\basedata_nl
  Delete $INSTDIR\examples\testcases\*.*
  Delete $INSTDIR\examples

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
  RMDIR $INSTDIR\library\geometry
  RMDIR $INSTDIR\library\basedata_nl\rdc
  RMDIR $INSTDIR\library\basedata_nl
  RMDIR $INSTDIR\library
  RMDIR $INSTDIR\examples\testcases
  RMDIR $INSTDIR\examples

  ; Remove the Apps & Features entry created at install time (issue #499).
  !if "${GeoDmsPlatform}" == "x64"
    SetRegView 64
  !endif
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GeoDms${GeoDmsVersion}.${GeoDmsFlavor}"

  RMDIR $INSTDIR

SectionEnd ; end the section
  
