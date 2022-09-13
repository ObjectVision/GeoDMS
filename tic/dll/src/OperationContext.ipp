#pragma once

#include "OperationContext.h"
#include "act/TriggerOperator.h"

#if !defined(__TIC_OPERATIONCONTEXT_IPP)
#define __TIC_OPERATIONCONTEXT_IPP


template <typename Func>
task_status OperationContext::ScheduleItemWriter(MG_SOURCE_INFO_DECL TreeItem* item, Func&& func, const FutureSuppliers& allInterests, bool runDirect, Explain::Context* context)
{
	dms_assert(!m_TaskFunc);
	m_TaskFunc = std::move(func);
	m_Context = context;

	return Schedule(item, allInterests, runDirect); // might run inline
}

#endif //!defined(__TIC_OPERATIONCONTEXT_IPP)
