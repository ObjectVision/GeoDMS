#include <imgui.h>
#include <imgui_internal.h>
#include "GuiToolbar.h"
#include "GuiStyles.h"
#include <vector>
#include <numeric>

#include "ViewPort.h"

Button::Button(ToolButtonID button_id1, GuiTextureID texture_id, int group_index, ButtonType type, std::string tooltip, bool state)
{
    m_ToolButtonId1 = button_id1;
    m_TextureId = texture_id;
    m_GroupIndex = group_index;
    m_Type = type;
    m_ToolTip = tooltip;
    m_State = state;
}

Button::Button(ToolButtonID button_id1, ToolButtonID button_id2, GuiTextureID texture_id, int group_index, ButtonType type, std::string tooltip, bool state)
{
    m_ToolButtonId1 = button_id1;
    m_ToolButtonId2 = button_id2;
    m_TextureId = texture_id;
    m_GroupIndex = group_index;
    m_Type = type;
    m_ToolTip = tooltip;
    m_State = state;
}

void Button::Update(GuiView& view)
{
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(m_TextureId).GetImage(), ImVec2(GetIcon(m_TextureId).GetWidth(), GetIcon(m_TextureId).GetHeight())))
    {
        switch (m_Type)
        {
        case ButtonType::SINGLE:
        {
            UpdateSingle(view);
            break;
        }
        case ButtonType::TOGGLE:
        {
            UpdateToggle(view);
            break;
        }
        case ButtonType::MODAL:
        {
            UpdateModal(view);
            break;
        }
        default:
        {
            break;
        }
        }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip(m_ToolTip.c_str());
    }
}

void Button::UpdateSingle(GuiView& view)
{
    SendMessage(view.GetHWND(), WM_COMMAND, m_ToolButtonId1, 0);
}

void Button::UpdateToggle(GuiView& view)
{
    if (m_State)
    {
        SendMessage(view.GetHWND(), WM_COMMAND, m_ToolButtonId2, 0);
        m_State = false;
    }
    else
    {
        SendMessage(view.GetHWND(), WM_COMMAND, m_ToolButtonId1, 0);
        m_State = true;
    }
}

void Button::UpdateModal(GuiView& view)
{

}

GuiToolbar::GuiToolbar()
{
    m_Buf.resize(1024);

    // emplace tableview buttons
    m_TableViewButtons.emplace_back(TB_TableCopy,                           GV_copy,                        -1,     ButtonType::SINGLE, "Copy To ClipBoard as SemiColon delimited Text", false);
    m_TableViewButtons.emplace_back(TB_Copy,                                GV_vcopy,                       -1,     ButtonType::SINGLE, "Copy the visible contents of the active view to the Clipboard", false);
    m_TableViewButtons.emplace_back(TB_Export,                              GV_save,                        -1,     ButtonType::MODAL, "Save to file as semicolon delimited text", false);                 // SHV_DataView_GetExportInfo(m_DataView, @nrRows, @nrCols, @nrDotRows, @nrDotCols, @fullFileNameBaseStr) then format as 569 dmscontrol.pas
    m_TableViewButtons.emplace_back(TB_ZoomSelectedObj,                     MV_table_show_first_selected,   -1,     ButtonType::SINGLE, "Show the first selected row", false);
    m_TableViewButtons.emplace_back(TB_SelectRows,                          MV_table_select_row,            -1,     ButtonType::SINGLE, "Select objects in the active layer by the mouse (use Ctrl for deselection)", false);
    m_TableViewButtons.emplace_back(TB_SelectAll,                           MV_select_all,                  -1,     ButtonType::SINGLE, "Select all objects in the active layer", false);
    m_TableViewButtons.emplace_back(TB_SelectNone,                          MV_select_none,                 -1,     ButtonType::SINGLE, "Deselect all objects in the active layer", false);
    m_TableViewButtons.emplace_back(TB_ShowSelOnlyOn, TB_ShowSelOnlyOff,    MV_show_selected_features,       2,     ButtonType::TOGGLE, "Show only selected rows", false);
    m_TableViewButtons.emplace_back(TB_TableGroupBy,                        GV_group_by,                     1,     ButtonType::SINGLE, "Group by the selected columns", false);
}

