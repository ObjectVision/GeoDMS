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
  File ..\bin\Release\${GeoDmsPlatform}\profiler.py
  File ..\bin\Release\${GeoDmsPlatform}\regression.py

  ; File ..\bin\Release\${GeoDmsPlatform}\*.dll
  File ..\bin\Release\${GeoDmsPlatform}\Rtc.dll
  File ..\bin\Release\${GeoDmsPlatform}\Shv.dll
  File ..\bin\Release\${GeoDmsPlatform}\Stg.dll
  File ..\bin\Release\${GeoDmsPlatform}\Stx.dll
  File ..\bin\Release\${GeoDmsPlatform}\Sym.dll
  File ..\bin\Release\${GeoDmsPlatform}\Tic.dll
  File ..\bin\Release\${GeoDmsPlatform}\Clc.dll
  File ..\bin\Release\${GeoDmsPlatform}\Geo.dll

  File ..\bin\Release\${GeoDmsPlatform}\Qt6Core.dll
  File ..\bin\Release\${GeoDmsPlatform}\Qt6Gui.dll
  File ..\bin\Release\${GeoDmsPlatform}\Qt6Svg.dll
  File ..\bin\Release\${GeoDmsPlatform}\Qt6Widgets.dll

  File ..\bin\Release\${GeoDmsPlatform}\vccorlib140.dll
  File ..\bin\Release\${GeoDmsPlatform}\vcomp140.dll
  File ..\bin\Release\${GeoDmsPlatform}\vcruntime140.dll
  File ..\bin\Release\${GeoDmsPlatform}\vcruntime140_1.dll
  File ..\bin\Release\${GeoDmsPlatform}\vcruntime140_threads.dll
  File ..\bin\Release\${GeoDmsPlatform}\concrt140.dll

  File ..\bin\Release\${GeoDmsPlatform}\msvcp140.dll
  File ..\bin\Release\${GeoDmsPlatform}\msvcp140_1.dll
  File ..\bin\Release\${GeoDmsPlatform}\msvcp140_2.dll
  File ..\bin\Release\${GeoDmsPlatform}\msvcp140_atomic_wait.dll
  File ..\bin\Release\${GeoDmsPlatform}\msvcp140_codecvt_ids.dll

  File ..\bin\Release\${GeoDmsPlatform}\boost_locale-vc143-mt-x64-1_88.dll
  File ..\bin\Release\${GeoDmsPlatform}\boost_thread-vc143-mt-x64-1_88.dll

  File ..\bin\Release\${GeoDmsPlatform}\gdal.dll
  File ..\bin\Release\${GeoDmsPlatform}\tiff.dll
  File ..\bin\Release\${GeoDmsPlatform}\arrow.dll
  File ..\bin\Release\${GeoDmsPlatform}\brotlicommon.dll
  File ..\bin\Release\${GeoDmsPlatform}\brotlidec.dll
  File ..\bin\Release\${GeoDmsPlatform}\brotlienc.dll
  File ..\bin\Release\${GeoDmsPlatform}\bz2.dll
  File ..\bin\Release\${GeoDmsPlatform}\freexl-1.dll
  File ..\bin\Release\${GeoDmsPlatform}\geos.dll
  File ..\bin\Release\${GeoDmsPlatform}\geos_c.dll
  File ..\bin\Release\${GeoDmsPlatform}\geotiff.dll
  File ..\bin\Release\${GeoDmsPlatform}\gif.dll
  File ..\bin\Release\${GeoDmsPlatform}\gmp-10.dll
  File ..\bin\Release\${GeoDmsPlatform}\hdf5.dll
  File ..\bin\Release\${GeoDmsPlatform}\hdf5_hl.dll
  File ..\bin\Release\${GeoDmsPlatform}\iconv-2.dll
  File ..\bin\Release\${GeoDmsPlatform}\jpeg62.dll
  File ..\bin\Release\${GeoDmsPlatform}\json-c.dll
  File ..\bin\Release\${GeoDmsPlatform}\Lerc.dll
  File ..\bin\Release\${GeoDmsPlatform}\libcrypto-3-x64.dll
  File ..\bin\Release\${GeoDmsPlatform}\libcurl.dll
  File ..\bin\Release\${GeoDmsPlatform}\libexpat.dll
  File ..\bin\Release\${GeoDmsPlatform}\liblzma.dll
  File ..\bin\Release\${GeoDmsPlatform}\libpng16.dll
  File ..\bin\Release\${GeoDmsPlatform}\LIBPQ.dll
  File ..\bin\Release\${GeoDmsPlatform}\libsharpyuv.dll
  File ..\bin\Release\${GeoDmsPlatform}\libssl-3-x64.dll
  File ..\bin\Release\${GeoDmsPlatform}\libwebp.dll
  File ..\bin\Release\${GeoDmsPlatform}\libxml2.dll
  File ..\bin\Release\${GeoDmsPlatform}\lz4.dll
  File ..\bin\Release\${GeoDmsPlatform}\minizip.dll
  File ..\bin\Release\${GeoDmsPlatform}\mpfr-6.dll
  File ..\bin\Release\${GeoDmsPlatform}\netcdf.dll
  File ..\bin\Release\${GeoDmsPlatform}\openjp2.dll
  File ..\bin\Release\${GeoDmsPlatform}\parquet.dll
  File ..\bin\Release\${GeoDmsPlatform}\pcre2-8.dll
  File ..\bin\Release\${GeoDmsPlatform}\proj_9.dll
  File ..\bin\Release\${GeoDmsPlatform}\qhull_r.dll
  File ..\bin\Release\${GeoDmsPlatform}\snappy.dll
  File ..\bin\Release\${GeoDmsPlatform}\spatialite.dll
  File ..\bin\Release\${GeoDmsPlatform}\sqlite3.dll
  File ..\bin\Release\${GeoDmsPlatform}\szip.dll
  File ..\bin\Release\${GeoDmsPlatform}\zlib1.dll
  File ..\bin\Release\${GeoDmsPlatform}\zstd.dll

  File ..\bin\Release\${GeoDmsPlatform}\geodms.pyd

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
  
  SetOutPath $INSTDIR\examples
  File ..\bin\Release\${GeoDmsPlatform}\examples\*.*

  SetOutPath $INSTDIR\library
  File ..\bin\Release\${GeoDmsPlatform}\library\*.*

  SetOutPath $INSTDIR\library\basedata_nl
  File ..\bin\Release\${GeoDmsPlatform}\library\basedata_nl\*.*

  SetOutPath $INSTDIR\library\basedata_nl\rdc
  File ..\bin\Release\${GeoDmsPlatform}\library\basedata_nl\rdc\*.*

  SetOutPath $INSTDIR\library\geometry
  File ..\bin\Release\${GeoDmsPlatform}\library\geometry\*.*

  IfSilent skip_set_all
    MessageBox MB_YESNO 'Install startmenu shortcuts for all users?' IDNO skip_set_all
    SetShellVarContext all
  skip_set_all:

  CreateDirectory "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}"
  CreateShortCut "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\GeoDms Qt GUI ${GeoDmsVersion}.lnk" "$INSTDIR\GeoDmsGuiQt.exe"   "" "$INSTDIR\GeoDmsGuiQt.exe"   0 SW_SHOWMAXIMIZED "" "Preview the new GeoDMS GuiQt"
  CreateShortCut "$SMPROGRAMS\GeoDMS\version${GeoDmsVersion}\uninstall.lnk" "$INSTDIR\uninstaller.exe" "" "$INSTDIR\uninstaller.exe" 0 SW_SHOWNORMAL    "" "Remove the Geographic Data & Model Software"
 
  
SectionEnd ; end the section


Section uninstall

  Delete $INSTDIR\GeoDmsRun.exe
  Delete $INSTDIR\GeoDmsGuiQt.exe
  Delete $INSTDIR\RewriteExpr.lsp
  Delete $INSTDIR\profiler.py
  Delete $INSTDIR\regression.py
  Delete $INSTDIR\*.dll
  Delete $INSTDIR\geodms.pyd
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
  RMDIR $INSTDIR\examples

  RMDIR $INSTDIR
  
SectionEnd ; end the section
  
