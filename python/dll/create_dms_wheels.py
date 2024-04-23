import os
import shutil
import glob

def get_wheel_intermediate_folder(target, output_folder):
    intermediate_folder = f"{output_folder}{target[0]}/" 
    return intermediate_folder

def get_list_of_to_be_copied_files_from_dms_release(build_output:str) -> list:
    full_copy_list = glob.glob(f"{build_output}*.dll")
    copy_list = []
    for file in full_copy_list:
        if any(x in file for x in ["Qt6", "Shv", "D3D", "opengl32"]):
            continue
        copy_list.append(file)
    
    copy_list.append(f"{build_output}geodms.pyd")
    return copy_list

def copy_setup_and_init_files_over(intermediate_fldr:str, dst):
    os.makedirs(os.path.dirname(intermediate_fldr), exist_ok=True)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy("./setup.py", f"{intermediate_fldr}setup.py")
    shutil.copy("./__init__.py", f"{dst}__init__.py")
    return

def copy_gdal_proj_data_folders(build_output:str, dst:str):
    shutil.copytree(f"{build_output}/proj4data", f"{dst}/proj4data/")
    shutil.copytree(f"{build_output}/gdaldata", f"{dst}/gdaldata/")
    return

def copy_files_for_wheel_packaging(target, build_output, output_folder):
    intermediate_folder = get_wheel_intermediate_folder(target, output_folder)
    copy_list = get_list_of_to_be_copied_files_from_dms_release(build_output)
    destination_package_folder = f"{intermediate_folder}geodms/"
    copy_setup_and_init_files_over(intermediate_folder, destination_package_folder)

    for file in copy_list:
        shutil.copy(file, destination_package_folder)

    copy_gdal_proj_data_folders(build_output, destination_package_folder)

    return

