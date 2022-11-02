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

void Button::Update(GuiView& view)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
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

void Button::UpdateSingle(GuiView& view)
{
    SendMessage(view.GetHWND(), WM_COMMAND, m_ToolButtonId1, 0);
}

void Button::UpdateToggle(GuiView& view)
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

void Button::UpdateTristate(GuiView& view)
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

void Button::UpdateModal(GuiView& view)
{

}

GuiToolbar::GuiToolbar()
{
    m_Buf.resize(1024);

    // TableView buttons
    m_TableViewButtons.emplace_back(TB_Export,                              GV_save,                        -1,     ButtonType::MODAL, "Save to file as semicolon delimited text", false);                 // SHV_DataView_GetExportInfo(m_DataView, @nrRows, @nrCols, @nrDotRows, @nrDotCols, @fullFileNameBaseStr) then format as 569 dmscontrol.pas
    m_TableViewButtons.emplace_back(TB_TableCopy,                           GV_copy,                        -1,     ButtonType::SINGLE, "Copy To ClipBoard as SemiColon delimited Text", false);
    m_TableViewButtons.emplace_back(TB_Copy,                                GV_vcopy,                       -1,     ButtonType::SINGLE, "Copy the visible contents of the active view to the Clipboard", false);
    m_TableViewButtons.emplace_back(TB_ZoomSelectedObj,                     MV_table_show_first_selected,   -1,     ButtonType::SINGLE, "Show the first selected row", false);
    m_TableViewButtons.emplace_back(TB_SelectRows,                          MV_table_select_row,            -1,     ButtonType::SINGLE, "Select objects in the active layer by the mouse (use Ctrl for deselection)", false);
    m_TableViewButtons.emplace_back(TB_SelectAll,                           MV_select_all,                  -1,     ButtonType::SINGLE, "Select all objects in the active layer", false);
    m_TableViewButtons.emplace_back(TB_SelectNone,                          MV_select_none,                 -1,     ButtonType::SINGLE, "Deselect all objects in the active layer", false);
    m_TableViewButtons.emplace_back(TB_ShowSelOnlyOn, TB_ShowSelOnlyOff,    MV_show_selected_features,      -1,     ButtonType::TOGGLE, "Show only selected rows", false);
    m_TableViewButtons.emplace_back(TB_TableGroupBy,                        GV_group_by,                     1,     ButtonType::SINGLE, "Group by the selected columns", false);

    // MapView buttons
    m_MapViewButtons.emplace_back(TB_ZoomAllLayers,   MV_zoom_all_layers,     -1, ButtonType::SINGLE, "Make the extents of all layers fit in the ViewPort of the active view", false);
    m_MapViewButtons.emplace_back(TB_ZoomActiveLayer, MV_zoom_active_layer,   -1, ButtonType::SINGLE, "Make the extent of the active layer fit in the ViewPort of the active view", false);
    m_MapViewButtons.emplace_back(TB_ZoomIn2,         MV_zoomin_button,        1, ButtonType::SINGLE, "Zoom in by drawing a rectangle", false);
    m_MapViewButtons.emplace_back(TB_ZoomOut2,        MV_zoomout_button,       1, ButtonType::SINGLE, "Zoom out by clicking on a ViewPort location", false);
    m_MapViewButtons.emplace_back(TB_Show_VP, TB_Show_VPLC, TB_Show_VPLCOV, MV_toggle_layout_1, -1, ButtonType::TRISTATE, "Toggle the layout of the ViewPort between{MapView only, with LayerControllers, with Overview and LayerControllers", false);
    m_MapViewButtons.emplace_back(TB_SP_All, TB_SP_Active, TB_SP_None, MV_toggle_palette, -1, ButtonType::TRISTATE, "Toggle Palette Visibiliy between{All, Active Layer Only, None}", false);
    m_MapViewButtons.emplace_back(TB_NeedleOn, TB_NeedleOff, MV_toggle_needle, -1, ButtonType::TOGGLE, "Show / Hide NeedleControler", false);
    m_MapViewButtons.emplace_back(TB_ScaleBarOn, TB_ScaleBarOff, MV_toggle_scalebar, -1, ButtonType::TOGGLE, "Show / Hide ScaleBar", false);
    m_MapViewButtons.emplace_back(TB_Export, GV_save, -1, ButtonType::MODAL, "Export the viewport data to bitmaps file(s) using the export set tings and the current ROI", false);
    m_MapViewButtons.emplace_back(TB_Copy, GV_copy, -1, ButtonType::SINGLE, "Copy the visible contents of the viewport to the Clipboard", false);
    m_MapViewButtons.emplace_back(TB_CopyLC, GV_vcopy, -1, ButtonType::SINGLE, "Copy the full contents of the LayerControlList of the active view to the Clipboard", false);
    m_MapViewButtons.emplace_back(TB_ZoomSelectedObj, MV_zoom_selected, -1, ButtonType::SINGLE, "Make the extent of the selected objects fit in the ViewPort of the active view", false);
    m_MapViewButtons.emplace_back(TB_SelectObject, MV_select_object, 1, ButtonType::SINGLE, "Select objects in the active layer by the mouse(use Ctrl for deselection)", false);
    m_MapViewButtons.emplace_back(TB_SelectRect, MV_select_rect, 1, ButtonType::SINGLE, "Select objects in the active layer by drawing a rectangle(use Ctrl for deselection)", false);
    m_MapViewButtons.emplace_back(TB_SelectCircle, MV_select_circle, 1, ButtonType::SINGLE, "Select objects in the active layer by drawing a circle(use Ctrl for deselection)", false);
    m_MapViewButtons.emplace_back(TB_SelectPolygon, MV_select_poly, 1, ButtonType::SINGLE, "Select objects in the active layer by drawing a polygon(use Ctrl for deselection)", false);
    m_MapViewButtons.emplace_back(TB_SelectDistrict, MV_select_district, 1, ButtonType::SINGLE, "Select contiguous regions in the active layer by clicking on them (use Ctrl for deselection)", false);
    m_MapViewButtons.emplace_back(TB_SelectAll, MV_select_all, -1, ButtonType::SINGLE, "Select all objects in the active layer", false);
    m_MapViewButtons.emplace_back(TB_SelectNone, MV_select_none, -1, ButtonType::SINGLE, "Deselect all objects in the active layer", false);
}

void GuiToolbar::ShowMapViewButtons(GuiView& view)
{
    for (auto& button : m_MapViewButtons)
    {
        button.Update(view);
        ImGui::SameLine();
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

void GuiToolbar::Update(bool* p_open, GuiView& view) // TODO: add int return to button which is its group. Untoggle all buttons in the same group.
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(117.0f/255.0f, 117.0f/255.0f, 138.0f/255.0f, 1.0f));
    if (!ImGui::Begin("Toolbar", p_open, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }
   
    // focus window when clicked
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    if (!view.m_Views.empty())
    {
        if (view.m_Views.at(view.m_ViewIndex).m_ViewStyle == tvsTableView)
        {
            ShowTableViewButtons(view);
        }
        else if (view.m_Views.at(view.m_ViewIndex).m_ViewStyle == tvsMapView)
        {
            ShowMapViewButtons(view);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}