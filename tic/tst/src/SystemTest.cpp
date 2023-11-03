#include "SystemTest.h"

#include "dbg/Check.h"
#include "dbg/debug.h"

// ============== Main

#include "RtcInterface.h"
#include "ClcInterface.h"
#include "dbg/DmsCatch.h"

MG_DEBUGCODE( CLC_CALL bool ExprCalculatorTest(); )

bool DmsSystemTest()
{
	DBG_START("DmsSystem", "TEST", true);

	DMS_CALL_BEGIN

		bool result = true;
		result &= DBG_TEST("Rtc", DMS_RTC_Test());
		result &= DBG_TEST("ExplCalculatorTest", ExprCalculatorTest());

		DBG_TRACE(("DmsSystemTest %s" , result ? "OK" : "Failed"));
		return true;

	DMS_CALL_END;
	DBG_TRACE(("Exception caught"));
	return false;
}
