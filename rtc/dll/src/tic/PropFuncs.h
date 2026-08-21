// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __TIC_PROPFUNCS_H
#define __TIC_PROPFUNCS_H

// Specific Properties

TIC_CALL TokenID   TreeItem_GetDialogType(const TreeItem* self);
TIC_CALL void      TreeItem_SetDialogType(TreeItem* self, TokenID dialogType);
TIC_CALL SharedStr TreeItem_GetDialogData(const TreeItem* item);
TIC_CALL void      TreeItem_SetDialogData(TreeItem* item, CharPtrRange dialogData);
SharedStr TreeItem_GetViewAction(const TreeItem* self);

// Specific DialogTypes

TIC_CALL bool IsClassBreakAttr(const TreeItem* adi);
TIC_CALL void MakeClassBreakAttr(AbstrDataItem* adi);
TIC_CALL bool HasMapType(const TreeItem* ti);

// cdf support

TIC_CALL bool                 HasCdfProp(const TreeItem* item);
TIC_CALL SharedStr            GetCdfProp(const TreeItem* item);
TIC_CALL const AbstrDataItem* GetCdfAttr(const TreeItem* item);

#endif // __TIC_PROPFUNCS_H
