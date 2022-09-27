#include <imgui.h>
#include "GuiOptions.h"
#include "utl/Environment.h"


void ShowOptionsWindow(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(800,400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Options", p_open, ImGuiWindowFlags_None))
    {
        if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("GUI"))
            {
                static bool dark_mode = false;
                ImGui::Checkbox("Dark mode", &dark_mode);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Advanced"))
            {
                ImGui::Text("Paths");
                ImGui::Separator();
                ImGui::Text("External Programs");
                ImGui::Separator();
                ImGui::Text("Parallel Processing");
                static bool pp0 = IsMultiThreaded0();
                static bool pp1 = IsMultiThreaded1();
                static bool pp2 = IsMultiThreaded2();
                static bool pp3 = IsMultiThreaded3();
                static DWORD flags = GetRegStatusFlags();

                if (ImGui::Checkbox("0: Suspend View Updates to Favor GUI", &pp0))
                    SetGeoDmsRegKeyDWord("StatusFlags", pp0? flags|=RSF_SuspendForGUI: flags &=~RSF_SuspendForGUI);

                if (ImGui::Checkbox("1: Tile/Segment Tasks", &pp1))
                    SetGeoDmsRegKeyDWord("StatusFlags", pp1 ? flags|=RSF_MultiThreading1 : flags &= ~RSF_MultiThreading1);

                if (ImGui::Checkbox("2: Multiple Operations Simultaneously", &pp2))
                    SetGeoDmsRegKeyDWord("StatusFlags", pp2 ? flags|=RSF_MultiThreading2 : flags &= ~RSF_MultiThreading2);

                if (ImGui::Checkbox("3: Pipelined Tile Operations", &pp3))
                    SetGeoDmsRegKeyDWord("StatusFlags", pp3 ? flags|=RSF_MultiThreading3 : flags &= ~RSF_MultiThreading3);

                
                ImGui::Separator();

                static int treshold_mem_flush = 100; // RTC_GetRegDWord(RegDWordEnum i);
                ImGui::Text("Flush Treshold");
                ImGui::SameLine();
                if (ImGui::SliderInt("##Treshold for Memory Flushing Wait Procedure", &treshold_mem_flush, 50, 100))
                {
                    if (treshold_mem_flush > 100)
                        treshold_mem_flush = 100;
                    else if (treshold_mem_flush<50)
                        treshold_mem_flush = 50;

                    //RTC_SetRegDWord(RegDWordEnum i, DWORD dw);
                }
                ImGui::SameLine();
                ImGui::Text("%%");
                //ImGui::InputInt("Treshold for Memory Flushing Wait Procedure", );
                //extern "C" RTC_CALL UInt32 DMS_CONV RTC_GetRegDWord(RegDWordEnum i);
                //extern "C" RTC_CALL void   DMS_CONV RTC_SetRegDWord(RegDWordEnum i, DWORD dw);


                static bool tracelog_file = false;
                ImGui::Checkbox("TraceLogFile", &tracelog_file);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Configuration"))
            {
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

    }
    ImGui::End();
}

void ShowEventLogOptionsWindow(bool* p_open)
{
    auto m_State = GuiState();
    if (ImGui::Begin("Options eventlog", p_open, NULL))
    {
        ImGui::Checkbox("show minor-trace messages", &m_State.m_OptionsEventLog.ShowMessageTypeMinorTrace);
        ImGui::Checkbox("show major-trace messages", &m_State.m_OptionsEventLog.ShowMessageTypeMajorTrace);
        ImGui::Checkbox("show warning messages", &m_State.m_OptionsEventLog.ShowMessageTypeWarning);
        ImGui::Checkbox("show error messages", &m_State.m_OptionsEventLog.ShowMessageTypeError);
        ImGui::Checkbox("show nothing messages", &m_State.m_OptionsEventLog.ShowMessageTypeNothing);

        ImGui::End();
    }
}