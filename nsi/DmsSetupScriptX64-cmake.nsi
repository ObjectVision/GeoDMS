; DmsSetupScriptX64-cmake.nsi
; NSIS installer for GeoDMS Windows x64 — built from CMake release output
; Source: build/windows-x64-release/bin  (relative: ..\build\windows-x64-release\bin)
; Run from the nsi/ directory:
;   set GeoDmsVersion=19.5.0
;   "C:\Program Files (x86)\NSIS\makensis.exe" DmsSetupScriptX64-cmake.nsi

;--------------------------------

!define GeoDmsPlatform "x64"
!define CMakeBinDir "..\build\windows-x64-release\bin"

!define GeoDmsVersion "$%GeoDmsVersion%"
!echo "Current Version: ${GeoDmsVersion}"
Name "GeoDms${GeoDmsVersion} for ${GeoDmsPlatform} (CMake)"

; The file to write
OutFile "..\distr\GeoDms${GeoDmsVersion}-Setup-${GeoDmsPlatform}-cmake.exe"

; The default installation directory
InstallDir $PROGRAMFILES64\ObjectVision\GeoDms${GeoDmsVersion}

; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------

; Pages

Page directory
Page instfiles

;--------------------------------

Section "GeoDMS Program Folder"

  SetOutPath $INSTDIR
  CreateDirectory $INSTDIR

  ; Executables and scripts
  File ${CMakeBinDir}\GeoDmsRun.exe
  File ${CMakeBinDir}\GeoDmsGuiQt.exe
  File ${CMakeBinDir}\RewriteExpr.lsp
  File ${CMakeBinDir}\profiler.py
  File ${CMakeBinDir}\regression.py

  ; Python binding
  File ${CMakeBinDir}\geodms.pyd

  ; GeoDMS DLLs (Dm-prefixed in CMake build)
  File ${CMakeBinDir}\DmRtc.dll
  File ${CMakeBinDir}\DmShv.dll
  File ${CMakeBinDir}\DmStg.dll
  File ${CMakeBinDir}\DmStx.dll
  File ${CMakeBinDir}\DmSym.dll
  File ${CMakeBinDir}\DmTic.dll
  File ${CMakeBinDir}\DmClc.dll
  File ${CMakeBinDir}\DmGeo.dll

  ; Qt core DLLs
  File ${CMakeBinDir}\Qt6Core.dll
  File ${CMakeBinDir}\Qt6Gui.dll
  File ${CMakeBinDir}\Qt6Network.dll
  File ${CMakeBinDir}\Qt6Svg.dll
  File ${CMakeBinDir}\Qt6Widgets.dll

  ; Python runtime (pulled in by geodms.pyd)
  File /nonfatal ${CMakeBinDir}\python3*.dll

  ; DirectX / shader support (from Qt)
  File /nonfatal ${CMakeBinDir}\D3Dcompiler_47.dll
  File /nonfatal ${CMakeBinDir}\dxcompiler.dll
  File /nonfatal ${CMakeBinDir}\dxil.dll
  File /nonfatal ${CMakeBinDir}\opengl32sw.dll

  ; vcpkg third-party DLLs
  File ${CMakeBinDir}\boost_locale-vc145-mt-x64-1_88.dll
  File ${CMakeBinDir}\boost_thread-vc145-mt-x64-1_88.dll
  File ${CMakeBinDir}\fftw3.dll
  File ${CMakeBinDir}\gdal.dll
  File ${CMakeBinDir}\tiff.dll
  File ${CMakeBinDir}\arrow.dll
  File ${CMakeBinDir}\brotlicommon.dll
  File ${CMakeBinDir}\brotlidec.dll
  File ${CMakeBinDir}\brotlienc.dll
  File ${CMakeBinDir}\bz2.dll
  File ${CMakeBinDir}\freexl-1.dll
  File ${CMakeBinDir}\geos.dll
  File ${CMakeBinDir}\geos_c.dll
  File ${CMakeBinDir}\geotiff.dll
  File ${CMakeBinDir}\gif.dll
  File ${CMakeBinDir}\gmp-10.dll
  File ${CMakeBinDir}\hdf5.dll
  File ${CMakeBinDir}\hdf5_hl.dll
  File ${CMakeBinDir}\iconv-2.dll
  File ${CMakeBinDir}\jpeg62.dll
  File ${CMakeBinDir}\json-c.dll
  File ${CMakeBinDir}\Lerc.dll
  File ${CMakeBinDir}\libcrypto-3-x64.dll
  File ${CMakeBinDir}\libcurl.dll
  File ${CMakeBinDir}\libexpat.dll
  File ${CMakeBinDir}\liblzma.dll
  File ${CMakeBinDir}\libpng16.dll
  File ${CMakeBinDir}\LIBPQ.dll
  File ${CMakeBinDir}\libsharpyuv.dll
  File ${CMakeBinDir}\libssl-3-x64.dll
  File ${CMakeBinDir}\libwebp.dll
  File ${CMakeBinDir}\libxml2.dll
  File ${CMakeBinDir}\lz4.dll
  File ${CMakeBinDir}\minizip.dll
  File ${CMakeBinDir}\mpfr-6.dll
  File ${CMakeBinDir}\netcdf.dll
  File ${CMakeBinDir}\openjp2.dll
  File ${CMakeBinDir}\parquet.dll
  File ${CMakeBinDir}\pcre2-8.dll
  File ${CMakeBinDir}\proj_9.dll
  File ${CMakeBinDir}\qhull_r.dll
  File ${CMakeBinDir}\snappy.dll
  File ${CMakeBinDir}\spatialite.dll
  File ${CMakeBinDir}\sqlite3.dll
  File ${CMakeBinDir}\szip.dll
  File ${CMakeBinDir}\zlib1.dll
  File ${CMakeBinDir}\zstd.dll
  File /nonfatal ${CMakeBinDir}\uriparser.dll

  ; Notepad++ syntax definition
  File ..\res\NotePadPlusPlus\GeoDMS_npp_def.xml

  WriteUninstaller $INSTDIR\uninstaller.exe

  ; Qt plugin subdirectories
  SetOutPath $INSTDIR\generic
  File ${CMakeBinDir}\generic\*.*

  SetOutPath $INSTDIR\iconengines
  File ${CMakeBinDir}\iconengines\*.*

  SetOutPath $INSTDIR\imageformats
  File ${CMakeBinDir}\imageformats\*.*

  SetOutPath $INSTDIR\networkinformation
  File ${CMakeBinDir}\networkinformation\*.*

  SetOutPath $INSTDIR\platforms
  File ${CMakeBinDir}\platforms\*.*

  SetOutPath $INSTDIR\styles
  File ${CMakeBinDir}\styles\*.*

  SetOutPath $INSTDIR\tls
  File ${CMakeBinDir}\tls\*.*

  SetOutPath $INSTDIR\translations
  File ${CMakeBinDir}\translations\*.*

  ; Geographic data
  SetOutPath $INSTDIR\gdaldata
  File ${CMakeBinDir}\gdaldata\*.*

  SetOutPath $INSTDIR\proj4data
  File ${CMakeBinDir}\proj4data\*.*

  ; Fonts
  SetOutPath $INSTDIR\misc\fonts
  File ${CMakeBinDir}\misc\fonts\*.*

  ; DMS library scripts
  SetOutPath $INSTDIR\library
  File ${CMakeBinDir}\library\*.*

  SetOutPath $INSTDIR\library\basedata_nl
  File ${CMakeBinDir}\library\basedata_nl\*.*

  SetOutPath $INSTDIR\library\basedata_nl\rdc
  File ${CMakeBinDir}\library\basedata_nl\rdc\*.*

  SetOutPath $INSTDIR\library\geometry
  File ${CMakeBinDir}\library\geometry\*.*

  SetOutPath $INSTDIR\examples
  File ${CMakeBinDir}\examples\*.*

  ; Start menu shortcuts
  IfSilent skip_set_all
    MessageBox MB_YESNO 'Install startmenu shortcuts for all users?' IDNO skip_set_all
    SetShellVarContext all
  skip_set_all:

  CreateDirectory "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}"
  CreateShortCut "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\GeoDms Qt GUI ${GeoDmsVersion}.lnk" "$INSTDIR\GeoDmsGuiQt.exe" "" "$INSTDIR\GeoDmsGuiQt.exe" 0 SW_SHOWMAXIMIZED "" "Preview the new GeoDMS GuiQt"
  CreateShortCut "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\uninstall.lnk" "$INSTDIR\uninstaller.exe" "" "$INSTDIR\uninstaller.exe" 0 SW_SHOWNORMAL "" "Remove the Geographic Data & Model Software"

