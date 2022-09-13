//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#if !defined(__DATACONTROLLER_H)
#define __DATACONTROLLER_H

#include "TicBase.h"

#include "LispRef.h"

#include <map>

//#include "ser/MapStream.h"

/*REMOVE
#include "Explain.h"
#include "TreeItemSet.h"
*/
#include "TreeItem.h"
#include "TreeItemDualRef.h"


#if defined(MG_DEBUG_DATA)
#	define MG_DEBUG_DCDATA
#endif

// *****************************************************************************
// Section:     DataController Interface
// *****************************************************************************

struct DataController : TreeItemDualRef
{
	typedef TreeItemDualRef base_type;
	typedef LispRef DataControllerKey;

	TIC_CALL FutureData CalcResultWithValuesUnits()  const;
	virtual SharedTreeItem MakeResult()  const =0;
	virtual FutureData CalcResult(Explain::Context* context = nullptr)  const= 0;
	TIC_CALL FutureData CalcCertainResult()  const;
	virtual const Class* GetResultCls() const;
	LispPtr GetLispRef()    const { return m_Key; }

//	specific support for Calcrule Source Tracking
	virtual bool            HasTemplSource() const { return false; }
	virtual SharedTreeItem  GetTemplSource() const { return {}; }
	virtual bool            IsForEachTemplHolder()  const { return false; }
	virtual SharedTreeItem  GetForEachTemplSource() const { return {}; }

	virtual bool IsCalculating() const;

	bool IsFileNameKnown() const { return m_State.Get(DCF_DSM_FileNameKnown); }
	void SetFileNameKnown() const { return m_State.Set(DCF_DSM_FileNameKnown); }
	void ClearFileNameKnown() const { return m_State.Clear(DCF_DSM_FileNameKnown); }

protected:
	TIC_CALL void DoInvalidate () const override;
	TIC_CALL SharedStr GetSourceName() const override; // override Object

	DataController(LispPtr keyExpr);
	virtual ~DataController();
public:
//	TIC_CALL bool MustApplyImpl() const override;
	TIC_CALL ActorVisitState DoUpdate(ProgressState ps) override;

#if defined(MG_DEBUG_DCDATA)
public:
	SharedStr md_sKeyExpr;  friend void DataControllerExit();
#endif	

protected:
	DataControllerKey m_Key;  // expr + root of context

	DECL_RTTI(TIC_CALL, Class);
};

TIC_CALL DataControllerRef GetOrCreateDataController(LispPtr keyExpr);
TIC_CALL DataControllerRef GetExistingDataController(LispPtr keyExpr);

// *****************************************************************************
// auxiliary contructs
// *****************************************************************************

/********** DataControllerMap **********/

struct DataControllerMap : std::map<DataController::DataControllerKey, const DataController*>
{
	~DataControllerMap(); // test that everything was cleaned-up neatly (debug only)
};

/********** DcRefListElem **********/

struct DcRefListElem
{
	DataControllerRef        m_DC;
	OwningPtr<DcRefListElem> m_Next;
};

/********** DataControllerContextHandle **********/

#include "dbg/DebugContext.h"

struct DataControllerContextHandle : ContextHandle
{
	DataControllerContextHandle(const DataController* self) : m_DC(self) {}

protected:
	void GenerateDescription() override;

protected:
	const DataController* m_DC;
};

// *****************************************************************************

#endif // __DATACONTROLLER_H
