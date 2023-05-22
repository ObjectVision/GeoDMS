// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include <imgui.h>
// DMS
#include "RtcGeneratedVersion.h"
#include "ShvDllInterface.h"
#include "TicInterface.h"
#include "ClcInterface.h"
#include "GeoInterface.h"
#include "StxInterface.h"
#include "RtcInterface.h"
#include "dbg/Debug.h"
#include "dbg/DebugLog.h"
#include "dbg/DmsCatch.h"
#include "ptr/AutoDeletePtr.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataLocks.h"

#include "GuiMain.h"
#include <windows.h>

int run_main_loop_in_se_guard(GuiMainComponent& main_component)
{
    DMS_SE_CALL_BEGIN
    if (main_component.MainLoop())
        return 1;
    else
        return 0;
    DMS_SE_CALL_END
    return GetLastExceptionCode();
}

int RunGui()
{
    GuiMainComponent gui_main_component;
    if (gui_main_component.Init())
        return 1;

    return run_main_loop_in_se_guard(gui_main_component);
}

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR     lpCmdLine,
    _In_ int       nShowCmd
)
{
    int result = 0;

    DMS_Shv_Load();

    DBG_INIT_COUT;
    result = RunGui();


    return result;
}
