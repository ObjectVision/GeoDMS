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
    RestoreAdvancedSettingsFromRegistry();
}

bool GuiOptions::AdvancedOptionsStringValueHasBeenChanged(std::string key, std::string_view value)
{
    std::string cached_geodms_str = "";

    if (key.compare("LocalDataDir")==0)
        cached_geodms_str = GetGeoDmsRegKey("LocalDataDir").c_str();
    else if (key.compare("SourceDataDir")==0)
        cached_geodms_str = GetGeoDmsRegKey("SourceDataDir").c_str();

    return value.compare(cached_geodms_str)!=0;
}

void GuiOptions::SetAdvancedOptionsInRegistry(GuiState &state)
{
    auto event_queues = GuiEventQueues::getInstance();
    if (AdvancedOptionsStringValueHasBeenChanged("LocalDataDir", m_Options.advanced_options.local_data_dir))
    {
        SetGeoDmsRegKeyString("LocalDataDir", m_Options.advanced_options.local_data_dir);
        event_queues->MainEvents.Add(GuiEvents::ShowLocalSourceDataChangedModalWindow);
        //ImGui::OpenPopup("ChangedLDSD", ImGuiPopupFlags_AnyPopupLevel);
    }

    if (AdvancedOptionsStringValueHasBeenChanged("SourceDataDir", m_Options.advanced_options.source_data_dir))
    {
        SetGeoDmsRegKeyString("SourceDataDir", m_Options.advanced_options.source_data_dir);
        event_queues->MainEvents.Add(GuiEvents::ShowLocalSourceDataChangedModalWindow);
        //ImGui::OpenPopup("ChangedLDSD", ImGuiPopupFlags_AnyPopupLevel);
    }

    SetGeoDmsRegKeyString("DmsEditor", m_Options.advanced_options.dms_editor_command);

    //reportF(SeverityTypeID::ST_Warning, "Changes made to Advanced Options require a restart of GeoDMS to take effect!");

    SetGeoDmsRegKeyDWord("StatusFlags", m_Options.advanced_options.pp0 ? m_Options.advanced_options.flags |= RSF_SuspendForGUI : m_Options.advanced_options.flags &= ~RSF_SuspendForGUI);
    SetGeoDmsRegKeyDWord("StatusFlags", m_Options.advanced_options.pp1 ? m_Options.advanced_options.flags |= RSF_MultiThreading1 : m_Options.advanced_options.flags &= ~RSF_MultiThreading1);
    SetGeoDmsRegKeyDWord("StatusFlags", m_Options.advanced_options.pp2 ? m_Options.advanced_options.flags |= RSF_MultiThreading2 : m_Options.advanced_options.flags &= ~RSF_MultiThreading2);
    SetGeoDmsRegKeyDWord("StatusFlags", m_Options.advanced_options.pp3 ? m_Options.advanced_options.flags |= RSF_MultiThreading3 : m_Options.advanced_options.flags &= ~RSF_MultiThreading3);
    SetGeoDmsRegKeyDWord("StatusFlags", m_Options.advanced_options.tracelog_file ? m_Options.advanced_options.flags |= RSF_TraceLogFile : m_Options.advanced_options.flags &= ~RSF_TraceLogFile);

    SetGeoDmsRegKeyDWord("MemoryFlushThreshold", m_Options.advanced_options.treshold_mem_flush);
}

void GuiOptions::SetAdvancedOptionsInCache()
{
    GetLocalDataDir();
    GetSourceDataDir();

    SetCachedStatusFlag(RSF_SuspendForGUI, m_Options.advanced_options.pp0);
    SetCachedStatusFlag(RSF_MultiThreading1, m_Options.advanced_options.pp1);
    SetCachedStatusFlag(RSF_MultiThreading2, m_Options.advanced_options.pp2);
    SetCachedStatusFlag(RSF_MultiThreading3, m_Options.advanced_options.pp3);
    SetCachedStatusFlag(RSF_TraceLogFile, m_Options.advanced_options.tracelog_file);
    RTC_SetCachedDWord(RegDWordEnum::MemoryFlushThreshold, m_Options.advanced_options.treshold_mem_flush);
}

void GuiOptions::ApplyChanges(GuiState& state)
{
    SetAdvancedOptionsInRegistry(state);
    SetAdvancedOptionsInCache();
}

void GuiOptions::RestoreAdvancedSettingsFromRegistry()
{
    // strings
    m_Options.advanced_options.local_data_dir = !GetGeoDmsRegKey("LocalDataDir").empty() ? GetGeoDmsRegKey("LocalDataDir").c_str() : SetDefaultRegKey("LocalDataDir", "C:\\LocalData").c_str();
    m_Options.advanced_options.source_data_dir = !GetGeoDmsRegKey("SourceDataDir").empty() ? GetGeoDmsRegKey("SourceDataDir").c_str() : SetDefaultRegKey("SourceDataDir", "C:\\SourceData").c_str();
    m_Options.advanced_options.dms_editor_command = !GetGeoDmsRegKey("DmsEditor").empty() ? GetGeoDmsRegKey("DmsEditor").c_str() : SetDefaultRegKey("DmsEditor", """%env:ProgramFiles%\\Notepad++\\Notepad++.exe"" ""%F"" -n%L").c_str();
    
    // status flags
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
                //static bool dark_mode = false;
                //ImGui::Checkbox("Dark mode", &dark_mode);
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

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));

                ImGui::SameLine();
                ImGui::PushID(1);
                if (ImGui::Button(ICON_RI_FOLDER_OPEN))
                {
                    auto folder_name = BrowseFolder(GetLocalDataDir().c_str());//StartWindowsFileDialog(ConvertDmsFileNameAlways(GetGeoDmsRegKey("LocalDataDir")).c_str(), L"test", L"*.*");
                    if (!folder_name.empty())
                    {
                        m_Options.advanced_options.local_data_dir = folder_name;
                        m_Options.advanced_options.changed = true;
                    }
                }
                ImGui::PopID();

                ImGui::Text("Source data:"); ImGui::SameLine();
                if (ImGui::InputText("##SourceData", &m_Options.advanced_options.source_data_dir, ImGuiInputTextFlags_None, InputTextCallback, nullptr))
                {
                    m_Options.advanced_options.changed = true;
                    //SetGeoDmsRegKeyString("SourceDataDir", m_SourceDataDirPath);
                }
                ImGui::SameLine();
                ImGui::PushID(2);
                if (ImGui::Button(ICON_RI_FOLDER_OPEN))
                {
                    auto folder_name = BrowseFolder(GetSourceDataDir().c_str());//StartWindowsFileDialog(ConvertDmsFileNameAlways(GetGeoDmsRegKey("LocalDataDir")).c_str(), L"test", L"*.*");
                    if (!folder_name.empty())
                    {
                        m_Options.advanced_options.source_data_dir = folder_name;
                        m_Options.advanced_options.changed = true;
                    }
                }
                ImGui::PopID();

                ImGui::PopStyleColor(3);

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
            ApplyChanges(state);
            m_Options.advanced_options.changed = false;
            state.ShowOptionsWindow = false;
        }

        ImGui::BeginDisabled(!m_Options.advanced_options.changed);
        ImGui::SameLine();
        if (ImGui::Button("Apply", ImVec2(50, 1.5*ImGui::GetTextLineHeight())))
        {
            ApplyChanges(state);
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