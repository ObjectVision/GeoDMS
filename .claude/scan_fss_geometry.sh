#!/bin/bash
echo "=== F: fss geometry sizes ==="
find /mnt/f/SourceData/RSOpen_Priv -name 'geometry.1.dmsdata' 2>/dev/null -printf '%s\t%p\n' | sort
echo
echo "=== OneDrive fss geometry sizes ==="
find "/mnt/c/Users/MaartenHilferink/Objectvision/Object Vision - GeoDMS/SourceData/RSOpen_Priv" -name 'geometry.1.dmsdata' 2>/dev/null -printf '%s\t%p\n' | sort
echo
echo "=== All RSopen .fss files in OneDrive (broader scan) ==="
find "/mnt/c/Users/MaartenHilferink/Objectvision/Object Vision - GeoDMS/SourceData/RSopen" -name 'geometry.1.dmsdata' 2>/dev/null -printf '%s\t%p\n' | sort | head -20
echo
echo "=== Match on F: SourceData ==="
find "/mnt/f/SourceData/RSopen" -name 'geometry.1.dmsdata' 2>/dev/null -printf '%s\t%p\n' | sort | head -20
