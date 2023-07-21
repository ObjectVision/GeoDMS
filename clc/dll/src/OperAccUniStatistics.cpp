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

#include "ClcPCH.h"
#pragma hdrstop

#include "OperAccUniNum.h"

namespace 
{
	CommonOperGroup cogSum("sum");
	CommonOperGroup cogSum_u08("sum_uint8");
	CommonOperGroup cogSum_u16("sum_uint16");
	CommonOperGroup cogSum_u32("sum_uint32");
	CommonOperGroup cogSum_u64("sum_uint64");

	CommonOperGroup cogSum_i08("sum_int8");
	CommonOperGroup cogSum_i16("sum_int16");
	CommonOperGroup cogSum_i32("sum_int32");
	CommonOperGroup cogSum_i64("sum_int64");

	CommonOperGroup cogSum_f32("sum_float32");
	CommonOperGroup cogSum_f64("sum_float64");
	CommonOperGroup cogSum_up("sum_upoint");
	CommonOperGroup cogSum_ip("sum_ipoint");
	CommonOperGroup cogSum_dp("sum_dpoint");

	CommonOperGroup cogMean("mean");
	CommonOperGroup cogVar("var");
	CommonOperGroup cogSd("sd");

	OperAccUniNum::AggrOperators<sum_total_best , sum_partial_best , typelists::ranged_unit_objects> s_SumOpers (&cogSum );

	OperAccUniNum::AggrOperators<specified_sum_func_generator< UInt8 >::total, specified_sum_func_generator< UInt8 >::partial, typelists::aints>        s_SumOpersU08(&cogSum_u08);
	OperAccUniNum::AggrOperators<specified_sum_func_generator< UInt16>::total, specified_sum_func_generator< UInt16>::partial, typelists::aints>        s_SumOpersU16(&cogSum_u16);
	OperAccUniNum::AggrOperators<specified_sum_func_generator< UInt32>::total, specified_sum_func_generator< UInt32>::partial, typelists::aints>        s_SumOpersU32(&cogSum_u32);
	OperAccUniNum::AggrOperators<specified_sum_func_generator< UInt64>::total, specified_sum_func_generator< UInt64>::partial, typelists::aints>        s_SumOpersU64(&cogSum_u64);
	OperAccUniNum::AggrOperators<specified_sum_func_generator<  Int8 >::total, specified_sum_func_generator<  Int8 >::partial, typelists::sints>        s_SumOpersI08(&cogSum_i08);
	OperAccUniNum::AggrOperators<specified_sum_func_generator<  Int16>::total, specified_sum_func_generator<  Int16>::partial, typelists::sints>        s_SumOpersI16(&cogSum_i16);
	OperAccUniNum::AggrOperators<specified_sum_func_generator<  Int32>::total, specified_sum_func_generator<  Int32>::partial, typelists::sints>        s_SumOpersI32(&cogSum_i32);
	OperAccUniNum::AggrOperators<specified_sum_func_generator<  Int64>::total, specified_sum_func_generator<  Int64>::partial, typelists::sints>        s_SumOpersI64(&cogSum_i64);
	OperAccUniNum::AggrOperators<specified_sum_func_generator<Float32>::total, specified_sum_func_generator<Float32>::partial, typelists::num_objects>  s_SumOpersF32(&cogSum_f32);
	OperAccUniNum::AggrOperators<specified_sum_func_generator<Float64>::total, specified_sum_func_generator<Float64>::partial, typelists::num_objects>  s_SumOpersF64(&cogSum_f64);
	OperAccUniNum::AggrOperators<specified_sum_func_generator< UPoint>::total, specified_sum_func_generator< UPoint>::partial, typelists::int_points>   s_SumOpersUP (&cogSum_up);
	OperAccUniNum::AggrOperators<specified_sum_func_generator< IPoint>::total, specified_sum_func_generator< IPoint>::partial, typelists::int_points>   s_SumOpersIP (&cogSum_ip);
	OperAccUniNum::AggrOperators<specified_sum_func_generator< DPoint>::total, specified_sum_func_generator< DPoint>::partial, typelists::float_points> s_SumOpersDP (&cogSum_dp);

	OperAccUniNum::AggrOperators<mean_total_best, mean_partial_best, typelists::ranged_unit_objects> s_MeanOpers(&cogMean);
	OperAccUniNum::AggrOperators<var_total_best , var_partial_best , typelists::ranged_unit_objects> s_VarOpers (&cogVar );
	OperAccUniNum::AggrOperators<std_total_best , std_partial_best , typelists::ranged_unit_objects> s_StdOpers (&cogSd  );
}
