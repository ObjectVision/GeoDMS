import sys
import os

if sys.platform == "win32":
    package_directory_pathname = os.path.dirname(os.path.dirname(__file__))   
    
    geodms_package_dir = f"{package_directory_pathname}/geodms"
    sys.path.append(geodms_package_dir)
    os.add_dll_directory(geodms_package_dir)

import geodms.geodms as geodms