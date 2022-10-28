#include <imgui.h>
#include "GuiOptions.h"
#include "utl/Environment.h"


std::string SetDefaultRegKey(std::string key, std::string value)
{
    SetGeoDmsRegKeyString(key.c_str(), value);
    return value;
}

GuiOptions::GuiOptions()
{
    m_LocalDataDirPath.resize(2048);
    m_SourceDataDirPath.resize(2048);
    m_DmsEditorPath.resize(2048);

    auto tmp_ld = GetGeoDmsRegKey("LocalDataDir");
    if (tmp_ld.empty())
        tmp_ld = SetDefaultRegKey("LocalDataDir", "C:\\LocalData").c_str();

    auto tmp_sd = GetGeoDmsRegKey("SourceDataDir");
    if (tmp_sd.empty())
        tmp_sd = SetDefaultRegKey("SourceDataDir", "C:\\SourceData").c_str();

    auto tmp_ed = GetGeoDmsRegKey("DmsEditor");
    if (tmp_ed.empty())
        tmp_ed = SetDefaultRegKey("DmsEditor", "c:\PROGRA~1\Notepad++\Notepad++.exe %F -n%L").c_str();

    std::copy(tmp_ld.begin(), tmp_ld.end(), m_LocalDataDirPath.begin());
    std::copy(tmp_sd.begin(), tmp_sd.end(), m_SourceDataDirPath.begin());
    std::copy(tmp_ed.begin(), tmp_ed.end(), m_DmsEditorPath.begin());
}

void GuiOptions::Update(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Options", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoDocking))
    {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            SetKeyboardFocusToThisHwnd();

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
                ImGui::Text("LocalDataDir:"); ImGui::SameLine(); 
                if (ImGui::InputText("##LocalDataDir", reinterpret_cast<char*> (&m_LocalDataDirPath[0]), m_LocalDataDirPath.size(), ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    std::string LocalDataDir = {m_LocalDataDirPath.begin(), m_LocalDataDirPath.end()};
                    SetGeoDmsRegKeyString("LocalDataDir", LocalDataDir);
                }

                ImGui::Text("SourceDataDir:"); ImGui::SameLine();
                if (ImGui::InputText("##SourceDataDir", reinterpret_cast<char*> (&m_SourceDataDirPath[0]), m_SourceDataDirPath.size(), ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    std::string SourceDataDir = { m_SourceDataDirPath.begin(), m_SourceDataDirPath.end() };
                    SetGeoDmsRegKeyString("SourceDataDir", SourceDataDir);
                }

                ImGui::Text("DmsEditor:"); ImGui::SameLine();
                if (ImGui::InputText("##DmsEditor", reinterpret_cast<char*> (&m_DmsEditorPath[0]), m_DmsEditorPath.size(), ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    std::string DmsEditor = { m_DmsEditorPath.begin(), m_DmsEditorPath.end() };
                    SetGeoDmsRegKeyString("DmsEditor", DmsEditor); 
                }

                //GetLocalDataDir
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
                    SetGeoDmsRegKeyDWord("StatusFlags", pp0 ? flags |= RSF_SuspendForGUI : flags &= ~RSF_SuspendForGUI);

                if (ImGui::Checkbox("1: Tile/Segment Tasks", &pp1))
                    SetGeoDmsRegKeyDWord("StatusFlags", pp1 ? flags |= RSF_MultiThreading1 : flags &= ~RSF_MultiThreading1);

                if (ImGui::Checkbox("2: Multiple Operations Simultaneously", &pp2))
                    SetGeoDmsRegKeyDWord("StatusFlags", pp2 ? flags |= RSF_MultiThreading2 : flags &= ~RSF_MultiThreading2);

                if (ImGui::Checkbox("3: Pipelined Tile Operations", &pp3))
                    SetGeoDmsRegKeyDWord("StatusFlags", pp3 ? flags |= RSF_MultiThreading3 : flags &= ~RSF_MultiThreading3);

                ImGui::Separator();

                static int treshold_mem_flush = 100; // RTC_GetRegDWord(RegDWordEnum i);
                ImGui::Text("Flush Treshold");
                ImGui::SameLine();
                if (ImGui::SliderInt("##Treshold for Memory Flushing Wait Procedure", &treshold_mem_flush, 50, 100))
                {
                    if (treshold_mem_flush > 100)
                        treshold_mem_flush = 100;
                    else if (treshold_mem_flush < 50)
                        treshold_mem_flush = 50;

                    //RTC_SetRegDWord(RegDWordEnum i, DWORD dw);
                }
                ImGui::SameLine();
                ImGui::Text("%%");

                static bool tracelog_file = GetRegStatusFlags() & RSF_TraceLogFile;
                if (ImGui::Checkbox("TraceLogFile", &tracelog_file))
                    SetGeoDmsRegKeyDWord("StatusFlags", tracelog_file ? flags |= RSF_TraceLogFile : flags &= ~RSF_TraceLogFile);

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