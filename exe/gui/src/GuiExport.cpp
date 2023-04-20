#include <imgui.h>
#include "GuiExport.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"

#include "mci/ValueClass.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"

#include "ShvUtils.h"

#include "Unit.h"

const AbstrUnit* CommonDomain(const TreeItem* item)
{
    if (IsDataItem(item))
        return AsDataItem(item)->GetAbstrDomainUnit();

    const AbstrUnit* domainCandidate = nullptr;
    bool foundSomeAttr = false;
    if (IsUnit(item))
    {
        domainCandidate = AsUnit(item);
        if (!domainCandidate->CanBeDomain())
            return nullptr;
    }

    for (auto subItem = item->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
        if (IsDataItem(subItem))
        {
            auto adu = AsDataItem(subItem)->GetAbstrDomainUnit();
            if (domainCandidate && domainCandidate->GetUnitClass() != Unit<Void>::GetStaticClass())
            {
                if (!domainCandidate->UnifyDomain(adu, "", "", UnifyMode::UM_AllowVoidRight))
                    return nullptr;
            }
            else
                domainCandidate = adu;
            foundSomeAttr = true;
        }

    return foundSomeAttr ? domainCandidate : nullptr;
}

bool CurrentItemCanBeExportedAsTable(const TreeItem* item)
{
    // category Attr: a single data item
    return CommonDomain(item) != nullptr;
}

bool CurrentItemCanBeExportedAsDatabase(const TreeItem* item)
{
    // category Table-b: container with data-items as direct sub-items that all have a compatible domain
    for (auto subItem = item->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
        if (CommonDomain(subItem))
            return true;
    return false;
}

bool CurrentItemCanBeExportedAsTableOrDatabase(const TreeItem* item)
{
    if (CurrentItemCanBeExportedAsTable(item))
        return true;
    return CurrentItemCanBeExportedAsDatabase(item);
}

bool CurrentItemCanBeExportedToVector(const TreeItem* item)
{
    if (!item)
        return false;

    // category Table
    if (CurrentItemCanBeExportedAsTableOrDatabase(item))
        return true;

    // category Database: container with tables.
    for (auto subItem = item->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
        if (CurrentItemCanBeExportedAsTable(subItem))
            return true;

    return false;
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
            if (!domainCandidate->CanBeDomain())
                return false;
            for (auto subItem = item; subItem; subItem = item->WalkConstSubTree(subItem))
                if (IsDataItem(subItem) && domainCandidate->UnifyDomain(AsDataItem(subItem)->GetAbstrDomainUnit()))
                    if (CurrentItemCanBeExportedToRaster(subItem))
                        return true;
        }
        return false;
    }

    auto adi = AsDataItem(item); assert(adi);
    if (adi->GetValueComposition() != ValueComposition::Single)
        return false;
    auto avu = adi->GetAbstrValuesUnit(); assert(avu);
    if (!avu->GetValueType()->IsNumericOrBool())
        return false;

    auto adu = adi->GetAbstrDomainUnit(); assert(adu);

    return CanBeRasterDomain(adu);
}

void GuiExport::SetDefaultNativeDriverUsage()
{
    // TODO: expand logic, simplified implementation
    if (m_selected_driver.is_native && m_selected_driver.shortname == "CSV")
        m_use_native_driver = true;
    else
        m_use_native_driver = false;
}

void GuiExport::SelectDriver(bool is_raster)
{
    if (m_selected_driver.is_raster != is_raster)
        m_selected_driver = {};

    ImGui::Text("Format:               "); ImGui::SameLine();
    


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
                SetDefaultNativeDriverUsage();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    // native driver selection
    
    ImGui::BeginDisabled(m_selected_driver.IsEmpty() || !m_selected_driver.is_native);
    ImGui::Checkbox("##native_driver", &m_use_native_driver);
    ImGui::SameLine();
    ImGui::Text("Use native driver");
    ImGui::EndDisabled();
    
}

void GuiExport::SetStorageLocation()
{            
    // foldername
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

    // filename
    ImGui::TextUnformatted("Filename:           ");
    ImGui::SameLine();
    if (ImGui::InputText("##export_filename", &m_file_name, ImGuiInputTextFlags_EnterReturnsTrue, InputTextCallback, nullptr))
    {
    }
}

GuiExport::GuiExport()
{
    m_folder_name = GetLocalDataDir().c_str();

    m_available_drivers.emplace_back("ESRI Shapefile", "ESRI Shapefile / DBF", false, true);
    m_available_drivers.emplace_back("GPKG", "GeoPackage vector", false, false);
    m_available_drivers.emplace_back("CSV", "Comma Separated Value(.csv)", false, true);
    m_available_drivers.emplace_back("GML", "Geography Markup Language", false, false);
    m_available_drivers.emplace_back("GeoJSON", "GeoJSON", false, false);
    m_available_drivers.emplace_back("GTiff", "GeoTIFF File Format", true, true);
    m_available_drivers.emplace_back("netCDF", "NetCDF: Network Common Data Form", true, false);
    m_available_drivers.emplace_back("PNG", "Portable Network Graphics", true, false);
    m_available_drivers.emplace_back("JPEG", "JPEG JFIF File Format", true, false);
}

void GuiExport::Update(bool* p_open, GuiState &state)
{
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_Once);
    if (!ImGui::Begin((std::string("Export ") + state.GetCurrentItem()->GetFullName().c_str()).c_str(), p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::End();
        return;
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    bool enable_vector_export = CurrentItemCanBeExportedToVector(state.GetCurrentItem());
    bool enable_raster_export = CurrentItemCanBeExportedToRaster(state.GetCurrentItem());
    bool enable_export = enable_vector_export || enable_raster_export;

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

    ImGui::BeginDisabled(!enable_export);
    if (ImGui::Button("Export", ImVec2(50, 1.5 * ImGui::GetTextLineHeight())))
    {
        if (DoExport(state))
            state.ShowExportWindow = false;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(50, 1.5 * ImGui::GetTextLineHeight())))
    {
        state.ShowExportWindow = false;
    }

    ImGui::End();
}

// ========================== actual export procedures, possibly to move to a separate code unit

#include "ser/FileStreamBuff.h"
#include "stg/AbstrStoragemanager.h"
#include "TicInterface.h"

void DoExportTableToCSV(const TreeItem* tableItem, SharedStr fullFileName)
{
    std::vector<TableColumnSpec> columnSpecs;
    auto domain = CommonDomain(tableItem);
    assert(domain);

    if (IsDataItem(tableItem))
        columnSpecs.emplace_back().m_DataItem = AsDataItem(tableItem);
    else for (auto attrItem = tableItem->GetFirstSubItem(); attrItem; attrItem->GetNextItem())
        if (IsDataItem(attrItem))
        {
            auto adi = AsDataItem(attrItem);
            if (adi->GetAbstrDomainUnit()->UnifyDomain(domain)) // adi could also be a skippable parameter
                columnSpecs.emplace_back().m_DataItem = adi;
        }

    auto fout = std::make_unique<FileOutStreamBuff>(ConvertDosFileName(fullFileName), nullptr, true);

    Table_Dump(fout.get(), begin_ptr(columnSpecs), end_ptr(columnSpecs), nullptr, nullptr);
}

void DoExportTableorDatabaseToCSV(const TreeItem* tableOrDatabaseItem, SharedStr fullFileName)
{
    if (CurrentItemCanBeExportedAsTable(tableOrDatabaseItem))
    {
        DoExportTableToCSV(tableOrDatabaseItem, fullFileName);
        return;
    }
    if (!CurrentItemCanBeExportedAsDatabase(tableOrDatabaseItem))
        return;

    for (auto tableItem=tableOrDatabaseItem->GetFirstSubItem(); tableItem; tableItem->GetNextItem())
        if (CurrentItemCanBeExportedAsTable(tableItem))
        {
            auto subFileName = DelimitedConcat(fullFileName.c_str(), tableItem->GetName().c_str());
            DoExportTableToCSV(tableItem, subFileName);
        }
}

bool GuiExport::DoExport(GuiState& state)
{
    auto item = state.GetCurrentItem();
    auto selectedDriver = m_selected_driver;
    auto folderName = m_folder_name;
    auto fileName = m_file_name;

    SharedStr ffName = DelimitedConcat(folderName.c_str(), fileName.c_str());
    CharPtr driverName = nullptr;
    CharPtr storageTypeName = selectedDriver.shortname.c_str();
    if (!selectedDriver.is_native)
    {
        driverName = storageTypeName;
        if (selectedDriver.is_raster)
            storageTypeName = "gdalwrite.grid";
        else
            storageTypeName = "gdalwrite.vect";
    }
    else if (!stricmp(storageTypeName,  "CSV"))
    {
        DoExportTableToCSV(item, ffName);
        return true;
    }

    // TODO: make shadow items if a storage on this gets in the way of other things, such as template instantiation
    item->SetStorageManager(ffName.c_str(), storageTypeName, false, driverName);
    return false;
}

