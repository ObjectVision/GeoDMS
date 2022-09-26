#include <imgui.h>
#include <imgui_internal.h>
#include "GuiToolbar.h"
#include "GuiStyles.h"
#include <vector>
#include <numeric>

#include "ViewPort.h"

GuiToolbar::GuiToolbar()
{
    m_Buf.resize(1024);
}

void GuiToolbar::Update(bool* p_open)
{
    if (!ImGui::Begin("Toolbar", p_open, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::End();
        return;
    }
   
    //auto im = GetIcon(GV_save).GetImage();

    ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(GV_save).GetImage(), ImVec2(GetIcon(GV_save).GetWidth(), GetIcon(GV_save).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(GV_copy).GetImage(), ImVec2(GetIcon(GV_copy).GetWidth(), GetIcon(GV_copy).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(GV_vcopy).GetImage(), ImVec2(GetIcon(GV_vcopy).GetWidth(), GetIcon(GV_vcopy).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(GV_group_by).GetImage(), ImVec2(GetIcon(GV_group_by).GetWidth(), GetIcon(GV_group_by).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_toggle_layout_1).GetImage(), ImVec2(GetIcon(MV_toggle_layout_1).GetWidth(), GetIcon(MV_toggle_layout_1).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_toggle_layout_2).GetImage(), ImVec2(GetIcon(MV_toggle_layout_2).GetWidth(), GetIcon(MV_toggle_layout_2).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_toggle_layout_3).GetImage(), ImVec2(GetIcon(MV_toggle_layout_3).GetWidth(), GetIcon(MV_toggle_layout_3).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_toggle_palette).GetImage(), ImVec2(GetIcon(MV_toggle_palette).GetWidth(), GetIcon(MV_toggle_palette).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_toggle_scalebar).GetImage(), ImVec2(GetIcon(MV_toggle_scalebar).GetWidth(), GetIcon(MV_toggle_scalebar).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_zoom_active_layer).GetImage(), ImVec2(GetIcon(MV_zoom_active_layer).GetWidth(), GetIcon(MV_zoom_active_layer).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_zoom_all_layers).GetImage(), ImVec2(GetIcon(MV_zoom_all_layers).GetWidth(), GetIcon(MV_zoom_all_layers).GetHeight()))) 
    {
        // OnCommand(ToolButtonID::TB_ZoomAllLayers);
        // GetContents()->OnCommand(TB_ZoomAllLayers);
        
        
    }; 
    ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_zoom_selected).GetImage(), ImVec2(GetIcon(MV_zoom_selected).GetWidth(), GetIcon(MV_zoom_selected).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_zoomin).GetImage(), ImVec2(GetIcon(MV_zoomin).GetWidth(), GetIcon(MV_zoomin).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_zoomout).GetImage(), ImVec2(GetIcon(MV_zoomout).GetWidth(), GetIcon(MV_zoomout).GetHeight()))) {}; ImGui::SameLine();


    ImGui::End();

}