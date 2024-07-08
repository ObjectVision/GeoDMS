// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__CLC_OPERACC_H)
#define __CLC_OPERACC_H

/*****************************************************************************/
//											Helper for partial accumulation result processing
/*****************************************************************************/

template<typename ResultType, typename AccumulationContainer>
void AssignResult(ResultType* result, const AccumulationContainer& resBuffer)
{
	auto resData = result->GetDataWrite();

	dms_assert(resData.size() == resBuffer.size());

	auto ri = resData.begin();
	auto
		i = resBuffer.begin(), 
		e = resBuffer.end();

	for (; i!=e; ++ri, ++i) 
		Assign( *ri, Convert<ResultType::value_type>( make_result( *i ) ) );
}

template<typename ResultType, typename AccumulationContainer>
void AssignResult(composite_cast<ResultType*> caster, const AccumulationContainer& resBuffer)
{
	AssignResult(caster.get_ptr(), resBuffer);
}

#endif
