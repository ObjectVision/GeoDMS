// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__TIC_DEDICATEDATTRS_H)
#define __TIC_DEDICATEDATTRS_H

// resolution of http://www.mantis.objectvision.nl.objectvision.hosting.it-rex.nl/view.php?id=54

extern "C" {

	TIC_CALL UInt32 DMS_CONV DMS_DataItem_VisitClassBreakCandidates(const AbstrDataItem* context, TSupplCallbackFunc callback, ClientHandle clientHandle);
	TIC_CALL UInt32 DMS_CONV DMS_DomainUnit_VisitPaletteCandidates (const AbstrUnit* domain,      TSupplCallbackFunc callback, ClientHandle clientHandle);

}


#endif // !defined(__TIC_DEDICATEDATTRS_H)