void GuiToolbar::ShowMapViewButtons(GuiView& view)
{
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(GuiTextureID::MV_zoom_all_layers).GetImage(), ImVec2(GetIcon(GuiTextureID::MV_zoom_all_layers).GetWidth(), GetIcon(GuiTextureID::MV_zoom_all_layers).GetHeight())))
    {
        SendMessage(view.GetHWND(), WM_COMMAND, ToolButtonID::TB_ZoomAllLayers, 0);
    };
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("Make the extents of all layers fit in the ViewPort of the active view");
    }
    
}

void GuiToolbar::ShowTableViewButtons(GuiView& view)
{
    // TB_Neutral

    for (auto& button : m_TableViewButtons)
    {
        button.Update(view);
        ImGui::SameLine();
    }
}

void GuiToolbar::Update(bool* p_open, GuiView& view)
{
    if (!ImGui::Begin("Toolbar", p_open, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::End();
        return;
    }
   
    // focus window when clicked
    if (ImGui::IsWindowHovered() && ImGui::IsAnyMouseDown())
        SetKeyboardFocusToThisHwnd();

    if (view.m_Views.at(view.m_ViewIndex).m_ViewStyle == tvsTableView)
    {
        ShowTableViewButtons(view);
    }
    else if (view.m_Views.at(view.m_ViewIndex).m_ViewStyle == tvsMapView)
    {
        ShowMapViewButtons(view);
    }

    
    //ImGui::SameLine();
    //if (ImGui::ImageButton((void*)(intptr_t)GetIcon(GV_save).GetImage(), ImVec2(GetIcon(GV_save).GetWidth(), GetIcon(GV_save).GetHeight()))) 
    //{
        /*auto view = GetActiveView();
        if (view)
            view->SendMessage(WM_COMMAND, ToolButtonID::TB_Copy, 0, nullptr);
        // TODO: access eventhandler directly (also for treeview)*/
    //};
    
    /*
    ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(GV_copy).GetImage(), ImVec2(GetIcon(GV_copy).GetWidth(), GetIcon(GV_copy).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(GV_vcopy).GetImage(), ImVec2(GetIcon(GV_vcopy).GetWidth(), GetIcon(GV_vcopy).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(GV_group_by).GetImage(), ImVec2(GetIcon(GV_group_by).GetWidth(), GetIcon(GV_group_by).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_toggle_layout_1).GetImage(), ImVec2(GetIcon(MV_toggle_layout_1).GetWidth(), GetIcon(MV_toggle_layout_1).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_toggle_layout_2).GetImage(), ImVec2(GetIcon(MV_toggle_layout_2).GetWidth(), GetIcon(MV_toggle_layout_2).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_toggle_layout_3).GetImage(), ImVec2(GetIcon(MV_toggle_layout_3).GetWidth(), GetIcon(MV_toggle_layout_3).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_toggle_palette).GetImage(), ImVec2(GetIcon(MV_toggle_palette).GetWidth(), GetIcon(MV_toggle_palette).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_toggle_scalebar).GetImage(), ImVec2(GetIcon(MV_toggle_scalebar).GetWidth(), GetIcon(MV_toggle_scalebar).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_zoom_active_layer).GetImage(), ImVec2(GetIcon(MV_zoom_active_layer).GetWidth(), GetIcon(MV_zoom_active_layer).GetHeight()))) {}; ImGui::SameLine();
    ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_zoom_selected).GetImage(), ImVec2(GetIcon(MV_zoom_selected).GetWidth(), GetIcon(MV_zoom_selected).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_zoomin).GetImage(), ImVec2(GetIcon(MV_zoomin).GetWidth(), GetIcon(MV_zoomin).GetHeight()))) {}; ImGui::SameLine();
    if (ImGui::ImageButton((void*)(intptr_t)GetIcon(MV_zoomout).GetImage(), ImVec2(GetIcon(MV_zoomout).GetWidth(), GetIcon(MV_zoomout).GetHeight()))) {}; ImGui::SameLine();
    */

    ImGui::End();

}