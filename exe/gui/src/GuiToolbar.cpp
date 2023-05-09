#include <imgui.h>
#include <imgui_internal.h>
#include "GuiToolbar.h"
#include "GuiStyles.h"
#include <vector>
#include <numeric>

#include "ViewPort.h"

Button::Button(ToolButtonID button_id1, GuiTextureID texture_id, bool is_unique, int group_index, ButtonType type, std::string tooltip, UInt4 state)
{
    m_ToolButtonId1 = button_id1;
    m_TextureId = texture_id;
    m_IsUnique = is_unique;
    m_GroupIndex = group_index;
    m_Type = type;
    m_ToolTip = tooltip;
    m_State = state;
}

Button::Button(ToolButtonID button_id1, ToolButtonID button_id2, GuiTextureID texture_id, bool is_unique, int group_index, ButtonType type, std::string tooltip, UInt4 state)
{
    m_ToolButtonId1 = button_id1;
    m_ToolButtonId2 = button_id2;
    m_TextureId = texture_id;
    m_IsUnique = is_unique;
    m_GroupIndex = group_index;
    m_Type = type;
    m_ToolTip = tooltip;
    m_State = state;
}

Button::Button(ToolButtonID button_id1, ToolButtonID button_id2, ToolButtonID button_id3, GuiTextureID texture_id, bool is_unique, int group_index, ButtonType type, std::string tooltip, UInt4 state)
{
    m_ToolButtonId1 = button_id1;
    m_ToolButtonId2 = button_id2;
    m_ToolButtonId3 = button_id3;
    m_TextureId = texture_id;
    m_IsUnique = is_unique;
    m_GroupIndex = group_index;
    m_Type = type;
    m_ToolTip = tooltip;
    m_State = state;
}

auto Button::GetGroupIndex() -> int
{
    return m_GroupIndex;
}

