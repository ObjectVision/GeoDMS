// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__STX_INTERFACE_H)
#define __STX_INTERFACE_H

#include "StxBase.h"

SYNTAX_CALL SharedStr ProcessADMS(const TreeItem* context, CharPtr url);

// External callable functions
SYNTAX_CALL TreeItem* CreateTreeFromConfiguration(CharPtr sourceFilename);

extern "C" {

SYNTAX_CALL TreeItem*      DMS_CONV DMS_CreateTreeFromConfiguration(CharPtr sourceFilename);
SYNTAX_CALL TreeItem*      DMS_CONV DMS_CreateTreeFromString       (CharPtr configString);
SYNTAX_CALL TreeItem*      DMS_CONV DMS_CreateTreeFromString       (CharPtr configString); // TODO: duplicate, remove
SYNTAX_CALL IStringHandle  DMS_CONV DMS_ProcessADMS                (const TreeItem* context, CharPtr url);
SYNTAX_CALL bool           DMS_CONV DMS_ProcessPostData            (      TreeItem* context, CharPtr postData, UInt32 dataSize);
SYNTAX_CALL void           DMS_CONV DMS_Stx_Load();

}



#endif // __STX_INTERFACE_H