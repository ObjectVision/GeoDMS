#include <imgui.h>
#include "GuiExport.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "utl/Environment.h"

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

void GuiExport::SelectDriver(bool is_raster)
{
    if (m_selected_driver.is_raster != is_raster)
        m_selected_driver = {};

    ImGui::Text("GDAL driver:     "); ImGui::SameLine();
    
    if (ImGui::BeginCombo("##driver_selector", m_selected_driver.shortname.c_str(), ImGuiComboFlags_None))
    {
        for (auto& available_driver : m_available_drivers)
        {
            if (is_raster != available_driver.is_raster)
                continue;

            bool is_selected = m_selected_driver.shortname == available_driver.shortname;
            if (ImGui::Selectable(available_driver.shortname.c_str(), is_selected))
            {
                m_selected_driver = available_driver;
            }
        }
        ImGui::EndCombo();
    }
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

    ImGui::TextUnformatted("Filename:           ");
    ImGui::SameLine();
    if (ImGui::InputText("##export_filename", &m_file_name, ImGuiInputTextFlags_EnterReturnsTrue, InputTextCallback, nullptr))
    {
    }
}

GuiExport::GuiExport()
{
    m_folder_name = GetLocalDataDir().c_str();

    m_available_drivers.emplace_back("ESRI Shapefile", "ESRI Shapefile / DBF", false);
    m_available_drivers.emplace_back("GPKG", "GeoPackage vector", false);
    m_available_drivers.emplace_back("CSV", "Comma Separated Value(.csv)", false);
    m_available_drivers.emplace_back("GML", "Geography Markup Language", false);
    m_available_drivers.emplace_back("GeoJSON", "GeoJSON", false);
    m_available_drivers.emplace_back("GTiff", "GeoTIFF File Format", true);
    m_available_drivers.emplace_back("netCDF", "NetCDF: Network Common Data Form", true);
    m_available_drivers.emplace_back("PNG", "Portable Network Graphics", true);
    m_available_drivers.emplace_back("JPEG", "JPEG JFIF File Format", true);
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
            SelectDriver(false);
            SetStorageLocation();
            ImGui::EndTabItem();
        }
        ImGui::EndDisabled();
        
        ImGui::BeginDisabled(!enable_raster_export);
        if (ImGui::BeginTabItem("Raster"))
        {
            SelectDriver(true);
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
        // current treeitem:     state.GetCurrentItem()
        // selected gdal driver: m_selected_driver
        // string foldername:    m_folder_name
        // string filename:      m_filename



    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(50, 1.5 * ImGui::GetTextLineHeight())))
    {
        state.ShowExportWindow = false;
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