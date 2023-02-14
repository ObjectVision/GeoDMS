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
    //m_LocalDataDirPath  = not GetGeoDmsRegKey("LocalDataDir").empty() ? GetGeoDmsRegKey("LocalDataDir").c_str() : SetDefaultRegKey("LocalDataDir", "C:\\LocalData").c_str();
    //m_SourceDataDirPath = not GetGeoDmsRegKey("SourceDataDir").empty() ? GetGeoDmsRegKey("SourceDataDir").c_str() : SetDefaultRegKey("SourceDataDir", "C:\\SourceData").c_str();
    //m_DmsEditorPath     = not GetGeoDmsRegKey("DmsEditor").empty() ? GetGeoDmsRegKey("DmsEditor").c_str() : SetDefaultRegKey("DmsEditor", """%env:ProgramFiles%\\Notepad++\\Notepad++.exe"" ""%F"" -n%L").c_str();
    RestoreAdvancedSettingsFromRegistry();
}

void GuiOptions::ApplyChanges()
{
    SetGeoDmsRegKeyString("LocalDataDir", m_Options.advanced_options.local_data_dir);
    SetGeoDmsRegKeyString("SourceDataDir",m_Options.advanced_options.source_data_dir);
    SetGeoDmsRegKeyString("DmsEditor", m_Options.advanced_options.dms_editor_command);
    SetGeoDmsRegKeyDWord("StatusFlags", m_Options.advanced_options.pp0 ? m_Options.advanced_options.flags |= RSF_SuspendForGUI : m_Options.advanced_options.flags &= ~RSF_SuspendForGUI);
    SetGeoDmsRegKeyDWord("StatusFlags", m_Options.advanced_options.pp1 ? m_Options.advanced_options.flags |= RSF_MultiThreading1 : m_Options.advanced_options.flags &= ~RSF_MultiThreading1);
    SetGeoDmsRegKeyDWord("StatusFlags", m_Options.advanced_options.pp2 ? m_Options.advanced_options.flags |= RSF_MultiThreading2 : m_Options.advanced_options.flags &= ~RSF_MultiThreading2);
    SetGeoDmsRegKeyDWord("StatusFlags", m_Options.advanced_options.pp3 ? m_Options.advanced_options.flags |= RSF_MultiThreading3 : m_Options.advanced_options.flags &= ~RSF_MultiThreading3);
    
    RTC_SetRegDWord(RegDWordEnum::MemoryFlushThreshold, m_Options.advanced_options.treshold_mem_flush);
    SetGeoDmsRegKeyDWord("StatusFlags", m_Options.advanced_options.tracelog_file ? m_Options.advanced_options.flags |= RSF_TraceLogFile : m_Options.advanced_options.flags &= ~RSF_TraceLogFile);

    reportF(SeverityTypeID::ST_Warning, "Changes made to Advanced Options require a restart of GeoDMS to take effect!");
}

