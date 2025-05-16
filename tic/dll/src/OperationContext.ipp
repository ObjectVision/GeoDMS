#pragma once

#include "OperationContext.h"
#include "act/TriggerOperator.h"

#if !defined(__TIC_OPERATIONCONTEXT_IPP)
#define __TIC_OPERATIONCONTEXT_IPP


template <typename Func>
task_status OperationContext::ScheduleItemWriter(MG_SOURCE_INFO_DECL TreeItem* item, Func&& func, const FutureSuppliers& allInterests, bool runDirect, Explain::Context* context)
{
	assert(IsMetaThread());
//	dms_assert(!m_TaskFunc);
	assert(m_Status == task_status::none);

	m_TaskFunc = std::move(func);
	m_Context = context;
	if (item)
		m_FenceNumber = item->GetFenceNumber();
	assert(m_FenceNumber);

	return Schedule(item, allInterests, runDirect); // might run inline
}

#endif //!defined(__TIC_OPERATIONCONTEXT_IPP)
