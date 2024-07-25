// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__DATACONTROLLER_H)
#define __DATACONTROLLER_H

#include "TicBase.h"

#include "LispRef.h"

#include <map>

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
	virtual SharedTreeItem MakeResult()  const = 0;
	virtual FutureData CallCalcResult(Explain::Context* context = nullptr)  const;
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

/********** DcRefListElem **********/

struct DcRefListElem
{
	~DcRefListElem()
	{
		// mitigate stack overflow issues
		while (m_Next)
		{
			OwningPtr<DcRefListElem> next = m_Next.release();
			m_Next.assign(next->m_Next.release());
			// next will be deleted by its destructor
		}
	}

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
