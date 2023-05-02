#include <imgui.h>
#include <imgui_internal.h>
#include "GuiToolbar.h"
#include "GuiStyles.h"
#include <vector>
#include <numeric>

#include "ViewPort.h"

Button::Button(ToolButtonID button_id1, GuiTextureID texture_id, int group_index, ButtonType type, std::string tooltip, UInt4 state)
{
    m_ToolButtonId1 = button_id1;
    m_TextureId = texture_id;
    m_GroupIndex = group_index;
    m_Type = type;
    m_ToolTip = tooltip;
    m_State = state;
}

Button::Button(ToolButtonID button_id1, ToolButtonID button_id2, GuiTextureID texture_id, int group_index, ButtonType type, std::string tooltip, UInt4 state)
{
    m_ToolButtonId1 = button_id1;
    m_ToolButtonId2 = button_id2;
    m_TextureId = texture_id;
    m_GroupIndex = group_index;
    m_Type = type;
    m_ToolTip = tooltip;
    m_State = state;
}

Button::Button(ToolButtonID button_id1, ToolButtonID button_id2, ToolButtonID button_id3, GuiTextureID texture_id, int group_index, ButtonType type, std::string tooltip, UInt4 state)
{
    m_ToolButtonId1 = button_id1;
    m_ToolButtonId2 = button_id2;
    m_ToolButtonId3 = button_id3;
    m_TextureId = texture_id;
    m_GroupIndex = group_index;
    m_Type = type;
    m_ToolTip = tooltip;
    m_State = state;
}

auto Button::GetGroupIndex() -> int
{
    return m_GroupIndex;
}

void Button::Update(GuiViews& view)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
    const ImVec2 uv0(0.1f, 0.1f);
    const ImVec2 uv1(0.9f, 0.9f);
    //const ImVec2 uv0(0.0f, 0.0f);
    //const ImVec2 uv1(1.0f, 1.0f);

    //const ImVec2 button_size(32.0f,32.0f); 
    //const ImVec2 target_button_size(25.0f, 25.0f);

    //const ImVec2 uv0(((button_size.x-target_button_size.x)/2.0f) / button_size.x, (button_size.y - target_button_size.y) / button_size.y);
    //const ImVec2 uv1(target_button_size.x / button_size.x, target_button_size.y / button_size.y);
    const ImVec4 background_color(0, 0, 1, 0);
    auto texture_size = ImVec2(GetIcon(m_TextureId).GetWidth(), GetIcon(m_TextureId).GetHeight());
    //auto texture_size = ImVec2(25, 25);
    auto test = GetIcon(m_TextureId).GetId();

    //if (ImGui::ImageButton(GetIcon(m_TextureId).GetId().data(), (void*)(intptr_t)GetIcon(m_TextureId).GetImage(), texture_size, uv0, uv1, background_color)) // , const ImVec2 & uv0 = ImVec2(0, 0), const ImVec2 & uv1 = ImVec2(1, 1), const ImVec4 & bg_col = ImVec4(0, 0, 0, 0), const ImVec4 & tint_col = ImVec4(1, 1, 1, 1)
    
    
    //IMGUI_API void          Image(ImTextureID user_texture_id, const ImVec2 & size, const ImVec2 & uv0 = ImVec2(0, 0), const ImVec2 & uv1 = ImVec2(1, 1), const ImVec4 & tint_col = ImVec4(1, 1, 1, 1), const ImVec4 & border_col = ImVec4(0, 0, 0, 0));
    ImGui::Image((void*)(intptr_t)GetIcon(m_TextureId).GetImage(), texture_size);//, uv0, uv1);
    if (ImGui::IsItemClicked())
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
        case ButtonType::TRISTATE:
        {
            UpdateTristate(view);
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
    ImGui::PopStyleColor();
}

void Button::UpdateSingle(GuiViews& view)
{
    SendMessage(view.GetHWND(), WM_COMMAND, m_ToolButtonId1, 0);
}

void Button::UpdateToggle(GuiViews& view)
{
    switch (m_State)
    {
    case 0: // untoggled
    {
        SendMessage(view.GetHWND(), WM_COMMAND, m_ToolButtonId1, 0);
        m_State = 1;
        break;
    }
    case 1: // toggled
    {
        SendMessage(view.GetHWND(), WM_COMMAND, m_ToolButtonId2, 0);
        m_State = 0;
        break;
    }
    }
}

void Button::UpdateTristate(GuiViews& view)
{
    switch (m_State)
    {
    case 0:
    {
        SendMessage(view.GetHWND(), WM_COMMAND, m_ToolButtonId2, 0);
        m_State = 1;
        break;
    }
    case 1:
    {
        SendMessage(view.GetHWND(), WM_COMMAND, m_ToolButtonId3, 0);
        m_State = 2;
        break;
    }
    case 2:
    {
        SendMessage(view.GetHWND(), WM_COMMAND, m_ToolButtonId1, 0);
        m_State = 0;
        break;
    }
    }
}

