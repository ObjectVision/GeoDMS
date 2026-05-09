; DmsSetupScript

;--------------------------------

!define GeoDmsPlatform "x64"
!define RedistPlatform "x64"
!define PlatformPF $PROGRAMFILES64

; Flavor suffix appended to install dir + setup filename, so several toolset
; outputs of the same version can co-exist:
;   m -> msbuild  (this script)
;   c -> cmake    (DmsSetupScriptX64-cmake.nsi)
;   l -> linux    (CreateLinuxSetup.sh)
!define GeoDmsFlavor "m"

!include DmsSetupScript.nsh
