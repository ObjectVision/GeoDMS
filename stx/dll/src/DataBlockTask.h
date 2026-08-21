// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined( __STX_DATABLOCKTASK_H)
#define __STX_DATABLOCKTASK_H

#include "DataBlockProd.h"
#include "AbstrCalculator.h"
#include "AbstrDataItem.h"

struct DataBlockTask  : AbstrCalculator // TODO G8: RENAME TO DataBlockExprKey
{
	typedef AbstrCalculator base_type;
	SYNTAX_CALL DataBlockTask(AbstrDataItem* adiCurr, 
		CharPtr begin, CharPtr end, row_id nrElems
	);
	
	SYNTAX_CALL DataBlockTask(AbstrDataItem* tiCurr, const DataBlockTask& src);
	virtual ~DataBlockTask();

	MetaInfo GetMetaInfo() const override; // TODO G8: consider non polymorphic integration with AbstrCalculator
	bool IsSourceRef() const override { return false; }

	bool IsDataBlock() const override { return true; }
	SharedStr GetExpr () const override { return SharedStr(CharPtrRange(m_DataBlock->GetStrnBeg(), m_DataBlock->GetStrnEnd())); }

private: friend struct DataBlockProd;
	const AbstrDataItem* GetContext() const { return AsDataItem(GetHolder()); }
	row_id         GetNrElems() const { return m_NrElems; }

	LispRef m_DataBlock;
	row_id  m_NrElems;
};

#endif
