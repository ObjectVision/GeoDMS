#include <imgui.h>
#include "GuiExport.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "utl/Environment.h"

#include "ShvUtils.h"

bool CurrentItemCanBeExportedToVector(TreeItem* item)
{
    if (!item)
        return false;



    return true;
}

bool CanBeRasterDomain(const AbstrUnit* domainCandidate)
{
    assert(domainCandidate);
    assert(domainCandidate->CanBeDomain()); // precondition that it was domain of something.
    if (domainCandidate->GetNrDimensions() == 2)
        return true;
    if (RefersToMappable(domainCandidate))
    { 
        auto baseGridCandidate = GetMappedData(domainCandidate);
        if (baseGridCandidate->GetAbstrDomainUnit()->GetNrDimensions() != 2) 
            return false; // some other sort of reference ?
        auto compactedDomain = baseGridCandidate->GetAbstrValuesUnit();
        if (compactedDomain->UnifyDomain(domainCandidate))
            return true;
    }
    return false;
}

bool CurrentItemCanBeExportedToRaster(const TreeItem* item)
{
    if (!item)
        return false;

    if (!IsDataItem(item))
    {
        if (IsUnit(item))
        {
            auto domainCandidate = AsUnit(item);
            for (auto subItem = item; subItem; subItem = item->WalkConstSubTree(subItem))
                if (IsDataItem(subItem) && domainCandidate->UnifyDomain(AsDataItem(subItem)->GetAbstrDomainUnit()))
                    return CanBeRasterDomain(domainCandidate);
        }
        return false;
    }

    auto adi = AsDataItem(item); assert(adi);
    auto adu = AsDataItem(item)->GetAbstrDomainUnit(); assert(adu);

    return CanBeRasterDomain(adu);
}

void GuiExport::SetStorageLocation()
{
    ImGui::Text("Output folder:   "); ImGui::SameLine();

    if (ImGui::InputText("##LocalDataDir", &m_folder_name, ImGuiInputTextFlags_None, InputTextCallback, nullptr))
    {
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));

    if (ImGui::Button(ICON_RI_FOLDER_OPEN))
    {
        auto tmp_folder_name = BrowseFolder(GetLocalDataDir().c_str());//StartWindowsFileDialog(ConvertDmsFileNameAlways(GetGeoDmsRegKey("LocalDataDir")).c_str(), L"test", L"*.*");
        if (!tmp_folder_name.empty())
        {
            m_folder_name = tmp_folder_name;
        }
    }
    ImGui::PopStyleColor(3);

    ImGui::TextUnformatted("Filename: ");
    ImGui::SameLine();
    if (ImGui::InputText("##export_filename", &m_file_name, ImGuiInputTextFlags_EnterReturnsTrue, InputTextCallback, nullptr))
    {
    }
}

GuiExport::GuiExport()
{
    m_folder_name = GetLocalDataDir().c_str();
}

void GuiExport::Update(bool* p_open, GuiState &state)
{
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Export", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::End();
        return;
    }


    if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    bool enable_vector_export = CurrentItemCanBeExportedToVector(state.GetCurrentItem());
    bool enable_raster_export = CurrentItemCanBeExportedToRaster(state.GetCurrentItem());;

    if (ImGui::BeginTabBar("ExportTypes", ImGuiTabBarFlags_None))
    {
        ImGui::BeginDisabled(!enable_vector_export);
        if (ImGui::BeginTabItem("Vector"))
        {

            SetStorageLocation();
            ImGui::EndTabItem();
        }


        ImGui::EndDisabled();

        ImGui::BeginDisabled(!enable_raster_export);
        if (ImGui::BeginTabItem("Raster"))
        {
            SetStorageLocation();
            ImGui::EndTabItem();
        }


        ImGui::EndDisabled();

        ImGui::EndTabBar();
    }


    auto options_window_pos = ImGui::GetWindowPos();
    auto options_window_content_region = ImGui::GetWindowContentRegionMax();
    auto current_cursor_pos_X = ImGui::GetCursorPosX();
    ImGui::SetCursorPos(ImVec2(current_cursor_pos_X, options_window_content_region.y - 1.5 * ImGui::GetTextLineHeight()));

    if (ImGui::Button("Export", ImVec2(50, 1.5 * ImGui::GetTextLineHeight())))
    {
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(50, 1.5 * ImGui::GetTextLineHeight())))
    {
    }

    ImGui::End();

    //if (ImGui::Begin("Export", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar))
   // {


    /*if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    bool enable_vector_export = CurrentItemCanBeExportedToVector(state.GetCurrentItem());
    bool enable_raster_export = CurrentItemCanBeExportedToRaster(state.GetCurrentItem());;

    if (ImGui::BeginTabBar("ExportTypes", ImGuiTabBarFlags_None))
    {
        ImGui::BeginDisabled(!enable_vector_export);
        if (ImGui::BeginTabItem("Vector"))
        {

            SetStorageLocation();
            ImGui::EndTabItem();
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!enable_raster_export);
        if (ImGui::BeginTabItem("Raster"))
        {
            SetStorageLocation();
            ImGui::EndTabItem();
        }
        ImGui::EndDisabled();

        ImGui::EndTabBar();
    }

    auto options_window_pos = ImGui::GetWindowPos();
    auto options_window_content_region = ImGui::GetWindowContentRegionMax();
    auto current_cursor_pos_X = ImGui::GetCursorPosX();
    ImGui::SetCursorPos(ImVec2(current_cursor_pos_X, options_window_content_region.y - 1.5 * ImGui::GetTextLineHeight()));

    if (ImGui::Button("Export", ImVec2(50, 1.5 * ImGui::GetTextLineHeight())))
    {
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(50, 1.5 * ImGui::GetTextLineHeight())))
    {
    }


    ImGui::End();
    */
}