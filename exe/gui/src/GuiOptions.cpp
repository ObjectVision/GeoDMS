#include <imgui.h>
#include "GuiOptions.h"


void ShowOptionsWindow(bool* p_open)
{
    if (ImGui::Begin("Options", p_open, NULL))
    {
        if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("GUI"))
            {
                static bool show_hidden_items       = false;
                static bool show_thousand_separator = false;
                static bool show_state_colors       = false;
                ImGui::Checkbox("Show Hidden Items", &show_hidden_items);
                ImGui::Checkbox("Show Thousand Separators", &show_thousand_separator);
                ImGui::Checkbox("Show State Colors in Treeview", &show_state_colors);
                ImGui::Text("Map View Color Settings");
                ImGui::Text("Default Classification Ramp Colors");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Advanced"))
            {
                ImGui::Text("Paths");
                ImGui::Text("External Programs");
                ImGui::Text("Parallel Processing");
                static bool pp0 = false;
                static bool pp1 = false;
                static bool pp2 = false;
                ImGui::Checkbox("0: Suspend View Updates to Favor GUI", &pp0);
                ImGui::Checkbox("1: Tile/Segment Tasks", &pp1);
                ImGui::Checkbox("2: Multiple Operations Simultaneously", &pp2);
                ImGui::Text("Minimum Swapsize");
                ImGui::Text("Treshold for Memory Flushing Wait Procedure");
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