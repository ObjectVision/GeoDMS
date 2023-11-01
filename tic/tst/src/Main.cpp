#include "RtcInterface.h"
#include "ClcInterface.h"
//#include "GeoInterface.h"
//#include "ShvInterface.h"

#include "MlModel.h"
#include "SystemTest.h"

#include <iostream>
#include <functional>
#include <algorithm>
#include <vector>

// ============== Main

#include "dbg/debug.h"
#include "dbg/DebugLog.h"
#include "dbg/DmsCatch.h"
#include "set/IndexedStrings.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"

void reportErr(CharPtr errMsg)
{
	std::cerr << std::endl << errMsg;
}

enum Test : short { a = true };
enum : int { b = 1 };

int main(int argc, char** argv)
{
	std::cout << "kerstpzzl" << std::endl;
	for (int k = 1; k != 10; k++)
	for (int e = 1; e != 10; e++) // if (e!=k)
	for (int r = 1; r != 10; r++) // if (e != r) if (r != k)
	for (int s = 1; s != 10; s++) // if (e != s) if (s != k) if (s != r)
	for (int t = 1; t != 10; t++) // if (e != t) if (t != k) if (t != r) if (t!= s)
	for (int i = 1; i != 10; i++) // if (e != i) if (i != k) if (i != r) if (s != i) if (i!=t)
	for (int l = 1; l != 10; l++) // if (e != l) if (l != k) if (l != r) if (s != l) if (l != t) if (l!=i)
	for (int m = 1; m != 10; m++) // if (m != i) if (e != m) if (m != k) if (m != r) if (s != m) if (m != t) if (l != m)
	for (int n = 1; n != 10; n++) // if (e != n) if (n != k) if (n != r) if (s != n) if (n != t) if (n != i) if (l != n) if (n != m)
		if (
			(((((k * 10 + e) * 10 + r) * 10 + s) * 10) + t)
			+ ((((((l * 10 + e) * 10 + t) * 10 + t) * 10 + e) * 10 + r) * 10 + s)
			==
			((((((r * 10 + e) * 10 + k) * 10 + e) * 10 + n) * 10 + e) * 10 + n)
			+ (((m * 10) + e) * 10 + t) * (((t * 10 + i) * 10 + e) * 10 + n)
		)
		{
			std::cout << "MINSTREEL: " << m << i << n << s << t << r << e << e << l << std::endl;
			std::cout << "REKENLES: " << r << e << k << e << n << l << e << s << std::endl;
		}
	return 0;

	CDebugLog tracelog(SharedStr("c:/LocalData/Trace.log"));

/*
	IndexedStrings ivec;
	GetDir("C:\\LocalData\\por\\CalcCache\\*", ivec);
	return 0;

	ThreeKPlusOne();
	return 0;
*/

//	SharedStr applFullName = SharedStr( argv[0] );
//	std::replace(applFullName.begin(), applFullName.send(), '\\', DELIMITER_CHAR);
//	DMS_Appl_SetExeDir( splitFullPath( ConvertDosFileName(applFullName) ).c_str() );
	DMS_Appl_SetExeDir( splitFullPath( ConvertDosFileName(SharedStr( argv[0] )) .c_str()).c_str() );

	DBG_START("Main", "", true);
	DMS_SetGlobalCppExceptionTranslator(reportErr);

	DMS_CALL_BEGIN

		bool result = true;

		result &= DmsSystemTest();
		result &= testML();

		if (!result)
			return 1;

		return 0;

	DMS_CALL_END
	return 1;

}