SectionEnd

;--------------------------------

Section "Install VSCode extension"

  CreateDirectory "$PROFILE\.vscode"
  CreateDirectory "$PROFILE\.vscode\extensions"
  CreateDirectory "$PROFILE\.vscode\extensions\local.geodms-language-0.0.2"

  SetOutPath "$PROFILE\.vscode\extensions\local.geodms-language-0.0.2"
  File /r "..\res\Visual Studio Code\local.geodms-language-0.0.2\*"

SectionEnd

;--------------------------------

Section uninstall

  Delete $INSTDIR\GeoDmsRun.exe
  Delete $INSTDIR\GeoDmsGuiQt.exe
  Delete $INSTDIR\RewriteExpr.lsp
  Delete $INSTDIR\profiler.py
  Delete $INSTDIR\regression.py
  Delete $INSTDIR\geodms.pyd
  Delete $INSTDIR\GeoDMS_npp_def.xml
  Delete $INSTDIR\*.dll
  Delete $INSTDIR\*.pyd

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
  Delete $INSTDIR\translations\*.*
  Delete $INSTDIR\library\geometry\*.*
  Delete $INSTDIR\library\basedata_nl\rdc\*.*
  Delete $INSTDIR\library\basedata_nl\*.*
  Delete $INSTDIR\library\*.*
  Delete $INSTDIR\examples\*.*

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
  RMDIR $INSTDIR\translations
  RMDIR $INSTDIR\library\geometry
  RMDIR $INSTDIR\library\basedata_nl\rdc
  RMDIR $INSTDIR\library\basedata_nl
  RMDIR $INSTDIR\library
  RMDIR $INSTDIR\examples
  RMDIR $INSTDIR

SectionEnd