void Button::UpdateModal(GuiViews& view)
{
    auto export_info = view.m_dms_view_it->m_DataView->GetExportInfo();
    SendMessage(view.GetHWND(), WM_COMMAND, m_ToolButtonId1, 0);
    
    if (view.m_dms_view_it->m_ViewStyle == tvsMapView)
        reportF(SeverityTypeID::ST_MajorTrace, "Exporting current viewport to bitmap in %s", export_info.m_FullFileNameBase);
    else
        reportF(SeverityTypeID::ST_MajorTrace, "Exporting current table to csv in %s", export_info.m_FullFileNameBase);
}

GuiToolbar::GuiToolbar()
{
    // TableView buttons
    m_TableViewButtons.emplace_back(TB_Export,                              GV_save,                        0,     ButtonType::MODAL, "Save to file as semicolon delimited text", false);                 // SHV_DataView_GetExportInfo(m_DataView, @nrRows, @nrCols, @nrDotRows, @nrDotCols, @fullFileNameBaseStr) then format as 569 dmscontrol.pas
    m_TableViewButtons.emplace_back(TB_TableCopy,                           GV_copy,                        0,     ButtonType::SINGLE, "Copy as semicolon delimited text to Clipboard ", false);
    m_TableViewButtons.emplace_back(TB_Copy,                                GV_vcopy,                       0,     ButtonType::SINGLE, "Copy the visible contents as image to Clipboard", false);
    
    m_TableViewButtons.emplace_back(TB_ZoomSelectedObj,                     MV_table_show_first_selected,   1,     ButtonType::SINGLE, "Show the first selected row", false);
    m_TableViewButtons.emplace_back(TB_SelectRows,                          MV_table_select_row,            1,     ButtonType::SINGLE, "Select row(s) by mouse-click (use Shift to add or Ctrl to deselect)", false);
    m_TableViewButtons.emplace_back(TB_SelectAll,                           MV_select_all,                  1,     ButtonType::SINGLE, "Select all rows", false);
    m_TableViewButtons.emplace_back(TB_SelectNone,                          MV_select_none,                 1,     ButtonType::SINGLE, "Deselect all rows", false);
    m_TableViewButtons.emplace_back(TB_ShowSelOnlyOn, TB_ShowSelOnlyOff,    MV_show_selected_features,      1,     ButtonType::TOGGLE, "Show only selected rows", false);
    
    m_TableViewButtons.emplace_back(TB_TableGroupBy,                        GV_group_by,                     2,     ButtonType::SINGLE, "Group by the highlighted columns", false);

    // MapView buttons
    m_MapViewButtons.emplace_back(TB_Export, GV_save,   0, ButtonType::MODAL, "Export the viewport data to bitmaps file(s) using the export settings and the current ROI", false);
    m_MapViewButtons.emplace_back(TB_Copy, GV_copy,     0, ButtonType::SINGLE, "Copy the visible contents of the viewport to the Clipboard", false);
    m_MapViewButtons.emplace_back(TB_CopyLC, GV_vcopy,  0, ButtonType::SINGLE, "Copy the full contents of the LayerControlList to the Clipboard", false);

    m_MapViewButtons.emplace_back(TB_ZoomAllLayers,   MV_zoom_all_layers,     1, ButtonType::SINGLE, "Make the extents of all layers fit in the ViewPort", false);
    m_MapViewButtons.emplace_back(TB_ZoomActiveLayer, MV_zoom_active_layer,   1, ButtonType::SINGLE, "Make the extent of the active layer fit in the ViewPort", false);
    m_MapViewButtons.emplace_back(TB_ZoomIn2,         MV_zoomin_button,       1, ButtonType::SINGLE, "Zoom in by drawing a rectangle", false);
    m_MapViewButtons.emplace_back(TB_ZoomOut2,        MV_zoomout_button,      1, ButtonType::SINGLE, "Zoom out by clicking on a ViewPort location", false);
    
    m_MapViewButtons.emplace_back(TB_ZoomSelectedObj, MV_zoom_selected, 2, ButtonType::SINGLE, "Make the extent of the selected elements fit in the ViewPort", false);
    m_MapViewButtons.emplace_back(TB_SelectObject, MV_select_object, 2, ButtonType::SINGLE, "Select elements in the active layer by mouse-click(use Shift to add or Ctrl to deselect)", false);
    m_MapViewButtons.emplace_back(TB_SelectRect, MV_select_rect, 2, ButtonType::SINGLE, "Select elements in the active layer by drawing a rectangle(use Shift to add or Ctrl to deselect)", false);
    m_MapViewButtons.emplace_back(TB_SelectCircle, MV_select_circle, 2, ButtonType::SINGLE, "Select elements in the active layer by drawing a circle(use Shift to add or Ctrl to deselect)", false);
    m_MapViewButtons.emplace_back(TB_SelectPolygon, MV_select_poly, 2, ButtonType::SINGLE, "Select elements in the active layer by drawing a polygon(use Shift to add or Ctrl to deselect)", false);
    m_MapViewButtons.emplace_back(TB_SelectDistrict, MV_select_district, 2, ButtonType::SINGLE, "Select contiguous regions in the active layer by clicking on them (use Shift to add or Ctrl to deselect)", false);
    m_MapViewButtons.emplace_back(TB_SelectAll, MV_select_all, 2, ButtonType::SINGLE, "Select all elements in the active layer", false);
    m_MapViewButtons.emplace_back(TB_SelectNone, MV_select_none, 2, ButtonType::SINGLE, "Deselect all elements in the active layer", false);
    m_MapViewButtons.emplace_back(TB_ShowSelOnlyOn, TB_ShowSelOnlyOff, MV_show_selected_features, 2, ButtonType::TOGGLE, "Show only selected elements", false);

    m_MapViewButtons.emplace_back(TB_Show_VP, TB_Show_VPLC, TB_Show_VPLCOV, MV_toggle_layout_1, 3, ButtonType::TRISTATE, "Toggle the layout of the ViewPort between{MapView only, with LayerControlList, with Overview and LayerControlList", false);
    m_MapViewButtons.emplace_back(TB_SP_All, TB_SP_Active, TB_SP_None, MV_toggle_palette, 3, ButtonType::TRISTATE, "Toggle Palette Visibiliy between{All, Active Layer Only, None}", false);
    m_MapViewButtons.emplace_back(TB_NeedleOn, TB_NeedleOff, MV_toggle_needle, 3, ButtonType::TOGGLE, "Show / Hide NeedleControler", false);
    m_MapViewButtons.emplace_back(TB_ScaleBarOn, TB_ScaleBarOff, MV_toggle_scalebar, 3, ButtonType::TOGGLE, "Show / Hide ScaleBar", false);
    
    

}

