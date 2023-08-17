

using stx_error = std::exception;

void reportErr(CharPtr errMsg);
UINT32 str2int(CharPtr str);

enum class CommandCode {
	// MSG, WPARAM, LPARAM, 
	SendApp = 0,
	SendMain = 1,
	SendFocus = 2,
	SendActiveDmsControl = 3,
	CopyActiveDmsControl = 4, // MSG=WM_COPYDATA, WPARAM=WindowHandle, LPARAM=cds(nrRemainingNumbers, {numbers})
	DefaultView = 5,
	GOTO = 6,
	EXPAND = 8,
	DP = 9, // followed by integer page number.
	SAVE_DP = 10
};


int RunTestScript(SharedStr testScriptName, HWND hwDispatch);