def make_compile_command(compiler:str, target:tuple) -> str:
    compile_part = f'/permissive- /MP /ifcOutput "..\\..\\obj\\Release\\x64\\PyDms" /analyze:external- /GS /external:anglebrackets /GL /analyze /W3 /Gy /Zc:wchar_t /I"{target[1]}include" /I"./src" /I"..\\..\\sym\\dll\\src" /I"..\\..\\tic\\dll\\src" /I"..\\..\\rtc\\dll\\src" /I"..\\..\\geo\\dll\\src" /I"..\\..\\clc\\dll\\include" /I"..\\..\\stx\\dll\\src" /I"..\\..\\shv\\dll\\src" /I"..\\..\\stg\\dll\\src" /guard:cf /Zi /Gm- /O2 /sdl /Fd"..\\..\\obj\\Release\\x64\\PyDms\\vc143.pdb" /Zc:inline /fp:precise /external:W0 /D "_CONSOLE" /D "NDEBUG" /D "_ITERATOR_DEBUG_LEVEL=0" /D "_CRT_SECURE_NO_WARNINGS" /D "BOOST_SPIRIT_USE_OLD_NAMESPACE" /D "BOOST_SPIRIT_THREADSAFE" /D "_WINDLL" /errorReport:prompt /GF /WX- /Zc:forScope /GR /Gd /Oy /Oi /MD /std:c++latest /FC /Fa"..\\..\\obj\\Release\\x64\\PyDms" /EHsc /nologo /Fo"..\\..\\obj\\Release\\x64\\PyDms" /Fp"..\\..\\obj\\Release\\x64\\PyDms\\PyDms.pch" /diagnostics:column ./Src/Bindings.cpp'
    #                       /permissive- /MP /ifcOutput "..\..\obj\Release\x64\PyDms\" /analyze:external- /GS /external:anglebrackets /GL /analyze /W3 /Gy /Zc:wchar_t /I"C:\Users\Cicada\dev\python_installations\3_12\include" /I"./src" /I"../../sym/dll/src" /I"../../tic/dll/src" /I"../../rtc/dll/src" /I"../../geo/dll/src" /I"../../clc/dll/include" /I"../../stx/dll/src" /I"../../shv/dll/src" /I"../../stg/dll/src" /guard:cf /Zi /Gm- /O2 /sdl /Fd"..\..\obj\Release\x64\PyDms\vc143.pdb" /Zc:inline /fp:precise /external:W0 /D "_CONSOLE" /D "NDEBUG" /D "_ITERATOR_DEBUG_LEVEL=0" /D "_CRT_SECURE_NO_WARNINGS" /D "BOOST_SPIRIT_USE_OLD_NAMESPACE" /D "BOOST_SPIRIT_THREADSAFE" /D "_WINDLL" /errorReport:prompt /GF /WX- /Zc:forScope /GR /Gd /Oy /Oi /MD /std:c++latest /FC /Fa"..\..\obj\Release\x64\PyDms\" /EHsc /nologo /Fo"..\..\obj\Release\x64\PyDms\" /Fp"..\..\obj\Release\x64\PyDms\PyDms.pch" /diagnostics:column 
    #compile_part = '/permissive- /MP /ifcOutput "..\..\obj\Release\x64\PyDms\" /analyze:external- /GS /external:anglebrackets /GL /analyze /W3 /Gy /Zc:wchar_t /I"C:\Users\Cicada\dev\python_installations\3_12\include" /I"./src" /I"../../sym/dll/src" /I"../../tic/dll/src" /I"../../rtc/dll/src" /I"../../geo/dll/src" /I"../../clc/dll/include" /I"../../stx/dll/src" /I"../../shv/dll/src" /I"../../stg/dll/src" /guard:cf /Zi /Gm- /O2 /sdl /Fd"..\..\obj\Release\x64\PyDms\vc143.pdb" /Zc:inline /fp:precise /external:W0 /D "_CONSOLE" /D "NDEBUG" /D "_ITERATOR_DEBUG_LEVEL=0" /D "_CRT_SECURE_NO_WARNINGS" /D "BOOST_SPIRIT_USE_OLD_NAMESPACE" /D "BOOST_SPIRIT_THREADSAFE" /D "_WINDLL" /errorReport:prompt /GF /WX- /Zc:forScope /GR /Gd /Oy /Oi /MD /std:c++latest /FC /Fa"..\..\obj\Release\x64\PyDms\" /EHsc /nologo /Fo"..\..\obj\Release\x64\PyDms\" /Fp"..\..\obj\Release\x64\PyDms\PyDms.pch" /diagnostics:column' 
    link_part = ""# f'/OUT:"../../bin/Release/x64/geodms.pyd" /MANIFEST /LTCG /NXCOMPAT /PDB:"../../bin/Release/x64/PyDms.pdb" /DYNAMICBASE "{target[1]}/libs/*.lib" "kernel32.lib" "user32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib" "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib" "odbc32.lib" "odbccp32.lib" /STACK:"67108864"",8192" /LARGEADDRESSAWARE /IMPLIB:"../../bin/Release/x64/PyDms.lib" /DEBUG /DLL /MACHINE:X64 /OPT:REF /INCREMENTAL:NO /PGD:"../../bin/Release/x64/PyDms.pgd" /SUBSYSTEM:CONSOLE /MANIFESTUAC:"level=\'asInvoker\' uiAccess=\'false\'" /ManifestFile:"../../obj/Release/x64/PyDms/PyDms.pyd.intermediate.manifest" /LTCGOUT:"../../obj/Release/x64/PyDms/PyDms.iobj" /OPT:ICF /ERRORREPORT:PROMPT /ILK:"../../obj/Release/x64/PyDms/PyDms.ilk" /NOLOGO /TLBID:1 '
    cl_compile_command = f"{compiler} {compile_part}"# /link {link_part}"
    return cl_compile_command

def build_wheels():
    # parameters
    configuration  = "Release"
    dms_version    = "14.17.3"
    build_output   = f"../../bin/{configuration}/x64/"
    output_folder  = f"../../bin/{configuration}/Python/"
    python_targets = [("py312", "C:\\Users\\Cicada\\dev\\python_installations\\3_12\\")]
    compiler = 'C:/"Program Files"/"Microsoft Visual Studio"/2022/Community/VC/Tools/MSVC/14.39.33519/bin/Hostx64/x64/cl.exe'
    cleanup_intermediate_files = False
    
    for target in python_targets:
        # compile geodms python bindings for python target
        #cl_command = make_compile_command(compiler, target)
        #os.system(cl_command)
        wheel_intermediate_folder = get_wheel_intermediate_folder(target, output_folder)
        shutil.rmtree(wheel_intermediate_folder)
        
        # copy over build results
        copy_files_for_wheel_packaging(target, build_output, output_folder)
        
        # package geodms as wheel
        # https://peps.python.org/pep-0427/: 
        #{distribution}-{version}(-{build tag})?-{python tag}-{abi tag}-{platform tag}.whl.
        wheel_filename = f"{output_folder}geodms-{dms_version}-{target[0]}-none-win_amd64.whl"
        os.system(f"pip wheel {wheel_intermediate_folder} -w {wheel_intermediate_folder}")
        shutil.copy(f"{wheel_intermediate_folder}geodms-0.0.0-py3-none-any.whl", wheel_filename)
    if cleanup_intermediate_files:
        shutil.rmtree(wheel_intermediate_folder)

    return

if __name__ == "__main__":
    build_wheels()