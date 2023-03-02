#include <imgui.h>
#include "GuiExport.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"

bool CurrentItemCanBeExportedToVector(TreeItem* item)
{
    if (!item)
        return false;



    return true;
}

bool CurrentItemCanBeExportedToRaster(TreeItem* item)
{
    if (!item)
        return false;

    if (!IsDataItem(item))
        return false;

    if (AsDataItem(item)->GetAbstrDomainUnit()->GetNrDimensions() == 2 && AsDataItem(item)->GetAbstrDomainUnit()->CanBeDomain()) // TODO: copy of HasRasterDomain
        return true;

    return false;
}

void GuiExport::Update(bool* p_open, GuiState &state)
{
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_Once);
    if (ImGui::Begin("Export", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar))
    {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            SetKeyboardFocusToThisHwnd();

        bool enable_vector_export = CurrentItemCanBeExportedToVector(state.GetCurrentItem());
        bool enable_raster_export = CurrentItemCanBeExportedToRaster(state.GetCurrentItem());;

        if (ImGui::BeginTabBar("ExportTypes", ImGuiTabBarFlags_None))
        {
            ImGui::BeginDisabled(!enable_vector_export);
            if (ImGui::BeginTabItem("Vector"))
            {
                ImGui::EndTabItem();
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(!enable_raster_export);
            if (ImGui::BeginTabItem("Raster"))
            {
                ImGui::EndTabItem();
            }
            ImGui::EndDisabled();

            ImGui::EndTabBar();
        }


        ImGui::End();
    }
}