void GuiToolbar::ShowMapViewButtons(GuiViews& view)
{
    auto cur_group_index = 0;
    for (auto& button : m_MapViewButtons)
    {
        if (cur_group_index == button.GetGroupIndex())
            ImGui::SameLine(0.0f, 0.0f);
        else
            ImGui::SameLine(0.0f, 25.0f); // gap between button groups // TODO: move style parameters to separate code unit?

        cur_group_index = button.GetGroupIndex();
        button.Update(view);
    }
}

void GuiToolbar::ShowTableViewButtons(GuiViews& view)
{
    auto cur_group_index = 0;
    for (auto& button : m_TableViewButtons)
    {
        if (cur_group_index == button.GetGroupIndex())
            ImGui::SameLine(0.0f, 0.0f);
        else
            ImGui::SameLine(0.0f, 25.0f); // gap between button groups // TODO: move style parameters to separate code unit?

        cur_group_index = button.GetGroupIndex();
        button.Update(view);
    }
}

auto GetToolBarDockNode(GuiState& state) -> ImGuiDockNode*
{
    auto ctx = ImGui::GetCurrentContext();
    ImGuiDockContext* dc = &ctx->DockContext;
    auto dockspace_docknode = (ImGuiDockNode*)dc->Nodes.GetVoidPtr(state.dockspace_id);
    ImGuiDockNode* toolbar_docknode = nullptr;
    if (dockspace_docknode->ChildNodes[0])
        toolbar_docknode = dockspace_docknode->ChildNodes[0]->ChildNodes[1]->ChildNodes[1];

    return toolbar_docknode;
}

void GuiToolbar::Update(bool* p_open, GuiState& state, GuiViews& view) // TODO: add int return to button which is its group. Untoggle all buttons in the same group.
{
    // TODO: on selection what toolbar to use: dataview.cpp, lines 1318 and SetMdiToolbar fmain.pas 670
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(117.0f/255.0f, 117.0f/255.0f, 138.0f/255.0f, 1.0f));

    if (!ImGui::Begin("Toolbar", p_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar) || view.m_dms_views.empty())
    {
        AutoHideWindowDocknodeTabBar(is_docking_initialized);
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    AutoHideWindowDocknodeTabBar(is_docking_initialized);
   
    // focus window when clicked
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    if (view.m_dms_view_it!=view.m_dms_views.end())
    {
        if (view.m_dms_view_it->m_ViewStyle == tvsTableView)
            ShowTableViewButtons(view);
        else if (view.m_dms_view_it->m_ViewStyle == tvsMapView)
            ShowMapViewButtons(view);
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}