void GuiOptions::RestoreAdvancedSettingsFromRegistry()
{
    m_Options.advanced_options.local_data_dir = !GetGeoDmsRegKey("LocalDataDir").empty() ? GetGeoDmsRegKey("LocalDataDir").c_str() : SetDefaultRegKey("LocalDataDir", "C:\\LocalData").c_str();;
    m_Options.advanced_options.source_data_dir = !GetGeoDmsRegKey("SourceDataDir").empty() ? GetGeoDmsRegKey("SourceDataDir").c_str() : SetDefaultRegKey("SourceDataDir", "C:\\SourceData").c_str();
    m_Options.advanced_options.dms_editor_command = !GetGeoDmsRegKey("DmsEditor").empty() ? GetGeoDmsRegKey("DmsEditor").c_str() : SetDefaultRegKey("DmsEditor", """%env:ProgramFiles%\\Notepad++\\Notepad++.exe"" ""%F"" -n%L").c_str();
    m_Options.advanced_options.pp0 = IsMultiThreaded0();
    m_Options.advanced_options.pp1 = IsMultiThreaded1();
    m_Options.advanced_options.pp2 = IsMultiThreaded2();
    m_Options.advanced_options.pp3 = IsMultiThreaded3();
    m_Options.advanced_options.flags = GetRegStatusFlags();
    m_Options.advanced_options.treshold_mem_flush =  RTC_GetRegDWord(RegDWordEnum::MemoryFlushThreshold);
    m_Options.advanced_options.tracelog_file = GetRegStatusFlags()& RSF_TraceLogFile;
}

void GuiOptions::Update(bool* p_open, GuiState &state)
{
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Options", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar))
    {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            SetKeyboardFocusToThisHwnd();

        if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("GUI"))
            {
                static bool dark_mode = false;
                ImGui::Checkbox("Dark mode", &dark_mode);
                ImGui::Checkbox("Show eventlog options", &state.ShowEventLogOptionsWindow);

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Advanced"))
            {
                ImGui::Text("Local data:   "); ImGui::SameLine(); 
                
                if (ImGui::InputText("##LocalDataDir", &m_Options.advanced_options.local_data_dir, ImGuiInputTextFlags_None, InputTextCallback, nullptr))
                {
                    m_Options.advanced_options.changed = true;
                    //SetGeoDmsRegKeyString("LocalDataDir", m_LocalDataDirPath);
                }

                ImGui::Text("Source data:"); ImGui::SameLine();
                if (ImGui::InputText("##SourceData", &m_Options.advanced_options.source_data_dir, ImGuiInputTextFlags_None, InputTextCallback, nullptr))
                {
                    m_Options.advanced_options.changed = true;
                    //SetGeoDmsRegKeyString("SourceDataDir", m_SourceDataDirPath);
                }

                ImGui::Separator();
                ImGui::Text("Editor:          "); ImGui::SameLine();
                if (ImGui::InputText("##DmsEditor", &m_Options.advanced_options.dms_editor_command, ImGuiInputTextFlags_None, InputTextCallback, nullptr))
                {
                    m_Options.advanced_options.changed = true;
                    //SetGeoDmsRegKeyString("DmsEditor", m_DmsEditorPath);
                }
                ImGui::Separator();
                ImGui::Text("Parallel processing");
                //static bool pp0 = IsMultiThreaded0();
                //static bool pp1 = IsMultiThreaded1();
                //static bool pp2 = IsMultiThreaded2();
                //static bool pp3 = IsMultiThreaded3();
                //static DWORD flags = GetRegStatusFlags();

                if (ImGui::Checkbox("0: Suspend View Updates to Favor GUI", &m_Options.advanced_options.pp0))
                    m_Options.advanced_options.changed = true;

                if (ImGui::Checkbox("1: Tile/Segment Tasks", &m_Options.advanced_options.pp1))
                    m_Options.advanced_options.changed = true;

                if (ImGui::Checkbox("2: Multiple Operations Simultaneously", &m_Options.advanced_options.pp2))
                    m_Options.advanced_options.changed = true;

                if (ImGui::Checkbox("3: Pipelined Tile Operations", &m_Options.advanced_options.pp3))
                    m_Options.advanced_options.changed = true;

                ImGui::Separator();

                //static int treshold_mem_flush = 100; // RTC_GetRegDWord(RegDWordEnum i);
                ImGui::Text("Flush Treshold");
                ImGui::SameLine();
                if (ImGui::SliderInt("##Treshold for Memory Flushing Wait Procedure", &m_Options.advanced_options.treshold_mem_flush, 50, 100))
                {
                    // bounds for user keyboard input
                    if (m_Options.advanced_options.treshold_mem_flush > 100)
                        m_Options.advanced_options.treshold_mem_flush = 100;
                    else if (m_Options.advanced_options.treshold_mem_flush < 50)
                        m_Options.advanced_options.treshold_mem_flush = 50;
                    m_Options.advanced_options.changed = true;
                    
                }
                ImGui::SameLine();
                ImGui::Text("%%");

                //static bool tracelog_file = GetRegStatusFlags() & RSF_TraceLogFile;
                if (ImGui::Checkbox("TraceLogFile", &m_Options.advanced_options.tracelog_file))
                    m_Options.advanced_options.changed = true;
                    

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Configuration"))
            {
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::Separator();


        auto options_window_pos = ImGui::GetWindowPos();
        auto options_window_content_region = ImGui::GetWindowContentRegionMax();
        auto current_cursor_pos_X = ImGui::GetCursorPosX();
        ImGui::SetCursorPos(ImVec2(current_cursor_pos_X, options_window_content_region.y - 1.5*ImGui::GetTextLineHeight()));

        if (ImGui::Button("Ok", ImVec2(50, 1.5*ImGui::GetTextLineHeight())))
        {
            ApplyChanges();
            m_Options.advanced_options.changed = false;
            state.ShowOptionsWindow = false;
        }

        ImGui::BeginDisabled(!m_Options.advanced_options.changed);
        ImGui::SameLine();
        if (ImGui::Button("Apply", ImVec2(50, 1.5*ImGui::GetTextLineHeight())))
        {
            ApplyChanges();
            m_Options.advanced_options.changed = false;
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(50, 1.5*ImGui::GetTextLineHeight())))
        {
            RestoreAdvancedSettingsFromRegistry();
            m_Options.advanced_options.changed = false;
        }
        ImGui::EndDisabled();

        ImGui::End();
    }
}