bool Button::Update(GuiViews& view)
{
    bool reset_unique_button_activation_state = false;

    // determine background color
    auto button_background_color = ImVec4(0.f, 0.f, 0.f, 0.f);
    if ((m_Type == ButtonType::SINGLE || m_Type == ButtonType::TOGGLE) && m_State == 1)
        button_background_color = ImVec4(66.0f/255.0f, 150.0f/255.0f, 250.0f/255.0f, 255.0f/255.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, button_background_color);
    auto texture_size = ImVec2(GetIcon(m_TextureId).GetWidth(), GetIcon(m_TextureId).GetHeight());

    if (ImGui::ImageButton(m_ToolTip.c_str(), (void*)(intptr_t)GetIcon(m_TextureId).GetImage(), texture_size, {0, 0.1f}, {1, 1.1f}))
    {
        switch (m_Type)
        {
        case ButtonType::SINGLE:
        {
            if (m_IsUnique)
            {
                m_State = m_State ? 0 : 1;
                reset_unique_button_activation_state = true;
            }
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

    return reset_unique_button_activation_state;
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
    m_TableViewButtons.emplace_back(TB_Export,                              GV_save,                        false, 0,     ButtonType::MODAL, "Save to file as semicolon delimited text", false);                 // SHV_DataView_GetExportInfo(m_DataView, @nrRows, @nrCols, @nrDotRows, @nrDotCols, @fullFileNameBaseStr) then format as 569 dmscontrol.pas
    m_TableViewButtons.emplace_back(TB_TableCopy,                           GV_copy,                        false, 0,     ButtonType::SINGLE, "Copy as semicolon delimited text to Clipboard ", false);
    m_TableViewButtons.emplace_back(TB_Copy,                                GV_vcopy,                       false, 0,     ButtonType::SINGLE, "Copy the visible contents as image to Clipboard", false);
    
    m_TableViewButtons.emplace_back(TB_ZoomSelectedObj,                     MV_table_show_first_selected, false, 1,     ButtonType::SINGLE, "Show the first selected row", false);
    m_TableViewButtons.emplace_back(TB_SelectRows,                          MV_table_select_row, false, 1,     ButtonType::SINGLE, "Select row(s) by mouse-click (use Shift to add or Ctrl to deselect)", false);
    m_TableViewButtons.emplace_back(TB_SelectAll,                           MV_select_all, false, 1,     ButtonType::SINGLE, "Select all rows", false);
    m_TableViewButtons.emplace_back(TB_SelectNone,                          MV_select_none, false, 1,     ButtonType::SINGLE, "Deselect all rows", false);
    m_TableViewButtons.emplace_back(TB_ShowSelOnlyOn, TB_ShowSelOnlyOff,    MV_show_selected_features, false, 1,     ButtonType::TOGGLE, "Show only selected rows", false);
    
    m_TableViewButtons.emplace_back(TB_TableGroupBy,                        GV_group_by, false, 2,     ButtonType::SINGLE, "Group by the highlighted columns", false);

    // MapView buttons
    m_MapViewButtons.emplace_back(TB_Export, GV_save, false, 0, ButtonType::MODAL, "Export the viewport data to bitmaps file(s) using the export settings and the current ROI", false);
    m_MapViewButtons.emplace_back(TB_Copy, GV_copy, false, 0, ButtonType::SINGLE, "Copy the visible contents of the viewport to the Clipboard", false);
    m_MapViewButtons.emplace_back(TB_CopyLC, GV_vcopy, false, 0, ButtonType::SINGLE, "Copy the full contents of the LayerControlList to the Clipboard", false);

    m_MapViewButtons.emplace_back(TB_ZoomAllLayers,   MV_zoom_all_layers, false, 1, ButtonType::SINGLE, "Make the extents of all layers fit in the ViewPort", false);
    m_MapViewButtons.emplace_back(TB_ZoomActiveLayer, MV_zoom_active_layer, false, 1, ButtonType::SINGLE, "Make the extent of the active layer fit in the ViewPort", false);
    m_MapViewButtons.emplace_back(TB_ZoomIn2,  TB_Neutral, MV_zoomin_button, true, 1, ButtonType::TOGGLE, "Zoom in by drawing a rectangle", false);
    m_MapViewButtons.emplace_back(TB_ZoomOut2, TB_Neutral, MV_zoomout_button, true, 1, ButtonType::TOGGLE, "Zoom out by clicking on a ViewPort location", false);
    
    m_MapViewButtons.emplace_back(TB_ZoomSelectedObj, MV_zoom_selected, false, 2, ButtonType::SINGLE, "Make the extent of the selected elements fit in the ViewPort", false);
    m_MapViewButtons.emplace_back(TB_SelectObject, MV_select_object, true, 2, ButtonType::SINGLE, "Select elements in the active layer by mouse-click(use Shift to add or Ctrl to deselect)", false);
    m_MapViewButtons.emplace_back(TB_SelectRect, MV_select_rect, true, 2, ButtonType::SINGLE, "Select elements in the active layer by drawing a rectangle(use Shift to add or Ctrl to deselect)", false);
    m_MapViewButtons.emplace_back(TB_SelectCircle, MV_select_circle, true, 2, ButtonType::SINGLE, "Select elements in the active layer by drawing a circle(use Shift to add or Ctrl to deselect)", false);
    m_MapViewButtons.emplace_back(TB_SelectPolygon, MV_select_poly, true, 2, ButtonType::SINGLE, "Select elements in the active layer by drawing a polygon(use Shift to add or Ctrl to deselect)", false);
    m_MapViewButtons.emplace_back(TB_SelectDistrict, MV_select_district, true, 2, ButtonType::SINGLE, "Select contiguous regions in the active layer by clicking on them (use Shift to add or Ctrl to deselect)", false);
    m_MapViewButtons.emplace_back(TB_SelectAll, MV_select_all, false, 2, ButtonType::SINGLE, "Select all elements in the active layer", false);
    m_MapViewButtons.emplace_back(TB_SelectNone, MV_select_none, false, 2, ButtonType::SINGLE, "Deselect all elements in the active layer", false);
    m_MapViewButtons.emplace_back(TB_ShowSelOnlyOn, TB_ShowSelOnlyOff, MV_show_selected_features, false, 2, ButtonType::TOGGLE, "Show only selected elements", false);

    m_MapViewButtons.emplace_back(TB_Show_VP, TB_Show_VPLC, TB_Show_VPLCOV, MV_toggle_layout_3, false, 3, ButtonType::TRISTATE, "Toggle the layout of the ViewPort between{MapView only, with LayerControlList, with Overview and LayerControlList", false);
    m_MapViewButtons.emplace_back(TB_SP_All, TB_SP_Active, TB_SP_None, MV_toggle_palette, false, 3, ButtonType::TRISTATE, "Toggle Palette Visibiliy between{All, Active Layer Only, None}", false);
    m_MapViewButtons.emplace_back(TB_NeedleOn, TB_NeedleOff, MV_toggle_needle, false, 3, ButtonType::TOGGLE, "Show / Hide NeedleControler", false);
    m_MapViewButtons.emplace_back(TB_ScaleBarOn, TB_ScaleBarOff, MV_toggle_scalebar, false, 3, ButtonType::TOGGLE, "Show / Hide ScaleBar", false);
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

        if (button.Update(view)) // all unique buttons but this one need their visual state reset to 0
        {
            for (auto& _button : m_MapViewButtons)
            {
                if (!_button.GetUniqueness())
                    continue;

                if (_button == button)
                    continue;

                _button.SetState(0);
            }
        }
    
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
    if (dockspace_docknode->ChildNodes[0] && dockspace_docknode->ChildNodes[0]->ChildNodes[0])
        toolbar_docknode = dockspace_docknode->ChildNodes[0]->ChildNodes[0];

    return toolbar_docknode;
}

void SetDefaultToolbarSize(GuiState& state)
{
    auto toolbar_docknode = GetToolBarDockNode(state);
    if (toolbar_docknode)
    {
        auto window_size = ImGui::GetWindowSize();
        ImGui::DockBuilderSetNodeSize(toolbar_docknode->ID, ImVec2(window_size.x, 32.0f));
    }
}

void GuiToolbar::Update(bool* p_open, GuiState& state, GuiViews& views) // TODO: add int return to button which is its group. Untoggle all buttons in the same group.
{
    // TODO: on selection what toolbar to use: dataview.cpp, lines 1318 and SetMdiToolbar fmain.pas 670
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(117.0f/255.0f, 117.0f/255.0f, 138.0f/255.0f, 1.0f));

    if (!ImGui::Begin("Toolbar", p_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar) || views.m_dms_views.empty())
    {
        SetDefaultToolbarSize(state);
        AutoHideWindowDocknodeTabBar(is_docking_initialized);
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    //AutoHideWindowDocknodeTabBar(is_docking_initialized);
    SetDefaultToolbarSize(state);
    // focus window when clicked
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    if (views.m_dms_view_it!=views.m_dms_views.end())
    {
        if (views.m_dms_view_it->m_ViewStyle == tvsTableView)
            ShowTableViewButtons(views);
        else if (views.m_dms_view_it->m_ViewStyle == tvsMapView)
            ShowMapViewButtons(views);
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}